#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define MAX_READERS 5


char shared_data[256] = "";

sem_t write_sem;
sem_t mutex;
int reader_count = 0;


void *reader(void *arg) {
    int id = *(int *)arg;

    
    sem_wait(&mutex);          
    reader_count++;
    if (reader_count == 1)
        sem_wait(&write_sem);  
    sem_post(&mutex);          

   
    printf("Reader %d: reading -> \"%s\"\n", id, shared_data);
    sleep(1);

    
    sem_wait(&mutex);          
    reader_count--;
    if (reader_count == 0)
        sem_post(&write_sem);  
    sem_post(&mutex);          

    printf("Reader %d: done\n", id);
    return NULL;
}



void *writer(void *arg) {
    char *content = (char *)arg;

    printf("Writer: waiting for exclusive access...\n");
    sem_wait(&write_sem);      

    printf("Writer: writing \"%s\"\n", content);
    strncpy(shared_data, content, sizeof(shared_data) - 1);
    sleep(1);

    sem_post(&write_sem);      
    printf("Writer: done\n");
    return NULL;
}



int main() {
    int choice;
    char input[256];

  
    sem_init(&write_sem, 0, 1);  
    sem_init(&mutex,     0, 1);  

    // printf("=== Multithreaded File Management System ===\n");
    // printf("    Semaphore-based | Concurrent Read & Exclusive Write\n\n");

    while (1) {
        // printf("----------------------------------\n");
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
