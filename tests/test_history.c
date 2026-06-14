#include "history.h"
#include "reddit.h"
#include "unity.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct rofi_reddit_paths fake_paths;
static char history_path[sizeof("/tmp/rofi_reddit_history_test_XXXXXX")];

void setUp(void) {
    snprintf(history_path, sizeof(history_path), "/tmp/rofi_reddit_history_test_XXXXXX");
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
    }
}

static void write_lines_to_file(const char* path, const char* const* lines, size_t count) {
    FILE* f = fopen(path, "w");
    TEST_ASSERT_NOT_NULL(f);
    for (size_t i = 0; i < count; ++i) {
        fprintf(f, "%s\n", lines[i]);
    }
    fclose(f);
}

static void assert_history_file_lines(const char* path, const char* const* lines, size_t count) {
    FILE* f = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(f);
    char line[MAX_HISTORY_ENTRY_LENGTH + 2] = {0};
    for (size_t i = 0; i < count; ++i) {
        TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), f));
        line[strcspn(line, "\n")] = '\0';
        TEST_ASSERT_EQUAL_STRING(lines[i], line);
    }
    TEST_ASSERT_NULL(fgets(line, sizeof(line), f));
    fclose(f);
}

void test_new_history_starts_with_placeholder_and_adds_first_entry(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);
    TEST_ASSERT_NOT_NULL(history);
    TEST_ASSERT_EQUAL(1, history->count); // Only "Add to history" entry
    TEST_ASSERT_EQUAL_STRING("Add to history", history->entries[0].subreddit);

    char subreddit[] = "testsubreddit";
    add_history_entry(subreddit, history);

    TEST_ASSERT_EQUAL(2, history->count);
    TEST_ASSERT_EQUAL_STRING("testsubreddit", history->entries[1].subreddit);
    free_subreddit_history(history);
}

void test_new_history_creates_missing_parent_directories(void) {
    char temp_dir_template[] = "/tmp/rofi_reddit_history_dir_test_XXXXXX";
    char* temp_dir = mkdtemp(temp_dir_template);
    TEST_ASSERT_NOT_NULL(temp_dir);

    char* rofi_dir = g_build_filename(temp_dir, "rofi", NULL);
    char* nested_history_path = g_build_filename(rofi_dir, "rofi_reddit_history", NULL);

    struct rofi_reddit_paths nested_paths = {
        .subreddit_history_path = nested_history_path,
        .subreddit_history_path_exists = false,
    };

    struct subreddit_history* history = new_subreddit_history(&nested_paths);

    TEST_ASSERT_NOT_NULL(history);
    TEST_ASSERT_EQUAL(1, history->count);
    TEST_ASSERT_EQUAL(0, access(nested_history_path, F_OK));

    free_subreddit_history(history);
    remove(nested_history_path);
    rmdir(rofi_dir);
    rmdir(temp_dir);
    g_free(nested_history_path);
    g_free(rofi_dir);
}

void test_add_entry_when_history_is_full(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    add_sequential_subreddits(history, 1, MAX_HISTORY_LENGTH);

    char subreddit[MAX_HISTORY_ENTRY_LENGTH];
    snprintf(subreddit, sizeof(subreddit), "subreddit%zu", (size_t)(MAX_HISTORY_LENGTH + 1));
    add_history_entry(subreddit, history);

    TEST_ASSERT_EQUAL(MAX_HISTORY_LENGTH + 1, history->count);
    assert_entries_in_reverse_order(history, MAX_HISTORY_LENGTH + 1, MAX_HISTORY_LENGTH);

    free_subreddit_history(history);
}

