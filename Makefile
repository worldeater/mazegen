PROG = maze
NO_MAN =

SRCS =  main.c
SRCS += maze.c


CSTD = c11
CFLAGS = -pipe -pedantic -Werror -march=native
LDFLAGS =

.if ${CC:T:Mclang*}
CFLAGS += -Weverything -fcolor-diagnostics
.elif ${CC:T:Mgcc*}
# suppress a bogus warning for maze.c:pov_move()
CFLAGS += -Wall -Wextra -Wno-error=return-type
.endif


.if make(release)
CFLAGS += -O3 -fomit-frame-pointer -DNDEBUG
LDFLAGS += -s
release: all
.endif

.if make(debug)
CFLAGS += -O0 -g
debug: all
.endif

.if make(analyze)
STATIC_ANALYZER ?= /usr/local/bin/scan-build35
analyze:
	$(STATIC_ANALYZER) make clean debug
.endif


.include <bsd.prog.mk>
