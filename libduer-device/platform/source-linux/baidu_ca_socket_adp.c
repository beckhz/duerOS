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
//
// File: baidu_ca_socket_adp.c
// Auth: Zhang Leliang (zhangleliang@baidu.com)
// Desc: Adapt the socket function to esp32.


#include "baidu_ca_adapter_internal.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "lightduer_connagent.h"
#include "lightduer_events.h"
#include "lightduer_lib.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"
#include "lightduer_priority_conf.h"
#include "lightduer_timers.h"

#define TASK_STACK_SIZE (1024 * 4)

#ifndef NON_BLOCKING
#define NON_BLOCKING    (0)
#endif

extern duer_handler my_handler;

#if defined(NON_BLOCKING) && (NON_BLOCKING == 1)
#define MSG_FLAG        (MSG_DONTWAIT)
#else
#define MSG_FLAG        (0)
#endif

typedef struct _bcasoc_s
{
    int fd;
    duer_timer_handler  _timer;
} bcasoc_t;

static duer_events_handler  g_lightduer_events = NULL;
static pthread_mutex_t    g_mutex;

static void bcasoc_lock() {
        pthread_mutex_lock(&g_mutex);
}

static void bcasoc_unlock() {
        pthread_mutex_unlock(&g_mutex);
}

static unsigned int bcasoc_parse_ip(const duer_addr_t *addr)
{
    unsigned int rs = 0;

    if (addr && addr->host) {
        DUER_LOGV("bcasoc_parse_ip: ip = %s", addr->host);
        struct hostent *hp = gethostbyname(addr->host);
        if (!hp) {
            DUER_LOGE("DNS failed!!!");
        } else {
            struct in_addr* ip4_addr;
            if (hp->h_addrtype == AF_INET) {
                ip4_addr = (struct in_addr*)hp->h_addr_list[0];
                DUER_LOGI("DNS lookup succeeded. IP=%s", inet_ntoa(*ip4_addr));
            } else if (hp->h_addrtype == AF_INET6) {
                DUER_LOGE("ipv6 address got");
            } else {
                DUER_LOGE("other type(%d) address got", hp->h_addrtype);
            }
            if (ip4_addr) {
                rs = htonl(ip4_addr->s_addr);
            }
        }
    }

    DUER_LOGV("bcasoc_parse_ip: rs = 0x%08x", rs);

    return rs;
}

static void bcasoc_check_data(int what, void *object)
{
    bcasoc_t *soc = (bcasoc_t *)object;
    int rs = 0;
    int fd = -1;

    if (!soc || soc->fd == -1) {
        return;
    }

    bcasoc_lock();
    // double check, maybe closed before get the lock
    if (!soc || soc->fd == -1) {
        return;
    }

    fd = soc->fd;

    fd_set fdread, fdex;
    //struct timeval time = {0, 0};

    FD_ZERO(&fdread);
    FD_ZERO(&fdex);

    FD_SET(fd, &fdread);
    FD_SET(fd, &fdex);

    rs = select(fd + 1, &fdread, NULL, &fdex, NULL);   // wait forever
    DUER_LOGD("select rs:%d", rs);
    if (rs > 0) {
        if (FD_ISSET(fd, &fdread)) {
            duer_data_available();
        } else if (FD_ISSET(fd, &fdex)) {
            DUER_LOGE("exception occurs %d:%s", errno, strerror(errno));
            rs = -1;
        }
    } else if (rs == 0) {
        if (soc->_timer) {
            duer_timer_start(soc->_timer, 100);
        }
    }

    bcasoc_unlock();
}

static int bcasoc_events_call(duer_events_func func, int what, void *object)
{
    if (g_lightduer_events) {
        return duer_events_call(g_lightduer_events, func, what, object);
    }
    return DUER_ERR_FAILED;
}

static void bcasoc_timer_expired(void *param)
{
    DUER_LOGD("bcasoc_timer_expired");
    bcasoc_events_call(bcasoc_check_data, 0, param);
}

void bcasoc_initialize(void)
{
    if (g_lightduer_events == NULL) {
        g_lightduer_events = duer_events_create("lightduer_socket", TASK_STACK_SIZE, 10);
    }

    pthread_mutex_init(&g_mutex, NULL);
}

duer_socket_t bcasoc_create(duer_transevt_func context)
{
    bcasoc_t *soc = DUER_MALLOC(sizeof(bcasoc_t));
    if (soc) {
        DUER_MEMSET(soc, 0, sizeof(bcasoc_t));
        soc->fd = -1;
    }
    return soc;
}

duer_status_t bcasoc_connect(duer_socket_t ctx, const duer_addr_t *addr)
{
    int rs = DUER_ERR_FAILED;
    bcasoc_t *soc = (bcasoc_t *)ctx;

    DUER_LOGV("Entry bcasoc_connect ctx = %p", ctx);

    if (soc && addr) {
        soc->fd = socket(AF_INET, addr->type == DUER_PROTO_TCP ? SOCK_STREAM : SOCK_DGRAM,
                         addr->type == DUER_PROTO_TCP ? IPPROTO_TCP : IPPROTO_UDP);
        DUER_LOGV("Result bcasoc_connect fd = %d", soc->fd);
        if (soc->fd < 0) {
            DUER_LOGE("socket create failed: %d", soc->fd);
            return soc->fd;
        }

        struct sockaddr_in addr_in;
        addr_in.sin_family = AF_INET;
        addr_in.sin_port = htons(addr->port);
        DUER_LOGV("Result bcasoc_connect: addr = %s", (char *)addr->host);
        addr_in.sin_addr.s_addr = htonl(bcasoc_parse_ip(addr));
        rs = connect(soc->fd, (struct sockaddr *)&addr_in, sizeof(addr_in));

        if (rs >= 0) {
#if defined(NON_BLOCKING) && (NON_BLOCKING == 1)
            int flags = fcntl(soc->fd, F_GETFL, 0);
            fcntl(soc->fd, F_SETFL, flags | O_NONBLOCK);
#endif
            soc->_timer = duer_timer_acquire(bcasoc_timer_expired, soc, DUER_TIMER_ONCE);

            bcasoc_events_call(bcasoc_check_data, 0, (void *)soc);
        }
    }

    DUER_LOGD("Result bcasoc_connect rs = %d", rs);

    return rs;
}

