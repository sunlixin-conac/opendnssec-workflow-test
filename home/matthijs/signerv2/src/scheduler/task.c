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
#include "signer/zone.h"
#include "signer/tools.h"
#include "util/duration.h"
#include "util/file.h"
#include "util/log.h"
#include "util/se_malloc.h"

#include <ldns/ldns.h> /* ldns_rdf*, ldns_rbtree* */
#include <time.h> /* time(), ctime() */
#include <stdio.h> /* fprintf(), snprintf() */
#include <stdlib.h>

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
    se_log_debug("create task for zone '%s'", who);

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
 * Clean up task.
 *
 */
void
task_cleanup(task_type* task)
{
    if (task) {
        se_log_debug3("clean up task");
        ldns_rdf_deep_free(task->dname);
        se_free((void*)task->who);
        se_free((void*)task);
        se_log_debug3("clean up task: done");
    } else {
        se_log_debug3("clean up task: null");
    }
}


/**
 * Perform task.
 *
 */
void task_perform(task_type* task)
{
    zone_type* zone = NULL;

    se_log_assert(task);
    se_log_debug("perform task");

    if (!task->zone) {
        se_log_error("cannot perform task: not bound to a zone");
        return;
    }

    zone = task->zone;
    switch (task->what) {
        case TASK_NONE:
            se_log_warning("no task for zone '%s'", task->who);
            break;
        case TASK_READ:
            if (tools_read_input(zone) != 0) {
                se_log_error("task [read zone '%s'] failed", task->who);
                goto task_perform_fail;
                break;
            }
            task->what = TASK_ADDKEYS;
        case TASK_ADDKEYS:
            if (tools_add_dnskeys(zone) != 0) {
                se_log_error("task [add dnskeys to zone '%s'] failed",
                    task->who);
                goto task_perform_fail;
                break;
            }
            task->what = TASK_NSECIFY;
        case TASK_NSECIFY:
            if (tools_nsecify(zone, 1) != 0) {
                se_log_error("task [nsecify zone '%s'] failed", task->who);
                goto task_perform_fail;
                break;
            }
            task->what = TASK_SIGN;
        case TASK_SIGN:
            if (tools_sign(zone) != 0) {
                se_log_error("task [sign zone '%s'] failed", task->who);
                goto task_perform_fail;
                break;
            }
            task->what = TASK_AUDIT;
        case TASK_AUDIT:
            if (tools_audit(zone) != 0) {
                se_log_error("task [audit zone '%s'] failed", task->who);
                task->what = TASK_SIGN;
                goto task_perform_fail;
                break;
            }
            task->what = TASK_WRITE;
        case TASK_WRITE:
            if (tools_write_output(zone) != 0) {
                se_log_error("task [write zone '%s'] failed", task->who);
                goto task_perform_fail;
                break;
            }
            task->what = TASK_SIGN;
            task->when = time(NULL) +
                duration2time(zone->signconf->sig_resign_interval);
            break;
        default:
            se_log_warning("unknown task id %i, zone '%s' "
                "trying full sign", task->what, task->who);
            task->what = TASK_READ;
            task->when = time(NULL);
            break;
    }
    se_log_debug3("perform task: done");
    return;

task_perform_fail:
    if (zone->backoff) {
        zone->backoff *= 2;
        if (zone->backoff > MAX_BACKOFF) {
            zone->backoff = MAX_BACKOFF;
        }
    } else {
        zone->backoff = 1;
    }

    task->when += zone->backoff;

    se_log_debug3("perform task: done");
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

    se_log_none("compare tasks");
    se_log_assert(x);
    se_log_assert(y);
    se_log_none("compare tasks '%s' and '%s'", x->who, y->who);

    if (x->flush != y->flush) {
        return (int) y->flush - x->flush;
    }
    if (x->when != y->when) {
        return (int) x->when - y->when;
    }
/*
    if (x->what != y->what) {
        return (int) x->what - y->what;
    }
*/
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
        case TASK_NSECIFY:
            return "rensec and resign";
            break;
        case TASK_SIGN:
            return "resign";
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
    time_t now = time(NULL);
    char* strtime = NULL;
    char* strtask = NULL;

    se_log_assert(task);
    if (!buftask) {
        strtask = (char*) se_calloc(MAX_LINE, sizeof(char));
    }

    if (task) {
        if (task->flush) {
            strtime = ctime(&now);
        } else {
            strtime = ctime(&task->when);
        }
        strtime[strlen(strtime)-1] = '\0';
        if (buftask) {
            (void)snprintf(buftask, MAX_LINE, "On %s I will %s zone '%s'\n", strtime,
                taskid2str(task->what), task->who);
            return buftask;
        } else {
            snprintf(strtask, MAX_LINE, "On %s I will %s zone '%s'\n", strtime,
                taskid2str(task->what), task->who);
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
    time_t now = time(NULL);
    char* strtime = NULL;

    se_log_assert(out);
    se_log_assert(task);
    se_log_debug("print task");

    if (task) {
        if (task->flush) {
            strtime = ctime(&now);
        } else {
            strtime = ctime(&task->when);
        }
        strtime[strlen(strtime)-1] = '\0';
        fprintf(out, "On %s I will %s zone '%s'\n", strtime,
            taskid2str(task->what), task->who);
    }

    se_log_debug3("print task: done");
}


/**
 * Log task.
 *
 */
static void
log_task(task_type* task)
{
    time_t now = time(NULL);
    char* strtime = NULL;

    se_log_assert(task);
    if (task) {
        if (task->flush) {
            strtime = ctime(&now);
        } else {
            strtime = ctime(&task->when);
        }
        strtime[strlen(strtime)-1] = '\0';
        se_log_info("On %s I will %s zone '%s'", strtime,
            taskid2str(task->what), task->who);
    }
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

    se_log_debug3("create task list: done");
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

        node = ldns_rbtree_first(list->tasks);
        while (node != LDNS_RBTREE_NULL) {
            task = (task_type*) node->key;
            task_cleanup(task);
            node = ldns_rbtree_next(node);
        }
        se_rbnode_free(list->tasks->root);
        ldns_rbtree_free(list->tasks);
        lock_basic_destroy(&list->tasklist_lock);
        se_free((void*) list);
        se_log_debug3("clean up task list: done");
    } else {
        se_log_debug3("clean up task list: null");
    }

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
    task_type* walk = NULL;

    se_log_assert(task);
    se_log_assert(list);
    se_log_assert(list->tasks);
    se_log_debug3("lookup task '%s'", task->who);

    node = ldns_rbtree_first(list->tasks);
    while (node && node != LDNS_RBTREE_NULL) {
        walk = (task_type*) node->key;
        if (ldns_dname_compare(walk->dname, task->dname) == 0) {
            se_log_debug3("lookup task: hit");
            return walk;
        }
        node = ldns_rbtree_next(node);
    }
    se_log_debug3("lookup task: not found");
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
        se_log_error("unable to schedule task[%i] for zone '%s': "
            " zone in progress", task->what, task->who);
        task_cleanup(task);
        return NULL;
    }

    if (tasklist_lookup(list, task) != NULL) {
        se_log_error("unable to schedule task[%i] for zone '%s': "
            " already present", task->what, task->who);
        task_cleanup(task);
        return NULL;
    }

    new_node = task2node(task);
    if (ldns_rbtree_insert(list->tasks, new_node) == NULL) {
        se_log_error("unable to schedule task[%i] for zone '%s': "
            " insert failed", task->what, task->who);
        task_cleanup(task);
        se_free((void*) new_node);
        return NULL;
    }

    if (log) {
        log_task(task);
    }
    se_log_debug3("schedule task: done");
    return task;
}


