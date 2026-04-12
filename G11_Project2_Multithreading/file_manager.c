#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#define MAX_PATH      256
#define LOG_FILE      "file_operations.log"
#define MAX_THREADS   16

static pthread_rwlock_t  file_rwlock  = PTHREAD_RWLOCK_INITIALIZER;
static pthread_mutex_t   log_mutex    = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t   sig_mutex    = PTHREAD_MUTEX_INITIALIZER;
static int               completed_ops = 0;

static void sigusr1_handler(int sig) {
    (void)sig;
    pthread_mutex_lock(&sig_mutex);
    completed_ops++;
    pthread_mutex_unlock(&sig_mutex);
}

static void log_operation(const char *operation, const char *target,
                          const char *status, const char *detail)
{
    pthread_mutex_lock(&log_mutex);

    FILE *logfp = fopen(LOG_FILE, "a");
    if (logfp) {
        time_t     now = time(NULL);
        struct tm *tm  = localtime(&now);
        char       ts[64];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

        fprintf(logfp, "[%s] TID=%-6ld  %-10s  %-8s  target=\"%s\"",
                ts, (long)pthread_self(), operation, status, target);
        if (detail && detail[0])
            fprintf(logfp, "  detail=\"%s\"", detail);
        fprintf(logfp, "\n");
        fclose(logfp);
    }

    pthread_mutex_unlock(&log_mutex);
}

static void print_metadata(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S",
                 localtime(&st.st_mtime));
        printf("    size       : %ld bytes\n", (long)st.st_size);
        printf("    modified   : %s\n", timebuf);
        printf("    permissions: %o\n", st.st_mode & 0777);
    }
}

typedef struct {
    char filepath[MAX_PATH];
} delete_args_t;

typedef struct {
    char old_path[MAX_PATH];
    char new_path[MAX_PATH];
} rename_args_t;

static void *delete_file_thread(void *arg)
{
    delete_args_t *da = (delete_args_t *)arg;
    const char *path  = da->filepath;

    printf("\n[DELETE] Thread %ld: attempting to delete \"%s\"\n",
           (long)pthread_self(), path);

    pthread_rwlock_wrlock(&file_rwlock);
    printf("[DELETE] Thread %ld: acquired write-lock\n",
           (long)pthread_self());

    struct stat st;
    if (stat(path, &st) == 0) {
        printf("[DELETE] File metadata before deletion:\n");
        print_metadata(path);
    }

    if (remove(path) == 0) {
        printf("[DELETE] Thread %ld: \"%s\" deleted successfully\n",
               (long)pthread_self(), path);
        log_operation("DELETE", path, "SUCCESS", NULL);
    } else {
        fprintf(stderr, "[DELETE] Thread %ld: failed to delete \"%s\" - %s\n",
                (long)pthread_self(), path, strerror(errno));
        log_operation("DELETE", path, "FAIL", strerror(errno));
    }

    pthread_rwlock_unlock(&file_rwlock);
    printf("[DELETE] Thread %ld: released write-lock\n",
           (long)pthread_self());

    kill(getpid(), SIGUSR1);

    free(da);
    return NULL;
}

static void *rename_file_thread(void *arg)
{
    rename_args_t *ra    = (rename_args_t *)arg;
    const char *old_path = ra->old_path;
    const char *new_path = ra->new_path;

    printf("\n[RENAME] Thread %ld: attempting to rename \"%s\" -> \"%s\"\n",
           (long)pthread_self(), old_path, new_path);

    pthread_rwlock_wrlock(&file_rwlock);
    printf("[RENAME] Thread %ld: acquired write-lock\n",
           (long)pthread_self());

    struct stat st;
    if (stat(old_path, &st) == 0) {
        printf("[RENAME] File metadata before rename:\n");
        print_metadata(old_path);
    }

    if (access(old_path, F_OK) != 0) {
        fprintf(stderr, "[RENAME] Thread %ld: source \"%s\" does not exist - %s\n",
                (long)pthread_self(), old_path, strerror(errno));
        log_operation("RENAME", old_path, "FAIL", "source not found");
        pthread_rwlock_unlock(&file_rwlock);
        kill(getpid(), SIGUSR1);
        free(ra);
        return NULL;
    }

    if (access(new_path, F_OK) == 0) {
        fprintf(stderr,
                "[RENAME] Thread %ld: destination \"%s\" already exists. "
                "Overwriting.\n",
                (long)pthread_self(), new_path);
        log_operation("RENAME", new_path, "WARN", "destination overwritten");
    }

    if (rename(old_path, new_path) == 0) {
        printf("[RENAME] Thread %ld: \"%s\" -> \"%s\" renamed successfully\n",
               (long)pthread_self(), old_path, new_path);
        log_operation("RENAME", old_path, "SUCCESS", new_path);

        printf("[RENAME] File metadata after rename:\n");
        print_metadata(new_path);
    } else {
        fprintf(stderr, "[RENAME] Thread %ld: failed - %s\n",
                (long)pthread_self(), strerror(errno));
        log_operation("RENAME", old_path, "FAIL", strerror(errno));
    }

    pthread_rwlock_unlock(&file_rwlock);
    printf("[RENAME] Thread %ld: released write-lock\n",
           (long)pthread_self());

    kill(getpid(), SIGUSR1);

    free(ra);
    return NULL;
}

