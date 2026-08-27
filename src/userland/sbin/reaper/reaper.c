#include <unistd.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
    while (1) {
        int pid = waitpid(-1, NULL, 0);
    }
    return 0;
}