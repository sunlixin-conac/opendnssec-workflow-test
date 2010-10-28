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
 * Tasks.
 *
 */

#include "scheduler/task.h"
#include "signer/backup.h"
#include "signer/zone.h"
#include "util/duration.h"
#include "util/file.h"
#include "util/log.h"
#include "util/se_malloc.h"

#include <ldns/ldns.h> /* ldns_dname_*(), ldns_rdf_*(), ldns_rbtree_*() */
#include <time.h> /* ctime() */
#include <stdio.h> /* fprintf(), snprintf() */
#include <stdlib.h>
#include <string.h> /* strlen() */

static void log_task(task_type* task);


/**
 * Create a new task.
 *
 */
task_type*
task_create(int what, time_t when, const char* who, struct zone_struct* zone)
{
    task_type* task = (task_type*) se_malloc(sizeof(task_type));

    se_log_assert(who);
    se_log_assert(zone);

    task->what = what;
    task->when = when;
    task->backoff = 0;
    task->who = se_strdup(who);
    task->dname = ldns_dname_new_frm_str(who);
    task->flush = 0;
    task->zone = zone;
    task->zone->task = task;
    return task;
}


/**
 * Recover a task from backup.
 *
 */
task_type*
task_recover_from_backup(const char* filename, struct zone_struct* zone)
{
    task_type* task = NULL;
    FILE* fd = NULL;
    const char* who = NULL;
    int what = 0;
    time_t when = 0;
    int flush = 0;
    time_t backoff = 0;

    se_log_assert(zone);
    fd = se_fopen(filename, NULL, "r");
    if (fd) {
        if (!backup_read_check_str(fd, ODS_SE_FILE_MAGIC) ||
            !backup_read_check_str(fd, ";who:") ||
            !backup_read_str(fd, &who) ||
            !backup_read_check_str(fd, ";what:") ||
            !backup_read_int(fd, &what) ||
            !backup_read_check_str(fd, ";when:") ||
            !backup_read_time_t(fd, &when) ||
            !backup_read_check_str(fd, ";flush:") ||
            !backup_read_int(fd, &flush) ||
            !backup_read_check_str(fd, ";backoff:") ||
            !backup_read_time_t(fd, &backoff) ||
            !backup_read_check_str(fd, ODS_SE_FILE_MAGIC))
        {
            se_log_error("unable to recover task from file %s: file corrupted",
                filename?filename:"(null)");
            task = NULL;
        } else {
            task = task_create((task_id) what, when, who, zone);
            task->flush = flush;
            task->backoff = backoff;
        }
        se_free((void*)who);
        se_fclose(fd);
        return task;
    }

    se_log_debug("unable to recover task from file %s: no such file or directory",
        filename?filename:"(null)");
    return NULL;
}


/**
 * Backup task.
 *
 */
void
task_backup(task_type* task)
{
    char* filename = NULL;
    FILE* fd = NULL;

    se_log_assert(task);

    if (task && task->who) {
        filename = se_build_path(task->who, ".task", 0);
        fd = se_fopen(filename, NULL, "w");
        se_free((void*)filename);
    }

    if (fd) {
        fprintf(fd, "%s\n", ODS_SE_FILE_MAGIC);
        fprintf(fd, ";who: %s\n", task->who);
        fprintf(fd, ";what: %i\n", (int) task->what);
        fprintf(fd, ";when: %u\n", (uint32_t) task->when);
        fprintf(fd, ";flush: %i\n", task->flush);
        fprintf(fd, ";backoff: %u\n", (uint32_t) task->backoff);
        fprintf(fd, "%s\n", ODS_SE_FILE_MAGIC);
        se_fclose(fd);
    } else {
        se_log_warning("cannot backup task for zone %s: cannot open file "
        "%s.task for writing",
        task->who?task->who:"(null)", task->who?task->who:"(null)");
    }
    return;
}


/**
 * Clean up task.
 *
 */
void
task_cleanup(task_type* task)
{
    if (task) {
        if (task->dname) {
            ldns_rdf_deep_free(task->dname);
            task->dname = NULL;
        }
        if (task->who) {
            se_free((void*)task->who);
            task->who = NULL;
        }
        se_free((void*)task);
    } else {
        se_log_warning("cleanup empty task");
    }
    return;
}


/**
 * Compare tasks.
 *
 */
int task_compare(const void* a, const void* b)
{
    task_type* x = (task_type*)a;
    task_type* y = (task_type*)b;

    se_log_assert(x);
    se_log_assert(y);

    if (x->when != y->when) {
        return (int) x->when - y->when;
    }
    return ldns_dname_compare((const void*) x->dname, (const void*) y->dname);
}


