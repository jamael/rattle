/*
 * RATTLE signal handler
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
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, SIGNALUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/table.h>

#define signum_to_string(n) sys_siglist[(n)]

static void handle_signal(int, siginfo_t *, void *);
struct sigaction l_sigaction = { .sa_sigaction = &handle_signal };

/* signal table initial size */
#ifndef SIGNAL_TABLESIZ
#define SIGNAL_TABLESIZ		4
#endif
static RATT_TABLE_INIT(l_sigtable);	/* signal table */

/* signal entry table initial size */
#ifndef SIGNAL_ENTRY_TABLESIZ
#define SIGNAL_ENTRY_TABLESIZ	4
#endif
typedef struct {
	int const num;			/* signal number */
	ratt_table_t entry;		/* signal entry table */
} signal_register_t;

typedef struct {
	/* signal handler */
	void (*handler)(int, siginfo_t const *, void *);
	/* signal user data */
	void *udata;
} signal_entry_t;

static int signum_compare(void const *in, void const *find)
{
	signal_register_t const *sig = in;
	int const *signum = find;
	return (sig->num == *signum);
}

static void handle_signal(int signum, siginfo_t *siginfo, void *unused)
{
	int retval;
	signal_register_t *sig = NULL;
	signal_entry_t *entry = NULL;

	retval = ratt_table_search(&l_sigtable, (void **) &sig,
	    &signum_compare, &signum);
	if (retval != OK) { /* signal not registered */
		debug("signal %i handled but not registered?", signum);
		return;
	}

	RATT_TABLE_FOREACH_REVERSE(&(sig->entry), entry)
	{
		if (entry->handler)
			entry->handler(signum, siginfo, entry->udata);
	}
}

static void unregister_signal_all()
{
	signal_register_t *sig = NULL;

	RATT_TABLE_FOREACH(&l_sigtable, sig)
	{
		ratt_table_destroy(&(sig->entry));
		signal(sig->num, SIG_DFL);
	}
}

static void unregister_signal(int signum)
{
	signal_register_t *sig = NULL;

	RATT_TABLE_FOREACH(&l_sigtable, sig)
	{
		if (sig->num == signum) {
			ratt_table_destroy(&(sig->entry));
			signal(sig->num, SIG_DFL);
		}
	}
}

static int register_signal(int signum)
{
	signal_register_t *sig = NULL;
	signal_register_t new_sig = { signum, NULL };
	int retval;

	retval = ratt_table_search(&l_sigtable, (void **) &sig,
	    &signum_compare, &signum);
	if (retval != OK) { /* signal not registered yet */
		retval = ratt_table_create(&(new_sig.entry),
		    SIGNAL_ENTRY_TABLESIZ, sizeof(signal_entry_t), 0);
		if (retval != OK) {
			debug("ratt_table_create() failed");
			return FAIL;
		}

		retval = ratt_table_push(&l_sigtable, &new_sig);
		if (retval != OK) {
			debug("ratt_table_push() failed");
			ratt_table_destroy(&(new_sig.entry));
			return FAIL;
		}

		sigaction(signum, &l_sigaction, NULL);

		debug("registered `%s' signal number %i, slot %i",
		    signum_to_string(signum), signum,
		    ratt_table_pos_last(&l_sigtable));
		return OK;
	}

	/* signal registered already */
	debug("signal number %i registered already", sig->num);
	return FAIL;
}

void signal_unregister(void (*handler)(int, siginfo_t const *, void *))
{
	RATTLOG_TRACE();
	signal_register_t *sig = NULL;
	signal_entry_t *entry = NULL;

	RATT_TABLE_FOREACH(&l_sigtable, sig)
	{
		RATT_TABLE_FOREACH(&(sig->entry), entry)
		{
			if (entry->handler == handler) {
				ratt_table_del_current(&(sig->entry));
				if (ratt_table_isempty(&(sig->entry)))
					unregister_signal(sig->num);
				break;
			}
		}
	}
}

int signal_register(int signum,
                    void (*handler)(int, siginfo_t const *, void *),
                    void *udata)
{
	RATTLOG_TRACE();
	signal_register_t *sig = NULL;
	signal_entry_t *entry, new_entry = { handler, udata };
	int retval;

	retval = ratt_table_search(&l_sigtable, (void **) &sig,
	    &signum_compare, &signum);
	if (retval != OK) { /* signal not registered yet */
		retval = register_signal(signum);
		if (retval != OK) {
			debug("register_signal() failed");
			return FAIL;
		}
		sig = ratt_table_get_current(&l_sigtable);
	} else
		RATT_TABLE_FOREACH(&(sig->entry), entry)
		{
			if (entry->handler == handler) {
				debug("cannot register twice (%p on %i)",
				    handler, sig->num);
				return FAIL;
			}
		}

	retval = ratt_table_push(&(sig->entry), &new_entry);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		if (ratt_table_isempty(&(sig->entry)))
			unregister_signal(sig->num);
		return FAIL;
	}

	debug("registered handler %p on signal %i, slot %i",
	    handler, sig->num, ratt_table_pos_last(&(sig->entry)));
	return OK;
}

void signal_fini(void *udata)
{
	RATTLOG_TRACE();
	unregister_signal_all();
	ratt_table_destroy(&l_sigtable);
}

int signal_init(void)
{
	RATTLOG_TRACE();
	int retval;
	
	retval = ratt_table_create(&l_sigtable,
	    SIGNAL_TABLESIZ, sizeof(signal_register_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	} else
		debug("allocated signal table of size `%u'",
		    SIGNAL_TABLESIZ);

	/* initialize signal mask */
	sigemptyset(&l_sigaction.sa_mask);

	return OK;
}
