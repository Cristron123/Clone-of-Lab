// wizard.c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include "dungeon_info.h"
#include "dungeon_settings.h"

static struct Dungeon *d;
static sem_t *lever2;

// Decode Caesar cipher in d->barrier.spell → d->wizard.spell
void decode_caesar(void) {
    char *in  = d->barrier.spell;
    char *out = d->wizard.spell;
    int key = (unsigned char)in[0];      // first char as shift
    key %= 26;                           // wrap into 0–25

    for (int i = 1; i < SPELL_BUFFER_SIZE && in[i] != '\0'; ++i) {
        char c = in[i];
        if ('A' <= c && c <= 'Z') {
            out[i-1] = 'A' + ( (c - 'A' - key + 26) % 26 );
        }
        else if ('a' <= c && c <= 'z') {
            out[i-1] = 'a' + ( (c - 'a' - key + 26) % 26 );
        }
        else {
            out[i-1] = c;  // leave punctuation/spaces untouched
        }
    }
    // null terminate
    out[strlen(in)-1] = '\0';
}

void handler(int sig) {
    if (sig == DUNGEON_SIGNAL) {
        decode_caesar();
    }
    else if (sig == SEMAPHORE_SIGNAL) {
        // Hold Lever Two
        sem_wait(lever2);
    }
}

int main(void) {
    // 1) Map shared memory
    int fd = shm_open(dungeon_shm_name, O_RDWR, 0);
    d = mmap(NULL, sizeof(*d),
             PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
    if (d == MAP_FAILED) { perror("mmap"); exit(1); }

    // 2) Open Lever Two semaphore
    lever2 = sem_open(dungeon_lever_two, 0);
    if (lever2 == SEM_FAILED) { perror("sem_open lever2"); exit(1); }

    // 3) Install handler for both signals
    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(DUNGEON_SIGNAL,   &sa, NULL);
    sigaction(SEMAPHORE_SIGNAL, &sa, NULL);

    // 4) Loop until dungeon->running == false
    while (d->running) {
        pause();
    }
    return 0;
}

