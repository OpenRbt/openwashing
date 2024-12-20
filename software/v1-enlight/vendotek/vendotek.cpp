#include <arpa/inet.h>
#include <byteswap.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vendotek.h"

/*
 * Logging
 */
void vtk_logline_default(int flags, const char *logline)
{
    FILE *outfile = ((flags & VTK_LOG_PRIMASK) <= LOG_ERR) ? stderr : stdout;
    const char *outform = (flags & VTK_LOG_NOEOL) ? "%s" : "%s\n";

    fprintf(outfile, outform, logline);
    fflush(outfile);
}
static int            vtk_loglevel = LOG_DEBUG;
static vtk_logline_fn vtk_logline  = vtk_logline_default;

void vtk_log(int flags, const char *format, ...)
{
    if ((flags & VTK_LOG_PRIMASK) > vtk_loglevel) {
        return;
    }

    char buffer[4096];

    va_list vlist;
    va_start(vlist, format);
    vsnprintf(buffer, sizeof(buffer), format, vlist);
    va_end(vlist);

    vtk_logline(flags, buffer);
}

void vtk_logline_set(vtk_logline_fn logline, int loglevel)
{
    vtk_loglevel = loglevel;
    vtk_logline  = logline ? logline : vtk_logline_default;
}

/*
 * Main State
 */
typedef struct vkt_sock_s {
    struct sockaddr_in addr;
    int                fd;
} vtk_sock_t;

struct vtk_s {
    vtk_net_t    net_state;
    vtk_sock_t   sock_conn;
    vtk_sock_t   sock_list;
    vtk_sock_t   sock_accept;
    vtk_stream_t stream_up;
    vtk_stream_t stream_down;
};

int vtk_init(vtk_t **vtk)
{
    *vtk  = (vtk_t*)malloc(sizeof(vtk_t));
    **vtk = (vtk_t) {};
    (**vtk).net_state = VTK_NET_DOWN;
    (**vtk).sock_conn.fd   = -1;
    (**vtk).sock_list.fd   = -1;
    (**vtk).sock_accept.fd = -1;
    return 0;    
}

void vtk_free(vtk_t *vtk){
    if (! VTK_NET_IS_DOWN(vtk->net_state)) {
        vtk_net_set(vtk, VTK_NET_DOWN, 0, NULL, NULL);
    }
    if (vtk->sock_conn.fd >= 0) {
        close(vtk->sock_conn.fd);
    }
    if (vtk->sock_accept.fd >= 0) {
        close(vtk->sock_accept.fd);
    }
    if (vtk->sock_list.fd >= 0) {
        close(vtk->sock_list.fd);
    }
    free(vtk->stream_up.data);
    free(vtk->stream_down.data);
    free(vtk);
}

/*
 * Messaging
 */
typedef struct msg_arg_s {
    uint16_t   id;
    uint16_t   len;
    char      *val;
    size_t     val_sz;
} msg_arg_t;

typedef struct msg_hdr_s {
    uint16_t   len;
    uint16_t   proto;
} __attribute__((packed)) msg_hdr_t;

struct vtk_msg_s {
    vtk_t     *vtk;
    msg_hdr_t  header;
    msg_arg_t *args;
    size_t     args_cnt;
    size_t     args_sz;
};

const char * vtk_msg_stringify(uint16_t id)
{
    struct msg_argid_s {
        uint16_t  id;
        const char     *desc;
    } argids[] = {
       { 0x01, "Message name            " },
       { 0x03, "Operation number        " },
       { 0x04, "Minor currency units    " },
       { 0x05, "Keepalive interval, sec " },
       { 0x06, "Operation timeout, sec  " },
       { 0x07, "Event name              " },
       { 0x08, "Event number            " },
       { 0x09, "Product id              " },
       { 0x0A, "QR-code data            " },
       { 0x0B, "TCP/IP destinactio      " },
       { 0x0C, "Outgoing byte counter   " },
       { 0x0D, "Simple data block       " },
       { 0x0E, "Confirmable data block  " },
       { 0x0F, "Product name            " },
       { 0x10, "POS management data     " },
       { 0x11, "Local time              " },
       { 0x12, "System information      " },
       { 0x13, "Banking receipt         " },
       { 0x14, "Display time, ms        " },
       { 0x19, "Host id                 " },
       { 0, NULL }
    };
    for (int iarg = 0; argids[iarg].id; iarg++) {
        if (argids[iarg].id == id) {
            return argids[iarg].desc;
        }
    }
    return "Unknown argument ID";
}

