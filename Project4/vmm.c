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
#include <stdbool.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define PAGE_SIZE 4096
#define FRAME_COUNT 256
#define VIRTUAL_MEMORY_SIZE (PAGE_SIZE * FRAME_COUNT)

// Define Page Table Entry and Page Table Structures
typedef struct {
    int frame_number;
    bool valid;
} PageTableEntry;

typedef struct {
    PageTableEntry entries[FRAME_COUNT];
} PageTable;

// Define Process Control Block and Frame Structures
typedef struct {
    int pid;
    PageTable page_table;
} ProcessControlBlock;

typedef struct {
    int process_id;
    int page_number;
    bool dirty;
    bool valid;
} Frame;

// Initialize Memory and Process Structures
Frame physical_memory[FRAME_COUNT];
ProcessControlBlock processes[FRAME_COUNT];
int next_free_frame = 0;
pid_t child_pid = -1;

// Signal Handler to Exit Shell
void exit_shell(int sig) {
    printf("\nExiting shell...\n");
    exit(0);
}

// Signal Handler to End Execution
void end_execution(int sig) {
    if (child_pid > 0) {
        kill(child_pid, SIGKILL);
        printf("\nCommand interrupted. Returning to prompt...\n");
    }
}

// Split Input into Commands
void split_commands(char *input, char **commands) {
    char *token = strtok(input, ";");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {
        commands[i++] = token;
        token = strtok(NULL, ";");
    }
    commands[i] = NULL;
}

// Parse Command into Arguments
void parse_command(char *cmd, char **args) {
    char *token = strtok(cmd, " \n");
    int i = 0;
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \n");
    }
    args[i] = NULL;
}

// Allocate Frame for a Process
int allocate_frame(int process_id, int page_number) {
    if (next_free_frame < FRAME_COUNT) {
        int frame_number = next_free_frame++;
        physical_memory[frame_number].process_id = process_id;
        physical_memory[frame_number].page_number = page_number;
        physical_memory[frame_number].valid = true;
        return frame_number;
    }
    // Implement a page replacement algorithm here (e.g., FIFO, LRU)
    return -1; // No free frames available
}

// Handle Page Fault by Allocating Frame and Loading Page
void handle_page_fault(int process_id, int page_number) {
    int frame_number = allocate_frame(process_id, page_number);
    if (frame_number == -1) {
        printf("No free frames available. Page replacement needed.\n");
        return;
    }
    processes[process_id].page_table.entries[page_number].frame_number = frame_number;
    processes[process_id].page_table.entries[page_number].valid = true;
    printf("Page %d allocated to frame %d for process %d\n", page_number, frame_number, process_id);
}

// Free a Specific Frame and Update Page Table
void free_memory(int process_id, int page_number) {
    int frame_number = processes[process_id].page_table.entries[page_number].frame_number;
    if (frame_number != -1 && processes[process_id].page_table.entries[page_number].valid) {
        physical_memory[frame_number].valid = false;
        processes[process_id].page_table.entries[page_number].valid = false;
        printf("Freed frame %d for process %d page %d\n", frame_number, process_id, page_number);
    } else {
        printf("Invalid free request for process %d page %d\n", process_id, page_number);
    }
}

// Display the Current State of Physical Memory and Page Tables
void show_memory() {
    printf("Physical Memory State:\n");
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (physical_memory[i].valid) {
            printf("Frame %d: Process %d Page %d\n", i, physical_memory[i].process_id, physical_memory[i].page_number);
        }
    }
    printf("Page Tables:\n");
    for (int i = 0; i < FRAME_COUNT; i++) {
        if (processes[i].pid != 0) {
            printf("Process %d Page Table:\n", processes[i].pid);
            for (int j = 0; j < FRAME_COUNT; j++) {
                if (processes[i].page_table.entries[j].valid) {
                    printf("  Page %d -> Frame %d\n", j, processes[i].page_table.entries[j].frame_number);
                }
            }
        }
    }
}

// Execute Command in a Child Process
void execute_command(char **args) {
    child_pid = fork();
    if (child_pid < 0) {
        perror("Fork failed");
        exit(1);
    } else if (child_pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("Exec failed");
            exit(1);
        }
    } else {
        int status;
        waitpid(child_pid, &status, 0);
        child_pid = -1;
    }
}

// Execute Multiple Commands Separated by ";"
void execute_commands(char *line) {
    char *commands[MAX_ARGS];
    split_commands(line, commands);
    for (int i = 0; commands[i] != NULL; i++) {
        char *args[MAX_ARGS];
        parse_command(commands[i], args);
        if (strcmp(args[0], "access_memory") == 0) {
            int process_id = atoi(args[1]);
            int page_number = atoi(args[2]);
            handle_page_fault(process_id, page_number);
        } else if (strcmp(args[0], "free_memory") == 0) {
            int process_id = atoi(args[1]);
            int page_number = atoi(args[2]);
            free_memory(process_id, page_number);
        } else if (strcmp(args[0], "show_memory") == 0) {
            show_memory();
        } else {
            execute_command(args);
        }
    }
}

// Execute Commands from a Batch File
void execute_batch_file(char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to open batch file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        printf("Batch command: %s\n", line);
        execute_commands(line);
    }
    fclose(file);
}

// Main Function
int main(int argc, char *argv[]) {
    signal(SIGINT, exit_shell);
    signal(SIGQUIT, end_execution);

    using_history();

    if (argc == 2) {
        execute_batch_file(argv[1]);
        return 0;
    }

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
