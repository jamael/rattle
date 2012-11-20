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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include <rattle/conf.h>
#ifdef DEBUG
#include <rattle/debug.h>
#endif
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/table.h>

#include "conf.h"
#include "module.h"

/* logger verbosity level */
#ifdef DEBUG
static int l_verbose = RATTLOGDBG;
#else
static int l_verbose = RATTLOGNOT;
#endif

/* configuration */
#define LOG_CONF_LABEL	"logger"

#ifndef RATTD_LOG_MODULE
#define RATTD_LOG_MODULE "syslog"
#endif
static RATT_CONF_DEFVAL(l_conf_module_defval, RATTD_LOG_MODULE);
static RATT_CONF_LIST_INIT(l_conf_module);

#ifndef RATTD_LOG_VERBOSE
#define RATTD_LOG_VERBOSE "notice"
#endif
static RATT_CONF_DEFVAL(l_conf_verbose_defval, RATTD_LOG_VERBOSE);
static char *l_conf_verbose = NULL;

static ratt_conf_t l_conf[] = {
	{ "verbose", "level of verbosity",
	    l_conf_verbose_defval, &l_conf_verbose,
	    RATTCONFDTSTR, 0 },
	{ "module", "list of logger modules to use",
	    l_conf_module_defval, &l_conf_module,
	    RATTCONFDTSTR, RATTCONFFLLST },
	{ NULL }
};

/* parent info */
static RATT_TABLE_INIT(l_hooktab);
static ratt_module_parent_t l_parent_info = {
	.name = RATT_LOG_NAME,
	.ver_major = RATT_LOG_VER_MAJOR,
	.ver_minor = RATT_LOG_VER_MINOR,
	.conf_label = LOG_CONF_LABEL,
	.hook_table = &l_hooktab,
	.hook_size = sizeof(ratt_log_hook_t),
};

/* time prefix format to strftime() */
#define LOGTIMFMT	"[%d %b %Y %T] "

/* time prefix size, once processed */
#define LOGTIMSIZ	24

/* total prefix size, including log level */
#define LOGPFXSIZ LOGTIMSIZ + RATTLOGLVLSIZ

static void std_print_msg(int level, char const *msg)
{
	FILE *out = NULL;
	char prefix[LOGPFXSIZ] = { '\0' };
	struct tm tmres;
	time_t now;

	out = (level > RATTLOGERR) ? stdout : stderr;

	/* datetime prefix */
	if (((now = time(NULL)) != (time_t) -1)
	    && (localtime_r(&now, &tmres) != NULL))
		strftime(prefix, LOGTIMSIZ, LOGTIMFMT, &tmres);
	/* level name prefix */
	strncat(prefix, ratt_log_level_name(level), RATTLOGLVLSIZ);

	/* message */
	fprintf(out, "%s: %s", prefix, msg);
}

static void on_msg(int level, char const *msg)
{
	ratt_log_hook_t **hook = NULL;

	/* honor the verbose level setting */
	if (level > l_verbose)
		return;

	/* builtin standard logger */
	std_print_msg(level, msg);

	/* registered logger module */
	RATT_TABLE_FOREACH(&l_hooktab, hook)
	{
		if ((*hook)->on_msg)
			(*hook)->on_msg(level, msg);
	}
}

void log_detach(void *udata)
{
	RATTLOG_TRACE();
	module_parent_detach(RATT_LOG_NAME);
}

int log_attach(void)
{
	RATTLOG_TRACE();
	char **modname = NULL;
	int retval;

	retval = module_parent_attach(&l_parent_info);
	if (retval != OK) {
		debug("module_parent_attach() failed");
		return FAIL;
	}

	RATT_CONF_LIST_FOREACH(&l_conf_module, modname)
	{
		ratt_module_attach(RATT_LOG_NAME, *modname);
	}
	return OK;
}

void log_fini(void *udata)
{
	RATTLOG_TRACE();
	conf_release(l_conf);
}

int log_init(void)
{
	RATTLOG_TRACE();
	int retval, level;

	retval = conf_parse(LOG_CONF_LABEL, l_conf);
	if (retval != OK) {
		debug("conf_parse() failed");
		return FAIL;
	}

	level = ratt_log_level(l_conf_verbose);
	if (level >= RATTLOGMAX) {
		warning("unknown verbose level: %s", l_conf_verbose);
	} else if (level != l_verbose) {
		debug("switching to verbose level: %s", l_conf_verbose);
		l_verbose = level;
	}

	return OK;
}

/**
 * \fn void ratt_log_msg(int level, const char *fmt, ...)
 * \brief log a message
 *
 * Log a message with verbose level information.
 *
 * \param level		the verbose level this message belongs to
 * \param fmt		printf-style format string
 * \param ...		additional params to match format string
 */
void ratt_log_msg(int level, const char *fmt, ...)
{
	va_list ap;
	char msg[RATTLOGMSGSIZ] = { '\0' };

	va_start(ap, fmt);
	vsnprintf(msg, RATTLOGMSGSIZ, fmt, ap);
	va_end(ap);

	on_msg(level, msg);
}
