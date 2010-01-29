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
 * Test reading zonelist.xml
 *
 */

#include "config.h"
#include "scheduler/task.h"
#include "signer/zone.h"
#include "util/log.h"

#include <ldns/ldns.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


/**
 * Prints usage.
 *
 */
static void
usage(FILE* out, const char* program)
{
    fprintf(out, "Usage: %s\n", program);
    fprintf(out, "test task list.\n\n");
    fprintf(out, "BSD licensed, see LICENSE in source package for "
                 "details.\n");
    fprintf(out, "Version %s. Report bugs to <%s>.\n",
        PACKAGE_VERSION, PACKAGE_BUGREPORT);
}

/**
 * Main. test zonelist file parsing.
 *
 */
int
main(int argc, char* argv[])
{
    tasklist_type* tl = NULL;
    task_type* task = NULL;
    zone_type* zone[(argc-1)];
    time_t when = 1257766772;
    int i, what = TASK_READ;

    if (argc <= 1) {
        usage(stderr, argv[0]);
        exit(1);
    }

    se_log_init(NULL, 0, 0);

    tl = tasklist_create();
    for (i=0; i < argc-1; i++) {
        zone[i] = zone_create(argv[i+1], LDNS_RR_CLASS_IN);
        task = task_create(what, when, zone[i]->name, zone[i]);
        tasklist_schedule_task(tl, task, 0);
        if (i > 1) {
            what = TASK_ADDKEYS;
        }
        if (i > 3) {
            what = TASK_NSECIFY;
        }
        if (i > 5) {
            what = TASK_SIGN;
        }
        if (i > 7) {
            what = TASK_WRITE;
        }
        /* when += i; */
    }

    tasklist_print(stdout, tl);
    tasklist_cleanup(tl);

    for (i=0; i < argc-1; i++) {
        zone_cleanup(zone[i]);
    }
    return 0;
}
