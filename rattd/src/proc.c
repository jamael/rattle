/*
 * RATTLE processor
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
#include <stdlib.h>

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/table.h>

#include "dtor.h"
#include "signal.h"

static enum {
	PROC_STATE_BOOT = 0,
	PROC_STATE_RUN,
	PROC_STATE_STOP
} l_state = PROC_STATE_BOOT;

typedef struct {
	void (*proc)(void *);		/* process pointer */
	void *udata;			/* process user data */
} proc_register_t;

/* processor table initial size */
#ifndef PROC_TABLESIZ
#define PROC_TABLESIZ	4
#endif
static RATT_TABLE_INIT(l_proctable);	/* processor table */

void proc_unregister(void (*proc)(void *udata))
{
	RATTLOG_TRACE();
	proc_register_t *reg = NULL;

	RATT_TABLE_FOREACH(&l_proctable, reg)
	{
		if (reg->proc == proc)
			reg->proc = reg->udata = NULL;
	}
}

int proc_register(void (*proc)(void *udata), void *udata)
{
	RATTLOG_TRACE();
	proc_register_t reg = { proc, udata };
	int retval;

	retval = ratt_table_push(&l_proctable, &reg);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		return FAIL;
	}

	debug("registered process %p, slot %i",
	    proc, ratt_table_pos_last(&l_proctable));
	return OK;
}

void proc_callback(void)
{
	RATTLOG_TRACE();
	proc_register_t *reg = NULL;

	RATT_TABLE_FOREACH_REVERSE(&l_proctable, reg)
	{
		if (reg->proc)
			reg->proc(reg->udata);
	}
}

void proc_start(void)
{
	RATTLOG_TRACE();

	if (l_state != PROC_STATE_RUN) {
		l_state = PROC_STATE_RUN;
	} else {
		debug("processor is already running");
		return;
	}

	do {
		proc_callback();

	} while (l_state == PROC_STATE_RUN);
}

void proc_interrupt(int signum, siginfo_t const *siginfo, void *udata)
{
	if (l_state != PROC_STATE_STOP)
		l_state = PROC_STATE_STOP;
}

void proc_fini(void *udata)
{
	RATTLOG_TRACE();
	ratt_table_destroy(&l_proctable);
}

int proc_init(void)
{
	RATTLOG_TRACE();
	int retval;
	
	retval = ratt_table_create(&l_proctable,
	    PROC_TABLESIZ, sizeof(proc_register_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	} else
		debug("allocated process table of size `%u'",
		    PROC_TABLESIZ);

	retval = signal_register(SIGINT, proc_interrupt, NULL);
	if (retval != OK) {
		debug("signal_register() failed");
		return FAIL;
	}

	retval = dtor_register(proc_fini, NULL);
	if (retval != OK) {
		debug("proc_register() failed");
		ratt_table_destroy(&l_proctable);
		return FAIL;
	}

	return OK;
}
