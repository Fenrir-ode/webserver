
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include <stdio.h>

#include <errno.h>
#include <signal.h>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#define sleep(x) Sleep(x * 1000)
#else
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

// Alias some things to simulate recieving data to fuzz library
#if defined(MDNS_FUZZING)
#define recvfrom(sock, buffer, capacity, flags, src_addr, addrlen) ((mdns_ssize_t)capacity)
#define printf
#endif

#include "mdns.h"
#include "../log.h"

#if defined(MDNS_FUZZING)
#undef recvfrom
#endif

static char addrbuffer[64];
static char entrybuffer[256];
static char namebuffer[256];
static char sendbuffer[1024];
static mdns_record_txt_t txtbuffer[128];

static struct sockaddr_in service_address_ipv4;
static struct sockaddr_in6 service_address_ipv6;

static int has_ipv4;
static int has_ipv6;

volatile sig_atomic_t running = 1;

// Data for our service including the mDNS records
typedef struct
{
    mdns_string_t service;
    mdns_string_t hostname;
    mdns_string_t service_instance;
    mdns_string_t hostname_qualified;
    struct sockaddr_in address_ipv4;
    struct sockaddr_in6 address_ipv6;
    int port;
    mdns_record_t record_ptr;
    mdns_record_t record_srv;
    mdns_record_t record_a;
    mdns_record_t record_aaaa;
    mdns_record_t txt_record[2];
} service_t;

static mdns_string_t
ipv4_address_to_string(char *buffer, size_t capacity, const struct sockaddr_in *addr,
                       size_t addrlen)
{
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr *)addr, (socklen_t)addrlen, host, NI_MAXHOST,
                          service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0)
    {
        if (addr->sin_port != 0)
            len = snprintf(buffer, capacity, "%s:%s", host, service);
        else
            len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int)capacity)
        len = (int)capacity - 1;
    mdns_string_t str;
    str.str = buffer;
    str.length = len;
    return str;
}

static mdns_string_t
ipv6_address_to_string(char *buffer, size_t capacity, const struct sockaddr_in6 *addr,
                       size_t addrlen)
{
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr *)addr, (socklen_t)addrlen, host, NI_MAXHOST,
                          service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0)
    {
        if (addr->sin6_port != 0)
            len = snprintf(buffer, capacity, "[%s]:%s", host, service);
        else
            len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int)capacity)
        len = (int)capacity - 1;
    mdns_string_t str;
    str.str = buffer;
    str.length = len;
    return str;
}

static mdns_string_t
ip_address_to_string(char *buffer, size_t capacity, const struct sockaddr *addr, size_t addrlen)
{
    if (addr->sa_family == AF_INET6)
        return ipv6_address_to_string(buffer, capacity, (const struct sockaddr_in6 *)addr, addrlen);
    return ipv4_address_to_string(buffer, capacity, (const struct sockaddr_in *)addr, addrlen);
}

// Callback handling parsing answers to queries sent
static int
query_callback(int sock, const struct sockaddr *from, size_t addrlen, mdns_entry_type_t entry,
               uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void *data,
               size_t size, size_t name_offset, size_t name_length, size_t record_offset,
               size_t record_length, void *user_data)
{
    (void)sizeof(sock);
    (void)sizeof(query_id);
    (void)sizeof(name_length);
    (void)sizeof(user_data);
    mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);
    const char *entrytype = (entry == MDNS_ENTRYTYPE_ANSWER) ? "answer" : ((entry == MDNS_ENTRYTYPE_AUTHORITY) ? "authority" : "additional");
    mdns_string_t entrystr =
        mdns_string_extract(data, size, &name_offset, entrybuffer, sizeof(entrybuffer));
    if (rtype == MDNS_RECORDTYPE_PTR)
    {
        mdns_string_t namestr = mdns_record_parse_ptr(data, size, record_offset, record_length,
                                                      namebuffer, sizeof(namebuffer));
        log_trace("%.*s : %s %.*s PTR %.*s rclass 0x%x ttl %u length %d",
                  MDNS_STRING_FORMAT(fromaddrstr), entrytype, MDNS_STRING_FORMAT(entrystr),
                  MDNS_STRING_FORMAT(namestr), rclass, ttl, (int)record_length);
    }
    else if (rtype == MDNS_RECORDTYPE_SRV)
    {
        mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length,
                                                      namebuffer, sizeof(namebuffer));
        log_trace("%.*s : %s %.*s SRV %.*s priority %d weight %d port %d",
                  MDNS_STRING_FORMAT(fromaddrstr), entrytype, MDNS_STRING_FORMAT(entrystr),
                  MDNS_STRING_FORMAT(srv.name), srv.priority, srv.weight, srv.port);
    }
    else if (rtype == MDNS_RECORDTYPE_A)
    {
        struct sockaddr_in addr;
        mdns_record_parse_a(data, size, record_offset, record_length, &addr);
        mdns_string_t addrstr =
            ipv4_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
        log_trace("%.*s : %s %.*s A %.*s", MDNS_STRING_FORMAT(fromaddrstr), entrytype,
                  MDNS_STRING_FORMAT(entrystr), MDNS_STRING_FORMAT(addrstr));
    }
    else if (rtype == MDNS_RECORDTYPE_AAAA)
    {
        struct sockaddr_in6 addr;
        mdns_record_parse_aaaa(data, size, record_offset, record_length, &addr);
        mdns_string_t addrstr =
            ipv6_address_to_string(namebuffer, sizeof(namebuffer), &addr, sizeof(addr));
        log_trace("%.*s : %s %.*s AAAA %.*s", MDNS_STRING_FORMAT(fromaddrstr), entrytype,
                  MDNS_STRING_FORMAT(entrystr), MDNS_STRING_FORMAT(addrstr));
    }
    else if (rtype == MDNS_RECORDTYPE_TXT)
    {
        size_t parsed = mdns_record_parse_txt(data, size, record_offset, record_length, txtbuffer,
                                              sizeof(txtbuffer) / sizeof(mdns_record_txt_t));
        for (size_t itxt = 0; itxt < parsed; ++itxt)
        {
            if (txtbuffer[itxt].value.length)
            {
                log_trace("%.*s : %s %.*s TXT %.*s = %.*s", MDNS_STRING_FORMAT(fromaddrstr),
                          entrytype, MDNS_STRING_FORMAT(entrystr),
                          MDNS_STRING_FORMAT(txtbuffer[itxt].key),
                          MDNS_STRING_FORMAT(txtbuffer[itxt].value));
            }
            else
            {
                log_trace("%.*s : %s %.*s TXT %.*s", MDNS_STRING_FORMAT(fromaddrstr), entrytype,
                          MDNS_STRING_FORMAT(entrystr), MDNS_STRING_FORMAT(txtbuffer[itxt].key));
            }
        }
    }
    else
    {
        log_trace("%.*s : %s %.*s type %u rclass 0x%x ttl %u length %d",
                  MDNS_STRING_FORMAT(fromaddrstr), entrytype, MDNS_STRING_FORMAT(entrystr), rtype,
                  rclass, ttl, (int)record_length);
    }
    return 0;
}

