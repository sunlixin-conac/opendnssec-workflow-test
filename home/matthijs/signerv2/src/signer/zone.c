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
 * Zone attributes.
 *
 */

#include "adapter/adapter.h"
#include "daemon/engine.h"
#include "scheduler/task.h"
#include "signer/signconf.h"
#include "signer/zone.h"
#include "signer/zonedata.h"
#include "util/file.h"
#include "util/log.h"
#include "util/se_malloc.h"

#include <ldns/ldns.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_TTL 3601
/* copycode: This define is from BIND9 */
#define DNS_SERIAL_GT(a, b) ((int)(((a) - (b)) & 0xFFFFFFFF) > 0)

/**
 * Create a new zone.
 *
 */
zone_type*
zone_create(const char* name, ldns_rr_class klass)
{
    zone_type* zone = (zone_type*) se_calloc(1, sizeof(zone_type));

    se_log_assert(name);
    se_log_debug("create zone '%s'", name);

    zone->name = se_strdup(name);
    zone->dname = ldns_dname_new_frm_str(name);
    zone->klass = klass;
    zone->default_ttl = DEFAULT_TTL;
    zone->inbound_serial = 0;
    zone->outbound_serial = 0;
    zone->policy_name = NULL;
    zone->signconf_filename = NULL;
    zone->signconf = NULL;
    zone->inbound_adapter = NULL;
    zone->outbound_adapter = NULL;
    zone->task = NULL;
    zone->just_removed = 0;
    zone->just_added = 0;
    zone->just_updated = 0;
    zone->backoff = 0;
    zone->in_progress = 0;
    zone->zonedata = NULL;
    zone->engine = NULL;
    lock_basic_init(&zone->zone_lock);
    return zone;
}


/**
 * Read zone fle.
 *
 */
int
zone_read_file(FILE* fd, zone_type* zone)
{
    se_log_assert(zone);
    se_log_assert(zone->inbound_adapter);
    se_log_debug2("read zone file '%s'", zone->inbound_adapter->filename);


    se_log_debug3("read zone file: done");
    return 0;
}


/**
 * Back up zone data.
 *
 */
int
zone_backup_data(zone_type* zone)
{
    se_log_assert(zone);

    se_log_debug2("back up zone data '%s'", zone->name);

    se_log_debug3("backup zone data: done");
    return 0;
}


static void
zone_clear_internals(const char* name, const char* ext)
{
    char* tmpname = NULL;

    tmpname = build_path(name, ext, 0);
    (void)unlink(tmpname);
    se_free((void*) tmpname);
    return;
}


/**
 * Clear the back up.
 *
 */
void
zone_clear_backup(zone_type* zone)
{
    int i = 0;
    char* extensions[] = {".unsorted", ".sorted", ".dnskeys", ".processed",
        ".nsecced", ".signed", ".signed2", ".finalized", ".signed.sorted",
        ".signed.processed", ".serial", (char*) NULL};

    se_log_assert(zone);
    se_log_assert(zone->name);
    se_log_debug2("clear back up zone '%s'", zone->name);

    while (extensions[i]) {
        zone_clear_internals(zone->name, extensions[i]);
        i++;
    }

    se_log_debug3("clear backup zone: done");
    return;
}


/**
 * Update zone configuration settings from zone list.
 *
 */
void
zone_update_zonelist(zone_type* z1, zone_type* z2)
{
    se_log_assert(z1);
    se_log_assert(z2);
    se_log_debug2("update zone '%s'", z1->name);

    if (se_strcmp(z2->policy_name, z1->policy_name) != 0) {
        se_free((void*)z1->policy_name);
        if (z2->policy_name) {
            z1->policy_name = se_strdup(z2->policy_name);
        } else {
            z1->policy_name = NULL;
        }
        se_log_debug3("update zone '%s': policy name changed", z1->name);
        z1->just_updated = 1;
    }

    if (se_strcmp(z2->signconf_filename, z1->signconf_filename) != 0) {
        se_free((void*)z1->signconf_filename);
        if (z2->signconf_filename) {
            z1->signconf_filename = se_strdup(z2->signconf_filename);
        } else {
            z1->signconf_filename = NULL;
        }
        se_log_debug3("update zone '%s': signconf filename changed", z1->name);
        z1->just_updated = 1;
    }

    if (adapter_compare(z1->inbound_adapter, z2->inbound_adapter) != 0) {
        adapter_cleanup(z1->inbound_adapter);
        if (z2->inbound_adapter) {
            z1->inbound_adapter = adapter_create(
                z2->inbound_adapter->filename,
                z2->inbound_adapter->type,
                z2->inbound_adapter->inbound);
        } else {
            z1->inbound_adapter = NULL;
        }
        se_log_debug3("update zone '%s': inbound adapter changed", z1->name);
        z1->just_updated = 1;
    }

    if (adapter_compare(z1->outbound_adapter, z2->outbound_adapter) != 0) {
        adapter_cleanup(z1->outbound_adapter);
        if (z2->outbound_adapter) {
            z1->outbound_adapter = adapter_create(
                z2->outbound_adapter->filename,
                z2->outbound_adapter->type,
                z2->outbound_adapter->inbound);
        } else {
            z1->outbound_adapter = NULL;
        }
        se_log_debug3("update zone '%s': outbound adapter changed", z1->name);
        z1->just_updated = 1;
    }

    zone_cleanup(z2);
    se_log_debug3("update zone: done");
    return;
}


