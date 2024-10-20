#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

void error_exit(const char *msg) {
    perror(msg);
    exit(1);
}

void setup_pipe(int pipefd[2]) {
    if (pipe(pipefd) == -1) {
        error_exit("pipe");
    }
}

void child_process(const char *arg, char **envp, int input_fd, int output_fd) {
    // Redirect input and output
    if (dup2(input_fd, STDIN_FILENO) == -1) error_exit("dup2 input");
    if (dup2(output_fd, STDOUT_FILENO) == -1) error_exit("dup2 output");

    // Close unused file descriptors
    close(input_fd);
    close(output_fd);

    // Execute the kitty process
    execlp("kitty", "kitty", arg, NULL);
    error_exit("exec");
}

int main(int argc, char *argv[], char *envp[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: cat-herder inputfile outputfile\n");
        return 1;
    }

    const char *inputfile = argv[1];
    const char *outputfile = argv[2];

    // Ensure inputfile and outputfile are not the same
    if (strcmp(inputfile, outputfile) == 0) {
        fprintf(stderr, "Error: inputfile and outputfile cannot be the same.\n");
        return 1;
    }

    // Open input and output files
    int input_fd = open(inputfile, O_RDONLY);
    if (input_fd == -1) error_exit("open inputfile");

    int output_fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1) error_exit("open outputfile");

    // Set up pipes for inter-process communication
    int pipe1[2], pipe2[2];
    setup_pipe(pipe1);
    setup_pipe(pipe2);

    pid_t pid1 = fork();
    if (pid1 == 0) {  // First child: kitty -2
        close(pipe1[0]);  // Close read end of pipe1
        char *new_env[] = { "CATFOOD=yummy", NULL };
        child_process("-2", new_env, input_fd, pipe1[1]);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {  // Second child: kitty -3
        close(pipe1[1]);  // Close write end of pipe1
        close(pipe2[0]);  // Close read end of pipe2
        unsetenv("KITTYLITTER");
        child_process("-3", envp, pipe1[0], pipe2[1]);
    }

    pid_t pid3 = fork();
    if (pid3 == 0) {  // Third child: kitty -4
        close(pipe2[1]);  // Close write end of pipe2
        char *new_env[] = {
            "PATH=/usr/bin",
            "HOME=/home/user",
            "CATFOOD=yummy",
            NULL
        };
        child_process("-4", new_env, pipe2[0], output_fd);
    }

    // Parent process closes all file descriptors
    close(input_fd);
    close(output_fd);
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);

    // Wait for all children to exit
    int status1, status2, status3;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);
    waitpid(pid3, &status3, 0);

    // Check exit statuses
    if (WIFEXITED(status1) && WIFEXITED(status2) && WIFEXITED(status3) &&
        WEXITSTATUS(status1) == 0 && WEXITSTATUS(status2) == 0 && WEXITSTATUS(status3) == 0) {
        return 0;
    } else {
        return 1;
    }
}
