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
 * Signer configuration.
 *
 */

#include "parser/confparser.h"
#include "parser/signconfparser.h"
#include "scheduler/task.h"
#include "shared/duration.h"
#include "shared/log.h"
#include "signer/backup.h"
#include "shared/file.h"
#include "signer/se_key.h"
#include "signer/signconf.h"
#include "util/se_malloc.h"

static const char* sc_str = "signconf";


/**
 * Create a new signer configuration with the 'empty' settings.
 *
 */
signconf_type*
signconf_create(void)
{
    signconf_type* sc = (signconf_type*) se_malloc(sizeof(signconf_type));

    /* Signatures */
    sc->sig_resign_interval = NULL;
    sc->sig_refresh_interval = NULL;
    sc->sig_validity_default = NULL;
    sc->sig_validity_denial = NULL;
    sc->sig_jitter = NULL;
    sc->sig_inception_offset = NULL;
    /* Denial of existence */
    sc->nsec_type = 0;
    sc->nsec3_optout = 0;
    sc->nsec3_algo = 0;
    sc->nsec3_iterations = 0;
    sc->nsec3_salt = NULL;
    /* Keys */
    sc->dnskey_ttl = NULL;
    sc->keys = NULL;
    /* Source of authority */
    sc->soa_ttl = NULL;
    sc->soa_min = NULL;
    sc->soa_serial = NULL;
    /* Other useful information */
    sc->last_modified = 0;
    sc->audit = 0;
    return sc;
}


/**
 * Read signer configuration.
 *
 */
signconf_type*
signconf_read(const char* filename, time_t last_modified)
{
    signconf_type* signconf;
    const char* rngfile = ODS_SE_RNGDIR "/signconf.rng";
    FILE* scfd = NULL;
    time_t st_mtime = 0;

    ods_log_assert(filename);

    st_mtime = ods_file_lastmodified(filename);
    if (st_mtime <= last_modified) {
        ods_log_debug("[%s] signconf file %s is unchanged", sc_str,
            filename?filename:"(null)");
        return NULL;
    }

    if (parse_file_check(filename, rngfile) != ODS_STATUS_OK) {
        ods_log_error("[%s] unable to parse signconf file %s", sc_str,
            filename?filename:"(null)");
        return NULL;
    }

    scfd = ods_fopen(filename, NULL, "r");
    if (scfd) {
        signconf = signconf_create();
        signconf->filename = se_strdup(filename);
        signconf->sig_resign_interval = parse_sc_sig_resign_interval(filename);
        signconf->sig_refresh_interval = parse_sc_sig_refresh_interval(filename);
        signconf->sig_validity_default = parse_sc_sig_validity_default(filename);
        signconf->sig_validity_denial = parse_sc_sig_validity_denial(filename);
        signconf->sig_jitter = parse_sc_sig_jitter(filename);
        signconf->sig_inception_offset = parse_sc_sig_inception_offset(filename);
        signconf->nsec_type = parse_sc_nsec_type(filename);
        if (signconf->nsec_type == LDNS_RR_TYPE_NSEC3) {
            signconf->nsec3_optout = parse_sc_nsec3_optout(filename);
            signconf->nsec3_algo = parse_sc_nsec3_algorithm(filename);
            signconf->nsec3_iterations = parse_sc_nsec3_iterations(filename);
            signconf->nsec3_salt = parse_sc_nsec3_salt(filename);
        }
        signconf->keys = parse_sc_keys(filename);
        signconf->dnskey_ttl = parse_sc_dnskey_ttl(filename);
        signconf->soa_ttl = parse_sc_soa_ttl(filename);
        signconf->soa_min = parse_sc_soa_min(filename);
        signconf->soa_serial = parse_sc_soa_serial(filename);
        signconf->audit = parse_sc_audit(filename);
        signconf->last_modified = st_mtime;

        ods_fclose(scfd);
        return signconf;
    }

    ods_log_error("[%s] unable to read signconf file %s", sc_str, filename);
    return NULL;
}


/**
 * Read a signer configuration from backup.
 *
 */
