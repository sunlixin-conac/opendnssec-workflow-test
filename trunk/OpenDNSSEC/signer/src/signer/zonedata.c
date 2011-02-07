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
#include "shared/allocator.h"
#include "shared/log.h"
#include "shared/util.h"
#include "signer/backup.h"
#include "signer/domain.h"
#include "signer/nsec3params.h"
#include "signer/zonedata.h"
#include "util/file.h"
#include "util/se_malloc.h"

#include <ldns/ldns.h> /* ldns_dname_*(), ldns_rbtree_*() */

static const char* zd_str = "data";


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
zonedata_create(allocator_type* allocator)
{
    zonedata_type* zd = NULL;

    if (!allocator) {
        ods_log_error("[%s] cannot create zonedata: no allocator", zd_str);
        return NULL;
    }
    ods_log_assert(allocator);

    zd = (zonedata_type*) allocator_alloc(allocator, sizeof(zonedata_type));
    if (!zd) {
        ods_log_error("[%s] cannot create zonedata: allocator failed", zd_str);
        return NULL;
    }
    ods_log_assert(zd);

    zd->domains = ldns_rbtree_create(domain_compare);
    zd->initialized = 0;
    zd->nsec3_domains = NULL;
    zd->inbound_serial = 0;
    zd->internal_serial = 0;
    zd->outbound_serial = 0;
    zd->default_ttl = 3600; /* configure --default-ttl option? */
    return zd;
}


/**
 * Convert a domain to a tree node.
 *
 */
static ldns_rbnode_t*
domain2node(domain_type* domain)
{
    ldns_rbnode_t* node = (ldns_rbnode_t*) malloc(sizeof(ldns_rbnode_t));
    if (!node) {
        return NULL;
    }
    node->key = domain->dname;
    node->data = domain;
    return node;
}


/**
 * Recover zone data from backup.
 *
 */
int
zonedata_recover_from_backup(zonedata_type* zd, FILE* fd)
{
    int corrupted = 0;
    const char* token = NULL;
    domain_type* current_domain = NULL;
    ldns_rdf* parent_rdf = NULL;
    ldns_rr* rr = NULL;
    ldns_status status = LDNS_STATUS_OK;
    ldns_rbnode_t* new_node = LDNS_RBTREE_NULL;

    ods_log_assert(zd);
    ods_log_assert(fd);

    if (!backup_read_check_str(fd, ODS_SE_FILE_MAGIC)) {
        corrupted = 1;
    }

    while (!corrupted) {
        if (backup_read_str(fd, &token)) {
            if (ods_strcmp(token, ";DNAME") == 0) {
                current_domain = domain_recover_from_backup(fd);
                if (!current_domain) {
                    ods_log_error("[%s] error reading domain from backup file", zd_str);
                    corrupted = 1;
                } else {
                    parent_rdf = ldns_dname_left_chop(current_domain->dname);
                    if (!parent_rdf) {
                        ods_log_error("[%s] unable to create parent domain name (rdf)",
                            zd_str);
                        corrupted = 1;
                    } else {
                        current_domain->parent =
                            zonedata_lookup_domain(zd, parent_rdf);
                        ldns_rdf_deep_free(parent_rdf);
                        ods_log_assert(current_domain->parent ||
                            current_domain->dstatus == DOMAIN_STATUS_APEX);

                        new_node = domain2node(current_domain);
                        if (!zd->domains) {
                            zd->domains = ldns_rbtree_create(domain_compare);
                        }
                        if (ldns_rbtree_insert(zd->domains, new_node) == NULL) {
                            ods_log_error("[%s] error adding domain from backup file",
                                zd_str);
                            se_free((void*)new_node);
                            corrupted = 1;
                        }
                        new_node = NULL;
                    }
                }
            } else if (ods_strcmp(token, ";DNAME3") == 0) {
                ods_log_assert(current_domain);
                current_domain->nsec3 = domain_recover_from_backup(fd);
                if (!current_domain->nsec3) {
                    ods_log_error("[%s] error reading nsec3 domain from backup file",
                        zd_str);
                    corrupted = 1;
                } else {
                    current_domain->nsec3->nsec3 = current_domain;
                    new_node = domain2node(current_domain->nsec3);
                    if (!zd->nsec3_domains) {
                        zd->nsec3_domains = ldns_rbtree_create(domain_compare);
                    }

                    if (ldns_rbtree_insert(zd->nsec3_domains, new_node) == NULL) {
                        ods_log_error("[%s] error adding nsec3 domain from backup file",
                            zd_str);
                        se_free((void*)new_node);
                        corrupted = 1;
                    }
                    new_node = NULL;
                }
            } else if (ods_strcmp(token, ";NSEC") == 0) {
                status = ldns_rr_new_frm_fp(&rr, fd, NULL, NULL, NULL);
                if (status != LDNS_STATUS_OK) {
                    ods_log_error("[%s] error reading NSEC RR from backup file", zd_str);
                    if (rr) {
                        ldns_rr_free(rr);
                    }
                    corrupted = 1;
                } else {
                    ods_log_assert(current_domain);
                    current_domain->nsec_rrset = rrset_create_frm_rr(rr);
                    if (!current_domain->nsec_rrset) {
                        ods_log_error("[%s] error adding NSEC RR from backup file", zd_str);
                        corrupted = 1;
                    }
                }
                rr = NULL;
                status = LDNS_STATUS_OK;
            } else if (ods_strcmp(token, ";NSEC3") == 0) {
                status = ldns_rr_new_frm_fp(&rr, fd, NULL, NULL, NULL);
                if (status != LDNS_STATUS_OK) {
                    ods_log_error("[%s] error reading NSEC3 RR from backup file", zd_str);
                    if (rr) {
                        ldns_rr_free(rr);
                    }
                    corrupted = 1;
                } else {
                    ods_log_assert(current_domain);
                    ods_log_assert(current_domain->nsec3);
                    current_domain->nsec3->nsec_rrset = rrset_create_frm_rr(rr);
                    if (!current_domain->nsec3->nsec_rrset) {
                        ods_log_error("[%s] error adding NSEC3 RR from backup file", zd_str);
                        corrupted = 1;
                    }
                }
                rr = NULL;
                status = LDNS_STATUS_OK;
            } else if (ods_strcmp(token, ODS_SE_FILE_MAGIC) == 0) {
                se_free((void*)token);
                token = NULL;
                break;
            } else {
                corrupted = 1;
            }
            se_free((void*)token);
            token = NULL;
        } else {
            corrupted = 1;
        }
    }

    return corrupted;
}


