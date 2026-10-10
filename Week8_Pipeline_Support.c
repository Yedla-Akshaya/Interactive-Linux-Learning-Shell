#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INPUT_SIZE 1024
#define MAX_ARGS 100
#define HISTORY_SIZE 100
#define EXPANDED_SIZE 4096

static char history[HISTORY_SIZE][INPUT_SIZE];
static int history_count = 0;
static char history_path[INPUT_SIZE];

/* ---------- Persistent command history ---------- */

static void add_history(const char *command)
{
    if (command == NULL || *command == '\0')
        return;

    if (history_count == HISTORY_SIZE) {
        memmove(history, history + 1,
                sizeof(history[0]) * (HISTORY_SIZE - 1));
        history_count--;
    }

    snprintf(history[history_count], INPUT_SIZE, "%s", command);
    history_count++;
}

static void save_history_entry(const char *command)
{
    FILE *file = fopen(history_path, "a");

    if (file == NULL) {
        perror("myshell: history");
        return;
    }

    fprintf(file, "%s\n", command);
    fclose(file);
}

static void load_history(void)
{
    const char *home = getenv("HOME");

    if (home == NULL || snprintf(history_path, sizeof(history_path),
                                "%s/.myshell_history", home)
                        >= (int)sizeof(history_path)) {
        snprintf(history_path, sizeof(history_path),
                 ".myshell_history");
    }

    FILE *file = fopen(history_path, "r");

    if (file == NULL)
        return;

    char line[INPUT_SIZE];

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0';

        if (*line != '\0')
            add_history(line);
    }

    fclose(file);
}

static void show_history(void)
{
    for (int i = 0; i < history_count; i++)
        printf("%4d  %s\n", i + 1, history[i]);
}

/* ---------- History search ---------- */

static int contains_ignore_case(const char *text,
                                const char *keyword)
{
    if (text == NULL || keyword == NULL)
        return 0;

    if (*keyword == '\0')
        return 1;

    for (; *text != '\0'; text++) {
        const char *a = text;
        const char *b = keyword;

        while (*a != '\0' && *b != '\0' &&
               tolower((unsigned char)*a) ==
               tolower((unsigned char)*b)) {
            a++;
            b++;
        }

        if (*b == '\0')
            return 1;
    }

    return 0;
}

static void find_history(const char *keyword)
{
    if (keyword == NULL || *keyword == '\0') {
        fprintf(stderr, "Usage: findhistory KEYWORD\n");
        return;
    }

    int matches = 0;

    /*
     * The latest entry is the findhistory command currently
     * being executed. Exclude it so a search cannot match itself.
     */
    int search_count = history_count;

    if (search_count > 0)
        search_count--;

    for (int i = 0; i < search_count; i++) {
        if (contains_ignore_case(history[i], keyword)) {
            printf("%4d  %s\n", i + 1, history[i]);
            matches++;
        }
    }

    if (matches == 0)
        printf("No history entries found containing \"%s\".\n",
               keyword);
}

/* ---------- Information command ---------- */

static void show_info(void)
{
    printf("\n");
    printf("====================================\n");
    printf("       MyShell - Week 8\n");
    printf("====================================\n");
    printf("Interactive Linux Learning Shell\n");
    printf("Features:\n");
    printf("  - External command execution\n");
    printf("  - Built-in commands\n");
    printf("  - Environment variables\n");
    printf("  - Single pipeline support\n");
    printf("  - Persistent command history\n");
    printf("  - Command history search\n");
    printf("Built-ins: cd, pwd, history, findhistory,\n");
    printf("           export, info, exit\n");
    printf("====================================\n\n");
}

/* ---------- Environment variable expansion ---------- */

