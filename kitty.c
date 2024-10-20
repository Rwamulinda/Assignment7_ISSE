/*
 * cat-herder.c
 *
 * ISSE Assignment 7: This program runs three instances of `kitty` 
 * as child processes, connected through pipes to mimic a shell pipeline. 
 * The input for the pipeline is read from `inputfile`, 
 * and the final output is written to `outputfile`.
 *
 * Author: <Your Name>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>

#define INPUT_FILE "inputfile"
#define OUTPUT_FILE "outputfile"
#define KITTY_EXEC "./kitty"

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void redirect(int old_fd, int new_fd) {
    if (dup2(old_fd, new_fd) == -1)
        error_exit("dup2 failed");
    close(old_fd);
}

void run_kitty(int runlevel, int input_fd, int output_fd) {
    char runlevel_arg[3];  // Enough to hold "-n"
    snprintf(runlevel_arg, sizeof(runlevel_arg), "-%d", runlevel);

    // Redirect input and output
    if (input_fd != STDIN_FILENO)
        redirect(input_fd, STDIN_FILENO);
    if (output_fd != STDOUT_FILENO)
        redirect(output_fd, STDOUT_FILENO);

    // Execute the `kitty` program
    execl(KITTY_EXEC, KITTY_EXEC, runlevel_arg, NULL);
    error_exit("execl failed");  // If exec fails, terminate
}

int main() {
    int pipe1[2], pipe2[2];

    // Create pipes to connect child processes
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1)
        error_exit("pipe failed");

    // Fork the first child (kitty -0)
    pid_t pid1 = fork();
    if (pid1 < 0) error_exit("fork failed");
    if (pid1 == 0) {
        // Child 1: Read from inputfile and write to pipe1
        int input_fd = open(INPUT_FILE, O_RDONLY);
        if (input_fd < 0) error_exit("Failed to open inputfile");
        close(pipe1[0]);  // Close unused read end of pipe1
        run_kitty(0, input_fd, pipe1[1]);
    }

    // Fork the second child (kitty -1)
    pid_t pid2 = fork();
    if (pid2 < 0) error_exit("fork failed");
    if (pid2 == 0) {
        // Child 2: Read from pipe1 and write to pipe2
        close(pipe1[1]);  // Close unused write end of pipe1
        close(pipe2[0]);  // Close unused read end of pipe2
        run_kitty(1, pipe1[0], pipe2[1]);
    }

    // Fork the third child (kitty -2)
    pid_t pid3 = fork();
    if (pid3 < 0) error_exit("fork failed");
    if (pid3 == 0) {
        // Child 3: Read from pipe2 and write to outputfile
        int output_fd = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) error_exit("Failed to open outputfile");
        close(pipe2[1]);  // Close unused write end of pipe2
        run_kitty(2, pipe2[0], output_fd);
    }

    // Parent process: Close all pipes and wait for children to finish
    close(pipe1[0]); close(pipe1[1]);
    close(pipe2[0]); close(pipe2[1]);

    int status1, status2, status3;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);
    waitpid(pid3, &status3, 0);

    // Check exit statuses of children
    if (WIFEXITED(status1) && WIFEXITED(status2) && WIFEXITED(status3) &&
        WEXITSTATUS(status1) == 0 && WEXITSTATUS(status2) == 0 && WEXITSTATUS(status3) == 0) {
        exit(EXIT_SUCCESS);
    } else {
        fprintf(stderr, "One or more child processes failed\n");
        exit(EXIT_FAILURE);
    }
}
