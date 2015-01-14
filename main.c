#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "maze.h"


extern const char *const __progname;


static _Noreturn void  die(int);
static unsigned int    read_arg(const char *const s, const char *const arg, unsigned int min, unsigned int max);


static void
die(int status)
{
  const char *const usage =
    "Usage: %s <gen> <width> <height> [delay]\n"
    "\n"
    "  gen      0, for Prim's Algorithm\n"
    "           1, for depth-first search\n"
    "           2, for recursive division\n"
    "  delay    0..1000, milliseconds to wait after each frame\n"
    "           if omitted or zero only the final maze is shown\n"
    "\n"
    "Example: %s 0 70 20 50\n"
    "\n";

  if (status == EX_USAGE) {
    fprintf(stderr, usage, __progname, __progname);
    exit(status);
  } else
    errx(status, "%s", strerror(errno));
}


static unsigned int
read_arg(const char *const str, const char *const arg, unsigned int min, unsigned int max)
{
  long long n;

  errno = 0;
  n = strtonum(str, min, max, NULL);
  if ((n == 0) && (errno != 0)) {
    fprintf(stderr, "%s: Parameter <%s> is out of range\n\n", __progname, arg);
    die(EX_USAGE);
  }

  return (unsigned int)n;
}


int
main(int argc, char **argv)
{
  unsigned int gen, width, height, delay;
  struct maze *m;

  setlocale(LC_ALL, "");

  delay = 0;
  switch (argc) {
  case 5:
    delay  = read_arg(argv[4], "delay",  0, 1000);
    /* FALLTHROUGH */
  case 4:
    gen    = read_arg(argv[1], "gen",    0, 3);
    width  = read_arg(argv[2], "width",  0, UINT_MAX);
    height = read_arg(argv[3], "height", 0, UINT_MAX);
    break;
  default:
    die(EX_USAGE);
  }

  maze_setsteptime(delay);
  m = maze_new(width, height, gen);
  maze_dump(m);
  maze_delete(m);

  exit(EX_OK);
}

