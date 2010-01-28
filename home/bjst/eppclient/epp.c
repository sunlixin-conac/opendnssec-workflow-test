/*
 * $Id$
 *
 * Copyright (c) 2010 .SE (The Internet Infrastructure Foundation).
 * All rights reserved.
 *
 * Written by Björn Stenberg <bjorn@haxx.se> for .SE
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
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>
#include <stdbool.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <unistd.h>

#include "ezxml.h"
#include "config.h"

static const char* head =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
    "<epp xmlns=\"urn:ietf:params:xml:ns:epp-1.0\"\n"
    " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
    " xsi:schemaLocation=\"urn:ietf:params:xml:ns:epp-1.0 epp-1.0.xsd\">\n";

static const char* foot = "</epp>\n";

static int sockfd = -1;
static SSL* ssl = NULL;

static ezxml_t read_frame(void)
{
    static char buffer[8192];

    int rc = SSL_read(ssl, buffer, 4);
    if (rc <= 0) {
        syslog(LOG_ERR, "SSL_read(4) returned error %d (ret %d, errno %d)",
               SSL_get_error(ssl, rc), rc, errno);
        long err;
        while ((err = ERR_get_error()))
            syslog(LOG_ERR, "SSL error %ld", err);
        return NULL;
    }
    int len = (((buffer[0] & 0xff) << 24) |
               ((buffer[1] & 0xff) << 16) |
               ((buffer[2] & 0xff) << 8) |
               (buffer[3] & 0xff));
    len -= 4;

    if (len > sizeof(buffer)) {
        len = sizeof(buffer);
        syslog(LOG_DEBUG,
               "Read frame is larger than buffer. Shrinking to %d bytes", len);
    }

    rc = SSL_read(ssl, buffer, len);
    if (rc <= 0) {
        syslog(LOG_ERR, "SSL_read(%d) returned error %d",
               len, SSL_get_error(ssl, rc));
        return NULL;
    }
    buffer[sizeof(buffer)-1] = 0;

#ifdef DEBUG
    FILE* f = fopen("input.xml", "w");
    if (f) {
        fwrite(buffer, len, 1, f);
        fclose(f);
    }
#endif

    ezxml_t xml = ezxml_parse_str(buffer, len);

    return xml;
}

static int send_frame(char* ptr, int len)
{
    unsigned char buf[4];
    len += 4;
    buf[0] = (len & 0xff000000) >> 24;
    buf[1] = (len & 0x00ff0000) >> 16;
    buf[2] = (len & 0x0000ff00) >> 8;
    buf[3] = (len & 0x000000ff);

    int rc = SSL_write(ssl, buf, 4);
    if (rc <= 0) {
        syslog(LOG_ERR, "SSL_write(4) returned %d",
               SSL_get_error(ssl, rc));
        return -1;
    }

    rc = SSL_write(ssl, ptr, len);
    if (rc <= 0) {
        syslog(LOG_ERR, "SSL_write(%d) returned %d",
               len, SSL_get_error(ssl, rc));
        return -1;
    }

#ifdef DEBUG
    FILE* f = fopen("output.xml", "w");
    if (f) {
        fprintf(f, "%02x %02x %02x %02x (%d)\n",
                buf[0], buf[1], buf[2], buf[3], len);
        fwrite(ptr, len-4, 1, f);
        fclose(f);
    }
#endif

    return 0;
}


static int read_response(void)
{
    ezxml_t response = read_frame();
    ezxml_t x = ezxml_get(response,"response",0,"result",-1);
    if (!x) {
        syslog(LOG_ERR, "No <result> in xml");
        return -1;
    }

    const char* code = ezxml_attr(x, "code");
    if (!x) {
        syslog(LOG_ERR, "No code= in <result>");
        return -1;
    }

    x = ezxml_child(x, "msg");
    if (!x) {
        syslog(LOG_ERR, "No <msg> in <result>");
        return -1;
    }
    char* msg = x->txt;

    syslog(LOG_DEBUG, "<< result %s (%s)", code, msg);

    if (code[0] == 1) {
        ezxml_free(response);
        return 0;
    }

    x = ezxml_get(response,"response",0,"result",0,"extValue",0,"reason",-1);
    if (x)
        syslog(LOG_DEBUG, "Failure reson: %s", x->txt);

    ezxml_free(response);
    return -1;
}

static bool server_supports_dnssec(ezxml_t greeting)
{
    /* verify dnssec support */
    ezxml_t uri = ezxml_get(greeting,
                            "greeting",0,
                            "svcMenu",0,
                            "svcExtension",0,
                            "extURI",-1);
    for (; uri; uri = uri->next)
        if (!strcmp(uri->txt, "urn:ietf:params:xml:ns:secDNS-1.0"))
            break;
    
    if (uri)
        return true;

    syslog(LOG_NOTICE, "Server doesn't support DNSSEC extension");
    return false;
}

