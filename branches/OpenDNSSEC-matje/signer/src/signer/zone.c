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
 * Zone.
 *
 */

#include "adapter/adapter.h"
#include "scheduler/locks.h"
#include "scheduler/task.h"
#include "signer/signconf.h"
#include "signer/zone.h"
#include "signer/zonedata.h"
#include "util/duration.h"
#include "util/file.h"
#include "util/log.h"
#include "util/se_malloc.h"

#include <ldns/ldns.h> /* ldns_*() */

/* copycode: This define is taken from BIND9 */
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
    se_log_debug("create zone %s", name);

    /* zone identification */
    zone->name = se_strdup(name);
    zone->dname = ldns_dname_new_frm_str(name);
    zone->klass = klass;
    /* policy */
    zone->policy_name = NULL;
    zone->signconf_filename = NULL;
    zone->signconf = NULL;
    zone->inbound_adapter = NULL;
    zone->outbound_adapter = NULL;
    /* status */
    zone->task = NULL;
    zone->backoff = 0;
    zone->worker = NULL;
    zone->just_added = 0;
    zone->just_updated = 0;
    zone->tobe_removed = 0;
    zone->in_progress = 0;
    /* zone data */
    zone->zonedata = zonedata_create();

    lock_basic_init(&zone->zone_lock);
    lock_basic_init(&zone->slhelper_lock);
    return zone;
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

    if (se_strcmp(z2->policy_name, z1->policy_name) != 0) {
        se_free((void*)z1->policy_name);
        if (z2->policy_name) {
            z1->policy_name = se_strdup(z2->policy_name);
        } else {
            z1->policy_name = NULL;
        }
        z1->just_updated = 1;
    }

    if (se_strcmp(z2->signconf_filename, z1->signconf_filename) != 0) {
        se_free((void*)z1->signconf_filename);
        if (z2->signconf_filename) {
            z1->signconf_filename = se_strdup(z2->signconf_filename);
        } else {
            z1->signconf_filename = NULL;
        }
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
        z1->just_updated = 1;
    }

    zone_cleanup(z2);
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
    time_t last_modified = 0;
    time_t now;

    se_log_assert(zone);
    se_log_debug("load zone %s signconf %s", zone->name, zone->signconf_filename);

    if (zone->signconf) {
        last_modified = zone->signconf->last_modified;
    }

    signconf = signconf_read(zone->signconf_filename, last_modified);
    if (!signconf) {
        if (!zone->policy_name) {
            se_log_warning("zone %s has no policy", zone->name);
        } else {
            signconf = signconf_read(zone->signconf_filename, 0);
            if (!signconf) {
                se_log_warning("zone %s has policy %s configured, "
                    "but has no (valid) signconf file",
                    zone->name, zone->policy_name);
                if (buf) {
                    (void)snprintf(buf, ODS_SE_MAXLINE,
                        "Zone %s config has errors.\n", zone->name);
                }
                return -1;
            } else {
                se_log_debug("zone %s has not changed", zone->name);
            }
        }
        if (buf) {
            (void)snprintf(buf, ODS_SE_MAXLINE,
                "Zone %s config has not changed.\n", zone->name);
        }
        return 0;
    } else if (signconf_check(signconf) != 0) {
        se_log_warning("zone %s signconf has errors", zone->name);
        if (buf) {
            (void)snprintf(buf, ODS_SE_MAXLINE,
                "Zone %s config has errors.\n", zone->name);
        }
        return -1;
    } else if (!zone->signconf) {
        zone->signconf = signconf;
        /* we don't check if foo in <Zone name="foo"> matches zone->name */
        zone->signconf->name = zone->name;
        se_log_debug("zone %s now has signconf", zone->name);
        /* zone state? */
        /* create task for new zone */
        now = time(NULL);
        zone->task = task_create(TASK_READ, now, zone->name, zone);
        task = tasklist_schedule_task(tl, zone->task, 0);
        if (!task) {
            if (buf) {
                (void)snprintf(buf, ODS_SE_MAXLINE, "Zone %s now has config, "
                    "but could not be scheduled.\n", zone->name);
            }
        } else {
            if (buf) {
                (void)snprintf(buf, ODS_SE_MAXLINE,
                    "Zone %s now has config.\n", zone->name);
            }
        }
        return 1;
    } else {
        /* update task for new zone */
        zone->task->what = signconf_compare(zone->signconf, signconf);
        signconf_cleanup(zone->signconf);
        zone->signconf = signconf;
        zone->signconf->name = zone->name;
        se_log_debug("zone %s signconf updated", zone->name);
        if (buf) {
            (void)snprintf(buf, ODS_SE_MAXLINE,
                "Zone %s config updated.\n", zone->name);
        }
        return 1;
    }
    /* not reached */
    return 0;
}


