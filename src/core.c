/*
 * RATTLE core manager
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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <rattle/args.h>
#include <rattle/core.h>
#include <rattle/debug.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/table.h>

#include "args.h"
#include "conf.h"

#define CORE_VERSION RATTLE_VERSION_MINOR

/* initial core hook table size */
#ifndef CORE_HOOK_TABSIZ
#define CORE_HOOK_TABSIZ	4
#endif

/* initial core table size */
#ifndef CORE_ENTRY_TABSIZ
#define CORE_ENTRY_TABSIZ	8
#endif
static RATT_TABLE_INIT(l_coretab);	/* core table */

static int compare_core_name(void const *in, void const *find)
{
	ratt_core_core_t const * const *core = in;
	char const *name = find;

	OOPS(core);
	OOPS(*core);
	OOPS((*core)->name);
	OOPS(name);

	return (strcmp((*core)->name, name)) ? NOMATCH : MATCH;
}

static int constrains_on_core_entry(void const *in, void const *find)
{
	ratt_core_core_t const * const *core = find;

	OOPS(in);
	OOPS(core);
	OOPS(*core);
	OOPS((*core)->name);

	return compare_core_name(in, (*core)->name);
}

static int compare_hook_module_name(void const *in, void const *find)
{
	ratt_core_hook_t const *hookinfo = in;
	char const *name = find;

	OOPS(hookinfo);
	OOPS(hookinfo->module);
	OOPS(hookinfo->module->name);
	OOPS(name);

	return (strcmp(hookinfo->module->name, name)) ? NOMATCH : MATCH;
}

static inline void destroy_core_table()
{
	ratt_table_destroy(&l_coretab);
}

static int create_core_table()
{
	int retval;

	retval = ratt_table_create(&l_coretab,
	    CORE_ENTRY_TABSIZ, sizeof(ratt_core_t *), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}
	ratt_table_set_constrains(&l_coretab, constrains_on_core_entry);
	return OK;
}

static void detach_hook(ratt_core_entry_t *entry)
{
	ratt_core_hook_t *hookinfo = NULL;

	OOPS(entry);

	if (entry->core) {
		OOPS(entry->core);
		ratt_table_search(entry->core->hook_table, (void **) &hookinfo,
		    compare_hook_core_name, entry->name);
		if (!hookinfo) {
			debug("hook for `%s' not found", entry->name);
		} else {
			debug("removing hook to '%s'", entry->name);
			free(hookinfo->hook);
			ratt_table_del_current(entry->core->hook_table);
		}
		if (entry->core->detach)
			entry->core->detach(entry);
		if (entry->detach)
			entry->detach();
		entry->core = NULL;
	}
}

static int
attach_core(
    ratt_core_core_t const *core,
    ratt_core_entry_t *core)
{
	ratt_core_hook_t hookinfo = { 0 };
	int retval;

	OOPS(core);
	OOPS(core);
						/* sanity checks */
	if (core->core) {
		debug("core `%s' is already attached to core `%s'",
		    core->name, core->core->name);
		return FAIL;
	} else if (!core->attach) {
		debug("core `%s' provides no hook", core->name);
		return FAIL;
	} else if (core->hook_size > core->hook_size) {
		debug("hook size is too big %u (%s) vs %u (%s)",
		    core->hook_size, core->name,
		    core->hook_size, core->name);
		return FAIL;
	} else if (!core->hook_size) {
		debug("core `%s' has hook size of 0", core->name);
		return FAIL;
	}
						/* configure core */
	if (core->config) {
		retval = conf_parse(core->conf_label, core->config);
		if (retval != OK) {
			debug("conf_parse() failed for `%s'", core->name);
			return FAIL;
		}
	}
						/* get core hook */
	hookinfo.hook = calloc(1, core->hook_size);
	if (!hookinfo.hook) {
		debug("calloc() failed");
		if (core->config)
			conf_release(core->config);
		return FAIL;
	}

	retval = core->attach(core, &hookinfo);
	if (retval != OK) {
		debug("core `%s' chose not to attach", core->name);
		free(hookinfo.hook);
		if (core->config)
			conf_release(core->config);
		return FAIL;
	} else if (hookinfo.version > core->ver_major) {
		debug("core `%s' hooked at version %u while core is at %u"
		    hookinfo.version, core->ver_major);
		if (core->detach)
			core->detach();
		free(hookinfo.hook);
		if (core->config)
			conf_release(core->config);
		return FAIL;
	}
						/* core is ok? */
	if (core->attach) {
		retval = core->attach(core, &hookinfo);
		if (retval != OK) {
			debug("core->attach() failed");
			if (core->detach)
				core->detach();
			free(hookinfo.hook);
			if (core->config)
				conf_release(core->config);
			return FAIL;
		}
	}
						/* hook core */
	hookinfo.core = core;
	retval = ratt_table_insert(core->hook_table, &hookinfo);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		if (core->detach)
			core->detach(core);
		if (core->detach)
			core->detach();
		free(hookinfo.hook);
		if (core->config)
			conf_release(core->config);
		return FAIL;
	}
	debug("`%s' hooked", core->name);
	core->core = core;
	return OK;
}

