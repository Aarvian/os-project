#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_READERS 5

/* Shared data */
char shared_data[256] = "";

/*
 * Semaphores:
 * write_sem  - controls exclusive writing (init to 1, only 1 thread allowed)
 * mutex      - protects the reader_count variable
 * reader_count - tracks how many readers are currently reading
 */
sem_t write_sem;
sem_t mutex;
int reader_count = 0;

/* ── Feature 1: Concurrent Reading ─────────────────────────────────── */

void *reader(void *arg) {
    int id = *(int *)arg;

    /* --- Entry section --- */
    sem_wait(&mutex);          /* lock reader_count */
    reader_count++;
    if (reader_count == 1)
        sem_wait(&write_sem);  /* first reader blocks writers */
    sem_post(&mutex);          /* unlock reader_count */

    /* --- Read --- */
    printf("Reader %d: reading -> \"%s\"\n", id, shared_data);
    sleep(1);

    /* --- Exit section --- */
    sem_wait(&mutex);          /* lock reader_count */
    reader_count--;
    if (reader_count == 0)
        sem_post(&write_sem);  /* last reader unblocks writers */
    sem_post(&mutex);          /* unlock reader_count */

    printf("Reader %d: done\n", id);
    return NULL;
}

/* ── Feature 2: Exclusive Writing ──────────────────────────────────── */

void *writer(void *arg) {
    char *content = (char *)arg;

    printf("Writer: waiting for exclusive access...\n");
    sem_wait(&write_sem);      /* block everyone else */

    printf("Writer: writing \"%s\"\n", content);
    strncpy(shared_data, content, sizeof(shared_data) - 1);
    sleep(1);

    sem_post(&write_sem);      /* release */
    printf("Writer: done\n");
    return NULL;
}

/* ── main ───────────────────────────────────────────────────────────── */

int main() {
    int choice;
    char input[256];

    /* init semaphores */
    sem_init(&write_sem, 0, 1);  /* 1 = one writer at a time */
    sem_init(&mutex,     0, 1);  /* 1 = protect reader_count */

    printf("=== Multithreaded File Management System ===\n");
    printf("    Semaphore-based | Concurrent Read & Exclusive Write\n\n");

    while (1) {
        printf("----------------------------------\n");
        printf("1. Write data (exclusive)\n");
        printf("2. Read data  (concurrent)\n");
        printf("3. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar();

        if (choice == 1) {
            pthread_t w;
            printf("Enter data to write: ");
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = '\0';

            pthread_create(&w, NULL, writer, input);
            pthread_join(w, NULL);

        } else if (choice == 2) {
            int n;
            printf("How many readers? (max %d): ", MAX_READERS);
            scanf("%d", &n);
            if (n < 1 || n > MAX_READERS) {
                printf("Invalid number.\n");
                continue;
            }

            pthread_t readers[MAX_READERS];
            int ids[MAX_READERS];

            for (int i = 0; i < n; i++) {
                ids[i] = i + 1;
                pthread_create(&readers[i], NULL, reader, &ids[i]);
            }
            for (int i = 0; i < n; i++)
                pthread_join(readers[i], NULL);

        } else if (choice == 3) {
            printf("Exiting.\n");
            break;
        } else {
            printf("Invalid choice.\n");
        }
    }

    sem_destroy(&write_sem);
    sem_destroy(&mutex);
    return 0;
}
