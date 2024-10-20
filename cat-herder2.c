#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"
#define EXPECTED_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/var/local/scottycheck/isse-07"
#define HOME_DIR "/home/puwase"  // Set HOME to the expected directory

// Function to close all pipe file descriptors
void close_pipe_ends(int pipefd[2][2], int child_index) {
    for (int i = 0; i < 2; i++) {
        if (i != child_index - 1) close(pipefd[i][0]);  // Close unused read ends
        if (i != child_index) close(pipefd[i][1]);      // Close unused write ends
    }
}

// Function to set the required environment variables
void setup_environment(int child_index) {
    clearenv();  // Clear all existing environment variables
    setenv("HOME", HOME_DIR, 1);  // Set HOME to /home/puwase
    setenv("PATH", EXPECTED_PATH, 1);  // Set PATH to the expected path

    // Set CATFOOD only for the first and third child
    if (child_index == 0 || child_index == 2) {
        setenv("CATFOOD", "yummy", 1);
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
        exit(EXIT_FAILURE);
    }

    // Fork three child processes
    for (int i = 0; i < 3; i++) {
        pid[i] = fork();

        if (pid[i] < 0) {  // Fork error
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid[i] == 0) {  // Child process
            setup_environment(i);  // Set up environment variables

            // Redirect input
            if (i == 0) {  // First child reads from the input file
                int in_fd = open(input_file, O_RDONLY);
                if (in_fd == -1) {
                    perror("open input file");
                    exit(EXIT_FAILURE);
                }
                if (dup2(in_fd, STDIN_FILENO) == -1) {
                    perror("dup2 input");
                    exit(EXIT_FAILURE);
                }
                close(in_fd);
            } else {  // Other children read from the previous pipe
                if (dup2(pipefd[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2 pipe read");
                    exit(EXIT_FAILURE);
                }
            }

            // Redirect output
            if (i < 2) {  // First two children write to the next pipe
                if (dup2(pipefd[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2 pipe write");
                    exit(EXIT_FAILURE);
                }
            } else {  // Last child writes to the output file
                if (dup2(out_fd, STDOUT_FILENO) == -1) {
                    perror("dup2 output");
                    exit(EXIT_FAILURE);
                }
            }

            // Close all unused pipes in the child process
            close_pipe_ends(pipefd, i);
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
    for (int i = 0; i < 2; i++) close(pipefd[i][1]);
    close(out_fd);

    // Wait for all child processes to complete
    for (int i = 0; i < 3; i++) {
        int status;
        if (waitpid(pid[i], &status, 0) == -1) {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child %d exited with status %d\n", i, WEXITSTATUS(status));
            exit(EXIT_FAILURE);  // Exit if any child fails
        }
    }

    printf("All child processes completed successfully\n");
    return 0;
}