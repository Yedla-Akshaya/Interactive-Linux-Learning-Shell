#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {

    char input[1024];

    // Store previous commands
    char history[100][1024];
    int history_count = 0;

    while (1) {

        // Display shell prompt
        printf("MyShell> ");
        fflush(stdout);

        // Read user input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        // Reject commands that do not fit in the input buffer.
        size_t input_len = strlen(input);

        if (input_len > 0 &&
            input[input_len - 1] != '\n' &&
            !feof(stdin)) {

            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {
                // Discard the rest of the oversized command.
            }

            fprintf(stderr,
                    "Input too long (maximum %zu characters). Command ignored.\n",
                    sizeof(input) - 1);
            continue;
        }

        // Remove newline
        input[strcspn(input, "\n")] = '\0';

        // Ignore empty input
        if (strlen(input) == 0) {
            continue;
        }

        // Save command in history
        if (history_count < 100) {
            strcpy(history[history_count], input);
            history_count++;
        }

        // Exit command
        if (strcmp(input, "exit") == 0) {
            break;
        }

        // Check for pipe BEFORE strtok changes input
        int has_pipe = (strchr(input, '|') != NULL);

        // Split input into arguments
        char *args[20];
        int i = 0;

        char *token = strtok(input, " ");

        while (token != NULL && i < 19) {
            args[i] = token;
            i++;
            token = strtok(NULL, " ");
        }

        args[i] = NULL;

        if (args[0] == NULL) {
            continue;
        }

        // =========================
        // Built-in: cd
        // =========================

        if (strcmp(args[0], "cd") == 0) {

            if (args[1] == NULL) {
                printf("Usage: cd <directory>\n");
            }
            else if (chdir(args[1]) != 0) {
                perror("cd");
            }

            continue;
        }

        // =========================
        // Built-in: hello
        // =========================

        if (strcmp(args[0], "hello") == 0) {

            printf("Hello! Welcome to MyShell.\n");

            continue;
        }

        // =========================
        // Built-in: info
        // =========================

        if (strcmp(args[0], "info") == 0) {

            printf("MyShell - A simple Linux shell written in C\n");
            printf("Process ID: %d\n", getpid());

            continue;
        }

        // =========================
        // Built-in: help
        // =========================

        if (strcmp(args[0], "help") == 0) {

            printf("Available commands:\n");
            printf("cd      - Change directory\n");
            printf("pwd     - Show current directory\n");
            printf("help    - Show available commands\n");
            printf("history - Show previous commands\n");
            printf("hello   - Display greeting\n");
            printf("info    - Show shell information\n");
            printf("exit    - Exit MyShell\n");

            continue;
        }

        // =========================
        // Built-in: history
        // =========================

        if (strcmp(args[0], "history") == 0) {

            for (int j = 0; j < history_count; j++) {
                printf("%d  %s\n", j + 1, history[j]);
            }

            continue;
        }

        // =========================
        // PIPE
        // Example:
        // ls | grep c
        // =========================

        if (has_pipe) {

            int pipe_index = -1;

            // Find "|" in arguments
            for (int j = 0; args[j] != NULL; j++) {

                if (strcmp(args[j], "|") == 0) {
                    pipe_index = j;
                    break;
                }
            }

            // Safety check
            if (pipe_index == -1) {
                printf("Pipe error\n");
                continue;
            }

            // Separate left and right commands
            args[pipe_index] = NULL;

            char **left_command = args;
            char **right_command = &args[pipe_index + 1];

            // Create pipe
            int pipefd[2];

            if (pipe(pipefd) == -1) {
                perror("pipe failed");
                continue;
            }

            // =========================
            // First child
            // Runs left command
            // =========================

            pid_t pid1 = fork();

            if (pid1 < 0) {

                perror("fork failed");
                continue;

            }
            else if (pid1 == 0) {

                // Send stdout to pipe
                dup2(pipefd[1], STDOUT_FILENO);

                // Close unused pipe ends
                close(pipefd[0]);
                close(pipefd[1]);

                // Run first command
                execvp(left_command[0], left_command);

                perror("Command failed");
                exit(1);
            }

            // =========================
            // Second child
            // Runs right command
            // =========================

            pid_t pid2 = fork();

            if (pid2 < 0) {

                perror("fork failed");
                continue;

            }
            else if (pid2 == 0) {

                // Take input from pipe
                dup2(pipefd[0], STDIN_FILENO);

                // Close unused pipe ends
                close(pipefd[0]);
                close(pipefd[1]);

                // Run second command
                execvp(right_command[0], right_command);

                perror("Command failed");
                exit(1);
            }

            // Parent closes pipe
            close(pipefd[0]);
            close(pipefd[1]);

            // Parent waits for both children
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);

        }

        // =========================
        // NORMAL COMMAND
        // =========================

        else {

            pid_t pid = fork();

            if (pid < 0) {

                perror("fork failed");

            }
            else if (pid == 0) {

                // Child runs command
                execvp(args[0], args);

                // If execvp fails
                perror("Command failed");
                exit(1);

            }
            else {

                // Parent waits for child
                wait(NULL);
            }
        }
    }

    return 0;
}