duer_status_t bcasoc_send(duer_socket_t ctx, const void *data, duer_size_t size, const duer_addr_t *addr)
{
    bcasoc_t *soc = (bcasoc_t *)ctx;
    int rs = DUER_ERR_FAILED;
    fd_set fdw;
    struct timeval tv;

    if (!soc || soc->fd == -1) {
        return rs;
    }

    FD_ZERO(&fdw);
    FD_SET(soc->fd, &fdw);

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    rs = select(soc->fd + 1, NULL, &fdw, NULL, &tv);
    if (FD_ISSET(soc->fd, &fdw)) {
        rs = send(soc->fd, data, size, MSG_FLAG);
    } else if (rs == 0) {
        DUER_LOGW("send timeout!!!");
        rs = DUER_ERR_TRANS_TIMEOUT;
    } else if (errno != EINTR) {
        rs = DUER_ERR_TRANS_INTERNAL_ERROR;
    }

    DUER_LOGD("Result bcasoc_send rs = %d", rs);
    if (rs < 0) {
        DUER_LOGE("write socket error %d:%s", errno, strerror(errno));
    }

    return rs > 0 ? size : rs;
}

duer_status_t bcasoc_recv(duer_socket_t ctx, void *data, duer_size_t size, duer_addr_t *addr)
{
    bcasoc_t *soc = (bcasoc_t *)ctx;
    int rs = DUER_ERR_FAILED;
    fd_set fdr;
    struct timeval tv;

    if (!soc || soc->fd == -1) {
        return rs;
    }

    FD_ZERO(&fdr);
    FD_SET(soc->fd, &fdr);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    rs = select(soc->fd + 1, &fdr, NULL, NULL, &tv);
    if (FD_ISSET(soc->fd, &fdr)) {
        rs = recv(soc->fd, data, size, MSG_FLAG);
    } else if (rs == 0) {
        rs = DUER_ERR_TRANS_WOULD_BLOCK;
    } else if (errno != EINTR) {
        rs = DUER_ERR_TRANS_INTERNAL_ERROR;
    }

    if (rs > 0 ) {
        bcasoc_timer_expired(soc);
    }

    if (rs < 0 && rs != DUER_ERR_TRANS_WOULD_BLOCK) {
        DUER_LOGE("read socket error %d:%s", errno, strerror(errno));
    }
    DUER_LOGD("Result bcasoc_recv rs = %d", rs);

    return rs;
}

duer_status_t bcasoc_recv_timeout(duer_socket_t ctx, void *data, duer_size_t size, duer_u32_t timeout, duer_addr_t *addr)
{
    DUER_LOGV("bcasoc_recv_timeout: ctx = %p, data = %p, size = %d, timeout = %d, addr = %p", ctx, data, size, timeout,
              addr);
    int rs = DUER_ERR_TRANS_INTERNAL_ERROR;
    bcasoc_t *soc = (bcasoc_t *)ctx;
    if (soc && soc->fd != -1) {
        fd_set fdr, fde;
        struct timeval time = {timeout / 1000, (timeout % 1000) * 1000};

        FD_ZERO(&fdr);
        FD_SET(soc->fd, &fdr);

        FD_ZERO(&fde);
        FD_SET(soc->fd, &fde);

        rs = select(soc->fd + 1, &fdr, NULL, &fde, &time);
        DUER_LOGV("bcasoc_recv_timeout select: rs = %d", rs);
        if (rs == 0) {
            rs = DUER_ERR_TRANS_TIMEOUT;
            DUER_LOGW("recv timeout!!!");
        } else if (rs < 0 || FD_ISSET(soc->fd, &fde)) {
            rs = DUER_ERR_TRANS_INTERNAL_ERROR;
            DUER_LOGW("recv failed: rs = %d", rs);
        } else if (FD_ISSET(soc->fd, &fdr)) {
            rs = bcasoc_recv(soc, data, size, addr);
        }
    }

    DUER_LOGV("bcasoc_recv_timeout: rs = %d, fd = %d", rs, soc->fd);
    return rs;
}

duer_status_t bcasoc_close(duer_socket_t ctx)
{
    bcasoc_t *soc = (bcasoc_t *)ctx;
    if (soc) {
        bcasoc_lock();
        if (soc->fd != -1) {
            close(soc->fd);
            soc->fd = -1;
        }

        if (soc->_timer) {
            duer_timer_release(soc->_timer);
            soc->_timer = NULL;
        }
        bcasoc_unlock();
    }
    return DUER_OK;
}

duer_status_t bcasoc_destroy(duer_socket_t ctx)
{
    bcasoc_t *soc = (bcasoc_t *)ctx;
    if (soc) {
        DUER_FREE(soc);
    }
    return DUER_OK;
}
