#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>  // For open()

void execute_kitty(char *args[], int input_fd, int output_fd) {
    // Fork a child process
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (pid == 0) {  // Child process
        // Redirect input and output if needed
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);  // Close the original input file descriptor
        }
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);  // Close the original output file descriptor
        }

        // Execute the kitty process
        if (execvp(args[0], args) == -1) {
            perror("execvp failed");
            exit(1);
        }
    } else {  // Parent process
        // Close the unused file descriptors in the parent
        close(input_fd); 
        close(output_fd);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <inputfile> <outputfile>\n", argv[0]);
        return 1;
    }

    char *inputfile = argv[1];
    char *outputfile = argv[2];

    if (strcmp(inputfile, outputfile) == 0) {
        fprintf(stderr, "Error: inputfile and outputfile cannot be the same.\n");
        return 1;
    }

    // Open input and output files
    int input_fd = open(inputfile, O_RDONLY);
    if (input_fd < 0) {
        perror("Failed to open input file");
        return 1;
    }

    int output_fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        perror("Failed to open output file");
        close(input_fd);
        return 1;
    }

    // Define the arguments for each kitty process
    char *args1[] = {"/var/local/isse-07/kitty", "-2", NULL};
    char *args2[] = {"/var/local/isse-07/kitty", "-3", NULL};
    char *args3[] = {"/var/local/isse-07/kitty", "-4", NULL};

    // Create pipes
    int pipe1[2], pipe2[2];
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("pipe failed");
        close(input_fd);
        close(output_fd);
        return 1;
    }

    // Execute kitty -2, redirecting input from the input file and output to pipe1
    execute_kitty(args1, input_fd, pipe1[1]);
    close(pipe1[1]);  // Parent closes the write-end of pipe1

    // Execute kitty -3, redirecting input from pipe1 and output to pipe2
    execute_kitty(args2, pipe1[0], pipe2[1]);
    close(pipe1[0]);  // Parent closes the read-end of pipe1
    close(pipe2[1]);  // Parent closes the write-end of pipe2

    // Execute kitty -4, redirecting input from pipe2 and output to the output file
    execute_kitty(args3, pipe2[0], output_fd);
    close(pipe2[0]);  // Parent closes the read-end of pipe2

    // Wait for all children to exit
    int status;
    for (int i = 0; i < 3; ++i) {
        if (wait(&status) == -1) {
            perror("wait failed");
            return 1;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "A child process exited with non-zero status\n");
            return 1;
        }
    }

    return 0;
}