int vtk_msg_init(vtk_msg_t **msg, vtk_t *vtk)
{
    *msg  = (vtk_msg_t*)malloc(sizeof(vtk_msg_t));
    **msg = (vtk_msg_t) {};
    (**msg).vtk = vtk;
    (**msg).header.proto = VTK_BASE_FROM_STATE(vtk->net_state);
    (**msg).header.len   = sizeof((*msg)->header.proto);
    return 0;
}

void vtk_msg_free(vtk_msg_t  *msg)
{
    for (unsigned int iarg = 0; (iarg < msg->args_sz); iarg++) {
        free(msg->args[iarg].val);
    }
    free(msg->args);
    free(msg);
}

int vtk_msg_find_param(vtk_msg_t *msg, uint16_t id, uint16_t *len, char **value)
{
    for (unsigned int iarg = 0; (iarg < msg->args_sz); iarg++) {
        if (msg->args[iarg].id == id) {
            if (len) {
                *len   = msg->args[iarg].len;
            }
            if (value) {
                *value = msg->args[iarg].val;
            }
            return 0;
        }
    }
    return -1;
}

int vtk_msg_iter_param(vtk_msg_t *msg, uint16_t iparam, uint16_t *id, uint16_t *len, char **value)
{
    if (iparam >= msg->args_cnt) {
        return -1;
    }
    *id    = msg->args[iparam].id;
    *len   = msg->args[iparam].len;
    *value = msg->args[iparam].val;
    return 0;
}

int vtk_msg_mod(vtk_msg_t *msg, vtk_msgmod_t mod, uint16_t id, uint16_t len, char *value)
{
    if (VTK_MSG_MODADD(mod)) {
        size_t newlen = msg->header.len + VTK_MSG_VARLEN(id) + VTK_MSG_VARLEN(len);
        if (newlen > VTK_MSG_MAXLEN) {
            return -1;
        }
        msg->header.len = newlen;

        if (msg->args_cnt == msg->args_sz) {
            msg->args_sz = msg->args_sz ? (msg->args_sz * 2) : 1;
            msg->args = (msg_arg_t*)realloc(msg->args, sizeof(msg_arg_t) * msg->args_sz);
            memset(&msg->args[msg->args_cnt], 0, sizeof(msg_arg_t) * (msg->args_sz - msg->args_cnt));
        }
    }
    if (mod == VTK_MSG_ADDSTR) {
        msg_arg_t *arg = &msg->args[msg->args_cnt];
        arg->id = id;
        arg->len = strlen(value);
        if (msg->header.len + arg->len > VTK_MSG_MAXLEN) {
            return -1;
        }
        msg->header.len += arg->len;

        if (arg->val_sz <= arg->len) {
            arg->val = (char*)realloc(arg->val, arg->len + 1);
            arg->val_sz = arg->len + 1;
        }
        snprintf(arg->val, arg->val_sz, "%s", value);
        msg->args_cnt++;
    }
    if (mod == VTK_MSG_ADDBIN) {
        msg_arg_t *arg = &msg->args[msg->args_cnt];
        arg->id = id;
        arg->len = len;
        if (msg->header.len + arg->len > VTK_MSG_MAXLEN) {
            return -1;
        }
        msg->header.len += arg->len;

        if (arg->val_sz <= arg->len) {
            arg->val = (char*)realloc(arg->val, arg->len + 1);
            arg->val_sz = arg->len + 1;
        }
        memcpy(arg->val, value, len);
        arg->val[len] = 0;
        msg->args_cnt++;
    }
    if (mod == VTK_MSG_RESET) {
        msg->header.proto = id;
        msg->header.len   = sizeof(msg->header.proto);
        msg->args_cnt     = 0;
    }

    return 0;
}

