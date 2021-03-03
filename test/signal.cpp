#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <signal.h>

void allocate_stuff() {
    for (int i = 0; i < 10; i++) {
        free(malloc(20));
    }
}

int main() {
    // raise(SIGUSR1);
    // printf("Raised SIGUSR1!\n");
    allocate_stuff();
    printf("Stop me!\n");
    sleep(5);
    // raise(SIGUSR2);
    // printf("Raised SIGUSR2!\n");
    allocate_stuff();
    printf("Done!\n");
    return 0;
}
