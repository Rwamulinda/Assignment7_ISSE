#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"
#define EXPECTED_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/var/local/scottycheck/isse-07"

// Function to close all pipe file descriptors
void close_all_pipes(int pipefd[2][2]) {
    for (int i = 0; i < 2; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
}

// Function to set only required environment variables
void setup_environment(int child_index) {
    // Clear unnecessary environment variables
    unsetenv("KITTYLITTER"); // Ensure KITTYLITTER is unset

    // Set specific environment variables based on child index
    if (child_index == 0) { // First child
        setenv("CATFOOD", "yummy", 1);
        setenv("HOME", "/home/puwase", 1); // Set HOME to the expected value
        setenv("PATH", EXPECTED_PATH, 1);
    } else if (child_index == 1) { // Second child
        setenv("HOME", "/home/puwase", 1); // Set HOME to the expected value
        setenv("PATH", EXPECTED_PATH, 1);
    } else if (child_index == 2) { // Third child
        setenv("CATFOOD", "yummy", 1);
        setenv("HOME", "/home/puwase", 1); // Set HOME to the expected value
        setenv("PATH", EXPECTED_PATH, 1);
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
            setup_environment(i); // Set up environment variables

            // Redirect input
            if (i == 0) { // First child reads from the input file
                int in_fd = open(input_file, O_RDONLY);
                if (in_fd == -1) {
                    perror("open input file");
                    close_all_pipes(pipefd);
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);  // Close after redirection
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
            close(out_fd); // Ensure output file is closed

            // Execute the kitty command
            char arg[3];
            snprintf(arg, sizeof(arg), "-%d", i + 2); // Correct command line argument
            execl(KITTY_EXEC, "kitty", arg, NULL);

            // If exec fails
            perror("execl");
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