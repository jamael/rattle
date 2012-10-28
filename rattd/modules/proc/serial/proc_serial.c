/*
 * RATTLE serial module processor
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

#include <signal.h>
#include <stdint.h>

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/proc.h>
#include <rattle/table.h>

#define MODULE_NAME	RATT_PROC "_serial"
#define MODULE_DESC	"serial processor"
#define MODULE_VERSION	"0.1"

static enum {
	PROC_SERIAL_STATE_BOOT = 0,	/* processor has not run yet */
	PROC_SERIAL_STATE_RUN,		/* processor is running */
	PROC_SERIAL_STATE_STOP		/* processor stopped */
} l_proc_state = PROC_SERIAL_STATE_BOOT;

typedef struct {
	int (*process)(void *);		/* process pointer */
	ratt_proc_attr_t *attr;		/* process attributes */
	void *udata;			/* process user data */
} proc_serial_register_t;

/* process table initial size */
#ifndef PROC_PROCTABSIZ
#define PROC_PROCTABSIZ		4
#endif
static RATT_TABLE_INIT(l_proctab);	/* process table */

static int compare_process(void const *in, void const *find)
{
	proc_serial_register_t const *entry = in;
	proc_serial_register_t const *proc = find;
	int retval;

	if (proc->udata) {
		if (proc->udata != entry->udata)
			return FAIL;
	}

	if (proc->attr && entry->attr) {
		retval = memcmp(proc->attr,
		    entry->attr, sizeof(ratt_proc_attr_t));
		if (retval != 0)
			return FAIL;
	}

	return (proc->process == entry->process) ? OK : FAIL;
}

static void
on_unregister(int (*process)(void *), ratt_proc_attr_t *attr, void *udata)
{
	proc_serial_register_t *entry = NULL, proc = { process, attr, udata };
	int retval;

	retval = ratt_table_search(&l_proctab, (void **) &entry,
	    &compare_process, &proc);
	if (retval != OK) {
		debug("no matching process found");
	} else
		ratt_table_del_current(&l_proctab);
}

static int
on_register(int (*process)(void *), ratt_proc_attr_t *attr, void *udata)
{
	proc_serial_register_t proc = { process, attr, udata };
	int retval;

	retval = ratt_table_insert(&l_proctab, &proc);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		return FAIL;
	}

	debug("registered process %p, slot %i",
	    process, ratt_table_pos_last(&l_proctab));

	return OK;
}

static int on_start(void)
{
	proc_serial_register_t *proc = NULL;
	int retval;

	if (l_proc_state == PROC_SERIAL_STATE_RUN) {
		debug("processor is running already");
		return FAIL;
	}

	l_proc_state = PROC_SERIAL_STATE_RUN;

	do {
		RATT_TABLE_FOREACH(&l_proctab, proc)
		{
			if (proc->process) {
				retval = proc->process(proc->udata);
				if (!(proc->attr->flags & RATTPROCFLSTC)) {
					ratt_table_del_current(&l_proctab);
					continue;
				} else if (retval != OK) {
					debug("process at %p failed",
					    proc->process);
					/* increase failure count;
					 * check for max sticky failure conf;
					 * trash if exceed.
					 * proc->failure++; */
				}
			} else {	/* trash ghost process */
				debug("ghost process registered on slot %u",
				    ratt_table_pos_current(&l_proctab));
				ratt_table_del_current(&l_proctab);
			}
		}

	} while (l_proc_state == PROC_SERIAL_STATE_RUN);

	return OK;
}

static int on_stop(void)
{
	if (l_proc_state != PROC_SERIAL_STATE_STOP) {
		l_proc_state = PROC_SERIAL_STATE_STOP;
		return OK;
	}

	debug("processor is not running");
	return FAIL;
}

static void on_interrupt(int signum, siginfo_t const *siginfo, void *udata)
{
	/* stop processor on interrupt */
	on_stop();
}

static ratt_proc_hook_t proc_serial_hook = {
	.on_start = &on_start,
	.on_stop = &on_stop,
	.on_interrupt = &on_interrupt,
	.on_unregister = &on_unregister,
	.on_register = &on_register,
};

static void *__proc_serial_attach(ratt_module_parent_t const *parinfo)
{
	return &proc_serial_hook;
}

static void __proc_serial_fini(void)
{
	RATTLOG_TRACE();
	ratt_table_destroy(&l_proctab);
}

static int __proc_serial_init(void)
{
	RATTLOG_TRACE();
	int retval;
	
	retval = ratt_table_create(&l_proctab,
	    PROC_PROCTABSIZ, sizeof(proc_serial_register_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	} else
		debug("allocated process table of size `%u'",
		    PROC_PROCTABSIZ);

	return OK;
}

static ratt_module_entry_t module_entry = {
	.name = MODULE_NAME,
	.desc = MODULE_DESC,
	.version = MODULE_VERSION,
	.attach = &__proc_serial_attach,
	.constructor = &__proc_serial_init,
	.destructor = &__proc_serial_fini,
};

void __attribute__ ((constructor)) proc_serial(void)
{
	RATTLOG_TRACE();
	ratt_module_register(&module_entry);
}
