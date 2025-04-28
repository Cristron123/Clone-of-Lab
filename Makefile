CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99
LIBS    = -lrt -pthread
DUNGEON = dungeon.o

all: game barbarian wizard rogue

game: game.c $(DUNGEON)
	$(CC) $(CFLAGS) -o $@ game.c $(DUNGEON) $(LIBS)

barbarian: barbarian.c
	$(CC) $(CFLAGS) -o $@ barbarian.c $(LIBS)

wizard: wizard.c
	$(CC) $(CFLAGS) -o $@ wizard.c $(LIBS)

rogue: rogue.c
	$(CC) $(CFLAGS) -o $@ rogue.c $(LIBS)

clean:
	rm -f game barbarian wizard rogue