// Callback handling questions incoming on service sockets
static int
service_callback(int sock, const struct sockaddr *from, size_t addrlen, mdns_entry_type_t entry,
                 uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void *data,
                 size_t size, size_t name_offset, size_t name_length, size_t record_offset,
                 size_t record_length, void *user_data)
{
    (void)sizeof(ttl);
    if (entry != MDNS_ENTRYTYPE_QUESTION)
        return 0;

    const char dns_sd[] = "_services._dns-sd._udp.local.";
    const service_t *service = (const service_t *)user_data;

    mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);

    size_t offset = name_offset;
    mdns_string_t name = mdns_string_extract(data, size, &offset, namebuffer, sizeof(namebuffer));

    const char *record_name = 0;
    if (rtype == MDNS_RECORDTYPE_PTR)
        record_name = "PTR";
    else if (rtype == MDNS_RECORDTYPE_SRV)
        record_name = "SRV";
    else if (rtype == MDNS_RECORDTYPE_A)
        record_name = "A";
    else if (rtype == MDNS_RECORDTYPE_AAAA)
        record_name = "AAAA";
    else if (rtype == MDNS_RECORDTYPE_TXT)
        record_name = "TXT";
    else if (rtype == MDNS_RECORDTYPE_ANY)
        record_name = "ANY";
    else
        return 0;
    log_trace("Query %s %.*s", record_name, MDNS_STRING_FORMAT(name));

    if ((name.length == (sizeof(dns_sd) - 1)) &&
        (strncmp(name.str, dns_sd, sizeof(dns_sd) - 1) == 0))
    {
        if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype == MDNS_RECORDTYPE_ANY))
        {
            // The PTR query was for the DNS-SD domain, send answer with a PTR record for the
            // service name we advertise, typically on the "<_service-name>._tcp.local." format

            // Answer PTR record reverse mapping "<_service-name>._tcp.local." to
            // "<hostname>.<_service-name>._tcp.local."
            mdns_record_t answer = {
                .name = name, .type = MDNS_RECORDTYPE_PTR, .data.ptr.name = service->service};

            // Send the answer, unicast or multicast depending on flag in query
            uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
            log_trace("  --> answer %.*s (%s)", MDNS_STRING_FORMAT(answer.data.ptr.name),
                      (unicast ? "unicast" : "multicast"));

            if (unicast)
            {
                mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                          query_id, rtype, name.str, name.length, answer, 0, 0, 0,
                                          0);
            }
            else
            {
                mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0, 0,
                                            0);
            }
        }
    }
    else if ((name.length == service->service.length) &&
             (strncmp(name.str, service->service.str, name.length) == 0))
    {
        if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype == MDNS_RECORDTYPE_ANY))
        {
            // The PTR query was for our service (usually "<_service-name._tcp.local"), answer a PTR
            // record reverse mapping the queried service name to our service instance name
            // (typically on the "<hostname>.<_service-name>._tcp.local." format), and add
            // additional records containing the SRV record mapping the service instance name to our
            // qualified hostname (typically "<hostname>.local.") and port, as well as any IPv4/IPv6
            // address for the hostname as A/AAAA records, and two test TXT records

            // Answer PTR record reverse mapping "<_service-name>._tcp.local." to
            // "<hostname>.<_service-name>._tcp.local."
            mdns_record_t answer = service->record_ptr;

            mdns_record_t additional[5] = {0};
            size_t additional_count = 0;

            // SRV record mapping "<hostname>.<_service-name>._tcp.local." to
            // "<hostname>.local." with port. Set weight & priority to 0.
            additional[additional_count++] = service->record_srv;

            // A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
            if (service->address_ipv4.sin_family == AF_INET)
                additional[additional_count++] = service->record_a;
            if (service->address_ipv6.sin6_family == AF_INET6)
                additional[additional_count++] = service->record_aaaa;

            // Add two test TXT records for our service instance name, will be coalesced into
            // one record with both key-value pair strings by the library
            additional[additional_count++] = service->txt_record[0];
            additional[additional_count++] = service->txt_record[1];

            // Send the answer, unicast or multicast depending on flag in query
            uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
            log_trace("  --> answer %.*s (%s)",
                      MDNS_STRING_FORMAT(service->record_ptr.data.ptr.name),
                      (unicast ? "unicast" : "multicast"));

            if (unicast)
            {
                mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                          query_id, rtype, name.str, name.length, answer, 0, 0,
                                          additional, additional_count);
            }
            else
            {
                mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
                                            additional, additional_count);
            }
        }
    }
    else if ((name.length == service->service_instance.length) &&
             (strncmp(name.str, service->service_instance.str, name.length) == 0))
    {
        if ((rtype == MDNS_RECORDTYPE_SRV) || (rtype == MDNS_RECORDTYPE_ANY))
        {
            // The SRV query was for our service instance (usually
            // "<hostname>.<_service-name._tcp.local"), answer a SRV record mapping the service
            // instance name to our qualified hostname (typically "<hostname>.local.") and port, as
            // well as any IPv4/IPv6 address for the hostname as A/AAAA records, and two test TXT
            // records

            // Answer PTR record reverse mapping "<_service-name>._tcp.local." to
            // "<hostname>.<_service-name>._tcp.local."
            mdns_record_t answer = service->record_srv;

            mdns_record_t additional[5] = {0};
            size_t additional_count = 0;

            // A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
            if (service->address_ipv4.sin_family == AF_INET)
                additional[additional_count++] = service->record_a;
            if (service->address_ipv6.sin6_family == AF_INET6)
                additional[additional_count++] = service->record_aaaa;

            // Add two test TXT records for our service instance name, will be coalesced into
            // one record with both key-value pair strings by the library
            additional[additional_count++] = service->txt_record[0];
            additional[additional_count++] = service->txt_record[1];

            // Send the answer, unicast or multicast depending on flag in query
            uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
            log_trace("  --> answer %.*s port %d (%s)",
                      MDNS_STRING_FORMAT(service->record_srv.data.srv.name), service->port,
                      (unicast ? "unicast" : "multicast"));

            if (unicast)
            {
                mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                          query_id, rtype, name.str, name.length, answer, 0, 0,
                                          additional, additional_count);
            }
            else
            {
                mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
                                            additional, additional_count);
            }
        }
    }
    else if ((name.length == service->hostname_qualified.length) &&
             (strncmp(name.str, service->hostname_qualified.str, name.length) == 0))
    {
        if (((rtype == MDNS_RECORDTYPE_A) || (rtype == MDNS_RECORDTYPE_ANY)) &&
            (service->address_ipv4.sin_family == AF_INET))
        {
            // The A query was for our qualified hostname (typically "<hostname>.local.") and we
            // have an IPv4 address, answer with an A record mappiing the hostname to an IPv4
            // address, as well as any IPv6 address for the hostname, and two test TXT records

            // Answer A records mapping "<hostname>.local." to IPv4 address
            mdns_record_t answer = service->record_a;

            mdns_record_t additional[5] = {0};
            size_t additional_count = 0;

            // AAAA record mapping "<hostname>.local." to IPv6 addresses
            if (service->address_ipv6.sin6_family == AF_INET6)
                additional[additional_count++] = service->record_aaaa;

            // Add two test TXT records for our service instance name, will be coalesced into
            // one record with both key-value pair strings by the library
            additional[additional_count++] = service->txt_record[0];
            additional[additional_count++] = service->txt_record[1];

            // Send the answer, unicast or multicast depending on flag in query
            uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
            mdns_string_t addrstr = ip_address_to_string(
                addrbuffer, sizeof(addrbuffer), (struct sockaddr *)&service->record_a.data.a.addr,
                sizeof(service->record_a.data.a.addr));
            log_trace("  --> answer %.*s IPv4 %.*s (%s)", MDNS_STRING_FORMAT(service->record_a.name),
                      MDNS_STRING_FORMAT(addrstr), (unicast ? "unicast" : "multicast"));

            if (unicast)
            {
                mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                          query_id, rtype, name.str, name.length, answer, 0, 0,
                                          additional, additional_count);
            }
            else
            {
                mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
                                            additional, additional_count);
            }
        }
        else if (((rtype == MDNS_RECORDTYPE_AAAA) || (rtype == MDNS_RECORDTYPE_ANY)) &&
                 (service->address_ipv6.sin6_family == AF_INET6))
        {
            // The AAAA query was for our qualified hostname (typically "<hostname>.local.") and we
            // have an IPv6 address, answer with an AAAA record mappiing the hostname to an IPv6
            // address, as well as any IPv4 address for the hostname, and two test TXT records

            // Answer AAAA records mapping "<hostname>.local." to IPv6 address
            mdns_record_t answer = service->record_aaaa;

            mdns_record_t additional[5] = {0};
            size_t additional_count = 0;

            // A record mapping "<hostname>.local." to IPv4 addresses
            if (service->address_ipv4.sin_family == AF_INET)
                additional[additional_count++] = service->record_a;

            // Add two test TXT records for our service instance name, will be coalesced into
            // one record with both key-value pair strings by the library
            additional[additional_count++] = service->txt_record[0];
            additional[additional_count++] = service->txt_record[1];

            // Send the answer, unicast or multicast depending on flag in query
            uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
            mdns_string_t addrstr =
                ip_address_to_string(addrbuffer, sizeof(addrbuffer),
                                     (struct sockaddr *)&service->record_aaaa.data.aaaa.addr,
                                     sizeof(service->record_aaaa.data.aaaa.addr));
            log_trace("  --> answer %.*s IPv6 %.*s (%s)",
                      MDNS_STRING_FORMAT(service->record_aaaa.name), MDNS_STRING_FORMAT(addrstr),
                      (unicast ? "unicast" : "multicast"));

            if (unicast)
            {
                mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                          query_id, rtype, name.str, name.length, answer, 0, 0,
                                          additional, additional_count);
            }
            else
            {
                mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
                                            additional, additional_count);
            }
        }
    }
    return 0;
}

