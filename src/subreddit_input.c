#include "subreddit_input.h"
#include "glib.h"
#include "history.h"
#include <string.h>

char* sanitize_subreddit_name(const char* subreddit) {
    if (!subreddit || strlen(subreddit) == 0)
        return NULL;
    char* trimmed = g_strstrip(g_strdup(subreddit));
    GString* result = g_string_new(NULL);
    for (const char* cursor = trimmed; *cursor; ++cursor) {
        if (!g_ascii_isspace(*cursor)) {
            g_string_append_c(result, *cursor);
        }
    }
    g_free(trimmed);
    char* final = g_strdup(result->str);
    g_string_free(result, TRUE);
    if (strlen(final) == 0) {
        g_free(final);
        return NULL;
    }
    return final;
}

char* subreddit_from_history_or_input(struct subreddit_history* history, char** input, size_t selected_line) {
    char* history_entry = get_history_entry_for_line(history, selected_line);
    if (history_entry) {
        return g_strdup(history_entry);
    }
    return sanitize_subreddit_name(input ? *input : NULL);
}

void reset_rofi_input(char** input) {
    if (input && *input) {
        (*input)[0] = '\0';
    }
}
