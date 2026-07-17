#define _POSIX_C_SOURCE 200809L

#include "listener.h"

#include "buffer.h"
#include "config.h"
#include "epos.h"
#include "escpos.h"
#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define READ_BUFFER_SIZE 4096

static bool handle_client(int client_fd);

bool listener_start(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in address;
    socklen_t address_length = 0;
    int reuse = 1;
    struct timeval tv = {
        .tv_sec = 1,
        .tv_usec = 0
    };

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        log_error("socket(): %s", strerror(errno));
        return false;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0)
    {
        log_warning("setsockopt(SO_REUSEADDR): %s", strerror(errno));
    }

    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)config_get_listen_port());

    if (inet_pton(AF_INET, config_get_listen_host(), &address.sin_addr) != 1)
    {
        log_error("Invalid listen address");

        close(server_fd);
        return false;
    }

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) != 0)
    {
        log_error("bind(): %s", strerror(errno));

        close(server_fd);
        return false;
    }

    if (listen(server_fd, 5) != 0)
    {
        log_error("listen(): %s", strerror(errno));

        close(server_fd);
        return false;
    }

    log_info("Listening on %s:%d", config_get_listen_host(), config_get_listen_port());

    for (;;)
    {
        address_length = sizeof(address);

        client_fd = accept(server_fd, (struct sockaddr *)&address, &address_length);

        if (client_fd < 0)
        {
            log_warning("accept(): %s", strerror(errno));
            continue;
        }
        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0)
        {
                log_warning("setsockopt(SO_RCVTIMEO): %s", strerror(errno));
        }

        handle_client(client_fd);

        close(client_fd);
    }
}

static bool handle_client(int client_fd)
{
    buffer_t escpos;
    buffer_t xml;

    uint8_t temp[READ_BUFFER_SIZE];

    ssize_t bytes_read;

    if (!buffer_init(&escpos))
        return false;

    if (!buffer_init(&xml))
    {
        buffer_free(&escpos);
        return false;
    }

    for (;;)

    {
        bytes_read = recv(client_fd, temp, sizeof(temp), 0);

        if (bytes_read == 0)
            break;

        if (bytes_read < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                log_debug("Receive timeout");
                break;
            }

            log_warning("recv(): %s", strerror(errno));

            buffer_free(&escpos);
            buffer_free(&xml);

            return false;

        }

        if (!buffer_append(&escpos, temp, (size_t)bytes_read))
        {
            buffer_free(&escpos);
            buffer_free(&xml);

            return false;

        }
    }

    log_info("Received %zu bytes", buffer_length(&escpos));

    if (buffer_length(&escpos) == 0)
    {
        buffer_free(&escpos);
        buffer_free(&xml);
        return true;
    }

    if (!escpos_parse(buffer_data(&escpos), buffer_length(&escpos), &xml))
    {
        log_error("ESC/POS parsing failed");

        buffer_free(&escpos);
        buffer_free(&xml);

        return false;
    }

    if (config_get_debug_xml())
    {
        log_debug(
                "========== BEGIN XML ==========\n"
                "%s\n"
                "=========== END XML ===========",
                buffer_data(&xml));
    }

    if (!epos_print(&xml))
    {
        log_error("Printing failed");
    }

    buffer_free(&escpos);
    buffer_free(&xml);

    return true;
}
