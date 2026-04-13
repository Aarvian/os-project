#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#define LOGFILE     "logs/operations.log"
#define TESTFILE    "test_files/sample.txt"
#define MAX_READERS 3

sem_t sem_log;    /* binary: one logger at a time          */
sem_t sem_write;  /* binary: exclusive write/delete access */
sem_t sem_read;   /* counting: up to MAX_READERS at once   */

/* Feature 8: Logging */
void log_op(const char *operation, const char *filename, const char *status) {
    sem_wait(&sem_log);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

    FILE *f = fopen(LOGFILE, "a");
    if (f) {
        fprintf(f, "[%s] OP=%-10s FILE=%-25s STATUS=%s\n",
                timestamp, operation, filename, status);
        fclose(f);
    } else {
        fprintf(stderr, "[LOG ERROR] Could not open log file: %s\n", strerror(errno));
    }

    printf("[LOG] %-10s | %-25s | %s\n", operation, filename, status);
    sem_post(&sem_log);
}

/* Feature 7: Error Handling */
void *thread_read_file(void *arg) {
    char *filename = (char *)arg;
    printf("\n[Thread] Trying to READ '%s'\n", filename);

    sem_wait(&sem_read);

    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        printf("[ERROR] Cannot open '%s' — %s\n", filename, strerror(errno));
        log_op("READ", filename, "FAILED - file not found");
        sem_post(&sem_read);
        return NULL;
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), f) == NULL) {
        printf("[ERROR] File '%s' is empty\n", filename);
        log_op("READ", filename, "FAILED - file empty");
        fclose(f);
        sem_post(&sem_read);
        return NULL;
    }

    printf("[Thread] Read from '%s': %s", filename, buffer);
    log_op("READ", filename, "SUCCESS");
    fclose(f);
    sem_post(&sem_read);
    return NULL;
}

void *thread_write_file(void *arg) {
    char *filename = (char *)arg;
    printf("\n[Thread] Trying to WRITE to '%s'\n", filename);

    sem_wait(&sem_write);

    FILE *f = fopen(filename, "a");
    if (f == NULL) {
        printf("[ERROR] Cannot open '%s' for writing — %s\n", filename, strerror(errno));
        log_op("WRITE", filename, "FAILED - cannot open");
        sem_post(&sem_write);
        return NULL;
    }

    fprintf(f, "Thread wrote this line.\n");
    printf("[Thread] Wrote to '%s' successfully\n", filename);
    log_op("WRITE", filename, "SUCCESS");
    fclose(f);
    sem_post(&sem_write);
    return NULL;
}

void *thread_delete_file(void *arg) {
    char *filename = (char *)arg;
    printf("\n[Thread] Trying to DELETE '%s'\n", filename);

    sem_wait(&sem_write);

    if (remove(filename) != 0) {
        printf("[ERROR] Cannot delete '%s' — %s\n", filename, strerror(errno));
        log_op("DELETE", filename, "FAILED - file not found");
        sem_post(&sem_write);
        return NULL;
    }

    printf("[Thread] Deleted '%s' successfully\n", filename);
    log_op("DELETE", filename, "SUCCESS");
    sem_post(&sem_write);
    return NULL;
}

int main() {
    pthread_t t1, t2, t3, t4;

    if (sem_init(&sem_log,   0, 1)           != 0 ||
        sem_init(&sem_write, 0, 1)           != 0 ||
        sem_init(&sem_read,  0, MAX_READERS) != 0) {
        fprintf(stderr, "[FATAL] Semaphore init failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    system("mkdir -p logs test_files");

    FILE *f = fopen(TESTFILE, "w");
    if (f) {
        fprintf(f, "Hello from the test file.\n");
        fclose(f);
    } else {
        fprintf(stderr, "[FATAL] Could not create test file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("======================================\n");
    printf("  Feature 7 : Error Handling\n");
    printf("  Feature 8 : Logging\n");
    printf("  Sync      : Semaphores (POSIX)\n");
    printf("======================================\n");

    printf("\n-- Test 1: Read an existing file --\n");
    pthread_create(&t1, NULL, thread_read_file, TESTFILE);
    pthread_join(t1, NULL);

    printf("\n-- Test 2: Read a missing file --\n");
    pthread_create(&t2, NULL, thread_read_file, "test_files/missing.txt");
    pthread_join(t2, NULL);

    printf("\n-- Test 3: Write to a file --\n");
    pthread_create(&t3, NULL, thread_write_file, TESTFILE);
    pthread_join(t3, NULL);

    printf("\n-- Test 4: Delete a missing file --\n");
    pthread_create(&t4, NULL, thread_delete_file, "test_files/ghost.txt");
    pthread_join(t4, NULL);

    sem_destroy(&sem_log);
    sem_destroy(&sem_write);
    sem_destroy(&sem_read);

    printf("\n======================================\n");
    printf("Done. Check logs/operations.log\n");
    printf("======================================\n");
    return EXIT_SUCCESS;
}