signconf_type*
signconf_recover_from_backup(const char* filename)
{
    signconf_type* signconf = NULL;
    const char* zonename = NULL;
    FILE* scfd = NULL;

    scfd = ods_fopen(filename, NULL, "r");
    if (scfd) {
        signconf = signconf_create();

        if (!backup_read_check_str(scfd, ODS_SE_FILE_MAGIC) ||
            !backup_read_check_str(scfd, ";name:") ||
            !backup_read_str(scfd, &zonename) ||
            !backup_read_check_str(scfd, ";filename:") ||
            !backup_read_str(scfd, &signconf->filename) ||
            !backup_read_check_str(scfd, ";last_modified:") ||
            !backup_read_time_t(scfd, &signconf->last_modified) ||
            !backup_read_check_str(scfd, ";sig_resign_interval:") ||
            !backup_read_duration(scfd, &signconf->sig_resign_interval) ||
            !backup_read_check_str(scfd, ";sig_refresh_interval:") ||
            !backup_read_duration(scfd, &signconf->sig_refresh_interval) ||
            !backup_read_check_str(scfd, ";sig_validity_default:") ||
            !backup_read_duration(scfd, &signconf->sig_validity_default) ||
            !backup_read_check_str(scfd, ";sig_validity_denial:") ||
            !backup_read_duration(scfd, &signconf->sig_validity_denial) ||
            !backup_read_check_str(scfd, ";sig_jitter:") ||
            !backup_read_duration(scfd, &signconf->sig_jitter) ||
            !backup_read_check_str(scfd, ";sig_inception_offset:") ||
            !backup_read_duration(scfd, &signconf->sig_inception_offset) ||
            !backup_read_check_str(scfd, ";nsec_type:") ||
            !backup_read_rr_type(scfd, &signconf->nsec_type) ||
            !backup_read_check_str(scfd, ";dnskey_ttl:") ||
            !backup_read_duration(scfd, &signconf->dnskey_ttl) ||
            !backup_read_check_str(scfd, ";soa_ttl:") ||
            !backup_read_duration(scfd, &signconf->soa_ttl) ||
            !backup_read_check_str(scfd, ";soa_min:") ||
            !backup_read_duration(scfd, &signconf->soa_min) ||
            !backup_read_check_str(scfd, ";soa_serial:") ||
            !backup_read_str(scfd, &signconf->soa_serial) ||
            !backup_read_check_str(scfd, ";audit:") ||
            !backup_read_int(scfd, &signconf->audit) ||
            !backup_read_check_str(scfd, ODS_SE_FILE_MAGIC))
        {
            ods_log_error("[%s] unable to recover signconf backup file %s: corrupt "
                "backup file ", sc_str, filename?filename:"(null)");
            signconf_cleanup(signconf);
            signconf = NULL;
        }

        if (zonename) {
            se_free((void*) zonename);
        }
        ods_fclose(scfd);
        return signconf;
    }

    ods_log_debug("[%s] unable to recover signconf backup file %s", sc_str,
        filename?filename:"(null)");
    return NULL;
}


/**
 * Backup duration.
 *
 */
static void
signconf_backup_duration(FILE* fd, const char* opt, duration_type* duration)
{
    char* str = duration2string(duration);
    fprintf(fd, ";%s: %s\n", opt, str);
    se_free((void*) str);
    return;
}



/**
 * Backup signconf values.
 *
 */
