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
 *
 * The zonelist and all.
 */

#include "config.h"
#include "parser/confparser.h"
#include "parser/zonelistparser.h"
#include "scheduler/task.h"
#include "signer/zone.h"
#include "signer/zonelist.h"
#include "util/file.h"
#include "util/log.h"
#include "util/se_malloc.h"

#include <ldns/ldns.h>


/**
 * Compare two zones.
 *
 */
static int
zone_compare(const void* a, const void* b)
{
    zone_type* x = (zone_type*)a;
    zone_type* y = (zone_type*)b;

    se_log_assert(x);
    se_log_assert(y);
    se_log_none("compare zones %s, %s", x->name, y->name);

    if (x->klass != y->klass) {
        if (x->klass < y->klass) {
            return -1;
        }
        return 1;
    }
    return ldns_dname_compare(x->dname, y->dname);
}


/**
 * Create a new zone list.
 *
 */
zonelist_type*
zonelist_create(void)
{
    zonelist_type* zlist = (zonelist_type*) se_malloc(sizeof(zonelist_type));
    zlist->zones = ldns_rbtree_create(zone_compare);
    zlist->engine = NULL;
    zlist->last_modified = 0;
    return zlist;
}


/**
 * Read a zonelist file.
 *
 */
zonelist_type*
zonelist_read(const char* zonelistfile, time_t last_modified)
{
    zonelist_type* zlist = NULL;
    time_t st_mtime;

    se_log_assert(zonelistfile);
    se_log_debug("read zone list file '%s'", zonelistfile);

    /* is the file updated? */
    st_mtime = file_lastmodified(zonelistfile);
    if (st_mtime <= last_modified) {
        se_log_debug("zone list file %s is unchanged", zonelistfile);
        return NULL;
    }
    /* does the file have no parse errors? */
    if (parse_file_check(zonelistfile, RNG_ZONELISTFILE) != 0) {
        se_log_error("unable to parse zone list file '%s'", zonelistfile);
        return NULL;
    }
    /* ok, parse it! */
    zlist = parse_zonelist_zones(zonelistfile);
    if (zlist) {
        zlist->last_modified = st_mtime;
    } else {
        se_log_error("unable to read zone list file '%s'", zonelistfile);
        return NULL;
    }
    se_log_debug3("read zone list file: done");
    return zlist;
}


/**
 * Lock all zones in zone list.
 *
 */
void
zonelist_lock(zonelist_type* zonelist)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    zone_type* zone = NULL;

    se_log_assert(zonelist);
    se_log_assert(zonelist->zones);
    se_log_debug3("lock zonelist");

    node = ldns_rbtree_first(zonelist->zones);
    while (node && node != LDNS_RBTREE_NULL) {
        zone = (zone_type*) node->key;
        lock_basic_lock(&zone->zone_lock);
        node = ldns_rbtree_next(node);
    }

    se_log_debug3("lock zonelist: done");
}

/**
 * Lock all zones in zone list.
 *
 */
void
zonelist_unlock(zonelist_type* zonelist)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    zone_type* zone = NULL;

    se_log_assert(zonelist);
    se_log_assert(zonelist->zones);
    se_log_debug3("unlock zonelist");

    node = ldns_rbtree_first(zonelist->zones);
    while (node && node != LDNS_RBTREE_NULL) {
        zone = (zone_type*) node->key;
        lock_basic_unlock(&zone->zone_lock);
        node = ldns_rbtree_next(node);
    }

    se_log_debug3("unlock zonelist: done");
}

/**
 * Convert a zone to a tree node.
 *
 */
static ldns_rbnode_t*
zone2node(zone_type* zone)
{
    ldns_rbnode_t* node = (ldns_rbnode_t*) se_malloc(sizeof(ldns_rbnode_t));
    node->key = zone;
    node->data = zone;
    return node;
}


/**
 * Lookup zone.
 *
 */
