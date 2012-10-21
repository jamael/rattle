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


#include <config.h>

#include <signal.h>
#include <stdint.h>

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/proc.h>
#include <rattle/table.h>

#define MODULE_NAME	RATTPROC "_serial"
#define MODULE_DESC	"serial processor"
#define MODULE_VERSION	"0.1"

static enum {
	PROC_STATE_BOOT = 0,	/* processor has not run yet */
	PROC_STATE_RUN,		/* processor is running */
	PROC_STATE_STOP		/* processor stopped */
} l_proc_state = PROC_STATE_BOOT;

typedef struct {
	int (*process)(void *);		/* process pointer */
	uint32_t flags;			/* process flags */
	void *udata;			/* process user data */
} proc_register_t;

/* process table initial size */
#ifndef PROC_TABLESIZ
#define PROC_TABLESIZ	4
#endif
static RATT_TABLE_INIT(l_proctable);	/* process table */

static int compare_process(void const *in, void const *find)
{
	proc_register_t const *proc = in;
	int (*process)(void *) = find;
	return (proc->process == process) ? OK : FAIL;
}

static void on_unregister(int (*process)(void *))
{
	proc_register_t *proc = NULL;
	int retval;

	retval = ratt_table_search(&l_proctable, (void **) &proc,
	    &compare_process, process);
	if (retval != OK) {
		debug("no matching process found");
	} else
		ratt_table_del_current(&l_proctable);
}

static int on_register(int (*process)(void *), uint32_t flags, void *udata)
{
	proc_register_t proc = { process, flags, udata };
	int retval;

	retval = ratt_table_push(&l_proctable, &proc);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		return FAIL;
	}

	debug("registered process %p, slot %i",
	    process, ratt_table_pos_last(&l_proctable));

	return OK;
}

static int on_start(void)
{
	proc_register_t *proc = NULL;

	if (l_proc_state == PROC_STATE_RUN) {
		debug("processor is running already");
		return FAIL;
	}

	l_proc_state = PROC_STATE_RUN;

	do {
		RATT_TABLE_FOREACH(&l_proctable, proc)
		{
			if (proc->process)
				proc->process(proc->udata);
		}

	} while (l_proc_state == PROC_STATE_RUN);

	return OK;
}

static int on_stop(void)
{
	if (l_proc_state != PROC_STATE_STOP) {
		l_proc_state = PROC_STATE_STOP;
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

static rattproc_callback_t callbacks = {
	.on_start = &on_start,
	.on_stop = &on_stop,
	.on_interrupt = &on_interrupt,
	.on_unregister = &on_unregister,
	.on_register = &on_register,
};

static rattmod_entry_t module_entry = {
	.name = MODULE_NAME,
	.desc = MODULE_DESC,
	.version = MODULE_VERSION,
	.callbacks = &callbacks,
};

void __attribute__ ((destructor)) proc_serial_fini(void)
{
	RATTLOG_TRACE();
	ratt_table_destroy(&l_proctable);
	rattmod_unregister(&module_entry);
}

void __attribute__ ((constructor)) proc_serial_init(void)
{
	RATTLOG_TRACE();
	int retval;
	
	retval = ratt_table_create(&l_proctable,
	    PROC_TABLESIZ, sizeof(proc_register_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return;
	} else
		debug("allocated process table of size `%u'",
		    PROC_TABLESIZ);

	retval = rattmod_register(&module_entry);
	if (retval != OK) {
		debug("rattmod_register() failed");
		ratt_table_destroy(&l_proctable);
	}
}
