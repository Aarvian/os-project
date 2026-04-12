# Multithreaded File Management System — Features 3 & 4

**Author:** Aravind Linga | **Adm No:** 24JE0645 | **Group:** G11

---

## What This Project Does

This is a C program that performs **file deletion** and **file renaming** using **multiple threads** (multithreading). Instead of doing operations one after another in the main program, each operation runs in its own separate thread — meaning multiple file operations can be requested at the same time.

The key challenge with multithreading is: **what happens if two threads try to modify the same file at the same time?** This can cause data corruption. To solve this, we use **synchronization** — locking mechanisms that ensure only one thread can modify files at a time.

---

## My Contribution

| Feature | What It Does |
|---|---|
| **Feature 3 — File Deletion** | User gives a file path → a new thread is created → the thread safely deletes the file |
| **Feature 4 — File Renaming** | User gives old name and new name → a new thread is created → the thread safely renames the file |

---

## How Synchronization Works

The main problem: if Thread A is deleting a file and Thread B tries to rename the same file at the same time, the program will crash or produce wrong results.

**Solution: Readers-Writer Lock (`pthread_rwlock_t`)**

- Before deleting or renaming, a thread must first **acquire a write lock**
- If another thread already holds the lock, the new thread **waits** until the lock is released
- This guarantees **only one thread modifies files at a time**
- After the operation is done, the thread **releases the lock** so others can proceed

```
Thread 1 (DELETE):   [--- acquire lock ---][--- delete file ---][--- release lock ---]
Thread 2 (RENAME):                          [waiting...]        [--- acquire lock ---][--- rename file ---][--- release lock ---]
```

**Other synchronization used:**
- `pthread_mutex_t` — a simpler lock used to protect the log file (so two threads don't write to the log at the same time)
- `SIGUSR1` signal — each thread sends this signal when it finishes, so the main program knows all operations are done

---

## How Each Feature Works

### Feature 3 — File Deletion (step by step)

1. User enters the path of the file to delete
2. Program checks if the file exists
3. A **new thread** is created using `pthread_create()`
4. Inside the thread:
   - Acquires the **write lock** (waits if another thread is working)
   - Prints file info (size, date, permissions)
   - Deletes the file using `remove()`
   - Logs the result to `file_operations.log`
   - Releases the write lock
   - Sends `SIGUSR1` signal to confirm completion
5. Main thread waits for the worker thread to finish using `pthread_join()`

### Feature 4 — File Renaming (step by step)

1. User enters the current file path and the new name
2. Program checks if the source file exists
3. A **new thread** is created using `pthread_create()`
4. Inside the thread:
   - Acquires the **write lock**
   - Checks if source file exists
   - Warns if the destination name already exists (it will be overwritten)
   - Renames the file using `rename()`
   - Logs the result to `file_operations.log`
   - Releases the write lock
   - Sends `SIGUSR1` signal to confirm completion
5. Main thread waits for the worker thread to finish

---

## Error Handling

| Error | How It's Handled |
|---|---|
| File doesn't exist | Checked before creating the thread; error message shown |
| Permission denied | Caught from `remove()`/`rename()` return value; logged with error details |
| Destination already exists (rename) | Warning printed; operation continues (overwrites) |
| Invalid menu input | Input is flushed; user is asked to re-enter |

---

## Build & Run

```bash
# Compile (requires Linux or WSL with gcc)
gcc -Wall -Wextra -pthread -g -o file_manager file_manager.c

# Run
./file_manager
```

The menu gives 5 options:
- **1** — Delete a file (you enter the path)
- **2** — Rename a file (you enter old and new path)
- **3** — Run automated demo (creates test files and runs 4 threads at once)
- **4** — Show the audit log
- **5** — Exit

---

## Execution Output (Automated Demo)

The demo creates 4 test files and launches **4 threads simultaneously** — 2 for deletion, 2 for renaming:

```
Multithreaded File Management System
Features 3 (Delete) & 4 (Rename)

========== AUTOMATED DEMO ==========
[SETUP] Created sample file "demo_delete1.txt"
[SETUP] Created sample file "demo_delete2.txt"
[SETUP] Created sample file "demo_rename1.txt"
[SETUP] Created sample file "demo_rename2.txt"

--- Launching 4 concurrent threads ---

[DELETE] Thread 124729130219072: attempting to delete "demo_delete1.txt"
[DELETE] Thread 124729130219072: acquired write-lock
[DELETE] Thread 124729121826368: attempting to delete "demo_delete2.txt"
[RENAME] Thread 124729113433664: attempting to rename "demo_rename1.txt" → "demo_renamed_A.txt"
[RENAME] Thread 124729105040960: attempting to rename "demo_rename2.txt" → "demo_renamed_B.txt"

[DELETE] File metadata before deletion:
    ├─ size       : 27 bytes
    ├─ modified   : 2026-04-12 14:50:37
    └─ permissions: 777
[DELETE] Thread 124729130219072: "demo_delete1.txt" deleted successfully ✓
[DELETE] Thread 124729130219072: released write-lock

[DELETE] Thread 124729121826368: acquired write-lock
[DELETE] Thread 124729121826368: "demo_delete2.txt" deleted successfully ✓
[DELETE] Thread 124729121826368: released write-lock

[RENAME] Thread 124729113433664: acquired write-lock
[RENAME] Thread 124729113433664: "demo_rename1.txt" → "demo_renamed_A.txt" renamed successfully ✓
[RENAME] Thread 124729113433664: released write-lock

[RENAME] Thread 124729105040960: acquired write-lock
[RENAME] Thread 124729105040960: "demo_rename2.txt" → "demo_renamed_B.txt" renamed successfully ✓
[RENAME] Thread 124729105040960: released write-lock

--- SIGUSR1 received 4 time(s) (expected 4) ---

========== DEMO COMPLETE ==========
```

**What the output shows:**
- All 4 threads start at the same time (see the "attempting to..." messages)
- But only one thread holds the lock at a time (they take turns — this is synchronization working)
- Each thread prints ✓ on success
- All 4 SIGUSR1 signals were received, confirming every thread completed

---

## Audit Log (`file_operations.log`)

Every operation is automatically logged with timestamp and thread ID:

```
[2026-04-12 14:50:37] TID=124729130219072  DELETE  SUCCESS  target="demo_delete1.txt"
[2026-04-12 14:50:37] TID=124729121826368  DELETE  SUCCESS  target="demo_delete2.txt"
[2026-04-12 14:50:37] TID=124729113433664  RENAME  SUCCESS  target="demo_rename1.txt"  detail="demo_renamed_A.txt"
[2026-04-12 14:50:37] TID=124729105040960  RENAME  SUCCESS  target="demo_rename2.txt"  detail="demo_renamed_B.txt"
```
