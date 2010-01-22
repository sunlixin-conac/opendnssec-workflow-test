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
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sqlite3.h>

#define DB_PATH         "/var/spool/eppclient/queue"

sqlite3* init_sqlite(void)
{
    sqlite3 *db;

    int rc = sqlite3_open_v2(DB_PATH, &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc) {
        printf("Can't open " DB_PATH ": %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(1);
    }

    return db;
}

void insert_data(sqlite3* db, int argc, char** argv)
{
    int rc;
    char buf[1024];

    /* wrap everything in a transaction */
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", 0,0,0);
    
    sqlite3_stmt* sth;

    /* delete pending jobs for this zone */
    rc = sqlite3_prepare_v2(db, "SELECT job FROM jobs WHERE zone=?",
                            -1, &sth, NULL);
    sqlite3_bind_text(sth, 1, argv[1], strlen(argv[1]), SQLITE_STATIC);
    while (sqlite3_step(sth) == 100) {
        int job = sqlite3_column_int(sth, 0);
        snprintf(buf, sizeof buf, "DELETE FROM keys WHERE job = %d;", job);
        rc = sqlite3_exec(db, buf, 0,0,0);
        snprintf(buf, sizeof buf, "DELETE FROM jobs WHERE job = %d;", job);
        rc = sqlite3_exec(db, buf, 0,0,0);
    }
    sqlite3_finalize(sth);
    
    /* store zone */
    rc = sqlite3_prepare_v2(db, "INSERT INTO jobs (zone) VALUES (?)",
                            -1, &sth, NULL);
    sqlite3_bind_text(sth, 1, argv[1], strlen(argv[1]), SQLITE_STATIC);
    sqlite3_step(sth);

    int job = sqlite3_last_insert_rowid(db);
    
    /* store keys */
    int i;
    for (i=2; i<argc; i++) {
        rc = sqlite3_prepare_v2(db, "INSERT INTO keys (job, key) VALUES (?,?)",
                                -1, &sth, NULL);
        sqlite3_bind_int(sth, 1, job);
        sqlite3_bind_text(sth, 2, argv[i], strlen(argv[i]), SQLITE_STATIC);
        sqlite3_step(sth);
    }
    sqlite3_finalize(sth);

    rc = sqlite3_exec(db, "COMMIT TRANSACTION;", 0,0,0);

}

int main(int argc, char** argv)
{
    if (argc < 3) {
        printf("usage: %s [zone] [keys] ...\n", argv[0]);
        return -1;
    }

    sqlite3* db = init_sqlite();
    insert_data(db, argc, argv);

    return 0;
}
