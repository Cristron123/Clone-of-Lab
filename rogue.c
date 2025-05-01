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

    if (!init_search) {
        // First time: initialize bounds
        low         = 0.0f;
        high        = MAX_PICK_ANGLE;
        init_search = 1;
    }
    else if (dir == 'u') {
        // Pick was too low → raise low bound
        low = d->rogue.pick;
    }
    else if (dir == 'd') {
        // Pick was too high → lower high bound
        high = d->rogue.pick;
    }
    else if (dir == '-') {
        // Success! reset for next lock
        init_search = 0;
        return;
    }
    else {
        // Still waiting on dungeon (“t”) or spurious; do nothing
        return;
    }

    // Compute next midpoint, publish it, and signal “new guess ready”
    float next = (low + high) / 2.0f;
    d->rogue.pick     = next;
    d->trap.direction = 't';
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
        // Release both levers so Barbarian + Wizard can finish up
        sem_post(lever1);
        sem_post(lever2);
        // Prepare binary‐search state for next time
        init_search = 0;
    }
}

int main(void) {
    // 1) Open and map shared memory
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0);
    if (fd == -1) { perror("shm_open"); exit(1); }

    d = mmap(NULL, sizeof(*d),
             PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
    if (d == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);

    // 2) Open both lever semaphores
    lever1 = sem_open(dungeon_lever_one, 0);
    lever2 = sem_open(dungeon_lever_two, 0);
    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("sem_open lever"); exit(1);
    }

    // 3) Prime the very first guess, but let the handler initialize bounds
    d->rogue.pick     = MAX_PICK_ANGLE / 2.0f;
    d->trap.direction = 't';
    init_search       = 0;  // clear so first signal sets low/high

    // 4) Install our single handler for both signals
    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    // 5) Loop until dungeon ends
    while (d->running) {
        pause();
    }

    return 0;
}
