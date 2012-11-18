/*
 * RATTLE main entry
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

#include <stdio.h>
#include <stdlib.h>

#include <rattle/args.h>
#include <rattle/def.h>
#include <rattle/log.h>

#include "args.h"
#include "conf.h"
#include "dtor.h"
#include "log.h"
#include "module.h"
#include "proc.h"
#include "rattd.h"
#include "signal.h"
#include "test.h"

/* globals */
char * const *g_main_argv = NULL;
int g_main_argc = 0;

/* program arguments */

#define MAIN_ARGSSEC_ID	ARGSSECMAIN

static int l_args_show_usage = 0;
static int l_args_show_version = 0;
static ratt_args_t l_args[] = {
	{ 'h', NULL, "show program usage",
	    &l_args_show_usage, NULL, 0 },
	{ 'v', NULL, "show program version",
	    &l_args_show_version, NULL, 0 },
	{ 0 }
};

/* program state */
typedef enum {
	MAIN_STATE_BOOT = 0,	/* program is booting */
	MAIN_STATE_RUN,		/* program is up */
	MAIN_STATE_STOP		/* program shutdown */
} main_state_t;

static main_state_t l_main_state = MAIN_STATE_BOOT;

/* core components */
static void main_fini(void *);
static int main_init(void);
static struct main_core {
	int (*init)(void);		/* component init function */
	void (*fini)(void *);		/* component fini function */
} l_main_coretab[] = {
	{ signal_init, signal_fini },	/* signals handler */
	{ args_init, args_fini },	/* arguments handler */
	{ main_init, main_fini },	/* main program */
	{ conf_init, conf_fini },	/* configure */
	{ log_init, log_fini },		/* logger parent */
	{ module_init, module_fini },	/* modules loader */
	{ log_attach, log_detach },	/* logger modules */
	{ proc_init, proc_fini },	/* processor parent */
	{ proc_attach, proc_detach },	/* processor modules */
	{ NULL }
};

static void initialize_core_components(void)
{
	struct main_core *core = l_main_coretab;
	int retval;

	for (; core && core->init; core++) {
		debug("---");
		switch (core->init()) {
		case OK:
			retval = dtor_register(core->fini, NULL);
			if (retval != OK) {
				debug("dtor_register() failed");
				if (core->fini)
					core->fini(NULL);
				exit(EXIT_FAILURE);
			}
			break;
		case STOP:
			exit(EXIT_SUCCESS);
		default:
		case FAIL:
			exit(EXIT_FAILURE);
		}
	}
}

static void initialize_global_destructor(void)
{
	if (dtor_init() != OK) {
		debug("dtor_init() failed");
	} else if (atexit(dtor_callback)) {
		debug("atexit() failed");
		dtor_fini(NULL);
	} else		/* global destructor ok */
		return;

	error("destructor failed to start");
	exit(EXIT_FAILURE);
}

static inline main_state_t get_state(void)
{
	return l_main_state;
}

static void set_state(main_state_t state)
{
	main_state_t old;
	old = l_main_state;
	l_main_state = state;
	debug("program state changed from %u to %u", old, state);
}

static void show_startup_notice()
{
	fprintf(stdout,
"RATTLE version %s - Copyright (c) 2012, Jamael Seun\n\n"
"THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND\n"
"ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
"IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"
"ARE DISCLAIMED.\n\n", VERSION);
}

static void on_interrupt(int signum, siginfo_t const *siginfo, void *udata)
{
	set_state(MAIN_STATE_STOP);
	proc_stop();
}

static void main_fini(void *udata)
{
	RATTLOG_TRACE();
	signal_unregister(SIGINT, &on_interrupt);
}

static int main_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = args_register(MAIN_ARGSSEC_ID, NULL, l_args);
	if (retval != OK) {
		debug("args_register() failed");
		return FAIL;
	}

	if (l_args_show_version)
		return STOP;

	retval = signal_register(SIGINT, &on_interrupt, NULL);
	if (retval != OK) {
		debug("signal_register() failed");
		return FAIL;
	}

	return OK;
}

int main(int argc, char * const argv[])
{
	RATTLOG_TRACE();

	show_startup_notice();

	/* global destructor registry */
	initialize_global_destructor();

	/* export program arguments */
	g_main_argc = argc;
	g_main_argv = argv;

	/* set core components up */
	initialize_core_components();

	/* if help requested, do that now */
	if (l_args_show_usage) {
		args_show();
		return EXIT_SUCCESS;
	}

	/* start processor */
	if (proc_start() != OK) {
		debug("proc_start() failed");
		return EXIT_FAILURE;
	}

	/* program is up, wait for signals */
	set_state(MAIN_STATE_RUN);
	while (get_state() == MAIN_STATE_RUN)
		signal_wait();

	/* program shutdown */

	return EXIT_SUCCESS;
}