/**
 * Read signer configuration.
 *
 */
int
zone_update_signconf(zone_type* zone, struct tasklist_struct* tl, char* buf)
{
    task_type* task = NULL;
    signconf_type* signconf = NULL;
    time_t last_modified = 0, now;

    se_log_assert(zone);
    se_log_debug("load zone signconf '%s'", zone->name);

    if (zone->signconf) {
        last_modified = zone->signconf->last_modified;
    }

    signconf = signconf_read(zone->signconf_filename, last_modified);
    if (!signconf) {
        if (!zone->policy_name) {
            se_log_warning("zone '%s' has no policy", zone->name);
        } else {
            signconf = signconf_read(zone->signconf_filename, 0);
            if (!signconf) {
                se_log_warning("zone '%s' has policy '%s' configured, "
                    "but has no (valid) signconf file configured",
                    zone->name, zone->policy_name);
                if (buf) {
                    (void)snprintf(buf, MAX_LINE, "Zone '%s' config has errors.\n",
                        zone->name);
                }
                return -1;
            } else {
                se_log_debug("zone '%s' has not changed", zone->name);
            }
        }
        if (buf) {
            (void)snprintf(buf, MAX_LINE, "Zone '%s' config has not changed.\n",
                zone->name);
        }
        return 0;
    } else if (signconf_check(signconf) != 0) {
        se_log_warning("zone '%s' signconf has errors", zone->name);
        if (buf) {
            (void)snprintf(buf, MAX_LINE, "Zone '%s' config has errors.\n", zone->name);
        }
        return -1;
    } else if (!zone->signconf) {
        zone->signconf = signconf;
        /* we don't check if foo in <Zone name="foo"> matches zone->name */
        zone->signconf->name = zone->name;
        se_log_debug("zone '%s' now has signconf", zone->name);
        /* zone state? */
        /* create task for new zone */
        now = time(NULL);
        zone->task = task_create(TASK_READ, now, zone->name, zone);
        task = tasklist_schedule_task(tl, zone->task, 0);
        if (!task) {
            if (buf) {
                (void)snprintf(buf, MAX_LINE, "Zone '%s' now has config, "
                    "but could not be scheduled.\n", zone->name);
            }
        } else {
            if (buf) {
                (void)snprintf(buf, MAX_LINE, "Zone '%s' now has config.\n", zone->name);
            }
        }
        return 1;
    } else {
        /* update task for new zone */
        zone->task->what = signconf_compare(zone->signconf, signconf);
        signconf_cleanup(zone->signconf);
        zone->signconf = signconf;
        zone->signconf->name = zone->name;
        se_log_debug("zone '%s' signconf updated", zone->name);
        if (buf) {
            (void)snprintf(buf, MAX_LINE, "Zone '%s' config updated.\n", zone->name);
        }
        return 1;
    }
    /* not reached */
    return 0;
}


/**
 * Convert class to string.
 *
 */
const char*
class2str(ldns_rr_class klass)
{
    switch (klass) {
        case LDNS_RR_CLASS_IN:
            return "IN";
            break;
        case LDNS_RR_CLASS_CH:
            return "CH";
            break;
        case LDNS_RR_CLASS_HS:
            return "HS";
            break;
        case LDNS_RR_CLASS_NONE:
            return "NONE";
            break;
        case LDNS_RR_CLASS_ANY:
            return "ANY";
            break;
        case LDNS_RR_CLASS_FIRST:
        case LDNS_RR_CLASS_LAST:
        case LDNS_RR_CLASS_COUNT:
        default:
            return "";
            break;
    }
    return "";
}


