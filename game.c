// game.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>       // O_CREAT, O_RDWR
#include <sys/mman.h>    // shm_open, mmap
#include <sys/stat.h>    // S_IRUSR, S_IWUSR
#include <unistd.h>      // ftruncate, fork, execl
#include <semaphore.h>   // sem_open, sem_unlink
#include "dungeon_info.h"
#include "dungeon_settings.h"

int main(void) {
    // 1) Create & size shared memory
    int shm_fd = shm_open(dungeon_shm_name, O_CREAT | O_RDWR, 0600);
    if (shm_fd < 0) { perror("shm_open"); exit(1); }
    if (ftruncate(shm_fd, sizeof(struct Dungeon)) < 0) {
        perror("ftruncate"); exit(1);
    }

    // 2) Map it into memory
    struct Dungeon *d = mmap(NULL, sizeof(*d),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, shm_fd, 0);
    if (d == MAP_FAILED) { perror("mmap"); exit(1); }

    // 3) Initialize the dungeon struct
    d->running    = true;
    d->dungeonPID = getpid();

    // 4) Create the two lever semaphores (initial count = 0)
    sem_t *lever1 = sem_open(dungeon_lever_one, O_CREAT, 0600, 0);
    sem_t *lever2 = sem_open(dungeon_lever_two, O_CREAT, 0600, 0);
    if (lever1 == SEM_FAILED || lever2 == SEM_FAILED) {
        perror("sem_open"); exit(1);
    }

    // 5) Fork & exec the Barbarian
    pid_t barb_pid = fork();
    if (barb_pid == 0) {
        execl("./barbarian", "barbarian", NULL);
        perror("execl barbarian"); exit(1);
    }

    // 6) Fork & exec the Wizard
    pid_t wiz_pid = fork();
    if (wiz_pid == 0) {
        execl("./wizard", "wizard", NULL);
        perror("execl wizard"); exit(1);
    }

    // 7) Fork & exec the Rogue
    pid_t rogue_pid = fork();
    if (rogue_pid == 0) {
        execl("./rogue", "rogue", NULL);
        perror("execl rogue"); exit(1);
    }


    // 8) Hand off control to the dungeon engine
    //    Note: order is (wizard, rogue, barbarian)
    RunDungeon(wiz_pid, rogue_pid, barb_pid);

    // 9) Cleanup: stop running, unlink semaphores & shared memory
    d->running = false;
    sem_unlink(dungeon_lever_one);
    sem_unlink(dungeon_lever_two);
    shm_unlink(dungeon_shm_name);

    return 0;
}

