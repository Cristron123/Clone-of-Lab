// barbarian.c
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
static sem_t *lever1;

// This handler is invoked for both the attack round (DUNGEON_SIGNAL)
// and the lever phase (SEMAPHORE_SIGNAL).
void handler(int sig) {
    if (sig == DUNGEON_SIGNAL) {
        // Copy the enemy's health into attack
        d->barbarian.attack = d->enemy.health;
    }
    else if (sig == SEMAPHORE_SIGNAL) {
        // Hold Lever One until Rogue releases it
        sem_wait(lever1);
    }
}

int main(void) {
    // 1) Map shared memory
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0);
    d = mmap(NULL, sizeof(*d),
             PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
    if (d == MAP_FAILED) { perror("mmap"); exit(1); }

    // 2) Open Lever One semaphore (initial value = 0)
    lever1 = sem_open(dungeon_lever_one, 0);
    if (lever1 == SEM_FAILED) { perror("sem_open lever1"); exit(1); }

    // 3) Install the signal handler
    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    // 4) Wait until the dungeon ends
    while (d->running) {
        pause();
    }
    return 0;
}

