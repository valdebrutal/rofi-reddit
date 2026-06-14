#ifndef _SUBREDDIT_INPUT_H
#define _SUBREDDIT_INPUT_H

#include <stddef.h>

struct subreddit_history;

char* sanitize_subreddit_name(const char* subreddit);
char* subreddit_from_history_or_input(struct subreddit_history* history, char** input, size_t selected_line);
void reset_rofi_input(char** input);

#endif
