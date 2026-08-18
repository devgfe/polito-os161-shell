#include <stdio.h>
#include <unistd.h>

int main(void)
{
    pid_t pid1, pid2, pid3;

    pid1 = getpid();
    pid2 = getpid();
    pid3 = getpid();

    printf("PID 1: %d\n", pid1);
    printf("PID 2: %d\n", pid2);
    
    printf("PID 3: %d\n", pid3);

    if (pid1 == pid2 && pid2 == pid3) {
        printf("getpid() test: PASS\n");
    } else {
        printf("getpid() test: FAIL\n");
    }

    return 0;
}