#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "maze.h"
#include "tga.h"


#define NDIRS  4
#define every_direction   (north | west | east | south)
#define random_direction  (1 << arc4random_uniform(NDIRS))

enum dir {
  north = 1 << 0,
  west  = 1 << 1,
  east  = 1 << 2,
  south = 1 << 3,
};

/* VT100 escape sequences */
static const char *const  VT100_CursorHome  = "\033[H";
static const char *const  VT100_EraseScreen = "\033[2J";

static const char  type_empty = ' ';
static const char  type_wall  = 'X';
static const char  type_list  = '+';
static const char  type_inval = '?';


static useconds_t  g_steptime;
static wchar_t    *g_dumpbuf;


/* maze.buf[] is treated as a two-dimensional array and contains the maze.
 * Think of a piece of graph paper: filled squares are walls, empty squares
 * are floor pieces.
 *
 * It does not include the surrounding wall (aka frame). Width and height
 * are expected to be even numbers.
 *
 * Regarding maze_gen_dfs() and maze_gen_prm():
 *
 *  - Nodes are on positions where both coordinates are even numbers.
 *  - Edges (if any) are on positions where exactly one coordinate is odd.
 *  - Everything in between is not part of the graph.
 */

struct maze {
  unsigned short  w;
  unsigned short  h;
  char            buf[];
};

struct pos {
  unsigned int  x;
  unsigned int  y;
};

struct region {
  unsigned int  x;
  unsigned int  y;
  unsigned int  w;
  unsigned int  h;
};

struct list {
  unsigned int  len;
  struct pos    buf[];
};


static struct list  *list_new(unsigned int len);
static void          list_add(struct list *const l, struct pos p);
static struct pos    list_remove(struct list *const l, unsigned int idx);
static void          list_add_unvisited_neighbors(struct maze *const m, struct list *const l, struct pos p);
static struct pos    list_remove_random(struct list *const l);

static enum dir      pick_rnd_dir(enum dir *const dirlist, unsigned int len);
static struct pos    pos_add(struct pos p, enum dir d, unsigned int step);
static char          type_get(const struct maze *const m, struct pos p);
static void          type_set(struct maze *const m, struct pos p, char type);
static int           is_wall(const struct maze *const m, unsigned int x, unsigned int y);

static void          maze_gen_dfs(struct maze *const m);
static void          maze_gen_dfs_recur(struct maze *const m, struct pos p);
static void          maze_gen_div(struct maze *const m);
static void          maze_gen_div_recur(struct maze *const m, struct region r);
static void          maze_gen_prm(struct maze *const m);

static void          maze_showstep(const struct maze *const m);


static struct list *
list_new(unsigned int len)
{
  struct list *list;

  list = malloc(sizeof*list + (len * sizeof*list->buf));
  if (list == NULL)
    errx(EX_UNAVAILABLE, "%s: malloc", __func__);
  list->len = 0;

  return list;
}


static void
list_add(struct list *const l, struct pos p)
{
  l->buf[l->len] = p;
  ++l->len;
}


static struct pos
list_remove(struct list *const l, unsigned int idx)
{
  struct pos p;

  p = l->buf[idx];
  --l->len;
  l->buf[idx] = l->buf[l->len];

  return p;
}


/* Helper function for maze_gen_prm() */
static void
list_add_unvisited_neighbors(struct maze *const m, struct list *const l, struct pos p)
{
  struct pos neighbor;
  enum dir dirs[NDIRS] = { north, west, east, south };

  for (unsigned int i = 0; i < NDIRS; ++i) {
    neighbor = pos_add(p, dirs[i], 2);
    if (type_get(m, neighbor) == type_wall) {
      type_set(m, neighbor, type_list);
      list_add(l, neighbor);
    }
  }
}


/* Helper function for maze_gen_prm() */
static struct pos
list_remove_random(struct list *const l)
{
  return list_remove(l, arc4random_uniform(l->len));
}


/* Helper function to iterate over an array of directions in random order */
static enum dir
pick_rnd_dir(enum dir *const dirlist, unsigned int len)
{
  unsigned int rnd;
  enum dir dir;

  rnd = arc4random_uniform(len);
  dir = dirlist[rnd];
  dirlist[rnd] = dirlist[len-1];

  return dir;
}