static void cleanup(void)
{
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
}

static int login(ezxml_t greeting)
{
    char* version = NULL;
    ezxml_t x = ezxml_get(greeting,"greeting",0,"svcMenu",0,"version",-1);
    if (!x) {
        syslog(LOG_ERR, "No <version> in greeting!");
        return -1;
    }
    version = x->txt;

    char* lang = NULL;
    x = ezxml_get(greeting,"greeting",0,"svcMenu",0,"lang",-1);
    if (!x) {
        syslog(LOG_ERR, "No <lang> in greeting!");
        return -1;
    }
    lang = x->txt;
    ezxml_free(greeting);
    
    /* construct login xml */
    char buffer[4096];
    snprintf(buffer, sizeof buffer,
             "%s"
             "<command>\n"
             " <login>\n"
             "  <clID>%s</clID>\n"
             "  <pw>%s</pw>\n"
             "  <options>\n"
             "   <version>%s</version>\n"
             "   <lang>%s</lang>\n"
             "  </options>\n"
             "  <svcs>\n"
             "   <objURI>urn:ietf:params:xml:ns:domain-1.0</objURI>\n"
             "   <objURI>urn:ietf:params:xml:ns:host-1.0</objURI>\n"
             "   <objURI>urn:ietf:params:xml:ns:contact-1.0</objURI>\n"
             "   <svcExtension>\n"
             "    <extURI>urn:ietf:params:xml:ns:secDNS-1.0</extURI>\n"
             "    <extURI>urn:se:iis:xml:epp:iis-1.1</extURI>\n" /* TODO: rm */
             "   </svcExtension>\n"
             "  </svcs>\n"
             " </login>\n"
             "</command>\n"
             "%s",
             head,
             config.user, config.password,
             version, lang,
             foot);

    syslog(LOG_DEBUG,">> login");
    send_frame(buffer, strlen(buffer));

    if (read_response())
        return -1;

    return 0;
}

int epp_logout(void)
{
    /* construct logout xml */
    char buffer[4096];
    snprintf(buffer, sizeof buffer,
             "%s"
             "<command>\n"
             " <logout/>\n"
             "</command>\n"
             "%s",
             head,
             foot);

    syslog(LOG_DEBUG,">> logout");
    send_frame(buffer, strlen(buffer));

    /* ignore result - server disconnects after <logout> */

    cleanup();

    return 0;
}

int epp_login(SSL_CTX* sslctx)
{
    static const char* host = "epptest.iis.se";
    static const char* service = "700";

    struct addrinfo* ai;
    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host, service, &hints, &ai)) {
        syslog(LOG_ERR, "getaddrinfo(%s,%s): %s",
               host, service, strerror(errno));
        return -1;
    }

    for (struct addrinfo* a = ai; a; a = a->ai_next) {
        sockfd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (sockfd < 0)
            continue;

        if (connect(sockfd, a->ai_addr, a->ai_addrlen) >= 0)
            break;

        close(sockfd);
    }
    if (sockfd == -1) {
        syslog(LOG_ERR, "socket(): %s", strerror(errno));
        return -1;
    }

    ssl = SSL_new(sslctx);
    if (!ssl) {
        syslog(LOG_ERR, "SSL_new(): %s",
               ERR_error_string(ERR_get_error(), NULL));
        return -1;
    }

    if (!SSL_set_fd(ssl, sockfd)) {
        syslog(LOG_ERR, "SSL_set_fd(): %s",
               ERR_error_string(ERR_get_error(), NULL));
        return -1;
    }

    int rc = SSL_connect(ssl);
    switch (rc) {
        case 1:
            /* success */
            syslog(LOG_DEBUG, "SSL connection established");
            break;

        case 2:
            /* connection shut down */
            syslog(LOG_WARNING, "SSL_connect() returned %d",
                   SSL_get_error(ssl, rc));
            return -1;

        default:
            /* error */
            syslog(LOG_ERR, "SSL_connect(): %s",
                   ERR_error_string(ERR_get_error(), NULL));
            return -1;
    }

    ezxml_t greeting = read_frame();
    
    if (!server_supports_dnssec(greeting))
        return -1;

    if (login(greeting))
        return -1;
    
    return 0;
}
