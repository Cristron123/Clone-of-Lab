// rogue.c
// This process represents the Rogue. It uses a binary‐search strategy
// to pick locks and then collects treasure once the levers are released.

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

// State for our binary search
static float low, high;
static int   init_search = 0;

// Perform or adjust a binary‐search guess
void do_binary_search(void) {
    char dir = d->trap.direction;

    // On success, reset so next dungeon SIGNAL re‐initializes
    if (dir == '-') {
        init_search = 0;
        return;
    }

    // If dungeon is still waiting on your guess, do nothing
    if (dir == 't') {
        return;
    }

    // Initialize your bounds the very first time
    if (!init_search) {
        low         = 0.0f;
        high        = MAX_PICK_ANGLE;
        init_search = 1;
    }
    // Adjust bounds for "too low" (w) or "too high" (d)
    else if (dir == 'w') {
        low = d->rogue.pick;
    }
    else if (dir == 'd') {
        high = d->rogue.pick;
    }
    // Any other spurious signal, skip
    else {
        return;
    }

    // Compute and publish the midpoint, then tell dungeon you're ready
    float next = (low + high) / 2.0f;
    d->rogue.pick     = next;
    d->trap.direction = 't';
}

void handler(int sig) {
    if (sig == DUNGEON_SIGNAL) {
        do_binary_search();
    }
    else if (sig == SEMAPHORE_SIGNAL) {
        // Copy treasure into our spoils and flip the levers
        for (int i = 0; i < 4; ++i) {
            d->spoils[i] = d->treasure[i];
        }
        sem_post(lever1);
        sem_post(lever2);
        init_search = 0;  // start fresh next lock
    }
}

int main(void) {
    // 1) Map the shared dungeon memory
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0);
    if (fd == -1) { perror("shm_open"); exit(1); }

    d = mmap(NULL, sizeof(*d),
             PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
    if (d == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);

    // 2) Open lever semaphores
    lever1 = sem_open(dungeon_lever_one, 0);
    lever2 = sem_open(dungeon_lever_two, 0);
    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("sem_open lever"); exit(1);
    }

    // 3) Install our single handler for both signals
    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    // 4) Prime nothing here—wait for the first DUNGEON_SIGNAL to kick off
    init_search = 0;
    d->trap.direction = 't';  // avoid accidental '-' at startup

    // 5) Run until the dungeon ends
    while (d->running) {
        pause();
    }

    return 0;
}
