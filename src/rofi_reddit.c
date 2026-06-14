#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "curl_wrappers.h"
#include "glib.h"
#include "history.h"
#include "reddit.h"
#include "subreddit_input.h"
#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/mode.h>

#include <stdint.h>

G_MODULE_EXPORT Mode mode;

typedef struct {
    RedditApp* app;
    RedditAccessToken* token;
    struct listings* listings;
    char* selected_subreddit;
    enum subreddit_access subreddit_access;
    struct subreddit_history* subreddit_history;
} RofiRedditModePrivateData;

static int rofi_reddit_mode_init(Mode* mode) {
    if (mode_get_private_data(mode) == NULL) {
        struct rofi_reddit_paths* paths = new_rofi_reddit_paths();
        struct rofi_reddit_cfg* config = new_rofi_reddit_cfg(paths);
        if (!config)
            exit(EXIT_FAILURE);
        RedditApp* app = new_reddit_app(config);
        RofiRedditModePrivateData* private_data = g_malloc0(sizeof(*private_data));
        mode_set_private_data(mode, (void*)private_data);
        private_data->app = app;
        RedditAccessToken* token = new_reddit_access_token(app);
        if (!token)
            exit(EXIT_FAILURE);
        private_data->token = token;
        private_data->subreddit_history = new_subreddit_history(paths);
        private_data->listings = NULL;
        private_data->selected_subreddit = NULL;
        private_data->subreddit_access = SUBREDDIT_ACCESS_UNINITIALIZED;
        fprintf(stdout, "Initialized Rofi Reddit Mode with app: %s\n", app->config->auth->client_name);
    }
    return TRUE;
}

static unsigned int rofi_reddit_mode_get_num_entries(const Mode* mode) {
    const RofiRedditModePrivateData* private_data = (const RofiRedditModePrivateData*)mode_get_private_data(mode);
    if (private_data->listings && private_data->listings->count > 0)
        return private_data->listings->count;
    if (private_data->subreddit_history)
        return private_data->subreddit_history->count;
    return 1;
}

static void set_selected_subreddit(RofiRedditModePrivateData* private_data, const char* subreddit) {
    g_free(private_data->selected_subreddit);
    private_data->selected_subreddit = subreddit ? g_strdup(subreddit) : NULL;
}

static void handle_forbidden_response(RofiRedditModePrivateData* pd, enum subreddit_access access) {
    switch (access) {
    case SUBREDDIT_ACCESS_EXPIRED_TOKEN:
        fprintf(stdout, "Access token is expired, attempting to refresh it.\n");
        free_reddit_access_token(pd->token);
        pd->token = NULL;
        size_t attempts = 0;
        while (!pd->token && attempts < 5) {
            pd->token = fetch_and_cache_token(pd->app);
            attempts++;
        }
        break;
    case SUBREDDIT_ACCESS_QUARANTINED:
    case SUBREDDIT_ACCESS_UNKNOWN:
    case SUBREDDIT_ACCESS_PRIVATE:
        pd->subreddit_access = access;
        break;
    default:
        break;
    }
}

static ModeMode rofi_reddit_mode_result(Mode* mode, int mretv, char** input, unsigned int selected_line) {
    ModeMode retv = MODE_EXIT;
    RofiRedditModePrivateData* private_data = (RofiRedditModePrivateData*)mode_get_private_data(mode);
    if (mretv & MENU_NEXT) {
        retv = NEXT_DIALOG;
    } else if (mretv & MENU_OK && (private_data->listings && private_data->listings->count > 0)) {
        char* url = private_data->listings->items[selected_line].url;
        char* cmdline = g_strdup_printf("xdg-open '%s'", url);
        g_spawn_command_line_async(cmdline, NULL);
        g_free(cmdline);
        return MODE_EXIT;
    } else if (mretv & MENU_PREVIOUS) {
        retv = PREVIOUS_DIALOG;
    } else if ((mretv & MENU_CUSTOM_INPUT || (mretv & MENU_OK))) {
        char* subreddit = subreddit_from_history_or_input(
            private_data->listings ? NULL : private_data->subreddit_history, input, selected_line);
        if (!subreddit || strlen(subreddit) == 0) {
            g_free(subreddit);
            reset_rofi_input(input);
            private_data->subreddit_access = SUBREDDIT_ACCESS_UNINITIALIZED;
            return RELOAD_DIALOG;
        }
        set_selected_subreddit(private_data, subreddit);
        add_history_entry(private_data->selected_subreddit, private_data->subreddit_history);
        g_free(subreddit);
        free_listings(private_data->listings);
        private_data->listings = NULL;
        struct reddit_api_response* response =
            fetch_hot_listings(private_data->app, private_data->token, private_data->selected_subreddit);
        switch (response->status_code) {
        case HTTP_OK:
            private_data->listings = deserialize_listings(response->response_buffer);
            private_data->subreddit_access = SUBREDDIT_ACCESS_OK;
            if (private_data->listings->count == 0) {
                private_data->subreddit_access = SUBREDDIT_ACCESS_NO_RESULTS;
            }
            break;
        case HTTP_UNAUTHORIZED:
        case HTTP_FORBIDDEN: {
            enum subreddit_access denied_reason = subreddit_access_denied_reason(response);
            handle_forbidden_response(private_data, denied_reason);
            if (denied_reason == SUBREDDIT_ACCESS_EXPIRED_TOKEN) {
                free_reddit_api_response(response);
                response = fetch_hot_listings(private_data->app, private_data->token, private_data->selected_subreddit);
                if (response->status_code == HTTP_OK) {
                    private_data->listings = deserialize_listings(response->response_buffer);
                    private_data->subreddit_access =
                        private_data->listings->count == 0 ? SUBREDDIT_ACCESS_NO_RESULTS : SUBREDDIT_ACCESS_OK;
                } else if (response->status_code == HTTP_NOT_FOUND) {
                    private_data->subreddit_access = SUBREDDIT_ACCESS_DOESNT_EXIST;
                }
            }
            break;
        }
        case HTTP_NOT_FOUND:
            private_data->subreddit_access = SUBREDDIT_ACCESS_DOESNT_EXIST;
        default:
            break;
        }
        retv = RELOAD_DIALOG;
        free_reddit_api_response(response);
    }
    return retv;
}

