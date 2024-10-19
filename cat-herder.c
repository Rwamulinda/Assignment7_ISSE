#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"  // Path to the kitty executable

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];
    pid_t pid[3];  // Store PIDs of the three child processes
    int pipefd[2][2];  // Two pipes for three processes

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

    // Create three child processes
    for (int i = 0; i < 3; i++) {
        pid[i] = fork();

        if (pid[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid[i] == 0) {  // Child process
            // Set environment variables
            setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin", 1);
            setenv("CATFOOD", "yummy", 1);  // You can change "yummy" to any appropriate value

            // Handle input from the previous pipe (if applicable)
            if (i > 0) {
                if (dup2(pipefd[i - 1][0], STDIN_FILENO) == -1) {  // Read from previous pipe
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            } else {
                // Read from the input file for the first child
                int in_fd = open(input_file, O_RDONLY);
                if (in_fd == -1) {
                    perror("open input file");
                    exit(EXIT_FAILURE);
                }
                if (dup2(in_fd, STDIN_FILENO) == -1) {  // Read from input file
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(in_fd);  // Close the input file descriptor
            }

            // Handle output to the next pipe (if applicable)
            if (i < 2) {
                if (dup2(pipefd[i][1], STDOUT_FILENO) == -1) {  // Write to next pipe
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            } else {
                // Write to the output file for the last child
                if (dup2(out_fd, STDOUT_FILENO) == -1) {  // Write to output file
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            // Close all pipe file descriptors
            for (int j = 0; j < 2; j++) {
                close(pipefd[j][0]);
                close(pipefd[j][1]);
            }

            // Use execl to run /var/local/isse-07/kitty with an argument indicating the child number
            char arg[3];
            snprintf(arg, sizeof(arg), "-%d", i);  // Format "-0", "-1", "-2"

            execl(KITTY_EXEC, "kitty", arg, NULL);
            // If execl fails, print an error and exit
            perror("execl");
            exit(EXIT_FAILURE);
        }
    }

    // Close all pipe write ends in the parent process
    for (int i = 0; i < 2; i++) {
        close(pipefd[i][1]);
    }

    // Close the output file descriptor in the parent process
    close(out_fd);

    // Parent process: Wait for all child processes to complete
    for (int i = 0; i < 3; i++) {
        int status;
        waitpid(pid[i], &status, 0);  // Wait for each child to complete

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child %d exited with status %d\n", i, WEXITSTATUS(status));
            exit(EXIT_FAILURE);  // Exit if any child failed
        }
    }

    printf("All child processes completed successfully\n");
    return 0;
}