/**
 * Internal lookup domain function.
 *
 */
static domain_type*
zonedata_domain_search(ldns_rbtree_t* tree, ldns_rdf* dname)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;

    if (!tree || !dname) {
        return NULL;
    }
    node = ldns_rbtree_search(tree, dname);
    if (node && node != LDNS_RBTREE_NULL) {
        return (domain_type*) node->data;
    }
    return NULL;
}


/**
 * Lookup domain in NSEC3 space.
 *
 */
static domain_type*
zonedata_lookup_domain_nsec3(zonedata_type* zd, ldns_rdf* name)
{
    ods_log_assert(zd);
    ods_log_assert(zd->nsec3_domains);
    ods_log_assert(name);
    return zonedata_domain_search(zd->nsec3_domains, name);
}


/**
 * Lookup domain.
 *
 */
domain_type*
zonedata_lookup_domain(zonedata_type* zd, ldns_rdf* dname)
{
    if (!zd || !zd->domains | !dname) {
        return NULL;
    }
    return zonedata_domain_search(zd->domains, dname);
}


/**
 * Add a NSEC3 domain to the zone data.
 *
 */
static domain_type*
zonedata_add_domain_nsec3(zonedata_type* zd, domain_type* domain,
    ldns_rdf* apex, nsec3params_type* nsec3params)
{
    ldns_rbnode_t* new_node = LDNS_RBTREE_NULL;
    ldns_rbnode_t* prev_node = LDNS_RBTREE_NULL;
    domain_type* nsec3_domain = NULL;
    domain_type* prev_domain = NULL;
    ldns_rdf* hashed_ownername = NULL;
    ldns_rdf* hashed_label = NULL;
    char* str = NULL;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);
    ods_log_assert(zd->nsec3_domains);
    ods_log_assert(domain);
    ods_log_assert(domain->rrsets);

    /**
     * The owner name of the NSEC3 RR is the hash of the original owner
     * name, prepended as a single label to the zone name.
     */
    hashed_label = ldns_nsec3_hash_name(domain->dname,
        nsec3params->algorithm, nsec3params->iterations,
        nsec3params->salt_len, nsec3params->salt_data);
    hashed_ownername = ldns_dname_cat_clone(
        (const ldns_rdf*) hashed_label,
        (const ldns_rdf*) apex);
    ldns_rdf_deep_free(hashed_label);

    nsec3_domain = zonedata_lookup_domain_nsec3(zd, hashed_ownername);
    if (!nsec3_domain) {
        nsec3_domain = domain_create(hashed_ownername);
        nsec3_domain->dstatus = DOMAIN_STATUS_HASH;
        ldns_rdf_deep_free(hashed_ownername);
        new_node = domain2node(nsec3_domain);
        if (!ldns_rbtree_insert(zd->nsec3_domains, new_node)) {
            str = ldns_rdf2str(nsec3_domain->dname);
            ods_log_error("[%s] unable to add NSEC3 domain %s", zd_str,
                str?str:"(null)");
            se_free((void*)str);
            se_free((void*)new_node);
            domain_cleanup(nsec3_domain);
            return NULL;
        }
        nsec3_domain->nsec_nxt_changed = 1;
        /* mark the change in the previous NSEC3 domain */
        prev_node = ldns_rbtree_previous(new_node);
        if (!prev_node || prev_node == LDNS_RBTREE_NULL) {
            prev_node = ldns_rbtree_last(zd->nsec3_domains);
        }
        ods_log_assert(prev_node);
        prev_domain = (domain_type*) prev_node->data;
        ods_log_assert(prev_domain);
        prev_domain->nsec_nxt_changed = 1;
        return nsec3_domain;
    } else {
        str = ldns_rdf2str(hashed_ownername);
        ldns_rdf_deep_free(hashed_ownername);
        ods_log_error("[%s] unable to add NSEC3 domain %s (has collision?) ",
            zd_str, str?str:"(null)");
        se_free((void*)str);
        return NULL;
    }
    return nsec3_domain;
}


/**
 * Add a domain to the zone data.
 *
 */
