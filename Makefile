# Makefile

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99
LDFLAGS  = -Wl,--allow-multiple-definition
LIBS     = -lrt -pthread

DUNGEON_OBJ = dungeon.o

.PHONY: all clean

all: game barbarian wizard rogue

game: game.c $(DUNGEON_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

barbarian: barbarian.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

wizard: wizard.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

rogue: rogue.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f game barbarian wizard rogue