/**
 * Flush task list.
 *
 */
void
tasklist_flush(tasklist_type* list)
{
    ldns_rbnode_t* node = LDNS_RBTREE_NULL;
    task_type* task = NULL;

    se_log_assert(list);
    se_log_assert(list->tasks);
    se_log_debug("flush task list");

    node = ldns_rbtree_first(list->tasks);
    while (node && node != LDNS_RBTREE_NULL) {
        task = (task_type*) node->key;
        task->flush = 1;
        node = ldns_rbtree_next(node);
    }

    se_log_debug3("flush task list: done");
}


/**
 * Delete task from task list.
 *
 */
task_type*
tasklist_delete_task(tasklist_type* list, task_type* task)
{
    ldns_rbnode_t* first_node = LDNS_RBTREE_NULL;

    se_log_assert(list);
    se_log_assert(list->tasks);

    if (task) {
        se_log_debug("delete task from list");
        first_node = ldns_rbtree_delete(list->tasks, task);
        se_free((void*)first_node);
        se_log_debug("delete task from list: done");
        return task;
    } else {
        se_log_debug("delete task from list: null");
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

    now = time(NULL);
    pop = (task_type*) first_node->key;
    if (pop && (pop->flush || pop->when <= now)) {
        if (pop->flush) {
            se_log_debug("flush task '%s'", pop->who);
        } else {
            se_log_debug("pop task '%s'", pop->who);
        }
        first_node = ldns_rbtree_delete(list->tasks, pop);
        se_free((void*)first_node);
        pop->flush = 0;
        return pop;
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

    pop = (task_type*) first_node->key;
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
    se_log_debug("print task list");

    node = ldns_rbtree_first(list->tasks);
    while (node && node != LDNS_RBTREE_NULL) {
        task = (task_type*) node->key;
        task_print(out, task);
        node = ldns_rbtree_next(node);
    }
    fprintf(out, "\n");

    se_log_debug3("print task list: done");
}