static int expand_variables(const char *input,
                            char *output,
                            size_t output_size)
{
    size_t used = 0;

    if (output_size == 0)
        return -1;

    for (size_t i = 0; input[i] != '\0';) {
        if (input[i] == '$') {
            size_t start;
            size_t length;
            char variable[INPUT_SIZE];
            const char *value = "";

            if (input[i + 1] == '{') {
                start = i + 2;
                size_t end = start;

                while (input[end] != '\0' && input[end] != '}')
                    end++;

                if (input[end] != '}') {
                    fprintf(stderr,
                            "myshell: unmatched ${ in variable\n");
                    return -1;
                }

                length = end - start;
                i = end + 1;
            } else {
                start = i + 1;

                if (!(isalpha((unsigned char)input[start]) ||
                      input[start] == '_')) {
                    if (used + 1 >= output_size)
                        return -1;

                    output[used++] = input[i++];
                    continue;
                }

                size_t end = start;

                while (isalnum((unsigned char)input[end]) ||
                       input[end] == '_')
                    end++;

                length = end - start;
                i = end;
            }

            if (length >= sizeof(variable)) {
                fprintf(stderr,
                        "myshell: variable name too long\n");
                return -1;
            }

            memcpy(variable, input + start, length);
            variable[length] = '\0';

            const char *environment_value = getenv(variable);

            if (environment_value != NULL)
                value = environment_value;

            size_t value_length = strlen(value);

            if (value_length >= output_size - used) {
                fprintf(stderr,
                        "myshell: expanded command is too long\n");
                return -1;
            }

            memcpy(output + used, value, value_length);
            used += value_length;
        } else {
            if (used + 1 >= output_size) {
                fprintf(stderr,
                        "myshell: expanded command is too long\n");
                return -1;
            }

            output[used++] = input[i++];
        }
    }

    output[used] = '\0';
    return 0;
}

/* ---------- Command parser ---------- */

/*
 * Split a command into arguments.
 * Supports single quotes, double quotes and backslash escaping.
 * Returns -1 for unmatched quotes or too many arguments.
 */
static int parse_command(char *command,
                         char *args[],
                         char *storage,
                         size_t storage_size)
{
    size_t input_length = strlen(command);
    size_t out = 0;
    int argc = 0;
    char quote = '\0';
    int token_started = 0;
    size_t token_start = 0;

    for (size_t i = 0; i <= input_length; i++) {
        char c = command[i];

        if (c == '\0') {
            if (quote != '\0') {
                fprintf(stderr, "myshell: unmatched quote\n");
                return -1;
            }

            if (token_started) {
                if (argc >= MAX_ARGS - 1 ||
                    out >= storage_size) {
                    fprintf(stderr,
                            "myshell: too many arguments\n");
                    return -1;
                }

                storage[out++] = '\0';
                args[argc++] = &storage[token_start];
            }

            break;
        }

        if (quote != '\0') {
            if (c == quote) {
                quote = '\0';
            } else if (c == '\\' && quote == '"' &&
                       command[i + 1] != '\0') {
                if (out + 1 >= storage_size)
                    return -1;

                storage[out++] = command[++i];
            } else {
                if (out + 1 >= storage_size)
                    return -1;

                storage[out++] = c;
            }

            token_started = 1;
            continue;
        }

        if (c == '\'' || c == '"') {
            if (!token_started)
                token_start = out;

            quote = c;
            token_started = 1;
            continue;
        }

        if (c == '\\' && command[i + 1] != '\0') {
            if (!token_started)
                token_start = out;

            if (out + 1 >= storage_size)
                return -1;

            storage[out++] = command[++i];
            token_started = 1;
            continue;
        }

        if (isspace((unsigned char)c)) {
            if (token_started) {
                if (argc >= MAX_ARGS - 1 ||
                    out >= storage_size) {
                    fprintf(stderr,
                            "myshell: too many arguments\n");
                    return -1;
                }

                storage[out++] = '\0';
                args[argc++] = &storage[token_start];
                token_started = 0;
            }

            continue;
        }

        if (!token_started)
            token_start = out;

        if (out + 1 >= storage_size) {
            fprintf(stderr, "myshell: command is too long\n");
            return -1;
        }

        storage[out++] = c;
        token_started = 1;
    }

    args[argc] = NULL;
    return argc;
}

