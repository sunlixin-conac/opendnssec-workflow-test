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

#ifndef SIGNER_ZONE_H
#define SIGNER_ZONE_H

#include "config.h"
#include "adapter/adapter.h"
#include "scheduler/locks.h"
#include "signer/signconf.h"
#include "signer/zonedata.h"

#include <ldns/ldns.h>
#include <stdio.h>

#define MAX_BACKOFF 3600

struct engine_struct;
struct task_struct;
struct tasklist_struct;

/**
 * Zone.
 *
 */
typedef struct zone_struct zone_type;
struct zone_struct {
    const char* name;
    ldns_rdf* dname;
    ldns_rr_class klass;
    uint32_t default_ttl;
    uint32_t inbound_serial;
    uint32_t outbound_serial;
    const char* policy_name;
    const char* signconf_filename;
    signconf_type* signconf;
    adapter_type* inbound_adapter;
    adapter_type* outbound_adapter;
    struct task_struct* task;
    int just_removed;
    int just_added;
    int just_updated;
    int in_progress;
    time_t backoff;

    /* last read */
    /* last write */

    zonedata_type* zonedata;
    struct engine_struct* engine; /* TODO: remove? */

    lock_basic_type zone_lock;
};

/**
 * Create a new zone.
 * \param[in] name zone name
 * \param[in] klass zone class
 * \return zone_type* zone
 *
 */
zone_type* zone_create(const char* name, ldns_rr_class klass);

/**
 * Read the zonedata from zonefile.
 * \param[in] fd file descriptor
 * \param[in] zone zone structure
 * \return 0 on success, 1 on error
 *
 */
int zone_read_file(FILE* fd, zone_type* zone);

/**
 * Back up the zone data.
 * \param[in] zone zone structure
 * \return 0 on success, 1 on error
 *
 */
int zone_backup_data(zone_type* zone);

/**
 * Clear back up of zone.
 * \param[in] zone zone structure
 *
 */
void zone_clear_backup(zone_type* zone);

/**
 * Update zone configuration settings from zone list.
 * \param[in] z1 zone to be updated
 * \param[in] z2 update
 *
 */
void zone_update_zonelist(zone_type* z1, zone_type* z2);

/**
 * Read signer configuration file.
 * \param[in] zone corresponding zone
 * \param[in] tl task list
 * \param[in] buf feedback buffer
 * \return int 0 on success, 1 on error
 *
 */
int zone_update_signconf(zone_type* zone, struct tasklist_struct* tl,
    char* buf);

/**
 * Read zone state.
 * \param[in] zone zone in question
 *
 */
//void zone_read_state(zone_type* zone);

/**
 * Write zone state.
 * \param[in] zone zone in question
 *
 */
//void zone_write_state(zone_type* zone);

/**
 * Calculate the output serial.
 * \param[in] zone zone in question
 *
 */
void zone_calc_outbound_serial(zone_type* zone);

/**
 * Clean up a zone.
 * \param[in] zone zone to cleanup
 *
 */
void zone_cleanup(zone_type* zone);

/**
 * Print a zone.
 * \param[in] out file descriptor
 * \param[in] zone zone to print
 * \param[in] skip_soa if we already have printed the soa
 *
 */
void zone_print(FILE* fd, zone_type* zone, int skip_soa);

#endif /* SIGNER_ZONE_H */
