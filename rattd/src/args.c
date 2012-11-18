/*
 * RATTLE arguments manager
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

#include <ctype.h>
#include <stdio.h>

#include <rattle/args.h>
#include <rattle/log.h>
#include <rattle/table.h>

#include "args.h"

/* globals */
extern char * const *g_main_argv;
extern int const g_main_argc;

/* arguments section, A to Z plus main section */
#define ARGSSECSIZ 'Z' - 'A' + 2
#define ARGSSECPOS(x) (x) - 'A'
#define ARGSSECMAINPOS ARGSSECSIZ - 1

/* user-supplied arguments */
#define ARGSUSRSIZ 'z' - 'a' + 1
#define ARGSUSRPOS(x) (x) - 'a'

/* section table initial size */
#define ARGSSECTABSIZ		1

static ratt_table_t l_sectab[ARGSSECSIZ] = { 0 };	/* section tables */

typedef struct {
	char const * const name;	/* entry name, NULL for principal */
	ratt_args_t *sysargs;		/* available arguments */

	struct {
		int found;		/* option found */
		char *arg;		/* option argument, if any */
	} usrargs[ARGSUSRSIZ];		/* user-supplied arguments */
} args_entry_t;

static inline
ratt_table_t *pointer_to_section(ratt_table_t *table, int section)
{
	if (section == ARGSSECMAIN) {	
		return &(table[ARGSSECMAINPOS]);
	} else if (!isupper(section)) {
		debug("section identifier is invalid (%c)", section);
		return NULL;
	}

	return &(table[ARGSSECPOS(section)]);
}

static int compare_entry_name(void const *in, void const *find)
{
	args_entry_t const *entry = in;
	char const *name = find;

	if (!entry->name) {
		return (name) ? NOMATCH : MATCH;
	} else if (!name)
		return NOMATCH;

	return (strcmp(entry->name, name)) ? NOMATCH : MATCH;
}

static int constrains_on_entry(void const *in, void const *find)
{
	args_entry_t const *entry = find;
	return compare_entry_name(in, entry->name);
}

static int update_on_constrains(void *old, void const *new)
{
	args_entry_t *entry = old;
	args_entry_t const *new_entry = new;
	int opt = 0;

	if (new_entry->sysargs) {			/* update sysargs */
		debug("(was) sysargs = %p", entry->sysargs);
		entry->sysargs = new_entry->sysargs;
		debug("(now) sysargs = %p", entry->sysargs);
		return OK;
	}

	while (opt < ARGSUSRSIZ) {
		if (new_entry->usrargs[opt].found) {	/* update usrargs */
			debug("(was) -%c: found = %i, arg = %s", opt + 'a',
			    entry->usrargs[opt].found,
			    entry->usrargs[opt].arg);
			entry->usrargs[opt].found++;
			entry->usrargs[opt].arg = new_entry->usrargs[opt].arg;
			debug("(now) -%c: found = %i, arg = %s", opt + 'a',
			    entry->usrargs[opt].found,
			    entry->usrargs[opt].arg);
		}
		opt++;
	}

	return OK;
}

static inline void destroy_section(ratt_table_t *table)
{
	if (ratt_table_exists(table))
		ratt_table_destroy(table);
}

static void destroy_section_all()
{
	int i = 0;
	while (i < ARGSSECSIZ)
		destroy_section(&(l_sectab[i++]));
}

static int create_section(ratt_table_t *table)
{
	int retval;

	retval = ratt_table_create(table, ARGSSECTABSIZ,
	    sizeof(args_entry_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}

	ratt_table_set_constrains(table, constrains_on_entry);
	ratt_table_set_on_constrains(table, update_on_constrains);

	return OK;
}

static int insert_args(int sec, char const *name, int opt, char *arg)
{
	args_entry_t entry = { name };
	ratt_table_t *secp = NULL;
	int retval;

	secp = pointer_to_section(l_sectab, sec);
	if (!secp) {
		debug("pointer_to_section() failed");
		return FAIL;
	}

	if (!ratt_table_exists(secp)		/* create section */
	    && create_section(secp) != OK) {
		debug("create_section() failed");
		return FAIL;
	}

						/* register usrargs */
	entry.usrargs[ARGSUSRPOS(opt)].found = 1;
	entry.usrargs[ARGSUSRPOS(opt)].arg = arg;

	retval = ratt_table_insert(secp, &entry);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		return FAIL;
	}
	debug("inserted option -%c into `%c/%s' with arg = %s",
	    opt, sec, name, arg);

	return OK;
}

