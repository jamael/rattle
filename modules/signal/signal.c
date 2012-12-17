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

#include "debug.h"

#define signum_to_string(n) sys_siglist[(n)]

/* mask of handled signals */
static sigset_t l_wait_sigmask;

/* pointer to altstack memory */
static void *l_altstack = NULL;

/* signal table initial size */
#ifndef SIGNAL_SIGTABSIZ
#define SIGNAL_SIGTABSIZ	4
#endif
static RATT_TABLE_INIT(l_sigtab);	/* signal table */

/* signal entry table initial size */
#ifndef SIGNAL_ENTTABSIZ
#define SIGNAL_ENTTABSIZ	4
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

static int compare_signal_number(void const *in, void const *find)
{
	signal_register_t const *sig = in;
	int const *signum = find;
	return (sig->num == *signum) ? MATCH : NOMATCH;
}

static int compare_entry_handler(void const *in, void const *find)
{
	signal_entry_t const *entry = in;
	return (entry->handler == find) ? MATCH : NOMATCH;
}

static int constrains_on_entry(void const *in, void const *find)
{
	signal_entry_t const *entry = find;
	return compare_entry_handler(in, entry->handler);
}

static void handle_signal(int signum, siginfo_t *siginfo, void *unused)
{
	int retval;
	signal_register_t *sig = NULL;
	signal_entry_t *entry = NULL;

	ratt_table_search(&l_sigtab, (void **) &sig,
	    compare_signal_number, &signum);
	if (!sig) {
		debug("signal %i handled but not registered?", signum);
	} else
		RATT_TABLE_FOREACH_REVERSE(&(sig->entry), entry)
		{
			if (entry->handler)
				entry->handler(signum,
				    siginfo, entry->udata);
		}
}

static void handle_oops(int signum, siginfo_t *siginfo, void *unused)
{
	debug_write();

//	printf("segfault at %p", siginfo->si_addr);
//	debug_backtrace(siginfo);
	exit(1);
}

static void unregister_signal_all()
{
	signal_register_t *sig = NULL;
	signal_entry_t *entry = NULL;

	sigemptyset(&l_wait_sigmask);
	RATT_TABLE_FOREACH(&l_sigtab, sig)
	{
		RATT_TABLE_FOREACH(&(sig->entry), entry)
		{
			debug("`%s' stack is not empty with %p still around",
			    signum_to_string(sig->num), entry->handler);
		}
		ratt_table_destroy(&(sig->entry));
	}
}

static void unregister_signal(int signum)
{
	signal_register_t *sig = NULL;

	RATT_TABLE_FOREACH(&l_sigtab, sig)
	{
		if (sig->num == signum) {
			ratt_table_destroy(&(sig->entry));
			sigdelset(&l_wait_sigmask, signum);
			ratt_table_del_current(&l_sigtab);
		}
	}
}

static int register_signal(int signum)
{
	signal_register_t *sig = NULL;
	signal_register_t new_sig = { signum, NULL };
	int retval;

	switch (signum) {
	case SIGSEGV:
	case SIGKILL:
	case SIGSTOP:
	case SIGILL:
	case SIGFPE:
		debug("cannot handle signal %i (%s)",
		    signum, signum_to_string(signum));
		return FAIL;
	}

	ratt_table_search(&l_sigtab, (void **) &sig,
	    compare_signal_number, &signum);
	if (sig) {
		debug("signal number %i registered already", sig->num);
		return FAIL;
	}

	retval = ratt_table_create(&(new_sig.entry),
	    SIGNAL_ENTTABSIZ, sizeof(signal_entry_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}

	ratt_table_set_constrains(&(new_sig.entry), constrains_on_entry);

	retval = ratt_table_push(&l_sigtab, &new_sig);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		ratt_table_destroy(&(new_sig.entry));
		return FAIL;
	}

	sigaddset(&l_wait_sigmask, signum);

	debug("registered `%s' signal number %i, slot %i",
	    signum_to_string(signum), signum,
	    ratt_table_pos_current(&l_sigtab));

	return OK;
}

void signal_unregister(int signum,
                       void (*handler)(int, siginfo_t const *, void *))
{
	RATTLOG_TRACE();
	signal_register_t *sig = NULL;
	signal_entry_t *entry = NULL;

	ratt_table_search(&l_sigtab, (void **) &sig,
	    compare_signal_number, &signum);
	if (sig) {
		ratt_table_search(&(sig->entry), (void **) &entry,
		    compare_entry_handler, handler);
		if (entry) {
			ratt_table_del_current(&(sig->entry));
			if (ratt_table_isempty(&(sig->entry)))
				unregister_signal(sig->num);
		} else
			debug("no entry for handler at %p in `%s'",
			    handler, signum_to_string(signum));
	} else
		debug("signal `%s' is not registered",
		    signum_to_string(signum));
}

int signal_register(int signum,
                    void (*handler)(int, siginfo_t const *, void *),
                    void *udata)
{
	RATTLOG_TRACE();
	signal_register_t *sig = NULL;
	signal_entry_t *entry, new_entry = { handler, udata };
	int retval;

	ratt_table_search(&l_sigtab, (void **) &sig,
	    compare_signal_number, &signum);
	if (!sig) {			/* register signal */
		retval = register_signal(signum);
		if (retval != OK) {
			debug("register_signal() failed");
			return FAIL;
		}

		sig = ratt_table_current(&l_sigtab);
	}
					/* register handler */
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

void signal_wait(void)
{
	siginfo_t siginfo;
	sigwaitinfo(&l_wait_sigmask, &siginfo);
	handle_signal(siginfo.si_signo, &siginfo, NULL);
}

void signal_fini(void *udata)
{
	RATTLOG_TRACE();
	unregister_signal_all();
	ratt_table_destroy(&l_sigtab);
	free(l_altstack);
}

int signal_init(void)
{
	RATTLOG_TRACE();
	struct sigaction sigsegv = { 0 };
	stack_t altstack = { 0 };
	sigset_t blockmask, unused;
	int retval;

	/* initialize signal mask to wait for */
	sigemptyset(&l_wait_sigmask);

	sigfillset(&blockmask);
	sigdelset(&blockmask, SIGSEGV);
	sigprocmask(SIG_BLOCK, &blockmask, &unused);

	retval = ratt_table_create(&l_sigtab,
	    SIGNAL_SIGTABSIZ, sizeof(signal_register_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}
	debug("allocated signal table of size `%u'",
	    SIGNAL_SIGTABSIZ);

	/* handle segfault on alternate stack */
	l_altstack = calloc(1, SIGSTKSZ);
	if (!l_altstack) {
		debug("calloc() failed");
		ratt_table_destroy(&l_sigtab);
		return FAIL;
	}
	/* export stack */
	altstack.ss_sp = l_altstack;
	altstack.ss_size = SIGSTKSZ;
	sigaltstack(&altstack, NULL);
	/* register handler */
	sigsegv.sa_sigaction = handle_oops;
	sigsegv.sa_flags = SA_ONSTACK;
	sigfillset(&sigsegv.sa_mask);
	sigaction(SIGSEGV, &sigsegv, NULL);

	return OK;
}
