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
 * Zone signing tools.
 *
 */

#include <unistd.h>

#include "config.h"
#include "adapter/adapter.h"
#include "daemon/engine.h"
#include "scheduler/locks.h"
#include "signer/tools.h"
#include "signer/zone.h"
#include "tools/tools.h"
#include "util/file.h"
#include "util/log.h"
#include "util/se_malloc.h"

/**
 * Read zone's input adapter.
 *
 */
int
tools_read_input(zone_type* zone)
{
    char* tmpname = NULL;
    char* tmpname2 = NULL;
    int result = 0;

    se_log_assert(zone);
    se_log_assert(zone->inbound_adapter);
    se_log_assert(zone->signconf);
    se_log_verbose("read zone '%s'", zone->name);

    /* make a copy */
    tmpname2 = build_path(zone->name, ".unsorted", 0);
    result = file_copy(zone->inbound_adapter->filename, tmpname2);
    se_free((void*)tmpname2);

#ifdef USE_TOOLS
    if (result == 0) {
        tmpname = build_path(zone->name, ".sorted", 0);
        result = tools_sorter(zone->inbound_adapter->filename, tmpname,
            zone->name, zone->signconf->soa_min, &zone->default_ttl,
            &zone->inbound_serial);
        se_free((void*)tmpname);
    }
#endif /* USE_TOOLS */
    se_log_debug3("read zone '%s': done", zone->name);
    return result;
}


static int
tools_publish_keys(const char* zonename, char* filename,
    keylist_type* keys, ldns_rr_class klass,
    duration_type* dnskey_ttl, uint32_t default_ttl)
{
    FILE* fd = NULL;
    key_type* key = NULL;
    uint32_t ttl = default_ttl;
    int count = 0;
    ldns_status status = LDNS_STATUS_OK;

    if (dnskey_ttl) {
        ttl = (uint32_t) duration2time(dnskey_ttl);
    }

    fd = fopen(filename, "w");
    if (!fd) {
        se_log_error("Error writing %s: %s",
            filename, strerror(errno));
        return 1;
    }

    key = keys->first_key;
    for (count=0; count < keys->count; count++) {
        if (key->publish) {
            if (!key->dnskey) {
                status = tools_create_dnskey(zonename, keys, klass, ttl);
                if (status != LDNS_STATUS_OK) {
                    se_log_error("error creating dnskeys for zone %s",
                        zonename);
                    return 1;
                }
            }

            ldns_rr_print(fd, key->dnskey);
        }

        key = key->next;
    }

    fclose(fd);
    return 0;
}

/**
 * Add DNSKEY records to zone.
 *
 */
int
tools_add_dnskeys(zone_type* zone)
{
    char* tmpname = NULL;
    char* tmpname2 = NULL;
    char* tmpname3 = NULL;
    int result = 0;

    se_log_assert(zone);
    se_log_assert(zone->signconf);
    se_log_verbose("publish dnskeys to zone '%s'", zone->name);
#ifdef USE_TOOLS

    tmpname3 = build_path(zone->name, ".dnskeys", 0);
    result = tools_publish_keys(zone->name, tmpname3,
        zone->signconf->keys, zone->klass,
        zone->signconf->dnskey_ttl, zone->default_ttl);

    if (result == 0) {
        tmpname = build_path(zone->name, ".sorted", 0);
        tmpname2 = build_path(zone->name, ".processed", 0);

        result = tools_zone_reader(tmpname, tmpname2, tmpname3,
            zone->name, zone->klass,
            (zone->signconf->nsec_type == LDNS_RR_TYPE_NSEC3),
            0, zone->signconf->nsec3_algo,
            (uint16_t) zone->signconf->nsec3_iterations,
            zone->signconf->nsec3_salt
        );

        se_free((void*)tmpname);
        se_free((void*)tmpname2);
        se_free((void*)tmpname3);
    }

#endif /* USE_TOOLS */
    se_log_debug3("publish dnskeys to zone '%s': done", zone->name);
    return result;
}


/**
 * Add NSEC / NSEC3 records to zone.
 *
 */
