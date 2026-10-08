
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define INPUT_SIZE 1024
#define HISTORY_SIZE 100
#define MAX_ARGS 64

/* Parse arguments while preserving spaces inside quotes. */
static int parse_arguments(char *input, char *args[], int max_args)
{
    char *src = input;
    char *dst = input;
    int argc = 0;

    while (*src != '\0') {
        while (*src == ' ' || *src == '\t') {
            src++;
        }

        if (*src == '\0' || *src == '\n') {
            break;
        }

        if (argc >= max_args - 1) {
            fprintf(stderr, "Too many arguments (maximum %d).\n",
                    max_args - 1);
            return -1;
        }

        args[argc++] = dst;
        char quote = '\0';

        while (*src != '\0' && *src != '\n') {
            if (quote != '\0') {
                if (*src == quote) {
                    quote = '\0';
                    src++;
                } else {
                    *dst++ = *src++;
                }
            } else if (*src == '\'' || *src == '"') {
                quote = *src++;
            } else if (*src == ' ' || *src == '\t') {
                break;
            } else {
                *dst++ = *src++;
            }
        }

        if (quote != '\0') {
            fprintf(stderr, "Syntax error: unmatched quote.\n");
            return -1;
        }

        /* Skip separators before writing the string terminator. */
        while (*src == ' ' || *src == '\t') {
            src++;
        }

        *dst++ = '\0';
    }

    args[argc] = NULL;
    return argc;
}

/* Display information about MyShell. */
static void show_info(void)
{
    printf("MyShell - Interactive Linux Learning Shell\n");
    printf("A beginner-friendly shell written in C.\n");
    printf("Week 4: Process execution and error handling.\n");
}

/* Wait for a child, retrying when interrupted by a signal. */
static int wait_for_child(pid_t pid, int *status)
{
    pid_t result;

    do {
        result = waitpid(pid, status, 0);
    } while (result == -1 && errno == EINTR);

    if (result == -1) {
        perror("waitpid");
        return -1;
    }

    return 0;
}

/* Report a child's exit status or terminating signal. */
static void report_child_status(const char *command, int status)
{
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);

        if (code != 0) {
            fprintf(stderr, "%s: exited with status %d\n",
                    command, code);
        }
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "%s: terminated by signal %d\n",
                command, WTERMSIG(status));
    }
}

/* Display the command history. */
static void show_history(char history[][INPUT_SIZE], int history_count)
{
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

/* Execute an external command or a single pipeline. */
static void execute_command(char *input)
{
    char *pipe_position = strchr(input, '|');

    if (pipe_position != NULL) {
        /* Split input into the two sides of the pipeline. */
        *pipe_position = '\0';

        char *left_args[MAX_ARGS];
        char *right_args[MAX_ARGS];

        int left_count =
            parse_arguments(input, left_args, MAX_ARGS);

        int right_count =
            parse_arguments(pipe_position + 1, right_args, MAX_ARGS);

        if (left_count <= 0 || right_count <= 0) {
            fprintf(stderr, "Invalid pipeline command.\n");
            return;
        }

        int pipe_fd[2];

        if (pipe(pipe_fd) == -1) {
            perror("pipe");
            return;
        }

        pid_t left_pid = fork();

        if (left_pid == -1) {
            perror("fork");
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            return;
        }

        if (left_pid == 0) {
            /* Connect the first command's standard output to the pipe. */
            if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }

            close(pipe_fd[0]);
            close(pipe_fd[1]);

            execvp(left_args[0], left_args);

            perror(left_args[0]);
            _exit(127);
        }

        pid_t right_pid = fork();

        if (right_pid == -1) {
            perror("fork");

            close(pipe_fd[0]);
            close(pipe_fd[1]);

            int left_status;
            wait_for_child(left_pid, &left_status);
            return;
        }

        if (right_pid == 0) {
            /* Connect the second command's standard input to the pipe. */
            if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
                perror("dup2");
                _exit(1);
            }

            close(pipe_fd[0]);
            close(pipe_fd[1]);

            execvp(right_args[0], right_args);

            perror(right_args[0]);
            _exit(127);
        }

        close(pipe_fd[0]);
        close(pipe_fd[1]);

        int left_status;
        int right_status;

        if (wait_for_child(left_pid, &left_status) == 0) {
            report_child_status(left_args[0], left_status);
        }

        if (wait_for_child(right_pid, &right_status) == 0) {
            report_child_status(right_args[0], right_status);
        }

        return;
    }

    char *args[MAX_ARGS];
    int arg_count = parse_arguments(input, args, MAX_ARGS);

    if (arg_count <= 0) {
        return;
    }

    /* Built-in commands. */
    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    if (strcmp(args[0], "hello") == 0) {
        printf("Hello! Welcome to MyShell.\n");
        return;
    }

    if (strcmp(args[0], "info") == 0) {
        show_info();
        return;
    }

    if (strcmp(args[0], "help") == 0) {
        printf("Available built-ins:\n");
        printf("  hello   - Display a greeting\n");
        printf("  info    - Display shell information\n");
        printf("  help    - Display help information\n");
        printf("  history - Display previous commands\n");
        printf("  cd      - Change the current directory\n");
        printf("  exit    - Exit MyShell\n");
        printf("External commands and one pipeline are supported.\n");
        return;
    }

    if (strcmp(args[0], "cd") == 0) {
        const char *directory =
            (args[1] == NULL) ? getenv("HOME") : args[1];

        if (directory == NULL) {
            fprintf(stderr, "cd: HOME is not set.\n");
        } else if (chdir(directory) == -1) {
            perror("cd");
        }

        return;
    }

    /* Launch an external program. */
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        execvp(args[0], args);

        /* Reached only if execvp fails. */
        perror(args[0]);
        _exit(127);
    }

    int status;

    if (wait_for_child(pid, &status) == 0) {
        report_child_status(args[0], status);
    }
}

int main(void)
{
    char input[INPUT_SIZE];
    char history[HISTORY_SIZE][INPUT_SIZE];
    int history_count = 0;

    while (1) {
        printf("MyShell> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        size_t input_len = strlen(input);

        /* Reject commands longer than the input buffer. */
        if (input_len > 0 &&
            input[input_len - 1] != '\n' &&
            !feof(stdin)) {

            int ch;

            while ((ch = getchar()) != '\n' && ch != EOF) {
                /* Discard the remaining characters. */
            }

            fprintf(stderr,
                    "Input too long (maximum %d characters).\n",
                    INPUT_SIZE - 1);
            continue;
        }

        input[strcspn(input, "\n")] = '\0';

        if (input[0] == '\0') {
            continue;
        }

        /* Store the original command before parsing modifies it. */
        if (history_count < HISTORY_SIZE) {
            strcpy(history[history_count], input);
            history_count++;
        }

        if (strcmp(input, "history") == 0) {
            show_history(history, history_count);
            continue;
        }

        execute_command(input);
    }

    return 0;
}
