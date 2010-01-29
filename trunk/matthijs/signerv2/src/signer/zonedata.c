/*
 * $Id$
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
 * Zone data.
 *
 */

#include "config.h"
#include "signer/zonedata.h"
#include "signer/domain.h"
#include "util/log.h"
#include "util/se_malloc.h"

#include <ldns/ldns.h>


/**
 * Compare domains.
 *
 */
static int
domain_compare(const void* a, const void* b)
{
    ldns_rdf* x = (ldns_rdf*)a;
    ldns_rdf* y = (ldns_rdf*)b;
    return ldns_dname_compare(x, y);
}


/**
 * Create empty zone data..
 *
 */
zonedata_type*
zonedata_create(void)
{
    zonedata_type* zd = (zonedata_type*) se_malloc(sizeof(zonedata_type));

    se_log_debug2("create zone data");
    zd->domains = ldns_rbtree_create(domain_compare);
    se_log_debug3("create zone data: done");
    return zd;
}


/**
 * Convert a domain to a tree node.
 *
 */
static ldns_rbnode_t*
domain2node(domain_type* domain)
{
    ldns_rbnode_t* node = (ldns_rbnode_t*) se_malloc(sizeof(ldns_rbnode_t));
    node->key = domain->name;
    node->data = domain;
    return node;
}


/**
 * Add a domain to the zone data.
 *
 */
domain_type*
zonedata_add_domain(zonedata_type* zd, domain_type* domain)
{
    ldns_rbnode_t* new_node = NULL;
    char* str;

    se_log_assert(zd);
    se_log_assert(zd->domains);
    se_log_assert(domain);
    se_log_debug2("add domain");

    if (zonedata_lookup_domain(zd, domain) != NULL) {
        se_log_debug("domain already present");
        domain_cleanup(domain);
        return NULL;
    }

    new_node = domain2node(domain);
    if (ldns_rbtree_insert(zd->domains, new_node) == NULL) {
        str = ldns_rdf2str(domain->name);
        se_log_error("unable to add domain '%s'", domain->name);
        se_free((void*)str);
        domain_cleanup(domain);
        se_free((void*) new_node);
        return NULL;
    }
    domain->just_added = 1;
    se_log_debug2("add domain: done");
    return domain;
}


/**
 * Lookup domain.
 *
 */
domain_type*
zonedata_lookup_domain(zonedata_type* zd, domain_type* domain)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;

    se_log_assert(zd);
    se_log_assert(zd->domains);
    se_log_assert(domain);
    se_log_debug3("look up domain");

    node = ldns_rbtree_search(zd->domains, domain->name);
    if (node) {
        return (domain_type*) node->key;
    }
    return NULL;
}


/**
 * Clean up zone data.
 *
 */
void
zonedata_cleanup(zonedata_type* zonedata)
{
    ldns_rbnode_t* node = NULL;
//    domain_type* name = NULL;

    /* destroy domains */
    if (zonedata) {
        se_log_debug2("clean up zone data");
        if (zonedata->domains) {
            node = ldns_rbtree_first(zonedata->domains);
        }
        while (node && node != LDNS_RBTREE_NULL) {
//            name = (domain_type*) node->data;
//            domain_delete(name);
            node = ldns_rbtree_next(node);
        }
        se_rbnode_free(zonedata->domains->root);
        ldns_rbtree_free(zonedata->domains);

        se_free((void*) zonedata);
        se_log_debug3("clean up zone data: done");
    } else {
        se_log_debug3("clean up zone data: null");
    }
}


/**
 * Print zone data.
 *
 */
void
zonedata_print(FILE* fd, zonedata_type* zonedata, int skip_soa)
{
    ldns_rbnode_t* node = NULL;
//    domain_type* name = NULL;

    se_log_assert(fd);
    se_log_debug3("print zone data");

    /* destroy domains */
    if (zonedata && zonedata->domains) {
        while (node && node != LDNS_RBTREE_NULL) {
           fprintf(fd, "; domain\n");
//            name = (domain_type*) node->data;
//            domain_print(name);
            node = ldns_rbtree_next(node);
        }
    } else {
    }

    se_log_debug3("print zone data: null");
}
