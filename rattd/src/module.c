/*
 * RATTLE module core
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

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <rattle/args.h>
#ifdef DEBUG
#include <rattle/debug.h>
#endif
#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/table.h>

#include "args.h"
#include "conf.h"

/* name of the config declaration */
#define MODULE_CONF_NAME	"module"

/* arguments section identifier */
#define MODULE_ARGSSEC_ID	'M'

/* initial parent hook table size */
#ifndef MODULE_PHKTABSIZ
#define MODULE_PHKTABSIZ		4
#endif

/* initial module table size */
#ifndef MODULE_MODTABSIZ
#define MODULE_MODTABSIZ		16
#endif
static RATT_TABLE_INIT(l_modtab);	/* module table */

/* initial parent table size */
#ifndef MODULE_PARTABSIZ
#define MODULE_PARTABSIZ		16
#endif
static RATT_TABLE_INIT(l_partab);	/* parent table */

#ifndef RATTD_MODULE_PATH
#define RATTD_MODULE_PATH "/usr/lib/rattd", "/usr/local/lib/rattd"
#endif
static RATT_CONF_DEFVAL(l_conf_modpath_defval, RATTD_MODULE_PATH);
static RATT_CONF_LIST_INIT(l_conf_modpath);

static ratt_conf_t l_conf[] = {
	{ "search-path", "list of path to search for modules",
	    l_conf_modpath_defval, &l_conf_modpath,
	    RATTCONFDTSTR, RATTCONFFLLST },
	{ NULL }
};

/* module arguments */
static int l_args_show_modules = 0;
static char const *l_args_module_path = NULL;
static int get_args_module_path(char const *);
static ratt_args_t l_args[] = {
	{ 'l', NULL, "show a list of available modules",
	    &l_args_show_modules, NULL, 0 },
	{ 'p', "path", "use specified path to search for modules",
	    NULL, get_args_module_path, RATTARGSFLARG },
	{ 0 }
};

static int get_args_module_path(char const *path)
{
	if (strlen(path) >= PATH_MAX) {
		warning("%s: path is too long", path);
		return FAIL;
	}
	l_args_module_path = path;
	return OK;
}

static int register_module_handle(void *handle)
{
	ratt_module_entry_t *entry = NULL;

	entry = ratt_table_last(&l_modtab);
	if (entry && (entry->handle == NULL)) {
		entry->handle = handle;
		return OK;
	}
	debug("none or wrong module entry");
	return FAIL;
}

static int unload_modules(void)
{
	void *handle = NULL;
	ratt_module_entry_t *entry = NULL;

	RATT_TABLE_FOREACH(&l_modtab, entry)
	{
		handle = entry->handle;
		ratt_module_unregister(entry);
		if (handle) {	/* shared object */
			dlclose(handle);
			handle = NULL;
		}
	}
	return OK;
}

static int load_modules_from_path(char const *path)
{
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	void *handle = NULL;
	char sofile[PATH_MAX] = { '\0' };
	int retval;

	OOPS(path);

	dir = opendir(path);
	if (!dir) {
		debug("could not load modules from `%s': %s",
		    path, strerror(errno));
		return FAIL;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type != DT_REG)
			continue;

		snprintf(sofile, PATH_MAX, "%s/%s",
		    path, entry->d_name);

		handle = dlopen(sofile, RTLD_LAZY);
		if (!handle) {
			debug("`%s' is not a module", entry->d_name);
			continue;
		}

		retval = register_module_handle(handle);
		if (retval != OK) {
			debug("register_module_handle() failed");
			dlclose(handle);
			continue;
		}
	}

	closedir(dir);
	return OK;
}

static int load_modules(void)
{
	char **modpath = NULL;
					/* from arguments */
	if (l_args_module_path) {
		load_modules_from_path(l_args_module_path);
	} else				/* from config */
		RATT_CONF_LIST_FOREACH(&l_conf_modpath, modpath)
		{
			load_modules_from_path(*modpath);
		}

	return OK;
}

static int sort_module_name(void const *a, void const *b)
{
	ratt_module_entry_t const * const *a_entry = a;
	ratt_module_entry_t const * const *b_entry = b;
	return strcmp((*a_entry)->name, (*b_entry)->name);
}

