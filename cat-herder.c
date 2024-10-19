#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"  // Define the full path to the kitty executable

int main() {
    pid_t pid[3];  // Store PIDs of the three child processes
    int pipefd[2][2];  // Two pipes for three processes

    // Create pipes
    for (int i = 0; i < 2; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Create three child processes
    for (int i = 0; i < 3; i++) {
        pid[i] = fork();

        if (pid[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (pid[i] == 0) {  // Child process
            // Set the PATH environment variable to include the expected directories
            setenv("PATH", "/home/puwase:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/var/local/scottycheck/isse-07", 1);

            // Set the CATFOOD environment variable
            setenv("CATFOOD", "yummy", 1);  // You can change "yummy" to any appropriate value

            // Handle input from the previous pipe (if applicable)
            if (i > 0) {
                dup2(pipefd[i - 1][0], STDIN_FILENO);  // Read from previous pipe
            }

            // Handle output to the next pipe (if applicable)
            if (i < 2) {
                dup2(pipefd[i][1], STDOUT_FILENO);  // Write to next pipe
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
