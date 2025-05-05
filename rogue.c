/*
rogue.c

This process represents the Rogue character in the dungeon game. 
It connects to shared memory and semaphores to interact with the Dungeon Master. The Rogue attempts to disarm traps using a binary search approach and collects treasure from the treasure room after the Barbarian and Wizard hold the levers.

Key Features:
1. **Shared Memory**: Connects to the shared Dungeon structure.
2. **Semaphore Usage**: Manages access to levers for treasure collection.
3. **Trap Disarming**: Implements a binary search algorithm to disarm traps.
4. **Signal Handling**: Responds to signals for dungeon events and cleanup.
 */

#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "dungeon_info.h"
#include "dungeon_settings.h"

static struct Dungeon *dungeon_ptr = NULL;
static sem_t *lever1_sem = SEM_FAILED, *lever2_sem = SEM_FAILED;
static int shm_fd = -1;
volatile sig_atomic_t exit_flag = 0;

void error_exit(const char *msg) {
    perror(msg);
    if (dungeon_ptr && dungeon_ptr != MAP_FAILED) {
        munmap(dungeon_ptr, sizeof(*dungeon_ptr));
    }
    if (lever1_sem != SEM_FAILED) sem_close(lever1_sem);
    if (lever2_sem != SEM_FAILED) sem_close(lever2_sem);
    exit(EXIT_FAILURE);
}

void rogue_signal_handler(int signum) {
    static float current_low = 0.0f, current_high = MAX_PICK_ANGLE;

    if (signum == SIGINT) {
        exit_flag = 1;
        return;
    }
    if (!dungeon_ptr || !dungeon_ptr->running) return;

    if (signum == DUNGEON_SIGNAL) {
        /* Trap‐disarm binary search */
        if (dungeon_ptr->trap.locked) {
            /* Reset on fresh start */
            char dir0 = dungeon_ptr->trap.direction;
            if (dir0!='u' && dir0!='d' && dir0!='-') {
                current_low  = 0.0f;
                current_high = MAX_PICK_ANGLE;
            }

            time_t start = time(NULL);
            while (dungeon_ptr->trap.locked && dungeon_ptr->running && !exit_flag) {
                if (difftime(time(NULL), start) > (SECONDS_TO_PICK - 0.5)) break;

                char feedback = dungeon_ptr->trap.direction;
                float last_pick = dungeon_ptr->rogue.pick;

                if (feedback == '-') {
                    break;
                }
                else if (feedback == 'u') {
                    if (last_pick > current_low)
                        current_low = last_pick;
                }
                else if (feedback == 'd') {
                    if (last_pick < current_high)
                        current_high = last_pick;
                }
                else {
                    continue;
                }

                if (current_high - current_low < 1e-6) break;
                float next = current_low + (current_high - current_low) / 2.0f;
                dungeon_ptr->rogue.pick     = next;
                dungeon_ptr->trap.direction = 't';
            }

            if (!dungeon_ptr->trap.locked) {
                current_low  = 0.0f;
                current_high = MAX_PICK_ANGLE;
            }
        }
        else {
            /* trap already unlocked — reset for next */
            current_low  = 0.0f;
            current_high = MAX_PICK_ANGLE;
        }
    }
    else if (signum == SEMAPHORE_SIGNAL) {
        /* Treasure‐room collection */
        int got = 0;
        memset(dungeon_ptr->spoils, 0, sizeof(dungeon_ptr->spoils));
        time_t ts = time(NULL);

        while (dungeon_ptr->running && !exit_flag && got < 4) {
            if (difftime(time(NULL), ts) > TIME_TREASURE_AVAILABLE) break;
            if (dungeon_ptr->treasure[got]) {
                dungeon_ptr->spoils[got] = dungeon_ptr->treasure[got];
                got++;
            }
        }
        /* release levers for Barbarian & Wizard */
        sem_post(lever1_sem);
        sem_post(lever2_sem);
    }
}

int main(void) {
    /* 1. Shared memory */
    shm_fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) error_exit("shm_open");
    dungeon_ptr = mmap(NULL, sizeof(*dungeon_ptr),
                       PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (dungeon_ptr == MAP_FAILED) error_exit("mmap");
    close(shm_fd);

    /* 2. Semaphores */
    lever1_sem = sem_open(dungeon_lever_one, 0);
    lever2_sem = sem_open(dungeon_lever_two, 0);
    if (lever1_sem==SEM_FAILED || lever2_sem==SEM_FAILED)
        error_exit("sem_open");

    /* 3. Initial pick */
    dungeon_ptr->rogue.pick     = MAX_PICK_ANGLE/2.0f;
    dungeon_ptr->trap.direction = 't';

    /* 4. Signal handlers */
    struct sigaction sa = { .sa_handler = rogue_signal_handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* 5. Wait for dungeon to finish */
    while (dungeon_ptr->running && !exit_flag) {
        pause();
    }

    /* Cleanup */
    munmap(dungeon_ptr, sizeof(*dungeon_ptr));
    sem_close(lever1_sem);
    sem_close(lever2_sem);
    return 0;
}