/**
 * Convert task id to string.
 *
 */
static const char*
taskid2str(int taskid)
{
    switch (taskid) {
        case TASK_NONE:
            return "do nothing with";
            break;
        case TASK_READ:
            return "read and sign";
            break;
        case TASK_ADDKEYS:
            return "add keys and sign";
            break;
        case TASK_UPDATE:
            return "prepare and sign";
            break;
        case TASK_NSECIFY:
            return "nsecify and sign";
            break;
        case TASK_SIGN:
            return "sign";
            break;
        case TASK_AUDIT:
            return "audit";
            break;
        case TASK_WRITE:
            return "output signed";
            break;
        default:
            return "???";
            break;
    }

    return "???";
}


/**
 * Convert task to string.
 *
 */
char*
task2str(task_type* task, char* buftask)
{
    time_t now = time_now();
    char* strtime = NULL;
    char* strtask = NULL;

    se_log_assert(task);

    if (task) {
        if (task->flush) {
            strtime = ctime(&now);
        } else {
            strtime = ctime(&task->when);
        }
        if (strtime) {
            strtime[strlen(strtime)-1] = '\0';
        }
        if (buftask) {
            (void)snprintf(buftask, ODS_SE_MAXLINE, "On %s I will %s zone %s\n",
                strtime?strtime:"(null)", taskid2str(task->what),
                task->who?task->who:"(null)");
            return buftask;
        } else {
            strtask = (char*) se_calloc(ODS_SE_MAXLINE, sizeof(char));
            snprintf(strtask, ODS_SE_MAXLINE, "On %s I will %s zone %s\n",
                strtime?strtime:"(null)", taskid2str(task->what),
                task->who?task->who:"(null)");
            return strtask;
        }
    }
    return NULL;
}


/**
 * Print task.
 *
 */
void
task_print(FILE* out, task_type* task)
{
    time_t now = time_now();
    char* strtime = NULL;

    se_log_assert(out);
    se_log_assert(task);

    if (task) {
        if (task->flush) {
            strtime = ctime(&now);
        } else {
            strtime = ctime(&task->when);
        }
        if (strtime) {
            strtime[strlen(strtime)-1] = '\0';
        }
        fprintf(out, "On %s I will %s zone %s\n", strtime?strtime:"(null)",
            taskid2str(task->what), task->who?task->who:"(null)");
    }
    return;
}


/**
 * Log task.
 *
 */
static void
log_task(task_type* task)
{
    time_t now = time_now();
    char* strtime = NULL;

    se_log_assert(task);
    if (task) {
        if (task->flush) {
            strtime = ctime(&now);
        } else {
            strtime = ctime(&task->when);
        }
        if (strtime) {
            strtime[strlen(strtime)-1] = '\0';
        }
        se_log_debug("On %s I will %s zone %s", strtime?strtime:"(null)",
            taskid2str(task->what), task->who?task->who:"(null)");
    }
    return;
}


/**
 * New task list.
 *
 */
tasklist_type*
tasklist_create(void)
{
    tasklist_type* tl = (tasklist_type*) se_malloc(sizeof(tasklist_type));

    se_log_debug("create task list");
    tl->tasks = ldns_rbtree_create(task_compare);
    tl->loading = 0;
    lock_basic_init(&tl->tasklist_lock);
    return tl;
}


/**
 * Clean up task list.
 *
 */
void
tasklist_cleanup(tasklist_type* list)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    task_type* task = NULL;

    if (list) {
        se_log_debug("clean up task list");
        if (list->tasks) {
            node = ldns_rbtree_first(list->tasks);
            while (node != LDNS_RBTREE_NULL) {
                task = (task_type*) node->data;
                task_cleanup(task);
                node = ldns_rbtree_next(node);
            }
            se_rbnode_free(list->tasks->root);
            ldns_rbtree_free(list->tasks);
            list->tasks = NULL;
        }
        lock_basic_destroy(&list->tasklist_lock);
        se_free((void*) list);
    } else {
        se_log_warning("cleanup empty task list");
    }
    return;
}


/**
 * Convert task to a tree node.
 *
 */
static ldns_rbnode_t*
task2node(task_type* task)
{
    ldns_rbnode_t* node = (ldns_rbnode_t*) se_malloc(sizeof(ldns_rbnode_t));
    node->key = task;
    node->data = task;
    return node;
}


/**
 * Look up task.
 *
 */