int vtk_msg_print(vtk_msg_t *msg)
{
    for (unsigned int iarg = 0; iarg < msg->args_cnt; iarg++) {
        msg_arg_t *arg = &msg->args[iarg];
        vtk_logio("  % 2d: 0x%x  %s  => ", iarg, arg->id, vtk_msg_stringify(arg->id));

        int hexout = 0;
        for (int i = 0; i < arg->len; i++) {
            if (! isprint(arg->val[i]) && (arg->val[i] != '\t')) {
                hexout = 1;
                break;
            }
        }
        if (hexout) {
            for (int i = 0; i < arg->len; i++) {
                vtk_logio("%0x ", (uint8_t)arg->val[i]);
            }
            vtk_logi("");
        } else {
            vtk_logi("%s", arg->val);
        }
    }
    return 0;
}

static int
vtk_stream_write(vtk_stream_t *stream, uint16_t len, void *data, int logdump)
{
    if (stream->len + len >= stream->size) {
        if (stream->len + len > stream->size * 2) {
            stream->size = stream->len + len;
        } else {
            stream->size = stream->size * 2;
        }
        stream->data = (char*)realloc(stream->data, stream->size);
    }
    char *cdata = (char *)data;
    for (int i = 0; i < len; i++) {
        stream->data[stream->len + i] = cdata[i];
        if (logdump) {
            vtk_logdo("%02X", (uint8_t)cdata[i]);
        }
    }
    stream->len += len;
    return len;
}

static int
vtk_stream_read(vtk_stream_t *stream, uint16_t len, void *data, int logdump)
{
    if (stream->offset + len > stream->len) {
        return -1;
    }
    char *cdata = (char *)data;
    for (int i = 0; i < len; i++) {
        cdata[i] = stream->data[stream->offset + i];
        if (logdump) {
            vtk_logdo("%02X", (uint8_t)cdata[i]);
        }
    }
    stream->offset += len;
    return len;
}

static int
vtk_varint_serialize(vtk_stream_t *stream, uint16_t value)
{
    uint8_t varint[3];
    if (value <= 127) {
        varint[0] = value;
    } else if (value <= 255) {
        varint[0] = 128 + 1;
        varint[1] = value;
    } else {
        varint[0] = 128 + 2;
        varint[1] = (uint8_t)(value >> 8);
        varint[2] = (uint8_t)(value & 255);
    }
    return vtk_stream_write(stream, VTK_MSG_VARLEN(value), varint, 1);
}

static int
vtk_varint_deserialize(vtk_stream_t *stream, uint16_t *value)
{
    uint8_t varint[3];

    // These 3 init lines just to fix false positive linter warns
    varint[0] = 0;
    varint[1] = 0;
    varint[2] = 0;
    
    vtk_stream_read(stream, 1, &varint[0], 1);

    if (varint[0] <= 127) {
        *value = varint[0];
    } else if ((varint[0] & 127) == 1) {
        vtk_stream_read(stream, 1, &varint[1], 1);
        *value = varint[0];
    } else if ((varint[0] & 127) == 2) {
        vtk_stream_read(stream, 1, &varint[1], 1);
        vtk_stream_read(stream, 1, &varint[2], 1);
        *value = (varint[1] << 8) + varint[2];
    } else {
        return -1;
    }
    return 0;
}

int vtk_msg_serialize(vtk_msg_t *msg, vtk_stream_t *stream)
{
    msg_hdr_t swap = {
        .len   = bswap_16(msg->header.len),
        .proto = bswap_16(msg->header.proto),
    };
    stream->offset = stream->len = 0;
    vtk_stream_write(stream, sizeof(swap.len), &swap.len, 1);
    vtk_stream_write(stream, sizeof(swap.proto), &swap.proto, 1);
    vtk_logio(" ");

    for (unsigned int iarg = 0; iarg < msg->args_cnt; iarg++) {
        msg_arg_t *arg = &msg->args[iarg];
        vtk_varint_serialize(stream, arg->id);
        vtk_varint_serialize(stream, arg->len);
        vtk_stream_write(stream, arg->len, arg->val, 1);
        vtk_logio(" ");
    }
    vtk_logi("");
    return 0;
}

