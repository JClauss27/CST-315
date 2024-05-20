//Joseph Clauss
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Buffer and associated variables
int buffer;
int is_buffer_full = 0; // Buffer status flag

// Function prototypes
void *producer(void *param);
void *consumer(void *param);

// Produce function
int produce() {
    static int product = 0;
    return product++;
}

// Consume function
void consume(int i) {
    printf("Consumed: %d\n", i);
}

// Put function (produces a product and puts it in the buffer)
void put(int i) {
    while (is_buffer_full) {
        // Busy wait if buffer is full
    }
    buffer = i;
    is_buffer_full = 1;
}

// Get function (gets a product from the buffer and consumes it)
int get() {
    while (!is_buffer_full) {
        // Busy wait if buffer is empty
    }
    int temp = buffer;
    is_buffer_full = 0;
    return temp;
}

// Producer thread function
void *producer(void *param) {
    while (1) {
        int item = produce();
        put(item);
        sleep(1); // Sleep to simulate time taken to produce an item
    }
}

// Consumer thread function
void *consumer(void *param) {
    while (1) {
        int item = get();
        consume(item);
        sleep(1); // Sleep to simulate time taken to consume an item
    }
}

int main() {
    pthread_t producer_thread, consumer_thread;

    // Create the producer thread
    pthread_create(&producer_thread, NULL, producer, NULL);

    // Create the consumer thread
    pthread_create(&consumer_thread, NULL, consumer, NULL);

    // Join the threads (this will wait for them to finish, which they won't in this infinite loop case)
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    return 0;
}