/* ---------- External command execution ---------- */

static int execute_external(char *args[])
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("myshell: fork");
        return -1;
    }

    if (pid == 0) {
        execvp(args[0], args);
        fprintf(stderr, "myshell: %s: %s\n",
                args[0], strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }

    int status;

    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR)
            continue;

        perror("myshell: waitpid");
        return -1;
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);

    return -1;
}

/* ---------- Built-in commands ---------- */

static int is_builtin(const char *command)
{
    return strcmp(command, "exit") == 0 ||
           strcmp(command, "cd") == 0 ||
           strcmp(command, "pwd") == 0 ||
           strcmp(command, "history") == 0 ||
           strcmp(command, "findhistory") == 0 ||
           strcmp(command, "info") == 0 ||
           strcmp(command, "export") == 0;
}

/*
 * Returns 1 when the shell should exit.
 * Returns 0 when the shell should continue.
 */
static int execute_builtin(char *args[])
{
    if (strcmp(args[0], "exit") == 0)
        return 1;

    if (strcmp(args[0], "cd") == 0) {
        const char *directory = args[1];

        if (directory == NULL || strcmp(directory, "~") == 0)
            directory = getenv("HOME");

        if (directory == NULL) {
            fprintf(stderr, "myshell: HOME is not set\n");
        } else if (chdir(directory) != 0) {
            fprintf(stderr, "myshell: cd: %s\n",
                    strerror(errno));
        }

        return 0;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char directory[PATH_MAX];

        if (getcwd(directory, sizeof(directory)) == NULL)
            perror("myshell: pwd");
        else
            puts(directory);

        return 0;
    }

    if (strcmp(args[0], "history") == 0) {
        show_history();
        return 0;
    }

    if (strcmp(args[0], "findhistory") == 0) {
        find_history(args[1]);
        return 0;
    }

    if (strcmp(args[0], "info") == 0) {
        show_info();
        return 0;
    }

    if (strcmp(args[0], "export") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "Usage: export NAME=VALUE\n");
            return 0;
        }

        char *equals = strchr(args[1], '=');

        if (equals == NULL || equals == args[1]) {
            fprintf(stderr, "Usage: export NAME=VALUE\n");
            return 0;
        }

        *equals = '\0';

        if (!(isalpha((unsigned char)args[1][0]) ||
              args[1][0] == '_')) {
            fprintf(stderr, "myshell: invalid variable name\n");
            return 0;
        }

        for (char *p = args[1] + 1; *p != '\0'; p++) {
            if (!(isalnum((unsigned char)*p) || *p == '_')) {
                fprintf(stderr, "myshell: invalid variable name\n");
                return 0;
            }
        }

        if (setenv(args[1], equals + 1, 1) != 0)
            perror("myshell: export");

        return 0;
    }

    return 0;
}

/* ---------- Single pipeline support ---------- */

