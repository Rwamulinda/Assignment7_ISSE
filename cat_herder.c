#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_CATS 10

void handle_sigint(int sig) {
    printf("Received SIGINT, cleaning up cats...\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <number_of_cats>\n", argv[0]);
        exit(1);
    }

    int num_cats = atoi(argv[1]);
    if (num_cats <= 0 || num_cats > MAX_CATS) {
        fprintf(stderr, "Please provide a number between 1 and %d\n", MAX_CATS);
        exit(1);
    }

    pid_t cats[num_cats];
    signal(SIGINT, handle_sigint); // Register signal handler

    for (int i = 0; i < num_cats; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            exit(1);
        } else if (pid == 0) {
            printf("Cat %d is running with PID: %d\n", i, getpid());
            sleep((rand() % 5) + 1); // Simulate work with sleep
            printf("Cat %d with PID %d is done\n", i, getpid());
            exit(0); // Child process exits
        } else {
            cats[i] = pid; // Parent stores the PID of the child
        }
    }

    for (int i = 0; i < num_cats; i++) {
        int status;
        pid_t pid = waitpid(cats[i], &status, 0);
        if (pid == -1) {
            perror("waitpid failed");
        } else if (WIFEXITED(status)) {
            printf("Cat with PID %d exited with status %d\n", pid, WEXITSTATUS(status));
        }
    }

    printf("All cats are herded!\n");
    return 0;
}