// Callback handling questions and answers dump
static int
dump_callback(int sock, const struct sockaddr *from, size_t addrlen, mdns_entry_type_t entry,
              uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void *data,
              size_t size, size_t name_offset, size_t name_length, size_t record_offset,
              size_t record_length, void *user_data)
{
    mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);

    size_t offset = name_offset;
    mdns_string_t name = mdns_string_extract(data, size, &offset, namebuffer, sizeof(namebuffer));

    const char *record_name = 0;
    if (rtype == MDNS_RECORDTYPE_PTR)
        record_name = "PTR";
    else if (rtype == MDNS_RECORDTYPE_SRV)
        record_name = "SRV";
    else if (rtype == MDNS_RECORDTYPE_A)
        record_name = "A";
    else if (rtype == MDNS_RECORDTYPE_AAAA)
        record_name = "AAAA";
    else if (rtype == MDNS_RECORDTYPE_TXT)
        record_name = "TXT";
    else if (rtype == MDNS_RECORDTYPE_ANY)
        record_name = "ANY";
    else
        record_name = "<UNKNOWN>";

    const char *entry_type = "Question";
    if (entry == MDNS_ENTRYTYPE_ANSWER)
        entry_type = "Answer";
    else if (entry == MDNS_ENTRYTYPE_AUTHORITY)
        entry_type = "Authority";
    else if (entry == MDNS_ENTRYTYPE_ADDITIONAL)
        entry_type = "Additional";

    log_trace("%.*s: %s %s %.*s rclass 0x%x ttl %u", MDNS_STRING_FORMAT(fromaddrstr), entry_type,
              record_name, MDNS_STRING_FORMAT(name), (unsigned int)rclass, ttl);

    return 0;
}

