/**
 * Copyright (2017) Baidu Inc. All rights reserveed.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * File: lightduer_http_client_ops.c
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: HTTP Client Socket OPS
 */

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lightduer_log.h"
#include "lightduer_http_client.h"
#include "lightduer_memory.h"

static int socket_init(void* socket_args)
{
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        DUER_LOGE("socket create failed: %d", fd);
    }

    return fd;
}

static int socket_open(int socket_handle)
{
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        DUER_LOGE("socket create failed: %d", fd);
    }

    return fd;
}

static int socket_connect(int socket_handle, const char *host, const int port)
{
    int ret;
#if 0
    unsigned int rs;
    struct hostent *hp = NULL;
    struct sockaddr_in addr_in;
    struct ip4_addr *ip4_addr = NULL;

    hp = gethostbyname(host);
    if (!hp) {
        DUER_LOGE("HTTP OPS: DNS failed");
    } else {
        ip4_addr = (struct ip4_addr *)hp->h_addr;
        if (ip4_addr) {
            DUER_LOGI("HTTP OPS: DNS lookup succeeded. IP = %s", inet_ntoa(*ip4_addr));
            rs = htonl(ip4_addr->addr);
        }
    }

    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port);
    //addr_in.sin_addr.s_addr = htonl(rs);
    addr_in.sin_addr.s_addr = rs;
    ret = connect(socket_handle, (struct sockaddr *)&addr_in, sizeof(addr_in));
    if (ret == -1) {
        DUER_LOGE("HTTP OPS: Connect failed");
    } else {
        DUER_LOGE("HTTP OPS: Connected to server");

        return 0;
    }
#else
    int http_connect_flag = -1;
    struct hostent *hp = NULL;
    struct sockaddr_in sock_info;
    struct ip4_addr *ip4_addr = NULL;

    memset(&sock_info, 0, sizeof(struct sockaddr_in));

    hp = gethostbyname(host);
    if (!hp) {
        DUER_LOGE("HTTP OPS: DNS failed");
    } else {
        ip4_addr = (struct ip4_addr *)hp->h_addr;
        if (ip4_addr) {
#if defined(DUER_PLATFORM_ESPRESSIF) || defined(TARGET_UNO_91H)
            DUER_LOGI("HTTP OPS: DNS lookup succeeded. IP = %s", inet_ntoa(*ip4_addr));
#endif
        }
    }
    sock_info.sin_family = AF_INET;
#if defined(DUER_PLATFORM_ESPRESSIF) || defined(TARGET_UNO_91H)
    sock_info.sin_addr.s_addr = inet_addr(inet_ntoa(*ip4_addr));
#else
    /*
     * The LWIP which provided by mw300 was different from esp32.
     * We need to adapate to mw300 project
     */
#endif
    sock_info.sin_port = htons(port);

    http_connect_flag = connect(socket_handle, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (http_connect_flag == -1) {
        DUER_LOGE("HTTP OPS: Connect to server failed! errno=%d", http_connect_flag);
        return -1;
    } else {
        DUER_LOGE("HTTP OPS: Connected to server");

        return 0;
    }
#endif
}

static void socket_set_blocking(int socket_handle, int blocking)
{

}

static void socket_set_timeout(int socket_handle, int timeout)
{

}

static int socket_recv(int socket_handle, void* data, unsigned size)
{
    int ret = 0;

    ret = recv(socket_handle, data, size, 0);
    if (ret < 0) {
        DUER_LOGE("HTTP OPS: Recv failed %d", ret);
    }

    return ret;
}

static int socket_send(int socket_handle, const void* data, unsigned size)
{
    int ret = 0;

    ret = send(socket_handle, data, size, 0);
    if (ret == -1) {
        DUER_LOGE("HTTP OPS: Send failed %d", ret);
    } else {
        DUER_LOGE("HTTP OPS: Send succeeded");
    }

    return ret;
}

static int socket_close(int socket_handle)
{
    int ret = 0;

    ret = close(socket_handle);
    if (ret < 0) {
        DUER_LOGE("HTTP OPS: Close failed %d", ret);
    }


    return 0;
}

static void socket_destroy(int socket_handle)
{

}

static struct http_client_socket_ops ops = {
    .init         = socket_init,
    .open         = socket_open,
    .connect      = socket_connect,
    .set_blocking = socket_set_blocking,
    .set_timeout  = socket_set_timeout,
    .recv         = socket_recv,
    .send         = socket_send,
    .close        = socket_close,
    .destroy      = socket_destroy,
};

http_client_c *duer_create_http_client(void)
{
    int ret = 0;
    http_client_c *client = NULL;

    client = (http_client_c *)DUER_MALLOC(sizeof(*client));
    if (client == NULL) {
        DUER_LOGE("OTA Downloader: Malloc failed");

        goto out;
    }

    ret = duer_http_client_init(client);
    if (ret != HTTP_OK) {
        DUER_LOGE("HTTP OPS:Init http client failed");

        goto init_client_err;
    }

    ret = duer_http_client_init_socket_ops(client, &ops, NULL);
    if (ret != HTTP_OK) {
        DUER_LOGE("HTTP OPS:Init socket ops failed");

        goto init_socket_ops_err;
    }

    goto out;

init_socket_ops_err:
init_client_err:
    DUER_FREE(client);
    client = NULL;
out:
    return client;
}