domain_type*
zonedata_add_domain(zonedata_type* zd, domain_type* domain)
{
    ldns_rbnode_t* new_node = LDNS_RBTREE_NULL;
    ldns_rbnode_t* prev_node = LDNS_RBTREE_NULL;
    domain_type* prev_domain = NULL;
    char* str = NULL;

    if (!domain) {
        ods_log_error("[%s] unable to add domain: no domain", zd_str);
        return NULL;
    }
    ods_log_assert(domain);

    if (!zd || !zd->domains) {
        str = ldns_rdf2str(domain->dname);
        ods_log_error("[%s] unable to add domain %s: no storage", zd_str,
            str?str:"(null)");
        free(str);
        return NULL;
    }
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    new_node = domain2node(domain);
    if (ldns_rbtree_insert(zd->domains, new_node) == NULL) {
        str = ldns_rdf2str(domain->dname);
        ods_log_error("[%s] unable to add domain %s: already present", zd_str,
            str?str:"(null)");
        free((void*)str);
        free((void*)new_node);
        return NULL;
    }
    str = ldns_rdf2str(domain->dname);
    ods_log_deeebug("+DD %s", str?str:"(null)");
    free((void*) str);
    domain->dstatus = DOMAIN_STATUS_NONE;
    domain->nsec_bitmap_changed = 1;
    domain->nsec_nxt_changed = 1;
    /* mark previous domain for NSEC */
    domain->nsec_nxt_changed = 1;
    prev_node = ldns_rbtree_previous(new_node);
    if (!prev_node || prev_node == LDNS_RBTREE_NULL) {
        prev_node = ldns_rbtree_last(zd->domains);
    }
    ods_log_assert(prev_node);
    ods_log_assert(prev_node->data);
    prev_domain = (domain_type*) prev_node->data;
    prev_domain->nsec_nxt_changed = 1;
    return domain;
}


/**
 * Internal delete domain function.
 *
 */
static domain_type*
zonedata_del_domain_fixup(ldns_rbtree_t* tree, domain_type* domain)
{
    domain_type* del_domain = NULL;
    domain_type* prev_domain = NULL;
    ldns_rbnode_t* del_node = LDNS_RBTREE_NULL;
    ldns_rbnode_t* prev_node = LDNS_RBTREE_NULL;
    char* str = NULL;

    ods_log_assert(tree);
    ods_log_assert(domain);
    ods_log_assert(domain->dname);

    del_node = ldns_rbtree_search(tree, (const void*)domain->dname);
    if (del_node) {
        /**
         * [CALC] if domain removed, mark previous domain NSEC(3) nxt changed.
         *
         */
        prev_node = ldns_rbtree_previous(del_node);
        if (!prev_node || prev_node == LDNS_RBTREE_NULL) {
            prev_node = ldns_rbtree_last(tree);
        }
        ods_log_assert(prev_node);
        ods_log_assert(prev_node->data);
        prev_domain = (domain_type*) prev_node->data;
        prev_domain->nsec_nxt_changed = 1;

        del_node = ldns_rbtree_delete(tree, (const void*)domain->dname);
        del_domain = (domain_type*) del_node->data;
        if (domain->parent) {
            domain->parent->subdomain_count -= 1;
            if (domain->dstatus == DOMAIN_STATUS_AUTH ||
                domain->dstatus == DOMAIN_STATUS_DS) {
                domain->parent->subdomain_auth -= 1;
            }
        }
        domain_cleanup(del_domain);
        free((void*)del_node);
        return NULL;
    } else {
        str = ldns_rdf2str(domain->dname);
        ods_log_error("[%s] unable to del domain %s: not found", zd_str,
            str?str:"(null)");
        free((void*)str);
    }
    return domain;
}


/**
 * Delete a NSEC3 domain from the zone data.
 *
 */
static domain_type*
zonedata_del_domain_nsec3(zonedata_type* zd, domain_type* domain)
{
    ods_log_assert(zd);
    ods_log_assert(zd->nsec3_domains);
    ods_log_assert(domain);
    return zonedata_del_domain_fixup(zd->nsec3_domains, domain);
}


/**
 * Delete a domain from the zone data.
 *
 */
domain_type*
zonedata_del_domain(zonedata_type* zd, domain_type* domain)
{
    domain_type* nsec3_domain = NULL;
    char* str = NULL;

    if (!domain) {
        ods_log_error("[%s] unable to delete domain: no domain", zd_str);
        return NULL;
    }
    ods_log_assert(domain);
    ods_log_assert(domain->dname);

    if (!zd || !zd->domains) {
        str = ldns_rdf2str(domain->dname);
        ods_log_error("[%s] unable to delete domain %s: no storage", zd_str,
            str?str:"(null)");
        free((void*)str);
        return domain;
    }
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    str = ldns_rdf2str(domain->dname);
    ods_log_deeebug("-DD %s", str?str:"(null)");
    if (domain->nsec3) {
        nsec3_domain = zonedata_del_domain_nsec3(zd, domain->nsec3);
        if (nsec3_domain) {
            ods_log_error("[%s] failed to delete corresponding NSEC3 domain, "
                "deleting domain %s", zd_str, str?str:"(null)");
        }
    }
    free((void*) str);
    return zonedata_del_domain_fixup(zd->domains, domain);
}


/**
 * Add empty non-terminals to a domain in the zone data.
 *
 */