static task_type*
tasklist_lookup(tasklist_type* list, task_type* task)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;

    se_log_assert(task);
    se_log_assert(list);
    se_log_assert(list->tasks);

    node = ldns_rbtree_search(list->tasks, task);
    if (node && node != LDNS_RBTREE_NULL) {
        return (task_type*) node->data;
    }
    return NULL;
}


/**
 * Schedule a task.
 *
 */
task_type*
tasklist_schedule_task(tasklist_type* list, task_type* task, int log)
{
    ldns_rbnode_t* new_node = NULL;
    zone_type* zone = NULL;

    se_log_assert(list);
    se_log_assert(list->tasks);
    se_log_assert(task);
    se_log_debug("schedule task");

    zone = task->zone;
    if (zone->in_progress) {
        se_log_error("unable to schedule task %s for zone %s: "
            " zone in progress", taskid2str(task->what),
            task->who?task->who:"(null)");
        task_cleanup(task);
        return NULL;
    }

    if (tasklist_lookup(list, task) != NULL) {
        se_log_error("unable to schedule task %s for zone %s: "
            " already present", taskid2str(task->what),
            task->who?task->who:"(null)");
        task_cleanup(task);
        return NULL;
    }

    new_node = task2node(task);
    if (ldns_rbtree_insert(list->tasks, new_node) == NULL) {
        se_log_error("unable to schedule task %s for zone %s: "
            " insert failed", taskid2str(task->what),
            task->who?task->who:"(null)");
        task_cleanup(task);
        se_free((void*) new_node);
        return NULL;
    }

    if (log) {
        log_task(task);
    }
    return task;
}


/**
 * Flush task list.
 *
 */
void
tasklist_flush(tasklist_type* list, task_id what)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    task_type* task = NULL;

    se_log_assert(list);
    se_log_assert(list->tasks);
    se_log_debug("flush task list");

    node = ldns_rbtree_first(list->tasks);
    while (node && node != LDNS_RBTREE_NULL) {
        task = (task_type*) node->data;
        task->flush = 1;
        if (what != TASK_NONE) {
            task->what = what;
        }
        node = ldns_rbtree_next(node);
    }
    return;
}


/**
 * Delete task from task list.
 *
 */
task_type*
tasklist_delete_task(tasklist_type* list, task_type* task)
{
    ldns_rbnode_t* del_node = LDNS_RBTREE_NULL;
    task_type* del_task = NULL;

    se_log_assert(list);
    se_log_assert(list->tasks);

    if (task) {
        se_log_debug("delete task from list");
        del_node = ldns_rbtree_delete(list->tasks, (const void*) task);
        if (del_node) {
            del_task = (task_type*) del_node->data;
            se_free((void*)del_node);
            return del_task;
        } else {
            se_log_error("delete task failed");
            log_task(task);
        }
    } else {
        se_log_warning("delete empty task from list");
    }
    return NULL;
}


/**
 * Pop task from task list.
 *
 */
task_type*
tasklist_pop_task(tasklist_type* list)
{
    ldns_rbnode_t* first_node = LDNS_RBTREE_NULL;
    task_type* pop = NULL;
    time_t now;

    se_log_assert(list);
    se_log_assert(list->tasks);

    first_node = ldns_rbtree_first(list->tasks);
    if (!first_node) {
        return NULL;
    }

    now = time_now();
    pop = (task_type*) first_node->key;
    if (pop && (pop->flush || pop->when <= now)) {
        if (pop->flush) {
            se_log_debug("flush task for zone %s", pop->who?pop->who:"(null)");
        } else {
            se_log_debug("pop task for zone %s", pop->who?pop->who:"(null)");
        }
        pop->flush = 0;
        return tasklist_delete_task(list, pop);
    }
    return NULL;
}


/**
 * First task from task list.
 *
 */
task_type*
tasklist_first_task(tasklist_type* list)
{
    ldns_rbnode_t* first_node = LDNS_RBTREE_NULL;
    task_type* pop = NULL;

    se_log_assert(list);
    se_log_assert(list->tasks);

    first_node = ldns_rbtree_first(list->tasks);
    if (!first_node) {
        return NULL;
    }

    pop = (task_type*) first_node->data;
    return pop;
}


/**
 * Print task list.
 *
 */
void
tasklist_print(FILE* out, tasklist_type* list)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    task_type* task = NULL;

    se_log_assert(out);
    se_log_assert(list);

    node = ldns_rbtree_first(list->tasks);
    while (node && node != LDNS_RBTREE_NULL) {
        task = (task_type*) node->data;
        task_print(out, task);
        node = ldns_rbtree_next(node);
    }
    fprintf(out, "\n");
    return;
}