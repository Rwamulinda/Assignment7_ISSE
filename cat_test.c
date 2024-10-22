#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

// Error checking macro
#define CHECK(expr, msg) \
    if ((expr) == -1) { perror(msg); exit(1); }

// Function to set up environment variables for each child process
char **set_env_for_kitty(int kitty_number, char *envp[]) {
    extern char **environ;
    char **new_env;

    switch (kitty_number) {
        case 2:
            new_env = environ;
            setenv("CATFOOD", "yummy", 1);  // Add CATFOOD=yummy
            break;
        case 3:
            new_env = environ;
            unsetenv("KITTYLITTER");  // Remove KITTYLITTER
            break;
        case 4:
            // Create a restricted environment
            new_env = (char **)malloc(4 * sizeof(char *));
            new_env[0] = getenv("PATH");
            new_env[1] = getenv("HOME");
            new_env[2] = strdup("CATFOOD=yummy");
            new_env[3] = NULL;
            break;
    }
    return new_env;
}

// Function to fork and exec each kitty process
void start_kitty(int kitty_number, int in_fd, int out_fd, char *envp[]) {
    pid_t pid = fork();
    CHECK(pid, "fork");

    if (pid == 0) {  // Child process
        // Redirect stdin and stdout
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }

        // Prepare the environment
        char **new_env = set_env_for_kitty(kitty_number, envp);

        // Execute the kitty process
        char cmd[10];
        snprintf(cmd, sizeof(cmd), "kitty -%d", kitty_number);
        execlp("kitty", "kitty", cmd, (char *)NULL);
        perror("execlp failed");  // If execlp fails
        exit(1);
    }
}

// Main function
int main(int argc, char *argv[], char *envp[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <inputfile> <outputfile>\n", argv[0]);
        exit(1);
    }

    // Ensure input and output files are different
    if (strcmp(argv[1], argv[2]) == 0) {
        fprintf(stderr, "Error: inputfile and outputfile must be different.\n");
        exit(1);
    }

    // Open input and output files
    int input_fd = open(argv[1], O_RDONLY);
    CHECK(input_fd, "open input file");
    int output_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK(output_fd, "open output file");

    // Set up pipes
    int pipe1[2], pipe2[2];
    CHECK(pipe(pipe1), "pipe1");
    CHECK(pipe(pipe2), "pipe2");

    // Start kitty -2
    start_kitty(2, input_fd, pipe1[1], envp);
    close(input_fd);
    close(pipe1[1]);

    // Start kitty -3
    start_kitty(3, pipe1[0], pipe2[1], envp);
    close(pipe1[0]);
    close(pipe2[1]);

    // Start kitty -4
    start_kitty(4, pipe2[0], output_fd, envp);
    close(pipe2[0]);
    close(output_fd);

    // Wait for all children to complete
    int status;
    int exit_code = 0;
    for (int i = 0; i < 3; i++) {
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            exit_code = 1;
        }
    }

    return exit_code;
}