void
signconf_backup(signconf_type* sc)
{
    FILE* fd = NULL;
    char* filename = NULL;

    ods_log_assert(sc);

    filename = ods_build_path(sc->name, ".sc", 0);
    fd = ods_fopen(filename, NULL, "w");
    if (fd) {
        fprintf(fd, "%s\n", ODS_SE_FILE_MAGIC);
        fprintf(fd, ";name: %s\n", sc->name?sc->name:"(null)");
        fprintf(fd, ";filename: %s\n", sc->filename?sc->filename:"(null)");
        fprintf(fd, ";last_modified: %u\n", (uint32_t) sc->last_modified);

        signconf_backup_duration(fd, "sig_resign_interval",
            sc->sig_resign_interval);
        signconf_backup_duration(fd, "sig_refresh_interval",
            sc->sig_refresh_interval);
        signconf_backup_duration(fd, "sig_validity_default",
            sc->sig_validity_default);
        signconf_backup_duration(fd, "sig_validity_denial",
            sc->sig_validity_denial);
        signconf_backup_duration(fd, "sig_jitter",
            sc->sig_jitter);
        signconf_backup_duration(fd, "sig_inception_offset",
            sc->sig_inception_offset);

        fprintf(fd, ";nsec_type: %u\n", (unsigned int) sc->nsec_type);

        signconf_backup_duration(fd, "dnskey_ttl", sc->dnskey_ttl);
        /** Keys are backed up in .dnskeys */

        signconf_backup_duration(fd, "soa_ttl", sc->soa_ttl);
        signconf_backup_duration(fd, "soa_min", sc->soa_min);
        fprintf(fd, ";soa_serial: %s\n",
            sc->soa_serial?sc->soa_serial:"(null)");

        fprintf(fd, ";audit: %i\n", sc->audit);

        fprintf(fd, "%s\n", ODS_SE_FILE_MAGIC);
        ods_fclose(fd);
    } else {
        ods_log_warning("[%s] cannot backup signconf: cannot open file "
        "%s for writing", sc_str, filename?filename:"(null)");
    }
    se_free((void*) filename);
    return;
}


/**
 * Check the SOA/Serial type.
 *
 */
static int
signconf_soa_serial_check(const char* serial) {
    if (!serial) {
        return 1;
    }

    if (strlen(serial) == 4 && strncmp(serial, "keep", 4) == 0) {
        return 0;
    }
    if (strlen(serial) == 7 && strncmp(serial, "counter", 7) == 0) {
        return 0;
    }
    if (strlen(serial) == 8 && strncmp(serial, "unixtime", 8) == 0) {
        return 0;
    }
    if (strlen(serial) == 11 && strncmp(serial, "datecounter", 11) == 0) {
        return 0;
    }
    return 1;
}

/**
 * Check signer configuration settings.
 *
 */
int
signconf_check(signconf_type* sc)
{
    int ret = 0;

    if (!sc->sig_resign_interval) {
        ods_log_error("[%s] check failed: no signature resign interval found",
            sc_str);
        ret = 1;
    }
    if (!sc->sig_refresh_interval) {
        ods_log_error("[%s] check failed: no signature resign interval found",
            sc_str);
        ret = 1;
    }
    if (!sc->sig_validity_default) {
        ods_log_error("[%s] check failed: no signature default validity found",
            sc_str);
        ret = 1;
    }
    if (!sc->sig_validity_denial) {
        ods_log_error("[%s] check failed: no signature denial validity found",
            sc_str);
        ret = 1;
    }
    if (!sc->sig_jitter) {
        ods_log_error("[%s] check failed: no signature jitter found", sc_str);
        ret = 1;
    }
    if (!sc->sig_inception_offset) {
        ods_log_error("[%s] check failed: no signature inception offset found",
            sc_str);
        ret = 1;
    }
    if (sc->nsec_type == LDNS_RR_TYPE_NSEC3) {
        if (sc->nsec3_algo == 0) {
            ods_log_error("[%s] check failed: no nsec3 algorithm found",
                sc_str);
            ret = 1;
        }
        /* iterations */
        /* salt */
        /* optout */
    } else if (sc->nsec_type != LDNS_RR_TYPE_NSEC) {
        ods_log_error("[%s] check failed: wrong nsec type %i", sc_str,
            sc->nsec_type);
        ret = 1;
    }
    if (!sc->keys || sc->keys->count == 0) {
        ods_log_error("[%s] check failed: no keys found", sc_str);
        ret = 1;
    }
    if (!sc->dnskey_ttl) {
        ods_log_error("[%s] check failed: no dnskey ttl found", sc_str);
        ret = 1;
    }
    if (!sc->soa_ttl) {
        ods_log_error("[%s] check failed: no soa ttl found", sc_str);
        ret = 1;
    }
    if (!sc->soa_min) {
        ods_log_error("[%s] check failed: no soa minimum found", sc_str);
        ret = 1;
    }
    if (signconf_soa_serial_check(sc->soa_serial) != 0) {
        ods_log_error("[%s] check failed: wrong soa serial type %s", sc_str,
            sc->soa_serial);
        ret = 1;
    }

    if (!ret) {
        ods_log_debug("[%s] signer configuration settings ok", sc_str);
    }
    return ret;

}


