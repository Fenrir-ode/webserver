#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "mongoose.h"
#include <utest.h>
#include "pack.h"
#include "log.h"
#include "cdfmt.h"
#include "httpd.h"
#include "scandir.h"
#include "menu.http.h"

#define FETCH_BUF_SIZE (256 * 1024)

uint32_t menu_poll_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
uint32_t menu_http_handler(struct mg_connection *c, int ev, void *ev_data, void *fn_data);

struct fetch_data
{
    char *buf;
    char *cur;
    int code, closed;
};
static void fcb(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        struct fetch_data *fd = (struct fetch_data *)fn_data;
        snprintf(fd->buf, FETCH_BUF_SIZE, "%.*s", (int)hm->message.len,
                 hm->message.ptr);
        fd->code = atoi(hm->uri.ptr);
        fd->closed = 1;
        c->is_closing = 1;
        (void)c;
    }
}

static int fetch(struct mg_mgr *mgr, struct fetch_data *fd, const char *url,
                 const char *fmt, ...)
{
    int i;
    struct mg_connection *c = mg_http_connect(mgr, url, fcb, fd);
    va_list ap;
    va_start(ap, fmt);
    mg_vprintf(c, fmt, ap);
    va_end(ap);
    fd->buf[0] = '\0';
    for (i = 0; i < 500 && fd->buf[0] == '\0'; i++)
        mg_mgr_poll(mgr, 1);
    if (!fd->closed)
        c->is_closing = 1;
    mg_mgr_poll(mgr, 1);
    return fd->code;
}

static void fcb_poll(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        struct fetch_data *fd = (struct fetch_data *)fn_data;
        /*
        snprintf(fd->buf, FETCH_BUF_SIZE, "%.*s", (int)hm->message.len,
            hm->message.ptr);*/
        fd->code = atoi(hm->uri.ptr);
        fd->closed = 1;
        c->is_closing = 1;
        (void)c;
    }
    if (ev == MG_EV_HTTP_CHUNK)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        struct fetch_data *fd = (struct fetch_data *)fn_data;
        memcpy(fd->cur, hm->chunk.ptr, hm->chunk.len);
        fd->cur += (int)hm->chunk.len;
        (void)c;
    }
}

static int fetch_chunked(struct mg_mgr *mgr, struct fetch_data *fd, const char *url,
                         const char *fmt, ...)
{
    int i;
    struct mg_connection *c = mg_http_connect(mgr, url, fcb_poll, fd);
    va_list ap;
    fd->cur = fd->buf;
    va_start(ap, fmt);
    mg_vprintf(c, fmt, ap);
    va_end(ap);
    fd->buf[0] = '\0';
    for (i = 0; i < 50 && fd->buf[0] == '\0'; i++)
        mg_mgr_poll(mgr, 1);
    if (!fd->closed)
        c->is_closing = 1;
    mg_mgr_poll(mgr, 1);
    return fd->code;
}

fenrir_user_data_t fud;

UTEST(menu, scandir_results)
{
    menu_http_handler_init(&fud);
    ASSERT_EQ(fud.sd_dir_entries_count, 2);
}

UTEST(menu, menu_head)
{
    const char *url = "http://127.0.0.1:3000";
    char buf[FETCH_BUF_SIZE];
    struct fetch_data fd = {buf, 0, 0};
    struct mg_mgr mgr;
    struct mg_http_message hm;
    mg_mgr_init(&mgr);
    httpd_init(&mgr);
    menu_register_routes(&mgr);
    mg_http_listen(&mgr, "http://0.0.0.0:3000", httpd_poll, (void *)&fud);
    menu_http_handler_init(&fud);

    const char *s = "HEAD /dir HTTP/1.0\r\n\n ";

    ASSERT_EQ(fetch(&mgr, &fd, url, s), 200);
    mg_http_parse(buf, strlen(buf), &hm);
    struct mg_str *msg = mg_http_get_header(&hm, "entry-count");
    ASSERT_EQ(mg_vcmp(msg, "2"), 0);

    mg_mgr_free(&mgr);
}

// 64bytes - fenrir fmt
typedef PACKED(
    struct
    {
        uint16_t id;
        uint32_t flag;
        char filename[58];
    }) sd_dir_entry_t;

UTEST(menu, menu_get)
{
    const char *url = "http://127.0.0.1:3000";
    char buf[FETCH_BUF_SIZE];
    struct fetch_data fd = {buf, 0, 0};
    struct mg_mgr mgr;
    struct mg_http_message hm;
    mg_mgr_init(&mgr);
    httpd_init(&mgr);
    menu_register_routes(&mgr);
    mg_http_listen(&mgr, "http://0.0.0.0:3000", httpd_poll, (void *)&fud);
    menu_http_handler_init(&fud);

    const char *s = "GET /dir HTTP/1.0\r\n\n ";

    ASSERT_EQ(fetch_chunked(&mgr, &fd, url, s), 206);
    mg_http_parse(buf, strlen(buf), &hm);

    sd_dir_entry_t *sd_dir_entries = (sd_dir_entry_t *)buf;

    ASSERT_STREQ("game0", sd_dir_entries[0].filename);
    ASSERT_STREQ("game1", sd_dir_entries[1].filename);

    mg_mgr_free(&mgr);
}

UTEST(menu, menu_get_by_id)
{
    menu_http_handler_init(&fud);
    
    int err;
    char filename[512];
    err = menu_get_filename_by_id(&fud, 0, filename);

    ASSERT_EQ(err, 0);
    ASSERT_STREQ(filename, "fullpath/game0.cue");

    err = menu_get_filename_by_id(&fud, 1, filename);
    ASSERT_EQ(err, 0);
    ASSERT_STREQ(filename, "fullpath/game1.iso");

    filename[0] = 0;
    err = menu_get_filename_by_id(&fud, 27, filename);
    ASSERT_EQ(err, -1);
    ASSERT_STREQ(filename, "");
}

UTEST_STATE();

int main(int argc, const char *const argv[])
{
    fud.http_buffer = (uint8_t *)malloc(2352);

    return utest_main(argc, argv);
}

int tree_scandir(char *dirname, scandir_cbk_t cbk, uintptr_t ud)
{
    cbk("fullpath/game0.cue", "game0", ud);
    cbk("fullpath/game1.iso", "game1", ud);
    cbk("fullpath/game2.unk", "game2", ud);
    return 0;
}
