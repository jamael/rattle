/*
 * RATTLE logger
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


#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/table.h>

#include "conf.h"
#include "dtor.h"
#include "module.h"

#ifndef LOG_CALLTABLESIZ
#define LOG_CALLTABLESIZ	4	/* callbacks table size */
#endif
static RATT_TABLE_INIT(l_calltable);	/* callbacks table */

/*
 * standard logger module (should have its own file)
 */
#define MODULE_NAME	RATTLOG "_std"
#define MODULE_DESC	"Standard logger"
#define MODULE_VERSION	VERSION

static void log_std_msg(int, char const *);
static rattlog_callback_t log_std_callbacks = {
	.on_msg = &log_std_msg
};

static rattmod_entry_t log_std_entry = {
	.name = MODULE_NAME,
	.desc = MODULE_DESC,
	.version = MODULE_VERSION,
	.callbacks = &log_std_callbacks
};

#define TIMESTRSIZ 21	/* %d %b %Y %T + \0 */
static void log_std_msg(int level, char const *msg)
{
	FILE *out = NULL;
	char timestr[TIMESTRSIZ] = { '\0' };
	struct tm tmres;
	time_t now;

	out = (level > RATTLOGERR) ? stdout : stderr;

	/* datetime prefix */
	if (((now = time(NULL)) != (time_t) -1)
	    && (localtime_r(&now, &tmres) != NULL)
	    && (strftime(timestr, TIMESTRSIZ, "%d %b %Y %T", &tmres) > 0))
		fprintf(out, "[%s] ", timestr);
	/* level name prefix */
	fprintf(out, "%s: ", rattlog_level_to_name(level));
	/* message */
	fprintf(out, "%s", msg);
}

/*
 * end of standard logger module
 */

/* logger state */
static enum {
	LOG_BOOT = 0,		/* bootstraping */
	LOG_INIT,		/* initialized */
	LOG_FINI		/* finishing */
} l_state = LOG_BOOT;

/* logger verbosity level */
static int l_verbose = RATTLOGMAX;

#ifndef LOG_MODULE
#define LOG_MODULE "std"
#define LOG_MODULE_COUNT 1
#elif !defined LOG_MODULE_COUNT
#error "LOG_MODULE requires LOG_MODULE_COUNT"
#endif
static char **l_conf_module_lst = NULL;
static size_t l_conf_module_cnt = 0;

#ifndef LOG_VERBOSE
#define LOG_VERBOSE "notice"
#endif
static char *l_conf_verbose = NULL;

static conf_decl_t l_conftable[] = {
	{ "logger/verbose", "level of verbosity",
	    .defval.str = LOG_VERBOSE, .val.str = &l_conf_verbose,
	    .datatype = RATTCONFDTSTR },
	{ "logger/module",
	    "list of modules to use as a logger",
	    .defval.lst.str = { LOG_MODULE },
	    .defval_lstcnt = LOG_MODULE_COUNT,
	    .val.lst.str = &l_conf_module_lst,
	    .val_cnt = &l_conf_module_cnt,
	    .datatype = RATTCONFDTSTR, .flags = RATTCONFFLLST },
	{ NULL }
};

static int attach_module(rattmod_entry_t *modentry)
{
	RATTLOG_TRACE();
	int retval;

	if (!(modentry->callbacks)) {
		debug("module `%s' provides no callback");
		return FAIL;
	}

	retval = ratt_table_push(&l_calltable, modentry->callbacks);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		return FAIL;
	}

	debug("attached callbacks for `%s'", modentry->name);
	return OK;
}

static void load_modules_callbacks(void)
{
	char *modname = NULL;
	int i, retval;

	for (i = 0, modname = l_conf_module_lst[i];
	    i < l_conf_module_cnt; modname = l_conf_module_lst[++i])
	{
		retval = module_attach(RATTLOG, modname);
		if (retval != OK)
			debug("module_attach() failed");
	}
}

void rattlog_msg(int level, const char *fmt, ...)
{
	rattlog_callback_t *callback = NULL;
	va_list ap;
	char msg[RATTLOG_MSGSIZMAX] = { '\0' };

	if (level > l_verbose) /* honor verbose level */
		return;

	va_start(ap, fmt);
	vsnprintf(msg, RATTLOG_MSGSIZMAX, fmt, ap);
	va_end(ap);

	/* if logger is up, use registered callbacks */
	if (l_state == LOG_INIT) {
		RATT_TABLE_FOREACH(&l_calltable, callback)
		{
			if (callback->on_msg)
				callback->on_msg(level, msg);
		}
	} else	/* default to standard logger */
		log_std_msg(level, msg);
}

void log_fini(void *udata)
{
	RATTLOG_TRACE();
	l_state = LOG_FINI;
//	rattmod_unregister(&log_std_entry);
	module_parent_detach(RATTLOG);
	ratt_table_destroy(&l_calltable);
	conf_table_release(l_conftable);
}

int log_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = conf_table_parse(l_conftable);
	if (retval != OK) {
		debug("conf_table_parse() failed");
		return FAIL;
	}

	retval = rattlog_name_to_level(l_conf_verbose);
	if (retval < RATTLOGMAX) {
		notice("switching to verbose level `%s'", l_conf_verbose);
		l_verbose = retval;
	} else
		warning("unknown verbose level `%s'", l_conf_verbose);

	retval = ratt_table_create(&l_calltable,
	    LOG_CALLTABLESIZ, sizeof(rattlog_callback_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		conf_table_release(l_conftable);
		return FAIL;
	} else
		debug("allocated module callback table of size `%u'",
		    LOG_CALLTABLESIZ);

	retval = module_parent_attach(RATTLOG, &attach_module);
	if (retval != OK) {
		debug("module_parent_attach() failed");
		ratt_table_destroy(&l_calltable);
		conf_table_release(l_conftable);
		return FAIL;
	}

	/* register standard logger */
	retval = rattmod_register(&log_std_entry);
	if (retval != OK) {
		debug("%s failed to register", MODULE_NAME);
		debug("rattmod_register() failed");
		module_parent_detach(RATTLOG);
		ratt_table_destroy(&l_calltable);
		conf_table_release(l_conftable);
		return FAIL;
	}
	
	retval = module_attach(RATTLOG, MODULE_NAME);
	if (retval != OK) {
		debug("%s failed to attach", MODULE_NAME);
		debug("module_attach() failed");
//		rattmod_unregister(&log_std_entry);
		module_parent_detach(RATTLOG);
		ratt_table_destroy(&l_calltable);
		conf_table_release(l_conftable);
		return FAIL;
	}

	/* load modules from config */
	load_modules_callbacks();

	retval = dtor_register(log_fini, NULL);
	if (retval != OK) {
		debug("dtor_register() failed");
		module_parent_detach(RATTLOG);
		ratt_table_destroy(&l_calltable);
		conf_table_release(l_conftable);
		return FAIL;
	}

	/* logger is up */
	l_state = LOG_INIT;

	return OK;
}
