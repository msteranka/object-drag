#include <cstdlib>

void do_something() {
    for (int i = 0; i < 1000000; i++);
}

int main() {
    char *ptr = (char *) malloc(20);
    ptr[0] = 'a';
    // do_something();
    free(ptr);
    return 0;
}
