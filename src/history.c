#include "history.h"
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t MAX_READABLE_HISTORY_ENTRIES = 10000;

static void set_history_entry_addition(struct subreddit_history* history) {
    strncpy(history->entries[0].subreddit, "Add to history", MAX_HISTORY_ENTRY_LENGTH - 1);
    history->entries[0].subreddit[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
    history->entries[0].is_new = false;
}

struct subreddit_history* allocate_subreddit_history(FILE* history_file) {
    struct subreddit_history* history = LOG_ERR_MALLOC(struct subreddit_history, 1);
    history->capacity = MAX_HISTORY_LENGTH + 1; // +1 for "Add to history" entry
    history->entries = LOG_ERR_MALLOC(struct history_entry, MAX_HISTORY_LENGTH + 1);
    history->history_file = history_file;
    set_history_entry_addition(history);
    return history;
}

size_t read_history_entries(FILE* history_file, char* buffer) {
    size_t history_records_read = 0;
    char entry_buffer[MAX_HISTORY_ENTRY_LENGTH];
    while (fgets(entry_buffer, sizeof(entry_buffer), history_file)) {
        entry_buffer[strcspn(entry_buffer, "\n")] = '\0';
        size_t offset = history_records_read * MAX_HISTORY_ENTRY_LENGTH;
        // Copy at most MAX_HISTORY_ENTRY_LENGTH - 1 chars, always null-terminate
        strncpy(buffer + offset, entry_buffer, MAX_HISTORY_ENTRY_LENGTH - 1);
        buffer[offset + MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
        fprintf(stdout, "History Entry: %s\n", buffer + offset);
        history_records_read++;
        // Discard the rest of the line in the history file if it was too long
        if (strlen(entry_buffer) == MAX_HISTORY_ENTRY_LENGTH - 1 &&
            entry_buffer[MAX_HISTORY_ENTRY_LENGTH - 2] != '\0' && entry_buffer[MAX_HISTORY_ENTRY_LENGTH - 2] != '\n') {
            int c;
            while ((c = fgetc(history_file)) != '\n' && c != EOF) {}
        }
    }
    fprintf(stdout, "History entries read: %zu\n", history_records_read);
    return history_records_read;
}

void fill_history_entries(struct subreddit_history* history, char* buffer, size_t history_records_read) {
    size_t entries_to_copy = history_records_read < MAX_HISTORY_LENGTH ? history_records_read : MAX_HISTORY_LENGTH;
    char* read_entries_index = buffer + ((history_records_read - 1) * MAX_HISTORY_ENTRY_LENGTH);
    for (size_t i = 0; i < entries_to_copy; ++i) {
        strncpy(history->entries[i + 1].subreddit, read_entries_index, MAX_HISTORY_ENTRY_LENGTH - 1);
        history->entries[i + 1].subreddit[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
        history->entries[i + 1].is_new = false;
        read_entries_index -= MAX_HISTORY_ENTRY_LENGTH;
    }
    history->count = entries_to_copy + 1; // +1 for "Add to history"
}

struct subreddit_history* new_subreddit_history(struct rofi_reddit_paths* paths) {
    FILE* history_file = fopen(paths->subreddit_history_path, "a+");
    if (!history_file) {
        fprintf(stderr, "Failed to open subreddit history file for reading and writting: %s\n",
                paths->subreddit_history_path);
        return NULL;
    }
    char* history_entries_buffer = LOG_ERR_MALLOC(char, MAX_READABLE_HISTORY_ENTRIES* MAX_HISTORY_ENTRY_LENGTH);
    size_t history_records_read = 0;

    history_records_read = read_history_entries(history_file, history_entries_buffer);

    struct subreddit_history* history = allocate_subreddit_history(history_file);

    if (history_records_read == 0) {
        history->count = 1; // Only "Add to history"
        fseek(history_file, 0, SEEK_END);
        free(history_entries_buffer);
        fprintf(stdout, "No history entries kept\n");
        return history;
    }
    fill_history_entries(history, history_entries_buffer, history_records_read);
    free(history_entries_buffer);

    fprintf(stdout, "History entries kept: %zu\n", history->count - 1);
    fseek(history_file, 0, SEEK_END);
    return history;
}

void free_subreddit_history(struct subreddit_history* history) {
    if (!history)
        return;
    fseek(history->history_file, 0, SEEK_END);
    for (size_t i = history->count - 1; i > 0; i--) {
        struct history_entry entry = history->entries[i];
        if (entry.is_new) { // persist new entries
            fprintf(history->history_file, "%s\n", entry.subreddit);
        }
    }
    free(history->entries);
    fflush(history->history_file);
    fclose(history->history_file);
    free(history);
}

void add_history_entry(char* subreddit, struct subreddit_history* history) {
    // Evict oldest if at capacity
    if (history->count == history->capacity) {
        // Shift all entries except [0] ("Add to history") down by one
        for (size_t i = history->count - 1; i > 1; i--) {
            history->entries[i] = history->entries[i - 1];
        }
    } else {
        // Shift all entries except [0] down by one
        for (size_t i = history->count; i > 1; i--) {
            history->entries[i] = history->entries[i - 1];
        }
        history->count++;
    }
    strncpy(history->entries[1].subreddit, subreddit, MAX_HISTORY_ENTRY_LENGTH - 1);
    history->entries[1].subreddit[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
    history->entries[1].is_new = true;
}
