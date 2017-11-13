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
 * File: lightduer_http_client.c
 * Author: Pan Haijun, Gang Chen(chengang12@baidu.com)
 * Desc: HTTP Client
 */

#include "lightduer_http_client.h"
#include <string.h>
#include <stdbool.h>
#include "lightduer_types.h"
#include "lightduer_mutex.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"

#ifndef true
#undef true
#define true 1
#endif

#ifndef false
#undef false
#define false 0
#endif

#define HC_CHECK_ERR(p_client, ret, log_str, http_res) \
    do{ \
        if(ret) { \
            p_client->ops.close(p_client->ops.n_handle); \
            DUER_LOGE(log_str "socket error (%d)", ret); \
            return http_res; \
    } \
} while(0)

static const int HC_CR_LF_LEN               = 2; //strlen("\r\n")
static const int HC_SCHEME_LEN_MAX          = 8;
static const int HC_HOST_LEN_MAX            = 255;
static const int HC_REDIRECTION_MAX         = 30;
static const int HC_HTTP_TIME_OUT           = (2 * 1000); //TIME OUT IN MILI-SECOND
static const int HC_HTTP_TIME_OUT_TRIES     = 30;
static const int HC_REQUEST_CMD_STR_LEN_MAX = 6;
static const int HC_CHUNK_SIZE              = 1025;
static const int HC_RSP_CHUNK_SIZE          = 100;
static const int HC_HEADER_CHUNK_SIZE       = 512;
static const int HC_MAX_RETRY_RESUME_COUNT  = 10;
static const int HC_MAX_RANGE_LEN           = 10;

const char* http_request_cmd[] = {
    "GET",      //HTTP_GET
    "POST",     //HTTP_POST
    "PUT",      //HTTP_PUT
    "DELETE",   //HTTP_DELETE
    "HEAD",     //HTTP_HEAD
};

static volatile http_client_statistic_t s_http_client_statistic;
//static volatile iot_mutex_t s_log_report_mutex;

/**
 * FUNC:
 * int duer_get_http_statistic(http_client_c *p_client, socket_ops *p_ops, void *socket_args)
 *
 * DESC:
 * used to get statistic info of http module
 *
 * PARAM:
 * @param[out] statistic_result: buffer to accept the statistic info
 *
 */
#if 0
void duer_get_http_statistic(http_client_statistic_t* statistic_result) {
    DUER_LOGV("entry\n");

    if (statistic_result == NULL) {
        DUER_LOGE("Args err!\n");
        return;
    }

    if (s_log_report_mutex) {
        iot_mutex_lock(s_log_report_mutex, 0);
    }

    memmove(statistic_result, (void*)&s_http_client_statistic, sizeof(s_http_client_statistic));
    memset((void*)&s_http_client_statistic, 0, sizeof(s_http_client_statistic));

    if (s_log_report_mutex) {
        iot_mutex_unlock(s_log_report_mutex);
    }

    DUER_LOGV("leave\n");
    return;
}
#endif

/**
 * FUNC:
 * int duer_http_client_init_socket_ops(http_client_c *p_client, socket_ops *p_ops, void *socket_args)
 *
 * DESC:
 * to init http client socket operations
 *
 * PARAM:
 * @param[in] p_ops: socket operations set
 * @param[in] socket_args: args for ops "get_inst"
 *
 */
