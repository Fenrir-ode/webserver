#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "mongoose.h"
#include <utest.h>
#include "log.h"
#include "cdfmt.h"
#include "httpd.h"
#include "scandir.h"
#include "menu.http.h"

#define FETCH_BUF_SIZE (256 * 1024)

uint32_t menu_poll_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data);
uint32_t menu_http_handler(struct mg_connection* c, int ev, void* ev_data, void* fn_data);

struct fetch_data {
    char* buf;
    int code, closed;
};
static void fcb(struct mg_connection* c, int ev, void* ev_data, void* fn_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        struct fetch_data* fd = (struct fetch_data*)fn_data;
        snprintf(fd->buf, FETCH_BUF_SIZE, "%.*s", (int)hm->message.len,
            hm->message.ptr);
        fd->code = atoi(hm->uri.ptr);
        fd->closed = 1;
        c->is_closing = 1;
        (void)c;
    }
} 

static int fetch(struct mg_mgr* mgr, struct fetch_data *fd, const char* url,
    const char* fmt, ...) {
    int i;
    struct mg_connection* c = mg_http_connect(mgr, url, fcb, fd);
    va_list ap;    
    va_start(ap, fmt);
    mg_vprintf(c, fmt, ap);
    va_end(ap);
    fd->buf[0] = '\0';
    for (i = 0; i < 250 && fd->buf[0] == '\0'; i++) mg_mgr_poll(mgr, 1);
    if (!fd->closed) c->is_closing = 1;
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
    const char* url = "http://127.0.0.1:3000";
    char buf[FETCH_BUF_SIZE];
    struct fetch_data fd = { buf, 0, 0 };
    struct mg_mgr mgr;
    struct mg_http_message hm;
    mg_mgr_init(&mgr);
    httpd_init(&mgr);
    menu_register_routes(&mgr);
    mg_http_listen(&mgr, "http://0.0.0.0:3000", httpd_poll, (void*)&fud);
	menu_http_handler_init(&fud);

    const char* s = "HEAD /dir HTTP/1.0\r\n\n ";

    ASSERT_EQ(fetch(&mgr, &fd, url, s), 200);
    mg_http_parse(buf, strlen(buf), &hm);
    struct mg_str * msg = mg_http_get_header(&hm, "entry-count");
    ASSERT_EQ(mg_vcmp(msg, "2"), 0);

    mg_mgr_free(&mgr);
}

UTEST_STATE();

int main(int argc, const char *const argv[])
{
    

  return utest_main(argc, argv);
}

int tree_scandir(char *dirname, scandir_cbk_t cbk, uintptr_t ud)
{
  cbk("fullpath/game0.cue", "game0", ud);
  cbk("fullpath/game1.iso", "game1", ud);
  cbk("fullpath/game2.unk", "game2", ud);
  return 0;
}