static int
zonedata_domain_entize(zonedata_type* zd, domain_type* domain, ldns_rdf* apex)
{
    int ent2unsigned_deleg = 0;
    ldns_rdf* parent_rdf = NULL;
    domain_type* parent_domain = NULL;
    char* str = NULL;

    ods_log_assert(apex);
    ods_log_assert(domain);
    ods_log_assert(domain->dname);
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    if (domain->parent) {
        /* domain already has parent */
        return 0;
    }

    if (domain_lookup_rrset(domain, LDNS_RR_TYPE_NS) &&
        !domain_lookup_rrset(domain, LDNS_RR_TYPE_DS)) {
        /* empty non-terminal to unsigned delegation */
        ent2unsigned_deleg = 1;
    }

    while (domain && ldns_dname_is_subdomain(domain->dname, apex) &&
           ldns_dname_compare(domain->dname, apex) != 0) {

        str = ldns_rdf2str(domain->dname);

        /**
         * RFC5155:
         * 4. If the difference in number of labels between the apex and
         *    the original owner name is greater than 1, additional NSEC3
         *    RRs need to be added for every empty non-terminal between
         *     the apex and the original owner name.
         */
        parent_rdf = ldns_dname_left_chop(domain->dname);
        if (!parent_rdf) {
            ods_log_error("[%s] unable to entize domain %s: left chop failed",
                zd_str, str);
            se_free((void*)str);
            return 1;
        }
        ods_log_assert(parent_rdf);

        parent_domain = zonedata_lookup_domain(zd, parent_rdf);
        if (!parent_domain) {
            parent_domain = domain_create(parent_rdf);
            ldns_rdf_deep_free(parent_rdf);
            if (!parent_domain) {
                ods_log_error("[%s] unable to entize domain %s: create parent "
                    "failed", zd_str, str);
                se_free((void*)str);
                return 1;
            }
            parent_domain = zonedata_add_domain(zd, parent_domain);
            if (!parent_domain) {
                ods_log_error("[%s] unable to entize domain %s: add parent "
                    "failed", zd_str, str);
                se_free((void*)str);
                return 1;
            }
            parent_domain->dstatus =
                (ent2unsigned_deleg?DOMAIN_STATUS_ENT_NS:
                                    DOMAIN_STATUS_ENT_AUTH);
            parent_domain->subdomain_count = 1;
            if (!ent2unsigned_deleg) {
                parent_domain->subdomain_auth = 1;
            }
            parent_domain->internal_serial = domain->internal_serial;
            domain->parent = parent_domain;
            /* continue with the parent domain */
            domain = parent_domain;
        } else {
            ldns_rdf_deep_free(parent_rdf);
            parent_domain->internal_serial = domain->internal_serial;
            parent_domain->subdomain_count += 1;
            if (!ent2unsigned_deleg) {
                parent_domain->subdomain_auth += 1;
            }
            domain->parent = parent_domain;
            if (domain_count_rrset(parent_domain) <= 0 &&
                parent_domain->dstatus != DOMAIN_STATUS_ENT_AUTH) {
                parent_domain->dstatus =
                    (ent2unsigned_deleg?DOMAIN_STATUS_ENT_NS:
                                        DOMAIN_STATUS_ENT_AUTH);
            }
            /* done */
            domain = NULL;
        }
        se_free((void*)str);
    }
    return 0;
}


/**
 * Revise the empty non-terminals domain status.
 *
 */
static void
zonedata_domain_entize_revised(domain_type* domain, int status)
{
    domain_type* parent = NULL;
    if (!domain) {
        return;
    }
    parent = domain->parent;
    while (parent) {
        if (parent->dstatus == DOMAIN_STATUS_ENT_AUTH ||
            parent->dstatus == DOMAIN_STATUS_ENT_GLUE ||
            parent->dstatus == DOMAIN_STATUS_ENT_NS) {
            parent->dstatus = status;
        } else {
           break;
        }
        parent = parent->parent;
    }
    return;
}


/**
 * Add empty non-terminals to zone data.
 *
 */
ods_status
zonedata_entize(zonedata_type* zd, ldns_rdf* apex)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;
    int prev_status = DOMAIN_STATUS_NONE;

    if (!zd || !zd->domains) {
        ods_log_error("[%s] unable to entize zone data: no zone data",
            zd_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    if (!apex) {
        ods_log_error("[%s] unable to entize zone data: no zone apex",
            zd_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(apex);

    node = ldns_rbtree_first(zd->domains);
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        if (zonedata_domain_entize(zd, domain, apex) != 0) {
            ods_log_error("[%s] unable to entize zone data: entize domain "
                "failed", zd_str);
            return ODS_STATUS_ERR;
        }
        /* domain has parent now, check for glue */
        prev_status = domain->dstatus;
        domain_update_status(domain);
        if (domain->dstatus == DOMAIN_STATUS_OCCLUDED &&
            prev_status != DOMAIN_STATUS_OCCLUDED) {
            zonedata_domain_entize_revised(domain, DOMAIN_STATUS_ENT_GLUE);
        }
        node = ldns_rbtree_next(node);
    }
    return ODS_STATUS_OK;
}


/**
 * Add NSEC records to zonedata.
 *
 */
ods_status
zonedata_nsecify(zonedata_type* zd, ldns_rr_class klass, stats_type* stats)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;
    domain_type* to = NULL;
    domain_type* apex = NULL;
    int have_next = 0;

    if (!zd || !zd->domains) {
        return ODS_STATUS_OK;
    }
    node = ldns_rbtree_first(zd->domains);
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        if (domain->dstatus == DOMAIN_STATUS_APEX) {
            apex = domain;
        }
        /* don't do glue-only or empty domains */
        if (domain->dstatus == DOMAIN_STATUS_NONE ||
            domain->dstatus == DOMAIN_STATUS_OCCLUDED ||
            domain_count_rrset(domain) <= 0) {
            node = ldns_rbtree_next(node);
            continue;
        }
        node = ldns_rbtree_next(node);
        have_next = 0;
        while (!have_next) {
            if (node && node != LDNS_RBTREE_NULL) {
                to = (domain_type*) node->data;
            } else if (apex) {
                to = apex;
            } else {
                ods_log_alert("[%s] unable to nsecify: apex undefined",
                    zd_str);
                return ODS_STATUS_ERR;
            }
            /* don't do glue-only or empty domains */
            if (to->dstatus == DOMAIN_STATUS_NONE ||
                to->dstatus == DOMAIN_STATUS_OCCLUDED ||
                domain_count_rrset(to) <= 0) {
                node = ldns_rbtree_next(node);
            } else {
                have_next = 1;
            }
        }
        /* ready to add the NSEC record */
        if (domain_nsecify(domain, to, zd->default_ttl, klass, stats) != 0) {
            ods_log_error("[%s] unable to nsecify: add NSEC to domain failed",
                zd_str);
            return ODS_STATUS_ERR;
        }
    }
    return ODS_STATUS_OK;
}


/**
 * Add NSEC3 records to zonedata.
 *
 */