/**
 * Compare two signer configurations.
 *
 */
int
signconf_compare(signconf_type* a, signconf_type* b, int* update)
{
   int new_task = TASK_SIGN;

   ods_log_assert(a);
   ods_log_assert(b);

   if (a->nsec_type != b->nsec_type) {
       new_task = TASK_READ;
       *update = 1;
   } else if (a->nsec_type == LDNS_RR_TYPE_NSEC3) {
       if ((ods_strcmp(a->nsec3_salt, b->nsec3_salt) != 0) ||
           (a->nsec3_algo != b->nsec3_algo) ||
           (a->nsec3_iterations != b->nsec3_iterations) ||
           (a->nsec3_optout != b->nsec3_optout)) {

           new_task = TASK_READ;
           *update = 1;
       }
   }

   if (duration_compare(a->soa_min, b->soa_min) != 0) {
       new_task = TASK_READ;
       *update = 1;
   }

   if (keylist_compare(a->keys, b->keys) != 0) {
       new_task = TASK_READ;
   }

   /* not like python: reschedule if resign/refresh differs */
   /* this needs review, tasks correct on signconf changes? */

   return new_task;
}

/**
 * Clean up signer configuration.
 *
 */
void
signconf_cleanup(signconf_type* sc)
{
    if (sc) {
        if (sc->sig_resign_interval) {
            duration_cleanup(sc->sig_resign_interval);
            sc->sig_resign_interval = NULL;
        }
        if (sc->sig_refresh_interval) {
            duration_cleanup(sc->sig_refresh_interval);
            sc->sig_refresh_interval = NULL;
        }
        if (sc->sig_validity_default) {
            duration_cleanup(sc->sig_validity_default);
            sc->sig_validity_default = NULL;
        }
        if (sc->sig_validity_denial) {
            duration_cleanup(sc->sig_validity_denial);
            sc->sig_validity_denial = NULL;
        }
        if (sc->sig_jitter) {
            duration_cleanup(sc->sig_jitter);
            sc->sig_jitter = NULL;
        }
        if (sc->sig_inception_offset) {
            duration_cleanup(sc->sig_inception_offset);
            sc->sig_inception_offset = NULL;
        }
        if (sc->dnskey_ttl) {
            duration_cleanup(sc->dnskey_ttl);
            sc->dnskey_ttl = NULL;
        }
        if (sc->soa_ttl) {
            duration_cleanup(sc->soa_ttl);
            sc->soa_ttl = NULL;
        }
        if (sc->soa_min) {
            duration_cleanup(sc->soa_min);
            sc->soa_min = NULL;
        }
        if (sc->keys) {
            keylist_cleanup(sc->keys);
            sc->keys = NULL;
        }
        if (sc->nsec3_salt) {
            se_free((void*)sc->nsec3_salt);
            sc->nsec3_salt = NULL;
        }
        if (sc->soa_serial) {
            se_free((void*)sc->soa_serial);
            sc->soa_serial = NULL;
        }
        if (sc->filename) {
            se_free((void*)sc->filename);
            sc->filename = NULL;
        }
        se_free((void*)sc);
    }
}


/**
 * Print sign configuration.
 *
 */