static int execute_pipeline(char *command)
{
    char *pipe_position = strchr(command, '|');

    if (pipe_position == NULL)
        return 0;

    if (strchr(pipe_position + 1, '|') != NULL) {
        fprintf(stderr,
                "myshell: only one pipe is supported\n");
        return 1;
    }

    *pipe_position = '\0';

    char *left_command = command;
    char *right_command = pipe_position + 1;

    while (isspace((unsigned char)*left_command))
        left_command++;

    while (isspace((unsigned char)*right_command))
        right_command++;

    if (*left_command == '\0' || *right_command == '\0') {
        fprintf(stderr, "myshell: invalid pipe command\n");
        return 1;
    }

    char left_expanded[EXPANDED_SIZE];
    char right_expanded[EXPANDED_SIZE];

    if (expand_variables(left_command, left_expanded,
                         sizeof(left_expanded)) != 0 ||
        expand_variables(right_command, right_expanded,
                         sizeof(right_expanded)) != 0)
        return 1;

    char left_storage[INPUT_SIZE];
    char right_storage[INPUT_SIZE];
    char *left_args[MAX_ARGS];
    char *right_args[MAX_ARGS];

    int left_argc = parse_command(left_expanded, left_args,
                                  left_storage,
                                  sizeof(left_storage));
    int right_argc = parse_command(right_expanded, right_args,
                                   right_storage,
                                   sizeof(right_storage));

    if (left_argc <= 0 || right_argc <= 0) {
        fprintf(stderr, "myshell: invalid pipe command\n");
        return 1;
    }

    /*
     * Built-ins are executed in the parent shell, so reject them
     * in pipelines rather than unexpectedly changing shell state.
     */
    if (is_builtin(left_args[0]) || is_builtin(right_args[0])) {
        fprintf(stderr,
                "myshell: built-ins are not supported in pipelines\n");
        return 1;
    }

    int pipefd[2];

    if (pipe(pipefd) < 0) {
        perror("myshell: pipe");
        return 1;
    }

    pid_t first_pid = fork();

    if (first_pid < 0) {
        perror("myshell: fork");
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    if (first_pid == 0) {
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            perror("myshell: dup2");
            _exit(1);
        }

        close(pipefd[1]);
        execvp(left_args[0], left_args);

        fprintf(stderr, "myshell: %s: %s\n",
                left_args[0], strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }

    pid_t second_pid = fork();

    if (second_pid < 0) {
        perror("myshell: fork");
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(first_pid, NULL, 0);
        return 1;
    }

    if (second_pid == 0) {
        close(pipefd[1]);

        if (dup2(pipefd[0], STDIN_FILENO) < 0) {
            perror("myshell: dup2");
            _exit(1);
        }

        close(pipefd[0]);
        execvp(right_args[0], right_args);

        fprintf(stderr, "myshell: %s: %s\n",
                right_args[0], strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    int status;
    while (waitpid(first_pid, &status, 0) < 0 && errno == EINTR)
        ;

    while (waitpid(second_pid, &status, 0) < 0 && errno == EINTR)
        ;

    return 1;
}

/* ---------- Main shell loop ---------- */

int main(void)
{
    char input[INPUT_SIZE];

    load_history();

    printf("Welcome to MyShell! Type 'info' for information.\n");

    while (1) {
        char directory[PATH_MAX];

        if (getcwd(directory, sizeof(directory)) == NULL)
            snprintf(directory, sizeof(directory), "?");

        printf("myshell:%s$ ", directory);
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (feof(stdin))
                break;

            if (errno == EINTR) {
                clearerr(stdin);
                putchar('\n');
                continue;
            }

            perror("myshell: input");
            break;
        }

        if (strchr(input, '\n') == NULL && !feof(stdin)) {
            int c;

            while ((c = getchar()) != '\n' && c != EOF)
                ;

            fprintf(stderr, "myshell: command is too long\n");
            continue;
        }

        input[strcspn(input, "\n")] = '\0';

        char *start = input;

        while (isspace((unsigned char)*start))
            start++;

        if (*start == '\0')
            continue;

        /* Save the original command to in-memory and disk history. */
        add_history(start);
        save_history_entry(start);

        char expanded[EXPANDED_SIZE];

        if (expand_variables(start, expanded,
                             sizeof(expanded)) != 0)
            continue;

        /* Handle a single pipeline first. */
        if (strchr(expanded, '|') != NULL) {
            execute_pipeline(expanded);
            continue;
        }

        char storage[INPUT_SIZE];
        char *args[MAX_ARGS];

        int argc = parse_command(expanded, args,
                                 storage, sizeof(storage));

        if (argc <= 0)
            continue;

        if (is_builtin(args[0])) {
            if (execute_builtin(args))
                break;
        } else {
            execute_external(args);
        }
    }

    printf("\nExiting MyShell. Goodbye!\n");
    return 0;
}
