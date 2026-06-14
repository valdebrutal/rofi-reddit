#include "reddit.h"
#include <stddef.h>

#ifndef _HISTORY_H
#define _HISTORY_H

#define MAX_HISTORY_LENGTH 20
#define MAX_HISTORY_ENTRY_LENGTH 256

struct history_entry {
    char subreddit[MAX_HISTORY_ENTRY_LENGTH];
};

struct subreddit_history {
    size_t count;
    size_t capacity;
    struct history_entry* entries;
    FILE* history_file;
};

struct history_entries_buffer {
    char* entries;
    size_t entry_capacity;
};

struct subreddit_history* new_subreddit_history(struct rofi_reddit_paths* paths);
void free_subreddit_history(struct subreddit_history* history);
void add_history_entry(char* subreddit, struct subreddit_history* history);
char* get_history_entry_for_line(struct subreddit_history* history, size_t selected_line);

size_t read_history_entries(FILE* history_file, struct history_entries_buffer output);
#endif
