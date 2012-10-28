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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libconfig.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/test.h>

#include "conf.h"
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
#ifndef TEST_TESTABSIZ
#define TEST_TESTABSIZ	4
#endif
static RATT_TABLE_INIT(l_testab);	/* test entry table */

typedef struct {
	char const * const name;	/* test module name */
	char const * const desc;	/* test description */
	char const * const version;	/* test version */

	ratt_test_data_t data;		/* test data */
	ratt_test_hook_t *hook;		/* test hook */
} test_entry_t;

/* from ../tests/entry.c */
extern void tests_ar_register(char const *, void (*)(char const *));

/* program arguments */
static unsigned int l_args = 0;

/* program built-in configuration */
static char const *l_cfg_builtin =
	"logger: { verbose = \"" TEST_LOGLEVEL "\"; module = \"file\";"
		"file: { debug = \"" TEST_LOGFILE_DEBUG "\"; };"
	"};"
	"module: { search-path = \"/usr/local/lib/rattd/\";"
	"};"
	;

static ratt_module_parent_t l_parent_info = {
	RATT_TEST, RATT_TEST_VER_MAJOR, RATT_TEST_VER_MINOR,
};

static int attach_module(ratt_module_entry_t *entry)
{
	ratt_test_hook_t *hook = NULL;
	test_entry_t test = {
	    entry->name, entry->desc, entry->version
	};
	int retval;

	test.hook = entry->attach(&l_parent_info);
	if (!test.hook) {
		debug("`%s' chose not to attach", entry->name);
		return FAIL;
	} else if (test.hook->on_register)
		test.hook->on_register(&test.data);
	
	retval = ratt_table_insert(&l_testab, &test);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		if (test.hook->on_unregister)
			test.hook->on_unregister(test.data.udata);
		notice("failed loading `%s'", entry->name);
		return FAIL;
	}

	notice("%s (%s %s)", entry->desc, entry->name, entry->version);

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
	test_entry_t *test = NULL;
	clock_t start, end;
	int retval;

	RATT_TABLE_FOREACH(&l_testab, test)
	{
		notice("%s...", test->desc);

		if (!(test->hook->on_run)) {
			debug("on_run() callback is NULL (%s)", test->name);
			test->data.retval = FAIL;
			continue;
		}

		if ((start = clock()) == (clock_t) -1)
			test->data.cpu_time_used = -1;

		retval = test->hook->on_run(test->data.udata);

		if ((end = clock()) == (clock_t) -1)
			test->data.cpu_time_used = -1;

		test->data.retval = retval;

		if (!test->data.cpu_time_used)
			test->data.cpu_time_used =
			    ((double) (end - start)) / CLOCKS_PER_SEC;
	}
}

static void expect(void)
{
	test_entry_t *test = NULL;
	int retval;

	RATT_TABLE_FOREACH(&l_testab, test)
	{
		if (!(test->hook->on_expect)) {
			debug("on_expect() callback is NULL");
			test->data.expect = FAIL;
			continue;
		}

		retval = test->hook->on_expect(&(test->data));
		if (retval != OK) {
			test->data.expect = FAIL;
		} else {
			test->data.expect = OK;
		}
	}	
}

static void summary(void)
{
	test_entry_t *test = NULL;
	int retval;

	RATT_TABLE_FOREACH(&l_testab, test)
	{
		notice("%s (%s):", test->desc, test->name);
		if (!(test->hook->on_summary)) {
			debug("on_summary() callback is NULL for `%s'",
			    test->name);
			continue;
		}

		test->hook->on_summary(test->data.udata);

		if (test->data.cpu_time_used >= 0) {
			notice("cpu time: %.2f seconds", test->data.cpu_time_used);
		} else
			notice("cpu time: unknown");

		notice("result: %s", (test->data.expect == OK) ?
		    "ok" : "fail");
		notice("");
	}	
}

static void fini(void)
{
	RATTLOG_TRACE();
	dtor_callback();
	signal_fini(NULL);
	ratt_table_destroy(&l_testab);
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
	retval = ratt_table_create(&l_testab,
	    TEST_TESTABSIZ, sizeof(test_entry_t), 0);
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

	/* initialize logger */
	if (log_init() != OK) {
		error("log_init() failed");
		return FAIL;
	}

	/* initialize modules */
	if (module_init() != OK) {
		error("module_init() failed");
		return FAIL;
	}

	/* register test module parent */
	retval = module_parent_attach(&l_parent_info, &attach_module);
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
	if (log_attach() != OK) {
		error("log_attach() failed");
		return FAIL;
	}

	/* register test entries */
	notice("");
	notice("--- LOAD ---");
	notice("");
	tests_ar_register(NULL, &attach_test_name);

	/* run tests */
	notice("");
	notice("--- RUN ---");
	notice("");
	run();

	/* see what they expected */
	expect();

	/* print summary */
	notice("");
	notice("--- SUMMARY ---");
	notice("");
	summary();

	return OK;
}
