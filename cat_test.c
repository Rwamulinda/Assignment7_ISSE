#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>  // Required for waitpid()

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <inputfile> <outputfile>\n", argv[0]);
        exit(1);
    }

    char *inputfile = argv[1];
    char *outputfile = argv[2];

    // Path to the kitty executable
    char *kitty_path = "/var/local/isse-07/kitty";

    // First child process
    pid_t pid1 = fork();
    if (pid1 == 0) {
        freopen(inputfile, "r", stdin);  // Redirect input
        execlp(kitty_path, "kitty", NULL);
        perror("execlp failed");
        exit(1);
    }

    // Second child process
    pid_t pid2 = fork();
    if (pid2 == 0) {
        execlp(kitty_path, "kitty", NULL);
        perror("execlp failed");
        exit(1);
    }

    // Third child process
    pid_t pid3 = fork();
    if (pid3 == 0) {
        freopen(outputfile, "w", stdout);  // Redirect output
        execlp(kitty_path, "kitty", NULL);
        perror("execlp failed");
        exit(1);
    }

    // Parent process waits for all child processes to finish
    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);
    waitpid(pid3, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("All processes succeeded.\n");
        return 0;
    } else {
        fprintf(stderr, "One or more processes failed.\n");
        return 1;
    }
}
