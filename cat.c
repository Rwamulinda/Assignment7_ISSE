#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CAT_HERDER "kitty"

void run_kitty(int runlevel) {
    // Prepare arguments for execvp
    char *args[3];
    args[0] = CAT_HERDER;
    args[1] = "-n"; // Set the runlevel argument
    args[2] = NULL;

    // Fork a child process to run kitty
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) { // Child process
        // Execute the kitty program
        execvp(args[0], args);
        perror("execvp"); // If execvp fails
        exit(1);
    } else { // Parent process
        // Wait for the child to finish
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            printf("kitty exited with status: %d\n", exit_status);
        } else {
            printf("kitty did not exit normally.\n");
        }
    }
}

int main(int argc, char *argv[], char *envp[]) {
    // Set environment variables before running kitty
    setenv("CATFOOD", "yummy", 1);
    unsetenv("KITTYLITTER"); // Ensure KITTYLITTER is not set

    // You can also verify if other required environment variables are set here
    // Example: if (getenv("PATH") == NULL) { /* handle the error */ }

    // Check for the runlevel from command-line arguments (default to 2 for this example)
    int runlevel = 2; // You can modify this based on your needs
    if (argc > 1) {
        runlevel = atoi(argv[1]);
    }

    // Run the kitty program
    run_kitty(runlevel);

    return 0;
}
