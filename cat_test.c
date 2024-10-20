#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

// Path to kitty executable
#define KITTY_PATH "/var/local/isse-07/kitty"

// Function to close unnecessary file descriptors
void close_extra_fds() {
    for (int fd = 3; fd < 1024; fd++) {
        close(fd);
    }
}

// Set up a minimal environment with required variables
char *minimal_env[] = {
    "PATH=/home/puwase",  // Ensure PATH contains the required string
    "HOME=/home/puwase",  // Required HOME variable
    "CATFOOD=yummy",       // CATFOOD variable needed for kitty -2
    NULL                   // Null terminator for environment array
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s inputfile outputfile\n", argv[0]);
        return 1;
    }

    // Ensure input and output files are not the same
    if (strcmp(argv[1], argv[2]) == 0) {
        fprintf(stderr, "Input and output files must be different.\n");
        return 1;
    }

    // Open input and output files
    int input_fd = open(argv[1], O_RDONLY);
    if (input_fd < 0) {
        perror("Error opening input file");
        return 1;
    }

    int output_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        perror("Error opening output file");
        close(input_fd);
        return 1;
    }

    // Create pipes
    int pipe1[2], pipe2[2];
    pipe(pipe1);
    pipe(pipe2);

    pid_t pid1 = fork();
    if (pid1 == 0) {  // Child 1
        close(pipe1[0]);  // Close read end of pipe1
        dup2(input_fd, STDIN_FILENO);  // Redirect input
        dup2(pipe1[1], STDOUT_FILENO);  // Redirect output to pipe1
        close(input_fd);
        close(pipe1[1]);

        close_extra_fds();  // Close extra file descriptors
        execle(KITTY_PATH, "kitty", "-0", NULL, minimal_env);  // Use mode 0
        perror("exec failed");
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {  // Child 2
        close(pipe1[1]);  // Close write end of pipe1
        close(pipe2[0]);  // Close read end of pipe2
        dup2(pipe1[0], STDIN_FILENO);  // Read from pipe1
        dup2(pipe2[1], STDOUT_FILENO);  // Write to pipe2
        close(pipe1[0]);
        close(pipe2[1]);

        setenv("CATFOOD", "yummy", 1);  // Set CATFOOD environment variable
        close_extra_fds();  // Close extra file descriptors
        execle(KITTY_PATH, "kitty", "-2", NULL, minimal_env);  // Use mode 2
        perror("exec failed");
        exit(1);
    }

    pid_t pid3 = fork();
    if (pid3 == 0) {  // Child 3
        close(pipe2[1]);  // Close write end of pipe2
        dup2(pipe2[0], STDIN_FILENO);  // Read from pipe2
        dup2(output_fd, STDOUT_FILENO);  // Write to output file
        close(pipe2[0]);
        close(output_fd);

        close_extra_fds();  // Close extra file descriptors
        execle(KITTY_PATH, "kitty", "-4", NULL, minimal_env);  // Use mode 4
        perror("exec failed");
        exit(1);
    }

    // Close all file descriptors in the parent
    close(input_fd);
    close(output_fd);
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);

    // Wait for all child processes
    int status1, status2, status3;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);
    waitpid(pid3, &status3, 0);

    // Check if all child processes exited successfully
    if (WIFEXITED(status1) && WEXITSTATUS(status1) == 0 &&
        WIFEXITED(status2) && WEXITSTATUS(status2) == 0 &&
        WIFEXITED(status3) && WEXITSTATUS(status3) == 0) {
        return 0;
    } else {
        return 1;
    }
}
