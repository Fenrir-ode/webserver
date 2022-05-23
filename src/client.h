#pragma once

enum client_method {
    CLIENT_METHOD_GET,
    CLIENT_METHOD_POST
};

typedef struct
{
    char *url;
    void * post;
    int data_len;
    enum client_method method;
    int finished;
} client_data_t;

int http_client(client_data_t *client_data);