ods_status
zonedata_nsecify3(zonedata_type* zd, ldns_rr_class klass,
    nsec3params_type* nsec3params, stats_type* stats)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    ldns_rbnode_t* nsec3_node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;
    domain_type* to = NULL;
    domain_type* apex = NULL;
    char* str = NULL;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);
    ods_log_assert(nsec3params);

    if (!zd->nsec3_domains) {
        ods_log_debug("[%s] create new nsec3 domain tree", zd_str);
        zd->nsec3_domains = ldns_rbtree_create(domain_compare);
    }

    node = ldns_rbtree_first(zd->domains);
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        if (domain->dstatus == DOMAIN_STATUS_APEX) {
            apex = domain;
        }

        /* don't do glue-only domains */
        if (domain->dstatus == DOMAIN_STATUS_NONE ||
            domain->dstatus == DOMAIN_STATUS_OCCLUDED ||
            domain->dstatus == DOMAIN_STATUS_ENT_GLUE) {
            str = ldns_rdf2str(domain->dname);
            ods_log_debug("[%s] nsecify3: skip glue domain %s", zd_str,
                str?str:"(null)");
            se_free((void*) str);

            node = ldns_rbtree_next(node);
            continue;
        }
        /* Opt-Out? */
        if (nsec3params->flags) {
            /* If Opt-Out is being used, owner names of unsigned delegations
               MAY be excluded. */
            if (domain->dstatus == DOMAIN_STATUS_ENT_NS ||
                domain->dstatus == DOMAIN_STATUS_NS) {
                str = ldns_rdf2str(domain->dname);
                ods_log_debug("[%s] opt-out %s: %s", zd_str, str?str:"(null)",
                    domain->dstatus == DOMAIN_STATUS_NS ?
                    "unsigned delegation" : "empty non-terminal (to unsigned "
                    "delegation)");
                se_free((void*) str);
                node = ldns_rbtree_next(node);
                continue;
            }
        }

        if (!apex) {
            ods_log_alert("[%s] apex undefined!, aborting nsecify3", zd_str);
            return ODS_STATUS_ERR;
        }

        /* add the NSEC3 domain */
        if (!domain->nsec3) {
            domain->nsec3 = zonedata_add_domain_nsec3(zd, domain, apex->dname,
                nsec3params);
            str = ldns_rdf2str(domain->dname);
            if (domain->nsec3 == NULL) {
                ods_log_alert("[%s] failed to add NSEC3 domain for %s", zd_str,
                    str?str:"(null)");
                se_free((void*) str);
                return ODS_STATUS_ERR;
            } else {
                ods_log_deeebug("[%s] NSEC3 domain added for %s", zd_str,
                    str?str:"(null)");
                se_free((void*) str);
            }
            domain->nsec3->nsec3 = domain; /* back reference */
        } else {
            ods_log_deeebug("[%s] domain already has NSEC3 domain", zd_str);
        }

        /* The Next Hashed Owner Name field is left blank for the moment. */

        /**
         * Additionally, for collision detection purposes, optionally
         * create an additional NSEC3 RR corresponding to the original
         * owner name with the asterisk label prepended (i.e., as if a
         * wildcard existed as a child of this owner name) and keep track
         * of this original owner name. Mark this NSEC3 RR as temporary.
        **/
        /* [TODO] */
        /**
         * pseudo:
         * wildcard_name = *.domain->dname;
         * hashed_ownername = ldns_nsec3_hash_name(domain->dname,
               nsec3params->algorithm, nsec3params->iterations,
               nsec3params->salt_len, nsec3params->salt);
         * domain->nsec3_wildcard = domain_create(hashed_ownername);
        **/

        node = ldns_rbtree_next(node);
    }

    /* Now we have the complete NSEC3 tree */

    /**
     * In each NSEC3 RR, insert the next hashed owner name by using the
     * value of the next NSEC3 RR in hash order.  The next hashed owner
     * name of the last NSEC3 RR in the zone contains the value of the
     * hashed owner name of the first NSEC3 RR in the hash order.
    **/
    node = ldns_rbtree_first(zd->nsec3_domains);
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        nsec3_node = ldns_rbtree_next(node);
        if (!nsec3_node || nsec3_node == LDNS_RBTREE_NULL) {
             nsec3_node = ldns_rbtree_first(zd->nsec3_domains);
        }
        to = (domain_type*) nsec3_node->data;

        /* ready to add the NSEC3 record */
        if (domain_nsecify3(domain, to, zd->default_ttl, klass,
            nsec3params, stats) != 0) {
            ods_log_error("[%s] adding NSEC3s to domain failed", zd_str);
            return ODS_STATUS_ERR;
        }
        node = ldns_rbtree_next(node);
    }

    return ODS_STATUS_OK;
}


static int
ods_max(uint32_t a, uint32_t b)
{
    return (a>b?a:b);
}


/**
 * Update the serial.
 *
 */
