#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "maze.h"


#define every_direction  (north | west | east | south)
#define random_direction (1 << arc4random_uniform(4))

enum dir {
  north = 1 << 0,
  east  = 1 << 1,
  south = 1 << 2,
  west  = 1 << 3,
};

/* VT100 escape sequences */
static const char *const  VT100_CursorHome  = "\033[H";
static const char *const  VT100_EraseScreen = "\033[2J";

static const char  node_empty = ' ';
static const char  node_wall  = 'X';
static const char  node_list  = '?';
static const char  node_inval = '!';

static useconds_t  g_steptime;
static wchar_t    *g_dumpbuf;


struct maze {
  unsigned int  w;
  unsigned int  h;
  char          buf[];
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

static struct pos    pos_move(struct pos p, enum dir d, unsigned int step);
static char          node_get(const struct maze *const m, struct pos p);
static void          node_set(struct maze *const m, struct pos p, char type);
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
    errx(EX_UNAVAILABLE, "%s: alloc", __func__);
  list->len = 0;

  return (list);
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
  enum dir dirs[4] = { north, east, west, south };

  for (unsigned int i = 0; i < 4; ++i) {
    neighbor = pos_move(p, dirs[i], 2);
    if (node_get(m, neighbor) == node_wall) {
      node_set(m, neighbor, node_list);
      list_add(l, neighbor);
    }
  }
}


/* Helper function for maze_gen_prm() */
static struct pos
list_remove_random(struct list *const l)
{
  return (list_remove(l, arc4random_uniform(l->len)));
}


static struct pos
pos_move(struct pos p, enum dir d, unsigned int step)
{
  switch (d) {
  case west:  return (struct pos){ .x = p.x-step, .y = p.y };
  case east:  return (struct pos){ .x = p.x+step, .y = p.y };
  case north: return (struct pos){ .x = p.x, .y = p.y-step };
  case south: return (struct pos){ .x = p.x, .y = p.y+step };
  }
}


static char
node_get(const struct maze *const m, struct pos p)
{
  if ((p.x < m->w) && (p.y < m->h))
    return (m->buf[m->w*p.y + p.x]);
  else
    return node_inval;
}


static void
node_set(struct maze *const m, struct pos p, char type)
{
  if ((p.x < m->w) && (p.y < m->h))
    m->buf[m->w*p.y + p.x] = type;
}


/* Helper function for maze_dump() */
static int
is_wall(const struct maze *const m, unsigned int x, unsigned int y)
{
  if ((x < m->w) && (y < m->h))
    return (m->buf[m->w*y + x] != node_empty);
  else
    return 1;
}


/* Generates a maze via depth-first search */
static void
maze_gen_dfs(struct maze *const m)
{
  /* the maze is carved out of the walls */
  memset(m->buf, node_wall, m->w * m->h);

  /* start at a random position */
  struct pos p = {
    arc4random_uniform(m->w / 2) * 2,
    arc4random_uniform(m->h / 2) * 2,
  };
  maze_gen_dfs_recur(m, p);
}


static void
maze_gen_dfs_recur(struct maze *const m, struct pos p)
{
  struct pos neighbor, wall;
  unsigned int rnd, i;
  enum dir dir;

  maze_showstep(m);

  node_set(m, p, node_empty);

  /* try all directions in random order */
  enum dir dirs[4] = { north, south, east, west };
  for (i = 4; i > 0; --i) {
    rnd = arc4random_uniform(i);
    dir = dirs[rnd];
    dirs[rnd] = dirs[i-1];

    neighbor = pos_move(p, dir, 2);
    if (node_get(m, neighbor) == node_wall) {
      wall = pos_move(p, dir, 1);
      node_set(m, wall, node_empty);
      maze_gen_dfs_recur(m, neighbor);
    }
  }
}


/* Generates a maze via recursive division */
static void
maze_gen_div(struct maze *const m)
{
  /* the maze is created by constructing walls with passages */
  memset(m->buf, node_empty, m->w*m->h);

  struct region r = { 0, 0, m->w, m->h };
  maze_gen_div_recur(m, r);
}


