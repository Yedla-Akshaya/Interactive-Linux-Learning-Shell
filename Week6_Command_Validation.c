
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define INPUT_SIZE 1024
#define HISTORY_SIZE 100
#define MAX_ARGS 64
#define PATH_SIZE 4096

extern char **environ;

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
    printf("Week 6: Command validation and history improvements.\n");
}

/* Display the current working directory. */
static void show_pwd(void)
{
    char cwd[PATH_SIZE];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return;
    }

    printf("%s\n", cwd);
}

/* Display all environment variables. */
static void show_environment(void)
{
    for (char **entry = environ; *entry != NULL; entry++) {
        printf("%s\n", *entry);
    }
}

/* Set an environment variable using NAME=value. */
static void set_environment_variable(const char *assignment)
{
    const char *equals = strchr(assignment, '=');

    if (equals == NULL || equals == assignment) {
        fprintf(stderr, "export: use export NAME=value\n");
        return;
    }

    size_t name_length = (size_t)(equals - assignment);
    char *name = malloc(name_length + 1);

    if (name == NULL) {
        perror("export");
        return;
    }

    memcpy(name, assignment, name_length);
    name[name_length] = '\0';

    if (!(name[0] == '_' ||
          (name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z'))) {
        fprintf(stderr, "export: invalid variable name\n");
        free(name);
        return;
    }

    for (size_t i = 1; name[i] != '\0'; i++) {
        if (!(name[i] == '_' ||
              (name[i] >= 'A' && name[i] <= 'Z') ||
              (name[i] >= 'a' && name[i] <= 'z') ||
              (name[i] >= '0' && name[i] <= '9'))) {
            fprintf(stderr, "export: invalid variable name\n");
            free(name);
            return;
        }
    }

    if (setenv(name, equals + 1, 1) == -1) {
        perror("export");
    }

    free(name);
}

/* Display command history. */
static void show_history(char history[][INPUT_SIZE], int history_count)
{
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
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

/* Execute a pipeline containing exactly one pipe. */
static void execute_pipeline(char *input, char *pipe_position)
{
    char *left_args[MAX_ARGS];
    char *right_args[MAX_ARGS];

    /* Reject additional pipes. */
    if (strchr(pipe_position + 1, '|') != NULL) {
        fprintf(stderr,
                "Syntax error: only one pipe is supported.\n");
        return;
    }

    *pipe_position = '\0';

    int left_count = parse_arguments(input, left_args, MAX_ARGS);
    int right_count = parse_arguments(
        pipe_position + 1, right_args, MAX_ARGS);

    if (left_count < 0 || right_count < 0) {
        fprintf(stderr, "Syntax error: invalid pipeline.\n");
        return;
    }

    if (left_count == 0 || right_count == 0) {
        fprintf(stderr,
                "Syntax error: both sides of the pipe need a command.\n");
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
        close(pipe_fd[0]);

        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            _exit(126);
        }

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

        int ignored_status;
        wait_for_child(left_pid, &ignored_status);
        return;
    }

    if (right_pid == 0) {
        close(pipe_fd[1]);

        if (dup2(pipe_fd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            _exit(126);
        }

        close(pipe_fd[0]);
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
}

/* Execute built-in commands and external programs. */
static void execute_command(char *input)
{
    char *pipe_position = strchr(input, '|');

    if (pipe_position != NULL) {
        execute_pipeline(input, pipe_position);
        return;
    }

    char *args[MAX_ARGS];
    int arg_count = parse_arguments(input, args, MAX_ARGS);

    if (arg_count <= 0) {
        return;
    }

    /* Exit MyShell. */
    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    /* Greeting. */
    if (strcmp(args[0], "hello") == 0) {
        printf("Hello! Welcome to MyShell.\n");
        return;
    }

    /* Shell information. */
    if (strcmp(args[0], "info") == 0) {
        show_info();
        return;
    }

    /* Current directory. */
    if (strcmp(args[0], "pwd") == 0) {
        show_pwd();
        return;
    }

    /* Environment variables. */
    if (strcmp(args[0], "env") == 0) {
        show_environment();
        return;
    }

    /* Set environment variable. */
    if (strcmp(args[0], "export") == 0) {
        if (args[1] == NULL || args[2] != NULL) {
            fprintf(stderr, "export: use export NAME=value\n");
        } else {
            set_environment_variable(args[1]);
        }
        return;
    }

    /* Help information. */
    if (strcmp(args[0], "help") == 0) {
        printf("Available built-ins:\n");
        printf("  hello   - Display a greeting\n");
        printf("  info    - Display shell information\n");
        printf("  help    - Display help information\n");
        printf("  history - Display previous commands\n");
        printf("  pwd     - Display the current directory\n");
        printf("  cd      - Change the current directory\n");
        printf("  env     - Display environment variables\n");
        printf("  export  - Set an environment variable\n");
        printf("  exit    - Exit MyShell\n");
        printf("External commands and one pipeline are supported.\n");
        return;
    }

    /* Change directory. */
    if (strcmp(args[0], "cd") == 0) {
        if (args[2] != NULL) {
            fprintf(stderr, "cd: too many arguments\n");
            return;
        }

        char previous_directory[PATH_SIZE];
        char current_directory[PATH_SIZE];

        if (getcwd(previous_directory, sizeof(previous_directory)) == NULL) {
            previous_directory[0] = '\0';
        }

        const char *directory = args[1];

        if (directory == NULL || strcmp(directory, "~") == 0) {
            directory = getenv("HOME");
        } else if (strcmp(directory, "-") == 0) {
            directory = getenv("OLDPWD");

            if (directory == NULL || directory[0] == '\0') {
                fprintf(stderr, "cd: OLDPWD is not set\n");
                return;
            }
        }

        if (directory == NULL || directory[0] == '\0') {
            fprintf(stderr, "cd: target directory is not set\n");
            return;
        }

        if (chdir(directory) == -1) {
            perror("cd");
            return;
        }

        if (previous_directory[0] != '\0' &&
            setenv("OLDPWD", previous_directory, 1) == -1) {
            perror("cd: OLDPWD");
        }

        if (getcwd(current_directory, sizeof(current_directory)) != NULL) {
            if (setenv("PWD", current_directory, 1) == -1) {
                perror("cd: PWD");
            }
        }

        if (args[1] != NULL && strcmp(args[1], "-") == 0) {
            show_pwd();
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
        perror(args[0]);
        _exit(127);
    }

    int status;

    if (wait_for_child(pid, &status) == 0) {
        report_child_status(args[0], status);
    }
}

/* Trim leading and trailing spaces or tabs in place. */
static void trim_whitespace(char *text)
{
    char *start = text;

    while (*start == ' ' || *start == '\t') {
        start++;
    }

    if (start != text) {
        memmove(text, start, strlen(start) + 1);
    }

    size_t length = strlen(text);

    while (length > 0 &&
           (text[length - 1] == ' ' || text[length - 1] == '\t')) {
        text[--length] = '\0';
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

        /* Reject oversized command lines. */
        if (input_len > 0 &&
            input[input_len - 1] != '\n' &&
            !feof(stdin)) {
            int ch;

            while ((ch = getchar()) != '\n' && ch != EOF) {
                /* Discard the rest of the oversized input. */
            }

            fprintf(stderr,
                    "Input too long (maximum %d characters).\n",
                    INPUT_SIZE - 1);
            continue;
        }

        input[strcspn(input, "\n")] = '\0';

        /* Ignore empty or whitespace-only commands. */
        trim_whitespace(input);

        if (input[0] == '\0') {
            continue;
        }

        /* Save the command before parsing changes the input buffer. */
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
