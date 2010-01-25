/*
 * $Id$
 *
 * Copyright (c) 2009 .SE (The Internet Infrastructure Foundation).
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
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include "config.h"

#define CONFIG_FILE     "eppclientd.conf"

struct configdata config;

void read_config(void)    
{
    memset(&config, 0, sizeof config);
    
    FILE* f = fopen(CONFIG_FILE, "r");
    if (!f)
        f = fopen("/etc/" CONFIG_FILE, "r");
    if (!f)
        f = fopen("/etc/opt/" CONFIG_FILE, "r");
    if (!f)
        f = fopen("/usr/local/etc/" CONFIG_FILE, "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof line, f)) {
            char* label = strtok(line, "\t =");
            char* data = strtok(NULL, "\t =\n");
            if (!strcmp(label, "database"))
                strcpy(config.dbpath, data);
            else if (!strcmp(label, "pipe"))
                strcpy(config.pipe, data);
            else if (!strcmp(label, "pidfile"))
                strcpy(config.pidfile, data);
            else if (!strcmp(label, "user"))
                strcpy(config.user, data);
            else if (!strcmp(label, "password"))
                strcpy(config.password, data);
            else {
                printf("Unknown config parameter: %s\n", label);
                exit(-1);
            }
        }
    }
    else {
        syslog(LOG_ERR, "%s: %s", CONFIG_FILE, strerror(errno));
        perror(CONFIG_FILE);
        exit(-1);
    }
}