void test_add_entry_with_existing_history_and_new_entries(void) {
    size_t existing_count = 5;
    char existing_lines[5][MAX_HISTORY_ENTRY_LENGTH];
    const char* existing_history[5];
    for (size_t i = 0; i < existing_count; i++) {
        snprintf(existing_lines[i], sizeof(existing_lines[i]), "oldsubreddit%zu", i + 1);
        existing_history[i] = existing_lines[i];
    }
    write_lines_to_file(fake_paths.subreddit_history_path, existing_history, existing_count);

    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    size_t new_count = 3;
    char subreddit[MAX_HISTORY_ENTRY_LENGTH];
    for (size_t i = 1; i <= new_count; i++) {
        snprintf(subreddit, sizeof(subreddit), "newsubreddit%zu", i);
        add_history_entry(subreddit, history);
    }

    for (size_t i = 1; i <= new_count; i++) {
        snprintf(subreddit, sizeof(subreddit), "newsubreddit%zu", new_count - i + 1);
        TEST_ASSERT_EQUAL_STRING(subreddit, history->entries[i].subreddit);
    }
    for (size_t i = 1; i <= existing_count; i++) {
        snprintf(subreddit, sizeof(subreddit), "oldsubreddit%zu", existing_count - i + 1);
        TEST_ASSERT_EQUAL_STRING(subreddit, history->entries[new_count + i].subreddit);
    }

    free_subreddit_history(history);
}

void test_add_entry_with_max_length(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    char max_length_subreddit[MAX_HISTORY_ENTRY_LENGTH];
    memset(max_length_subreddit, 'a', MAX_HISTORY_ENTRY_LENGTH - 1);
    max_length_subreddit[MAX_HISTORY_ENTRY_LENGTH - 1] = '\0';

    add_history_entry(max_length_subreddit, history);

    TEST_ASSERT_EQUAL_STRING(max_length_subreddit, history->entries[1].subreddit);
    TEST_ASSERT_EQUAL(MAX_HISTORY_ENTRY_LENGTH - 1, strlen(history->entries[1].subreddit));

    free_subreddit_history(history);
}

void test_add_entry_longer_than_max_length(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    char long_subreddit[MAX_HISTORY_ENTRY_LENGTH + 10];
    memset(long_subreddit, 'b', MAX_HISTORY_ENTRY_LENGTH + 9);
    long_subreddit[MAX_HISTORY_ENTRY_LENGTH + 9] = '\0';

    add_history_entry(long_subreddit, history);

    for (int i = 0; i < MAX_HISTORY_ENTRY_LENGTH - 1; i++) {
        TEST_ASSERT_EQUAL('b', history->entries[1].subreddit[i]);
    }
    TEST_ASSERT_EQUAL('\0', history->entries[1].subreddit[MAX_HISTORY_ENTRY_LENGTH - 1]);
    free_subreddit_history(history);
}

void test_add_existing_entry_moves_it_to_front_without_duplication(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    add_history_entry("firstsubreddit", history);
    add_history_entry("secondsubreddit", history);
    add_history_entry("firstsubreddit", history);

    TEST_ASSERT_EQUAL(3, history->count);
    TEST_ASSERT_EQUAL_STRING("firstsubreddit", history->entries[1].subreddit);
    TEST_ASSERT_EQUAL_STRING("secondsubreddit", history->entries[2].subreddit);

    free_subreddit_history(history);
}

void test_reselecting_existing_history_entry_uses_original_value_after_reorder(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);
    char* existing_entry = NULL;

    add_history_entry("firstsubreddit", history);
    add_history_entry("secondsubreddit", history);
    add_history_entry("thirdsubreddit", history);

    existing_entry = get_history_entry_for_line(history, 3);
    TEST_ASSERT_EQUAL_STRING("firstsubreddit", existing_entry);

    add_history_entry(existing_entry, history);

    TEST_ASSERT_EQUAL(4, history->count);
    TEST_ASSERT_EQUAL_STRING("firstsubreddit", history->entries[1].subreddit);
    TEST_ASSERT_EQUAL_STRING("thirdsubreddit", history->entries[2].subreddit);
    TEST_ASSERT_EQUAL_STRING("secondsubreddit", history->entries[3].subreddit);

    free_subreddit_history(history);
}

void test_add_entry_persists_history_immediately(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);
    const char* expected_lines[] = {"firstsubreddit", "secondsubreddit"};

    add_history_entry("firstsubreddit", history);
    add_history_entry("secondsubreddit", history);

    assert_history_file_lines(fake_paths.subreddit_history_path, expected_lines, 2);

    free_subreddit_history(history);
}

