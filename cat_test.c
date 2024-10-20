#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#define KITTY_PATH "/var/local/isse-07/kitty"

void close_extra_fds() {
    for (int fd = 3; fd < 1024; fd++) {
        close(fd);
    }
}

char *minimal_env[] = {
    "PATH=/home/puwase",
    "HOME=/home/puwase",
    "CATFOOD=yummy",
    "SHELL=/bin/bash",  // Add common environment variables
    "USER=puwase",
    NULL
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s inputfile outputfile\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], argv[2]) == 0) {
        fprintf(stderr, "Input and output files must be different.\n");
        return 1;
    }

    int input_fd = open(argv[1], O_RDONLY);
    if (input_fd < 0) {
        perror("Error opening input file");
        return 1;
    }

    int output_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        perror("Error opening output file");
        close(input_fd);
        return 1;
    }

    int pipe1[2], pipe2[2];
    pipe(pipe1);
    pipe(pipe2);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        close(pipe1[0]);
        dup2(input_fd, STDIN_FILENO);
        dup2(pipe1[1], STDOUT_FILENO);
        close(input_fd);
        close(pipe1[1]);

        close_extra_fds();
        execle(KITTY_PATH, "kitty", "-0", NULL, minimal_env);
        perror("exec failed");
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        close(pipe1[1]);
        close(pipe2[0]);
        dup2(pipe1[0], STDIN_FILENO);
        dup2(pipe2[1], STDOUT_FILENO);
        close(pipe1[0]);
        close(pipe2[1]);

        close_extra_fds();
        execle(KITTY_PATH, "kitty", "-3", NULL, minimal_env);  // Use -3 here
        perror("exec failed");
        exit(1);
    }

    pid_t pid3 = fork();
    if (pid3 == 0) {
        close(pipe2[1]);
        dup2(pipe2[0], STDIN_FILENO);
        dup2(output_fd, STDOUT_FILENO);
        close(pipe2[0]);
        close(output_fd);

        close_extra_fds();
        execle(KITTY_PATH, "kitty", "-4", NULL, minimal_env);
        perror("exec failed");
        exit(1);
    }

    close(input_fd);
    close(output_fd);
    close(pipe1[0]);
    close(pipe1[1]);
    close(pipe2[0]);
    close(pipe2[1]);

    int status1, status2, status3;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);
    waitpid(pid3, &status3, 0);

    if (WIFEXITED(status1) && WEXITSTATUS(status1) == 0 &&
        WIFEXITED(status2) && WEXITSTATUS(status2) == 0 &&
        WIFEXITED(status3) && WEXITSTATUS(status3) == 0) {
        return 0;
    } else {
        return 1;
    }
}
