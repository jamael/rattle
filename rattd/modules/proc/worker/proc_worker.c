/*
 * RATTLE worker module processor
 * Copyright (c) 2012, Jamael Seun
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WANT_THREADS
#error "proc_worker implies WANT_THREADS"
#endif

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/proc.h>
#include <rattle/table.h>

#define MODULE_NAME	RATT_PROC "_worker"
#define MODULE_DESC	"worker processor"
#define MODULE_VERSION	"0.1"

#ifndef PROC_WORKER_MIN
#define PROC_WORKER_MIN		"2"	/* minimum threads */
#endif
static RATTCONF_DEFVAL(l_conf_worker_min_defval, PROC_WORKER_MIN);
static uint8_t l_conf_worker_min = 0;

#ifndef PROC_WORKER_MAX
#define PROC_WORKER_MAX		"25"	/* maximum threads */
#endif
static RATTCONF_DEFVAL(l_conf_worker_max_defval, PROC_WORKER_MAX);
static uint8_t l_conf_worker_max = 0;

static ratt_conf_t l_conf[] = {
	{ "worker/min-workers", "minimum number of threads (workers)",
	    l_conf_worker_min_defval, &l_conf_worker_min,
	    RATTCONFDTNUM8, RATTCONFFLUNS },
	{ "worker/max-workers", "maximum number of threads (workers)",
	    l_conf_worker_max_defval, &l_conf_worker_max,
	    RATTCONFDTNUM8, RATTCONFFLUNS },
	{ NULL }
};

/* worker flags */
#define PROC_WORKER_FLMEM	0x1	/* memory holder */

/* worker state */
typedef enum {
	PROC_WORKER_STATE_BOOT = 0,	/* processor has not run yet */
	PROC_WORKER_STATE_IDLE,		/* processor on pause */
	PROC_WORKER_STATE_RUN,		/* processor running */
	PROC_WORKER_STATE_STOP		/* processor stopped */
} proc_worker_state_t;

static proc_worker_state_t l_proc_worker_state = PROC_WORKER_STATE_BOOT;

/* worker table initial size */
#ifndef PROC_WORKER_WORKTABSIZ
#define PROC_WORKER_WORKTABSIZ		4
#endif
static RATT_TABLE_INIT(l_worktab);	/* worker table */
static pthread_mutex_t l_worktab_lock = PTHREAD_MUTEX_INITIALIZER;

/* process table initial size */
#ifndef PROC_WORKER_PROCTABSIZ
#define PROC_WORKER_PROCTABSIZ		4
#endif

typedef struct {
	pthread_t id;			/* worker id */
	pthread_attr_t attr;		/* worker attributes */
	pthread_cond_t get_to_work;	/* worker has to work */
	pthread_mutex_t lock;		/* worker lock */
	proc_worker_state_t state;	/* worker state */
	uint32_t flags;			/* worker flags */

	ratt_table_t proctab;		/* process table */
	pthread_mutex_t proctab_lock;	/* process table lock */

	/* not locked */

	sigset_t sigblockmask;		/* blocked signals */
	int retval;			/* worker return value */
} worker_register_t;

typedef struct {
	int (*process)(void *);		/* process pointer */
	ratt_proc_attr_t *attr;		/* process attributes */
	void *udata;			/* process user data */
	unsigned int failure;		/* process failure count */
} proc_register_t;

static int check_config()
{
	if (l_conf_worker_min <= 0) {
		error("proc_worker: min-workers of `%i' is not ok",
		    l_conf_worker_min);
		return FAIL;
	} else if (l_conf_worker_min > l_conf_worker_max) {
		error("proc_worker: min-workers greater than max-workers");
		return FAIL;
	}

	return OK;
}

static inline void set_state(proc_worker_state_t state)
{
	proc_worker_state_t old;
	old = l_proc_worker_state;
	l_proc_worker_state = state;
	debug("proc_worker state changed from %u to %u",
	    old, state);
}

static inline void
worker_set_state(worker_register_t *worker, proc_worker_state_t state)
{
	proc_worker_state_t old;
	old = worker->state;
	worker->state = state;
	debug("worker (%p) state changed from %u to %u",
	    worker, old, worker->state);
}