static ratt_module_entry_t **sort_modules(void)
{
	ratt_module_entry_t **list = NULL, **p = NULL;
	ratt_module_entry_t *entry = NULL;
	size_t cnt = 0;

	cnt = ratt_table_count(&l_modtab);
	if (!cnt)
		return NULL;

	list = calloc(cnt + 1, sizeof(ratt_module_entry_t *));
	if (!list) {
		debug("calloc() failed");
		return NULL;
	}

	p = list;
	RATT_TABLE_FOREACH(&l_modtab, entry)
	{
		*p = entry;
		p++;
	}
	qsort(list, cnt, sizeof(ratt_module_entry_t *), sort_module_name);
	*p = NULL;

	return list;
}

static void show_modules(void)
{
	ratt_module_entry_t **entry = NULL, **list = NULL;

	fprintf(stdout, "%-*s %-*s %s\n\n",
	    32, "MODULE", 8, "VERSION", "DESCRIPTION");

	list = sort_modules();
	if (!list)
		return;

	for (entry = list; *entry != NULL; entry++) {
		fprintf(stdout, "%-*s %-*s %s\n",
		    32, (*entry)->name,
		    8, (*entry)->version,
		    (*entry)->desc);
	}
	free(list);
}

static int compare_parent_name(void const *in, void const *find)
{
	ratt_module_parent_t const *parent = in;
	char const *name = find;
	return (strcmp(parent->name, name)) ? NOMATCH : MATCH;
}

static int constrains_on_parent(void const *in, void const *find)
{
	ratt_module_parent_t const *parent = find;
	return compare_parent_name(in, parent->name);
}

static int compare_module_name(void const *in, void const *find)
{
	ratt_module_entry_t const *module = in;
	char const *name = find;
	return (strcmp(module->name, name)) ? NOMATCH : MATCH;
}

static int constrains_on_module(void const *in, void const *find)
{
	ratt_module_entry_t const *module = find;
	return compare_module_name(in, module->name);
}

static inline void destroy_module_table()
{
	ratt_table_destroy(&l_modtab);
}

