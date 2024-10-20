#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// Function to create child processes and run "kitty" with appropriate arguments
void create_child(int index, int input_fd, int output_fd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    } else if (pid == 0) { // Child process
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            perror("dup2 input_fd failed");
            exit(1);
        }
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("dup2 output_fd failed");
            exit(1);
        }

        // Close unused file descriptors
        close(input_fd);
        close(output_fd);

        // Create argument string safely
        char arg[16];  // Increased buffer size to prevent truncation
        snprintf(arg, sizeof(arg), "-%d", index + 2);  // Generate argument (e.g., "-2", "-3", "-4")

        // Execute "kitty" with the generated argument
        execlp("kitty", "kitty", arg, (char *)NULL);

        // If execlp fails
        perror("execlp failed");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <inputfile> <outputfile>\n", argv[0]);
        exit(1);
    }

    // Open input and output files
    int input_fd = open(argv[1], O_RDONLY);
    if (input_fd < 0) {
        perror("Error opening input file");
        exit(1);
    }

    int output_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        perror("Error opening output file");
        close(input_fd);
        exit(1);
    }

    // Create three child processes for the pipeline
    create_child(0, input_fd, STDOUT_FILENO);  // First child, input from input file
    create_child(1, STDIN_FILENO, STDOUT_FILENO);  // Second child, intermediate
    create_child(2, STDIN_FILENO, output_fd);  // Third child, output to output file

    // Close input and output file descriptors in the parent process
    close(input_fd);
    close(output_fd);

    // Wait for all child processes to complete
    int status;
    for (int i = 0; i < 3; ++i) {
        if (wait(&status) == -1) {
            perror("wait failed");
            exit(1);
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child process %d failed\n", i);
            exit(1);
        }
    }

    return 0;
}
