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
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "config.h"

static const char* head =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
    "<epp xmlns=\"urn:ietf:params:xml:ns:epp-1.0\"\n"
    " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
    " xsi:schemaLocation=\"urn:ietf:params:xml:ns:epp-1.0 epp-1.0.xsd\">\n";

static const char* foot = "</epp>\n";

static int sockfd = -1;
static SSL* ssl = NULL;

static xmlXPathContext* xml_parse(char* string)
{
    xmlDoc* doc = xmlParseMemory(string, strlen(string));
    if (!doc) {
        syslog(LOG_DEBUG, "error: could not parse string: %s", string);
        return NULL;
    }

    xmlXPathContext* context = xmlXPathNewContext(doc);
    if(!context) {
        syslog(LOG_DEBUG,"error: unable to create new XPath context\n");
        xmlFreeDoc(doc); 
        return NULL;
    }

    xmlXPathRegisterNs(context, (xmlChar*)"epp", (xmlChar*)"urn:ietf:params:xml:ns:epp-1.0");
    xmlXPathRegisterNs(context, (xmlChar*)"domain", (xmlChar*)"urn:ietf:params:xml:ns:domain-1.0");
    xmlXPathRegisterNs(context, (xmlChar*)"secdns", (xmlChar*)"urn:ietf:params:xml:ns:secDNS-1.0");
    
    return context;
}

static char* xml_get(xmlXPathContext* xml, char* path)
{
    char* result = NULL;

    xmlXPathObject* obj = xmlXPathEvalExpression((xmlChar*)path, xml);
    if (obj) {
        if (obj->nodesetval->nodeNr) {
            xmlNode* node = obj->nodesetval->nodeTab[0];
            if (node && node->children && node->children->content)
                result = strdup((char*)(node->children->content));
        }
        xmlXPathFreeObject(obj);
    }
    else
        syslog(LOG_DEBUG,
               "Error: unable to evaluate xpath expression '%s'\n", path);
    
    return result;
}

static void xml_free(xmlXPathContext* xml)
{
    xmlDoc* doc = xml->doc;
    xmlXPathFreeContext(xml);
    xmlFreeDoc(doc);
}

static xmlXPathContext* read_frame(void)
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
    int len = ntohl(*((uint32_t*)buffer));
    len -= 4;

    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1; /* leave room for \0 */
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
    buffer[len] = 0;

#ifdef DEBUG
    FILE* f = fopen("input.xml", "w");
    if (f) {
        fwrite(buffer, len, 1, f);
        fclose(f);
    }
#endif

    return xml_parse(buffer);
}

static int send_frame(char* ptr, int len)
{
    unsigned char buf[4];

    *((uint32_t*)buf) = htonl(len+4);

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
        fwrite(ptr, len, 1, f);
        fclose(f);
    }
#endif

    return 0;
}


static int read_response(xmlXPathContext** return_xml)
{
    int rc = -1;
    xmlXPathContext* xml = read_frame();

    char* code = xml_get(xml, "//epp:result/@code");
    char* msg = xml_get(xml, "//epp:result/epp:msg");

    if (code && msg) {
        syslog(LOG_DEBUG, "<< result %s (%s)", code, msg);
                
        rc = atoi(code);

        free(msg);
        free(code);
    }
    else
        syslog(LOG_ERR, "No <msg> in <result>");
    
    if (rc != 1000) {
        char* reason = xml_get(xml, "//epp:reason");
        if (reason) {
            syslog(LOG_DEBUG, "Failure reson: %s", reason);
            free(reason);
        }
        else
            syslog(LOG_DEBUG, "No failure reason in response");
    }
    else
        rc = 0;

    if (return_xml)
        *return_xml = xml;
    else
        xml_free(xml);

    return rc;
}

static xmlXPathContext* read_greeting(void)
{
    xmlXPathContext* response = read_frame();
    char* version = xml_get(response, "//epp:svcMenu/epp:version");
    if (version) {
        syslog(LOG_DEBUG, "<< greeting %s", version);
        free(version);

        char* dnssec = xml_get(response, "//epp:svcExtension[epp:extURI = \"urn:ietf:params:xml:ns:secDNS-1.0\"]");
        if (!dnssec) {
            syslog(LOG_ERR, "Server doesn't support DNSSEC extension");
            xml_free(response);
            return NULL;
        }
        else
            free(dnssec);
    }
    else {
        syslog(LOG_ERR, "No <version> in xml");
        xml_free(response);
        return NULL;
    }

    return response;
}

static void cleanup(void)
{
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
}

static int login(xmlXPathContext* greeting)
{
    char* version = xml_get(greeting, "//epp:svcMenu/epp:version");
    if (!version) {
        syslog(LOG_ERR, "No <version> in greeting!");
        return -1;
    }

    char* lang = xml_get(greeting, "//epp:svcMenu/epp:lang");
    if (!lang) {
        syslog(LOG_ERR, "No <lang> in greeting!");
        free(version);
        return -1;
    }
    xml_free(greeting);
    
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

    if (read_response(NULL))
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

    xmlXPathContext* greeting = read_greeting();
    if (greeting)
        if (login(greeting))
            return -1;

    return 0;
}

int epp_change_key(void)
{
    char buffer[4096];
    snprintf(buffer, sizeof buffer,
             "%s"
             "<command>\n"
             " <update>\n"
             "  <domain:update\n"
             "   xmlns:domain=\"urn:ietf:params:xml:ns:domain-1.0\"\n"
             "   xsi:schemaLocation=\"urn:ietf:params:xml:ns:domain-1.0 domain-1.0.xsd\">\n"
             "    <domain:name>eppclient.se</domain:name>\n"
             "  </domain:update>\n"
             " </update>\n"
             " <extension>\n"
             "  <secDNS:update\n"
             "   xmlns:secDNS=\"urn:ietf:params:xml:ns:secDNS-1.0\"\n"
             "   xsi:schemaLocation=\"urn:ietf:params:xml:ns:secDNS-1.0 secDNS-1.0.xsd\">\n"
             "   <secDNS:chg>\n"
             "    <secDNS:dsData>\n"
             "      <secDNS:keyTag>22133</secDNS:keyTag>\n"
             "      <secDNS:alg>3</secDNS:alg>\n"
             "      <secDNS:digestType>1</secDNS:digestType>\n"
             "      <secDNS:digest>0774AB2F48D0FBD0AB0FD3F5E80C495C48046E3D</secDNS:digest>\n"
             "    </secDNS:dsData>\n"
             "   </secDNS:chg>\n"
             "  </secDNS:update>\n"
             " </extension>\n"
             "</command>\n"
             "%s",
             head,
             foot);

    syslog(LOG_DEBUG,">> change key");
    send_frame(buffer, strlen(buffer));

    if (read_response(NULL))
        return -1;

    return 0;
}
