A command line tool for generating mazes. Written in C on FreeBSD.

  Usage: maze [-w 5..10000] [-h 5..10000] [-g 0|1|2] [-s 1..1000] [-f filename]

    -w, -h : if even, decremented to the next smallest odd number (default: 23)

    -g : algorithm used for maze generation (default: 0)
          0  for Prim's Algorithm
          1  for depth-first search
          2  for recursive division

    -s : show progress, waits the given number of milliseconds after each step

    -f : write maze to file, creates an uncompressed, 8-bit grayscale TGA image,
         no output to stdout

  Examples: maze -g1 -w70 -h20 -s50
            maze -w500 -h500 -f./maze_prim_499x499.tga