int
tools_nsecify(zone_type* zone, int do_nsec3params)
{
    char* tmpname = NULL;
    char* tmpname2 = NULL;
    int result = 0;

    se_log_assert(zone);
    se_log_assert(zone->signconf);
    se_log_verbose("nsecify zone '%s'", zone->name);
#ifdef USE_TOOLS
    tmpname = build_path(zone->name, ".processed", 0);
    tmpname2 = build_path(zone->name, ".nsecced", 0);
    if (zone->signconf->nsec_type == LDNS_RR_TYPE_NSEC3) {
        result = tools_nsec3er(tmpname, tmpname2, zone->name,
            zone->signconf->soa_min, zone->signconf->nsec3_algo, 0,
            (bool) zone->signconf->nsec3_optout,
            zone->signconf->nsec3_iterations, zone->signconf->nsec3_salt);
    } else {
        result = tools_nseccer(tmpname, tmpname2, zone->signconf->soa_min);

    }
    se_free((void*)tmpname);
    se_free((void*)tmpname2);
#endif /* USE_TOOLS */
    se_log_debug3("nsecify zone '%s': done", zone->name);
    return result;
}


/**
 * Sign zone.
 * \param[in] zone zone
 * \return int 0 on success, 1 on fail
 *
 */
int
tools_sign(zone_type* zone)
{
    char* tmpname = NULL;
    char* tmpname2 = NULL;
    char* tmpname3 = NULL;
    int result = 0;

    se_log_assert(zone);
    se_log_assert(zone->signconf);
    se_log_verbose("sign zone '%s'", zone->name);
#ifdef USE_TOOLS
    tmpname = build_path(zone->name, ".nsecced", 0);
    tmpname2 = build_path(zone->name, ".signed2", 0);
    tmpname3 = build_path(zone->name, ".signed", 0);
    zone_calc_outbound_serial(zone);
    result = tools_signer(tmpname, tmpname2, tmpname3, zone);
    if (result == 0) {
        result = rename((const char*) tmpname2, (const char*) tmpname3);
        if (result != 0) {
            se_log_error("unable to move file '%s' to '%s'", tmpname2, tmpname3);
        }
    }

    se_free((void*)tmpname);
    se_free((void*)tmpname2);
    se_free((void*)tmpname3);
#endif /* USE_TOOLS */
    se_log_debug3("sign zone '%s': done", zone->name);
    return result;
}


/**
 * Audit zone.
 *
 */
int tools_audit(zone_type* zone)
{
    char* audit_call = NULL;
    char* tmpname = NULL;
    char* tmpname2 = NULL;
    engine_type* engine = NULL;
    int result = 0, size_audit_call = 1024;

    se_log_assert(zone);
    se_log_assert(zone->signconf);
    se_log_verbose("finalize zone '%s'", zone->name);

#ifdef USE_TOOLS
    tmpname = build_path(zone->name, ".signed", 0);
    tmpname2 = build_path(zone->name, ".finalized", 0);
    result = tools_finalizer(tmpname, tmpname2);

    /* audit */
    if (zone->signconf->audit) {
        se_log_verbose("audit zone '%s'", zone->name);
        audit_call = (char*) se_calloc(size_audit_call, (sizeof(char)));
        engine = zone->engine;

	snprintf(audit_call, size_audit_call, "ods-auditor -c %s -s %s -z %s",
            engine->config->cfg_filename, tmpname2, zone->name);

        se_log_debug("call auditor: %s", audit_call);
        result = system(audit_call);
        if (result == -1) {
            se_log_error("cannot issue ods-auditor: %s", strerror(errno));
            result = 1;
        } else if (result != 0) {
            se_log_error("auditor reports errors");
        }

        se_free((void*)audit_call);
        se_log_debug3("audit zone '%s': done", zone->name);
    }

    se_free((void*)tmpname);
    se_free((void*)tmpname2);
#endif /* USE_TOOLS */
    se_log_debug3("finalize zone '%s': done", zone->name);
    return result;
}

/**
 * Write zone to output adapter.
 * \param[in] zone zone
 * \return int 0 on success, 1 on fail
 *
 */
int tools_write_output(zone_type* zone)
{
    char* tmpname = NULL;
    int result = 0;

    se_log_assert(zone);
    se_log_assert(zone->outbound_adapter);
    se_log_verbose("write zone '%s'", zone->name);

    /* make a copy */
    tmpname = build_path(zone->name, ".finalized", 0);
    result = rename((const char*) tmpname, zone->outbound_adapter->filename);
    se_free((void*)tmpname);
    if (result != 0) {
        se_log_error("write zone '%s' failed: %s", zone->name, strerror(errno));
    }

    se_log_debug3("write zone '%s': done", zone->name);
    return result;
}