static void worker_cleanup(void *udata)
{
	worker_register_t *worker = udata;

	pthread_attr_destroy(&(worker->attr));
	pthread_cond_destroy(&(worker->get_to_work));
	pthread_mutex_destroy(&(worker->lock));
	pthread_mutex_destroy(&(worker->proctab_lock));
}

static void worker_cleanup_mutex_unlock(void *udata)
{
	pthread_mutex_t *mutex = udata;
	pthread_mutex_unlock(mutex);
}

static void *worker_loop(void *udata)
{
	worker_register_t *self = udata;
	proc_register_t *proc = NULL;
	int retval;

	pthread_sigmask(SIG_BLOCK, &(self->sigblockmask), NULL);
	pthread_cleanup_push(&worker_cleanup, self);

	do {
		pthread_cleanup_push(&worker_cleanup_mutex_unlock,
		    &(self->lock));
		pthread_mutex_lock(&(self->lock));
		while (self->state != PROC_WORKER_STATE_RUN)
			pthread_cond_wait(&(self->get_to_work), &(self->lock));

		/* worker_cleanup_mutex_unlock (self) */
		pthread_cleanup_pop(0);	/* do not execute */

		pthread_cleanup_push(&worker_cleanup_mutex_unlock,
		    &(self->proctab_lock));
		pthread_mutex_lock(&(self->proctab_lock));

		proc = ratt_table_get_circular_next(&(self->proctab));

		/* worker_cleanup_mutex_unlock (proctab) */
		pthread_cleanup_pop(1);

		if (!proc) { /* nothing to process */
			worker_set_state(self, PROC_WORKER_STATE_IDLE);
			pthread_mutex_unlock(&(self->lock));
			continue;
		} else
			pthread_mutex_unlock(&(self->lock));

		/* We should not hold any worker's locks as the
		 * process we shall execute might need to lock again; i.e.
		 * it may end up calling on_register, on_unregister where
		 * those locks are needed.
		 *
		 * Note that recursive calls to on_register() will
		 * exhaust the thread stack as would any endless recursivity.
		 */

		if (!proc->process) {
			debug("ghost process; should not happen");
		} else if (proc->attr && proc->attr->flags & RATTPROCFLNTS) {
			/* process is not thread-safe */
			pthread_cleanup_push(&worker_cleanup_mutex_unlock,
			    proc->attr->lock);
			pthread_mutex_lock(proc->attr->lock);
			retval = proc->process(proc->udata);
			/* worker_cleanup_mutex_unlock (process) */
			pthread_cleanup_pop(1);
		} else
			retval = proc->process(proc->udata);
	} while (1);

	/* NOTREACHED */
	pthread_cleanup_pop(1);	/* worker_cleanup (self) */
	pthread_exit(NULL);
}

static void worker_destroy(worker_register_t *worker)
{
	/* worker_cleanup should have been called already */
	ratt_table_destroy(&(worker->proctab));
}

static void worker_flush(void)
{
	worker_register_t **worker = NULL;

	RATT_TABLE_FOREACH_REVERSE(&l_worktab, worker)
	{
		worker_destroy(*worker);
		if ((*worker)->flags & PROC_WORKER_FLMEM) {/* memory holder */
			debug("found memory holder at %p", *worker);
			free(*worker);
		}
	}
}