// Open sockets for sending one-shot multicast queries from an ephemeral port
static int
open_client_sockets(int *sockets, int max_sockets, int port)
{
    // When sending, each socket can only send to one network interface
    // Thus we need to open one socket for each interface and address family
    int num_sockets = 0;

#ifdef _WIN32

    IP_ADAPTER_ADDRESSES *adapter_address = 0;
    ULONG address_size = 8000;
    unsigned int ret;
    unsigned int num_retries = 4;
    do
    {
        adapter_address = (IP_ADAPTER_ADDRESSES *)malloc(address_size);
        ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, 0,
                                   adapter_address, &address_size);
        if (ret == ERROR_BUFFER_OVERFLOW)
        {
            free(adapter_address);
            adapter_address = 0;
            address_size *= 2;
        }
        else
        {
            break;
        }
    } while (num_retries-- > 0);

    if (!adapter_address || (ret != NO_ERROR))
    {
        free(adapter_address);
        log_trace("Failed to get network adapter addresses");
        return num_sockets;
    }

    int first_ipv4 = 1;
    int first_ipv6 = 1;
    for (PIP_ADAPTER_ADDRESSES adapter = adapter_address; adapter; adapter = adapter->Next)
    {
        if (adapter->TunnelType == TUNNEL_TYPE_TEREDO)
            continue;
        if (adapter->OperStatus != IfOperStatusUp)
            continue;

        for (IP_ADAPTER_UNICAST_ADDRESS *unicast = adapter->FirstUnicastAddress; unicast;
             unicast = unicast->Next)
        {
            if (unicast->Address.lpSockaddr->sa_family == AF_INET)
            {
                struct sockaddr_in *saddr = (struct sockaddr_in *)unicast->Address.lpSockaddr;
                if ((saddr->sin_addr.S_un.S_un_b.s_b1 != 127) ||
                    (saddr->sin_addr.S_un.S_un_b.s_b2 != 0) ||
                    (saddr->sin_addr.S_un.S_un_b.s_b3 != 0) ||
                    (saddr->sin_addr.S_un.S_un_b.s_b4 != 1))
                {
                    int log_addr = 0;
                    if (first_ipv4)
                    {
                        service_address_ipv4 = *saddr;
                        first_ipv4 = 0;
                        log_addr = 1;
                    }
                    has_ipv4 = 1;
                    if (num_sockets < max_sockets)
                    {
                        saddr->sin_port = htons((unsigned short)port);
                        int sock = mdns_socket_open_ipv4(saddr);
                        if (sock >= 0)
                        {
                            sockets[num_sockets++] = sock;
                            log_addr = 1;
                        }
                        else
                        {
                            log_addr = 0;
                        }
                    }
                    if (log_addr)
                    {
                        char buffer[128];
                        mdns_string_t addr = ipv4_address_to_string(buffer, sizeof(buffer), saddr,
                                                                    sizeof(struct sockaddr_in));
                        log_trace("Local IPv4 address: %.*s", MDNS_STRING_FORMAT(addr));
                    }
                }
            }
            else if (unicast->Address.lpSockaddr->sa_family == AF_INET6)
            {
                struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)unicast->Address.lpSockaddr;
                // Ignore link-local addresses
                if (saddr->sin6_scope_id)
                    continue;
                static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                          0, 0, 0, 0, 0, 0, 0, 1};
                static const unsigned char localhost_mapped[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                                 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};
                if ((unicast->DadState == NldsPreferred) &&
                    memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
                    memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16))
                {
                    int log_addr = 0;
                    if (first_ipv6)
                    {
                        service_address_ipv6 = *saddr;
                        first_ipv6 = 0;
                        log_addr = 1;
                    }
                    has_ipv6 = 1;
                    if (num_sockets < max_sockets)
                    {
                        saddr->sin6_port = htons((unsigned short)port);
                        int sock = mdns_socket_open_ipv6(saddr);
                        if (sock >= 0)
                        {
                            sockets[num_sockets++] = sock;
                            log_addr = 1;
                        }
                        else
                        {
                            log_addr = 0;
                        }
                    }
                    if (log_addr)
                    {
                        char buffer[128];
                        mdns_string_t addr = ipv6_address_to_string(buffer, sizeof(buffer), saddr,
                                                                    sizeof(struct sockaddr_in6));
                        log_trace("Local IPv6 address: %.*s", MDNS_STRING_FORMAT(addr));
                    }
                }
            }
        }
    }

    free(adapter_address);

