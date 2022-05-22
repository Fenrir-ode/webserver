#include "mongoose.h"
#include "mdns/mdns.h"
#include "client.h"
#include "log.h"

#define MDNS_FENRIR_SRV_NAME ("FENRIR-WebServer._http._tcp.local.")

int find_service(const char *name, find_service_t *mdns_record_a);

static void sockaddr_to_string(struct sockaddr *_sockaddr, char * str)
{
    char clientservice[NI_MAXSERV];
    getnameinfo((const struct sockaddr *)_sockaddr, sizeof(struct sockaddr), str, NI_MAXSERV, clientservice, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
}

int mdns_setup_fenrir()
{
    find_service_t find_service_r;

    if (find_service(MDNS_FENRIR_SRV_NAME, &find_service_r) == 0)
    {

        char clienthost[NI_MAXHOST]; // The clienthost will hold the IP address.
        char serverhost[NI_MAXHOST]; // The clienthost will hold the IP address.
        sockaddr_to_string((struct sockaddr*)&find_service_r.addr, clienthost);
        log_trace("found fenrir ip: %s !!", clienthost);
        sockaddr_to_string(&find_service_r.interface_addr, serverhost);
        log_trace("found local ip: %s !!", serverhost);

        char url[256];
        sprintf(url, "http://%s/link?server_ip=%s", clienthost, serverhost);

        client_data_t client_data = {
            .url = url,
            .method = CLIENT_METHOD_GET};

        http_client(&client_data);

        return 0;
    }

    return -1;
}
