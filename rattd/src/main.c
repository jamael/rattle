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

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rattle/def.h>
#include <rattle/log.h>

#include "conf.h"
#include "dtor.h"
#include "log.h"
#include "module.h"
#include "proc.h"
#include "rattd.h"
#include "signal.h"

#ifdef WANT_TESTS
extern int test_main(int, char * const *);
#endif

#ifndef CONFFILEPATH
#define CONFFILEPATH	"/etc/rattle/rattd.conf"
#endif

#ifdef WANT_TESTS
#define ARGS_TESTS	0x1
#endif

/* program arguments */
static unsigned int l_args = 0;

/* program state */
typedef enum {
	MAIN_STATE_BOOT = 0,	/* program is booting */
	MAIN_STATE_RUN,		/* program is up */
	MAIN_STATE_STOP		/* program shutdown */
} main_state_t;

static main_state_t l_state = MAIN_STATE_BOOT;

/* configuration file path */
static char l_conffile[PATH_MAX] = { '\0' };

static inline void set_state(main_state_t state)
{
	main_state_t old;
	old = l_state;
	l_state = state;
	debug("program state changed from %u to %u", old, state);
}

static inline main_state_t get_state(void)
{
	return l_state;
}

static int parse_argv_opts(int argc, char * const argv[])
{
	int c;

	while ((c = getopt(argc, argv, "f:T")) != -1)
	{
		switch (c)
		{
		case 'f':	/* explicit config file */
			if (strlen(optarg) >= PATH_MAX) {
				error("%s: path is too long", optarg);
				return FAIL;
			} else {
				strcpy(l_conffile, optarg);
				l_conffile[PATH_MAX-1] = '\0';
			}
			break;
		case 'T':	/* test mode */
#ifdef WANT_TESTS
			l_args |= ARGS_TESTS;
			break;
#else
			error("-T given but test mode not compiled in.");
			return FAIL;
#endif
		}
	}

	return OK;
}

static int load_config()
{
	int retval;
	size_t n;
	if (*l_conffile == '\0') {
		if ((n = strlen(CONFFILEPATH)) >= PATH_MAX) {
			error("default configuration path is too long");
			debug("CONFFILEPATH exceeds PATH_MAX by %i byte(s)",
			    n - PATH_MAX + 1);
			return FAIL;
		}
		strcpy(l_conffile, CONFFILEPATH);
		l_conffile[PATH_MAX-1] = '\0';
	}
	notice("configuring from `%s'", l_conffile);
	retval = conf_open(l_conffile);
	return (retval == OK) ? OK : FAIL;
}

static void show_startup_notice()
{
	fprintf(stdout,
"RATTLE version %s\n\n"
"THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND\n"
"ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"
"IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"
"ARE DISCLAIMED.\n\n", VERSION);
}

static void on_interrupt(int signum, siginfo_t const *siginfo, void *udata)
{
	set_state(MAIN_STATE_STOP);
}

static void fini(void)
{
	RATTLOG_TRACE();
	dtor_callback();
	signal_unregister(SIGINT, &on_interrupt);
	signal_fini(NULL);
	debug("finished");
}

int main(int argc, char * const argv[])
{
	RATTLOG_TRACE();
	int retval;

	show_startup_notice();	

	/* parse command line arguments */
	if (parse_argv_opts(argc, argv) != OK) {
		debug("parse_argv_opts() failed");
		exit(1);
	}

#ifdef WANT_TESTS
	if (l_args & ARGS_TESTS) {
		/* entering tests mode */
		if (test_main(argc, argv) != OK) {
			exit(1);
		}
		exit(0);
	}
#endif

	/* handle signal */
	if (signal_init() != OK) {
		debug("signal_init() failed");
		exit(1);
	} else
		signal_register(SIGINT, &on_interrupt, NULL);

	/* initialize global destructor register */
	if (dtor_init() != OK) {
		debug("dtor_init() failed");
		signal_fini(NULL);
		exit(1);
	}

	/* register main destructor */
	retval = atexit(fini);
	if (retval != 0) {
		error("cannot register main destructor");
		debug("atexit() returns %i", retval);
		dtor_fini(NULL);
		signal_fini(NULL);
		exit(1);
	}

	/*
	 * From now on, finish functions (_fini)
	 * must register with dtor_register().
	 */

	/* load configuration file */
	if (conf_init() != OK || load_config() != OK) {
		debug("conf_init() or load_config() failed");
		exit(1);
	}

	/* initialize logger */
	if (log_init() != OK) {
		debug("log_init() failed");
		exit(1);
	}

	/* initialize modules */
	if (module_init() != OK) {
		debug("module_init() failed");
		exit(1);
	}

	/* set processor */
	if (proc_init() != OK) {
		debug("proc_init() failed");
		exit(1);
	} else if (proc_attach() != OK) {
		debug("proc_attach() failed");
		exit(1);
	}

	/* set logger */
	if (log_attach() != OK) {
		debug("log_attach() failed");
		exit(1);
	}

	if (rattd_init() != OK) {
		debug("rattd_init() failed");
		exit(1);
	}

	/* release config file */
	conf_close();

	/* start processor */
	if (proc_start() != OK) {
		debug("proc_start() failed");
		exit(1);
	}

	/* program is up */
	set_state(MAIN_STATE_RUN);

	/* wait for signals */
	do {
		signal_wait();

	} while (get_state() == MAIN_STATE_RUN);


	return 0;
}