#else

    struct ifaddrs *ifaddr = 0;
    struct ifaddrs *ifa = 0;

    if (getifaddrs(&ifaddr) < 0)
        log_trace("Unable to get interface addresses");

    int first_ipv4 = 1;
    int first_ipv6 = 1;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
            continue;
        if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_MULTICAST))
            continue;
        if ((ifa->ifa_flags & IFF_LOOPBACK) || (ifa->ifa_flags & IFF_POINTOPOINT))
            continue;

        if (ifa->ifa_addr->sa_family == AF_INET)
        {
            struct sockaddr_in *saddr = (struct sockaddr_in *)ifa->ifa_addr;
            if (saddr->sin_addr.s_addr != htonl(INADDR_LOOPBACK))
            {
                int log_addr = 0;
                if (first_ipv4)
                {
                    service_address_ipv4 = *saddr;
                    first_ipv4 = 0;
                    log_addr = 1;
                }
                has_ipv4 = 1;
                if (num_sockets < max_sockets)
                {
                    saddr->sin_port = htons(port);
                    int sock = mdns_socket_open_ipv4(saddr);
                    if (sock >= 0)
                    {
                        sockets[num_sockets++] = sock;
                        log_addr = 1;
                    }
                    else
                    {
                        log_addr = 0;
                    }
                }
                if (log_addr)
                {
                    char buffer[128];
                    mdns_string_t addr = ipv4_address_to_string(buffer, sizeof(buffer), saddr,
                                                                sizeof(struct sockaddr_in));
                    log_trace("Local IPv4 address: %.*s", MDNS_STRING_FORMAT(addr));
                }
            }
        }
        else if (ifa->ifa_addr->sa_family == AF_INET6)
        {
            struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)ifa->ifa_addr;
            // Ignore link-local addresses
            if (saddr->sin6_scope_id)
                continue;
            static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                      0, 0, 0, 0, 0, 0, 0, 1};
            static const unsigned char localhost_mapped[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                             0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};
            if (memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
                memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16))
            {
                int log_addr = 1;
                if (first_ipv6)
                {
                    service_address_ipv6 = *saddr;
                    first_ipv6 = 0;
                    log_addr = 1;
                }
                has_ipv6 = 1;
                if (num_sockets < max_sockets)
                {
                    saddr->sin6_port = htons(port);
                    int sock = mdns_socket_open_ipv6(saddr);
                    if (sock >= 0)
                    {
                        sockets[num_sockets++] = sock;
                        log_addr = 1;
                    }
                    else
                    {
                        log_addr = 0;
                    }
                }
                if (log_addr)
                {
                    char buffer[128];
                    mdns_string_t addr = ipv6_address_to_string(buffer, sizeof(buffer), saddr,
                                                                sizeof(struct sockaddr_in6));
                    log_trace("Local IPv6 address: %.*s", MDNS_STRING_FORMAT(addr));
                }
            }
        }
    }

    freeifaddrs(ifaddr);

#endif

    return num_sockets;
}

// Open sockets to listen to incoming mDNS queries on port 5353
static int
open_service_sockets(int *sockets, int max_sockets)
{
    // When recieving, each socket can recieve data from all network interfaces
    // Thus we only need to open one socket for each address family
    int num_sockets = 0;

    // Call the client socket function to enumerate and get local addresses,
    // but not open the actual sockets
    open_client_sockets(0, 0, 0);

    if (num_sockets < max_sockets)
    {
        struct sockaddr_in sock_addr;
        memset(&sock_addr, 0, sizeof(struct sockaddr_in));
        sock_addr.sin_family = AF_INET;
#if defined(_WIN32) && defined(MVSC)
        sock_addr.sin_addr = in4addr_any;
#else
        sock_addr.sin_addr.s_addr = INADDR_ANY;
#endif
        sock_addr.sin_port = htons(MDNS_PORT);
#ifdef __APPLE__
        sock_addr.sin_len = sizeof(struct sockaddr_in);
#endif
        int sock = mdns_socket_open_ipv4(&sock_addr);
        if (sock >= 0)
            sockets[num_sockets++] = sock;
    }

    if (num_sockets < max_sockets)
    {
        struct sockaddr_in6 sock_addr;
        memset(&sock_addr, 0, sizeof(struct sockaddr_in6));
        sock_addr.sin6_family = AF_INET6;
        sock_addr.sin6_addr = in6addr_any;
        sock_addr.sin6_port = htons(MDNS_PORT);
#ifdef __APPLE__
        sock_addr.sin6_len = sizeof(struct sockaddr_in6);
#endif
        int sock = mdns_socket_open_ipv6(&sock_addr);
        if (sock >= 0)
            sockets[num_sockets++] = sock;
    }

    return num_sockets;
}

