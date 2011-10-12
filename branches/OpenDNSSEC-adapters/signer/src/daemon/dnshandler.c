/*
 * $Id: dnshandler.c 4518 2011-02-24 15:39:09Z matthijs $
 *
 * Copyright (c) 2009 NLNet Labs. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * DNS handler.
 *
 */

#include "config.h"
#include "daemon/dnshandler.h"
#include "daemon/engine.h"
#include "shared/status.h"

#include <errno.h>
#include <string.h>

static const char* dnsh_str = "dnshandler";

static void dnshandler_handle_xfr(netio_type* netio,
    netio_handler_type* handler, netio_events_type event_types);

/**
 * Create dns handler.
 *
 */
dnshandler_type*
dnshandler_create(allocator_type* allocator, listener_type* interfaces)
{
    dnshandler_type* dnsh = NULL;
    socklist_type* socklist = NULL;
    if (!allocator || !interfaces || interfaces->count <= 0) {
        return NULL;
    }
    dnsh = (dnshandler_type*) allocator_alloc(allocator,
        sizeof(dnshandler_type));
    if (!dnsh) {
        ods_log_error("[%s] unable to create dnshandler: "
            "allocator_alloc() failed", dnsh_str);
        return NULL;
    }
    socklist = (socklist_type*) allocator_alloc(allocator,
        sizeof(socklist_type));
    if (!socklist) {
        ods_log_error("[%s] unable to create socklist: "
            "allocator_alloc() failed", dnsh_str);
        return NULL;
    }
    dnsh->allocator = allocator;
    dnsh->need_to_exit = 0;
    dnsh->engine = NULL;
    dnsh->interfaces = interfaces;
    dnsh->socklist = socklist;
    dnsh->xfrhandler.fd = -1;
    dnsh->xfrhandler.xfrd = (void*) dnsh;
    dnsh->xfrhandler.timeout = 0;
    dnsh->xfrhandler.event_types = NETIO_EVENT_READ;
    dnsh->xfrhandler.event_handler = dnshandler_handle_xfr;
    return dnsh;
}


/**
 * Start dns handler.
 *
 */
void
dnshandler_start(dnshandler_type* dnshandler)
{
    engine_type* engine = NULL;
    ods_status status = ODS_STATUS_OK;
    size_t i = 0;
    fd_set rset, wset, eset;
    struct timeval timeout;
    int count, maxfd = 0;
    count = 0;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    ods_log_assert(dnshandler);
    ods_log_assert(dnshandler->engine);
    ods_log_debug("[%s] start", dnsh_str);
    /* setup */
    engine = (engine_type*) dnshandler->engine;
    status = sock_listen(dnshandler->socklist, dnshandler->interfaces);
    if (status != ODS_STATUS_OK) {
        ods_log_error("[%s] unable to start: sock_listen() "
            "failed (%s)", dnsh_str, ods_status2str(status));
        engine->need_to_exit = 1;
        return;
    }
    /* service */
    while (dnshandler->need_to_exit == 0) {
        FD_ZERO(&rset);
        FD_ZERO(&wset);
        FD_ZERO(&eset);
        for (i=0; i < MAX_INTERFACES; i++) {
            if (dnshandler->socklist->udp[i].s != -1) {
                FD_SET(dnshandler->socklist->udp[i].s, &rset);
            }
            if (dnshandler->socklist->tcp[i].s != -1) {
                FD_SET(dnshandler->socklist->tcp[i].s, &rset);
            }
            if (dnshandler->socklist->udp[i].s > maxfd) {
                maxfd = dnshandler->socklist->udp[i].s;
            }
            if (dnshandler->socklist->tcp[i].s > maxfd) {
                maxfd = dnshandler->socklist->tcp[i].s;
            }
        }
        if (select(maxfd+1, &rset, &wset, &eset, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            ods_log_error("[%s] select() failed: %s", dnsh_str,
                strerror(errno));
        }
        for (i=0; i < MAX_INTERFACES; i++) {
            if (dnshandler->socklist->udp[i].s != -1 &&
                FD_ISSET(dnshandler->socklist->udp[i].s, &rset)) {
                sock_handle_udp(dnshandler->socklist->udp[i].s,
                    dnshandler->engine);
            }
            if (dnshandler->socklist->tcp[i].s != -1 &&
                FD_ISSET(dnshandler->socklist->tcp[i].s, &rset)) {
                sock_handle_tcp(dnshandler->socklist->tcp[i].s,
                    dnshandler->engine);
            }
        }
    }
    /* shutdown */
    ods_log_debug("[%s] shutdown", dnsh_str);
    for (i=0; i < MAX_INTERFACES; i++) {
        if (dnshandler->socklist->udp[i].s != -1) {
            close(dnshandler->socklist->udp[i].s);
            freeaddrinfo((void*)dnshandler->socklist->udp[i].addr);
        }
        if (dnshandler->socklist->tcp[i].s != -1) {
            close(dnshandler->socklist->tcp[i].s);
            freeaddrinfo((void*)dnshandler->socklist->tcp[i].addr);
        }
    }
    return;
}


/**
 * Signal dns handler.
 *
 */
void
dnshandler_signal(dnshandler_type* dnshandler)
{
    if (dnshandler) {
        ods_thread_kill(dnshandler->thread_id, SIGHUP);
    }
    return;
}


/**
 * Forward notify to zone transfer handler.
 *
 */
void
dnshandler_fwd_notify(dnshandler_type* dnshandler, uint8_t* pkt, size_t len)
{
    ssize_t nb = 0;
    ods_log_assert(dnshandler);
    ods_log_assert(pkt);
    nb = send(dnshandler->xfrhandler.fd, (const void*) pkt, len, 0);
    if (nb < 0) {
        ods_log_error("[%s] unable to forward notify: send() failed (%s)",
            dnsh_str, strerror(errno));
    } else if (nb == 0) {
        ods_log_error("[%s] unable to forward notify: no data sent", dnsh_str);
    } else {
        ods_log_error("[%s] forwarded notify: %u bytes sent", dnsh_str, nb);
    }
    return;
}


/**
 * Handle forwarded dns packets.
 *
 */
static void
dnshandler_handle_xfr(netio_type* ATTR_UNUSED(netio),
    netio_handler_type* handler, netio_events_type event_types)
{
    dnshandler_type* dnshandler = NULL;
    uint8_t buf[MAX_PACKET_SIZE];
    ssize_t received = 0;
    if (!handler) {
        return;
    }
    dnshandler = (dnshandler_type*) handler->xfrd;
    ods_log_assert(event_types & NETIO_EVENT_READ);
    ods_log_debug("[%s] read forwared xfr packet", dnsh_str);
    received = read(dnshandler->xfrhandler.fd, &buf, MAX_PACKET_SIZE);
    return;
}


/**
 * Cleanup dns handler.
 *
 */
void
dnshandler_cleanup(dnshandler_type* dnshandler)
{
    allocator_type* allocator = NULL;
    if (!dnshandler) {
        return;
    }
    allocator = dnshandler->allocator;
    allocator_deallocate(allocator, (void*) dnshandler->socklist);
    allocator_deallocate(allocator, (void*) dnshandler);
    return;
}