/**
 * Calculate output serial.
 *
 */
void
zone_calc_outbound_serial(zone_type* zone)
{
    uint32_t soa, prev, update;

    se_log_assert(zone);
    se_log_assert(zone->signconf);
    se_log_debug3("calculate serial for zone '%s'", zone->name);

    if (zone->signconf->soa_serial == NULL)
        return;

    prev = zone->outbound_serial;

    if (strncmp(zone->signconf->soa_serial, "unixtime", 8) == 0) {
        soa = (uint32_t) time(NULL);
        if (!DNS_SERIAL_GT(soa, prev)) {
            soa = prev + 1;
        }
        update = soa - prev;
    } else if (strncmp(zone->signconf->soa_serial, "counter", 7) == 0) {
        soa = zone->inbound_serial;
        if (!DNS_SERIAL_GT(soa, prev)) {
            soa = prev + 1;
        }
        update = soa - prev;
    } else if (strncmp(zone->signconf->soa_serial, "datecounter", 11) == 0) {
        soa = (uint32_t) time_datestamp(0, "%Y%m%d", NULL) * 100;

        if (!DNS_SERIAL_GT(soa, prev)) {
            soa = prev + 1;
        }
        update = soa - prev;
    } else if (strncmp(zone->signconf->soa_serial, "keep", 4) == 0) {
        soa = zone->inbound_serial;
        if (!DNS_SERIAL_GT(soa, prev)) {
            se_log_error("can not keep SOA SERIAL from input zone '%s' (%u): "
                "output SOA SERIAL is %u", zone->name, soa, prev);
            return;
        }
        prev = soa;
        update = 0;
    } else {
        se_log_error("zone '%s' has unknown serial type '%s'",
            zone->name, zone->signconf->soa_serial);
        return;
    }

    /* serial is stored in 32 bits */
    if (update > 0x7FFFFFFF) {
        update = 0x7FFFFFFF;
    }
    soa = (prev + update); /* automatically does % 2^32 */

    zone->outbound_serial = soa;
    se_log_debug3("calculate serial for zone '%s': done (new serial %u)",
        zone->name, zone->outbound_serial);
    return;
}


/**
 * Print zone.
 *
 */
void
zone_print(FILE* out, zone_type* zone, int skip_soa)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    /* domain_type* domain = NULL; */

    se_log_assert(zone);
    /* se_log_assert(zone->domains); */

    se_log_debug3("print zone '%s', class %s", zone->name,
        class2str(zone->klass));

    node = LDNS_RBTREE_NULL; /* ldns_rbtree_first(zone->domains); */
    if (!node || node == LDNS_RBTREE_NULL)
        fprintf(out, "; empty zone, class %s\n", class2str(zone->klass));


    while (node && node != LDNS_RBTREE_NULL) {
        /* domain = (domain_type*) node->data; */
        /* domain_print(out, domain, skip_soa); */
        fprintf(out, "$DOMAIN\n");
        node = ldns_rbtree_next(node);
    }

    se_log_debug3("print zone '%s', class %s done", zone->name,
        class2str(zone->klass));
}


/**
 * Read zone state.
 *
 */
/*
void
zone_read_state(zone_type* zone)
{
    time_t now;
    now = time(NULL);

    se_log_assert(zone);
    se_log_debug("read state for zone '%s'", zone->name);

    zone->inbound_serial = 0;
    zone->outbound_serial = 0;
    zone->scheduled_task = 1;
    zone->perform_task = now;
    zone->backoff = 0;

}
*/

/**
 * Write zone state.
 *
 */
/*
void
zone_write_state(zone_type* zone)
{
    se_log_assert(zone);
    se_log_debug("write state for zone '%s'", zone->name);

}
*/

/**
 * Clean up a zone.
 *
 */
void
zone_cleanup(zone_type* zone)
{
    if (zone) {
        se_log_debug3("clean up zone '%s'", zone->name);
        ldns_rdf_deep_free(zone->dname);
        adapter_cleanup(zone->inbound_adapter);
        adapter_cleanup(zone->outbound_adapter);
        signconf_cleanup(zone->signconf);
        zonedata_cleanup(zone->zonedata);
        se_free((void*) zone->policy_name);
        se_free((void*) zone->signconf_filename);
        se_free((void*) zone->name);
        lock_basic_destroy(&zone->zone_lock);
        se_free((void*) zone);
        /* clean up domain storage */
        se_log_debug3("clean up zone: done");
    } else {
        se_log_debug3("clean up zone: null");
    }
}
