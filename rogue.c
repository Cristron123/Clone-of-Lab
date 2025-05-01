/*
 * rogue.c - Merged improvements for binary-search trap logic and treasure collection
 */
#define _XOPEN_SOURCE 700       // for usleep(), sigaction, etc.
#define _POSIX_C_SOURCE 200809L // for POSIX.1-2008 compliance

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include "dungeon_info.h"   // shared-memory struct and names
#include "dungeon_settings.h" // signals, MAX_PICK_ANGLE, time constants

// Global handles
static struct Dungeon *d = NULL;
static sem_t *lever1 = SEM_FAILED;
static sem_t *lever2 = SEM_FAILED;
static int shm_fd = -1;
volatile sig_atomic_t exit_flag = 0;

// Signal handler for SIGINT -> graceful exit
static void handle_sigint(int sig) {
    void(sig);
    exit_flag = 1;
}

// Main signal handler: trap ticks and treasure semaphore
static void rogue_handler(int sig) {
    static float low = 0.0f, high = MAX_PICK_ANGLE;
    static bool inited = false;

    if (sig == SIGINT) {
        exit_flag = 1;
        return;
    }
    if (!d || !d->running || exit_flag) return;

    if (sig == DUNGEON_SIGNAL) {
        // On first tick for this trap, reset bounds
        if (!inited || (d->trap.direction != 'u' && d->trap.direction != 'd' && d->trap.direction != '-')) {
            low = 0.0f;
            high = MAX_PICK_ANGLE;
            inited = true;
        }
        // If still locked, binary-search
        if (d->trap.locked) {
            char dir = d->trap.direction;
            if (dir == 'u')      low  = d->rogue.pick;
            else if (dir == 'd') high = d->rogue.pick;
            else if (dir == '-') {
                // found it!
                inited = false;
                return;
            }
            // compute next guess
            if (high - low > 0.0001f) {
                d->rogue.pick = low + (high - low) / 2.0f;
                d->trap.direction = 't'; // tell dungeon we guessed
            } else {
                // range too small, give up
                inited = false;
            }
        } else {
            // trap already unlocked
            inited = false;
        }
    }
    else if (sig == SEMAPHORE_SIGNAL) {
        // Treasure stage: copy 4 chars
        for (int i = 0; i < 4; i++) {
            d->spoils[i] = d->treasure[i];
        }
        // release levers
        sem_post(lever1);
        sem_post(lever2);
        // reset for next trap
        inited = false;
    }
}

int main(void) {
    struct sigaction sa;

    // 1) Open shared memory
    shm_fd = shm_open(dungeon_shm_name, O_RDWR, 0);
    if (shm_fd < 0) { perror("shm_open"); exit(1); }
    d = mmap(NULL, sizeof(*d), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (d == MAP_FAILED) { perror("mmap"); exit(1); }
    close(shm_fd);

    // 2) Open semaphores
    lever1 = sem_open(dungeon_lever_one, O_RDWR);
    lever2 = sem_open(dungeon_lever_two, O_RDWR);
    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("sem_open"); exit(1);
    }

    // 3) Set SIGINT handler
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // 4) Set rogue_handler for dungeon + semaphore signals
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = rogue_handler;
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    // 5) Initial pick to middle
    d->rogue.pick     = MAX_PICK_ANGLE / 2.0f;
    d->trap.direction = 't';

    // 6) Main loop
    while (d->running && !exit_flag) {
        pause();
    }

    // Cleanup
    munmap(d, sizeof(*d));
    sem_close(lever1);
    sem_close(lever2);
    return 0;
}