static int create_module_table()
{
	int retval;

	retval = ratt_table_create(&l_modtab,
	    MODULE_MODTABSIZ, sizeof(ratt_module_entry_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}
	ratt_table_set_constrains(&l_modtab, constrains_on_module);
	debug("allocated module table of size `%u'",
	    MODULE_MODTABSIZ);
	return OK;
}

static inline void destroy_parent_table()
{
	ratt_table_destroy(&l_partab);
}

static int create_parent_table()
{
	int retval;

	retval = ratt_table_create(&l_partab,
	    MODULE_PARTABSIZ, sizeof(ratt_module_parent_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}
	ratt_table_set_constrains(&l_partab, constrains_on_parent);
	debug("allocated parent table of size `%u'",
	    MODULE_PARTABSIZ);
	return OK;
}

static void detach_module(ratt_module_entry_t *entry)
{
	OOPS(entry);

	if (entry->parinfo) {
		if (entry->parinfo->detach)
			entry->parinfo->detach(entry);
		if (entry->detach)
			entry->detach();
		entry->parinfo = NULL;
	}
}

static int
attach_module(
    ratt_module_parent_t *parent,
    ratt_module_entry_t  *module)
{
	void const *hook = NULL;
	int retval;

	OOPS(parent);
	OOPS(module);
						/* hook size match? */
	if (module->hook_size != parent->hook_size) {
		debug("hook size mismatch %i (%s) vs %i (%s)",
		    module->hook_size, module->name,
		    parent->hook_size, parent->name);
		return FAIL;
	}
						/* module is orphan? */
	if (module->parinfo) {
		debug("module `%s' is already attached to parent `%s'",
		    module->name, module->parinfo->name);
		return FAIL;
	}
						/* module can attach? */
	if (!module->attach) {
		debug("module `%s' provides no hook", module->name);
		return FAIL;
	}
						/* does parent agree? */
	if (parent->attach) {
		retval = parent->attach(module);
		if (retval != OK) {
			debug("parent->attach() failed");
			return FAIL;
		}
	}
						/* configure module */
	if (module->config) {
		retval = conf_parse(parent->conf_label, module->config);
		if (retval != OK) {
			debug("conf_parse() failed for `%s'", module->name);
			return FAIL;
		}
	}
						/* module is ok? */
	hook = module->attach(parent);
	if (!hook) {
		debug("`%s' chose not to attach", module->name);
		if (module->config)
			conf_release(module->config);
		return FAIL;
	}
						/* hook module */
	retval = ratt_table_insert(parent->hook_table, &hook);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		if (module->config)
			conf_release(module->config);
		if (module->detach)
			module->detach();
		return FAIL;
	}
	debug("`%s' hooked", module->name);
	module->parinfo = parent;
	return OK;
}

static int
try_attach_module(
    char const *parname,
    char const *modname)
{
	ratt_module_entry_t *module = NULL;
	ratt_module_parent_t *parent = NULL;
	int retval;

	OOPS(parname);
	OOPS(modname);
						/* search for parent */
	ratt_table_search(&l_partab, (void **) &parent,
	    &compare_parent_name, parname);
	if (!parent) {
		debug("could not find parent `%s'", parname);
		return FAIL;
	}
						/* search for module */
	ratt_table_search(&l_modtab, (void **) &module,
	    &compare_module_name, modname);
	if (!module) {
		debug("could not find module `%s'", modname);
		return FAIL;
	}
						/* attach module */
	retval = attach_module(parent, module);
	if (retval != OK) {
		debug("attach_module() failed");
		return FAIL;
	}
	return OK;
}

int module_parent_detach(char const *parname)
{
	RATTLOG_TRACE();
	ratt_module_parent_t *parent = NULL;
	ratt_module_entry_t *entry = NULL;

	OOPS(parname);
						/* search parent */
	ratt_table_search(&l_partab, (void **) &parent,
	    &compare_parent_name, parname);
	if (!parent) {
		debug("could not find parent `%s'", parname);
		return FAIL;
	}
						/* detach modules */
	RATT_TABLE_FOREACH(&l_modtab, entry)
	{
		if (entry->parinfo == parent) {
			debug("detaching %s...", entry->name);
			detach_module(entry);
		}
	}
						/* delete parent */
	ratt_table_destroy(parent->hook_table);
	ratt_table_del_current(&l_partab);
	return OK;
}

int module_parent_attach(ratt_module_parent_t const *parinfo)
{
	RATTLOG_TRACE();
	size_t hook_table_size = MODULE_PHKTABSIZ;
	int hook_table_flags = 0;
	int retval;

	OOPS(parinfo);
	OOPS(parinfo->hook_table);

	if (parinfo->flags & RATTMODPARFLONE) {
		hook_table_size = 1;
		hook_table_flags = RATTTABFLNRA;
	}

	retval = ratt_table_create(parinfo->hook_table,
	    hook_table_size, sizeof(void *), hook_table_flags);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}

	retval = ratt_table_insert(&l_partab, parinfo);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		ratt_table_destroy(parinfo->hook_table);
		return FAIL;
	}
	debug("module parent `%s' added", parinfo->name);
	return OK;
}

int module_parent_unregister(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);
	OOPS(entry->parinfo);
	OOPS(entry->parinfo->name);

	retval = module_parent_detach(entry->parinfo->name);
	if (retval != OK) {
		debug("module_parent_detach() failed");
		return FAIL;
	}

	return OK;
}

int module_parent_register(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);
	OOPS(entry->parinfo);

	retval = module_parent_attach(entry->parinfo);
	if (retval != OK) {
		debug("module_parent_attach() failed");
		return FAIL;
	}

	return OK;
}

int module_unregister(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	ratt_module_entry_t *module = NULL;
	int retval;

	OOPS(entry);

	retval = ratt_table_search(&l_modtab, (void **) &module,
	    &compare_module_name, entry->name);
	if (retval != OK) {
		debug("ratt_table_search() failed");
		return FAIL;
	}

	ratt_table_del_current(&l_modtab);

	return OK;
}

int module_register(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);

	retval = ratt_table_insert(&l_modtab, entry);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		return FAIL;
	}

	return OK;
}

void module_fini(void *udata)
{
	RATTLOG_TRACE();
	unload_modules();
	destroy_parent_table();
	destroy_module_table();
	conf_release(l_conf);
}