static void
maze_gen_div_recur(struct maze *const m, struct region r)
{
  struct pos rnd, poi, wall, pass;
  unsigned int nlen, wlen, slen, elen;
  struct region nw, ne, sw, se;
  int dirs;

  if ((r.w == 1) || (r.h == 1))
    return;

  maze_showstep(m);

  /* divide the room into four sections by constructing two
   * intersecting walls */

  rnd.x = 1 + arc4random_uniform(r.w/2)*2;
  rnd.y = 1 + arc4random_uniform(r.h/2)*2;

  poi.x = r.x + rnd.x;
  poi.y = r.y + rnd.y;

  wall.x = r.x;
  wall.y = poi.y;
  for (; wall.x < r.x+r.w; ++wall.x)
    node_set(m, wall, node_wall);

  wall.x = poi.x;
  wall.y = r.y;
  for (; wall.y < r.y+r.h; ++wall.y)
    node_set(m, wall, node_wall);

  /* create passages on three of the four sides of the intersection
   * so all sections stay accessible */

  dirs = every_direction & ~random_direction;

  nlen = rnd.y;
  wlen = rnd.x;
  slen = r.h - rnd.y;
  elen = r.w - rnd.x;

  if (dirs & north) {
    pass = pos_move(poi, north, 1 + arc4random_uniform(nlen/2)*2);
    node_set(m, pass, node_empty);
  }
  if (dirs & west) {
    pass = pos_move(poi, west,  1 + arc4random_uniform(wlen/2)*2);
    node_set(m, pass, node_empty);
  }
  if (dirs & south) {
    pass = pos_move(poi, south, 1 + arc4random_uniform(slen/2)*2);
    node_set(m, pass, node_empty);
  }
  if (dirs & east) {
    pass = pos_move(poi, east,  1 + arc4random_uniform(elen/2)*2);
    node_set(m, pass, node_empty);
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
  unsigned int rnd, i;
  enum dir dir;

  l = list_new((m->w+m->h)*4); /* XXX: Value guesstimated by observing the
                                       maximum list size of a couple of runs */
  memset(m->buf, node_wall, m->w * m->h);

  current.x = arc4random_uniform(m->w/2)*2;
  current.y = arc4random_uniform(m->h/2)*2;
  node_set(m, current, node_empty);

  list_add_unvisited_neighbors(m, l, current);

  while (l->len > 0) {
    maze_showstep(m);

    current = list_remove_random(l);
    node_set(m, current, node_empty);

    /* try all directions in random order... */
    enum dir dirs[4] = { north, south, east, west };
    for (i = 4; i > 0; --i) {
      rnd = arc4random_uniform(i);
      dir = dirs[rnd];
      dirs[rnd] = dirs[i-1];

      neighbor = pos_move(current, dir, 2);
      if (node_get(m, neighbor) == node_empty) {
        wall = pos_move(current, dir, 1);
        node_set(m, wall, node_empty);
        /* ...but stop as soon as unvisited neighbor has been found */
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
    maze_dump(m);
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
maze_dump(const struct maze *const m)
{
  wchar_t *p;
  unsigned int x, y;
  int n, e, s, w;

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
      e = is_wall(m, x+1, y);
      s = is_wall(m, x, y+1);
      w = is_wall(m, x-1, y);
      if (n && s && e && w) { *p++ = L'╋'; continue; }
      if (n && s && e)      { *p++ = L'┣'; continue; }
      if (n && s && w)      { *p++ = L'┫'; continue; }
      if (e && w && n)      { *p++ = L'┻'; continue; }
      if (e && w && s)      { *p++ = L'┳'; continue; }
      if (s && e)           { *p++ = L'┏'; continue; }
      if (s && w)           { *p++ = L'┓'; continue; }
      if (n && e)           { *p++ = L'┗'; continue; }
      if (n && w)           { *p++ = L'┛'; continue; }
      if (n && s)           { *p++ = L'┃'; continue; }
      if (w && e)           { *p++ = L'━'; continue; }
      if (n)                { *p++ = L'╹'; continue; }
      if (e)                { *p++ = L'╺'; continue; }
      if (s)                { *p++ = L'╻'; continue; }
      if (w)                { *p++ = L'╸'; continue; }
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


struct maze *
maze_new(unsigned int w, unsigned int h, enum maze_gen gen)
{
  struct maze *m;
  size_t bufsize;

  if (w < 5) w = 5;
  if (h < 5) h = 5;

  /* keep the effective width and effective height even
   * by making them odd (Q: wat?! A: think [0..3]) */
  if (w % 2 == 0) --w;
  if (h % 2 == 0) --h;

  bufsize = w * h * sizeof*m->buf;
  m = malloc(sizeof*m + bufsize);
  if (m == NULL)
    errx(EX_UNAVAILABLE, "%s: alloc", __func__);

  m->w = w;
  m->h = h;

  /* XXX: Again, this does not belong in here */
  size_t nelem;
  nelem  = m->w * m->h;          /* the maze itself */
  nelem += 2*m->w + 2*m->h + 4;  /* its frame */
  nelem += m->h + 2 + 1;         /* some newlines and a NUL terminator */
  g_dumpbuf = malloc(nelem * sizeof*g_dumpbuf);
  if (g_dumpbuf == NULL)
    errx(EX_UNAVAILABLE, "%s: alloc", __func__);

  if (g_steptime)
    printf(VT100_EraseScreen);

  switch (gen) {
  case maze_dfs: maze_gen_dfs(m); break;
  case maze_div: maze_gen_div(m); break;
  case maze_prm: maze_gen_prm(m); break;
  }

  if (g_steptime)
    printf(VT100_CursorHome);

  return (m);
}


void
maze_setsteptime(unsigned int ms)
{
  g_steptime = ms * 1000; /* ms to µm */
}
