#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "maze.h"


extern const char *const __progname;

static const enum mazegen   default_gen  = mazegen_prm;
static const unsigned short default_size = 23;


static _Noreturn void  usage(void);


static void
usage(void)
{
  const char *const usage =
    "Usage: %s [-w 5..10000] [-h 5..10000] [-g 0|1|2] [-s 1..1000] [-f filename]\n"
    "\n"
    "  -w, -h : if even, decremented to the next smallest odd number (default: %u)\n"
    "\n"
    "  -g : algorithm used for maze generation (default: %u)\n"
    "         0 : Prim's Algorithm\n"
    "         1 : depth-first search\n"
    "         2 : recursive division\n"
    "\n"
    "  -s : show progress, waits the given number of milliseconds after each step\n"
    "\n"
    "  -f : write maze to file, creates an uncompressed, 8-bit grayscale TGA image,\n"
    "       no output to stdout\n"
    "\n"
    "Examples: %s -g1 -w70 -h20 -s50\n"
    "          %s -w500 -h500 -f./maze_prim_499x499.tga\n"
    "\n";

  fprintf(stderr, usage, __progname, default_size, default_gen, __progname, __progname);
  exit(EX_USAGE);
}


int
main(int argc, char **argv)
{
  enum mazegen generator;
  unsigned short width, height, steptime, minval, maxval;
  FILE *tgaimg;
  struct maze *maze;
  int ch;

  generator = default_gen;
  width = height = default_size;
  steptime = 0;
  tgaimg = NULL;

  setlocale(LC_ALL, "");

  opterr = 0;
  while ((ch = getopt(argc, argv, "g:w:h:f:s:")) != -1) {
    switch (ch) {
    case 'g':
      minval = 0; maxval = mazegen_max;
      generator = (enum mazegen)(int)strtonum(optarg, minval, maxval, NULL);
      if (generator == 0 && errno != 0)
        errx(EX_DATAERR, "valid range for -%c: [%d..%d]", optopt, minval, maxval);
      break;
    case 'w':
      minval = 5; maxval = 10000;
      width = (unsigned short)strtonum(optarg, minval, maxval, NULL);
      if (width == 0 && errno != 0)
        errx(EX_DATAERR, "valid range for -%c: [%d..%d]", optopt, minval, maxval);
      break;
    case 'h':
      minval = 5; maxval = 10000;
      height = (unsigned short)strtonum(optarg, minval, maxval, NULL);
      if (height == 0 && errno != 0)
        errx(EX_DATAERR, "valid range for -%c: [%d..%d]", optopt, minval, maxval);
      break;
    case 's':
      minval = 1; maxval = 1000;
      steptime = (unsigned short)strtonum(optarg, minval, maxval, NULL);
      if (steptime == 0 && errno != 0)
        errx(EX_DATAERR, "valid range for -%c: [%d..%d]", optopt, minval, maxval);
      break;
    case 'f':
      tgaimg = fopen(optarg, "w");
      if (tgaimg == NULL)
        errx(EX_IOERR, "Could not open %s: %s", optarg, strerror(errno));
      break;
    default:
      usage();
    }
  }

  if (steptime && tgaimg)
    steptime = 0; /* file output wins, suppress console output */

  maze_setsteptime(steptime);
  maze = maze_new(width, height, generator);
  if (tgaimg)
    maze_dump_tga(maze, tgaimg);
  else
    maze_dump_txt(maze);
  maze_delete(maze);

  exit(EX_OK);
}

