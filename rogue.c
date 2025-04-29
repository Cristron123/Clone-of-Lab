// rogue.c
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

// For binary search state
static float low, high;
static int   init_search = 0;

// Perform or adjust a binary-search guess
void do_binary_search(void) {
    if (!init_search) {
        low  = 0.0f;
        high = MAX_PICK_ANGLE;
        init_search = 1;
    } else {
        char dir = d->trap.direction;
        if (dir == 'u') {
            low = d->rogue.pick;
        } else if (dir == 'd') {
            high = d->rogue.pick;
        } else if (dir == '-') {
            return;  // Guess is within LOCK_THRESHOLD
        }
    }
    d->rogue.pick = (low + high) / 2.0f;

    // debug print to watch convergence
    printf("[rogue] tick: direction=%c, new pick=%.6f\n",
           d->trap.direction, d->rogue.pick);
    fflush(stdout);
}

void handler(int sig) {
    if (sig == DUNGEON_SIGNAL) {
        do_binary_search();
    }
    else if (sig == SEMAPHORE_SIGNAL) {
        // Copy all 4 treasure chars into spoils
        for (int i = 0; i < 4; ++i) {
            d->spoils[i] = d->treasure[i];
        }
        // Release both levers so Barbarian + Wizard unblock
        sem_post(lever1);
        sem_post(lever2);

        // reset search for next trap
        init_search = 0;
    }
}

int main(void) {
    // 1) Map shared memory
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0);
    d = mmap(NULL, sizeof(*d),
             PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
    if (d == MAP_FAILED) { perror("mmap"); exit(1); }

    // 2) Open both lever semaphores
    lever1 = sem_open(dungeon_lever_one, 0);
    lever2 = sem_open(dungeon_lever_two, 0);
    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("sem_open lever"); exit(1);
    }

    // 3) Install a single handler
    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    // 4) Loop until dungeon terminates
    while (d->running) {
        pause();
    }
    return 0;
}