zone_type*
zonelist_lookup_zone(zonelist_type* zonelist, zone_type* zone)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;

    se_log_assert(zonelist);
    se_log_assert(zonelist->zones);
    se_log_assert(zone);
    se_log_debug3("look up zone: '%s'", zone->name);

    node = ldns_rbtree_search(zonelist->zones, zone);
    if (node) {
        return (zone_type*) node->key;
    }
    return NULL;
}


/**
 * Add zone.
 *
 */
zone_type*
zonelist_add_zone(zonelist_type* zonelist, zone_type* zone)
{
    ldns_rbnode_t* new_node = NULL;

    se_log_assert(zonelist);
    se_log_assert(zonelist->zones);
    se_log_assert(zone);
    se_log_debug2("add zone '%s'", zone->name);

    if (zonelist_lookup_zone(zonelist, zone) != NULL) {
        se_log_info("zone '%s' already present", zone->name);
        zone_cleanup(zone);
        return NULL;
    }

    new_node = zone2node(zone);
    if (ldns_rbtree_insert(zonelist->zones, new_node) == NULL) {
        se_log_error("unable to add zone '%s'", zone->name);
        zone_cleanup(zone);
        se_free((void*) new_node);
        return NULL;
    }
    zone->just_added = 1;
    se_log_debug3("add zone: done");
    return zone;
}


/**
 * Delete a zone
 *
 */
zone_type*
zonelist_delete_zone(zonelist_type* zonelist, zone_type* zone)
{
    ldns_rbnode_t* old_node = LDNS_RBTREE_NULL;

    se_log_assert(zonelist);
    se_log_assert(zonelist->zones);
    se_log_assert(zone);
    se_log_debug("delete zone '%s'", zone->name);

    old_node = ldns_rbtree_delete(zonelist->zones, zone);
    if (!old_node) {
        se_log_error("unable to delete zone %s, not present", zone->name);
        return zone;
    }

    se_free((void*) old_node);
    zone_cleanup(zone);
    se_log_debug3("delete zone: done");
    return NULL;
}


/**
 * Update zone list.
 *
 */
void
zonelist_update(zonelist_type* zl, struct tasklist_struct* tl, char* buf)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    zone_type* zone = NULL;
    task_type* task = NULL;
    int just_removed = 0;
    int just_added = 0;
    int just_updated = 0;

    se_log_debug("update zone list");

    node = ldns_rbtree_first(zl->zones);
    while (node && node != LDNS_RBTREE_NULL) {
        zone = (zone_type*) node->key;

        se_log_debug3("zone [%i, %i, %i]", zone->just_removed,
            zone->just_added, zone->just_updated);

        /* removed */
        if (zone->just_removed) {
            if (zone->task) {
                se_log_debug3("clean up task for zone '%s'",
                    zone->task->who);
                task = tasklist_delete_task(tl, zone->task);
                task_cleanup(task);
            }
            se_log_debug3("delete zone '%s' from zone list",
               zone->name);
            node = ldns_rbtree_next(node);
            lock_basic_unlock(&zone->zone_lock);
            zonelist_delete_zone(zl, zone);
            just_removed++;
            continue;
        }

        /* added */
        else if (zone->just_added) {
            zone->just_added = 0;
            just_added++;
        }

        /* updated */
        else if (zone->just_updated) {
            zone->just_updated = 0;
            just_updated++;
        }

        node = ldns_rbtree_next(node);
    }

    if (buf) {
        (void)snprintf(buf, MAX_LINE, "Zone list updated: %i removed, %i added, "
            "%i updated.\n", just_removed, just_added, just_updated);
    }

    se_log_debug3("update zone list: done");
    return;
}


/**
 * Merge zone lists.
 *
 */
