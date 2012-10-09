/*
 * RATTD logger
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

#include <rattd/conf.h>
#include <rattd/def.h>
#include <rattd/log.h>

#include "conf.h"
#include "dtor.h"

static int l_verbose = RATTLOGMAX;	/* default to maximum verbose */

static char *l_conf_verbose = NULL;
static conf_decl_t l_conftable[] = {
	{ "logger/verbose", "level of verbosity",
	    .defval.str = "notice", .val.str = &l_conf_verbose,
	    .datatype = RATTCONFDTSTR },

	{ NULL }
};

#define TIMESTRSIZ 21	/* %d %b %Y %T + \0 */
void log_msg(int level, const char *fmt, ...)
{
	va_list ap;
	FILE *out = stdout;
	char timestr[TIMESTRSIZ] = { '\0' };
	struct tm tmres;
	time_t now;

	/* honor verbose level */
	if (level > l_verbose)
		return;

	va_start(ap, fmt);
	/* if there is modules attached, use them */

	/* else, default to std logging */
	if (level == RATTLOGERR)
		out = stderr;

	/* datetime prefix */
	if (((now = time(NULL)) != (time_t) -1)
	    && (localtime_r(&now, &tmres) != NULL)
	    && (strftime(timestr, TIMESTRSIZ, "%d %b %Y %T", &tmres) > 0))
		fprintf(out, "[%s] ", timestr);

	/* level name prefix */
	fprintf(out, "%s: ", rattlog_level_to_name(level));

	vfprintf(out, fmt, ap);
	va_end(ap);
}

void log_fini(void *udata)
{
	RATTLOG_TRACE();
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

	retval = dtor_register(log_fini, NULL);
	if (retval != OK) {
		debug("dtor_register() failed");
		log_fini(NULL);
		return FAIL;
	}

	return OK;
}
