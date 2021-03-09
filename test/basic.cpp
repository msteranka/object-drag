#include <cstdlib>
#include <cstring>

void do_something() {
    char *ptr;
    for (int i = 0; i < 5; i++) {
        ptr = (char *) malloc(10);
        memset(ptr, 'a', 10);
        free(ptr);
    }
}

int main() {
    char *ptr = (char *) malloc(100);
    ptr[0] = 'a';
    ptr[1] = 'b';
    do_something();
    free(ptr);
    return 0;
}