static int
zonedata_update_serial(zonedata_type* zd, signconf_type* sc)
{
    uint32_t soa = 0;
    uint32_t prev = 0;
    uint32_t update = 0;

    ods_log_assert(zd);
    ods_log_assert(sc);

    prev = zd->internal_serial;
    ods_log_debug("[%s] update serial: inbound=%u internal=%u outbound=%u now=%u",
        zd_str, zd->inbound_serial, zd->internal_serial, zd->outbound_serial,
        (uint32_t) time_now());

    if (!sc->soa_serial) {
        ods_log_error("[%s] no serial type given", zd_str);
        return 1;
    }

    if (ods_strcmp(sc->soa_serial, "unixtime") == 0) {
        soa = ods_max(zd->inbound_serial, (uint32_t) time_now());
        if (!DNS_SERIAL_GT(soa, prev)) {
            soa = prev + 1;
        }
        update = soa - prev;
    } else if (strncmp(sc->soa_serial, "counter", 7) == 0) {
        soa = ods_max(zd->inbound_serial, prev);
        if (!zd->initialized) {
            zd->internal_serial = soa + 1;
            zd->initialized = 1;
            return 0;
        }
        if (!DNS_SERIAL_GT(soa, prev)) {
            soa = prev + 1;
        }
        update = soa - prev;
    } else if (strncmp(sc->soa_serial, "datecounter", 11) == 0) {
        soa = (uint32_t) time_datestamp(0, "%Y%m%d", NULL) * 100;
        soa = ods_max(zd->inbound_serial, soa);
        if (!DNS_SERIAL_GT(soa, prev)) {
            soa = prev + 1;
        }
        update = soa - prev;
    } else if (strncmp(sc->soa_serial, "keep", 4) == 0) {
        soa = zd->inbound_serial;
        if (zd->initialized && !DNS_SERIAL_GT(soa, prev)) {
            ods_log_error("[%s] cannot keep SOA SERIAL from input zone "
                " (%u): output SOA SERIAL is %u", zd_str, soa, prev);
            return 1;
        }
        prev = soa;
        update = 0;
    } else {
        ods_log_error("[%s] unknown serial type %s", zd_str, sc->soa_serial);
        return 1;
    }

    if (!zd->initialized) {
        zd->initialized = 1;
    }

    /* serial is stored in 32 bits */
    if (update > 0x7FFFFFFF) {
        update = 0x7FFFFFFF;
    }
    zd->internal_serial = (prev + update); /* automatically does % 2^32 */
    ods_log_debug("[%s] update serial: previous=%u update=%u new=%u", zd_str,
        prev, update, zd->internal_serial);
    return 0;
}


/**
 * Add RRSIG records to zonedata.
 *
 */
int
zonedata_sign(zonedata_type* zd, ldns_rdf* owner, signconf_type* sc,
    stats_type* stats)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;
    time_t now = 0;
    hsm_ctx_t* ctx = NULL;
    int error = 0;

    ods_log_assert(sc);
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    if (!DNS_SERIAL_GT(zd->internal_serial, zd->outbound_serial)) {
        error = zonedata_update_serial(zd, sc);
    }
    if (error || !zd->internal_serial) {
        ods_log_error("[%s] unable to sign zone data: failed to update serial",
            zd_str);
        return 1;
    }

    now = time_now();
    ctx = hsm_create_context();
    if (!ctx) {
        ods_log_error("[%s] error creating libhsm context", zd_str);
        return 2;
    }

    ods_log_debug("[%s] rrsig timers: offset=%u jitter=%u validity=%u", zd_str,
        duration2time(sc->sig_inception_offset),
        duration2time(sc->sig_jitter),
        duration2time(sc->sig_validity_denial));

    node = ldns_rbtree_first(zd->domains);
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        if (domain_sign(ctx, domain, owner, sc, now, zd->internal_serial,
            stats) != 0) {
            ods_log_error("[%s] unable to sign zone data: failed to sign domain",
                zd_str);
            hsm_destroy_context(ctx);
            return 1;
        }
        node = ldns_rbtree_next(node);
    }
    hsm_destroy_context(ctx);
    return 0;
}


/**
 * Examine domain for occluded data.
 *
 */
static int
zonedata_examine_domain_is_occluded(zonedata_type* zd, domain_type* domain,
    ldns_rdf* apex)
{
    ldns_rdf* parent_rdf = NULL;
    ldns_rdf* next_rdf = NULL;
    domain_type* parent_domain = NULL;
    char* str_name = NULL;
    char* str_parent = NULL;

    ods_log_assert(apex);
    ods_log_assert(domain);
    ods_log_assert(domain->dname);
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    if (ldns_dname_compare(domain->dname, apex) == 0) {
        return 0;
    }

    if (domain_examine_valid_zonecut(domain) != 0) {
        str_name = ldns_rdf2str(domain->dname);
        ods_log_warning("[%s] occluded (non-glue non-DS) data at %s NS",
            zd_str, str_name);
        se_free((void*)str_name);
        return 1;
    }

    parent_rdf = ldns_dname_left_chop(domain->dname);
    while (parent_rdf && ldns_dname_is_subdomain(parent_rdf, apex) &&
           ldns_dname_compare(parent_rdf, apex) != 0) {

        parent_domain = zonedata_lookup_domain(zd, parent_rdf);
        next_rdf = ldns_dname_left_chop(parent_rdf);
        ldns_rdf_deep_free(parent_rdf);

        if (parent_domain) {
            /* check for DNAME or NS */
            if (domain_examine_data_exists(parent_domain, LDNS_RR_TYPE_DNAME,
                0) == 0 && domain_examine_data_exists(domain, 0, 0) == 0) {
                /* data below DNAME */
                str_name = ldns_rdf2str(domain->dname);
                str_parent = ldns_rdf2str(parent_domain->dname);
                ods_log_warning("[%s] occluded data at %s (below %s DNAME)",
                    zd_str, str_name, str_parent);
                se_free((void*)str_name);
                se_free((void*)str_parent);
                return 1;
            } else if (domain_examine_data_exists(parent_domain,
                LDNS_RR_TYPE_NS, 0) == 0 &&
                domain_examine_data_exists(domain, 0, 1) == 0) {
                /* data (non-glue) below NS */
                str_name = ldns_rdf2str(domain->dname);
                str_parent = ldns_rdf2str(parent_domain->dname);
                ods_log_warning("[%s] occluded (non-glue) data at %s (below "
                    "%s NS)", zd_str, str_name, str_parent);
                se_free((void*)str_name);
                se_free((void*)str_parent);
                return 1;
            } else if (domain_examine_data_exists(parent_domain,
                LDNS_RR_TYPE_NS, 0) == 0 &&
                domain_examine_data_exists(domain, 0, 0) == 0 &&
                domain_examine_ns_rdata(parent_domain, domain->dname) != 0) {
                /* glue data not signalled by NS RDATA */
                str_name = ldns_rdf2str(domain->dname);
                str_parent = ldns_rdf2str(parent_domain->dname);
                ods_log_warning("[%s] occluded data at %s (below %s NS)",
                    zd_str, str_name, str_parent);
                se_free((void*)str_name);
                se_free((void*)str_parent);
                return 1;
            }
        }

        parent_rdf = next_rdf;
    }

    if (parent_rdf) {
        ldns_rdf_deep_free(parent_rdf);
    }
    return 0;
}