static void rofi_reddit_mode_destroy(Mode* mode) {
    RofiRedditModePrivateData* private_data = (RofiRedditModePrivateData*)mode_get_private_data(mode);
    if (private_data != NULL) {
        fprintf(stdout, "Destroying Rofi Reddit Mode.\n");
        free_reddit_access_token(private_data->token);
        free_reddit_app(private_data->app);
        free_listings(private_data->listings);
        g_free(private_data->selected_subreddit);
        free_subreddit_history(private_data->subreddit_history);
        g_free(private_data);
        mode_set_private_data(mode, NULL);
    }
}

static char* get_display_value(const Mode* mode, unsigned int selected_line, G_GNUC_UNUSED int* state,
                               G_GNUC_UNUSED GList** attr_list, int get_entry) {
    RofiRedditModePrivateData* private_data = (RofiRedditModePrivateData*)mode_get_private_data(mode);
    if (!private_data->listings || private_data->listings->count == 0) {
        if (!private_data->subreddit_history) {
            return g_strdup("Add to history");
        }
        if (selected_line >= private_data->subreddit_history->count) {
            fprintf(stderr, "Selected line out of range.\n");
            return NULL;
        }
        return g_strdup_printf("%s", private_data->subreddit_history->entries[selected_line].subreddit);
    }
    if (selected_line >= private_data->listings->count) {
        fprintf(stderr, "Selected line out of range.\n");
        return NULL;
    }
    return g_strdup_printf("%s", private_data->listings->items[selected_line].title);
}

static int rofi_reddit_token_match(const Mode* sw, rofi_int_matcher** tokens, unsigned int index) {
    return true;
}

static char* get_message(const Mode* mode) {
    RofiRedditModePrivateData* private_data = (RofiRedditModePrivateData*)mode_get_private_data(mode);
    switch (private_data->subreddit_access) {
    case SUBREDDIT_ACCESS_UNINITIALIZED:
        return g_strdup("Type a subreddit to fetch threads for!");
    case SUBREDDIT_ACCESS_NO_RESULTS:
        return g_strdup_printf("No threads available for subreddit %s. Type another subreddit to fetch threads for!",
                               private_data->selected_subreddit);
    case SUBREDDIT_ACCESS_OK:
        return g_strdup_printf("Found %zu threads for subreddit %s. Now select a thread to open in your browser!",
                               private_data->listings->count, private_data->selected_subreddit);
    case SUBREDDIT_ACCESS_DOESNT_EXIST:
        return g_strdup_printf("Subreddit %s does not exist. Please try another one.",
                               private_data->selected_subreddit);
    case SUBREDDIT_ACCESS_PRIVATE:
        return g_strdup("This subreddit is private. You do not have access.");
    case SUBREDDIT_ACCESS_QUARANTINED:
        return g_strdup("This subreddit is quarantined. Can't fetch threads.");
    case SUBREDDIT_ACCESS_UNKNOWN:
        return g_strdup("Unknown access status for subreddit. Can't fetch threads.");
    default:
        return g_strdup("An unknown error occurred. Please try again.");
    }
}

Mode mode = {
    .abi_version = ABI_VERSION,
    .name = "reddit",
    .cfg_name_key = "display-rofi-reddit",
    ._init = rofi_reddit_mode_init,
    ._get_num_entries = rofi_reddit_mode_get_num_entries,
    ._result = rofi_reddit_mode_result,
    ._destroy = rofi_reddit_mode_destroy,
    ._token_match = rofi_reddit_token_match,
    ._get_display_value = get_display_value,
    ._get_message = get_message,
    ._get_completion = NULL,
    ._preprocess_input = NULL,
    .private_data = NULL,
    .free = NULL,
};
