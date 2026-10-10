
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
    FILE *file = fopen(history_path, "r");
    char line[INPUT_SIZE];

    if (file == NULL)
        return;

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

static int contains_ignore_case(const char *text, const char *keyword)
{
    size_t length = strlen(keyword);

    if (length == 0)
        return 1;

    for (; *text != '\0'; text++) {
        if (strncasecmp(text, keyword, length) == 0)
            return 1;
    }

    return 0;
}

static void find_history(const char *keyword)
{
    int matches = 0;
    int search_count = history_count;

    if (keyword == NULL || *keyword == '\0') {
        fprintf(stderr, "Usage: findhistory KEYWORD\n");
        return;
    }

    /* Exclude the current findhistory invocation. */
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

/* ---------- Environment variable expansion ---------- */

static int expand_variables(const char *input, char *output,
                            size_t output_size)
{
    size_t out = 0;

    for (size_t i = 0; input[i] != '\0';) {
        if (input[i] != '$') {
            if (out + 1 >= output_size)
                return -1;

            output[out++] = input[i++];
            continue;
        }

        i++;

        char name[INPUT_SIZE];
        size_t name_length = 0;

        if (input[i] == '{') {
            i++;

            while (input[i] != '\0' && input[i] != '}') {
                if (name_length + 1 >= sizeof(name))
                    return -1;

                name[name_length++] = input[i++];
            }

            if (input[i] != '}') {
                fprintf(stderr,
                        "myshell: missing '}' in variable expansion\n");
                return -1;
            }

            i++;
        } else {
            while (isalnum((unsigned char)input[i]) ||
                   input[i] == '_') {
                if (name_length + 1 >= sizeof(name))
                    return -1;

                name[name_length++] = input[i++];
            }

            if (name_length == 0) {
                if (out + 1 >= output_size)
                    return -1;

                output[out++] = '$';
                continue;
            }
        }

        name[name_length] = '\0';

        const char *value = getenv(name);

        if (value != NULL) {
            size_t value_length = strlen(value);

            if (out + value_length >= output_size)
                return -1;

            memcpy(output + out, value, value_length);
            out += value_length;
        }
    }

    output[out] = '\0';
    return 0;
}

/* ---------- Command parsing ---------- */

struct command {
    char *args[MAX_ARGS];
    int argc;
    char *input_file;
    char *output_file;
    int append_output;
};

static void free_command(struct command *cmd)
{
    for (int i = 0; i < cmd->argc; i++)
        free(cmd->args[i]);

    free(cmd->input_file);
    free(cmd->output_file);

    memset(cmd, 0, sizeof(*cmd));
}

static char *copy_token(const char *start, size_t length)
{
    char *token = malloc(length + 1);

    if (token == NULL) {
        perror("myshell: malloc");
        return NULL;
    }

    memcpy(token, start, length);
    token[length] = '\0';
    return token;
}

static int parse_command(char *line, struct command *cmd)
{
    char *p = line;

    memset(cmd, 0, sizeof(*cmd));

    while (*p != '\0') {
        while (isspace((unsigned char)*p))
            p++;

        if (*p == '\0')
            break;

        if (*p == '<' || *p == '>') {
            char op = *p++;
            int append = 0;

            if (op == '>' && *p == '>') {
                append = 1;
                p++;
            }

            while (isspace((unsigned char)*p))
                p++;

            if (*p == '\0' || *p == '<' || *p == '>') {
                fprintf(stderr,
                        "myshell: redirection requires a filename\n");
                goto error;
            }

            char filename[INPUT_SIZE];
            size_t length = 0;
            char quote = '\0';

            while (*p != '\0') {
                if (quote == '\0' &&
                    (isspace((unsigned char)*p) ||
                     *p == '<' || *p == '>'))
                    break;

                if (*p == '\\' && quote != '\'') {
                    p++;

                    if (*p == '\0') {
                        fprintf(stderr,
                                "myshell: incomplete escape\n");
                        goto error;
                    }

                    if (length + 1 >= sizeof(filename))
                        goto too_long;

                    filename[length++] = *p++;
                    continue;
                }

                if (*p == '\'' || *p == '"') {
                    if (quote == '\0') {
                        quote = *p++;
                        continue;
                    }

                    if (quote == *p) {
                        quote = '\0';
                        p++;
                        continue;
                    }
                }

                if (length + 1 >= sizeof(filename))
                    goto too_long;

                filename[length++] = *p++;
            }

            if (quote != '\0') {
                fprintf(stderr, "myshell: unmatched quote\n");
                goto error;
            }

            if (length == 0) {
                fprintf(stderr,
                        "myshell: redirection requires a filename\n");
                goto error;
            }

            filename[length] = '\0';
            char *name = copy_token(filename, length);

            if (name == NULL)
                goto error;

            if (op == '<') {
                if (cmd->input_file != NULL) {
                    fprintf(stderr,
                            "myshell: multiple input redirections\n");
                    free(name);
                    goto error;
                }

                cmd->input_file = name;
            } else {
                if (cmd->output_file != NULL) {
                    fprintf(stderr,
                            "myshell: multiple output redirections\n");
                    free(name);
                    goto error;
                }

                cmd->output_file = name;
                cmd->append_output = append;
            }

            continue;
        }

        if (cmd->argc >= MAX_ARGS - 1) {
            fprintf(stderr, "myshell: too many arguments\n");
            goto error;
        }

        char token[EXPANDED_SIZE];
        size_t length = 0;
        char quote = '\0';

        while (*p != '\0') {
            if (quote == '\0' &&
                (isspace((unsigned char)*p) ||
                 *p == '<' || *p == '>'))
                break;

            if (*p == '\\' && quote != '\'') {
                p++;

                if (*p == '\0') {
                    fprintf(stderr,
                            "myshell: incomplete escape\n");
                    goto error;
                }

                if (length + 1 >= sizeof(token))
                    goto too_long;

                token[length++] = *p++;
                continue;
            }

            if (*p == '\'' || *p == '"') {
                if (quote == '\0') {
                    quote = *p++;
                    continue;
                }

                if (quote == *p) {
                    quote = '\0';
                    p++;
                    continue;
                }
            }

            if (length + 1 >= sizeof(token))
                goto too_long;

            token[length++] = *p++;
        }

        if (quote != '\0') {
            fprintf(stderr, "myshell: unmatched quote\n");
            goto error;
        }

        token[length] = '\0';
        cmd->args[cmd->argc] = copy_token(token, length);

        if (cmd->args[cmd->argc] == NULL)
            goto error;

        cmd->argc++;
    }

    cmd->args[cmd->argc] = NULL;
    return 0;

too_long:
    fprintf(stderr, "myshell: token too long\n");

error:
    free_command(cmd);
    return -1;
}

/* ---------- Built-in commands ---------- */

static void show_info(void)
{
    printf("\n====================================\n");
    printf("       MyShell - Week 9\n");
    printf("====================================\n");
    printf("Interactive Linux Learning Shell\n");
    printf("Features:\n");
    printf("  - External command execution\n");
    printf("  - Built-in commands\n");
    printf("  - Environment variables\n");
    printf("  - Single pipeline support\n");
    printf("  - Input/output redirection\n");
    printf("  - Persistent command history\n");
    printf("  - Command history search\n");
    printf("Built-ins: cd, pwd, history, findhistory,\n");
    printf("           export, info, exit\n");
    printf("====================================\n\n");
}

static int is_builtin(const char *name)
{
    return strcmp(name, "cd") == 0 ||
           strcmp(name, "pwd") == 0 ||
           strcmp(name, "history") == 0 ||
           strcmp(name, "findhistory") == 0 ||
           strcmp(name, "info") == 0 ||
           strcmp(name, "export") == 0 ||
           strcmp(name, "exit") == 0;
}

/* Return 1 when the caller should exit; otherwise return 0. */
static int run_builtin(struct command *cmd)
{
    char **args = cmd->args;

    if (strcmp(args[0], "exit") == 0) {
        printf("Exiting MyShell. Goodbye!\n");
        return 1;
    }

    if (strcmp(args[0], "cd") == 0) {
        const char *path = args[1];

        if (path == NULL || strcmp(path, "~") == 0) {
            path = getenv("HOME");
        } else if (strncmp(path, "~/", 2) == 0) {
            const char *home = getenv("HOME");

            if (home == NULL) {
                fprintf(stderr, "myshell: HOME is not set\n");
                return 0;
            }

            static char expanded_path[INPUT_SIZE];

            if (snprintf(expanded_path, sizeof(expanded_path),
                         "%s/%s", home, path + 2) >=
                (int)sizeof(expanded_path)) {
                fprintf(stderr, "myshell: path too long\n");
                return 0;
            }

            path = expanded_path;
        }

        if (path == NULL || chdir(path) != 0)
            perror("myshell: cd");

        return 0;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char cwd[PATH_MAX];

        if (getcwd(cwd, sizeof(cwd)) == NULL)
            perror("myshell: pwd");
        else
            printf("%s\n", cwd);

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

        if (!isalpha((unsigned char)args[1][0]) &&
            args[1][0] != '_') {
            fprintf(stderr, "myshell: invalid variable name\n");
            return 0;
        }

        for (char *p = args[1] + 1; *p != '\0'; p++) {
            if (!isalnum((unsigned char)*p) && *p != '_') {
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

/* ---------- Input/output redirection ---------- */

static int apply_redirections(const struct command *cmd)
{
    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);

        if (fd < 0) {
            perror(cmd->input_file);
            return -1;
        }

        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("myshell: dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT;

        flags |= cmd->append_output ? O_APPEND : O_TRUNC;

        int fd = open(cmd->output_file, flags, 0666);

        if (fd < 0) {
            perror(cmd->output_file);
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("myshell: dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    return 0;
}

/* ---------- External command execution ---------- */

static int execute_external(struct command *cmd)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("myshell: fork");
        return -1;
    }

    if (pid == 0) {
        if (apply_redirections(cmd) != 0)
            _exit(EXIT_FAILURE);

        execvp(cmd->args[0], cmd->args);
        perror(cmd->args[0]);
        _exit(127);
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

    return -1;
}

static int execute_single(struct command *cmd)
{
    if (cmd->argc == 0)
        return 0;

    if (!is_builtin(cmd->args[0]))
        return execute_external(cmd);

    /*
     * Run built-ins in the parent so cd and export affect this shell.
     * Save and restore standard I/O when redirecting a built-in.
     */
    int saved_stdin = -1;
    int saved_stdout = -1;
    int has_redirection =
        cmd->input_file != NULL || cmd->output_file != NULL;

    fflush(NULL);

    if (has_redirection) {
        saved_stdin = dup(STDIN_FILENO);
        saved_stdout = dup(STDOUT_FILENO);

        if (saved_stdin < 0 || saved_stdout < 0) {
            perror("myshell: dup");

            if (saved_stdin >= 0)
                close(saved_stdin);
            if (saved_stdout >= 0)
                close(saved_stdout);

            return -1;
        }

        if (apply_redirections(cmd) != 0) {
            dup2(saved_stdin, STDIN_FILENO);
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdin);
            close(saved_stdout);
            return -1;
        }
    }

    int should_exit = run_builtin(cmd);
    fflush(NULL);

    if (has_redirection) {
        if (dup2(saved_stdin, STDIN_FILENO) < 0)
            perror("myshell: restore stdin");

        if (dup2(saved_stdout, STDOUT_FILENO) < 0)
            perror("myshell: restore stdout");

        close(saved_stdin);
        close(saved_stdout);
    }

    return should_exit ? 100 : 0;
}

/* ---------- Single pipeline support ---------- */

static int execute_pipeline(char *line)
{
    char *pipe_position = strchr(line, '|');

    if (pipe_position == NULL)
        return 0;

    if (strchr(pipe_position + 1, '|') != NULL) {
        fprintf(stderr, "myshell: only one pipe is supported\n");
        return -1;
    }

    *pipe_position = '\0';

    struct command left = {0};
    struct command right = {0};

    if (parse_command(line, &left) != 0)
        return -1;

    if (parse_command(pipe_position + 1, &right) != 0) {
        free_command(&left);
        return -1;
    }

    if (left.argc == 0 || right.argc == 0) {
        fprintf(stderr, "myshell: invalid pipeline\n");
        free_command(&left);
        free_command(&right);
        return -1;
    }

    int pipefd[2];

    if (pipe(pipefd) < 0) {
        perror("myshell: pipe");
        free_command(&left);
        free_command(&right);
        return -1;
    }

    pid_t first = fork();

    if (first < 0) {
        perror("myshell: fork");
        close(pipefd[0]);
        close(pipefd[1]);
        free_command(&left);
        free_command(&right);
        return -1;
    }

    if (first == 0) {
        close(pipefd[0]);

        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            perror("myshell: dup2");
            _exit(EXIT_FAILURE);
        }

        close(pipefd[1]);

        if (left.output_file != NULL) {
            int flags = O_WRONLY | O_CREAT |
                (left.append_output ? O_APPEND : O_TRUNC);

            int fd = open(left.output_file, flags, 0666);

            if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0) {
                perror(left.output_file);

                if (fd >= 0)
                    close(fd);

                _exit(EXIT_FAILURE);
            }

            close(fd);
        }

        if (left.input_file != NULL) {
            int fd = open(left.input_file, O_RDONLY);

            if (fd < 0 || dup2(fd, STDIN_FILENO) < 0) {
                perror(left.input_file);

                if (fd >= 0)
                    close(fd);

                _exit(EXIT_FAILURE);
            }

            close(fd);
        }

        if (is_builtin(left.args[0])) {
            run_builtin(&left);
            fflush(NULL);
            _exit(EXIT_SUCCESS);
        }

        execvp(left.args[0], left.args);
        perror(left.args[0]);
        _exit(127);
    }

    pid_t second = fork();

    if (second < 0) {
        perror("myshell: fork");
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(first, NULL, 0);
        free_command(&left);
        free_command(&right);
        return -1;
    }

    if (second == 0) {
        close(pipefd[1]);

        if (dup2(pipefd[0], STDIN_FILENO) < 0) {
            perror("myshell: dup2");
            _exit(EXIT_FAILURE);
        }

        close(pipefd[0]);

        if (right.input_file != NULL) {
            int fd = open(right.input_file, O_RDONLY);

            if (fd < 0 || dup2(fd, STDIN_FILENO) < 0) {
                perror(right.input_file);

                if (fd >= 0)
                    close(fd);

                _exit(EXIT_FAILURE);
            }

            close(fd);
        }

        if (right.output_file != NULL) {
            int flags = O_WRONLY | O_CREAT |
                (right.append_output ? O_APPEND : O_TRUNC);

            int fd = open(right.output_file, flags, 0666);

            if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0) {
                perror(right.output_file);

                if (fd >= 0)
                    close(fd);

                _exit(EXIT_FAILURE);
            }

            close(fd);
        }

        if (is_builtin(right.args[0])) {
            run_builtin(&right);
            fflush(NULL);
            _exit(EXIT_SUCCESS);
        }

        execvp(right.args[0], right.args);
        perror(right.args[0]);
        _exit(127);
    }

    close(pipefd[0]);
    close(pipefd[1]);

    int status;

    while (waitpid(first, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("myshell: waitpid");
            break;
        }
    }

    while (waitpid(second, &status, 0) < 0) {
        if (errno != EINTR) {
            perror("myshell: waitpid");
            break;
        }
    }

    free_command(&left);
    free_command(&right);
    return 1;
}

/* ---------- Main shell loop ---------- */

int main(void)
{
    const char *home = getenv("HOME");

    if (home == NULL)
        home = ".";

    if (snprintf(history_path, sizeof(history_path),
                 "%s/.myshell_history", home) >=
        (int)sizeof(history_path)) {
        fprintf(stderr, "myshell: history path too long\n");
        return EXIT_FAILURE;
    }

    load_history();

    char input[INPUT_SIZE];
    char expanded[EXPANDED_SIZE];

    printf("Welcome to MyShell! Type 'info' for information.\n");

    for (;;) {
        char cwd[PATH_MAX];

        if (getcwd(cwd, sizeof(cwd)) == NULL)
            snprintf(cwd, sizeof(cwd), "?");

        printf("myshell:%s$ ", cwd);
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            putchar('\n');
            break;
        }

        if (strchr(input, '\n') == NULL && !feof(stdin)) {
            int ch;

            while ((ch = getchar()) != '\n' && ch != EOF)
                ;

            fprintf(stderr, "myshell: command too long\n");
            continue;
        }

        input[strcspn(input, "\n")] = '\0';

        char *start = input;

        while (isspace((unsigned char)*start))
            start++;

        if (*start == '\0')
            continue;

        add_history(start);
        save_history_entry(start);

        if (expand_variables(start, expanded, sizeof(expanded)) != 0) {
            fprintf(stderr, "myshell: expanded command too long\n");
            continue;
        }

        if (strchr(expanded, '|') != NULL) {
            execute_pipeline(expanded);
            continue;
        }

        struct command cmd = {0};

        if (parse_command(expanded, &cmd) != 0)
            continue;

        int result = execute_single(&cmd);
        free_command(&cmd);

        if (result == 100)
            break;
    }

    return EXIT_SUCCESS;
}
