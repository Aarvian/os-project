#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

// Semaphores for synchronization
sem_t log_sem;
sem_t file_sem;

// Simple log function to write to logs/operations.log
void write_to_log(char *op, char *file, char *status) {
    sem_wait(&log_sem);
    
    FILE *f = fopen("logs/operations.log", "a");
    if (f != NULL) {
        fprintf(f, "[LOG] Operation: %s | Target: %s | Status: %s\n", op, file, status);
        fclose(f);
    }
    
    // Also print to console so we see it happening
    printf(">> Logged: %s on %s (%s)\n", op, file, status);
    
    sem_post(&log_sem);
}

// Thread function for reading
void *read_func(void *arg) {
    char *fname = (char *)arg;
    sem_wait(&file_sem);

    FILE *f = fopen(fname, "r");
    if (f == NULL) {
        write_to_log("READ", fname, "FAILED: File not found");
    } else {
        char line[100];
        if (fgets(line, sizeof(line), f)) {
            printf("Thread read: %s", line);
            write_to_log("READ", fname, "SUCCESS");
        } else {
            write_to_log("READ", fname, "FAILED: Empty");
        }
        fclose(f);
    }

    sem_post(&file_sem);
    free(fname); // Clean up the strdup memory
    return NULL;
}

// Thread function for writing
void *write_func(void *arg) {
    char *fname = (char *)arg;
    sem_wait(&file_sem);

    FILE *f = fopen(fname, "a");
    if (f == NULL) {
        write_to_log("WRITE", fname, "FAILED: Permission denied");
    } else {
        fprintf(f, "New entry from thread.\n");
        write_to_log("WRITE", fname, "SUCCESS");
        fclose(f);
    }

    sem_post(&file_sem);
    free(fname);
    return NULL;
}

int main() {
    int choice;
    char name_input[100];
    char path[150];
    pthread_t thread_id;

    sem_init(&log_sem, 0, 1);
    sem_init(&file_sem, 0, 1);

    printf("--- File Operation System ---\n");

    while (1) {
        printf("\n1. Read a file (from test_files/)\n");
        printf("2. Write to a file (in test_files/)\n");
        printf("3. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        if (choice == 3) break;

        printf("Enter filename (e.g., sample.txt): ");
        scanf("%s", name_input);

        // Combine folder name with user input
        sprintf(path, "test_files/%s", name_input);
        char *pass_path = strdup(path);

        if (choice == 1) {
            pthread_create(&thread_id, NULL, read_func, pass_path);
            pthread_join(thread_id, NULL);
        } else if (choice == 2) {
            pthread_create(&thread_id, NULL, write_func, pass_path);
            pthread_join(thread_id, NULL);
        } else {
            printf("Invalid option.\n");
            free(pass_path);
        }
    }

    sem_destroy(&log_sem);
    sem_destroy(&file_sem);
    printf("Goodbye!\n");

    return 0;
}