static struct pos
pos_add(struct pos p, enum dir d, unsigned int step)
{
  struct pos new;
  switch (d) {
  case north: new.x = p.x;      new.y = p.y-step; break;
  case west:  new.x = p.x-step; new.y = p.y;      break;
  case east:  new.x = p.x+step; new.y = p.y;      break;
  case south: new.x = p.x;      new.y = p.y+step; break;
  }
  return new;
}


static char
type_get(const struct maze *const m, struct pos p)
{
  if ((p.x < m->w) && (p.y < m->h))
    return m->buf[m->w*p.y + p.x];
  else
    return type_inval;
}


static void
type_set(struct maze *const m, struct pos p, char type)
{
  if ((p.x < m->w) && (p.y < m->h))
    m->buf[m->w*p.y + p.x] = type;
}


/* Helper function for maze_dump_txt() and maze_dump_tga() */
static int
is_wall(const struct maze *const m, unsigned int x, unsigned int y)
{
  /* x != type_empty is used to draw nodes of type_list as walls */
  if ((x < m->w) && (y < m->h))
    return (m->buf[m->w*y + x] != type_empty);
  else
    return 1; /* part of the frame, ergo wall */
}


/* Generates a maze via depth-first search */
static void
maze_gen_dfs(struct maze *const m)
{
  /* the maze is created by removing walls */
  memset(m->buf, type_wall, m->w * m->h);

  struct pos start = {
    arc4random_uniform(m->w / 2) * 2,
    arc4random_uniform(m->h / 2) * 2,
  };
  maze_gen_dfs_recur(m, start);
}


static void
maze_gen_dfs_recur(struct maze *const m, struct pos current)
{
  struct pos neighbor, wall;
  unsigned int dlen;
  enum dir *dirs, dir;

  maze_showstep(m);

  type_set(m, current, type_empty);

  dirs = (enum dir[]){ north, west, east, south };
  for (dlen = NDIRS; dlen > 0; --dlen) {
    dir = pick_rnd_dir(dirs, dlen);

    neighbor = pos_add(current, dir, 2);
    if (type_get(m, neighbor) == type_wall) { /* if not visited yet */
      wall = pos_add(current, dir, 1);
      type_set(m, wall, type_empty);          /* connect nodes */
      maze_gen_dfs_recur(m, neighbor);
    }
  }
}


/* Generates a maze via recursive division */
static void
maze_gen_div(struct maze *const m)
{
  /* the maze is created by constructing walls with passages */
  memset(m->buf, type_empty, m->w*m->h);

  struct region r = { 0, 0, m->w, m->h };
  maze_gen_div_recur(m, r);
}


static void
maze_gen_div_recur(struct maze *const m, struct region r)
{
  struct pos offset, poi, wall, pass;
  unsigned int nlen, wlen, slen, elen;
  struct region nw, ne, sw, se;
  int dirs;

  if ((r.w == 1) || (r.h == 1))
    return;

  maze_showstep(m);

  /* divide the room into four sections by constructing two
   * intersecting walls */

  offset.x = 1 + arc4random_uniform(r.w/2)*2;
  offset.y = 1 + arc4random_uniform(r.h/2)*2;

  poi.x = r.x + offset.x; /* point of intersection */
  poi.y = r.y + offset.y;

  wall.x = r.x;
  wall.y = poi.y;
  for (; wall.x < r.x+r.w; ++wall.x)
    type_set(m, wall, type_wall);

  wall.x = poi.x;
  wall.y = r.y;
  for (; wall.y < r.y+r.h; ++wall.y)
    type_set(m, wall, type_wall);

  /* create passages on three of the four sides of the intersection
   * so all sections stay accessible */

  dirs = every_direction & ~random_direction;

  nlen = offset.y;
  wlen = offset.x;
  slen = r.h - offset.y;
  elen = r.w - offset.x;

  if (dirs & north) {
    pass = pos_add(poi, north, 1 + arc4random_uniform(nlen/2)*2);
    type_set(m, pass, type_empty);
  }
  if (dirs & west) {
    pass = pos_add(poi, west,  1 + arc4random_uniform(wlen/2)*2);
    type_set(m, pass, type_empty);
  }
  if (dirs & east) {
    pass = pos_add(poi, east,  1 + arc4random_uniform(elen/2)*2);
    type_set(m, pass, type_empty);
  }
  if (dirs & south) {
    pass = pos_add(poi, south, 1 + arc4random_uniform(slen/2)*2);
    type_set(m, pass, type_empty);
  }

  /* repeat for each section */

  nw = (struct region){ r.x,     r.y,     wlen,   nlen   };
  ne = (struct region){ poi.x+1, r.y,     elen-1, nlen   };
  sw = (struct region){ r.x,     poi.y+1, wlen,   slen-1 };
  se = (struct region){ poi.x+1, poi.y+1, elen-1, slen-1 };

  maze_gen_div_recur(m, nw);
  maze_gen_div_recur(m, ne);
  maze_gen_div_recur(m, sw);
  maze_gen_div_recur(m, se);
}


