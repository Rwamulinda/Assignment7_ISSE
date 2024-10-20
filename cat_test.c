#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#define KITTY_EXEC "/var/local/isse-07/kitty"
#define SCOTTYCHECK_PATH "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin:/var/local/scottycheck/isse-07"
#define LOCAL_PATH "/home/puwase:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/snap/bin"
#define SCOTTYCHECK_HOME "/nonexistent"
#define LOCAL_HOME "/home/puwase"

void setup_environment(int child_index) {
    // Clear the entire environment
    clearenv();

    // Set only the essential environment variables
    if (getenv("SCOTTYCHECK_ENV")) {
        setenv("HOME", SCOTTYCHECK_HOME, 1);
        setenv("PATH", SCOTTYCHECK_PATH, 1);
    } else {
        setenv("HOME", LOCAL_HOME, 1);
        setenv("PATH", LOCAL_PATH, 1);
    }

    // Set CATFOOD only for the first and third child
    if (child_index == 0 || child_index == 2) {
        setenv("CATFOOD", "yummy", 1);
    }

    // Retain SCOTTYCHECK_ENV if present
    const char *scottycheck_env = getenv("SCOTTYCHECK_ENV");
    if (scottycheck_env) {
        setenv("SCOTTYCHECK_ENV", scottycheck_env, 1);
    }
}

void close_all_pipes(int pipefd[2][2]) {
    for (int i = 0; i < 2; i++) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }
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

    for (int i = 0; i < 2; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
        perror("open output file");
        close_all_pipes(pipefd);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < 3; i++) {
        pid[i] = fork();

        if (pid[i] < 0) {
            perror("fork");
            close_all_pipes(pipefd);
            close(out_fd);
            exit(EXIT_FAILURE);
        }

        if (pid[i] == 0) {
            setup_environment(i);

            if (i == 0) {
                int in_fd = open(input_file, O_RDONLY);
                if (in_fd == -1) {
                    perror("open input file");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            } else {
                dup2(pipefd[i - 1][0], STDIN_FILENO);
            }

            if (i < 2) {
                dup2(pipefd[i][1], STDOUT_FILENO);
            } else {
                dup2(out_fd, STDOUT_FILENO);
            }

            close_all_pipes(pipefd);
            close(out_fd);

            char arg[3];
            snprintf(arg, sizeof(arg), "-%d", i + 2);
            execl(KITTY_EXEC, "kitty", arg, NULL);

            perror("execl");
            exit(EXIT_FAILURE);
        }
    }

    close_all_pipes(pipefd);
    close(out_fd);

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