int duer_http_client_init_socket_ops(http_client_c* p_client, socket_ops* p_ops, void* socket_args) {
    DUER_LOGV("entry\n");

    if (!p_client || !p_ops) {
        DUER_LOGE("Args err!\n");
        return HTTP_FAILED;
    }

    memmove(&p_client->ops, p_ops, sizeof(socket_ops));

    if (!p_client->ops.init) {
        DUER_LOGE("http sokcet ops.init is null!\n");
        return HTTP_FAILED;
    }

    p_client->ops.n_handle = p_client->ops.init(socket_args);
    if (p_client->ops.n_handle < 0) {
        DUER_LOGE("http sokcet ops.init initialize failed!\n");
        return HTTP_FAILED;
    }

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * int duer_http_client_init(http_client_c *p_client)
 *
 * DESC:
 * to init http client
 *
 * PARAM:
 * none
 */
int duer_http_client_init(http_client_c* p_client) {
    DUER_LOGV("entry\n");
#if 0
    if (!s_log_report_mutex) {
        s_log_report_mutex = iot_mutex_create();
    }
#endif

    if (!p_client) {
        DUER_LOGE("Args err: p_client is null!\n");
        return HTTP_FAILED;
    }

    memset(p_client, 0, sizeof(http_client_c));

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * void duer_http_client_destroy(http_client_c *p_client)
 *
 * DESC:
 * to destroy http client
 *
 * PARAM:
 * none
 */
void duer_http_client_destroy(http_client_c* p_client) {
    DUER_LOGV("entry\n");

    if (!p_client) {
        DUER_LOGE("Args err: p_client is null!\n");
        return;
    }

    DUER_FREE(p_client->p_location);
    p_client->p_location = NULL;
    DUER_FREE(p_client->p_chunk_buf);
    p_client->p_chunk_buf = NULL;
    p_client->ops.destroy(p_client->ops.n_handle);

    DUER_LOGV("leave\n");
    return;
}

/**
 * FUNC:
 * void duer_http_client_set_cust_headers(http_client_c *p_client, const char **headers, size_t pairs)
 *
 * DESC:
 * Set custom headers for request.
 * Pass NULL, 0 to turn off custom headers.
 *
 * @code
 *  const char * hdrs[] =
 *         {
 *         "Connection", "keep-alive",
 *         "Accept", "text/html",
 *         "User-Agent", "Mozilla/5.0 (Windows NT 6.1; WOW64)",
 *         "Accept-Encoding", "gzip,deflate,sdch",
 *         "Accept-Language", "en-US,en;q=0.8",
 *         };
 *
 *     http.customHeaders(hdrs, 5);
 * @endcode

 * PARAM:
 * @param[in] headers: an array (size multiple of two) key-value pairs,
 *                     must remain valid during the whole HTTP session
 * @param[in] pairs: number of key-value pairs
 */
void duer_http_client_set_cust_headers(http_client_c* p_client, const char** headers, size_t pairs) {
    DUER_LOGV("entry\n");

    if (!p_client) {
        DUER_LOGE("Args err: p_client is null!\n");
        return;
    }

    p_client->pps_custom_headers = headers;
    p_client->sz_headers_count = pairs;

    DUER_LOGV("leave\n");
}

/**
 * FUNC:
 * void duer_http_client_reg_data_hdlr_cb(http_client_c *p_client, data_out_handler_cb data_hdlr_cb, void *p_usr_ctx)
 *
 * DESC:
 * register data output handler callback to handle data block
 *
 * PARAM:
 * @param[in] data_hdlr_cb: data output handler callback to be registered
 * @param[in] p_usr_ctx: args for data output handler callback to be registered
 *
 */
void duer_http_client_reg_data_hdlr_cb(http_client_c* p_client, data_out_handler_cb data_hdlr_cb,
                                  void* p_usr_ctx) {
    DUER_LOGV("entry\n");

    if (!p_client) {
        DUER_LOGE("Args err: p_client is null!\n");
        return;
    }

    p_client->data_hdlr_cb = data_hdlr_cb;
    p_client->p_data_hdlr_ctx = p_usr_ctx;

    DUER_LOGV("leave\n");
}

/**
 * FUNC:
 * void duer_http_client_reg_cb_to_get_url(http_client_c* p_client, get_url_cb_t cb)
 *
 * DESC:
 * register callback to get the url which is used by http to get data
 *
 * PARAM:
 * @param[in] pointer point to the http_client_c
 * @param[in] cb: the callback to be registered
 *
 */
void duer_http_client_reg_cb_to_get_url(http_client_c* p_client, get_url_cb_t cb) {
    DUER_LOGV("entry\n");

    if (!p_client) {
        DUER_LOGE("Args err: p_client is null!\n");
        return;
    }

    p_client->get_url_cb = cb;

    DUER_LOGV("leave\n");
}

/**
 * FUNC:
 * void duer_http_client_reg_stop_notify_cb(http_client_c *p_client, check_stop_notify_cb_t chk_stp_cb)
 *
 * DESC:
 * register callback to get stop flag
 *
 * PARAM:
 * @param[in] stp_cb: to notify httpclient to stop
 *
 */
void duer_http_client_reg_stop_notify_cb(http_client_c* p_client, check_stop_notify_cb_t chk_stp_cb) {
    DUER_LOGV("entry\n");

    if (!p_client) {
        DUER_LOGE("Args err: p_client is null!\n");
        return;
    }

    p_client->check_stop_notify_cb = chk_stp_cb;

    DUER_LOGV("leave\n");
}

/**
 * FUNC:
 * int duer_http_client_get_resp_code(http_client_c *p_client)
 *
 * DESC:
 * get http response code
 *
 * PARAM:
 * none
 */
int duer_http_client_get_resp_code(http_client_c* p_client) {
    DUER_LOGV("entry\n");

    if (!p_client) {
        DUER_LOGE("Args err: p_client is null!\n");
        return -1;
    }

    DUER_LOGV("leave\n");
    return p_client->n_http_response_code;
}

/**
 * FUNC:
 * e_http_result http_client_parse_url(const char* url, char* scheme, size_t scheme_len, \
 *                                     char* host, size_t host_len, unsigned short* port,
 *                                     \char* path, size_t path_len)
 *
 * DESC:
 * parse http url
 *
 * PARAM:
 * @param[in] url: url to be parsed
 * @param[out] scheme: protocal scheme buffer
 * @param[in] scheme_len: length of arg "scheme"
 * @param[out] host: http host name buffer
 * @param[in] host_len: length of arg "host"
 * @param[out] port: http port number
 * @param[out] path: http path buffer
 * @param[in] path_len: length of arg "path"
 * @param[in] is_relative_url: the url is a relative url or not
 *            a relative url do not have host/port, the previous url's host/port should be used
 */
static e_http_result http_client_parse_url(const char* url, char* scheme, size_t scheme_len,
        char* host, unsigned short* port,
        char* path, size_t path_len, bool is_relative_url) {
    size_t r_host_len = 0;
    size_t r_path_len = 0;
    char*  p_path     = NULL;
    char*  p_fragment = NULL;
    char*  p_host     = NULL;
    char*  p_scheme   = (char*) url;

    DUER_LOGV("entry\n");

    if (is_relative_url) {
        p_path = (char*)url;
    } else {
        p_host = (char*)strstr(url, "://");
        if (p_host == NULL) {
            DUER_LOGE("Could not find host");
            return HTTP_PARSE; //URL is invalid
        }

        //to find http scheme
        if ((int)scheme_len < p_host - p_scheme + 1) { //including NULL-terminating char
            DUER_LOGE("Scheme str is too small (%d >= %d)", scheme_len, p_host - p_scheme + 1);
            return HTTP_PARSE;
        }
        memmove(scheme, p_scheme, p_host - p_scheme);
        scheme[p_host - p_scheme] = '\0';

        //to find port
        p_host += strlen("://");
        char* p_port = strchr(p_host, ':');
        if (p_port != NULL) {
            r_host_len = p_port - p_host;
            p_port++;
            if (sscanf(p_port, "%hu", port) != 1) {
                DUER_LOGE("Could not find port");
                return HTTP_PARSE;
            }
        } else {
            *port = 0;
        }

        //to find host
        p_path = strchr(p_host, '/');
        if (!p_path) {
            DUER_LOGE("Could not find path");
            return HTTP_PARSE;
        }

        if (r_host_len == 0) {
            r_host_len = p_path - p_host;
        }

        if (r_host_len > HC_HOST_LEN_MAX) {
            DUER_LOGE("Host is too long(%d > %d)!", r_host_len, HC_HOST_LEN_MAX);
            return HTTP_PARSE;
        }

        memcpy(host, p_host, r_host_len);
        host[r_host_len] = '\0';
    }

    //to find path
    p_fragment = strchr(p_host, '#');
    if (p_fragment != NULL) {
        r_path_len = p_fragment - p_path;
    } else {
        r_path_len = strlen(p_path);
    }

    if (path_len < r_path_len + 1) { //including NULL-terminating char
        DUER_LOGE("Path str is too small (%d >= %d)", path_len, r_path_len + 1);
        return HTTP_PARSE;
    }
    memmove(path, p_path, r_path_len);
    path[r_path_len] = '\0';

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * e_http_result http_client_send(http_client_c *p_client, const char* buf, size_t len)
 *
 * DESC:
 * wrapper socket "send" to send data
 *
 * PARAM:
 * @param[in] buf: data to be sent
 * @param[in] len: data length to be sent, if it's 0, then it equals the 'buf' 's length
 */
static e_http_result http_client_send(http_client_c* p_client, const char* buf, size_t len) {
    size_t written_len = 0;
    int    ret         = 0;

    DUER_LOGV("entry\n");

    if (len == 0) {
        len = strlen(buf);
    }

    DUER_LOGD("send %d bytes:(%s)\n", len, buf);

    p_client->ops.set_blocking(p_client->ops.n_handle, true);
    ret = p_client->ops.send(p_client->ops.n_handle, buf, len);
    if (ret > 0) {
        written_len += ret;
#if 0
        iot_mutex_lock(s_log_report_mutex, 0);
        s_http_client_statistic.upload_data_size += ret;
        iot_mutex_unlock(s_log_report_mutex);
#endif
    } else if (ret == 0) {
        DUER_LOGE("Connection was closed by server");
        return HTTP_CLOSED;
    } else {
        DUER_LOGE("Connection error (send returned %d)", ret);
        return HTTP_CONN;
    }

    DUER_LOGD("Written %d bytes", written_len);

    DUER_LOGV("entry\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * e_http_result http_client_recv(http_client_c *p_client, char* buf, size_t maxLen, \
 *                                size_t* p_read_len, int to_try)
 *
 * DESC:
 * wrapper socket "recv" to receive data
 *
 * PARAM:
 * @param[in] buf: a buffer to store received data
 * @param[in] max_len: max size needed to read
 * @param[out] p_read_len: really size the data is read
 * @param[in] to_try: check to try or not when timeout
 */
static e_http_result http_client_recv(http_client_c* p_client, char* buf, size_t max_len,
                                      size_t* p_read_len, int to_try) {
    int    ret             = 0;
    int    n_try_times     = 0;
    size_t start_timestamp = 0;
    size_t time_consume    = 0;
    size_t sz_read_len     = 0;
    e_http_result res      = HTTP_OK;

    DUER_LOGV("entry\n");

    DUER_LOGV("Trying to read %d bytes", max_len);

    //start_timestamp = us_ticker_read();

    while (sz_read_len < max_len) {
        DUER_LOGD("Trying(%d ) times to read at most %4d bytes [Not blocking] %d,%d",
                  n_try_times, max_len - sz_read_len, max_len, sz_read_len);
        p_client->ops.set_blocking(p_client->ops.n_handle, true);
        p_client->ops.set_timeout(p_client->ops.n_handle, HC_HTTP_TIME_OUT);
        ret = p_client->ops.recv(p_client->ops.n_handle, buf + sz_read_len, max_len - sz_read_len);

        if (ret > 0) {
            sz_read_len += ret;
#if 0
            iot_mutex_lock(s_log_report_mutex, 0);
            s_http_client_statistic.download_data_size += ret;
            iot_mutex_unlock(s_log_report_mutex);
#endif
        } else if (ret == 0) {
            break;
#if 0 //in block mode no need to try again
            n_try_times ++;
            if (n_try_times >= 3) {
                break;
            }
            wait_ms(20);
#endif
        } else {
#if 0
            DUER_LOGD("http recv error:%d", ret);
             if (ret == NSAPI_ERROR_WOULD_BLOCK) {
                if (!to_try) {
                    break;
                }

                n_try_times++;

                if (n_try_times >= HC_HTTP_TIME_OUT_TRIES) {
                    res = HTTP_TIMEOUT;
                    break;
                }
            } else {
                res = HTTP_CONN;
                break;
            }
#endif
                res = HTTP_CONN;
                break;
        }
    }

    DUER_LOGV("Read %d bytes", sz_read_len);

    buf[sz_read_len] = '\0';
    *p_read_len = sz_read_len;
    //time_consume = us_ticker_read() - start_timestamp;

#if 0
    iot_mutex_lock(s_log_report_mutex, 0);
    if (time_consume > s_http_client_statistic.recv_packet_longest_time) {
        s_http_client_statistic.recv_packet_longest_time = time_consume;
    }
    iot_mutex_unlock(s_log_report_mutex);
#endif

    DUER_LOGV("leave\n");
    return res;
}

/**
 * FUNC:
 * e_http_result http_client_send_request(http_client_c *p_client, e_http_meth method, char *path,
 *                                        char *host, unsigned short port)
 * DESC:
 * send http request header
 *
 * PARAM:
 * @param[in] method: method to comunicate with server
 * @param[in] path: resource path
 * @param[in] host: server host
 * @param[in] port: server port
 */
static e_http_result http_client_send_request(http_client_c* p_client, e_http_meth method,
        char* path, char* host, unsigned short port) {
    int   ret           = 0;
    int   req_fmt_len   = 0;
    char* p_request_buf = NULL;

    DUER_LOGV("entry");

    req_fmt_len = strlen("%s %s HTTP/1.1\r\nHost: %s\r\nRange: bytes=%d-\r\n");
    req_fmt_len += HC_REQUEST_CMD_STR_LEN_MAX + strlen(path) + strlen(host) + HC_MAX_RANGE_LEN;

    p_request_buf = (char*)DUER_MALLOC(req_fmt_len);
    if (!p_request_buf) {
        DUER_LOGE("Mallco memory(size:%d) for request buffer Failed!\n", req_fmt_len);
        return HTTP_FAILED;
    }

    memset(p_request_buf, 0, req_fmt_len);
    snprintf(p_request_buf, req_fmt_len, "%s %s HTTP/1.1\r\nHost: %s\r\nRange: bytes=%d-\r\n",
                http_request_cmd[method], path, host, p_client->recv_size);

    DUER_LOGD(" request buffer %d bytes{%s}", req_fmt_len, p_request_buf);

    ret = http_client_send(p_client, p_request_buf, 0);
    DUER_FREE(p_request_buf);
    if (ret != HTTP_OK) {
        p_client->ops.close(p_client->ops.n_handle);
        DUER_LOGD("Could not write request");
        return HTTP_CONN;
    }

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * e_http_result http_client_send_headers(http_client_c *p_client, char *buff, int buff_size)
 *
 * DESC:
 * send all http headers
 *
 * PARAM:
 * @param[in] buff: provide buffer to send headers
 * @param[in] buff_size: 'buff' 's size
 */
static e_http_result http_client_send_headers(http_client_c* p_client, char* buff, int buff_size) {
    size_t nh         = 0;
    int    header_len = 0;
    e_http_result ret = HTTP_OK;

    DUER_LOGV("enrty\n");

    DUER_LOGD("Send custom header(s) %d (if any)", p_client->sz_headers_count);

    for (nh = 0; nh < p_client->sz_headers_count * 2; nh += 2) {
        DUER_LOGD("hdr[%2d] %s: %s", nh, p_client->pps_custom_headers[nh],
                  p_client->pps_custom_headers[nh + 1]);

        header_len = strlen("%s: %s\r\n") + strlen(p_client->pps_custom_headers[nh]) \
                     + strlen(p_client->pps_custom_headers[nh + 1]);
        if (header_len >= buff_size) {
            DUER_LOGD("Http header (%d) is too long!", header_len);
            p_client->ops.close(p_client->ops.n_handle);
            return HTTP_FAILED;
        }

        snprintf(buff, buff_size, "%s: %s\r\n", p_client->pps_custom_headers[nh],
                    p_client->pps_custom_headers[nh + 1]);
        ret = http_client_send(p_client, buff, 0);
        HC_CHECK_ERR(p_client, ret, "Could not write headers", HTTP_CONN);
        DUER_LOGD("send() returned %d", ret);
    }

    //Close headers
    DUER_LOGD("Close Http Headers");
    ret = http_client_send(p_client, "\r\n", 0);
    HC_CHECK_ERR(p_client, ret, "Close http headers failed!", ret);

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * void http_client_set_content_len(http_client_c *p_client, int content_len)
 *
 * DESC:
 * set http content length
 *
 * PARAM:
 * @param[in] content_len: http content lenght to be set
 */
static void http_client_set_content_len(http_client_c* p_client, int content_len) {
    DUER_LOGV("enrty\n");
    p_client->n_http_content_len = content_len;
    DUER_LOGV("leave\n");
}

/**
 * FUNC:
 * void http_client_set_content_type(http_client_c* p_client, char* p_content_type)
 *
 * DESC:
 * set http content type
 *
 * PARAM:
 * @param[in] p_content_type: http content type to be set
 */
static void http_client_set_content_type(http_client_c* p_client, char* p_content_type) {
    DUER_LOGV("enrty\n");

    if (p_content_type) {
        snprintf(p_client->p_http_content_type, HC_CONTENT_TYPE_LEN_MAX, "%s", p_content_type);
        if (strlen(p_content_type) >= HC_CONTENT_TYPE_LEN_MAX) {
            p_client->p_http_content_type[HC_CONTENT_TYPE_LEN_MAX - 1] = '\0';
        }
    }

    DUER_LOGV("leave\n");
}

/**
 * FUNC:
 * e_http_result http_client_recv_rsp_and_parse(http_client_c *p_client, char* buf,
 *                                              size_t max_len, size_t* p_read_len)
 * DESC:
 * receive http response and parse it
 *
 * PARAM:
 * @param[in/out] buf: [in]pass a buffer to store received data; [out]to store the remaining received data except http response
 * @param[in] max_len: max size needed to read
 * @param[out] p_read_len: the remaining size of the data in 'buf'
 */
static e_http_result http_client_recv_rsp_and_parse(http_client_c* p_client, char* buf,
        size_t max_len, size_t* p_read_len) {
    int    ret       = 0;
    size_t cr_lf_pos = 0;
    size_t rev_len   = 0;
    char*  p_cr_lf   = NULL;

    DUER_LOGV("enrty\n");

    ret = http_client_recv(p_client, buf, max_len, &rev_len, 1);
    HC_CHECK_ERR(p_client, ret, "Receiving  http response error!", HTTP_CONN);

    buf[rev_len] = '\0';
    DUER_LOGD("Received: \r\n(%s\r\n)", buf);

    p_cr_lf = strstr(buf, "\r\n");
    if (!p_cr_lf) {
        HC_CHECK_ERR(p_client, HTTP_PRTCL, "Response Protocol error", HTTP_PRTCL);
    }
    cr_lf_pos = p_cr_lf - buf;
    buf[cr_lf_pos] = '\0';

    //to parse HTTP response
    if (sscanf(buf, "HTTP/%*d.%*d %d %*[^\r\n]", &p_client->n_http_response_code) != 1) {
        //cannot match string, error
        DUER_LOGE("Not a correct HTTP answer : {%s}\n", buf);
        HC_CHECK_ERR(p_client, HTTP_PRTCL, "Response Protocol error", HTTP_PRTCL);
    }

    if ((p_client->n_http_response_code < 200) || (p_client->n_http_response_code >= 400)) {
        //Did not return a 2xx code;
        //TODO fetch headers/(&data?) anyway and implement a mean of writing/reading headers
        DUER_LOGD("Response code %d", p_client->n_http_response_code);
        HC_CHECK_ERR(p_client, HTTP_PRTCL, "Response Protocol error", HTTP_PRTCL);
    }

    rev_len -= (cr_lf_pos + HC_CR_LF_LEN);
    //Be sure to move NULL-terminating char as well
    memmove(buf, &buf[cr_lf_pos + HC_CR_LF_LEN], rev_len + 1);

    if (p_read_len) {
        *p_read_len = rev_len;
    }

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * e_http_result http_client_recv_headers_and_parse(http_client_c *p_client, char* buf,
 *                                                  size_t max_len, size_t* p_read_len)
 * DESC:
 * receive http response and parse it
 *
 * PARAM:
 * @param[in/out] buf: [in]pass a buffer to store received data;
 *                     [out]to store the remaining received data except http response
 * @param[in] max_len: max size needed to read each time
 * @param[in/out] p_read_len: [in&out]the remaining size of the data in 'buf'
 */
static e_http_result http_client_recv_headers_and_parse(http_client_c* p_client, char* buf,
        size_t max_len, size_t* p_read_len) {
    int    i             = 0;
    char*  key           = NULL;
    char*  value         = NULL;
    char*  p_cr_lf       = NULL;
    size_t len           = 0;
    size_t cr_lf_pos     = 0;
    size_t rev_len       = 0;
    size_t new_rev_len   = 0;
    e_http_result res    = HTTP_OK;
    int http_content_len = 0;

    DUER_LOGV("enrty\n");

    HC_CHECK_ERR(p_client, !p_read_len, "arg 'p_read_len' is null!", HTTP_FAILED);

    rev_len = *p_read_len;
    p_client->n_is_chunked = false;

    while (true) {
        p_cr_lf = strstr(buf, "\r\n");

        if (p_cr_lf == NULL) {
            if (rev_len < max_len) {
                res = http_client_recv(p_client, buf + rev_len, max_len - rev_len,
                                       &new_rev_len, 0);
                rev_len += new_rev_len;
                buf[rev_len] = '\0';
                DUER_LOGD("Receive %d chars in buf: \n[%s]", new_rev_len, buf);
                HC_CHECK_ERR(p_client, res, "Receiving  http headers error!", res);
                continue;
            } else {
                HC_CHECK_ERR(p_client, HTTP_PRTCL, "Headers Protocol error", HTTP_PRTCL);
            }
        }

        cr_lf_pos = p_cr_lf - buf;

        //end of headers to break
        if (cr_lf_pos == 0) {
            DUER_LOGD("All Http Headers readed over!");
            rev_len -= HC_CR_LF_LEN;
            //Be sure to move NULL-terminating char as well
            memmove(buf, &buf[HC_CR_LF_LEN], rev_len + 1);
            break;
        }

        key = NULL;
        value = NULL;
        buf[cr_lf_pos] = '\0';

        // parse the header
        for (i = 0; i < cr_lf_pos; i++) {
            if (buf[i] == ':') {
                buf[i] = '\0';
                key = buf;
                value = &buf[i + 1];
                break;
            }
        }

        if ((key != NULL) && (value != NULL)) {
            while ((*value != '\0') && (*value == ' ')) {
                value++;
            }

            DUER_LOGD("Read header (%s: %s)", key, value);

            if (!strncmp(key, "Content-Length", max_len)) {
                sscanf(value, "%d", &http_content_len);
                http_client_set_content_len(p_client, http_content_len + p_client->recv_size);
                DUER_LOGI("Total size of the data need to read: %d bytes",
                          http_content_len + p_client->recv_size);
            } else if (!strncmp(key, "Transfer-Encoding", max_len)) {
                if (!strncmp(value, "Chunked", max_len - (value - buf))
                        || !strncmp(value, "chunked",  max_len - (value - buf))) {
                    //HC_CHECK_ERR(p_client, HTTP_NOT_SUPPORT, "Chunked Header not supported!",
                    //             HTTP_NOT_SUPPORT);
                    p_client->n_is_chunked = true;

                    DUER_LOGD("Http client data is chunked transfored\n");

                    p_client->p_chunk_buf = (char*)DUER_MALLOC(HC_CHUNK_SIZE);
                    HC_CHECK_ERR(p_client, !p_client->p_chunk_buf,
                                 "Malloc memory failed for p_chunk_buf!", HTTP_FAILED);
                }
            } else if (!strncmp(key, "Content-Type", max_len)) {
                http_client_set_content_type(p_client, value);
            } else if (!strncmp(key, "Location", max_len)) {
                DUER_FREE(p_client->p_location);
                p_client->p_location = NULL;
                len = strlen(value);
                p_client->p_location = (char*)DUER_MALLOC(len + 1);
                HC_CHECK_ERR(p_client, !p_client->p_location,
                             "Malloc memory failed for p_location!", HTTP_FAILED);

                snprintf(p_client->p_location, len + 1, "%s", value);

                HC_CHECK_ERR(p_client, HTTP_REDIRECTTION,
                             "Taking http redirection!", HTTP_REDIRECTTION);
            } else {
                // do nothing
            }

            rev_len -= (cr_lf_pos + HC_CR_LF_LEN);
            //Be sure to move NULL-terminating char as well
            memmove(buf, &buf[cr_lf_pos + HC_CR_LF_LEN], rev_len + 1);
        } else {
            HC_CHECK_ERR(p_client, HTTP_PRTCL, "Could not parse header", HTTP_PRTCL);
        }
    }

    *p_read_len = rev_len;

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * e_http_result http_client_recv_chunked_size(http_client_c *p_client, char* buf,
 *                                            size_t max_len, size_t* p_read_len)
 * DESC:
 * receive http chunked size, and save it into http_client_c's 'sz_chunk_size'
 *
 * PARAM:
 * @param[out] p_client: to store chunk size in "p_client->sz_chunk_size"
 * @param[in/out] buf: [in]pass a buffer to store received data;
 *                     [out]to store the remaining received data except http response
 * @param[in] max_len: max size needed to read each time
 * @param[in/out] p_read_len: [in&out]the remaining size of the data in 'buf'
 */
static e_http_result http_client_recv_chunked_size(http_client_c* p_client, char* buf,
        size_t max_len, size_t* p_read_len) {
    int    n           = 0;
    int    found_cr_lf = false;
    size_t rev_len     = 0;
    size_t new_rev_len = 0;
    size_t cr_lf_pos   = 0;
    e_http_result res  = HTTP_OK;

    DUER_LOGV("enrty\n");

    HC_CHECK_ERR(p_client, !p_read_len, "arg 'p_read_len' is null!", HTTP_FAILED);
    DUER_LOGD("args:max_len=%d, *p_read_len=%d", max_len, *p_read_len);

    rev_len = *p_read_len;

    while (!found_cr_lf) {
        // to check chunked size
        if (rev_len >= HC_CR_LF_LEN) {
            for (cr_lf_pos = 0; cr_lf_pos < rev_len - 2; cr_lf_pos++) {
                if (buf[cr_lf_pos] == '\r' && buf[cr_lf_pos + 1] == '\n') {
                    found_cr_lf = true;
                    DUER_LOGD("Found cr&lf at index %d", cr_lf_pos);
                    break;
                }
            }
        }

        if (found_cr_lf) {
            break;
        }

        //Try to read more
        if (rev_len < max_len) {
            res = http_client_recv(p_client, buf + rev_len, max_len - rev_len, &new_rev_len, 0);
            rev_len += new_rev_len;
            buf[rev_len] = '\0';
            DUER_LOGD("Receive %d chars in buf: \n[%s]", new_rev_len, buf);
            HC_CHECK_ERR(p_client, res, "Receiving  http chunk headers error!", res);
        } else {
            HC_CHECK_ERR(p_client, HTTP_PRTCL,
                         "Get http chunked size failed:not found cr lf in size line!", HTTP_PRTCL);
        }
    }

    buf[cr_lf_pos] = '\0';
    DUER_LOGD("found_cr_lf=%d, cr_lf_pos=%d, buf=%s", found_cr_lf, cr_lf_pos, buf);

    //read chunked size
    n = sscanf(buf, "%x", &p_client->sz_chunk_size);
    if (n != 1) {
        HC_CHECK_ERR(p_client, HTTP_PRTCL,
                     "Get http chunked size failed: Could not read chunk length!", HTTP_PRTCL);
    }

    DUER_LOGD("HTTP chunked size:%d\n", p_client->sz_chunk_size);

    rev_len -= (cr_lf_pos + HC_CR_LF_LEN);
    memmove(buf, &buf[cr_lf_pos + HC_CR_LF_LEN],
               rev_len + 1); //Be sure to move NULL-terminating char as well
    *p_read_len = rev_len;

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * static e_http_result http_client_recv_chunk(http_client_c *p_client, char* buf,
 *                                             size_t max_len, size_t read_len)
 * DESC:
 * receive http chunk data
 * PARAM:
 * @param[in] buf: pass a buffer to store received data with 'data_len_in_buf' data in it at first
 * @param[in] max_len: max size needed to read each time
 * @param[in] data_len_in_buf: data remained in 'buf' at first
 */
static e_http_result http_client_recv_chunk(http_client_c* p_client, char* buf,
        size_t max_len, size_t data_len_in_buf) {
    size_t need_to_read   = 0;
    size_t rev_len        = 0;
    size_t temp           = 0;
    size_t new_rev_len    = 0;
    size_t chunk_buff_len = 0;
    size_t max_read_len   = 0;
    e_http_result res     = HTTP_OK;

    DUER_LOGV("enrty\n");

    DUER_LOGD("args:max_len=%d, data_len_in_buf=%d", max_len, data_len_in_buf);

    if (data_len_in_buf > max_len) {
        DUER_LOGE("Args err, data over buffer(%d > %d):", data_len_in_buf, max_len);
        HC_CHECK_ERR(p_client, HTTP_FAILED, "Args err!", HTTP_FAILED);
    }
    rev_len = data_len_in_buf;

    while (true) {
        if (p_client->check_stop_notify_cb && p_client->check_stop_notify_cb()) {
            DUER_LOGI("Stopped getting media data from url by stop flag!\n");
            goto RET;
        }

        res = http_client_recv_chunked_size(p_client, buf, max_len, &rev_len);
        if (res != HTTP_OK) {
            DUER_LOGE("Receiving http chunked size error:res=%d", res);
            goto RET;
        }

        DUER_LOGD("*************HTTP chunk size is %d************\n", p_client->sz_chunk_size);

        //last chunk to return
        if (p_client->sz_chunk_size == 0) {
            DUER_LOGD("Receive the last HTTP chunk!!!");

            //to flush the last chunk buffer in the "RET"
            if (chunk_buff_len) {
                //add NULL terminate for print string
                p_client->p_chunk_buf[chunk_buff_len] = '\0';
            }
            goto RET;
        }

        //when chunk size is less than max_len of "buf"
        if (p_client->sz_chunk_size  + HC_CR_LF_LEN <= max_len) {
            if (rev_len < p_client->sz_chunk_size + HC_CR_LF_LEN) {
                if (p_client->check_stop_notify_cb && p_client->check_stop_notify_cb()) {
                    DUER_LOGI("Stopped getting media data from url by stop flag!\n");
                    goto RET;
                }

                res = http_client_recv(
                              p_client,
                              buf + rev_len,
                              max_len - rev_len,
                              &new_rev_len,
                              1);
                rev_len += new_rev_len;
                buf[rev_len] = '\0';
                DUER_LOGD("Receive %d chars in buf: \n[%s]", new_rev_len, buf);

                if (res != HTTP_OK) {
                    DUER_LOGE("Receiving  http chunked data error:res=%d", res);
                    goto RET;
                }
            }

            if (rev_len < p_client->sz_chunk_size + HC_CR_LF_LEN) {
                res = HTTP_PRTCL;
                DUER_LOGE("No CR&LF after http data chunk: HTTP_PRTCL");
                goto RET;
            }

            //when received data is more than a chunk with "\r\n"
            if (rev_len >= p_client->sz_chunk_size + HC_CR_LF_LEN) {
                //for -1 reserve the last bytes to do nothing in p_chunk_buf
                if (chunk_buff_len + p_client->sz_chunk_size >= HC_CHUNK_SIZE - 1) {
                    temp = HC_CHUNK_SIZE - 1 - chunk_buff_len;
                    memmove(p_client->p_chunk_buf + chunk_buff_len, buf, temp);
                    //add NULL terminate for print string
                    p_client->p_chunk_buf[HC_CHUNK_SIZE - 1] = '\0';
                    chunk_buff_len += temp;
                    //call callback to output

                    if (p_client->data_hdlr_cb) {
                        p_client->data_hdlr_cb(p_client->p_data_hdlr_ctx, p_client->data_pos,
                                               p_client->p_chunk_buf, chunk_buff_len,
                                               p_client->p_http_content_type);
                        p_client->recv_size += chunk_buff_len;
                    }
                    p_client->data_pos = DATA_MID;
                    memmove(p_client->p_chunk_buf, buf + temp, p_client->sz_chunk_size - temp);
                    chunk_buff_len = p_client->sz_chunk_size - temp;
                } else {
                    memmove(p_client->p_chunk_buf + chunk_buff_len, buf,
                               p_client->sz_chunk_size);
                    chunk_buff_len += p_client->sz_chunk_size;
                }

                //remove "\r\n"
                if (buf[p_client->sz_chunk_size] != '\r'
                        || buf[p_client->sz_chunk_size + 1] != '\n') {
                    res = HTTP_PRTCL;
                    DUER_LOGE("No CR&LF after http data chunk when remove cr&lf: HTTP_PRTCL");
                    goto RET;
                }

                rev_len -= (p_client->sz_chunk_size + HC_CR_LF_LEN);
                //Be sure to move NULL-terminating char as well
                memmove(buf, &buf[p_client->sz_chunk_size + HC_CR_LF_LEN], rev_len + 1);
            }
        } else { //when chunk size with "\r\n" is more than max_len of "buf"
            DUER_LOGD("Here to read large HTTP chunk (%d) bytes!", p_client->sz_chunk_size);
            if (p_client->sz_chunk_size <= rev_len) {
                res = HTTP_FAILED;
                DUER_LOGE("Receiving  http large chunked data error: sz_chunk_size <= rev_len!");
                goto RET;
            }

            //read a large chunk data, (1)read all chunk data first
            need_to_read = p_client->sz_chunk_size - rev_len;

            while (need_to_read) {
                if (p_client->check_stop_notify_cb && p_client->check_stop_notify_cb()) {
                    DUER_LOGI("Stopped getting media data from url by stop flag!\n");
                    goto RET;
                }

                if (max_len - rev_len > need_to_read) {
                    max_read_len = need_to_read;
                } else {
                    max_read_len = max_len - rev_len;
                }

                res = http_client_recv(p_client, buf + rev_len, max_read_len, &new_rev_len, 1);
                rev_len += new_rev_len;
                need_to_read -= new_rev_len;
                buf[rev_len] = '\0';
                DUER_LOGD("Receive %d chars in buf: \n", new_rev_len);

                if (res != HTTP_OK) {
                    DUER_LOGE("Receiving  http large chunk data error:res=%d", res);
                    goto RET;
                }

                //for sub 1 reserve '\0' the last bytes to do nothing in p_chunk_buf
                if (chunk_buff_len + rev_len >= HC_CHUNK_SIZE - 1) {
                    temp = HC_CHUNK_SIZE - 1 - chunk_buff_len;
                    memmove(p_client->p_chunk_buf + chunk_buff_len, buf, temp);
                    //add NULL terminate for print string
                    p_client->p_chunk_buf[HC_CHUNK_SIZE - 1] = '\0';
                    chunk_buff_len += temp;
                    //call callback to output

                    if (p_client->data_hdlr_cb) {
                        //wait_ms(1000); use for latency test
                        p_client->data_hdlr_cb(p_client->p_data_hdlr_ctx, p_client->data_pos,
                                               p_client->p_chunk_buf, chunk_buff_len,
                                               p_client->p_http_content_type);
                        p_client->recv_size += chunk_buff_len;
                    }
                    p_client->data_pos = DATA_MID;
                    memmove(p_client->p_chunk_buf, buf + temp, rev_len - temp);
                    chunk_buff_len  = rev_len - temp;
                } else {
                    memmove(p_client->p_chunk_buf + chunk_buff_len, buf, rev_len);
                    chunk_buff_len += rev_len;
                }
                rev_len = 0;
            }

            DUER_LOGD("Read a large HTTP chunk (%d) bytes finished", p_client->sz_chunk_size);
            //(2)to receive "\r\n" and remove it
            DUER_LOGD("receive cr&lf and remove it");

            res = http_client_recv(p_client, buf, HC_CR_LF_LEN, &new_rev_len, 1);
            buf[new_rev_len] = '\0';

            DUER_LOGD("Receive %d chars in buf: \n[0x%x, 0x%x]", new_rev_len, buf[0], buf[1]);

            if (res != HTTP_OK) {
                DUER_LOGE("Receiving cr&lf http large chunked data error:res=%d", res);
                goto RET;
            }

            if ((new_rev_len != HC_CR_LF_LEN) || buf[0] != '\r' || buf[1] != '\n') {
                res = HTTP_PRTCL;
                DUER_LOGE("No CR&LF after http large data chunk:HTTP_PRTCL");
                goto RET;
            }
            // to reset rev_len when run here
            rev_len  = 0;
        }
    }//end of while(true)

RET:
    if (res == HTTP_OK) {
        p_client->data_pos = DATA_MID;
        if (p_client->data_hdlr_cb) {
            p_client->data_hdlr_cb(p_client->p_data_hdlr_ctx,
                                   p_client->data_pos,
                                   p_client->p_chunk_buf,
                                   chunk_buff_len,
                                   p_client->p_http_content_type);
            p_client->recv_size += chunk_buff_len;
        }
    }

    DUER_LOGV("leave\n");
    return res;
}

/**
 * FUNC:
 * static e_http_result http_client_recv_content_data(http_client_c *p_client, char* buf,
 *                                                    size_t max_len, size_t read_len)
 * DESC:
 * receive http data
 *
 * PARAM:
 * @param[in] buf: [in]pass a buffer to store received data with 'data_len_in_buf' data in it at first
 * @param[in] max_len: max size needed to read each time
 * @param[in] data_len_in_buf: data remained in 'buf' at first
 */
static e_http_result http_client_recv_content_data(http_client_c* p_client, char* buf,
        size_t max_len, size_t data_len_in_buf) {
    size_t need_to_read = p_client->n_http_content_len - p_client->recv_size;
    size_t rev_len      = data_len_in_buf;
    size_t new_rev_len  = 0;
    e_http_result res   = HTTP_OK;

    DUER_LOGV("enrty\n");

    if (data_len_in_buf > max_len || data_len_in_buf > need_to_read) {
        DUER_LOGE("Args err, data over buffer(%d > %d or  %d):",
                  data_len_in_buf, max_len, need_to_read);

        HC_CHECK_ERR(p_client, HTTP_FAILED, "Args err!", HTTP_FAILED);
    }
    need_to_read -= rev_len;

    while (need_to_read || rev_len) {
        if (p_client->check_stop_notify_cb && p_client->check_stop_notify_cb()) {
            DUER_LOGI("Stopped getting media data from url by stop flag!\n");
            break;
        }

        DUER_LOGD("lefting %d bytes to read for all %d bytes(%f)", need_to_read,
                  p_client->n_http_content_len, 1.0 * need_to_read / p_client->n_http_content_len);

        res = http_client_recv(
                      p_client, buf + rev_len,
                      (max_len - rev_len) > need_to_read ? need_to_read : (max_len - rev_len),
                      &new_rev_len,
                      1);

        rev_len += new_rev_len;
        need_to_read -= new_rev_len;
        buf[rev_len] = '\0';

        DUER_LOGV("Receive %d bytes total %d bytes in buf: \n[%s]", new_rev_len, rev_len, buf);

        if (rev_len) {
            if (!p_client->data_hdlr_cb) {
                DUER_LOGE("Data handler callback is NULL!");
                res = HTTP_FAILED;
                goto RET;
            }

            // PERFORM_STATISTIC_END(PERFORM_STATISTIC_GET_MEDIA_BY_URL, 0,
                                 // UNCONTINUOUS_TIME_MEASURE);
            // PERFORM_STATISTIC_BEGIN(PERFORM_STATISTIC_PLAY_URL_MEDIA, MAX_TIME_CALCULATE_COUNT);
            p_client->data_hdlr_cb(p_client->p_data_hdlr_ctx, p_client->data_pos, buf,
                                   rev_len, p_client->p_http_content_type);
            p_client->recv_size += rev_len;
            // PERFORM_STATISTIC_END(PERFORM_STATISTIC_PLAY_URL_MEDIA, rev_len,
            //                     UNCONTINUOUS_TIME_MEASURE);
            p_client->data_pos = DATA_MID;
        }

        if (res != HTTP_OK) {
            DUER_LOGE("Receive http data finished !errcode= %d when remaining %d bytes to receive",
                      res, need_to_read);
            break;
        }

        if (need_to_read > 0 && rev_len == 0) {
            DUER_LOGE("Remaining %d bytes cannot receive,and give up\n", need_to_read);
            res = HTTP_FAILED;
            break;
        }

        rev_len = 0;
    }

RET:
    DUER_LOGV("leave\n");
    return res;
}

/**
 * DESC:
 * get http download progress
 *
 * PARAM:
 * @param[out] total_size: to receive the total size to be download
 *                         If chunked Transfer-Encoding is used, we cannot know the total size
 *                         untile download finished, in this case the total size will be 0
 * @param[out] recv_size:  to receive the data size have been download
 *
 * @return     none
 */
void duer_http_client_get_download_progress(http_client_c* p_client,
                                       size_t* total_size,
                                       size_t* recv_size) {
    if (!p_client || !total_size || !recv_size) {
        DUER_LOGE("Args error!\n");
        return;
    }

    *total_size = p_client->n_http_content_len;
    *recv_size = p_client->recv_size;
}

/**
 * FUNC:
 * e_http_result http_client_parse_redirect_location(http_client_c *p_client, const char *p_path,
 *                                                   bool *is_relative_url)
 * DESC:
 * parse the redirect location, create the new path if it is a relative url
 *
 * PARAM:
 * @param[in] p_path: the path of the old url
 * @param[in] path_len: the max length of path
 * @param[out] is_relative_url: return whether it is a relative url
 */
static e_http_result http_client_parse_redirect_location(http_client_c* p_client,
        const char* p_path, const size_t path_len, bool* is_relative_url) {
    char*  new_path = NULL;
    size_t len      = 0;

    DUER_LOGV("enrty\n");

    if ((!p_client) || (!is_relative_url) || (!p_path)) {
        DUER_LOGE("Invalid param!\n");
        return HTTP_PARSE;
    }

    if (!p_client->p_location) {
        DUER_LOGE("Invalid location!\n");
        return HTTP_PARSE;
    }

    if (p_client->p_location[0] == 'h') {
        *is_relative_url = false;
        return HTTP_OK;
    }

    *is_relative_url = true;

    // the relative path's format may be "./path", "../path"
    if (p_client->p_location[0] != '/') {
        // one more byte for '/', one more byte for '\0'
        len = strlen(p_path) + strlen(p_client->p_location) + 2;
        new_path = DUER_MALLOC(len);

        if (!new_path) {
            DUER_LOGE("No enough memory!\n");
            return HTTP_PARSE;
        }
        snprintf(new_path, len, "%s", p_path);

        // for example: the previous path is "p1/p2", the relative path is "../",
        // the new path should be "p1/p2/../", we should add a '/'
        if (p_path[strlen(p_path)] != '/') {
            strncat(new_path, "/", 1);
        }

        strncat(new_path, p_client->p_location, HC_CHUNK_SIZE);
        DUER_LOGI("new path: %s\n", new_path);
        DUER_FREE(p_client->p_location);
        p_client->p_location = NULL;
        p_client->p_location = new_path;
    }

    DUER_LOGV("leave\n");
    return HTTP_OK;
}

/**
 * FUNC:
 * e_http_result http_client_connect(const char* url, e_http_meth method)
 *
 * DESC:
 * connect to the given url
 *
 * PARAM:
 * @param[in] url: url to be connected
 * @param[in] method: method to comunicate with server
 */
static e_http_result http_client_connect(http_client_c* p_client, const char* url,
        e_http_meth method, size_t* remain_size_in_buf) {
    char   scheme[HC_SCHEME_LEN_MAX];
    char*  p_path          = NULL;
    char*  host            = NULL;
    bool   is_relative_url = false;
    int    path_len_max    = 0;
    int    ret             = 0;
    int    redirect_count  = 0;
    size_t rev_len         = 0;
    unsigned short port    = 0;
    e_http_result  res     = HTTP_OK;

    DUER_LOGV("enrty\n");

    host = (char *)DUER_MALLOC(HC_HOST_LEN_MAX + 1); //including NULL-terminating char
    if (host == NULL) {
        DUER_LOGE("No memory!");
        return HTTP_PARSE;
    }

    while (redirect_count <= HC_REDIRECTION_MAX) {
        if (!url) {
            DUER_LOGE("url is NULL!");
            res = HTTP_FAILED;
            goto RET;
        }

        if (HC_CHUNK_SIZE <= HC_RSP_CHUNK_SIZE || HC_CHUNK_SIZE <= HC_HEADER_CHUNK_SIZE) {
            DUER_LOGE("http client chunk size config error!!!\n");
            res = HTTP_FAILED;
            goto RET;
        }

        path_len_max = strlen(url) + 1;
        p_path = (char*)DUER_MALLOC(path_len_max);
        if (!p_path) {
            DUER_LOGE("Mallco memory(size:%d) Failed!\n", path_len_max);
            res = HTTP_FAILED;
            goto RET;
        }

        //to parse url
        res = http_client_parse_url(url, scheme, sizeof(scheme), host, &port, p_path,
                                    path_len_max, is_relative_url);
        if (res != HTTP_OK) {
            DUER_LOGE("parseURL returned %d", res);
            res = HTTP_FAILED;
            goto RET;
        }

        //to set default port
        if (port == 0) { //to do handle HTTPS->443
            port = 80;
        }

        DUER_LOGI("Scheme: %s", scheme);
        DUER_LOGI("Host: %s", host);
        DUER_LOGI("Port: %d", port);
        DUER_LOGI("Path: %s", p_path);
        if ((p_client->p_location) || (p_client->resume_retry_count)) {
            DUER_LOGI("Open socket when http 302 relocating or resume from break point");
            // We create a new socket
            p_client->ops.n_handle = p_client->ops.open(p_client->ops.n_handle);
        }

        //to connect server
        DUER_LOGD("Connecting socket to server");
        ret = p_client->ops.connect(p_client->ops.n_handle, host, port);
        if (ret < 0) {
            p_client->ops.close(p_client->ops.n_handle);
            DUER_LOGE("Could not connect(%d): %s:%d\n", ret, host, port);
            res = HTTP_CONN;
            goto RET;
        }
        DUER_LOGD("Connecting socket to server success:%d", ret);

        //to send http request
        DUER_LOGD("Sending request");
        res = http_client_send_request(p_client, method, p_path, host, port);
        if (res != HTTP_OK) {
            goto RET;
        }

        //to send http headers
        res = http_client_send_headers(p_client, p_client->buf, HC_CHUNK_SIZE);
        if (res != HTTP_OK) {
            goto RET;
        }

        //to receive http response
        DUER_LOGD("Receiving  http response");
        res = http_client_recv_rsp_and_parse(p_client, p_client->buf, HC_RSP_CHUNK_SIZE, &rev_len);
        if (res != HTTP_OK) {
            goto RET;
        }

        //to receive http headers and pasrse it
        DUER_LOGD("Receiving  http headers");
        res = http_client_recv_headers_and_parse(p_client,
                                                 p_client->buf,
                                                 HC_HEADER_CHUNK_SIZE,
                                                 &rev_len);
        if (res == HTTP_REDIRECTTION) {
            res = http_client_parse_redirect_location(p_client, p_path,
                                                      path_len_max, &is_relative_url);
            if (res != HTTP_OK) {
                goto RET;
            }

            DUER_FREE(p_path);
            p_path = NULL;
            url = p_client->p_location;
            redirect_count++;
            DUER_LOGI("Taking %d redirections to [%s]", redirect_count, url);
            continue;
        } else if (res != HTTP_OK) {
            goto RET;
        } else {
            if (p_client->get_url_cb) {
                p_client->get_url_cb(url);
            }
            break;
        }
    }

    if (redirect_count > HC_REDIRECTION_MAX) {
        DUER_LOGE("Too many redirections!");
    }

RET:
    if (p_path) {
        DUER_FREE(p_path);
    }

    if (host) {
        DUER_FREE(host);
    }

    *remain_size_in_buf = rev_len;

    DUER_LOGV("leave\n");
    return res;
}

e_http_result duer_http_client_get_data(http_client_c* p_client, size_t remain_size_in_buf) {
    e_http_result res     = HTTP_OK;

    DUER_LOGV("enrty\n");

    //to receive http chunk data
    if (p_client->n_is_chunked) {
        DUER_LOGD("Receiving  http chunk data\n");
        res = http_client_recv_chunk(p_client, p_client->buf, HC_CHUNK_SIZE - 1,
                                     remain_size_in_buf);
    } else {
        //to receive http data content
        DUER_LOGD("Receiving  http data content\n");
        res = http_client_recv_content_data(p_client, p_client->buf, HC_CHUNK_SIZE - 1,
                                            remain_size_in_buf);
    }

    p_client->ops.close(p_client->ops.n_handle);

    if (res == HTTP_OK) {
        DUER_LOGI("Completed HTTP transaction");
    }

    DUER_LOGV("leave\n");
    return res;
}

/**
 * FUNC:
 * e_http_result duer_http_client_get(http_client_c *p_client, const char* url)
 *
 * DESC:
 * http get
 *
 * PARAM:
 * @param[in] url: url resource
 * @param[in] pos: the position to receive the http data,
 *                 sometimes user only want to get part of the data
 */
e_http_result duer_http_client_get(http_client_c* p_client, const char* url, const size_t pos) {
    size_t         rev_len = 0;
    e_http_result  ret     = HTTP_OK;

    DUER_LOGV("enrty\n");

    DUER_LOGD("http get url: %s\n", url);

    p_client->data_pos = DATA_FIRST;
    p_client->resume_retry_count = 0;
    p_client->recv_size = pos;
    http_client_set_content_len(p_client, 0);

    p_client->buf = DUER_MALLOC(HC_CHUNK_SIZE);
    if (!p_client->buf) {
        ret = HTTP_FAILED;
        goto RET;
    }

    while (p_client->resume_retry_count <= HC_MAX_RETRY_RESUME_COUNT) {
        if (p_client->check_stop_notify_cb && p_client->check_stop_notify_cb()) {
            DUER_LOGI("Stopped getting media data from url by stop flag!\n");
            break;
        }
        // create http connect
        ret = http_client_connect(p_client, url, HTTP_GET, &rev_len);
        if ((ret == HTTP_CONN) || (ret == HTTP_TIMEOUT)) {
            p_client->resume_retry_count++;
            DUER_LOGI("Try to resume from break-point %d time\n", p_client->resume_retry_count);
            continue;
        }

        if (ret != HTTP_OK) {
            break;
        }

        // get http data
        ret = duer_http_client_get_data(p_client, rev_len);
        if ((ret == HTTP_CONN) || (ret == HTTP_TIMEOUT)) {
            p_client->resume_retry_count++;
            if (p_client->resume_retry_count <= HC_MAX_RETRY_RESUME_COUNT) {
                DUER_LOGI("Resume from break-point:retry %d time\n", p_client->resume_retry_count);
            } else {
                DUER_LOGE("Resume from break-point too many times\n");
            }
            continue;
        }

        break;
    }

    DUER_LOGI("HTTP: %d bytes received\n", p_client->recv_size);

    if (ret != HTTP_OK) {
#if 0
        iot_mutex_lock(s_log_report_mutex, 0);
        s_http_client_statistic.error_count++;
        s_http_client_statistic.last_error_code = ret;
        iot_mutex_unlock(s_log_report_mutex);
#endif
    }

RET:
    if ((p_client->data_pos != DATA_FIRST) && (p_client->data_hdlr_cb)) {
        p_client->data_hdlr_cb(
                p_client->p_data_hdlr_ctx,
                DATA_LAST,
                NULL,
                0,
                p_client->p_http_content_type);
    }

    if (p_client->buf) {
        DUER_FREE(p_client->buf);
        p_client->buf = NULL;
    }

    DUER_LOGV("leave\n");
    return ret;
}
