// Joseph Clauss
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define TIME_QUANTUM 2
#define DELAY_BETWEEN_PROCESSES 5
#define MAX_NAME_LEN 255
#define MAX_PATH_LEN 4096

typedef struct Process {
    int id;
    char *command;
    int priority;
    bool completed;
    struct Process *next;
} Process;

typedef struct File {
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    int size;
    struct File *next;
} File;

typedef struct Directory {
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    struct Directory *next;
    File *files;
    struct Directory *subdirs;
} Directory;

// Function Declarations
void enqueue_process(int id, char *command, int priority);
Process* dequeue_process();
void* scheduler(void *arg);
void execute_command(char *command);
void list_processes(bool detailed);
void show_process_info(int process_id);
void modify_process_priority(int process_id, int new_priority);
void* process_command_handler(void *arg);
void execute_batch_file(char *filename);
void wait_for_all_processes();
void init_fs();
Directory* find_directory(Directory *dir, const char *path);
File* find_file(Directory *dir, const char *name);
void create_directory(const char *path, const char *name);
void rename_directory(const char *path, const char *new_name);
void delete_directory(const char *path, int recursive);
void create_file(const char *path, const char *name, int size);
void delete_file(const char *path, const char *name);
void list_directory(const char *path);

Process *head = NULL;
Process *tail = NULL;
pthread_mutex_t queue_lock;
pthread_cond_t queue_cond;
Directory *root;

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

Process* dequeue_process() {
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

void* scheduler(void *arg) {
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

void* process_command_handler(void *arg) {
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
            } else if (strncmp(line, "mkdir ", 6) == 0) {
                char path[MAX_PATH_LEN], name[MAX_NAME_LEN];
                sscanf(line + 6, "%s %s", path, name);
                create_directory(path, name);
            } else if (strncmp(line, "rmdir ", 6) == 0) {
                char path[MAX_PATH_LEN];
                int recursive = 0;
                if (sscanf(line + 6, "-r %s", path) == 1) {
                    recursive = 1;
                } else {
                    sscanf(line + 6, "%s", path);
                }
                delete_directory(path, recursive);
            } else if (strncmp(line, "touch ", 6) == 0) {
                char path[MAX_PATH_LEN], name[MAX_NAME_LEN];
                int size;
                sscanf(line + 6, "%s %s %d", path, name, &size);
                create_file(path, name, size);
            } else if (strncmp(line, "rm ", 3) == 0) {
                char path[MAX_PATH_LEN], name[MAX_NAME_LEN];
                sscanf(line + 3, "%s %s", path, name);
                delete_file(path, name);
            } else if (strncmp(line, "ls ", 3) == 0) {
                char path[MAX_PATH_LEN];
                sscanf(line + 3, "%s", path);
                list_directory(path);
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

void init_fs() {
    root = (Directory *)malloc(sizeof(Directory));
    strcpy(root->name, "/");
    strcpy(root->path, "/");
    root->files = NULL;
    root->subdirs = NULL;
}

Directory* find_directory(Directory *dir, const char *path) {
    if (strcmp(dir->path, path) == 0) {
        return dir;
    }
    Directory *subdir = dir->subdirs;
    while (subdir) {
        Directory *found = find_directory(subdir, path);
        if (found) {
            return found;
        }
        subdir = subdir->next;
    }
    return NULL;
}

File* find_file(Directory *dir, const char *name) {
    File *file = dir->files;
    while (file) {
        if (strcmp(file->name, name) == 0) {
            return file;
        }
        file = file->next;
    }
    return NULL;
}

void create_directory(const char *path, const char *name) {
    Directory *parent = find_directory(root, path);
    if (!parent) {
        printf("Directory not found: %s\n", path);
        return;
    }
    Directory *new_dir = (Directory *)malloc(sizeof(Directory));
    strcpy(new_dir->name, name);
    snprintf(new_dir->path, MAX_PATH_LEN, "%s/%s", path, name);
    new_dir->files = NULL;
    new_dir->subdirs = parent->subdirs;
    parent->subdirs = new_dir;
    printf("Directory created: %s\n", new_dir->path);
}

void rename_directory(const char *path, const char *new_name) {
    Directory *dir = find_directory(root, path);
    if (!dir) {
        printf("Directory not found: %s\n", path);
        return;
    }
    char new_path[MAX_PATH_LEN];
    int len = snprintf(new_path, MAX_PATH_LEN, "%s/%s", dir->path, new_name);
    if (len >= MAX_PATH_LEN) {
        printf("Error: New path name is too long.\n");
        return;
    }
    strcpy(dir->name, new_name);
    strcpy(dir->path, new_path);
    printf("Directory renamed to: %s\n", dir->path);
}

void delete_directory(const char *path, int recursive) {
    Directory *parent = root;
    Directory *prev = NULL;
    Directory *dir = root->subdirs;
    while (dir) {
        if (strcmp(dir->path, path) == 0) {
            if (recursive || (!recursive && !dir->files && !dir->subdirs)) {
                if (prev) {
                    prev->next = dir->next;
                } else {
                    parent->subdirs = dir->next;
                }
                free(dir);
                printf("Directory deleted: %s\n", path);
                return;
            } else {
                printf("Directory not empty: %s\n", path);
                return;
            }
        }
        prev = dir;
        dir = dir->next;
    }
    printf("Directory not found: %s\n", path);
}

void create_file(const char *path, const char *name, int size) {
    Directory *dir = find_directory(root, path);
    if (!dir) {
        printf("Directory not found: %s\n", path);
        return;
    }
    File *new_file = (File *)malloc(sizeof(File));
    strcpy(new_file->name, name);
    snprintf(new_file->path, MAX_PATH_LEN, "%s/%s", path, name);
    new_file->size = size;
    new_file->next = dir->files;
    dir->files = new_file;
    printf("File created: %s (%d bytes)\n", new_file->path, size);
}

void delete_file(const char *path, const char *name) {
    Directory *dir = find_directory(root, path);
    if (!dir) {
        printf("Directory not found: %s\n", path);
        return;
    }
    File *prev = NULL;
    File *file = dir->files;
    while (file) {
        if (strcmp(file->name, name) == 0) {
            if (prev) {
                prev->next = file->next;
            } else {
                dir->files = file->next;
            }
            free(file);
            printf("File deleted: %s/%s\n", path, name);
            return;
        }
        prev = file;
        file = file->next;
    }
    printf("File not found: %s/%s\n", path, name);
}

void list_directory(const char *path) {
    Directory *dir = find_directory(root, path);
    if (!dir) {
        printf("Directory not found: %s\n", path);
        return;
    }
    printf("Directory: %s\n", dir->path);
    File *file = dir->files;
    while (file) {
        printf("  File: %s (%d bytes)\n", file->name, file->size);
        file = file->next;
    }
    Directory *subdir = dir->subdirs;
    while (subdir) {
        printf("  Directory: %s\n", subdir->name);
        subdir = subdir->next;
    }
}

int main(int argc, char *argv[]) {
    pthread_t scheduler_thread, handler_thread;
    pthread_mutex_init(&queue_lock, NULL);
    pthread_cond_init(&queue_cond, NULL);
    init_fs();

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
