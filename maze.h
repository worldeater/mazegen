#ifndef MAZE_H_
#define MAZE_H_

#include <stdio.h>


enum mazegen {
  mazegen_prm,
  mazegen_dfs,
  mazegen_div,
  mazegen_max = mazegen_div
};

struct maze;

void         maze_delete(struct maze *m);
void         maze_dump_txt(const struct maze *const m);
void         maze_dump_tga(const struct maze *const m, FILE *const tgaimg);
struct maze *maze_new(unsigned short cols, unsigned short rows, enum mazegen gen);
void         maze_setsteptime(unsigned int ms);

#endif /* MAZE_H_ */
