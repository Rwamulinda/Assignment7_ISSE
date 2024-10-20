#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"
#define EXPECTED_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/var/local/scottycheck/isse-07"
#define HOME_DIR "/home/puwase"

// Function to close all pipe file descriptors
void close_all_pipes(int pipefd[2][2]) {
    for (int i = 0; i < 2; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
}

// Function to set the required environment variables for each child process
void setup_environment(int child_index) {
    clearenv();  // Clear all existing environment variables

    setenv("HOME", HOME_DIR, 1);  // Set HOME to the expected directory
    setenv("PATH", EXPECTED_PATH, 1);  // Set PATH to the expected path

    // Set environment variables based on the child index (runlevel)
    if (child_index == 2) {
        setenv("CATFOOD", "yummy", 1);  // Set CATFOOD to 'yummy'
    } else if (child_index == 3) {
        unsetenv("KITTYLITTER");  // Ensure KITTYLITTER is not set
    }
}

// Function to handle the creation of a single child process
void create_child(int pipefd[2][2], int index) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {  // Child process
        if (index == 0) {
            dup2(pipefd[0][1], STDOUT_FILENO);  // Redirect stdout to the first pipe's write end
        } else if (index == 1) {
            dup2(pipefd[0][0], STDIN_FILENO);   // Read from the first pipe's read end
            dup2(pipefd[1][1], STDOUT_FILENO);  // Write to the second pipe's write end
        } else if (index == 2) {
            dup2(pipefd[1][0], STDIN_FILENO);   // Read from the second pipe's read end
        }

        close_all_pipes(pipefd);  // Close all pipe file descriptors in child

        // Set up the environment and execute the kitty program with appropriate runlevel
        setup_environment(index + 2);
        char arg[3];
        snprintf(arg, sizeof(arg), "-%d", index + 2);  // Generate argument (e.g., "-2", "-3", "-4")
        execl(KITTY_EXEC, "kitty", arg, NULL);  // Execute kitty with the appropriate runlevel

        perror("execl");  // If execl fails
        exit(1);
    }
}

// Main function
int main() {
    int pipefd[2][2];  // Create two pipes

    // Create the pipes
    if (pipe(pipefd[0]) == -1 || pipe(pipefd[1]) == -1) {
        perror("pipe");
        exit(1);
    }

    // Create the three child processes
    for (int i = 0; i < 3; i++) {
        create_child(pipefd, i);
    }

    // Close all pipe file descriptors in the parent process
    close_all_pipes(pipefd);

    // Wait for all child processes to finish
    int status;
    for (int i = 0; i < 3; i++) {
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child process %d failed with exit status %d\n", i, WEXITSTATUS(status));
            exit(1);  // Exit with status 1 if any child fails
        }
    }

    printf("All child processes completed successfully.\n");
    return 0;
}