static int parse_args(int argc, char * const *argv)
{
	char *arg = NULL, *next = NULL, *name = NULL;
	char section = ARGSSECMAIN;
	int i;

	for (i = 1, arg = argv[i]; i < argc; arg = argv[++i]) {
		if (strlen(arg) == 2 && arg[0] == '-') {
			if (isupper(arg[1])) {		/* start of section */
				section = arg[1];
				if (i + 1 < argc) {
					next = argv[i + 1];
					if (isalnum(next[0])) {
						name = next;
						i++;
					} else
						name = NULL;
				}
			} else if (isalpha(arg[1])) {	/* found option */
				if (i + 1 < argc) {
					next = argv[i + 1];
					if (next[0] != '-') {	/* with arg */
						insert_args(section, name,
						    arg[1], next);
						i++;
						continue;
					}
				}

				insert_args(section, name, arg[1], NULL);
			} else if (arg[1] == '-') {	/* end of parsing */
				break;
			} else	{			/* invalid option */
				error("invalid option: -%c", arg[1]);
				return FAIL;
			}
		} else {
			error("failed parsing argument #%i: %s", i, arg);
			return FAIL;
		}
	}

	return OK;
}

static int bind_args(args_entry_t const *entry, ratt_args_t *args)
{
	char const *arg = NULL;
	int found = 0, retval;

	for (; args && islower(args->option); args++) {
		found = entry->usrargs[ARGSUSRPOS(args->option)].found;
		if (found) {
			if ((args->flags & RATTARGSFLONE) && found > 1) {
				error("-%c specified twice", args->option);
				return FAIL;
			}

			arg = entry->usrargs[ARGSUSRPOS(args->option)].arg;
			if ((args->flags & RATTARGSFLARG) && !arg) {
				error("-%c requires argument", args->option);
				return FAIL;
			}

			if (arg && !args->get) {
				error("-%c cannot have argument", args->option);
				return FAIL;
			} else if (arg) {
				retval = args->get(arg);
				if (retval != OK) {
					debug("args->get() failed");
					return FAIL;
				}
			}

			if (args->found)
				*(args->found) = found;
		}
	}

	return OK;
}

int args_unregister(int section, char const *name)
{
	RATTLOG_TRACE();
	args_entry_t *entry = NULL;
	ratt_table_t *secp = NULL;
	int retval;

	secp = pointer_to_section(l_sectab, section);
	if (!secp) {
		debug("pointer_to_section() failed");
		return FAIL;
	} else if (!ratt_table_exists(secp)) {
		debug("section `%c' does not exist", section);
		return FAIL;
	}

	ratt_table_search(secp, (void **) &entry,
	    compare_entry_name, name);
	if (!entry) {				/* not found */
		debug("entry `%c/%s' does not exist", section, name);
		return FAIL;
	}
					/* unregister sysargs */
	entry->sysargs = NULL;
	debug("unregistered arguments for `%c/%s'", section, name);

	return OK;
}

int args_register(int section, char const *name, ratt_args_t *sysargs)
{
	RATTLOG_TRACE();
	args_entry_t new = { name }, *entry = NULL;
	ratt_table_t *secp = NULL;
	ratt_args_t *arg = NULL;
	int retval;

	secp = pointer_to_section(l_sectab, section);
	if (!secp) {
		debug("pointer_to_section() failed");
		return FAIL;
	}

	if (!ratt_table_exists(secp)		/* create section */
	    && create_section(secp) != OK) {
		debug("create_section() failed");
		return FAIL;
	}
						/* register sysargs */
	new.sysargs = sysargs;

	retval = ratt_table_insert(secp, &new);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		return FAIL;
	}
	debug("registered arguments for `%c/%s'", section, name);

						/* bind arguments */
	entry = ratt_table_current(secp);
	retval = bind_args(entry, sysargs);
	if (retval != OK) {
		debug("bind_args() failed");
		return FAIL;
	}

	return OK;
}

void args_show(void)
{
	fprintf(stdout, "\nUSAGE:\n");

	/* todo */
}

void args_fini(void *udata)
{
	destroy_section_all();
}

int args_init(void)
{
	int retval;

	if (!g_main_argv) {
		debug("program arguments not made global");
		return FAIL;
	} else if (g_main_argc > 0) {
		retval = parse_args(g_main_argc, g_main_argv);
		if (retval != OK) {
			debug("parse_args() failed");
			return FAIL;
		}
	}

	return OK;
}