// Send a DNS-SD query
int send_dns_sd(void)
{
    int sockets[32];
    int num_sockets = open_client_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]), 0);
    if (num_sockets <= 0)
    {
        log_trace("Failed to open any client sockets");
        return -1;
    }
    log_trace("Opened %d socket%s for DNS-SD", num_sockets, num_sockets > 1 ? "s" : "");

    log_trace("Sending DNS-SD discovery");
    for (int isock = 0; isock < num_sockets; ++isock)
    {
        if (mdns_discovery_send(sockets[isock]))
            log_trace("Failed to send DNS-DS discovery: %s", strerror(errno));
    }

    size_t capacity = 2048;
    void *buffer = malloc(capacity);
    void *user_data = 0;
    size_t records;

    // This is a simple implementation that loops for 5 seconds or as long as we get replies
    int res;
    log_trace("Reading DNS-SD replies");
    do
    {
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int nfds = 0;
        fd_set readfs;
        FD_ZERO(&readfs);
        for (int isock = 0; isock < num_sockets; ++isock)
        {
            if (sockets[isock] >= nfds)
                nfds = sockets[isock] + 1;
            FD_SET(sockets[isock], &readfs);
        }

        records = 0;
        res = select(nfds, &readfs, 0, 0, &timeout);
        if (res > 0)
        {
            for (int isock = 0; isock < num_sockets; ++isock)
            {
                if (FD_ISSET(sockets[isock], &readfs))
                {
                    records += mdns_discovery_recv(sockets[isock], buffer, capacity, query_callback,
                                                   user_data);
                }
            }
        }
    } while (res > 0);

    free(buffer);

    for (int isock = 0; isock < num_sockets; ++isock)
        mdns_socket_close(sockets[isock]);
    log_trace("Closed socket%s", num_sockets ? "s" : "");

    return 0;
}

// Send a mDNS query
int send_mdns_query(mdns_query_t *query, size_t count)
{
    int sockets[32];
    int query_id[32];
    int num_sockets = open_client_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]), 0);
    if (num_sockets <= 0)
    {
        log_trace("Failed to open any client sockets");
        return -1;
    }
    log_trace("Opened %d socket%s for mDNS query", num_sockets, num_sockets ? "s" : "");

    size_t capacity = 2048;
    void *buffer = malloc(capacity);
    void *user_data = 0;

    log_trace("Sending mDNS query");
    for (size_t iq = 0; iq < count; ++iq)
    {
        const char *record_name = "PTR";
        if (query[iq].type == MDNS_RECORDTYPE_SRV)
            record_name = "SRV";
        else if (query[iq].type == MDNS_RECORDTYPE_A)
            record_name = "A";
        else if (query[iq].type == MDNS_RECORDTYPE_AAAA)
            record_name = "AAAA";
        else
            query[iq].type = MDNS_RECORDTYPE_PTR;
        log_trace(" : %s %s", query[iq].name, record_name);
    }
    log_trace("");
    for (int isock = 0; isock < num_sockets; ++isock)
    {
        query_id[isock] =
            mdns_multiquery_send(sockets[isock], query, count, buffer, capacity, 0);
        if (query_id[isock] < 0)
            log_trace("Failed to send mDNS query: %s", strerror(errno));
    }

    // This is a simple implementation that loops for 5 seconds or as long as we get replies
    int res;
    log_trace("Reading mDNS query replies");
    int records = 0;
    do
    {
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int nfds = 0;
        fd_set readfs;
        FD_ZERO(&readfs);
        for (int isock = 0; isock < num_sockets; ++isock)
        {
            if (sockets[isock] >= nfds)
                nfds = sockets[isock] + 1;
            FD_SET(sockets[isock], &readfs);
        }

        res = select(nfds, &readfs, 0, 0, &timeout);
        if (res > 0)
        {
            for (int isock = 0; isock < num_sockets; ++isock)
            {
                if (FD_ISSET(sockets[isock], &readfs))
                {
                    int rec = mdns_query_recv(sockets[isock], buffer, capacity, query_callback,
                                              user_data, query_id[isock]);
                    if (rec > 0)
                        records += rec;
                }
                FD_SET(sockets[isock], &readfs);
            }
        }
    } while (res > 0);

    log_trace("Read %d records", records);

    free(buffer);

    for (int isock = 0; isock < num_sockets; ++isock)
        mdns_socket_close(sockets[isock]);
    log_trace("Closed socket%s", num_sockets ? "s" : "");

    return 0;
}

