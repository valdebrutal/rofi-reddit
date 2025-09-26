#include "history.h"
#include "reddit.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

static struct rofi_reddit_paths fake_paths;

void setUp(void) {
    fake_paths = (struct rofi_reddit_paths){
        .subreddit_history_path = "/tmp/foobar_reddit_history.txt",
        .subreddit_history_path_exists = true,
    };
}

void tearDown(void) {
    remove(fake_paths.subreddit_history_path);
}

static void add_sequential_subreddits(struct subreddit_history* history, size_t start, size_t end) {
    char subreddit[MAX_HISTORY_ENTRY_LENGTH];
    for (size_t i = start; i <= end; i++) {
        snprintf(subreddit, sizeof(subreddit), "subreddit%zu", i);
        add_history_entry(subreddit, history);
    }
}

static void assert_entries_in_reverse_order(struct subreddit_history* history, size_t start_num, size_t count) {
    char subreddit[MAX_HISTORY_ENTRY_LENGTH];
    for (size_t i = 1; i <= count; i++) {
        size_t expected_num = start_num - i + 1;
        snprintf(subreddit, sizeof(subreddit), "subreddit%zu", expected_num);
        TEST_ASSERT_EQUAL_STRING(subreddit, history->entries[i].subreddit);
        TEST_ASSERT_TRUE(history->entries[i].is_new);
    }
}

void test_add_entry_when_history_is_empty(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);
    TEST_ASSERT_NOT_NULL(history);
    TEST_ASSERT_EQUAL(1, history->count); // Only "Add to history" entry

    char subreddit[] = "testsubreddit";
    add_history_entry(subreddit, history);

    TEST_ASSERT_EQUAL(2, history->count);
    TEST_ASSERT_EQUAL_STRING("testsubreddit", history->entries[1].subreddit);
    TEST_ASSERT_TRUE(history->entries[1].is_new);

    free_subreddit_history(history);
}

void test_add_entry_when_history_is_full(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    add_sequential_subreddits(history, 1, MAX_HISTORY_LENGTH);

    // add one more to trigger eviction
    char subreddit[MAX_HISTORY_ENTRY_LENGTH];
    snprintf(subreddit, sizeof(subreddit), "subreddit%zu", (size_t)(MAX_HISTORY_LENGTH + 1));
    add_history_entry(subreddit, history);

    TEST_ASSERT_EQUAL(MAX_HISTORY_LENGTH + 1, history->count);
    assert_entries_in_reverse_order(history, MAX_HISTORY_LENGTH + 1, MAX_HISTORY_LENGTH);

    free_subreddit_history(history);
}

void test_add_entry_when_history_has_less_entries_than_capacity(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    size_t half_capacity = (MAX_HISTORY_LENGTH + 1) / 2;
    add_sequential_subreddits(history, 1, half_capacity);

    // Check count (+1 for "Add to history")
    TEST_ASSERT_EQUAL(half_capacity + 1, history->count);

    assert_entries_in_reverse_order(history, half_capacity, half_capacity);
    free_subreddit_history(history);
}

void test_add_entry_with_existing_history_and_new_entries(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    // Simulate existing history loaded from file (is_new = false)
    size_t existing_count = 5;
    for (size_t i = 0; i < existing_count; i++) {
        snprintf(history->entries[i + 1].subreddit, MAX_HISTORY_ENTRY_LENGTH, "oldsubreddit%zu", existing_count - i);
        history->entries[i + 1].is_new = false;
    }
    history->count = existing_count + 1; // +1 for "Add to history"

    // Add new entries (is_new = true)
    size_t new_count = 3;
    char subreddit[MAX_HISTORY_ENTRY_LENGTH];
    for (size_t i = 1; i <= new_count; i++) {
        snprintf(subreddit, sizeof(subreddit), "newsubreddit%zu", i);
        add_history_entry(subreddit, history);
    }

    // Check order: new entries first, then old entries
    for (size_t i = 1; i <= new_count; i++) {
        snprintf(subreddit, sizeof(subreddit), "newsubreddit%zu", new_count - i + 1);
        TEST_ASSERT_EQUAL_STRING(subreddit, history->entries[i].subreddit);
        TEST_ASSERT_TRUE(history->entries[i].is_new);
    }
    for (size_t i = 1; i <= existing_count; i++) {
        snprintf(subreddit, sizeof(subreddit), "oldsubreddit%zu", existing_count - i + 1);
        TEST_ASSERT_EQUAL_STRING(subreddit, history->entries[new_count + i].subreddit);
        TEST_ASSERT_FALSE(history->entries[new_count + i].is_new);
    }

    free_subreddit_history(history);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_add_entry_when_history_is_empty);
    RUN_TEST(test_add_entry_when_history_is_full);
    RUN_TEST(test_add_entry_when_history_has_less_entries_than_capacity);
    RUN_TEST(test_add_entry_with_existing_history_and_new_entries);
    return UNITY_END();
}
