#include <curl/curl.h>
#include <stddef.h>

#ifndef CURL_WRAPPERS_H
#define CURL_WRAPPERS_H

struct response_buffer {
    char* buffer;
    size_t size;
};

struct access_token_request {
    const char* username;
    const char* password;
    curl_write_callback write_callback;
    struct response_buffer* response_buffer;
    const char* url;
    struct curl_slist* headers;
    const char* post_fields;
};

struct hot_listings_request {
    curl_write_callback write_callback;
    struct response_buffer* response_buffer;
    const char* url;
    const char* bearer_token;
    struct curl_slist* headers;
};

struct response_buffer* new_response_buffer();

void free_response_buffer(struct response_buffer* resp);

long* get_response_status(CURL* client);

void configure_access_token_request(CURL* client, const struct access_token_request* request);

void configure_hot_listings_request(CURL* client, const struct hot_listings_request* request);

enum http_status_code {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_UNAUTHORIZED = 401,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404
};

enum http_status_code http_status_code_from(long code);

#endif
