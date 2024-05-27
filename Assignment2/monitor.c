// Joseph Clauss
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

int balance = 1000; // Shared bank account balance
pthread_mutex_t mutex;
pthread_cond_t cond;

void* deposit(void* amount) {
    int deposit_amount = *((int*)amount);
    pthread_mutex_lock(&mutex); // Acquire lock
    balance += deposit_amount;
    printf("Deposited %d, new balance: %d\n", deposit_amount, balance);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex); // Release lock
    return NULL;
}

void* withdraw(void* amount) {
    int withdraw_amount = *((int*)amount);
    pthread_mutex_lock(&mutex); // Acquire lock
    if (balance >= withdraw_amount) {
        balance -= withdraw_amount;
        printf("Withdrew %d, new balance: %d\n", withdraw_amount, balance);
    } else {
        printf("Insufficient funds for withdrawal of %d\n", withdraw_amount);
    }
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex); // Release lock
    return NULL;
}

int main() {
    pthread_t threads[4];
    int amounts[4] = {200, 150, 300, 100};

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);

    pthread_create(&threads[0], NULL, deposit, &amounts[0]);
    pthread_create(&threads[1], NULL, withdraw, &amounts[1]);
    pthread_create(&threads[2], NULL, deposit, &amounts[2]);
    pthread_create(&threads[3], NULL, withdraw, &amounts[3]);

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    printf("Final balance: %d\n", balance);
    return 0;
}
