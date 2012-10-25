/*
 * RATTLE test mode main entry
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

#include <dlfcn.h>
#include <libconfig.h>
#include <stdlib.h>
#include <unistd.h>

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/test.h>

#include "dtor.h"
#include "log.h"
#include "module.h"
#include "proc.h"
#include "signal.h"

/* logger's verbose setting */
#ifdef DEBUG
#define TEST_LOGLEVEL	"debug"
#else
#define TEST_LOGLEVEL	"notice"
#endif

#ifndef TEST_LOGFILE_DEBUG
#define TEST_LOGFILE_DEBUG	"rattle_test_run.log"
#endif

/* test entry table initial size */
#ifndef TEST_TABLESIZ
#define TEST_TABLESIZ	4
#endif
static RATT_TABLE_INIT(l_testtable);	/* test entry table */

typedef struct {

	char const * const name;
	char const * const desc;
	char const * const version;

	ratt_test_data_t test;
	ratt_test_callback_t *callbacks;
} test_entry_t;

/* from ../tests/entry.c */
extern void tests_ar_register(char const *, void (*)(char const *));

/* program arguments */
static unsigned int l_args = 0;

/* program built-in configuration */
static char const *l_cfg_builtin =
	"logger: { verbose = \"" TEST_LOGLEVEL "\"; module = \"file\";"
		"file: { debug = \"" TEST_LOGFILE_DEBUG "\"; }"
	"};"
	;

static int attach_module(rattmod_entry_t *modentry)
{
	test_entry_t entry = {
	    modentry->name, modentry->desc, modentry->version
	};
	ratt_test_callback_t *callbacks = NULL;
	int retval;

	if (modentry && modentry->callbacks) {
		entry.callbacks = modentry->callbacks;
		entry.callbacks->on_register(&entry.test);
	} else {
		debug("reject modentry without callbacks");
		return FAIL;
	}
	
	retval = ratt_table_push(&l_testtable, &entry);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		entry.callbacks->on_unregister(entry.test.udata);
		return FAIL;
	}

	notice("`%s' (%s) registered", modentry->desc, modentry->name);

	return OK;
}

static void attach_test_name(char const *name)
{
	int retval;

	retval = module_attach(RATT_TEST, name);
	if (retval != OK)
		error("module_attach() failed on %s", name);
}

static int parse_argv_opts(int argc, char * const argv[])
{
	int c;

	while ((c = getopt(argc, argv, "T:")) != -1)
	{
		switch (c)
		{
			/* empty */
		}
	}

	return OK;
}

static void run(void)
{
	test_entry_t *entry = NULL;
	int retval;

	RATT_TABLE_FOREACH(&l_testtable, entry)
	{
		if (!(entry->callbacks->on_run)) {
			debug("on_run() callback is NULL (%s)", entry->name);
			entry->test.retval = FAIL;
			continue;
		}

		retval = entry->callbacks->on_run(entry->test.udata);
		entry->test.retval = retval;
	}
}

static void expect(void)
{
	test_entry_t *entry = NULL;
	int retval;

	RATT_TABLE_FOREACH(&l_testtable, entry)
	{
		if (!(entry->callbacks->on_expect)) {
			debug("on_expect() callback is NULL");
			notice("%s... fail", entry->desc);
			continue;
		}

		retval = entry->callbacks->on_expect(entry->test.udata);
		if (retval != OK) {
			notice("%s... fail", entry->desc);
		} else
			notice("%s... ok", entry->desc);
	}	
}

static void summary(void)
{
}

static void fini(void)
{
	RATTLOG_TRACE();

	/* callback registered destructor */
	dtor_callback();
	signal_fini(NULL);
	ratt_table_destroy(&l_testtable);
	debug("finished");
}

int test_main(int argc, char * const argv[])
{
	RATTLOG_TRACE();
	int retval;

	/* parse command line arguments */
	if (parse_argv_opts(argc, argv) != OK) {
		debug("parse_argv_opts() failed");
		return FAIL;
	}

	/* handle signal */
	if (signal_init() != OK) {
		debug("signal_init() failed");
		return FAIL;
	}

	/* initialize global destructor register */
	if (dtor_init() != OK) {
		debug("dtor_init() failed");
		signal_fini(NULL);
		return FAIL;
	}

	/* register main destructor */
	retval = atexit(fini);
	if (retval != 0) {
		error("cannot register main destructor");
		debug("atexit() returns %i", retval);
		dtor_fini(NULL);
		signal_fini(NULL);
		return FAIL;
	}

	/* initialize test entry table */
	retval = ratt_table_create(&l_testtable,
	    TEST_TABLESIZ, sizeof(test_entry_t), 0);
	if (retval != OK) {
		error("ratt_table_create() failed");
		return FAIL;
	}

	/* load built-in configuration */
	if (conf_init() != OK) {
		error("conf_init() failed");
		return FAIL;
	} else if (conf_open_builtin(l_cfg_builtin) != OK) {
		error("conf_open_builtin() failed");
		return FAIL;
	}

	/* initialize modules */
	if (module_init() != OK) {
		error("module_init() failed");
		return FAIL;
	}

	/* register test module parent */
	retval = module_parent_attach(RATT_TEST, &attach_module);
	if (retval != OK) {
		error("module_parent_attach() failed");
		return FAIL;
	}

	/* set processor */
	if (proc_init() != OK) {
		error("proc_init() failed");
		return FAIL;
	}

	/* set logger */
	if (log_init() != OK) {
		error("log_init() failed");
		return FAIL;
	}

	/* register test entries */
	tests_ar_register(NULL, &attach_test_name);

	/* run tests */
	run();

	/* see what they expected */
	expect();

	/* print summary */
	summary();

	return OK;
}
