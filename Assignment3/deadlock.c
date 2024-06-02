//Joseph Clauss
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_PROCESSES 5
#define RESOURCE_AVAILABLE 1
#define RESOURCE_UNAVAILABLE 0
#define TIMEOUT 5

typedef struct {
    int id;
    int resource_status;
    int timer;
} Process;

Process processes[NUM_PROCESSES];
pthread_mutex_t resource_mutex;
int running = 1; // Global variable to control the running state

void* process_function(void* arg) {
    Process* process = (Process*)arg;
    while (running) {
        pthread_mutex_lock(&resource_mutex);
        if (process->resource_status == RESOURCE_AVAILABLE) {
            printf("Process %d acquired the resource\n", process->id);
            process->resource_status = RESOURCE_UNAVAILABLE;
            sleep(rand() % 3 + 1); // Simulate resource usage
            process->resource_status = RESOURCE_AVAILABLE;
            printf("Process %d released the resource\n", process->id);
        } else {
            if (process->timer >= TIMEOUT) {
                printf("Process %d is terminated due to timeout\n", process->id);
                process->timer = 0;
            } else {
                process->timer++;
                printf("Process %d is waiting for the resource, timer: %d\n", process->id, process->timer);
            }
        }
        pthread_mutex_unlock(&resource_mutex);
        sleep(1); // Simulate some time before the next attempt
    }
    return NULL;
}

void quit_program() {
    running = 0; // Set running to 0 to stop the threads
}

int main() {
    pthread_t threads[NUM_PROCESSES];
    pthread_mutex_init(&resource_mutex, NULL);

    for (int i = 0; i < NUM_PROCESSES; i++) {
        processes[i].id = i + 1;
        processes[i].resource_status = RESOURCE_AVAILABLE;
        processes[i].timer = 0;
        pthread_create(&threads[i], NULL, process_function, (void*)&processes[i]);
    }

    printf("Press Enter to quit...\n");
    getchar(); // Wait for user input to quit the program
    quit_program(); // Call the function to set running to 0

    for (int i = 0; i < NUM_PROCESSES; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&resource_mutex);
    return 0;
}
