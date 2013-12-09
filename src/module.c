/*
 * RATTLE module manager
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
#include <rattle.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <rattle/data.h>
#include <rattle/debug.h>

/* initial module array size */
#ifndef MODULE_ARRAY_SIZE
#define MODULE_ARRAY_SIZE	16
#endif

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

		/* XXX: dlsym __module_init_x(handle)

		retval = register_module_handle(handle);
		if (retval != OK) {
			debug("register_module_handle() failed");
			dlclose(handle);
			continue;
		}
		*/
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

static int compare_core_name(void const *in, void const *find)
{
	ratt_module_core_t const * const *core = in;
	char const *name = find;

	OOPS(core);
	OOPS(*core);
	OOPS((*core)->name);
	OOPS(name);

	return (strcmp((*core)->name, name)) ? NOMATCH : MATCH;
}

static int constrains_on_core(void const *in, void const *find)
{
	ratt_module_core_t const * const *core = find;

	OOPS(in);
	OOPS(core);
	OOPS(*core);
	OOPS((*core)->name);

	return compare_core_name(in, (*core)->name);
}

static int compare_module_name(void const *in, void const *find)
{
	ratt_module_entry_t const *module = in;
	char const *name = find;

	OOPS(module);
	OOPS(module->name);
	OOPS(name);

	return (strcmp(module->name, name)) ? NOMATCH : MATCH;
}

static int constrains_on_module(void const *in, void const *find)
{
	ratt_module_entry_t const *module = find;

	OOPS(in);
	OOPS(module);
	OOPS(module->name);

	return compare_module_name(in, module->name);
}

static int compare_hook_module_name(void const *in, void const *find)
{
	ratt_module_hook_t const *hookinfo = in;
	char const *name = find;

	OOPS(hookinfo);
	OOPS(hookinfo->module);
	OOPS(hookinfo->module->name);
	OOPS(name);

	return (strcmp(hookinfo->module->name, name)) ? NOMATCH : MATCH;
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
	return OK;
}

static inline void destroy_core_table()
{
	ratt_table_destroy(&l_cortab);
}

static int create_core_table()
{
	int retval;

	retval = ratt_table_create(&l_cortab,
	    MODULE_CORTABSIZ, sizeof(ratt_module_core_t *), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	}
	ratt_table_set_constrains(&l_cortab, constrains_on_core);
	return OK;
}

static void detach_module(ratt_module_entry_t *entry)
{
	ratt_module_hook_t *hookinfo = NULL;

	OOPS(entry);

	if (entry->core) {
		OOPS(entry->core);
		ratt_table_search(entry->core->hook_table, (void **) &hookinfo,
		    compare_hook_module_name, entry->name);
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
attach_module(
    ratt_module_core_t const *core,
    ratt_module_entry_t *module)
{
	ratt_module_hook_t hookinfo = { 0 };
	int retval;

	OOPS(core);
	OOPS(module);
						/* sanity checks */
	if (module->core) {
		debug("module `%s' is already attached to core `%s'",
		    module->name, module->core->name);
		return FAIL;
	} else if (!module->attach) {
		debug("module `%s' provides no hook", module->name);
		return FAIL;
	} else if (module->hook_size > core->hook_size) {
		debug("hook size is too big %u (%s) vs %u (%s)",
		    module->hook_size, module->name,
		    core->hook_size, core->name);
		return FAIL;
	} else if (!module->hook_size) {
		debug("module `%s' has hook size of 0", module->name);
		return FAIL;
	}
						/* configure module */
	if (module->config) {
		retval = conf_parse(core->conf_label, module->config);
		if (retval != OK) {
			debug("conf_parse() failed for `%s'", module->name);
			return FAIL;
		}
	}
						/* get module hook */
	hookinfo.hook = calloc(1, core->hook_size);
	if (!hookinfo.hook) {
		debug("calloc() failed");
		if (module->config)
			conf_release(module->config);
		return FAIL;
	}

	retval = module->attach(core, &hookinfo);
	if (retval != OK) {
		debug("module `%s' chose not to attach", module->name);
		free(hookinfo.hook);
		if (module->config)
			conf_release(module->config);
		return FAIL;
	} else if (hookinfo.version > core->ver_major) {
		debug("module `%s' hooked at version %u while core is at %u"
		    hookinfo.version, core->ver_major);
		if (module->detach)
			module->detach();
		free(hookinfo.hook);
		if (module->config)
			conf_release(module->config);
		return FAIL;
	}
						/* core is ok? */
	if (core->attach) {
		retval = core->attach(module, &hookinfo);
		if (retval != OK) {
			debug("core->attach() failed");
			if (module->detach)
				module->detach();
			free(hookinfo.hook);
			if (module->config)
				conf_release(module->config);
			return FAIL;
		}
	}
						/* hook module */
	hookinfo.module = module;
	retval = ratt_table_insert(core->hook_table, &hookinfo);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		if (core->detach)
			core->detach(module);
		if (module->detach)
			module->detach();
		free(hookinfo.hook);
		if (module->config)
			conf_release(module->config);
		return FAIL;
	}
	debug("`%s' hooked", module->name);
	module->core = core;
	return OK;
}

static int
attach_module_name(
    ratt_module_core_t const *core,
    char const *modname)
{
	ratt_module_entry_t *module = NULL;
	int retval;

	OOPS(core);
	OOPS(modname);
						/* search for module */
	ratt_table_search(&l_modtab, (void **) &module,
	    compare_module_name, modname);
	if (!module) {
		debug("could not find module `%s'", modname);
		return FAIL;
	}
						/* attach module */
	retval = attach_module(core, module);
	if (retval != OK) {
		debug("attach_module() failed");
		return FAIL;
	}
	return OK;
}

int module_core_detach(char const *corname)
{
	RATTLOG_TRACE();
	ratt_module_core_t **core = NULL;
	ratt_module_entry_t *entry = NULL;

	OOPS(corname);
						/* search core */
	ratt_table_search(&l_cortab, (void **) &core,
	    compare_core_name, corname);
	if (!core) {
		debug("could not find core `%s'", corname);
		return FAIL;
	}
						/* detach modules */
	RATT_TABLE_FOREACH(&l_modtab, entry)
	{
		if (entry->core == *core) {
			debug("detaching %s...", entry->name);
			detach_module(entry);
		}
	}
						/* delete core */
	ratt_table_del_current(&l_cortab);
	ratt_table_destroy((*core)->hook_table);
	return OK;
}

int module_core_attach(ratt_module_core_t const *core)
{
	RATTLOG_TRACE();
	size_t hook_table_size = MODULE_CHKTABSIZ;
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

	retval = ratt_table_insert(&l_cortab, &core);
	if (retval != OK) {
		debug("ratt_table_insert() failed");
		ratt_table_destroy(core->hook_table);
		return FAIL;
	}
	debug("module core `%s' added", core->name);
	return OK;
}

int module_core_unregister(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);
	OOPS(entry->core);
	OOPS(entry->core->name);

	retval = module_core_detach(entry->core->name);
	if (retval != OK) {
		debug("module_core_detach() failed");
		return FAIL;
	}

	return OK;
}

