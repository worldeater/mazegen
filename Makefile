.include <bsd.compiler.mk>

PROG = maze
NO_MAN =

SRCS =  main.c
SRCS += maze.c


CSTD = c11
SSP_CFLAGS =

.if make(debug)
CFLAGS += -O0 -g
.else
CFLAGS += -O3 -fomit-frame-pointer -DNDEBUG
LDFLAGS = -s
.endif

.if ${COMPILER_TYPE} == "clang"
CFLAGS += -pedantic -Werror -Weverything -fcolor-diagnostics
.else  # assume it's GCC (or something compatible)
CFLAGS += -pedantic -Werror -Wall -Wextra
.endif


debug: all


.include <bsd.prog.mk>