/**
 * Add a RR to the zone.
 *
 */
int
zone_add_rr(zone_type* zone, ldns_rr* rr)
{
    ldns_rr_type type = 0;
    int at_apex = 0;
    uint32_t tmp = 0;
    ldns_rdf* soa_min = NULL;

    se_log_assert(zone);
    se_log_assert(zone->zonedata);
    se_log_assert(zone->signconf);
    se_log_assert(rr);

    /* in-zone? */
    if (ldns_dname_compare(zone->dname, ldns_rr_owner(rr)) != 0 &&
        !ldns_dname_is_subdomain(ldns_rr_owner(rr), zone->dname)) {
        se_log_warning("zone %s contains out of zone data, skipping",
            zone->name);
        ldns_rr_free(rr);
        return 0; /* consider success */
    }
    if (ldns_dname_compare(zone->dname, ldns_rr_owner(rr)) == 0) {
        at_apex = 1;
    }

    /* type specific configuration */
    type = ldns_rr_get_type(rr);
    if (type == LDNS_RR_TYPE_DNSKEY && zone->signconf->dnskey_ttl) {
        tmp = (uint32_t) duration2time(zone->signconf->dnskey_ttl);
        se_log_verbose("zone %s set DNSKEY TTL to %u", zone->name, tmp);
        ldns_rr_set_ttl(rr, tmp);
    }
    if (type == LDNS_RR_TYPE_SOA) {
        if (zone->signconf->soa_ttl) {
            tmp = (uint32_t) duration2time(zone->signconf->soa_ttl);
            se_log_verbose("zone %s set SOA TTL to %u", zone->name, tmp);
            ldns_rr_set_ttl(rr, tmp);
        }
        if (zone->signconf->soa_min) {
            tmp = (uint32_t) duration2time(zone->signconf->soa_min);
            se_log_verbose("zone %s set SOA MINIMUM to %u",
                zone->name, tmp);
            soa_min = ldns_rr_set_rdf(rr,
                ldns_native2rdf_int32(LDNS_RDF_TYPE_INT32, tmp),
                SE_SOA_RDATA_MINIMUM);
            if (soa_min) {
                ldns_rdf_deep_free(soa_min);
            } else {
                se_log_error("zone %s failed to replace SOA MINIMUM "
                    "rdata", zone->name);
            }
        }
    }
    return zonedata_add_rr(zone->zonedata, rr, at_apex);
}


/**
 * Clean up a zone.
 *
 */
void
zone_cleanup(zone_type* zone)
{
    if (zone) {
        if (zone->dname) {
            ldns_rdf_deep_free(zone->dname);
            zone->dname = NULL;
        }
        if (zone->inbound_adapter) {
            adapter_cleanup(zone->inbound_adapter);
            zone->inbound_adapter = NULL;
        }
        if (zone->outbound_adapter) {
            adapter_cleanup(zone->outbound_adapter);
            zone->outbound_adapter = NULL;
        }
        if (zone->signconf) {
            signconf_cleanup(zone->signconf);
            zone->signconf = NULL;
        }
        if (zone->zonedata) {
            zonedata_cleanup(zone->zonedata);
            zone->zonedata = NULL;
        }
        if (zone->policy_name) {
            se_free((void*) zone->policy_name);
            zone->policy_name = NULL;
        }
            se_free((void*) zone->signconf_filename);
            se_free((void*) zone->name);

        lock_basic_destroy(&zone->zone_lock);
        lock_basic_destroy(&zone->slhelper_lock);
        se_free((void*) zone);
    } else {
        se_log_warning("cleanup emtpy zone");
    }
}