/* Generate a maze by using Prim's Algorithm */
static void
maze_gen_prm(struct maze *const m)
{
  struct list *l;
  struct pos current, neighbor, wall;
  unsigned int dlen;
  enum dir *dirs, dir;

  l = list_new((m->w+m->h)*4); /* XXX: Value guesstimated after observing the
                                       maximum list size of a couple of runs */

  /* the maze is created by removing walls */
  memset(m->buf, type_wall, m->w * m->h);

  current.x = arc4random_uniform(m->w/2)*2;
  current.y = arc4random_uniform(m->h/2)*2;
  type_set(m, current, type_empty);

  list_add_unvisited_neighbors(m, l, current);

  while (l->len > 0) {
    maze_showstep(m);

    current = list_remove_random(l);
    type_set(m, current, type_empty);

    dirs = (enum dir[]){ north, west, east, south };
    for (dlen = NDIRS; dlen > 0; --dlen) {
      dir = pick_rnd_dir(dirs, dlen);

      neighbor = pos_add(current, dir, 2);
      if (type_get(m, neighbor) == type_empty) { /* if part of the maze */
        wall = pos_add(current, dir, 1);
        type_set(m, wall, type_empty);           /* connect nodes */
        break;
      }
    }

    list_add_unvisited_neighbors(m, l, current);
  }

  free(l);
}


static void
maze_showstep(const struct maze *const m)
{
  if (g_steptime) {
    printf(VT100_CursorHome);
    maze_dump_txt(m);
    usleep(g_steptime);
  }
}


void
maze_delete(struct maze *m)
{
  free(m);
  free(g_dumpbuf);
}


/* Outputs a beautified version of the plain ASCII maze to stdout,
 * uses fancy Unicode "Box drawing" elements */
void
maze_dump_txt(const struct maze *const m)
{
  static const wchar_t *const frame = L"╹╸┛╺┗━┻╻┃┓┫┏┣┳╋";
  wchar_t *p;
  unsigned int x, y;
  int n, e, s, w, idx;

  p = g_dumpbuf;

  /* top of frame */
  *p++ = L'┏';
  for (x = 0; x < m->w; ++x)
    *p++ = is_wall(m, x, 0) ? L'┳' : L'━';
  *p++ = L'┓';
  *p++ = L'\n';

  /* left side of frame, maze and right side of frame */
  for (y = 0; y < m->h; ++y) {
    *p++ = is_wall(m, 0, y) ? L'┣' : L'┃';  /* left piece of frame */
    for (x = 0; x < m->w; ++x) {            /* a line from the maze */
      if (!is_wall(m, x, y)) { *p++ = L' '; continue; }
      n = is_wall(m, x, y-1);
      w = is_wall(m, x-1, y);
      e = is_wall(m, x+1, y);
      s = is_wall(m, x, y+1);
      idx = n | w<<1 | e<<2 | s<<3;
      *p++ = frame[idx-1];
    }
    *p++ = is_wall(m, x-1, y) ? L'┫' : L'┃';  /* right piece of frame */
    *p++ = L'\n';
  }

  /* bottom of frame */
  *p++ = L'┗';
  for (x = 0; x < m->w; ++x)
    *p++ = is_wall(m, x, y-1) ? L'┻' : L'━';
  *p++ = L'┛';
  *p++ = L'\n';

  *p = '\0';

  wprintf(L"%ls", g_dumpbuf);
}