static int create_sample_file(const char *path, const char *contents)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "[SETUP] Cannot create \"%s\" - %s\n",
                path, strerror(errno));
        return -1;
    }
    fprintf(fp, "%s", contents);
    fclose(fp);
    printf("[SETUP] Created sample file \"%s\"\n", path);
    return 0;
}

static void print_menu(void)
{
    printf("\n");
    printf("Multithreaded File Manager\n");
    printf("--------------------------\n");
    printf("  1. Delete a file   (Feature 3)\n");
    printf("  2. Rename a file   (Feature 4)\n");
    printf("  3. Run automated demo\n");
    printf("  4. Show operation log\n");
    printf("  5. Exit\n");
    printf("  Choice: ");
}

static void run_demo(void)
{
    printf("\n========== AUTOMATED DEMO ==========\n");

    pthread_mutex_lock(&sig_mutex);
    completed_ops = 0;
    pthread_mutex_unlock(&sig_mutex);

    create_sample_file("demo_delete1.txt", "This file will be deleted.\n");
    create_sample_file("demo_delete2.txt", "This file will also be deleted.\n");
    create_sample_file("demo_rename1.txt", "This file will be renamed.\n");
    create_sample_file("demo_rename2.txt", "This file will also be renamed.\n");

    printf("\n--- Launching 4 concurrent threads ---\n");

    pthread_t threads[4];
    int thread_count = 0;

    {
        delete_args_t *da = malloc(sizeof(delete_args_t));
        strncpy(da->filepath, "demo_delete1.txt", MAX_PATH);
        pthread_create(&threads[thread_count++], NULL, delete_file_thread, da);
    }

    {
        delete_args_t *da = malloc(sizeof(delete_args_t));
        strncpy(da->filepath, "demo_delete2.txt", MAX_PATH);
        pthread_create(&threads[thread_count++], NULL, delete_file_thread, da);
    }

    {
        rename_args_t *ra = malloc(sizeof(rename_args_t));
        strncpy(ra->old_path, "demo_rename1.txt", MAX_PATH);
        strncpy(ra->new_path, "demo_renamed_A.txt", MAX_PATH);
        pthread_create(&threads[thread_count++], NULL, rename_file_thread, ra);
    }

    {
        rename_args_t *ra = malloc(sizeof(rename_args_t));
        strncpy(ra->old_path, "demo_rename2.txt", MAX_PATH);
        strncpy(ra->new_path, "demo_renamed_B.txt", MAX_PATH);
        pthread_create(&threads[thread_count++], NULL, rename_file_thread, ra);
    }

    for (int i = 0; i < thread_count; i++)
        pthread_join(threads[i], NULL);

    pthread_mutex_lock(&sig_mutex);
    printf("\n--- SIGUSR1 received %d time(s) (expected %d) ---\n",
           completed_ops, thread_count);
    pthread_mutex_unlock(&sig_mutex);

    remove("demo_renamed_A.txt");
    remove("demo_renamed_B.txt");

    printf("\n========== DEMO COMPLETE ==========\n");
}

static void show_log(void)
{
    printf("\n--- Operation Log (%s) ---\n", LOG_FILE);
    FILE *fp = fopen(LOG_FILE, "r");
    if (!fp) {
        printf("(no log file yet)\n");
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp))
        printf("  %s", line);
    fclose(fp);
    printf("--- End of Log ---\n");
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    printf("Multithreaded File Management System\n");
    printf("Features 3 (Delete) & 4 (Rename)\n\n");

    int running = 1;
    while (running) {
        print_menu();

        int choice;
        if (scanf("%d", &choice) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        getchar();

        switch (choice) {
        case 1: {
            char path[MAX_PATH];
            printf("  Enter file path to delete: ");
            if (!fgets(path, sizeof(path), stdin)) break;
            path[strcspn(path, "\n")] = '\0';

            if (access(path, F_OK) != 0) {
                fprintf(stderr, "  Error: \"%s\" does not exist.\n", path);
                break;
            }

            delete_args_t *da = malloc(sizeof(delete_args_t));
            strncpy(da->filepath, path, MAX_PATH);

            pthread_t tid;
            pthread_create(&tid, NULL, delete_file_thread, da);
            pthread_join(tid, NULL);
            break;
        }

        case 2: {
            char old_path[MAX_PATH], new_path[MAX_PATH];
            printf("  Enter current file path: ");
            if (!fgets(old_path, sizeof(old_path), stdin)) break;
            old_path[strcspn(old_path, "\n")] = '\0';

            printf("  Enter new file name/path: ");
            if (!fgets(new_path, sizeof(new_path), stdin)) break;
            new_path[strcspn(new_path, "\n")] = '\0';

            if (access(old_path, F_OK) != 0) {
                fprintf(stderr, "  Error: \"%s\" does not exist.\n", old_path);
                break;
            }

            rename_args_t *ra = malloc(sizeof(rename_args_t));
            strncpy(ra->old_path, old_path, MAX_PATH);
            strncpy(ra->new_path, new_path, MAX_PATH);

            pthread_t tid;
            pthread_create(&tid, NULL, rename_file_thread, ra);
            pthread_join(tid, NULL);
            break;
        }

        case 3:
            run_demo();
            break;

        case 4:
            show_log();
            break;

        case 5:
            printf("\nGoodbye!\n");
            running = 0;
            break;

        default:
            printf("  Invalid choice. Try again.\n");
        }
    }

    pthread_rwlock_destroy(&file_rwlock);
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&sig_mutex);

    return EXIT_SUCCESS;
}
