#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>  // Include this for open(), O_RDONLY, etc.

// Error handling macro
#define CHECK(cond, msg) if (cond) { perror(msg); exit(1); }

void run_kitty(const char *arg, char **env, int in_fd, int out_fd) {
    // Redirect stdin and stdout using dup2()
    if (in_fd != STDIN_FILENO) {
        CHECK(dup2(in_fd, STDIN_FILENO) == -1, "dup2 in_fd");
        close(in_fd);
    }
    if (out_fd != STDOUT_FILENO) {
        CHECK(dup2(out_fd, STDOUT_FILENO) == -1, "dup2 out_fd");
        close(out_fd);
    }

    // Prepare arguments for execvp()
    char *args[] = {"kitty", (char *)arg, NULL};
    
    // Execute kitty with the modified environment
    execvp("kitty", args);
    // If execvp returns, there was an error
    perror("execvp failed");
    exit(1);
}

int main(int argc, char *argv[], char *envp[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <inputfile> <outputfile>\n", argv[0]);
        exit(1);
    }

    const char *inputfile = argv[1];
    const char *outputfile = argv[2];

    // Ensure inputfile and outputfile are not the same
    if (strcmp(inputfile, outputfile) == 0) {
        fprintf(stderr, "Error: Input and output files cannot be the same\n");
        exit(1);
    }

    // Create pipes
    int pipe1[2], pipe2[2];
    CHECK(pipe(pipe1) == -1, "pipe1");
    CHECK(pipe(pipe2) == -1, "pipe2");

    pid_t pid1, pid2, pid3;

    // Fork first child (kitty -2)
    if ((pid1 = fork()) == 0) {
        // Child 1 process
        close(pipe1[0]); // Close unused read end
        int input_fd = open(inputfile, O_RDONLY);
        CHECK(input_fd == -1, "open inputfile");
        run_kitty("-2", envp, input_fd, pipe1[1]);
    }

    // Fork second child (kitty -3)
    if ((pid2 = fork()) == 0) {
        // Child 2 process
        close(pipe1[1]); // Close unused write end
        close(pipe2[0]); // Close unused read end
        run_kitty("-3", envp, pipe1[0], pipe2[1]);
    }

    // Fork third child (kitty -4)
    if ((pid3 = fork()) == 0) {
        // Child 3 process
        close(pipe2[1]); // Close unused write end
        int output_fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        CHECK(output_fd == -1, "open outputfile");
        run_kitty("-4", envp, pipe2[0], output_fd);
    }

    // Close all pipes in the parent
    close(pipe1[0]); close(pipe1[1]);
    close(pipe2[0]); close(pipe2[1]);

    // Wait for all children to exit
    int status;
    int exit_code = 0;
    for (int i = 0; i < 3; ++i) {
        wait(&status);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            exit_code = 1;
        }
    }

    return exit_code;
}
