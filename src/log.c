/*
 * RATTLE standard logger
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

#ifdef DEBUG
#include <rattle/debug.h>
#endif
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/table.h>

/* logger verbosity level */
#ifdef DEBUG
static int l_verbose = RATTLOGDBG;
#else
static int l_verbose = RATTLOGNOT;
#endif

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
	/* honor the verbose level setting */
	if (level > l_verbose)
		return;

	/* builtin standard logger */
	std_print_msg(level, msg);
}

void log_fini(void *udata)
{
	RATTLOG_TRACE();
}

int log_init(void)
{
	RATTLOG_TRACE();
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