void
zonelist_merge(zonelist_type* zl1, zonelist_type* zl2)
{
    zone_type* z1 = NULL;
    zone_type* z2 = NULL;
    ldns_rbnode_t* n1 = LDNS_RBTREE_NULL;
    ldns_rbnode_t* n2 = LDNS_RBTREE_NULL;
    int ret;

    se_log_assert(zl1);
    se_log_assert(zl2);
    se_log_debug2("merge zone lists");

    n1 = ldns_rbtree_first(zl1->zones);
    n2 = ldns_rbtree_first(zl2->zones);
    while (n2 && n2 != LDNS_RBTREE_NULL) {
        z2 = (zone_type*) n2->key;
        if (n1 && n1 != LDNS_RBTREE_NULL) {
            z1 = (zone_type*) n1->key;
        } else {
            z1 = NULL;
        }

        if (!z2) {
            return;
        } else if (!z1) {
            se_log_debug3("merge zonelists: add zone '%s'", z2->name);
            z2 = zonelist_add_zone(zl1, z2);
            if (!z2) {
                se_log_error("zone list merge failed, z2 not added");
                return;
            }
            lock_basic_lock(&z2->zone_lock);
            n2 = ldns_rbtree_next(n2);
        } else {
            ret = zone_compare(z1, z2);
            if (ret < 0) {
                se_log_debug3("merge zone lists: remove zone '%s'", z1->name);
                z1->just_removed = 1;
                n1 = ldns_rbtree_next(n1);
            } else if (ret > 0) {
                se_log_debug3("merge zone lists: add zone '%s'", z2->name);
                z2 = zonelist_add_zone(zl1, z2);
                if (!z2) {
                    se_log_error("zone list merge failed, z2 not added");
                    return;
                }
                lock_basic_lock(&z2->zone_lock);
                n2 = ldns_rbtree_next(n2);
            } else {
                se_log_debug3("merge zone lists: update zone '%s'",
                    z1->name);
                n1 = ldns_rbtree_next(n1);
                n2 = ldns_rbtree_next(n2);
                zone_update_zonelist(z1, z2);
            }
        }
    }

    while (n1 && n1 != LDNS_RBTREE_NULL) {
        z1 = (zone_type*) n1->key;
        se_log_debug3("merge zone lists: remove zone '%s'", z1->name);
        z1->just_removed = 1;
        n1 = ldns_rbtree_next(n1);
    }

    zl1->last_modified = zl2->last_modified;
    se_rbnode_free(zl2->zones->root);
    ldns_rbtree_free(zl2->zones);
    se_free((void*) zl2);
    se_log_debug3("merge zone lists: done");
    return;
}


/**
 * Clean up a zonelist.
 *
 */
void
zonelist_cleanup(zonelist_type* zonelist)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    zone_type* zone = NULL;

    if (zonelist) {
        se_log_debug("clean up zone list");

        node = ldns_rbtree_first(zonelist->zones);
        while (node != LDNS_RBTREE_NULL) {
            zone = (zone_type*) node->key;
            zone_cleanup(zone);
            node = ldns_rbtree_next(node);
        }
        se_rbnode_free(zonelist->zones->root);
        ldns_rbtree_free(zonelist->zones);
        se_free((void*) zonelist);
        se_log_debug3("clean up zone list: done");
    } else {
        se_log_debug3("clean up zone list: null");
    }
}

/**
 * Print the zonelist.
 *
 */
void
zonelist_print(FILE* out, zonelist_type* zonelist)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    zone_type* zone = NULL;

    se_log_assert(zonelist);
    se_log_assert(out);
    se_log_debug3("print zone list");

    fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(out, "<ZoneList>\n");

    node = ldns_rbtree_first(zonelist->zones);
    while (node && node != LDNS_RBTREE_NULL) {
        zone = (zone_type*) node->key;

        fprintf(out, "\t<Zone name=\"%s\">\n", zone->name);
        fprintf(out, "\t\t<Policy>%s</Policy>\n", zone->policy_name);
        fprintf(out, "\t\t<SignerConfiguration>%s</SignerConfiguration>\n",
            zone->signconf_filename);
        fprintf(out, "\t\t<Adapters>\n");
        adapter_print(out, zone->inbound_adapter);
        adapter_print(out, zone->outbound_adapter);
        fprintf(out, "\t\t</Adapters>\n");

        fprintf(out, "\t</Zone>\n");
        fprintf(out, "\n");
        node = ldns_rbtree_next(node);
    }

    fprintf(out, "</ZoneList>\n");

    se_log_debug3("print zone list: done");
}
