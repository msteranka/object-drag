#include <cstdlib>

void do_something() {
    char buf[256];
    for (int i = 0; i < 1000000; i++) {
        for (int j = 0; j < 256; j++) {
            buf[j] = (char) rand();
        }
    }
}

int main() {
    char *ptr = (char *) malloc(20);
    ptr[0] = 'a';
    // do_something();
    free(ptr);
    return 0;
}