/**
 * Examine updates to zone data.
 *
 */
ods_status
zonedata_examine(zonedata_type* zd, ldns_rdf* apex, adapter_mode mode)
{
    int error = 0;
    int result = 0;
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    if (zd->domains->root != LDNS_RBTREE_NULL) {
        node = ldns_rbtree_first(zd->domains);
    }
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        error =
        /* Thou shall not have other data next to CNAME */
        domain_examine_rrset_is_alone(domain, LDNS_RR_TYPE_CNAME) ||
        /* Thou shall have at most one CNAME per name */
        domain_examine_rrset_is_singleton(domain, LDNS_RR_TYPE_CNAME) ||
        /* Thou shall have at most one DNAME per name */
        domain_examine_rrset_is_singleton(domain, LDNS_RR_TYPE_DNAME);
        if (error) {
            result = error;
        }

        if (mode == ADAPTER_FILE) {
            error =
            /* Thou shall not have occluded data in your zone file */
            zonedata_examine_domain_is_occluded(zd, domain, apex);
/* just warn if there is occluded data
            if (error) {
                result = error;
            }
*/
        }

        node = ldns_rbtree_next(node);
    }

    if (result) {
         return ODS_STATUS_ERR;
    }
    return ODS_STATUS_OK;
}


/**
 * Update zone data with pending changes.
 *
 */
int
zonedata_update(zonedata_type* zd, signconf_type* sc)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;
    domain_type* parent = NULL;
    int error = 0;

    ods_log_assert(sc);
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    error = zonedata_update_serial(zd, sc);
    if (error || !zd->internal_serial) {
        ods_log_error("[%s] unable to update zonedata: failed to update serial", zd_str);
        zonedata_cancel_update(zd);
        return 1;
    }

    if (zd->domains->root != LDNS_RBTREE_NULL) {
        node = ldns_rbtree_first(zd->domains);
    }
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        error = domain_update(domain, zd->internal_serial);
        if (error != 0) {
            if (error == 1) {
                ods_log_crit("[%s] unable to update zonedata to serial %u: rr "
                    "compare function failed", zd_str, zd->internal_serial);
                /* If this happens, the zone is partially updated. */
            } else {
                ods_log_error("[%s] unable to update zonedata to serial %u: "
                    "serial too small", zd_str, zd->internal_serial);
                zonedata_cancel_update(zd);
                return 1;
            }
            return 1;
        }
        node = ldns_rbtree_next(node);

        /* delete memory of domain if no RRsets exists */
        /* if this domain is now an empty non-terminal, don't delete */

        if (domain_count_rrset(domain) <= 0 &&
            (domain->dstatus != DOMAIN_STATUS_ENT_AUTH &&
             domain->dstatus != DOMAIN_STATUS_ENT_NS &&
             domain->dstatus != DOMAIN_STATUS_ENT_GLUE)) {

            parent = domain->parent;
            if (domain->subdomain_count <= 0) {
                ods_log_deeebug("[%s] obsoleted domain: #rrset=%i, status=%i",
                    zd_str, domain_count_rrset(domain), domain->dstatus);
                domain = zonedata_del_domain(zd, domain);
            }
            if (domain) {
                ods_log_error("[%s] failed to delete obsoleted domain", zd_str);
            }
            while (parent && domain_count_rrset(parent) <= 0) {
                domain = parent;
                parent = domain->parent;
                if (domain->subdomain_count <= 0) {
                    domain = zonedata_del_domain(zd, domain);
                    if (domain) {
                        ods_log_error("[%s] failed to delete obsoleted domain", zd_str);
                    }
                }
            }
        }
    }
    return 0;
}


/**
 * Cancel update.
 *
 */
void
zonedata_cancel_update(zonedata_type* zd)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    if (zd->domains->root != LDNS_RBTREE_NULL) {
        node = ldns_rbtree_first(zd->domains);
    }
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        domain_cancel_update(domain);
        node = ldns_rbtree_next(node);
    }
    return;
}


/**
 * Add RR to the zone data.
 *
 */
int
zonedata_add_rr(zonedata_type* zd, ldns_rr* rr, int at_apex)
{
    domain_type* domain = NULL;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);
    ods_log_assert(rr);

    domain = zonedata_lookup_domain(zd, ldns_rr_owner(rr));
    if (domain) {
        return domain_add_rr(domain, rr);
    }
    /* no domain with this name yet */
    domain = domain_create(ldns_rr_owner(rr));
    domain = zonedata_add_domain(zd, domain);
    if (!domain) {
        ods_log_error("[%s] unable to add RR to zonedata: failed to add domain",
            zd_str);
        return 1;
    }
    if (at_apex) {
        domain->dstatus = DOMAIN_STATUS_APEX;
    }
    return domain_add_rr(domain, rr);
}


/**
 * Recover RR from backup.
 *
 */