void
signconf_print(FILE* out, signconf_type* sc, const char* name)
{
    char* s = NULL;

    fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    if (sc) {
        fprintf(out, "<SignerConfiguration>\n");
        fprintf(out, "\t<Zone name=\"%s\">\n", name?name:"(null)");

        /* Signatures */
        fprintf(out, "\t\t<Signatures>\n");
        s = duration2string(sc->sig_resign_interval);
        fprintf(out, "\t\t\t<Resign>%s</Resign>\n", s?s:"(null)");
        se_free((void*)s);

        s = duration2string(sc->sig_refresh_interval);
        fprintf(out, "\t\t\t<Refresh>%s</Refresh>\n", s?s:"(null)");
        se_free((void*)s);

        fprintf(out, "\t\t\t<Validity>\n");

        s = duration2string(sc->sig_validity_default);
        fprintf(out, "\t\t\t\t<Default>%s</Default>\n", s?s:"(null)");
        se_free((void*)s);

        s = duration2string(sc->sig_validity_denial);
        fprintf(out, "\t\t\t\t<Denial>%s</Denial>\n", s?s:"(null)");
        se_free((void*)s);

        fprintf(out, "\t\t\t</Validity>\n");

        s = duration2string(sc->sig_jitter);
        fprintf(out, "\t\t\t<Jitter>%s</Jitter>\n", s?s:"(null)");
        se_free((void*)s);

        s = duration2string(sc->sig_inception_offset);
        fprintf(out, "\t\t\t<InceptionOffset>%s</InceptionOffset>\n",
            s?s:"(null)");
        se_free((void*)s);

        fprintf(out, "\t\t</Signatures>\n");
        fprintf(out, "\n");

        /* Denial */
        fprintf(out, "\t\t<Denial>\n");
        if (sc->nsec_type == LDNS_RR_TYPE_NSEC) {
            fprintf(out, "\t\t\t<NSEC />\n");
        } else if (sc->nsec_type == LDNS_RR_TYPE_NSEC3) {
            fprintf(out, "\t\t\t<NSEC3>\n");
            if (sc->nsec3_optout) {
                fprintf(out, "\t\t\t\t<OptOut />\n");
            }
            fprintf(out, "\t\t\t\t<Hash>\n");
            fprintf(out, "\t\t\t\t\t<Algorithm>%i</Algorithm>\n",
                sc->nsec3_algo);
            fprintf(out, "\t\t\t\t\t<Iterations>%i</Iterations>\n",
                sc->nsec3_iterations);
            fprintf(out, "\t\t\t\t\t<Salt>%s</Salt>\n",
                sc->nsec3_salt?sc->nsec3_salt:"(null)");
            fprintf(out, "\t\t\t\t</Hash>\n");
            fprintf(out, "\t\t\t</NSEC3>\n");
        }
        fprintf(out, "\t\t</Denial>\n");
        fprintf(out, "\n");

        /* Keys */
        fprintf(out, "\t\t<Keys>\n");
        s = duration2string(sc->dnskey_ttl);
        fprintf(out, "\t\t\t<TTL>%s</TTL>\n", s?s:"(null)");
        se_free((void*)s);
        fprintf(out, "\n");
        keylist_print(out, sc->keys);
        fprintf(out, "\t\t</Keys>\n");
        fprintf(out, "\n");

        /* SOA */
        fprintf(out, "\t\t<SOA>\n");
        s = duration2string(sc->soa_ttl);
        fprintf(out, "\t\t\t<TTL>%s</TTL>\n", s?s:"(null)");
        se_free((void*)s);

        s = duration2string(sc->soa_min);
        fprintf(out, "\t\t\t<Minimum>%s</Minimum>\n", s?s:"(null)");
        se_free((void*)s);

        fprintf(out, "\t\t\t<Serial>%s</Serial>\n",
            sc->soa_serial?sc->soa_serial:"(null)");
        fprintf(out, "\t\t</SOA>\n");
        fprintf(out, "\n");

        /* Audit */
        if (sc->audit) {
            fprintf(out, "\t\t<Audit />\n");
            fprintf(out, "\n");
        }

        fprintf(out, "\t</Zone>\n");
        fprintf(out, "</SignerConfiguration>\n");
    }

    return;
}
