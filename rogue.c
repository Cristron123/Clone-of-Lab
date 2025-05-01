// rogue.c
// The Rogue process: binary‐search lock‐picking + treasure collection.

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include "dungeon_info.h"
#include "dungeon_settings.h"

static struct Dungeon *d;
static sem_t *lever1, *lever2;

// Binary‐search state
static float low, high;
static int   init_search = 0;

// On each DUNGEON_SIGNAL, adjust our guess:
void do_binary_search(void) {
    char dir = d->trap.direction;

    if (!init_search) {
        // first guess
        low         = 0.0f;
        high        = MAX_PICK_ANGLE;
        init_search = 1;
    }
    else if (dir == 'u') {
        // pick too low → raise low bound
        low = d->rogue.pick;
    }
    else if (dir == 'd') {
        // pick too high → lower high bound
        high = d->rogue.pick;
    }
    else if (dir == '-') {
        // success!
        init_search = 0;
        return;
    }
    else {
        // still waiting or unrecognized; do nothing
        return;
    }

    // compute next midpoint and publish it
    d->rogue.pick = (low + high) / 2.0f;
}

// Handles both lock‐pick signals and treasure‐room signals
void handler(int sig) {
    if (sig == DUNGEON_SIGNAL) {
        do_binary_search();
    }
    else if (sig == SEMAPHORE_SIGNAL) {
        // copy treasure → spoils
        for (int i = 0; i < 4; ++i) {
            d->spoils[i] = d->treasure[i];
        }
        // release the levers
        sem_post(lever1);
        sem_post(lever2);
        // reset for next lock
        init_search = 0;
    }
}

int main(void) {
    // 1) open & map shared memory
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0);
    if (fd == -1) { perror("shm_open"); exit(1); }

    d = mmap(NULL, sizeof(*d),
             PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (d == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);

    // 2) open the two lever semaphores
    lever1 = sem_open(dungeon_lever_one, 0);
    lever2 = sem_open(dungeon_lever_two, 0);
    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("sem_open"); exit(1);
    }

    // 3) prime initial pick so Dungeon can start us off
    d->rogue.pick = MAX_PICK_ANGLE / 2.0f;
    init_search   = 0;

    // 4) install one handler for both signals
    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    // 5) wait here until dungeon tells us it's done
    while (d->running) {
        pause();
    }

    return 0;
}