void test_reordering_existing_entry_persists_history_immediately(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);
    const char* expected_lines[] = {"secondsubreddit", "thirdsubreddit", "firstsubreddit"};

    add_history_entry("firstsubreddit", history);
    add_history_entry("secondsubreddit", history);
    add_history_entry("thirdsubreddit", history);
    add_history_entry("firstsubreddit", history);

    assert_history_file_lines(fake_paths.subreddit_history_path, expected_lines, 3);

    free_subreddit_history(history);
}

void test_free_history_does_not_rewrite_file_without_changes(void) {
    size_t original_count = MAX_HISTORY_LENGTH + 2;
    char original_lines[MAX_HISTORY_LENGTH + 2][MAX_HISTORY_ENTRY_LENGTH];
    const char* expected_lines[MAX_HISTORY_LENGTH + 2];
    for (size_t i = 0; i < original_count; ++i) {
        snprintf(original_lines[i], sizeof(original_lines[i]), "rawsubreddit%zu", i + 1);
        expected_lines[i] = original_lines[i];
    }
    write_lines_to_file(fake_paths.subreddit_history_path, expected_lines, original_count);

    struct subreddit_history* history = new_subreddit_history(&fake_paths);
    TEST_ASSERT_EQUAL(MAX_HISTORY_LENGTH + 1, history->count);
    free_subreddit_history(history);

    assert_history_file_lines(fake_paths.subreddit_history_path, expected_lines, original_count);
}

void test_persisted_history_reloads_in_mru_order(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    add_history_entry("firstsubreddit", history);
    add_history_entry("secondsubreddit", history);
    free_subreddit_history(history);

    history = new_subreddit_history(&fake_paths);

    TEST_ASSERT_EQUAL(3, history->count);
    TEST_ASSERT_EQUAL_STRING("secondsubreddit", history->entries[1].subreddit);
    TEST_ASSERT_EQUAL_STRING("firstsubreddit", history->entries[2].subreddit);

    free_subreddit_history(history);
}

void test_get_history_entry_for_line_skips_placeholder(void) {
    struct subreddit_history* history = new_subreddit_history(&fake_paths);

    add_history_entry("firstsubreddit", history);
    add_history_entry("secondsubreddit", history);

    TEST_ASSERT_NULL(get_history_entry_for_line(history, 0));
    TEST_ASSERT_EQUAL_STRING("secondsubreddit", get_history_entry_for_line(history, 1));
    TEST_ASSERT_EQUAL_STRING("firstsubreddit", get_history_entry_for_line(history, 2));
    TEST_ASSERT_NULL(get_history_entry_for_line(history, history->count));

    free_subreddit_history(history);
}

void test_read_history_entries_empty_file(void) {
    FILE* f = fopen(fake_paths.subreddit_history_path, "w+");
    TEST_ASSERT_NOT_NULL(f);
    char buffer[MAX_HISTORY_ENTRY_LENGTH * 10] = {0};
    size_t read = read_history_entries(f, (struct history_entries_buffer){.entries = buffer, .entry_capacity = 10});
    TEST_ASSERT_EQUAL(0, read);
    fclose(f);
}

void test_read_history_entries_single_entry(void) {
    const char* lines[] = {"testsubreddit"};
    write_lines_to_file(fake_paths.subreddit_history_path, lines, 1);

    FILE* f = fopen(fake_paths.subreddit_history_path, "r");
    TEST_ASSERT_NOT_NULL(f);
    char buffer[MAX_HISTORY_ENTRY_LENGTH * 2] = {0};
    size_t read = read_history_entries(f, (struct history_entries_buffer){.entries = buffer, .entry_capacity = 2});
    TEST_ASSERT_EQUAL(1, read);
    TEST_ASSERT_EQUAL_STRING("testsubreddit", buffer);
    fclose(f);
}

