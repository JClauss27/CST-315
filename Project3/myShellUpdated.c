// Joseph Clauss
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

// Global variable to keep track of child processes
pid_t child_pid = -1;

// Signal handler to exit the shell
void exit_shell(int sig) {
    printf("\nExiting shell...\n");
    exit(0);
}

// Signal handler to end the execution of the current command
void end_execution(int sig) {
    if (child_pid > 0) {
        kill(child_pid, SIGKILL); // Kill the child process
        printf("\nCommand interrupted. Returning to prompt...\n");
    }
}

// Function to split the input into commands using ";" as the delimiter
void split_commands(char *input, char **commands) {
    char *token = strtok(input, ";");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {
        commands[i++] = token;
        token = strtok(NULL, ";");
    }
    commands[i] = NULL;
}

// Function to parse the command into arguments using whitespace as the delimiter
void parse_command(char *cmd, char **args) {
    char *token = strtok(cmd, " \n");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \n");
    }
    args[i] = NULL;
}

// Function to execute a single command
void execute_command(char **args) {
    child_pid = fork();
    if (child_pid < 0) {
        perror("Fork failed");
        exit(1);
    } else if (child_pid == 0) {
        // Child process executes the command
        if (execvp(args[0], args) == -1) {
            perror("Exec failed");
            exit(1);
        }
    } else {
        // Parent process waits for the child to complete
        int status;
        waitpid(child_pid, &status, 0);
        child_pid = -1; // Reset child_pid after command execution
    }
}

// Function to execute multiple commands separated by ";"
void execute_commands(char *line) {
    char *commands[MAX_ARGS];
    split_commands(line, commands);
    for (int i = 0; commands[i] != NULL; i++) {
        char *args[MAX_ARGS];
        parse_command(commands[i], args);
        execute_command(args);
    }
}

// Function to execute commands from a batch file
void execute_batch_file(char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to open batch file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        printf("Batch command: %s\n", line); // Print the command before execution
        execute_commands(line);
    }
    fclose(file);
}

int main(int argc, char *argv[]) {
    // Set up signal handlers
    signal(SIGINT, exit_shell);
    signal(SIGQUIT, end_execution);

    // Initialize history feature
    using_history();

    // Check if a batch file is provided
    if (argc == 2) {
        execute_batch_file(argv[1]);
        return 0;
    }

    // Interactive mode
    char *line;
    while (1) {
        line = readline("Shell> ");
        if (line && *line) {
            add_history(line);
            if (strcmp(line, "quit") == 0) {
                free(line);
                break;
            }
            execute_commands(line);
            free(line);
        }
    }
    printf("Exiting Shell...\n");
    return 0;
}
