#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"

void close_all_pipes(int pipefd[2][2]) {
    for (int i = 0; i < 2; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];
    pid_t pid[3];
    int pipefd[2][2];

    // Create pipes
    for (int i = 0; i < 2; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Open the output file
    int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        perror("open output file");
        close_all_pipes(pipefd);
        exit(EXIT_FAILURE);
    }

    // Fork three child processes
    for (int i = 0; i < 3; i++) {
        pid[i] = fork();

        if (pid[i] < 0) { // Fork error
            perror("fork");
            close_all_pipes(pipefd);
            close(out_fd);
            exit(EXIT_FAILURE);
        }

        if (pid[i] == 0) { // Child process
            // Set environment variables according to the child
            if (i == 0) { // kitty -2
                setenv("CATFOOD", "yummy", 1);
                unsetenv("KITTYLITTER"); // Remove KITTYLITTER if set
                char *new_env[] = {
                    "PATH=/usr/bin:/home/puwase", // Ensure it contains the home directory
                    "HOME=/home/puwase",
                    "CATFOOD=yummy",
                    NULL
                };
                dup2(pipefd[0][1], STDOUT_FILENO); // Redirect output to pipe
                execle(KITTY_EXEC, "kitty", "-2", NULL, new_env);
            } else if (i == 1) { // kitty -3
                unsetenv("KITTYLITTER");
                char *new_env[] = {
                    "PATH=/usr/bin:/home/puwase", // Ensure it contains the home directory
                    "HOME=/home/puwase",
                    NULL
                };
                dup2(pipefd[1][0], STDIN_FILENO); // Read from the previous pipe
                execle(KITTY_EXEC, "kitty", "-3", NULL, new_env);
            } else if (i == 2) { // kitty -4
                setenv("CATFOOD", "yummy", 1);
                char *new_env[] = {
                    "PATH=/usr/bin:/home/puwase", // Ensure it contains the home directory
                    "HOME=/home/puwase",
                    "CATFOOD=yummy",
                    NULL
                };
                dup2(out_fd, STDOUT_FILENO); // Write to the output file
                execle(KITTY_EXEC, "kitty", "-4", NULL, new_env);
            }

            // Close all pipes in the child process
            close_all_pipes(pipefd);
            close(out_fd); // Ensure output file is closed

            // If exec fails, exit
            perror("execle");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process: Close all pipe write ends
    close_all_pipes(pipefd);
    close(out_fd); // Close output file in parent

    // Wait for all child processes to complete
    for (int i = 0; i < 3; i++) {
        int status;
        waitpid(pid[i], &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child %d exited with status %d\n", i, WEXITSTATUS(status));
            exit(EXIT_FAILURE); // Exit if any child fails
        }
    }

    printf("All child processes completed successfully\n");
    return 0;
}