int module_core_register(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	OOPS(entry);
	OOPS(entry->core);

	if (entry->core->ver_major < MODULE_CORE_VERSION) {
		debug("module core version %u less than %u",
		    entry->core->ver_major, MODULE_CORE_VERSION);
		return FAIL;
	}

	retval = module_core_attach(entry->core);
	if (retval != OK) {
		debug("module_core_attach() failed");
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
	    compare_module_name, entry->name);
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
	destroy_core_table();
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
					/* core table */
	retval = create_core_table();
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

	if (!entry)
		return FAIL;
						/* unregister */
	if (entry->flags & RATTMODFLCOR) {
		retval = module_core_unregister(entry);
		if (retval != OK) {
			debug("module_core_unregister() failed");
			return FAIL;
		}
	}

	retval = module_unregister(entry);
	if (retval != OK) {
		debug("module_unregister() failed");
		return FAIL;
	}
						/* arguments */
	if (entry->args) {
		retval = args_unregister(MODULE_ARGSSEC_ID, entry->name);
		if (retval != OK)
			debug("args_unregister() failed");
	}
						/* destructor */
	if (entry->destructor)
		entry->destructor();
	return OK;
}

/**
 * \fn int ratt_module_register(ratt_module_entry_t const *entry)
 * \brief register a module
 *
 * Usage of ratt_module_register() is necessary for a module to
 * introduce itself to the RATTLE module registry.
 *
 * \param entry		pointer to module entry
 */
int ratt_module_register(ratt_module_entry_t const *entry, int version)
{
	RATTLOG_TRACE();
	int retval;

	if (!entry) {
		return FAIL;
	} else if (version != RATT_MODULE_VERSION) {
		debug("module version mismatch %i (%s) vs %i",
		    version, entry->name, RATT_MODULE_VERSION);
		return FAIL;
	}
						/* constructor */
	if (entry->constructor) {
		retval = entry->constructor();
		if (retval != OK) {
			debug("entry->constructor() failed");
			return FAIL;
		}
	}
						/* arguments */
	if (entry->args) {
		retval = args_register(MODULE_ARGSSEC_ID,
		    entry->name, entry->args);
		if (retval != OK) {
			debug("args_register() failed");
			if (entry->destructor)
				entry->destructor();
			return FAIL;
		}
	}
						/* register */
	retval = module_register(entry);
	if (retval != OK) {
		debug("module_register() failed");
		if (entry->args)
			args_unregister(MODULE_ARGSSEC_ID,
			    entry->name);
		if (entry->destructor)
			entry->destructor();
		return FAIL;
	}

	if (entry->flags & RATTMODFLCOR) {
		retval = module_core_register(entry);
		if (retval != OK) {
			debug("module_core_register() failed");
			module_unregister(entry);
			if (entry->args)
				args_unregister(MODULE_ARGSSEC_ID,
				    entry->name);
			if (entry->destructor)
				entry->destructor();
			return FAIL;
		}
	}

	return OK;
}

/**
 * \fn int ratt_module_attach(
 *             ratt_module_core_t const *core,
 *             char const *modname)
 *
 * \brief attach module to a core
 *
 * attach a module specified by its full or relative name to the
 * core specified by its core information structure.
 *
 * \param core		pointer to core information structure
 * \param modname	name of the module
 * \return OK if module attached, FAIL otherwise.
 */
int ratt_module_attach(ratt_module_core_t const *core, char const *modname)
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
		return attach_module_name(core, modname);

	/* modname must be prefixed with corname_ */
	snprintf(realmodname, RATTMODNAMSIZ, "%s_%s", core->name, modname);

	return attach_module_name(core, realmodname);
}
