#include "curl/mock_easy.h"
#include "fixtures.h"
#include "mock_curl_wrappers.h"
#include "reddit.h"
#include "unity.h"
#include <glib.h>

static RedditApp* app;
static struct response_buffer* some_response;

static void assert_access_token_request(CURL* client, const struct access_token_request* request, int cmock_num_calls) {
    TEST_ASSERT_EQUAL(0, cmock_num_calls);
    TEST_ASSERT_EQUAL_PTR(app->http_client, client);
    TEST_ASSERT_NOT_NULL(request);
    TEST_ASSERT_EQUAL_STRING("id", request->username);
    TEST_ASSERT_EQUAL_STRING("sicrit", request->password);
    TEST_ASSERT_EQUAL_PTR(some_response, request->response_buffer);
    TEST_ASSERT_EQUAL_STRING("https://www.reddit.com/api/v1/access_token/", request->url);
    TEST_ASSERT_NOT_NULL(request->headers);
    TEST_ASSERT_EQUAL_STRING("User-Agent: lol", request->headers->data);
    TEST_ASSERT_EQUAL_STRING("scope=read&grant_type=client_credentials", request->post_fields);
    TEST_ASSERT_TRUE(request->write_callback != NULL);
}

static void assert_hot_listings_request(CURL* client, const struct hot_listings_request* request, int cmock_num_calls) {
    TEST_ASSERT_EQUAL(0, cmock_num_calls);
    TEST_ASSERT_EQUAL_PTR(app->http_client, client);
    TEST_ASSERT_NOT_NULL(request);
    TEST_ASSERT_EQUAL_PTR(some_response, request->response_buffer);
    TEST_ASSERT_EQUAL_STRING("https://oauth.reddit.com/r/libertarian/hot/?limit=15", request->url);
    TEST_ASSERT_EQUAL_STRING("fake-token", request->bearer_token);
    TEST_ASSERT_NOT_NULL(request->headers);
    TEST_ASSERT_EQUAL_STRING("User-Agent: lol", request->headers->data);
    TEST_ASSERT_TRUE(request->write_callback != NULL);
}

static long* fake_status_code(long status_code) {
    long* code = g_new(long, 1);
    *code = status_code;
    return code;
}

void setUp(void) {
    mock_curl_wrappers_Init();
    mock_easy_Init();
    app = fake_app();
    some_response = fake_response("{\"access_token\":\"the reddit app token\"}");
}

void tearDown(void) {
    mock_curl_wrappers_Verify();
    mock_easy_Verify();
    mock_curl_wrappers_Destroy();
    mock_easy_Destroy();
    free(app->config->auth);
    free(app->config);
    free(app);
    free(some_response);
}

void test_happy_path(void) {
    new_response_buffer_ExpectAndReturn(some_response);

    curl_easy_reset_Expect(app->http_client);
    configure_access_token_request_ExpectAnyArgs();
    configure_access_token_request_StubWithCallback(assert_access_token_request);
    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_OK);

    long* fake_response_code = fake_status_code((long)HTTP_OK);
    get_response_status_ExpectAndReturn(app->http_client, fake_response_code);
    http_status_code_from_ExpectAndReturn(*fake_response_code, HTTP_OK);
    free_response_buffer_Expect(some_response);

    struct reddit_api_response* response = fetch_reddit_access_token_from_api(app);
    TEST_ASSERT_EQUAL(some_response, response->response_buffer);

    free_reddit_api_response(response);
}

void test_fetch_reddit_access_token_non_200_response_status(void) {
    new_response_buffer_ExpectAndReturn(some_response);
    curl_easy_reset_Expect(app->http_client);
    configure_access_token_request_ExpectAnyArgs();
    configure_access_token_request_StubWithCallback(assert_access_token_request);

    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_OK);

    long* fake_response_code = fake_status_code((long)HTTP_BAD_REQUEST);
    get_response_status_ExpectAndReturn(app->http_client, fake_response_code);
    http_status_code_from_ExpectAndReturn(*fake_response_code, HTTP_BAD_REQUEST);
    free_response_buffer_Expect(some_response);

    struct reddit_api_response* response = fetch_reddit_access_token_from_api(app);
    TEST_ASSERT_EQUAL(some_response, response->response_buffer);

    free_reddit_api_response(response);
}

void test_fetch_hot_listings_configures_request(void) {
    RedditAccessToken token = {.token = "fake-token"};
    new_response_buffer_ExpectAndReturn(some_response);
    curl_easy_reset_Expect(app->http_client);
    configure_hot_listings_request_ExpectAnyArgs();
    configure_hot_listings_request_StubWithCallback(assert_hot_listings_request);
    curl_easy_perform_ExpectAndReturn(app->http_client, CURLE_OK);

    long* fake_response_code = fake_status_code((long)HTTP_OK);
    get_response_status_ExpectAndReturn(app->http_client, fake_response_code);
    http_status_code_from_ExpectAndReturn(*fake_response_code, HTTP_OK);
    free_response_buffer_Expect(some_response);

    struct reddit_api_response* response = fetch_hot_listings(app, &token, "libertarian");
    TEST_ASSERT_EQUAL(some_response, response->response_buffer);

    free_reddit_api_response(response);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_happy_path);
    RUN_TEST(test_fetch_reddit_access_token_non_200_response_status);
    RUN_TEST(test_fetch_hot_listings_configures_request);
    return UNITY_END();
}
