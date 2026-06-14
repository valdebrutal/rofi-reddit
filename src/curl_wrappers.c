#include "curl_wrappers.h"
#include <curl/curl.h>
#include <glib.h>

static const int INITIAL_RESPONSE_BUFFER_SIZE = (256 * 1024);

struct response_buffer* new_response_buffer() {
    struct response_buffer* resp = g_new(struct response_buffer, 1);
    resp->buffer = g_new(char, INITIAL_RESPONSE_BUFFER_SIZE);
    resp->size = 0;
    return resp;
}

void free_response_buffer(struct response_buffer* resp) {
    g_free(resp->buffer);
    g_free(resp);
}

long* get_response_status(CURL* client) {
    long* http_code = g_new(long, 1);
    curl_easy_getinfo(client, CURLINFO_RESPONSE_CODE, http_code);
    return http_code;
}

void configure_access_token_request(CURL* client, const struct access_token_request* request) {
    if (!client || !request)
        return;

    curl_easy_setopt(client, CURLOPT_POST, 1L);
    curl_easy_setopt(client, CURLOPT_USERNAME, request->username);
    curl_easy_setopt(client, CURLOPT_PASSWORD, request->password);
    curl_easy_setopt(client, CURLOPT_WRITEFUNCTION, request->write_callback);
    curl_easy_setopt(client, CURLOPT_WRITEDATA, request->response_buffer);
    curl_easy_setopt(client, CURLOPT_URL, request->url);
    curl_easy_setopt(client, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(client, CURLOPT_HTTPHEADER, request->headers);
    curl_easy_setopt(client, CURLOPT_POSTFIELDS, request->post_fields);
}

void configure_hot_listings_request(CURL* client, const struct hot_listings_request* request) {
    if (!client || !request)
        return;

    curl_easy_setopt(client, CURLOPT_POST, 0L);
    curl_easy_setopt(client, CURLOPT_WRITEFUNCTION, request->write_callback);
    curl_easy_setopt(client, CURLOPT_WRITEDATA, request->response_buffer);
    curl_easy_setopt(client, CURLOPT_URL, request->url);
    curl_easy_setopt(client, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(client, CURLOPT_XOAUTH2_BEARER, request->bearer_token);
    curl_easy_setopt(client, CURLOPT_HTTPHEADER, request->headers);
    curl_easy_setopt(client, CURLOPT_FOLLOWLOCATION, 1L);
}

enum http_status_code http_status_code_from(long code) {
    switch (code) {
    case 200L:
        return HTTP_OK;
    case 400L:
        return HTTP_BAD_REQUEST;
    case 401L:
        return HTTP_UNAUTHORIZED;
    case 403L:
        return HTTP_FORBIDDEN;
    case 404L:
        return HTTP_NOT_FOUND;
    default:
        return (enum http_status_code)code;
    }
}
