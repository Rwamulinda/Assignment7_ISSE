#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"

extern char **environ; // Accessing the envirnoment variableable using environ 


// closing all the pipes
void close_all_pipes(int pipefd[2][2]) {
    for (int i = 0; i < 2; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
}

// Creating a modified environment for each child process
char **create_environment(int child_index) {
    
    int env_count = 0; // counting the existing environment
    while (environ[env_count]) env_count++;

    char **new_environ;
    if (child_index == 2) 
        new_environ = calloc(4, sizeof(char *));
        if (!new_environ) {
            perror("calloc");
            exit(EXIT_FAILURE);
        }
        
        // Add PATH and HOME
        const char *path = getenv("PATH");
        const char *home = getenv("HOME");

        new_environ[0] = malloc(strlen("PATH=") + strlen(path) + 1);
        sprintf(new_environ[0], "PATH=%s", path);

        new_environ[1] = malloc(strlen("HOME=") + strlen(home) + 1);
        sprintf(new_environ[1], "HOME=%s", home);

        // Add CATFOOD environment
        new_environ[2] = strdup("CATFOOD=yummy");
        new_environ[3] = NULL; // Null-terminate
    } 
    else {  // For "kitty 2" and "kitty 3"
        new_environ = calloc(env_count + 2, sizeof(char *));
        if (!new_environ) {
            perror("calloc");
            exit(EXIT_FAILURE);
        }

        int j = 0;
        for (int i = 0; i < env_count; i++) {
            if (child_index == 1 && strstr(environ[i], "KITTYLITTER=")) {
                continue;  // Skipping the  KITTYLITTER for "kitty -3"
            }
            new_environ[j++] = strdup(environ[i]);
        }

        if (child_index == 0) {  // Add CATFOOD for "kitty -2"
            new_environ[j++] = strdup("CATFOOD=yummy");
        }

        new_environ[j] = NULL;  // Null-terminate
    }

    return new_environ;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];
    pid_t pid[3];
    int pipefd[2][2];

    // Create pipes
    for (int i = 0; i < 2; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Open the output file
    int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        perror("open output file");
        close_all_pipes(pipefd);
        exit(EXIT_FAILURE);
    }

    // Fork three child processes
    for (int i = 0; i < 3; i++) {
        pid[i] = fork();

        if (pid[i] < 0) {  // Fork error
            perror("fork");
            close_all_pipes(pipefd);
            close(out_fd);
            exit(EXIT_FAILURE);
        }

        if (pid[i] == 0) {  // Child process
            char **new_environ = create_environment(i);  // Set environment

            // Redirect input
            if (i == 0) {  // First child reads from the input file
                int in_fd = open(input_file, O_RDONLY);
                if (in_fd == -1) {
                    perror("open input file");
                    close_all_pipes(pipefd);
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            } else {  // Other children read from the previous pipe
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }

            // Redirect output
            if (i < 2) {  // First two children write to the next pipe
                dup2(pipefd[i][1], STDOUT_FILENO);
            } else {  // Last child writes to the output file
                dup2(out_fd, STDOUT_FILENO);
            }

            close_all_pipes(pipefd);  // Close all pipes
            close(out_fd);

            // Execute the kitty command
            char arg[3];
            snprintf(arg, sizeof(arg), "-%d", i + 2);
            execle(KITTY_EXEC, "kitty", arg, NULL, new_environ);

            // If exec fails
            perror("execle");
            exit(EXIT_FAILURE);
        }
    }

    // Parent process: Close all pipe write ends
    close_all_pipes(pipefd);
    close(out_fd);

    // Wait for all child processes to complete
    for (int i = 0; i < 3; i++) {
        int status;
        waitpid(pid[i], &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            fprintf(stderr, "Child %d exited with status %d\n", i, WEXITSTATUS(status));
            exit(EXIT_FAILURE);
        }
    }

    printf("All child processes completed successfully\n");
    return 0;
}