static int worker_create(unsigned int wanted)
{
	worker_register_t *worker = NULL;
	size_t worker_now = 0;
	unsigned int i;
	int retval;

	worker_now = ratt_table_count(&l_worktab);
	if ((worker_now + wanted) > l_conf_worker_max) {
		debug("maximum of `%u' workers reached", l_conf_worker_max);
		wanted = l_conf_worker_max - worker_now;
	}

	if (wanted)
		worker = calloc(wanted, sizeof(worker_register_t));

	if (!worker) {
		debug("either wanted is 0 or calloc() failed");
		return FAIL;
	}

	/* first worker is the memory holder */
	worker->flags |= PROC_WORKER_FLMEM;

	for (i = 0; i < wanted; ++i, ++worker) {

		/* individual process table, destroyed via worker_destroy() */
		retval = ratt_table_create(&(worker->proctab),
		    PROC_WORKER_PROCTABSIZ, sizeof(proc_register_t), 0);
		if (retval != OK) {
			debug("ratt_table_create() failed");
			break;
		}

		/* all signals blocked, set later with pthread_sigmask */
		sigfillset(&(worker->sigblockmask));

		/* initializer, destroyed via worker_cleanup() */
		pthread_attr_init(&(worker->attr));
		pthread_cond_init(&(worker->get_to_work), NULL);
		pthread_mutex_init(&(worker->lock), NULL);
		pthread_mutex_init(&(worker->proctab_lock), NULL);

		/* detached */
		pthread_attr_setdetachstate(&(worker->attr),
		    PTHREAD_CREATE_DETACHED);

		/* stopped */
		worker_set_state(worker, PROC_WORKER_STATE_STOP);

		retval = pthread_create(&(worker->id),
		    &(worker->attr), worker_loop, worker);
		if (retval) {
			debug("pthread_create() failed");
			ratt_table_destroy(&(worker->proctab));
			break;
		}

		retval = ratt_table_push(&l_worktab, &worker);
		if (retval != OK) {
			debug("ratt_table_push() failed");
			/* cancel worker */
			pthread_cancel(worker->id);
			break;
		}
	}

	debug("created %u workers, wanted %u", i, wanted);
	return (i == wanted) ? OK : FAIL;
}

static int compare_process(void const *in, void const *find)
{
	proc_register_t const *entry = in;
	proc_register_t const *proc = find;
	int retval;

	if (proc->process != entry->process) {
		/* not the same process */
		return FAIL;
	}

	if (proc->udata && (proc->udata != entry->udata)) {
		/* process to find has user data,
		 * entry might have but did not match */
		return FAIL;
	} else if (entry->udata) {
		/* process to find has no user data, entry has */
		return FAIL;
	}

	if (proc->attr) {
		/* process to find has attributes */
		if (entry->attr) {
			retval = memcmp(proc->attr,
			    entry->attr, sizeof(ratt_proc_attr_t));
			if (retval != 0) {
				/* attributes did not match */
				return FAIL;
			}
		} else	/* entry has no attribute */
			return FAIL;
	} else if (entry->attr) {
		/* process to find has no attribute, entry has */
		return FAIL;
	}

	return OK;
}

static void
on_unregister(int (*process)(void *), ratt_proc_attr_t *attr, void *udata)
{
	worker_register_t **worker = NULL;
	proc_register_t *entry = NULL, proc = { process, attr, udata };
	int retval;

	pthread_cleanup_push(&worker_cleanup_mutex_unlock, &l_worktab_lock);
	pthread_mutex_lock(&l_worktab_lock);

	RATT_TABLE_FOREACH(&l_worktab, worker)
	{
		pthread_cleanup_push(&worker_cleanup_mutex_unlock,
		    &((*worker)->proctab_lock));
		pthread_mutex_lock(&((*worker)->proctab_lock));
		retval = ratt_table_search(&((*worker)->proctab),
		    (void **) &entry, &compare_process, &proc);
		if (retval == OK) {
			ratt_table_del_current(&((*worker)->proctab));
		}

		/* worker_cleanup_mutex_unlock (proctab) */
		pthread_cleanup_pop(1);

		if (retval == OK)
			break;
	}

	/* worker_cleanup_mutex_unlock (worktab) */
	pthread_cleanup_pop(1);
}