// Provide a mDNS service, answering incoming DNS-SD and mDNS queries
static int
service_mdns(const char *hostname, const char *service_name, int service_port)
{
    int sockets[32];
    int num_sockets = open_service_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]));
    if (num_sockets <= 0)
    {
        log_trace("Failed to open any client sockets");
        return -1;
    }
    log_trace("Opened %d socket%s for mDNS service", num_sockets, num_sockets ? "s" : "");

    size_t service_name_length = strlen(service_name);
    if (!service_name_length)
    {
        log_trace("Invalid service name");
        return -1;
    }

    char *service_name_buffer = malloc(service_name_length + 2);
    memcpy(service_name_buffer, service_name, service_name_length);
    if (service_name_buffer[service_name_length - 1] != '.')
        service_name_buffer[service_name_length++] = '.';
    service_name_buffer[service_name_length] = 0;
    service_name = service_name_buffer;

    log_trace("Service mDNS: %s:%d", service_name, service_port);
    log_trace("Hostname: %s", hostname);

    size_t capacity = 2048;
    void *buffer = malloc(capacity);

    mdns_string_t service_string = (mdns_string_t){service_name, strlen(service_name)};
    mdns_string_t hostname_string = (mdns_string_t){hostname, strlen(hostname)};

    // Build the service instance "<hostname>.<_service-name>._tcp.local." string
    char service_instance_buffer[256] = {0};
    snprintf(service_instance_buffer, sizeof(service_instance_buffer) - 1, "%.*s.%.*s",
             MDNS_STRING_FORMAT(hostname_string), MDNS_STRING_FORMAT(service_string));
    mdns_string_t service_instance_string =
        (mdns_string_t){service_instance_buffer, strlen(service_instance_buffer)};

    // Build the "<hostname>.local." string
    char qualified_hostname_buffer[256] = {0};
    snprintf(qualified_hostname_buffer, sizeof(qualified_hostname_buffer) - 1, "%.*s.local.",
             MDNS_STRING_FORMAT(hostname_string));
    mdns_string_t hostname_qualified_string =
        (mdns_string_t){qualified_hostname_buffer, strlen(qualified_hostname_buffer)};

    service_t service = {0};
    service.service = service_string;
    service.hostname = hostname_string;
    service.service_instance = service_instance_string;
    service.hostname_qualified = hostname_qualified_string;
    service.address_ipv4 = service_address_ipv4;
    service.address_ipv6 = service_address_ipv6;
    service.port = service_port;

    // Setup our mDNS records

    // PTR record reverse mapping "<_service-name>._tcp.local." to
    // "<hostname>.<_service-name>._tcp.local."
    service.record_ptr = (mdns_record_t){.name = service.service,
                                         .type = MDNS_RECORDTYPE_PTR,
                                         .data.ptr.name = service.service_instance,
                                         .rclass = 0,
                                         .ttl = 0};

    // SRV record mapping "<hostname>.<_service-name>._tcp.local." to
    // "<hostname>.local." with port. Set weight & priority to 0.
    service.record_srv = (mdns_record_t){.name = service.service_instance,
                                         .type = MDNS_RECORDTYPE_SRV,
                                         .data.srv.name = service.hostname_qualified,
                                         .data.srv.port = service.port,
                                         .data.srv.priority = 0,
                                         .data.srv.weight = 0,
                                         .rclass = 0,
                                         .ttl = 0};

    // A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
    service.record_a = (mdns_record_t){.name = service.hostname_qualified,
                                       .type = MDNS_RECORDTYPE_A,
                                       .data.a.addr = service.address_ipv4,
                                       .rclass = 0,
                                       .ttl = 0};

    service.record_aaaa = (mdns_record_t){.name = service.hostname_qualified,
                                          .type = MDNS_RECORDTYPE_AAAA,
                                          .data.aaaa.addr = service.address_ipv6,
                                          .rclass = 0,
                                          .ttl = 0};

    // Add two test TXT records for our service instance name, will be coalesced into
    // one record with both key-value pair strings by the library
    service.txt_record[0] = (mdns_record_t){.name = service.service_instance,
                                            .type = MDNS_RECORDTYPE_TXT,
                                            .data.txt.key = {MDNS_STRING_CONST("test")},
                                            .data.txt.value = {MDNS_STRING_CONST("1")},
                                            .rclass = 0,
                                            .ttl = 0};
    service.txt_record[1] = (mdns_record_t){.name = service.service_instance,
                                            .type = MDNS_RECORDTYPE_TXT,
                                            .data.txt.key = {MDNS_STRING_CONST("other")},
                                            .data.txt.value = {MDNS_STRING_CONST("value")},
                                            .rclass = 0,
                                            .ttl = 0};

    // Send an announcement on startup of service
    {
        log_trace("Sending announce");
        mdns_record_t additional[5] = {0};
        size_t additional_count = 0;
        additional[additional_count++] = service.record_srv;
        if (service.address_ipv4.sin_family == AF_INET)
            additional[additional_count++] = service.record_a;
        if (service.address_ipv6.sin6_family == AF_INET6)
            additional[additional_count++] = service.record_aaaa;
        additional[additional_count++] = service.txt_record[0];
        additional[additional_count++] = service.txt_record[1];

        for (int isock = 0; isock < num_sockets; ++isock)
            mdns_announce_multicast(sockets[isock], buffer, capacity, service.record_ptr, 0, 0,
                                    additional, additional_count);
    }

    // This is a crude implementation that checks for incoming queries
    while (running)
    {
        int nfds = 0;
        fd_set readfs;
        FD_ZERO(&readfs);
        for (int isock = 0; isock < num_sockets; ++isock)
        {
            if (sockets[isock] >= nfds)
                nfds = sockets[isock] + 1;
            FD_SET(sockets[isock], &readfs);
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(nfds, &readfs, 0, 0, &timeout) >= 0)
        {
            for (int isock = 0; isock < num_sockets; ++isock)
            {
                if (FD_ISSET(sockets[isock], &readfs))
                {
                    mdns_socket_listen(sockets[isock], buffer, capacity, service_callback,
                                       &service);
                }
                FD_SET(sockets[isock], &readfs);
            }
        }
        else
        {
            break;
        }
    }

    // Send a goodbye on end of service
    {
        log_trace("Sending goodbye");
        mdns_record_t additional[5] = {0};
        size_t additional_count = 0;
        additional[additional_count++] = service.record_srv;
        if (service.address_ipv4.sin_family == AF_INET)
            additional[additional_count++] = service.record_a;
        if (service.address_ipv6.sin6_family == AF_INET6)
            additional[additional_count++] = service.record_aaaa;
        additional[additional_count++] = service.txt_record[0];
        additional[additional_count++] = service.txt_record[1];

        for (int isock = 0; isock < num_sockets; ++isock)
            mdns_goodbye_multicast(sockets[isock], buffer, capacity, service.record_ptr, 0, 0,
                                   additional, additional_count);
    }

    free(buffer);
    free(service_name_buffer);

    for (int isock = 0; isock < num_sockets; ++isock)
        mdns_socket_close(sockets[isock]);
    log_trace("Closed socket%s", num_sockets ? "s" : "");

    return 0;
}