/* Creates an uncompressed, 8-bit grayscale TGA image of the maze */
void
maze_dump_tga(const struct maze *const m, FILE *const tgaimg)
{
  struct tga_header header;
  size_t npixels, bufsize;
  unsigned char *buffer, *p;
  unsigned int x, y;

  header = (struct tga_header) {
    .id_len     = 0,
    .map_type   = TGA_NO_COLOR_MAP,
    .img_type   = TGA_UNCOMPRESSED_GRAYSCALE,
    .map_idx    = 0,
    .map_len    = 0,
    .map_elemsz = 0,
    .img_x      = 0,
    .img_y      = 0,
    .img_w      = m->w + 2, /* add frame width */
    .img_h      = m->h + 2, /* add frame width */
    .img_depth  = 8,
    .img_alpha  = 0,
    .img_dir    = 2,
  };

  npixels = header.img_w * header.img_h;
  bufsize = npixels * header.img_depth/8;
  buffer = p = malloc(bufsize);
  if (buffer == NULL)
    errx(EX_UNAVAILABLE, "%s: malloc", __func__);

  /* top of frame */
  for (x = 0; x < header.img_w; ++x)
    *p++ = 0x00;
  /* left side of frame, maze and right side of frame */
  for (y = 0; y < m->h; ++y) {
    *p++ = 0x00;                /* left piece of frame */
    for (x = 0; x < m->w; ++x)  /* a line from the maze */
      *p++ = is_wall(m, x, y) ? 0x00 : 0xFF;
    *p++ = 0x00;                /* right piece of frame */
  }
  /* bottom of frame */
  for (x = 0; x < header.img_w; ++x)
    *p++ = 0x00;

  fwrite(&header, 1, sizeof header, tgaimg);
  if (ferror(tgaimg)) errx(EX_IOERR, "%s: fwrite()", __func__);
  fwrite(buffer, 1, bufsize, tgaimg);
  if (ferror(tgaimg)) errx(EX_IOERR, "%s: fwrite()", __func__);

  fclose(tgaimg);
  free(buffer);
}


struct maze *
maze_new(unsigned short w, unsigned short h, enum mazegen gen)
{
  struct maze *maze;
  size_t bufsize, nelem;

  /* enforce a minimum maze size */
  if (w < 5) w = 5;
  if (h < 5) h = 5;
  /* remove the frame during the maze generation */
  w -= 2;
  h -= 2;
  /* make sure width and height are odd numbers */
  if (w % 2 == 0) --w;
  if (h % 2 == 0) --h;

  bufsize = w * h * sizeof*maze->buf;
  maze = malloc(sizeof*maze + bufsize);
  if (maze == NULL)
    errx(EX_UNAVAILABLE, "%s: malloc", __func__);

  maze->w = w;
  maze->h = h;

  nelem  = maze->w * maze->h;          /* the maze itself */
  nelem += 2*maze->w + 2*maze->h + 4;  /* its frame */
  nelem += maze->h + 2;                /* newlines after maze and frame */
  nelem += 1;                          /* the NUL terminator */
  g_dumpbuf = malloc(nelem * sizeof*g_dumpbuf);
  if (g_dumpbuf == NULL)
    errx(EX_UNAVAILABLE, "%s: malloc", __func__);

  if (g_steptime)
    printf(VT100_EraseScreen);

  switch (gen) {
  case mazegen_dfs: maze_gen_dfs(maze); break;
  case mazegen_div: maze_gen_div(maze); break;
  case mazegen_prm: maze_gen_prm(maze); break;
  }

  if (g_steptime)
    printf(VT100_CursorHome);

  return maze;
}


void
maze_setsteptime(unsigned int ms)
{
  g_steptime = ms * 1000; /* ms to µs */
}