int module_init(void)
{
	RATTLOG_TRACE();
	int retval;
					/* arguments */
	retval = args_register(MODULE_ARGSSEC_ID, NULL, l_args);
	if (retval != OK) {
		debug("args_register() failed");
		return FAIL;
	}
					/* configuration */
	retval = conf_parse(MODULE_CONF_NAME, l_conf);
	if (retval != OK) {
		debug("conf_parse() failed");
		return FAIL;
	}
					/* module table */
	retval = create_module_table();
	if (retval != OK) {
		debug("create_module_table() failed");
		conf_release(l_conf);
		return FAIL;
	}
					/* parent table */
	retval = create_parent_table();
	if (retval != OK) {
		debug("create_module_table() failed");
		destroy_module_table();
		conf_release(l_conf);
		return FAIL;
	}
					/* load modules */
	load_modules();

	/* if user wants a list, do that now */
	if (l_args_show_modules) {
		show_modules();
		module_fini(NULL);
		return STOP;
	}

	return OK;
}

/**
 * \fn ratt_module_unregister(ratt_module_entry_t const *entry)
 * \brief unregister a module
 *
 * Usage of ratt_module_unregister() is necessary for a module to
 * vanish from the RATTLE module registry.
 *
 * \param entry		A pointer to the module entry
 */
int ratt_module_unregister(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);
						/* arguments */
	if (entry->args) {
		retval = args_unregister(MODULE_ARGSSEC_ID, entry->name);
		if (retval != OK)
			debug("args_unregister() failed");
	}
						/* destructor */
	if (entry->destructor)
		entry->destructor();
						/* is parent */
	if (entry->flags & RATTMODFLPAR) {
		retval = module_parent_unregister(entry);
		if (retval != OK) {
			debug("module_parent_unregister() failed");
			return FAIL;
		}
						/* is module */
	} else {
		retval = module_unregister(entry);
		if (retval != OK) {
			debug("module_unregister() failed");
			return FAIL;
		}
	}
	return OK;
}

/**
 * \fn ratt_module_register(ratt_module_entry_t const *entry)
 * \brief register a module
 *
 * Usage of ratt_module_register() is necessary for a module to
 * introduce itself to the RATTLE module registry.
 *
 * \param entry		A pointer to the module entry
 */
int ratt_module_register(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);
						/* is parent */
	if (entry->flags & RATTMODFLPAR) {
		retval = module_parent_register(entry);
		if (retval != OK) {
			debug("module_parent_register() failed");
			return FAIL;
		}
						/* is module */
	} else {
		retval = module_register(entry);
		if (retval != OK) {
			debug("module_register() failed");
			return FAIL;
		}
	}
						/* constructor */
	if (entry->constructor) {
		retval = entry->constructor();
		if (retval != OK) {
			debug("entry->constructor() failed");
			module_unregister(entry);
			return FAIL;
		}
	}
						/* arguments */
	if (entry->args) {
		retval = args_register(MODULE_ARGSSEC_ID,
		    entry->name, entry->args);
		if (retval != OK) {
			debug("args_register() failed");
			module_unregister(entry);
			if (entry->destructor)
				entry->destructor();
			return FAIL;
		}
	}
	return OK;
}

/**
 * \fn int ratt_module_attach(char const *parname, char const *modname)
 * \brief attach module to a parent
 *
 * Attach a module specified by its full or relative name to the
 * parent specified by its name.
 *
 * \param parname	name of the parent
 * \param modname	name of the module
 * \return OK if module attached, FAIL otherwise.
 */
int ratt_module_attach(char const *parname, char const *modname)
{
	RATTLOG_TRACE();
	char realmodname[RATTMODNAMSIZ] = { '\0' };
	char const *separator = modname;
	size_t parlen, modlen;

	OOPS(parname);
	OOPS(modname);

	modlen = strlen(modname);
	parlen = strlen(parname);
	separator += parlen;

	if ((modlen > (parlen + 2)) && (!strncmp(parname, modname, parlen)
	    && (*separator == '_'))) /* modname with parname_ prefix ok */
		return try_attach_module(parname, modname);

	/* modname must be prefixed with parname_ */
	snprintf(realmodname, RATTMODNAMSIZ, "%s_%s", parname, modname);

	return try_attach_module(parname, realmodname);
}
