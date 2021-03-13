#include <cstdlib>
#include <csignal>

// TODO: if object is never used? - Set last accessed to current time

int main() {
    raise(SIGUSR1);
    char *buf = (char *) malloc(39);
    buf[7] = 'a';
    free(malloc(21));
    buf[8] = 'c';
    buf[17] = 'd';
    free(malloc(42));
    buf[28] = 'f';
    free(malloc(87));
    buf[38] = 'h';
    free(malloc(8));
    buf[4] = 'b';
    free(buf);
    return 0;
}
