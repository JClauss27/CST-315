// Joseph Clauss
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define TIME_QUANTUM 2
#define DELAY_BETWEEN_PROCESSES 5

typedef struct Process {
    int id;
    char *command;
    int priority;
    bool completed;
    struct Process *next;
} Process;

Process *head = NULL;
Process *tail = NULL;
pthread_mutex_t queue_lock;
pthread_cond_t queue_cond;

void enqueue_process(int id, char *command, int priority) {
    Process *new_process = (Process *)malloc(sizeof(Process));
    new_process->id = id;
    new_process->command = strdup(command);
    new_process->priority = priority;
    new_process->completed = false;
    new_process->next = NULL;
    pthread_mutex_lock(&queue_lock);
    if (tail == NULL) {
        head = tail = new_process;
    } else {
        tail->next = new_process;
        tail = new_process;
    }
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_lock);
}

Process *dequeue_process() {
    pthread_mutex_lock(&queue_lock);
    while (head == NULL) {
        pthread_cond_wait(&queue_cond, &queue_lock);
    }
    Process *process = head;
    head = head->next;
    if (head == NULL) {
        tail = NULL;
    }
    pthread_mutex_unlock(&queue_lock);
    return process;
}

void *scheduler(void *arg) {
    while (1) {
        Process *process = dequeue_process();
        if (process->completed) {
            free(process->command);
            free(process);
            continue;
        }
        printf("Running process %d: %s\n", process->id, process->command);

        pid_t pid = fork();
        if (pid == 0) { // Child process
            execl("/bin/sh", "sh", "-c", process->command, (char *)NULL);
            perror("execl failed");
            exit(1);
        } else if (pid > 0) { // Parent process
            sleep(TIME_QUANTUM);
            kill(pid, SIGSTOP);
            int status;
            waitpid(pid, &status, WNOHANG);
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                process->completed = true;
            } else {
                enqueue_process(process->id, process->command, process->priority);
            }
        } else {
            perror("fork failed");
        }
        
        // Add delay between process executions
        sleep(DELAY_BETWEEN_PROCESSES);
    }
    return NULL;
}

void execute_command(char *command) {
    static int process_id = 1;
    enqueue_process(process_id++, command, 0); // Default priority is 0
}

void list_processes(bool detailed) {
    pthread_mutex_lock(&queue_lock);
    Process *current = head;
    printf("List of processes:\n");
    while (current != NULL) {
        if (detailed) {
            printf("Process ID: %d, Command: %s, Priority: %d, Completed: %s\n",
                   current->id, current->command, current->priority,
                   current->completed ? "Yes" : "No");
        } else {
            printf("Process ID: %d, Command: %s\n", current->id, current->command);
        }
        current = current->next;
    }
    pthread_mutex_unlock(&queue_lock);
}

void show_process_info(int process_id) {
    pthread_mutex_lock(&queue_lock);
    Process *current = head;
    while (current != NULL) {
        if (current->id == process_id) {
            printf("Process ID: %d, Command: %s, Priority: %d, Completed: %s\n",
                   current->id, current->command, current->priority,
                   current->completed ? "Yes" : "No");
            break;
        }
        current = current->next;
    }
    if (current == NULL) {
        printf("Process ID %d not found.\n", process_id);
    }
    pthread_mutex_unlock(&queue_lock);
}

void modify_process_priority(int process_id, int new_priority) {
    pthread_mutex_lock(&queue_lock);
    Process *current = head;
    while (current != NULL) {
        if (current->id == process_id) {
            current->priority = new_priority;
            printf("Process ID %d priority changed to %d\n", current->id, new_priority);
            break;
        }
        current = current->next;
    }
    if (current == NULL) {
        printf("Process ID %d not found.\n", process_id);
    }
    pthread_mutex_unlock(&queue_lock);
}

void *process_command_handler(void *arg) {
    char *line;
    while (1) {
        line = readline("Shell> ");
        if (line && *line) {
            add_history(line);
            if (strcmp(line, "quit") == 0) {
                free(line);
                break;
            } else if (strcmp(line, "procs") == 0) {
                list_processes(false);
            } else if (strcmp(line, "procs -a") == 0) {
                list_processes(true);
            } else if (strncmp(line, "info ", 5) == 0) {
                int process_id = atoi(line + 5);
                show_process_info(process_id);
            } else if (strncmp(line, "priority ", 9) == 0) {
                int process_id, new_priority;
                sscanf(line + 9, "%d %d", &process_id, &new_priority);
                modify_process_priority(process_id, new_priority);
            } else {
                execute_command(line);
            }
            free(line);
        }
    }
    return NULL;
}

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
        execute_command(line);
    }
    fclose(file);
}

void wait_for_all_processes() {
    while (1) {
        pthread_mutex_lock(&queue_lock);
        Process *current = head;
        bool all_completed = true;
        while (current != NULL) {
            if (!current->completed) {
                all_completed = false;
                break;
            }
            current = current->next;
        }
        pthread_mutex_unlock(&queue_lock);

        if (all_completed) {
            break;
        }

        sleep(1); // Check again after a short delay
    }
}

int main(int argc, char *argv[]) {
    pthread_t scheduler_thread, handler_thread;
    pthread_mutex_init(&queue_lock, NULL);
    pthread_cond_init(&queue_cond, NULL);

    pthread_create(&scheduler_thread, NULL, scheduler, NULL);
    if (argc == 2) {
        execute_batch_file(argv[1]);
        wait_for_all_processes();
    } else {
        pthread_create(&handler_thread, NULL, process_command_handler, NULL);
        pthread_join(handler_thread, NULL);
    }
    pthread_cancel(scheduler_thread);

    pthread_mutex_destroy(&queue_lock);
    pthread_cond_destroy(&queue_cond);

    printf("Exiting Shell...\n");
    return 0;
}
