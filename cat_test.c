#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"

extern char **environ; // Declare environ to access environment variables

void close_all_pipes(int pipefd[2][2]) {
    for (int i = 0; i < 2; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
}

void filter_environment(int child_index) {
    // Allocate space for up to 4 environment variables (3 + 1 NULL terminator)
    char **new_environ = calloc(4, sizeof(char *));
    if (new_environ == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    int count = 0;

    // Check the current environment and retain only needed variables
    for (char **env = environ; *env != NULL; env++) {
        if (strstr(*env, "HOME=") == *env ||
            (child_index == 0 && strstr(*env, "CATFOOD=") == *env) ||
            (child_index == 1 && strstr(*env, "KITTYLITTER=") == *env)) {
            new_environ[count++] = *env;
        }
    }

    new_environ[count] = NULL; // Terminate the array
    environ = new_environ;      // Redirect environ to the new environment
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
            // Filter environment variables for the child
            filter_environment(i);

            // Set specific environment variables according to the child
            if (i == 0) { // kitty -2
                setenv("CATFOOD", "yummy", 1);
                unsetenv("KITTYLITTER");
            } else if (i == 1) { // kitty -3
                unsetenv("KITTYLITTER");
            } else if (i == 2) { // kitty -4
                setenv("CATFOOD", "yummy", 1);
            }

            // Redirect input
            if (i == 0) { // First child reads from the input file
                int in_fd = open(input_file, O_RDONLY);
                if (in_fd == -1) {
                    perror("open input file");
                    close_all_pipes(pipefd);
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            } else { // Other children read from the previous pipe
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }

            // Redirect output
            if (i < 2) { // First two children write to the next pipe
                dup2(pipefd[i][1], STDOUT_FILENO);
            } else { // Last child writes to the output file
                dup2(out_fd, STDOUT_FILENO);
            }

            // Close all pipes in the child process
            close_all_pipes(pipefd);
            close(out_fd);

            // Execute the kitty command
            char arg[3];
            snprintf(arg, sizeof(arg), "-%d", i + 2);
            execl(KITTY_EXEC, "kitty", arg, NULL);

            // If exec fails
            perror("execl");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process: Close all pipe write ends
    close_all_pipes(pipefd);
    close(out_fd);

    // Wait for all child processes to complete
    for (int i = 0; i < 3; i++) {
        int status;
        waitpid(pid[i], &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child %d exited with status %d\n", i, WEXITSTATUS(status));
            exit(EXIT_FAILURE);
        }
    }

    printf("All child processes completed successfully\n");
    return 0;
}
