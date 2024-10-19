#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// Define the full path to the kitty executable
#define KITTY_EXEC "/var/local/isse-07/kitty"

int main() {
    pid_t pid[3];  // Store PIDs of the three child processes

    // Create three child processes
    for (int i = 0; i < 3; i++) {
        pid[i] = fork();

        if (pid[i] < 0) {
            perror("fork");
            exit(1);
        }

        if (pid[i] == 0) {  // Child process
            // Use execl to run /var/local/isse-07/kitty with an argument indicating the child number
            char arg[3];
            snprintf(arg, sizeof(arg), "-%d", i);  // Format "-0", "-1", "-2"

            execl(KITTY_EXEC, "kitty", arg, NULL);

            // If execl fails, print an error and exit
            perror("execl");
            exit(1);
        }
    }

    // Parent process: Wait for all child processes to complete
    for (int i = 0; i < 3; i++) {
        int status;
        waitpid(pid[i], &status, 0);  // Wait for each child to complete

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child %d exited with status %d\n", i, WEXITSTATUS(status));
            exit(1);  // Exit if any child failed
        }
    }

    printf("All child processes completed successfully\n");
    return 0;
}
