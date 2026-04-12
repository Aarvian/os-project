# ──────────────────────────────────────────────────────────────
#  Makefile — Multithreaded File Management System (Features 3 & 4)
#  Group G11  |  Project 2
# ──────────────────────────────────────────────────────────────

CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g
TARGET  = file_manager
SRC     = file_manager.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) file_operations.log
	rm -f demo_delete*.txt demo_rename*.txt demo_renamed*.txt