void test_read_history_entries_multiple_entries(void) {
    const char* lines[] = {"foo", "bar", "baz"};
    write_lines_to_file(fake_paths.subreddit_history_path, lines, 3);

    FILE* f = fopen(fake_paths.subreddit_history_path, "r");
    TEST_ASSERT_NOT_NULL(f);
    char buffer[MAX_HISTORY_ENTRY_LENGTH * 4] = {0};
    size_t read = read_history_entries(f, (struct history_entries_buffer){.entries = buffer, .entry_capacity = 4});
    TEST_ASSERT_EQUAL(3, read);
    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_STRING(lines[i], buffer + i * MAX_HISTORY_ENTRY_LENGTH);
    }
    fclose(f);
}

void test_read_history_entries_keeps_newest_entries_when_buffer_is_full(void) {
    const char* lines[] = {"one", "two", "three", "four", "five"};
    const char* expected[] = {"three", "four", "five"};
    write_lines_to_file(fake_paths.subreddit_history_path, lines, 5);

    FILE* f = fopen(fake_paths.subreddit_history_path, "r");
    TEST_ASSERT_NOT_NULL(f);
    char buffer[MAX_HISTORY_ENTRY_LENGTH * 3] = {0};
    size_t read = read_history_entries(f, (struct history_entries_buffer){.entries = buffer, .entry_capacity = 3});

    TEST_ASSERT_EQUAL(3, read);
    for (size_t i = 0; i < 3; ++i) {
        TEST_ASSERT_EQUAL_STRING(expected[i], buffer + i * MAX_HISTORY_ENTRY_LENGTH);
    }
    fclose(f);
}

void test_read_history_entries_truncates_long_lines(void) {
    char long_line[MAX_HISTORY_ENTRY_LENGTH + 20];
    memset(long_line, 'x', sizeof(long_line) - 1);
    long_line[sizeof(long_line) - 1] = '\0';
    const char* lines[] = {long_line};
    write_lines_to_file(fake_paths.subreddit_history_path, lines, 1);

    FILE* f = fopen(fake_paths.subreddit_history_path, "r");
    TEST_ASSERT_NOT_NULL(f);
    char buffer[MAX_HISTORY_ENTRY_LENGTH * 3] = {0};
    size_t read = read_history_entries(f, (struct history_entries_buffer){.entries = buffer, .entry_capacity = 3});

    TEST_ASSERT_EQUAL(1, read);
    for (int i = 0; i < MAX_HISTORY_ENTRY_LENGTH - 1; ++i) {
        TEST_ASSERT_EQUAL('x', buffer[i]);
    }
    TEST_ASSERT_EQUAL('\0', buffer[MAX_HISTORY_ENTRY_LENGTH - 1]);
    for (int i = MAX_HISTORY_ENTRY_LENGTH; i < MAX_HISTORY_ENTRY_LENGTH * 3; ++i) {
        TEST_ASSERT_EQUAL(0, buffer[i]);
    }
    fclose(f);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_new_history_starts_with_placeholder_and_adds_first_entry);
    RUN_TEST(test_new_history_creates_missing_parent_directories);
    RUN_TEST(test_add_entry_when_history_is_full);
    RUN_TEST(test_add_entry_with_existing_history_and_new_entries);
    RUN_TEST(test_add_entry_with_max_length);
    RUN_TEST(test_add_entry_longer_than_max_length);
    RUN_TEST(test_add_existing_entry_moves_it_to_front_without_duplication);
    RUN_TEST(test_reselecting_existing_history_entry_uses_original_value_after_reorder);
    RUN_TEST(test_add_entry_persists_history_immediately);
    RUN_TEST(test_reordering_existing_entry_persists_history_immediately);
    RUN_TEST(test_free_history_does_not_rewrite_file_without_changes);
    RUN_TEST(test_persisted_history_reloads_in_mru_order);
    RUN_TEST(test_get_history_entry_for_line_skips_placeholder);
    RUN_TEST(test_read_history_entries_empty_file);
    RUN_TEST(test_read_history_entries_single_entry);
    RUN_TEST(test_read_history_entries_multiple_entries);
    RUN_TEST(test_read_history_entries_keeps_newest_entries_when_buffer_is_full);
    RUN_TEST(test_read_history_entries_truncates_long_lines);

    return UNITY_END();
}