// Dump all incoming mDNS queries and answers
int dump_mdns(void)
{
    int sockets[32];
    int num_sockets = open_service_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]));
    if (num_sockets <= 0)
    {
        log_trace("Failed to open any client sockets");
        return -1;
    }
    log_trace("Opened %d socket%s for mDNS dump", num_sockets, num_sockets ? "s" : "");

    size_t capacity = 2048;
    void *buffer = malloc(capacity);

    // This is a crude implementation that checks for incoming queries and answers
    while (running)
    {
        int nfds = 0;
        fd_set readfs;
        FD_ZERO(&readfs);
        for (int isock = 0; isock < num_sockets; ++isock)
        {
            if (sockets[isock] >= nfds)
                nfds = sockets[isock] + 1;
            FD_SET(sockets[isock], &readfs);
        }

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(nfds, &readfs, 0, 0, &timeout) >= 0)
        {
            for (int isock = 0; isock < num_sockets; ++isock)
            {
                if (FD_ISSET(sockets[isock], &readfs))
                {
                    mdns_socket_listen(sockets[isock], buffer, capacity, dump_callback, 0);
                }
                FD_SET(sockets[isock], &readfs);
            }
        }
        else
        {
            break;
        }
    }

    free(buffer);

    for (int isock = 0; isock < num_sockets; ++isock)
        mdns_socket_close(sockets[isock]);
    log_trace("Closed socket%s", num_sockets ? "s" : "");

    return 0;
}

// FENRIR-WebServer._http._tcp.local. SRV fenrir-uuid-todo.local. priority 0 weight 0 port 80
static int fenrir_service_callback(int sock, const struct sockaddr *from, size_t addrlen, mdns_entry_type_t entry,
                                   uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void *data,
                                   size_t size, size_t name_offset, size_t name_length, size_t record_offset,
                                   size_t record_length, void *user_data)
{
    mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);

    size_t offset = name_offset;
    mdns_string_t name = mdns_string_extract(data, size, &offset, namebuffer, sizeof(namebuffer));

    const char *record_name = 0;
    if (rtype == MDNS_RECORDTYPE_PTR)
        record_name = "PTR";
    else if (rtype == MDNS_RECORDTYPE_SRV)
        record_name = "SRV";
    else if (rtype == MDNS_RECORDTYPE_A)
        record_name = "A";
    else if (rtype == MDNS_RECORDTYPE_AAAA)
        record_name = "AAAA";
    else if (rtype == MDNS_RECORDTYPE_TXT)
        record_name = "TXT";
    else if (rtype == MDNS_RECORDTYPE_ANY)
        record_name = "ANY";
    else
        record_name = "<UNKNOWN>";

    const char *entry_type = "Question";
    if (entry == MDNS_ENTRYTYPE_ANSWER)
        entry_type = "Answer";
    else if (entry == MDNS_ENTRYTYPE_AUTHORITY)
        entry_type = "Authority";
    else if (entry == MDNS_ENTRYTYPE_ADDITIONAL)
        entry_type = "Additional";

    log_trace("%.*s: %s %s %.*s rclass 0x%x ttl %u", MDNS_STRING_FORMAT(fromaddrstr), entry_type,
              record_name, MDNS_STRING_FORMAT(name), (unsigned int)rclass, ttl);

    find_service_t *ud = (find_service_t *)user_data;
    memcpy(&ud->addr, from, sizeof(struct sockaddr_in));
    return 0;
}

// Send a mDNS query
int find_service(const char *name, find_service_t *mdns_found_service)
{
    int sockets[32];
    int query_id[32];
    int num_sockets = open_client_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]), 0);
    if (num_sockets <= 0)
    {
        log_trace("Failed to open any client sockets");
        return -1;
    }
    log_trace("Opened %d socket%s for mDNS query", num_sockets, num_sockets ? "s" : "");

    size_t capacity = 2048;
    void *buffer = malloc(capacity);
    void *user_data = mdns_found_service;

    log_trace("Sending mDNS query");
    log_trace("");
    for (int isock = 0; isock < num_sockets; ++isock)
    {
        query_id[isock] =
            mdns_query_send(sockets[isock], MDNS_RECORDTYPE_SRV, name, strlen(name), buffer, capacity, 0);
        if (query_id[isock] < 0)
            log_trace("Failed to send mDNS query: %s", strerror(errno));
    }

    // This is a simple implementation that loops for 5 seconds or as long as we get replies
    int res;
    int ret = -1;
    log_trace("Reading mDNS query replies");
    int records = 0;
    do
    {
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        int nfds = 0;
        fd_set readfs;
        FD_ZERO(&readfs);
        for (int isock = 0; isock < num_sockets; ++isock)
        {
            if (sockets[isock] >= nfds)
                nfds = sockets[isock] + 1;
            FD_SET(sockets[isock], &readfs);
        }

        res = select(nfds, &readfs, 0, 0, &timeout);
        if (res > 0)
        {
            for (int isock = 0; isock < num_sockets; ++isock)
            {
                if (FD_ISSET(sockets[isock], &readfs))
                {
                    int rec = mdns_query_recv(sockets[isock], buffer, capacity, fenrir_service_callback,
                                              user_data, query_id[isock]);
                    if (rec > 0)
                        records += rec;

                    ret = 0;

                    socklen_t len = sizeof(struct sockaddr);
                    getsockname(sockets[isock], (struct sockaddr *)&mdns_found_service->interface_addr, &len);
                }
                FD_SET(sockets[isock], &readfs);
            }
        }
    } while (res > 0 && ret == -1);

    log_trace("Read %d records", records);

    free(buffer);

    for (int isock = 0; isock < num_sockets; ++isock)
        mdns_socket_close(sockets[isock]);
    log_trace("Closed socket%s", num_sockets ? "s" : "");

    return ret;
}