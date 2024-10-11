#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 256

void print_usage() {
    fprintf(stderr, "Usage: cat-herder (inputfile) (outputfile)\n");
}

int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 3) {
        print_usage();
        return EXIT_FAILURE;
    }

    const char *inputfile = argv[1];
    const char *outputfile = argv[2];

    // Ensure input and output files are not the same
    if (strcmp(inputfile, outputfile) == 0) {
        fprintf(stderr, "Error: inputfile and outputfile cannot be the same.\n");
        return EXIT_FAILURE;
    }

    // Create pipes for inter-process communication
    int pipe1[2], pipe2[2];
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("Pipe creation failed");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 3; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) { // Child process
            if (i == 0) { // kitty -2
                int input_fd = open(inputfile, O_RDONLY);
                if (input_fd == -1) {
                    perror("Opening input file failed");
                    exit(EXIT_FAILURE);
                }
                dup2(input_fd, STDIN_FILENO); // Redirect stdin from inputfile
                dup2(pipe1[1], STDOUT_FILENO); // Connect stdout to pipe1
                close(pipe1[0]); // Close unused read end of pipe1
                close(input_fd); // Close the input file descriptor
                setenv("CATFOOD", "yummy", 1); // Set environment variable
                execlp("kitty", "kitty", "-2", NULL);
                perror("Exec failed");
                exit(EXIT_FAILURE);
            }

            if (i == 1) { // kitty -3
                dup2(pipe1[0], STDIN_FILENO); // Connect stdin to pipe1
                dup2(pipe2[1], STDOUT_FILENO); // Connect stdout to pipe2
                close(pipe1[1]); // Close unused write end of pipe1
                close(pipe2[0]); // Close unused read end of pipe2
                unsetenv("KITTYLITTER"); // Remove environment variable
                execlp("kitty", "kitty", "-3", NULL);
                perror("Exec failed");
                exit(EXIT_FAILURE);
            }

            if (i == 2) { // kitty -4
                dup2(pipe2[0], STDIN_FILENO); // Connect stdin to pipe2
                freopen(outputfile, "w", stdout); // Redirect stdout to outputfile
                close(pipe2[1]); // Close unused write end of pipe2
                setenv("CATFOOD", "yummy", 1); // Set environment variable
                setenv("PATH", getenv("PATH"), 1); // Set PATH variable
                setenv("HOME", getenv("HOME"), 1); // Set HOME variable
                execlp("kitty", "kitty", "-4", NULL);
                perror("Exec failed");
                exit(EXIT_FAILURE);
            }
        }
    }

    // Parent process: close pipe ends
    close(pipe1[1]);
    close(pipe2[1]);

    // Wait for all child processes to exit
    int status;
    for (int i = 0; i < 3; i++) {
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            return EXIT_FAILURE; // One of the children exited with non-zero status
        }
    }

    return EXIT_SUCCESS; // All children exited successfully
}