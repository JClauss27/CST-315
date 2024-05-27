// Joseph Clauss
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

int balance = 1000; // Shared bank account balance
sem_t semaphore;

void* deposit(void* amount) {
    int deposit_amount = *((int*)amount);
    sem_wait(&semaphore); // Acquire semaphore
    balance += deposit_amount;
    printf("Deposited %d, new balance: %d\n", deposit_amount, balance);
    sem_post(&semaphore); // Release semaphore
    return NULL;
}

void* withdraw(void* amount) {
    int withdraw_amount = *((int*)amount);
    sem_wait(&semaphore); // Acquire semaphore
    if (balance >= withdraw_amount) {
        balance -= withdraw_amount;
        printf("Withdrew %d, new balance: %d\n", withdraw_amount, balance);
    } else {
        printf("Insufficient funds for withdrawal of %d\n", withdraw_amount);
    }
    sem_post(&semaphore); // Release semaphore
    return NULL;
}

int main() {
    pthread_t threads[4];
    int amounts[4] = {200, 150, 300, 100};

    sem_init(&semaphore, 0, 1); // Initialize semaphore

    pthread_create(&threads[0], NULL, deposit, &amounts[0]);
    pthread_create(&threads[1], NULL, withdraw, &amounts[1]);
    pthread_create(&threads[2], NULL, deposit, &amounts[2]);
    pthread_create(&threads[3], NULL, withdraw, &amounts[3]);

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&semaphore); // Destroy semaphore
    printf("Final balance: %d\n", balance);
    return 0;
}
