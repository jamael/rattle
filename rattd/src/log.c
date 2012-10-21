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


#include <config.h>

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

/* name of the config declaration */
#define LOG_CONF_NAME	"logger"

/* callback table initial size */
#ifndef LOG_CALLTABLESIZ
#define LOG_CALLTABLESIZ	4
#endif
static RATT_TABLE_INIT(l_calltable);	/* callback table */

/* logger verbosity level */
static int l_verbose = RATTLOGMAX;

#ifndef RATTD_LOG_MODULE
#define RATTD_LOG_MODULE "syslog"
#endif
static RATTCONF_DEFVAL(l_conf_module_defval, RATTD_LOG_MODULE);
static RATTCONF_LIST_INIT(l_conf_module);

#ifndef RATTD_LOG_VERBOSE
#define RATTD_LOG_VERBOSE "notice"
#endif
static RATTCONF_DEFVAL(l_conf_verbose_defval, RATTD_LOG_VERBOSE);
static char *l_conf_verbose = NULL;

static conf_decl_t l_conftable[] = {
	{ "verbose", "level of verbosity",
	    l_conf_verbose_defval, &l_conf_verbose,
	    RATTCONFDTSTR, 0 },
	{ "module", "list of logger modules to use",
	    l_conf_module_defval, &l_conf_module,
	    RATTCONFDTSTR, RATTCONFFLLST },
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
	char **modname = NULL;
	int retval;

	RATTCONF_LIST_FOREACH(&l_conf_module, modname)
	{
		retval = module_attach(RATTLOG, *modname);
		if (retval != OK)
			debug("module_attach() failed");
	}
}

#define TIMESTRSIZ 21	/* %d %b %Y %T + \0 */
static void std_print_msg(int level, char const *msg)
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

static void on_msg(int level, char const *msg)
{
	rattlog_callback_t *callback = NULL;

	/* honor the verbose level setting */
	if (level > l_verbose)
		return;

	/* builtin standard logger */
	std_print_msg(level, msg);

	/* registered logger module */
	RATT_TABLE_FOREACH(&l_calltable, callback)
	{
		if (callback->on_msg)
			callback->on_msg(level, msg);
	}
}

void log_fini(void *udata)
{
	RATTLOG_TRACE();
	module_parent_detach(RATTLOG);
	ratt_table_destroy(&l_calltable);
	conf_release(l_conftable);
}

int log_init(void)
{
	RATTLOG_TRACE();
	int retval, level;

	retval = conf_parse(LOG_CONF_NAME, l_conftable);
	if (retval != OK) {
		debug("conf_parse() failed");
		return FAIL;
	}

	level = rattlog_name_to_level(l_conf_verbose);
	if (level < RATTLOGMAX) {
		notice("switching to verbose level `%s'", l_conf_verbose);
		l_verbose = level;
	} else
		warning("unknown verbose level `%s'", l_conf_verbose);

	retval = ratt_table_create(&l_calltable,
	    LOG_CALLTABLESIZ, sizeof(rattlog_callback_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		conf_release(l_conftable);
		return FAIL;
	} else
		debug("allocated module callback table of size `%u'",
		    LOG_CALLTABLESIZ);

	retval = module_parent_attach(RATTLOG, &attach_module);
	if (retval != OK) {
		debug("module_parent_attach() failed");
		ratt_table_destroy(&l_calltable);
		conf_release(l_conftable);
		return FAIL;
	}

	/* load modules from config */
	load_modules_callbacks();

	retval = dtor_register(log_fini, NULL);
	if (retval != OK) {
		debug("dtor_register() failed");
		module_parent_detach(RATTLOG);
		ratt_table_destroy(&l_calltable);
		conf_release(l_conftable);
		return FAIL;
	}

	return OK;
}

/**
 * \fn void rattlog_msg(int level, const char *fmt, ...)
 * \brief log a message
 *
 * Send a message to attached loggers
 *
 * \param level		the verbose level this message belongs to
 * \param fmt		printf-style format string
 * \param ...		additional params to match format string
 *
 */
void rattlog_msg(int level, const char *fmt, ...)
{
	va_list ap;
	char msg[RATTLOG_MSGSIZMAX] = { '\0' };

	va_start(ap, fmt);
	vsnprintf(msg, RATTLOG_MSGSIZMAX, fmt, ap);
	va_end(ap);

	on_msg(level, msg);
}