static int
attach_core_name(
    ratt_core_core_t const *core,
    char const *modname)
{
	ratt_core_entry_t *core = NULL;
	int retval;

	OOPS(core);
	OOPS(modname);
						/* search for core */
	ratt_table_search(&l_modtab, (void **) &core,
	    compare_core_name, modname);
	if (!core) {
		debug("could not find core `%s'", modname);
		return FAIL;
	}
						/* attach core */
	retval = attach_core(core, core);
	if (retval != OK) {
		debug("attach_core() failed");
		return FAIL;
	}
	return OK;
}

int core_core_detach(char const *corname)
{
	RATTLOG_TRACE();
	ratt_core_core_t **core = NULL;
	ratt_core_entry_t *entry = NULL;

	OOPS(corname);
						/* search core */
	ratt_table_search(&l_coretab, (void **) &core,
	    compare_core_name, corname);
	if (!core) {
		debug("could not find core `%s'", corname);
		return FAIL;
	}
						/* detach cores */
	RATT_TABLE_FOREACH(&l_modtab, entry)
	{
		if (entry->core == *core) {
			debug("detaching %s...", entry->name);
			detach_core(entry);
		}
	}
						/* delete core */
	ratt_table_del_current(&l_coretab);
	ratt_table_destroy((*core)->hook_table);
	return OK;
}

int core_core_attach(ratt_core_core_t const *core)
{
	RATTLOG_TRACE();
	size_t hook_table_size = CORE_CHKTABSIZ;
	int hook_table_flags = 0;
	int retval;

	OOPS(core);
	OOPS(core->hook_table);

	if (core->flags & RATTMODCORFLONE) {
		hook_table_size = 1;
		hook_table_flags = RATTTABFLNRA;
	}

	retval = ratt_table_create(core->hook_table,
	    hook_table_size, sizeof(void *), hook_table_flags);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}

	return OK;
}

int core_unregister(ratt_core_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);
	OOPS(entry->name);

	retval = core_core_detach(entry->core->name);
	if (retval != OK) {
		debug("core_core_detach() failed");
		return FAIL;
	}

	return OK;
}

int core_register(ratt_core_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);

	if (entry->ver_major < RATT_CORE_VERSION) {
		debug("core `%s' is not compatible with core v%u",
		    entry->name, RATT_CORE_VERSION);
		return FAIL;
	} else if (!strlen(entry->name)) {
		debug("core at %p has no name?", entry);
		return FAIL;
	}

	retval = ratt_table_insert(&l_coretab, &entry);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		return FAIL;
	}
	debug("core `%s' registered", entry->name);
	return OK;
}

int core_load(ratt_core_t const *core)
{
}

void core_fini(void *udata)
{
	RATTLOG_TRACE();
	destroy_core_table();
}

int core_init(void)
{
	RATTLOG_TRACE();
	int retval;
					/* core table */
	retval = create_core_table();
	if (retval != OK) {
		debug("create_core_table() failed");
		return FAIL;
	}
	return OK;
}

/**
 * \fn int ratt_core_attach(
 *             ratt_core_core_t const *core,
 *             char const *modname)
 *
 * \brief attach core to a core
 *
 * attach a core specified by its full or relative name to the
 * core specified by its core information structure.
 *
 * \param core		pointer to core information structure
 * \param modname	name of the core
 * \return OK if core attached, FAIL otherwise.
 */
int ratt_core_attach(ratt_core_core_t const *core, char const *modname)
{
	RATTLOG_TRACE();
	char realmodname[RATTMODNAMSIZ] = { '\0' };
	char const *separator = modname;
	size_t corlen, modlen;

	OOPS(core);
	OOPS(modname);

	modlen = strlen(modname);
	corlen = strlen(core->name);
	separator += corlen;

	if ((modlen > (corlen + 2)) && (!strncmp(core->name, modname, corlen)
	    && (*separator == '_'))) /* modname with corname_ prefix ok */
		return attach_core_name(core, modname);

	/* modname must be prefixed with corname_ */
	snprintf(realmodname, RATTMODNAMSIZ, "%s_%s", core->name, modname);

	return attach_core_name(core, realmodname);
}
static void initialize_core_components(void)
{
	struct main_core *core = l_main_coretab;
	int retval;

	for (; core && core->init; core++) {
		debug("---");
		switch (core->init()) {
		case OK:
			retval = dtor_register(core->fini, NULL);
			if (retval != OK) {
				debug("dtor_register() failed");
				if (core->fini)
					core->fini(NULL);
				exit(EXIT_FAILURE);
			}

			/* FALLTHROUGH */

		case NOSTART:
			break;
		case STOP:
			exit(EXIT_SUCCESS);
		default:
		case FAIL:
			exit(EXIT_FAILURE);
		}
	}
}