static int
on_register(int (*process)(void *), ratt_proc_attr_t *attr, void *udata)
{
	worker_register_t **worker = NULL;
	proc_register_t proc = { process, attr, udata };
	static size_t rrpos = 0;
	int retval;

	/* round-robin selection of workers */
	pthread_cleanup_push(&worker_cleanup_mutex_unlock, &l_worktab_lock);
	pthread_mutex_lock(&l_worktab_lock);
	if (rrpos > ratt_table_pos_last(&l_worktab)) {
		worker = ratt_table_get_first(&l_worktab);
		rrpos = 0;
	} else
		worker = ratt_table_get(&l_worktab, rrpos);

	rrpos++;
	/* worker_cleanup_mutex_unlock (worktab) */
	pthread_cleanup_pop(1);

	if (!worker) {
		debug("round-robin selection failed");
		return FAIL;
	} else if (!*worker) {
		debug("worker is gone; should not happen");
		return FAIL;
	}

	pthread_mutex_lock(&((*worker)->lock));
	pthread_mutex_lock(&((*worker)->proctab_lock));

	retval = ratt_table_insert(&((*worker)->proctab), &proc);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		pthread_mutex_unlock(&((*worker)->proctab_lock));
		pthread_mutex_unlock(&((*worker)->lock));
		return FAIL;
	}

	if ((*worker)->state == PROC_WORKER_STATE_IDLE) {
		worker_set_state(*worker, PROC_WORKER_STATE_RUN);
		pthread_cond_signal(&((*worker)->get_to_work));
	}

	pthread_mutex_unlock(&((*worker)->proctab_lock));
	pthread_mutex_unlock(&((*worker)->lock));

	debug("registered process %p, slot %i on worker (%p)",
	    process, ratt_table_pos_last(&((*worker)->proctab)), *worker);

	return OK;
}

static int on_start(void)
{
	worker_register_t **worker = NULL;

	if (l_proc_worker_state == PROC_WORKER_STATE_RUN) {
		debug("proc_worker is running already");
		return FAIL;
	} else
		set_state(PROC_WORKER_STATE_RUN);

	RATT_TABLE_FOREACH(&l_worktab, worker)
	{
		pthread_mutex_lock(&((*worker)->lock));
		worker_set_state(*worker, PROC_WORKER_STATE_RUN);
		pthread_cond_signal(&((*worker)->get_to_work));
		pthread_mutex_unlock(&((*worker)->lock));
	}

	return OK;
}

static int on_stop(void)
{
	worker_register_t **worker = NULL;

	if (l_proc_worker_state == PROC_WORKER_STATE_STOP) {
		debug("proc_worker is not running");
		return FAIL;
	} else
		set_state(PROC_WORKER_STATE_STOP);

	RATT_TABLE_FOREACH(&l_worktab, worker)
	{
		if (*worker)
			pthread_cancel((*worker)->id);

		/* workers are removed from worktab later */
	}

	return OK;
}

static void on_interrupt(int signum, siginfo_t const *siginfo, void *udata)
{
	worker_register_t **worker = NULL;

	/* stop workers on interrupt */
	on_stop();
}

static ratt_proc_hook_t proc_worker_hook = {
	.on_start = &on_start,
	.on_stop = &on_stop,
	.on_interrupt = &on_interrupt,
	.on_unregister = &on_unregister,
	.on_register = &on_register,
};

void *__proc_worker_attach(ratt_module_parent_t const *parinfo)
{
	int retval;

	if (check_config() != OK)
		return NULL;

	/* start initial workers */
	retval = worker_create(l_conf_worker_min);
	if (retval != OK) {
		debug("worker_create() failed");
		return NULL;
	}

	return &proc_worker_hook;
}

void __proc_worker_fini(void)
{
	RATTLOG_TRACE();
	worker_flush();
	ratt_table_destroy(&l_worktab);
	pthread_mutex_destroy(&l_worktab_lock);
}

int __proc_worker_init(void)
{
	RATTLOG_TRACE();
	int retval;
	
	retval = ratt_table_create(&l_worktab, PROC_WORKER_WORKTABSIZ,
	    sizeof(worker_register_t *), RATTTABFLNRU);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	} else
		debug("allocated worker table of size `%u'",
		    PROC_WORKER_WORKTABSIZ);

	return OK;
}

static ratt_module_entry_t module_entry = {
	.name = MODULE_NAME,
	.desc = MODULE_DESC,
	.version = MODULE_VERSION,
	.attach = &__proc_worker_attach,
	.constructor = &__proc_worker_init,
	.destructor = &__proc_worker_fini,
	.config = l_conf,
};

void __attribute__ ((constructor)) __proc_worker(void)
{
	ratt_module_register(&module_entry);
}