int vtk_msg_deserialize(vtk_msg_t *msg, vtk_stream_t *stream)
{
    stream->offset = 0;

    if (stream->len < sizeof(msg->header.len)) {
        return -1;
    }
    msg_hdr_t swap;
    vtk_stream_read(stream, sizeof(swap), &swap, 1);
    msg->header.len   = bswap_16(msg->header.len);
    msg->header.proto = bswap_16(msg->header.proto);

    vtk_logio(" ");
    for (int iarg = 0; stream->offset < stream->len; iarg++) {
        msg_arg_t arg = {0};
        vtk_varint_deserialize(stream, &arg.id);
        vtk_varint_deserialize(stream, &arg.len);
        vtk_msg_mod(msg, VTK_MSG_ADDBIN, arg.id, arg.len, &stream->data[stream->offset]);

        for (int i = 0; i < arg.len; i++) {
            vtk_logio("%02X", (uint8_t)stream->data[stream->offset]);
            stream->offset++;
        }
        vtk_logio(" ");
    }
    vtk_logi("");
    return 0;
}

/*
 * Network State
 */
const char *vtk_net_stringify(vtk_net_t vtk_net)
{
    switch(vtk_net) {
        case VTK_NET_DOWN:      return "DOWN";
        case VTK_NET_CONNECTED: return "CONNECTED";
        case VTK_NET_LISTENED:  return "LISTENED";
        case VTK_NET_ACCEPTED:  return "ACCEPTED";
        default:                return "UNKNOWN";
    }
}

