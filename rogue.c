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

static struct Dungeon *dungeon_ptr = NULL; // Pointer to the Dungeon structure
static sem_t *lever1_sem = SEM_FAILED, *lever2_sem = SEM_FAILED; // Semaphore pointers
static int shm_fd = -1; // Shared memory file descriptor
volatile sig_atomic_t exit_flag = 0; // Flag for graceful exit

// Function to handle errors and cleanup resources
void error_exit(const char *msg) {
    perror(msg);
    if (dungeon_ptr && dungeon_ptr != MAP_FAILED) {
        munmap(dungeon_ptr, sizeof(*dungeon_ptr)); // Unmap shared memory
    }
    if (lever1_sem != SEM_FAILED) sem_close(lever1_sem); // Close semaphore 1
    if (lever2_sem != SEM_FAILED) sem_close(lever2_sem); // Close semaphore 2
    exit(EXIT_FAILURE);
}

// Signal handler for Rogue actions
void rogue_signal_handler(int signum) {
    static float current_low = 0.0f, current_high = MAX_PICK_ANGLE; // Variables for binary search

    if (signum == SIGINT) {
        exit_flag = 1; // Set exit flag on interrupt signal
        return;
    }
    if (!dungeon_ptr || !dungeon_ptr->running) return; // Check dungeon state

    if (signum == DUNGEON_SIGNAL) {
        // Trap-disarm binary search
        if (dungeon_ptr->trap.locked) {
            // Reset on fresh start
            char dir0 = dungeon_ptr->trap.direction;
            if (dir0 != 'u' && dir0 != 'd' && dir0 != '-') {
                current_low  = 0.0f;
                current_high = MAX_PICK_ANGLE;
            }

            time_t start = time(NULL); // Start time for disarming
            while (dungeon_ptr->trap.locked && dungeon_ptr->running && !exit_flag) {
                if (difftime(time(NULL), start) > (SECONDS_TO_PICK - 0.5)) break; // Timeout check

                char feedback = dungeon_ptr->trap.direction; // Get feedback from the trap
                float last_pick = dungeon_ptr->rogue.pick; // Last pick angle

                // Update binary search bounds based on feedback
                if (feedback == '-') {
                    break; // Trap disarmed
                } else if (feedback == 'u') {
                    if (last_pick > current_low)
                        current_low = last_pick; // Update lower bound
                } else if (feedback == 'd') {
                    if (last_pick < current_high)
                        current_high = last_pick; // Update upper bound
                } else {
                    continue; // Invalid feedback
                }

                if (current_high - current_low < 1e-6) break; // Stop if close enough
                float next = current_low + (current_high - current_low) / 2.0f; // Next pick angle
                dungeon_ptr->rogue.pick     = next; // Update pick angle
                dungeon_ptr->trap.direction = 't'; // Signal trap to test pick
            }

            if (!dungeon_ptr->trap.locked) {
                current_low  = 0.0f; // Reset on success
                current_high = MAX_PICK_ANGLE;
            }
        } else {
            // Trap already unlocked — reset for next attempt
            current_low  = 0.0f;
            current_high = MAX_PICK_ANGLE;
        }
    } else if (signum == SEMAPHORE_SIGNAL) {
        // Treasure-room collection
        int got = 0; // Count of treasures collected
        memset(dungeon_ptr->spoils, 0, sizeof(dungeon_ptr->spoils)); // Clear spoils
        time_t ts = time(NULL); // Start time for collection

        while (dungeon_ptr->running && !exit_flag && got < 4) {
            if (difftime(time(NULL), ts) > TIME_TREASURE_AVAILABLE) break; // Timeout check
            if (dungeon_ptr->treasure[got]) {
                dungeon_ptr->spoils[got] = dungeon_ptr->treasure[got]; // Collect treasure
                got++;
            }
        }
        // Release levers for Barbarian & Wizard
        sem_post(lever1_sem);
        sem_post(lever2_sem);
    }
}

int main(void) {
    // 1. Shared memory
    shm_fd = shm_open(dungeon_shm_name, O_RDWR, 0666);
    if (shm_fd == -1) error_exit("shm_open");
    dungeon_ptr = mmap(NULL, sizeof(*dungeon_ptr),
                       PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (dungeon_ptr == MAP_FAILED) error_exit("mmap");
    close(shm_fd); // Close the file descriptor after mapping

    // 2. Semaphores
    lever1_sem = sem_open(dungeon_lever_one, 0);
    lever2_sem = sem_open(dungeon_lever_two, 0);
    if (lever1_sem == SEM_FAILED || lever2_sem == SEM_FAILED)
        error_exit("sem_open");

    // 3. Initial pick
    dungeon_ptr->rogue.pick     = MAX_PICK_ANGLE / 2.0f; // Set initial pick angle
    dungeon_ptr->trap.direction = 't'; // Initialize trap direction

    // 4. Signal handlers
    struct sigaction sa = { .sa_handler = rogue_signal_handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL); // Handle dungeon signal
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL); // Handle semaphore signal
    sigaction(SIGINT, &sa, NULL); // Handle interrupt signal

    // 5. Wait for dungeon to finish
    while (dungeon_ptr->running && !exit_flag) {
        pause(); // Wait for signals
    }

    // Cleanup
    munmap(dungeon_ptr, sizeof(*dungeon_ptr)); // Unmap shared memory
    sem_close(lever1_sem); // Close semaphore 1
    sem_close(lever2_sem); // Close semaphore 2
    return 0; // Exit the program
}
