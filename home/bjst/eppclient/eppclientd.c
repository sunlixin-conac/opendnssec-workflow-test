/*
 * $Id $
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
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sqlite3.h>
#include <errno.h>

#define RUNNING_DIR	"var/run"
#define LOCK_FILE	RUNNING_DIR "/eppclient.lock"
#define LOG_FILE	RUNNING_DIR "/eppclient.log"
#define CONFIG_FILE     "eppclientd.conf"

static struct {
    sqlite3* db;
    char queue[250];
    char user[80];
    char password[80];
} global = {0};

void signal_handler(int sig)
{
    switch(sig) {
	case SIGHUP:
            syslog(LOG_INFO, "got SIGHUP");
            break;

	case SIGTERM:
            syslog(LOG_INFO, "got SIGTERM");
            exit(0);
            break;
    }
}

void daemonize()
{
    if(getppid()==1)
        return; /* already a daemon */
    int i=fork();
    if (i<0)
        exit(1); /* fork error */
    if (i>0)
        exit(0); /* parent exits */

    /* child (daemon) continues */
    setsid(); /* obtain a new process group */
    for (i=getdtablesize();i>=0;--i)
        close(i); /* close all descriptors */
    /* open dummy stdin/-out/-err */
    i=open("/dev/null",O_RDWR);
    dup(i);
    dup(i); /* handle standard I/O */

    umask(027); /* set newly created file permissions */
    chdir(RUNNING_DIR); /* change running directory */

    int lfp = open(global.queue, O_RDWR|O_CREAT, 0640);
    if (lfp < 0) {
        syslog(LOG_ERR, "%s: %s", global.queue, strerror(errno));
        exit(1); /* can not open */
    }
    if (lockf(lfp,F_TLOCK,0) < 0) {
        syslog(LOG_ERR, "Daemon already running");
        exit(0); /* can not lock */
    }

    char str[10];
    sprintf(str,"%d\n",getpid());
    write(lfp,str,strlen(str)); /* record pid to lockfile */
    signal(SIGCHLD,SIG_IGN); /* ignore child */
    signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGHUP,signal_handler); /* catch hangup signal */
    signal(SIGTERM,signal_handler); /* catch kill signal */
}

void read_config(void)
{
    FILE* f = fopen(CONFIG_FILE, "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof line, f)) {
            char* label = strtok(line, "\t =");
            char* data = strtok(NULL, "\t =\n");
            if (!strcmp(label, "queue"))
                strcpy(global.queue, data);
            else if (!strcmp(label, "user"))
                strcpy(global.user, data);
            else if (!strcmp(label, "password"))
                strcpy(global.password, data);
            else {
                printf("Unknown config parameter: %s\n", label);
                exit(-1);
            }
        }
    }
    else {
        perror(CONFIG_FILE);
        exit(-1);
    }
}

void init(void)
{
    openlog("eppclient", 0, LOG_USER);

    read_config();
    
    /* prepare sqlite */
    int rc = sqlite3_open_v2(global.queue, &global.db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc) {
        syslog(LOG_ERR, "Can't open %s: %s", global.queue, sqlite3_errmsg(global.db));
        sqlite3_close(global.db);
        exit(1);
    }

    /* ensure database has the necessary tables */
    rc = sqlite3_exec(global.db, "BEGIN TRANSACTION;", 0,0,0);
    rc = sqlite3_exec(global.db, "CREATE TABLE IF NOT EXISTS jobs (job INTEGER PRIMARY KEY, zone TEXT);",0,0,0);
    rc = sqlite3_exec(global.db, "CREATE TABLE IF NOT EXISTS keys (job NUMERIC, key TEXT);",0,0,0);
    rc = sqlite3_exec(global.db, "END TRANSACTION;",0,0,0);
}

int count_jobs(void)
{
    sqlite3_stmt* sth;
    int rc = sqlite3_prepare_v2(global.db, "SELECT count(*) FROM jobs",
                                -1, &sth, NULL);
    int count = -1;
    while (1) {
        rc = sqlite3_step(sth);
        switch (rc) {
            case 100: /* row */
                count = sqlite3_column_int(sth, 0);
                break;

            case 101: /* done */
                sqlite3_finalize(sth);
                return count;
                
            default:
                syslog(LOG_ERR, "%d: step() gave error: %d", __LINE__, rc);
                return -1;
        }
    }

    return -1;
}

void work(void)
{
    sqlite3_stmt* sth;

    /* get first job */
    sqlite3_prepare_v2(global.db,
                       "SELECT job,zone FROM jobs ORDER BY job LIMIT 1",
                       -1, &sth, NULL);
    int rc = sqlite3_step(sth);
    if (rc < 100) {
        syslog(LOG_ERR, "%d:sqlite says %s",
               __LINE__, sqlite3_errmsg(global.db));
        sqlite3_finalize(sth);
        return;
    }
    int job = sqlite3_column_int(sth, 0);
    const unsigned char* zone = sqlite3_column_text(sth, 1);

    /* get keys */
    sqlite3_prepare_v2(global.db, "SELECT key FROM keys WHERE job = ?",
                       -1, &sth, NULL);
    
    while ((rc = sqlite3_step(sth))) {
        if (rc == 101)
            break;

        if (rc < 100) {
            syslog(LOG_ERR, "%d:sqlite says %s",
                   __LINE__, sqlite3_errmsg(global.db));
            sqlite3_finalize(sth);
            return;
        }
    }
}

int main()
{
    init();
    daemonize();
    
    while (1) {
        int count = count_jobs();
        if (count) {
            syslog(LOG_INFO, "%d jobs in queue", count);
            work();
        }
        sleep(1);
    }

    return 0;
}
