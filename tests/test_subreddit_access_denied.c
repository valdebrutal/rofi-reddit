#include "memory.h"
#include "reddit.h"
#include "unity.h"
#include <jansson.h>
#include <string.h>

void setUp(void) {
}

void tearDown(void) {
}

static struct reddit_api_response* make_response(enum http_status_code status, const char* json) {
    struct response_buffer* resp_buf = LOG_ERR_MALLOC(struct response_buffer, 1);
    resp_buf->buffer = strdup(json);
    resp_buf->size = strlen(resp_buf->buffer);

    struct reddit_api_response* response = LOG_ERR_MALLOC(struct reddit_api_response, 1);
    response->status_code = status;
    response->response_buffer = resp_buf;
    return response;
}

void test_happy_path(void) {
    struct {
        const char* json;
        enum http_status_code status;
        enum subreddit_access expected;
    } cases[] = {
        {"{\"reason\": \"private\"}", HTTP_FORBIDDEN, SUBREDDIT_ACCESS_PRIVATE},
        {"{\"reason\": \"quarantined\"}", HTTP_FORBIDDEN, SUBREDDIT_ACCESS_QUARANTINED},
        {"{\"reason\": \"unknown\"}", HTTP_FORBIDDEN, SUBREDDIT_ACCESS_UNKNOWN},
    };

    for (size_t i = 0; i < 3; ++i) {
        struct reddit_api_response* response = make_response(cases[i].status, cases[i].json);
        enum subreddit_access access = subreddit_access_denied_reason(response);
        TEST_ASSERT_EQUAL(cases[i].expected, access);
        free_reddit_api_response(response);
    }
}

void test_expired_token(void) {
    struct reddit_api_response* response = make_response(HTTP_UNAUTHORIZED, "{\"error\": \"invalid_token\"}");
    enum subreddit_access access = subreddit_access_denied_reason(response);
    TEST_ASSERT_EQUAL(SUBREDDIT_ACCESS_EXPIRED_TOKEN, access);
    free_reddit_api_response(response);
}

void test_unknown_access(void) {
    struct reddit_api_response* response =
        make_response(HTTP_FORBIDDEN, "{\"reason\": \"what is love? baby don't hurt me\"}");
    enum subreddit_access access = subreddit_access_denied_reason(response);
    TEST_ASSERT_EQUAL(SUBREDDIT_ACCESS_UNKNOWN, access);
    free_reddit_api_response(response);
}

void test_json_parse_failure(void) {
    struct reddit_api_response* response = make_response(HTTP_FORBIDDEN, "{invalid_json");
    enum subreddit_access access = subreddit_access_denied_reason(response);
    TEST_ASSERT_EQUAL(SUBREDDIT_ACCESS_EXPIRED_TOKEN, access); // fallback for parse error
    free_reddit_api_response(response);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_happy_path);
    RUN_TEST(test_expired_token);
    RUN_TEST(test_unknown_access);
    RUN_TEST(test_json_parse_failure);
    return UNITY_END();
}
