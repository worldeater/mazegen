#ifndef MAZE_H_
#define MAZE_H_

enum maze_gen {
  maze_prm,
  maze_dfs,
  maze_div,
};

struct maze;

void         maze_delete(struct maze *m);
void         maze_dump(const struct maze *const m);
struct maze *maze_new(unsigned cols, unsigned rows, enum maze_gen gen);
void         maze_setsteptime(unsigned int ms);

#endif // MAZE_H_
