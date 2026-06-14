#include "history.h"
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>

static const size_t MAX_READABLE_HISTORY_ENTRIES = 10000;
static const int HISTORY_DIRECTORY_MODE = 0700;

static void set_history_entry_addition(struct subreddit_history* history) {
    strncpy(history->entries[0].subreddit, "Add to history", MAX_HISTORY_ENTRY_LENGTH - 1);
    history->entries[0].subreddit[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
}

static int ensure_history_parent_directory(const char* history_path) {
    if (!history_path)
        return EINVAL;

    char* parent_dir = g_path_get_dirname(history_path);
    int mkdir_result = g_mkdir_with_parents(parent_dir, HISTORY_DIRECTORY_MODE);
    if (mkdir_result != 0) {
        fprintf(stderr, "Failed to create subreddit history directory %s: %s\n", parent_dir, g_strerror(errno));
    }
    g_free(parent_dir);
    return mkdir_result;
}

static void persist_subreddit_history(struct subreddit_history* history) {
    if (!history || !history->history_file || history->count == 0)
        return;
    FILE* reopened_history = freopen(NULL, "w+", history->history_file);
    if (!reopened_history) {
        fprintf(stderr, "Failed to reopen subreddit history file for writing.\n");
        history->history_file = NULL;
        return;
    }
    history->history_file = reopened_history;
    for (size_t i = history->count - 1; i > 0; i--) {
        fprintf(history->history_file, "%s\n", history->entries[i].subreddit);
    }
    fflush(history->history_file);
}

static struct subreddit_history* allocate_subreddit_history(FILE* history_file) {
    struct subreddit_history* history = g_new0(struct subreddit_history, 1);
    history->capacity = MAX_HISTORY_LENGTH + 1; // +1 for "Add to history" entry
    history->entries = g_new0(struct history_entry, MAX_HISTORY_LENGTH + 1);
    history->history_file = history_file;
    history->count = 1; // Only "Add to history"
    set_history_entry_addition(history);
    return history;
}

size_t read_history_entries(FILE* history_file, struct history_entries_buffer output) {
    if (!history_file || !output.entries || output.entry_capacity == 0)
        return 0;

    size_t history_records_read = 0;
    size_t history_records_stored = 0;
    char entry_buffer[MAX_HISTORY_ENTRY_LENGTH];
    while (fgets(entry_buffer, sizeof(entry_buffer), history_file)) {
        int entry_was_truncated =
            strchr(entry_buffer, '\n') == NULL && strlen(entry_buffer) == sizeof(entry_buffer) - 1;
        entry_buffer[strcspn(entry_buffer, "\n")] = '\0';
        size_t offset = (history_records_read % output.entry_capacity) * MAX_HISTORY_ENTRY_LENGTH;
        strncpy(output.entries + offset, entry_buffer, MAX_HISTORY_ENTRY_LENGTH - 1);
        output.entries[offset + MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
        history_records_read++;
        if (history_records_stored < output.entry_capacity) {
            history_records_stored++;
        }
        if (entry_was_truncated) {
            int next_char = 0;
            while ((next_char = fgetc(history_file)) != '\n' && next_char != EOF) {}
        }
    }
    if (history_records_read > output.entry_capacity) {
        char* ordered_entries = g_new(char, (history_records_stored * MAX_HISTORY_ENTRY_LENGTH));
        size_t first_entry = history_records_read % output.entry_capacity;
        for (size_t i = 0; i < history_records_stored; ++i) {
            size_t entry_index = (first_entry + i) % output.entry_capacity;
            memcpy(ordered_entries + (i * MAX_HISTORY_ENTRY_LENGTH),
                   output.entries + (entry_index * MAX_HISTORY_ENTRY_LENGTH), MAX_HISTORY_ENTRY_LENGTH);
        }
        memcpy(output.entries, ordered_entries, history_records_stored * MAX_HISTORY_ENTRY_LENGTH);
        g_free(ordered_entries);
    }
    return history_records_stored;
}

static void fill_history_entries(struct subreddit_history* history, char* buffer, size_t history_records_read) {
    if (!history || !buffer || history_records_read == 0) {
        if (history) {
            history->count = 1;
        }
        return;
    }

    size_t entries_to_copy = history_records_read < MAX_HISTORY_LENGTH ? history_records_read : MAX_HISTORY_LENGTH;
    char* read_entries_index = buffer + ((history_records_read - 1) * MAX_HISTORY_ENTRY_LENGTH);
    for (size_t i = 0; i < entries_to_copy; ++i) {
        strncpy(history->entries[i + 1].subreddit, read_entries_index, MAX_HISTORY_ENTRY_LENGTH - 1);
        history->entries[i + 1].subreddit[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
        read_entries_index -= MAX_HISTORY_ENTRY_LENGTH;
    }
    history->count = entries_to_copy + 1; // +1 for "Add to history"
}

struct subreddit_history* new_subreddit_history(struct rofi_reddit_paths* paths) {
    if (!paths || !paths->subreddit_history_path)
        return NULL;

    if (ensure_history_parent_directory(paths->subreddit_history_path) != 0)
        return NULL;

    FILE* history_file = fopen(paths->subreddit_history_path, "a+");
    if (!history_file) {
        fprintf(stderr, "Failed to open subreddit history file for reading and writing: %s\n",
                paths->subreddit_history_path);
        return NULL;
    }
    fseek(history_file, 0, SEEK_SET);
    char* history_entries_buffer = g_new0(char, (MAX_READABLE_HISTORY_ENTRIES * MAX_HISTORY_ENTRY_LENGTH));
    struct history_entries_buffer output = {
        .entries = history_entries_buffer,
        .entry_capacity = MAX_READABLE_HISTORY_ENTRIES,
    };
    size_t history_records_read = read_history_entries(history_file, output);

    struct subreddit_history* history = allocate_subreddit_history(history_file);

    if (history_records_read == 0) {
        history->count = 1; // Only "Add to history"
        g_free(history_entries_buffer);
        return history;
    }
    fill_history_entries(history, history_entries_buffer, history_records_read);
    g_free(history_entries_buffer);
    return history;
}

void free_subreddit_history(struct subreddit_history* history) {
    if (!history)
        return;
    g_free(history->entries);
    if (history->history_file) {
        fclose(history->history_file);
    }
    g_free(history);
}

void add_history_entry(char* subreddit, struct subreddit_history* history) {
    if (!history || !subreddit)
        return;

    char subreddit_copy[MAX_HISTORY_ENTRY_LENGTH];
    struct history_entry entry;
    strncpy(subreddit_copy, subreddit, MAX_HISTORY_ENTRY_LENGTH - 1);
    subreddit_copy[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';

    for (size_t index = 1; index < history->count; index++) {
        entry = history->entries[index];
        if (strcmp(entry.subreddit, subreddit_copy) == 0) {
            for (size_t j = index; j > 1; j--) {
                history->entries[j] = history->entries[j - 1];
            }
            strncpy(history->entries[1].subreddit, subreddit_copy, MAX_HISTORY_ENTRY_LENGTH - 1);
            history->entries[1].subreddit[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
            persist_subreddit_history(history);
            return;
        }
    }
    if (history->count == history->capacity) {
        for (size_t i = history->count - 1; i > 1; i--) {
            history->entries[i] = history->entries[i - 1];
        }
    } else {
        for (size_t i = history->count; i > 1; i--) {
            history->entries[i] = history->entries[i - 1];
        }
        history->count++;
    }
    strncpy(history->entries[1].subreddit, subreddit_copy, MAX_HISTORY_ENTRY_LENGTH - 1);
    history->entries[1].subreddit[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';
    persist_subreddit_history(history);
}

char* get_history_entry_for_line(struct subreddit_history* history, size_t selected_line) {
    if (!history || selected_line == 0 || selected_line >= history->count) {
        return NULL;
    }
    return history->entries[selected_line].subreddit;
}
