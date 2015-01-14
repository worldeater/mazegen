A command line tool for generating mazes. Written in C on FreeBSD.

```
Usage: maze <gen> <width> <height> [delay]

  gen      0, for Prim's Algorithm
           1, for depth-first search
           2, for recursive division
  delay    0..1000, milliseconds to wait after each frame
           if omitted or zero only the final maze is shown

Example: maze 0 70 20 50
```