int vtk_net_set(vtk_t *vtk, vtk_net_t net_to, int tm, char *addr, char *port)
{
    if (VTK_NET_IS_DOWN(vtk->net_state) && VTK_NET_IS_LISTEN(net_to)) {
        /*
         * setup listen socket
         */
        vtk_sock_t *lsock = &vtk->sock_list;

        lsock->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (lsock->fd < 0) {
            vtk_loge("%s", "Can't create listen socket");
            return -1;
        }
        int sockoption = 1;
        setsockopt(lsock->fd, SOL_SOCKET, SO_REUSEADDR, &sockoption, sizeof(sockoption));

        lsock->addr.sin_family = AF_INET;
        lsock->addr.sin_port   = htons(atoi(port));
        if (inet_aton(addr, &lsock->addr.sin_addr) == 0) {
            vtk_loge("%s %s", "Bad listen addr:", addr);
            close(lsock->fd);
            lsock->fd = -1;
            return -1;
        }
        if (bind(lsock->fd, (struct sockaddr *)&lsock->addr, sizeof(lsock->addr)) < 0) {
            vtk_loge("%s %s", "Listen socket binding error:", strerror(errno));
            close(lsock->fd);
            lsock->fd = -1;
            return -1;
        }
        if (listen(lsock->fd, INT32_MAX) < 0) {
            vtk_loge("%s %s", "Listen socket error:", strerror(errno));
            close(lsock->fd);
            lsock->fd = -1;
            return -1;
        }
        vtk->net_state = net_to;
        vtk_logi("Start to listen on %s:%s", addr, port);
        return 0;
    }

    if (VTK_NET_IS_LISTEN(vtk->net_state) && VTK_NET_IS_ACCEPTED(net_to)) {
        /*
         * accept incoming connection
         */
        vtk_sock_t *lsock = &vtk->sock_list;
        vtk_sock_t *asock = &vtk->sock_accept;
        socklen_t   asize = sizeof(asock->addr);

        asock->fd = accept(lsock->fd, (struct sockaddr *)&asock->addr, &asize);
        if (asock->fd < 0) {
            vtk_loge("%s %s", "Can't accept incoming connection:", strerror(errno));
            return -1;
        }
        long fdflags = fcntl(asock->fd, F_GETFL, NULL);
        fdflags = (fdflags) < 0 ? 0 : fdflags;
        fcntl(asock->fd, F_SETFL, fdflags | O_NONBLOCK);

        vtk->net_state = net_to;
        vtk_logi("Client connected from %s:%u",
                  inet_ntoa(asock->addr.sin_addr), ntohs(asock->addr.sin_port));
        return 0;
    }

    if (VTK_NET_IS_ACCEPTED(vtk->net_state) && VTK_NET_IS_LISTEN(net_to)) {
        /*
         * close incoming connection; listen socket remains open and should be reused
         */
        vtk_sock_t *lsock = &vtk->sock_list;
        vtk_sock_t *asock = &vtk->sock_accept;
        close(asock->fd);
        asock->fd = -1;
        memset(&asock->addr, 0, sizeof(asock->addr));

        vtk->net_state = net_to;
        vtk_logi("Client connected was closed. Continue listen on %s:%u",
                  inet_ntoa(lsock->addr.sin_addr), ntohs(lsock->addr.sin_port));
        return 0;
    }

    if (VTK_NET_IS_ACCEPTED(vtk->net_state) && VTK_NET_IS_DOWN(net_to)) {
        /*
         * close incoming connection; listen socket should be closed too
         */
        vtk_sock_t *lsock = &vtk->sock_list;
        vtk_sock_t *asock = &vtk->sock_accept;
        close(asock->fd);
        asock->fd = -1;
        memset(&asock->addr, 0, sizeof(asock->addr));

        close(lsock->fd);
        lsock->fd = -1;
        memset(&lsock->addr, 0, sizeof(lsock->addr));

        vtk->net_state = net_to;
        vtk_logi("Network state is DOWN");

        return 0;
    }

    if (VTK_NET_IS_LISTEN(vtk->net_state) && VTK_NET_IS_DOWN(net_to)) {
        /*
         * close listen socket
         */
        vtk_sock_t *lsock = &vtk->sock_list;

        close(lsock->fd);
        lsock->fd = -1;
        memset(&lsock->addr, 0, sizeof(lsock->addr));

        vtk->net_state = net_to;
        vtk_logi("Network state is DOWN");
        return 0;
    }

    if (VTK_NET_IS_DOWN(vtk->net_state) && VTK_NET_IS_CONNECTED(net_to)) {
        /*
         * setup outgoing connection
         */
        vtk_sock_t *csock = &vtk->sock_conn;

        csock->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (csock->fd < 0) {
            vtk_loge("%s", "Can't create connect socket");
            return -1;
        }
        int       sockopt = 1;
        socklen_t socksize = sizeof(sockopt);
        setsockopt(csock->fd, SOL_SOCKET, SO_REUSEADDR, &sockopt, socksize);

        long fdflags = fcntl(csock->fd, F_GETFL, NULL);
        fdflags = (fdflags) < 0 ? 0 : fdflags;
        fcntl(csock->fd, F_SETFL, fdflags | O_NONBLOCK);

        csock->addr.sin_family = AF_INET;
        csock->addr.sin_port   = htons(atoi(port));
        if (inet_aton(addr, &csock->addr.sin_addr) == 0) {
            vtk_loge("%s %s", "Bad connection addr:", addr);
            close(csock->fd);
            csock->fd = -1;
            return -1;
        }
        int rconn = connect(csock->fd, (struct sockaddr *)&csock->addr, sizeof(csock->addr));
        if ((rconn >= 0) || (errno != EINPROGRESS)) {
            vtk_loge("%s %s:%u (%s)", "Can't connect to: ",
                      inet_ntoa(csock->addr.sin_addr), ntohs(csock->addr.sin_port), strerror(errno));
            close(csock->fd);
            csock->fd = -1;
            return -1;
        }
        struct pollfd pollfd = {
            .fd     = csock->fd,
            .events = POLLOUT
        };
        int rpoll = poll(&pollfd, 1, tm);
        if (rpoll == 0) {
            vtk_loge("%s %s:%u", "Connection timeout. Endpoint:",
                      inet_ntoa(csock->addr.sin_addr), ntohs(csock->addr.sin_port));
            close(csock->fd);
            csock->fd = -1;
            return -1;
        } else if ((rpoll < 0) ||
                   (getsockopt(csock->fd, SOL_SOCKET, SO_ERROR, &sockopt, &socksize) < 0) ||
                   (sockopt != 0)
                  ) {
            vtk_loge("%s %s:%u", "Can't connect to:",
                      inet_ntoa(csock->addr.sin_addr), ntohs(csock->addr.sin_port));
            close(csock->fd);
            csock->fd = -1;
            return -1;
        }

        vtk->net_state = net_to;
        vtk_logi("Connected to %s:%u", inet_ntoa(csock->addr.sin_addr), ntohs(csock->addr.sin_port));
        return 0;
    }

    if (VTK_NET_IS_CONNECTED(vtk->net_state) && VTK_NET_IS_DOWN(net_to)) {
        /*
         * do disconnect
         */
        vtk_sock_t *csock = &vtk->sock_conn;

        close(csock->fd);
        csock->fd = -1;
        memset(&csock->addr, 0, sizeof(csock->addr));

        vtk->net_state = net_to;
        vtk_logi("Network state is DOWN");
        return 0;

    }

    vtk_loge("%s -> %s: Unsupported network state transition", vtk_net_stringify(vtk->net_state), vtk_net_stringify(net_to));
    return -1;
}

