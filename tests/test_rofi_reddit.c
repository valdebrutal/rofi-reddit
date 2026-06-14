#include "history.h"
#include "subreddit_input.h"
#include "unity.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static struct rofi_reddit_paths fake_paths;
static char history_path[sizeof("/tmp/rofi_reddit_input_history_test_XXXXXX")];

void setUp(void) {
    snprintf(history_path, sizeof(history_path), "/tmp/rofi_reddit_input_history_test_XXXXXX");
    int history_fd = mkstemp(history_path);
    TEST_ASSERT_NOT_EQUAL(-1, history_fd);
    (void)close(history_fd);

    fake_paths = (struct rofi_reddit_paths){
        .subreddit_history_path = history_path,
        .subreddit_history_path_exists = true,
    };
}

void tearDown(void) {
    remove(history_path);
}

void test_sanitize_subreddit_name_removes_whitespace(void) {
    char* sanitized = sanitize_subreddit_name("  foo bar\t\n");

    TEST_ASSERT_NOT_NULL(sanitized);
    TEST_ASSERT_EQUAL_STRING("foobar", sanitized);
    g_free(sanitized);
}

void test_sanitize_subreddit_name_returns_null_for_whitespace_only_input(void) {
    TEST_ASSERT_NULL(sanitize_subreddit_name("   \t\n  "));
}

void test_reset_rofi_input_clears_existing_input_buffer(void) {
    char input_buffer[] = "askreddit";
    char* input = input_buffer;

    reset_rofi_input(&input);

    TEST_ASSERT_EQUAL_STRING("", input_buffer);
}

void test_subreddit_from_history_or_input_returns_copy_that_survives_history_reorder(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);
    char input_buffer[] = "";
    char* input = input_buffer;

    add_history_entry("firstsubreddit", history);
    add_history_entry("secondsubreddit", history);
    add_history_entry("thirdsubreddit", history);

    char* subreddit = subreddit_from_history_or_input(history, &input, 3);
    TEST_ASSERT_EQUAL_STRING("firstsubreddit", subreddit);

    add_history_entry(subreddit, history);

    TEST_ASSERT_EQUAL_STRING("firstsubreddit", subreddit);
    TEST_ASSERT_EQUAL_STRING("firstsubreddit", history->entries[1].subreddit);
    TEST_ASSERT_EQUAL_STRING("thirdsubreddit", history->entries[2].subreddit);
    TEST_ASSERT_EQUAL_STRING("secondsubreddit", history->entries[3].subreddit);

    g_free(subreddit);
    free_subreddit_history(history);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sanitize_subreddit_name_removes_whitespace);
    RUN_TEST(test_sanitize_subreddit_name_returns_null_for_whitespace_only_input);
    RUN_TEST(test_reset_rofi_input_clears_existing_input_buffer);
    RUN_TEST(test_subreddit_from_history_or_input_returns_copy_that_survives_history_reorder);
    return UNITY_END();
}
