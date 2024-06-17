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
void edit_file(const char *path, const char *name, const char *new_content);
void move_file(const char *src_path, const char *file_name, const char *dest_path);
void duplicate_file(const char *path, const char *file_name, const char *new_name);
void duplicate_directory(const char *src_path, const char *dest_path);
void search_file(Directory *dir, const char *file_name);
void display_tree(Directory *dir, int level);
void get_file_info(const char *path, const char *name);
void get_file_detailed_info(const char *path, const char *name);
void get_directory_info(const char *path);
void get_directory_detailed_info(const char *path);

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
            } else if (strncmp(line, "edit ", 5) == 0) {
                char path[MAX_PATH_LEN], name[MAX_NAME_LEN], new_content[MAX_LINE];
                sscanf(line + 5, "%s %s %s", path, name, new_content);
                edit_file(path, name, new_content);
            } else if (strncmp(line, "mvfile ", 7) == 0) {
                char src_path[MAX_PATH_LEN], file_name[MAX_NAME_LEN], dest_path[MAX_PATH_LEN];
                sscanf(line + 7, "%s %s %s", src_path, file_name, dest_path);
                move_file(src_path, file_name, dest_path);
            } else if (strncmp(line, "cpfile ", 7) == 0) {
                char path[MAX_PATH_LEN], file_name[MAX_NAME_LEN], new_name[MAX_NAME_LEN];
                sscanf(line + 7, "%s %s %s", path, file_name, new_name);
                duplicate_file(path, file_name, new_name);
            } else if (strncmp(line, "cpdir ", 6) == 0) {
                char src_path[MAX_PATH_LEN], dest_path[MAX_PATH_LEN];
                sscanf(line + 6, "%s %s", src_path, dest_path);
                duplicate_directory(src_path, dest_path);
            } else if (strncmp(line, "search ", 7) == 0) {
                char path[MAX_PATH_LEN], file_name[MAX_NAME_LEN];
                sscanf(line + 7, "%s %s", path, file_name);
                Directory *dir = find_directory(root, path);
                if (dir) {
                    search_file(dir, file_name);
                } else {
                    printf("Directory not found: %s\n", path);
                }
            } else if (strncmp(line, "tree ", 5) == 0) {
                char path[MAX_PATH_LEN];
                sscanf(line + 5, "%s", path);
                Directory *dir = find_directory(root, path);
                if (dir) {
                    display_tree(dir, 0);
                } else {
                    printf("Directory not found: %s\n", path);
                }
            } else if (strncmp(line, "fileinfo ", 9) == 0) {
                char path[MAX_PATH_LEN], name[MAX_NAME_LEN];
                sscanf(line + 9, "%s %s", path, name);
                get_file_info(path, name);
            } else if (strncmp(line, "fileinfo -d ", 12) == 0) {
                char path[MAX_PATH_LEN], name[MAX_NAME_LEN];
                sscanf(line + 12, "%s %s", path, name);
                get_file_detailed_info(path, name);
            } else if (strncmp(line, "dirinfo ", 8) == 0) {
                char path[MAX_PATH_LEN];
                sscanf(line + 8, "%s", path);
                get_directory_info(path);
            } else if (strncmp(line, "dirinfo -d ", 11) == 0) {
                char path[MAX_PATH_LEN];
                sscanf(line + 11, "%s", path);
                get_directory_detailed_info(path);
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

// Function to edit a file (append content)
void edit_file(const char *path, const char *name, const char *new_content) {
    Directory *dir = find_directory(root, path);
    if (!dir) {
        printf("Directory not found: %s\n", path);
        return;
    }
    File *file = find_file(dir, name);
    if (!file) {
        printf("File not found: %s/%s\n", path, name);
        return;
    }
    // Simulate editing by just displaying a message
    printf("File edited: %s/%s, new content: %s\n", path, name, new_content);
}

// Function to move a file across directories
void move_file(const char *src_path, const char *file_name, const char *dest_path) {
    Directory *src_dir = find_directory(root, src_path);
    if (!src_dir) {
        printf("Source directory not found: %s\n", src_path);
        return;
    }
    Directory *dest_dir = find_directory(root, dest_path);
    if (!dest_dir) {
        printf("Destination directory not found: %s\n", dest_path);
        return;
    }
    File *file = find_file(src_dir, file_name);
    if (!file) {
        printf("File not found: %s/%s\n", src_path, file_name);
        return;
    }
    // Remove file from source directory
    if (src_dir->files == file) {
        src_dir->files = file->next;
    } else {
        File *prev = src_dir->files;
        while (prev->next && prev->next != file) {
            prev = prev->next;
        }
        if (prev->next == file) {
            prev->next = file->next;
        }
    }
    // Add file to destination directory
    file->next = dest_dir->files;
    dest_dir->files = file;
    snprintf(file->path, MAX_PATH_LEN, "%s/%s", dest_path, file_name);
    printf("File moved: %s/%s to %s/%s\n", src_path, file_name, dest_path, file_name);
}

// Function to duplicate a file
void duplicate_file(const char *path, const char *file_name, const char *new_name) {
    Directory *dir = find_directory(root, path);
    if (!dir) {
        printf("Directory not found: %s\n", path);
        return;
    }
    File *file = find_file(dir, file_name);
    if (!file) {
        printf("File not found: %s/%s\n", path, file_name);
        return;
    }
    create_file(path, new_name, file->size);
    printf("File duplicated: %s/%s to %s/%s\n", path, file_name, path, new_name);
}

// Function to duplicate a directory
void duplicate_directory(const char *src_path, const char *dest_path) {
    Directory *src_dir = find_directory(root, src_path);
    if (!src_dir) {
        printf("Source directory not found: %s\n", src_path);
        return;
    }
    Directory *dest_dir = find_directory(root, dest_path);
    if (!dest_dir) {
        printf("Destination directory not found: %s\n", dest_path);
        return;
    }
    // Recursive duplication
    Directory *new_dir = (Directory *)malloc(sizeof(Directory));
    strcpy(new_dir->name, src_dir->name);
    snprintf(new_dir->path, MAX_PATH_LEN, "%s/%s", dest_path, src_dir->name);
    new_dir->files = NULL;
    new_dir->subdirs = dest_dir->subdirs;
    dest_dir->subdirs = new_dir;
    printf("Directory duplicated: %s to %s/%s\n", src_path, dest_path, src_dir->name);

    File *file = src_dir->files;
    while (file) {
        create_file(new_dir->path, file->name, file->size);
        file = file->next;
    }
    Directory *subdir = src_dir->subdirs;
    while (subdir) {
        char new_subdir_path[MAX_PATH_LEN];
        snprintf(new_subdir_path, MAX_PATH_LEN, "%s/%s", new_dir->path, subdir->name);
        duplicate_directory(subdir->path, new_subdir_path);
        subdir = subdir->next;
    }
}

// Function to search for a file in a directory tree
void search_file(Directory *dir, const char *file_name) {
    File *file = dir->files;
    while (file) {
        if (strcmp(file->name, file_name) == 0) {
            printf("File found: %s/%s\n", dir->path, file->name);
        }
        file = file->next;
    }
    Directory *subdir = dir->subdirs;
    while (subdir) {
        search_file(subdir, file_name);
        subdir = subdir->next;
    }
}

// Function to display a directory tree given a starting node
void display_tree(Directory *dir, int level) {
    for (int i = 0; i < level; i++) {
        printf("  ");
    }
    printf("%s/\n", dir->name);
    File *file = dir->files;
    while (file) {
        for (int i = 0; i < level + 1; i++) {
            printf("  ");
        }
        printf("%s (%d bytes)\n", file->name, file->size);
        file = file->next;
    }
    Directory *subdir = dir->subdirs;
    while (subdir) {
        display_tree(subdir, level + 1);
        subdir = subdir->next;
    }
}

// Function to get basic information about a file
void get_file_info(const char *path, const char *name) {
    Directory *dir = find_directory(root, path);
    if (!dir) {
        printf("Directory not found: %s\n", path);
        return;
    }
    File *file = find_file(dir, name);
    if (!file) {
        printf("File not found: %s/%s\n", path, name);
        return;
    }
    printf("File: %s/%s\n", path, name);
    printf("Size: %d bytes\n", file->size);
}

// Function to get detailed information about a file
void get_file_detailed_info(const char *path, const char *name) {
    // Basic information as well as simulated additional details
    get_file_info(path, name);
    printf("Created: Unknown (simulation)\n");
    printf("Last modified: Unknown (simulation)\n");
}

// Function to get basic information about a directory
void get_directory_info(const char *path) {
    Directory *dir = find_directory(root, path);
    if (!dir) {
        printf("Directory not found: %s\n", path);
        return;
    }
    printf("Directory: %s\n", dir->path);
    printf("Subdirectories: ");
    Directory *subdir = dir->subdirs;
    while (subdir) {
        printf("%s ", subdir->name);
        subdir = subdir->next;
    }
    printf("\nFiles: ");
    File *file = dir->files;
    while (file) {
        printf("%s ", file->name);
        file = file->next;
    }
    printf("\n");
}

// Function to get detailed information about a directory
void get_directory_detailed_info(const char *path) {
    // Basic information as well as simulated additional details
    get_directory_info(path);
    printf("Created: Unknown (simulation)\n");
    printf("Last modified: Unknown (simulation)\n");
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