vtk_net_t vtk_net_get_state(vtk_t *vtk)
{
    return vtk->net_state;
}

int vtk_net_get_socket(vtk_t *vtk)
{
    switch(vtk->net_state) {
        case VTK_NET_CONNECTED: return vtk->sock_conn.fd;
        case VTK_NET_LISTENED:  return vtk->sock_list.fd;
        case VTK_NET_ACCEPTED:  return vtk->sock_accept.fd;
        default:                return -1;
    }
}

int vtk_net_send(vtk_t *vtk, vtk_msg_t *msg)
{
    if (! VTK_NET_IS_ESTABLISHED(vtk->net_state)) {
        vtk_loge("Message can be send in case of %s or %s network state",
                  vtk_net_stringify(VTK_NET_ACCEPTED),
                  vtk_net_stringify(VTK_NET_CONNECTED));
        return -1;
    }
    int sock = vtk_net_get_socket(vtk);
    vtk_msg_serialize(msg, &vtk->stream_up);

    size_t bwritten = 0;
    for (; bwritten < vtk->stream_up.len; ) {
        ssize_t wresult = write(sock, &vtk->stream_up.data[bwritten], vtk->stream_up.len - bwritten);
        if (wresult < 0) {
            vtk_loge("socket error: %s", strerror(errno));
            return -1;
        } else if (wresult == 0) {
            vtk_loge("unexpected socket behavior (buffer overflow?)");
            return -1;
        } else {
            bwritten += wresult;
        }
    }
    vtk_logi("%lu bytes were sent", bwritten);
    return 0;
}

int vtk_net_recv(vtk_t *vtk, vtk_msg_t *msg, int *eof)
{
    if (! VTK_NET_IS_ESTABLISHED(vtk->net_state)) {
        vtk_loge("Message can be send in case of %s or %s network state",
                  vtk_net_stringify(VTK_NET_ACCEPTED),
                  vtk_net_stringify(VTK_NET_CONNECTED));
        return -1;
    }
    int     sock = vtk_net_get_socket(vtk);
    ssize_t rcount = 0;
    char    buffer[0xff];

    vtk->stream_down.len = 0;
    for (;;) {
        rcount = read(sock, buffer, sizeof(buffer));
        if (rcount > 0) {
            vtk_stream_write(&vtk->stream_down, rcount, buffer, 0);
        } else {
            *eof = (rcount == 0);
            break;
        }
    }
    if (vtk->stream_down.len) {
        vtk_logi("%lu bytes were read", vtk->stream_down.len);

        vtk_msg_mod(msg, VTK_MSG_RESET, VTK_BASE_FROM_STATE(vtk->net_state), 0, NULL);

        if (vtk_msg_deserialize(msg, &vtk->stream_down) >= 0) {
            return vtk->stream_down.len;
        } else {
            return -1;
        }
    }
    return 0;
}
