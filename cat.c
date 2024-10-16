#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define KITTYPATH "/var/local/isse-07/kitty" // Update this path

void run_kitty(const char *arg, char *envp[]) {
    char *args[3];
    args[0] = KITTYPATH; // Full path to kitty
    args[1] = (char *)arg;
    args[2] = NULL;

    execvp(args[0], args); // Use execvp instead of execve
    perror("execvp"); // Only reached if execvp fails
    exit(1);
}

int main(int argc, char *argv[], char *envp[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <inputfile> <outputfile>\n", argv[0]);
        return 1;
    }

    const char *inputfile = argv[1];
    const char *outputfile = argv[2];

    if (strcmp(inputfile, outputfile) == 0) {
        fprintf(stderr, "Error: inputfile and outputfile must be different.\n");
        return 1;
    }

    int pipe1[2], pipe2[2];
    pipe(pipe1);
    pipe(pipe2);

    // Fork first child
    if (fork() == 0) {
        // Redirect stdin to inputfile
        int in_fd = open(inputfile, O_RDONLY);
        if (in_fd < 0) {
            perror("open inputfile");
            exit(1);
        }
        dup2(in_fd, STDIN_FILENO);
        close(in_fd);

        // Run kitty -2
        run_kitty("-2", envp);
    }

    // Fork second child
    if (fork() == 0) {
        // Redirect stdin to the first pipe and stdout to the second pipe
        dup2(pipe1[0], STDIN_FILENO);
        dup2(pipe2[1], STDOUT_FILENO);
        close(pipe1[1]);
        close(pipe2[0]);

        // Run kitty -3
        run_kitty("-3", envp);
    }

    // Fork third child
    if (fork() == 0) {
        // Redirect stdin to the second pipe and stdout to outputfile
        dup2(pipe2[0], STDIN_FILENO);
        int out_fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (out_fd < 0) {
            perror("open outputfile");
            exit(1);
        }
        dup2(out_fd, STDOUT_FILENO);
        close(pipe2[1]);
        close(out_fd);

        // Run kitty -4
        run_kitty("-4", envp);
    }

    // Close all pipe ends in the parent
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);

    // Wait for all children
    int status;
    for (int i = 0; i < 3; i++) {
        wait(&status);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            return 1; // If any child fails, return 1
        }
    }

    return 0; // Success
}