int
zonedata_recover_rr_from_backup(zonedata_type* zd, ldns_rr* rr)
{
    domain_type* domain = NULL;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);
    ods_log_assert(rr);

    domain = zonedata_lookup_domain(zd, ldns_rr_owner(rr));
    if (domain) {
        return domain_recover_rr_from_backup(domain, rr);
    }

    ods_log_error("[%s] unable to recover RR to zonedata: domain does not exist",
        zd_str);
    return 1;
}


/**
 * Recover RRSIG from backup.
 *
 */
int
zonedata_recover_rrsig_from_backup(zonedata_type* zd, ldns_rr* rrsig,
    const char* locator, uint32_t flags)
{
    domain_type* domain = NULL;
    ldns_rr_type type_covered;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);
    ods_log_assert(rrsig);

    type_covered = ldns_rdf2rr_type(ldns_rr_rrsig_typecovered(rrsig));
    if (type_covered == LDNS_RR_TYPE_NSEC3) {
        domain = zonedata_lookup_domain_nsec3(zd, ldns_rr_owner(rrsig));
    } else {
        domain = zonedata_lookup_domain(zd, ldns_rr_owner(rrsig));
    }
    if (domain) {
        return domain_recover_rrsig_from_backup(domain, rrsig, type_covered,
            locator, flags);
    }
    ods_log_error("[%s] unable to recover RRSIG to zonedata: domain does not exist",
        zd_str);
    return 1;
}


/**
 * Delete RR from the zone data.
 *
 */
int
zonedata_del_rr(zonedata_type* zd, ldns_rr* rr)
{
    domain_type* domain = NULL;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);
    ods_log_assert(rr);

    domain = zonedata_lookup_domain(zd, ldns_rr_owner(rr));
    if (domain) {
        return domain_del_rr(domain, rr);
    }
    /* no domain with this name yet */
    ods_log_warning("[%s] unable to delete RR from zonedata: no such domain",
        zd_str);
    return 0;
}


/**
 * Delete all current RRs from the zone data.
 *
 */
int
zonedata_del_rrs(zonedata_type* zd)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;

    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    if (zd->domains->root != LDNS_RBTREE_NULL) {
        node = ldns_rbtree_first(zd->domains);
    }
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        if (domain_del_rrs(domain) != 0) {
            return 1;
        }
        node = ldns_rbtree_next(node);
    }
    return 0;
}


/**
 * Clean up domains in zone data.
 *
 */
void
zonedata_cleanup_domains(ldns_rbtree_t* domain_tree)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;

    if (domain_tree && domain_tree->root != LDNS_RBTREE_NULL) {
        node = ldns_rbtree_first(domain_tree);
    }
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        domain_cleanup(domain);
        node = ldns_rbtree_next(node);
    }
    if (domain_tree && domain_tree->root != LDNS_RBTREE_NULL) {
        se_rbnode_free(domain_tree->root);
    }
    if (domain_tree) {
        ldns_rbtree_free(domain_tree);
    }
    return;
}


/**
 * Clean up zone data.
 *
 */
void
zonedata_cleanup(zonedata_type* zonedata)
{
    /* destroy domains */
    if (zonedata) {
        if (zonedata->domains) {
            zonedata_cleanup_domains(zonedata->domains);
            zonedata->domains = NULL;
        }
        if (zonedata->nsec3_domains) {
            zonedata_cleanup_domains(zonedata->nsec3_domains);
            zonedata->nsec3_domains = NULL;
        }
        se_free((void*) zonedata);
    }
    return;
}


/**
 * Print zone data.
 *
 */
ods_status
zonedata_print(FILE* fd, zonedata_type* zd)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;

    if (!fd) {
        ods_log_error("[%s] unable to print zone data: no file descriptor",
            zd_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(fd);

    if (!zd || !zd->domains) {
        ods_log_error("[%s] unable to print zone data: no zone data",
            zd_str);
        return ODS_STATUS_ASSERT_ERR;
    }
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    node = ldns_rbtree_first(zd->domains);
    if (!node || node == LDNS_RBTREE_NULL) {
        fprintf(fd, "; empty zone\n");
        return ODS_STATUS_OK;
    }
    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        domain_print(fd, domain);
        node = ldns_rbtree_next(node);
    }
    return ODS_STATUS_OK;
}


/**
 * Print NSEC(3)s in zone data.
 *
 */
void
zonedata_print_nsec(FILE* fd, zonedata_type* zd)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;

    if (!fd) {
        ods_log_error("[%s] unable to print nsec: no file descriptor",
            zd_str);
        return;
    }
    ods_log_assert(fd);

    if (!zd || !zd->domains) {
        ods_log_error("[%s] unable to print nsec: no zone data",
            zd_str);
        return;
    }
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    node = ldns_rbtree_first(zd->domains);
    if (!node || node == LDNS_RBTREE_NULL) {
        fprintf(fd, "; empty zone\n");
        return;
    }

    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        domain_print_nsec(fd, domain);
        node = ldns_rbtree_next(node);
    }
    return;
}


/**
 * Print RRSIGs zone data.
 *
 */
void
zonedata_print_rrsig(FILE* fd, zonedata_type* zd)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    domain_type* domain = NULL;

    if (!fd) {
        ods_log_error("[%s] unable to print rrsig: no file descriptor",
            zd_str);
        return;
    }
    ods_log_assert(fd);

    if (!zd || !zd->domains) {
        ods_log_error("[%s] unable to print rrsig: no zone data",
            zd_str);
        return;
    }
    ods_log_assert(zd);
    ods_log_assert(zd->domains);

    node = ldns_rbtree_first(zd->domains);
    if (!node || node == LDNS_RBTREE_NULL) {
        fprintf(fd, "; empty zone\n");
        return;
    }

    while (node && node != LDNS_RBTREE_NULL) {
        domain = (domain_type*) node->data;
        domain_print_rrsig(fd, domain);
        node = ldns_rbtree_next(node);
    }
    return;
}
