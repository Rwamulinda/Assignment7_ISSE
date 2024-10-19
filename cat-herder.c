#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define NUM_PROCESSES 3 // We need 3 child processes
#define KITTY_EXEC "./kitty"

// Utility function for error handling
void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Function to close a file descriptor and handle errors
void safe_close(int fd) {
    if (close(fd) == -1) {
        error_exit("close");
    }
}

// Wrapper to fork and handle errors
pid_t safe_fork() {
    pid_t pid = fork();
    if (pid < 0) {
        error_exit("fork");
    }
    return pid;
}

int main(int argc, char *argv[]) {
    int pipe1[2], pipe2[2];  // Two pipes to link the three processes
    pid_t pids[NUM_PROCESSES];  // To store child PIDs
    int status;

    // Create the first pipe
    if (pipe(pipe1) == -1) {
        error_exit("pipe1");
    }

    // Create the second pipe
    if (pipe(pipe2) == -1) {
        error_exit("pipe2");
    }

    // Start the first child process: kitty -0
    pids[0] = safe_fork();
    if (pids[0] == 0) {
        // Child 1: Redirect stdout to pipe1's write end
        safe_close(pipe1[0]);  // Close unused read end
        dup2(pipe1[1], STDOUT_FILENO);  // Redirect stdout to pipe1
        safe_close(pipe1[1]);

        // Execute kitty -0
        execlp(KITTY_EXEC, KITTY_EXEC, "-0", NULL);
        error_exit("execlp kitty -0");  // If exec fails
    }

    // Start the second child process: kitty -1
    pids[1] = safe_fork();
    if (pids[1] == 0) {
        // Child 2: Redirect stdin from pipe1's read end, stdout to pipe2's write end
        safe_close(pipe1[1]);  // Close unused write end
        dup2(pipe1[0], STDIN_FILENO);  // Redirect stdin to pipe1
        safe_close(pipe1[0]);

        safe_close(pipe2[0]);  // Close unused read end of pipe2
        dup2(pipe2[1], STDOUT_FILENO);  // Redirect stdout to pipe2
        safe_close(pipe2[1]);

        // Execute kitty -1
        execlp(KITTY_EXEC, KITTY_EXEC, "-1", NULL);
        error_exit("execlp kitty -1");  // If exec fails
    }

    // Start the third child process: kitty -2
    pids[2] = safe_fork();
    if (pids[2] == 0) {
        // Child 3: Redirect stdin from pipe2's read end
        safe_close(pipe2[1]);  // Close unused write end
        dup2(pipe2[0], STDIN_FILENO);  // Redirect stdin to pipe2
        safe_close(pipe2[0]);

        // Execute kitty -2
        execlp(KITTY_EXEC, KITTY_EXEC, "-2", NULL);
        error_exit("execlp kitty -2");  // If exec fails
    }

    // Parent process: Close all unused pipe ends
    safe_close(pipe1[0]);
    safe_close(pipe1[1]);
    safe_close(pipe2[0]);
    safe_close(pipe2[1]);

    // Wait for all child processes to finish
    int success = 1;
    for (int i = 0; i < NUM_PROCESSES; i++) {
        if (waitpid(pids[i], &status, 0) == -1) {
            error_exit("waitpid");
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            success = 0;  // If any child exits with non-zero, set failure
        }
    }

    // Exit with 0 if all processes succeed, otherwise 1
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
