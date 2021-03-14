#include <cstdlib>
#include <csignal>

int main() {
    raise(SIGUSR1);
    for (int i = 0; i < 10; i++) {
        malloc(42);
    }
    raise(SIGUSR2);
    return 0;
}
