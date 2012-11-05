/*
 * RATTLE module loader
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

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/table.h>

#include "conf.h"
#include "dtor.h"

/* name of the config declaration */
#define MODULE_CONF_NAME	"module"

/* initial module table size */
#ifndef MODULE_MODTABSIZ
#define MODULE_MODTABSIZ		16
#endif
static RATT_TABLE_INIT(l_modtab);	/* module table */

/* initial parent table size */
#ifndef MODULE_PARTABSIZ
#define MODULE_PARTABSIZ	16
#endif
static RATT_TABLE_INIT(l_partab);	/* parent table */

typedef struct {
	ratt_module_parent_t const *info;	/* parent information */
	int (*attach)(ratt_module_entry_t const *); /* attach callback */
} module_parent_t;

#ifndef RATTD_MODULE_PATH
#define RATTD_MODULE_PATH "/usr/lib/rattd", "/usr/local/lib/rattd"
#endif
static RATTCONF_DEFVAL(l_conf_modpath_defval, RATTD_MODULE_PATH);
static RATTCONF_LIST_INIT(l_conf_modpath);

static ratt_conf_t l_conf[] = {
	{ "search-path", "list of path to search for modules",
	    l_conf_modpath_defval, &l_conf_modpath,
	    RATTCONFDTSTR, RATTCONFFLLST },
	{ NULL }
};

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

static int unload_modules()
{
	void *handle = NULL;
	ratt_module_entry_t *entry = NULL;

	RATT_TABLE_FOREACH(&l_modtab, entry)
	{
		handle = entry->handle;
		ratt_module_unregister(entry);
		if (handle) {/* external module */
			dlclose(handle);
			handle = NULL;
		}
	}

	return OK;
}

static int load_modules()
{
	DIR *dir = NULL;
	char **modpath = NULL;
	struct dirent *entry = NULL;
	void *handle = NULL;
	char sofile[PATH_MAX] = { '\0' };
	int retval;

	RATTCONF_LIST_FOREACH(&l_conf_modpath, modpath)
	{
		debug("looking for modules inside `%s'", *modpath);

		dir = opendir(*modpath);
		if (!dir) {
			debug("could not load modules from `%s': %s",
			    *modpath, strerror(errno));
			continue;
		}

		while ((entry = readdir(dir)) != NULL) {
			if (entry->d_type != DT_REG)
				continue;

			snprintf(sofile, PATH_MAX, "%s/%s",
			    *modpath, entry->d_name);

			handle = dlopen(sofile, RTLD_LAZY);
			if (!handle) {
				debug("`%s' is not a module", entry->d_name);
				debug("dlopen() tells %s", dlerror());
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
	}
	return OK;
}

static int compare_parent_name(void const *in, void const *find)
{
	module_parent_t const *parent = in;
	char const *name = find;
	return (strcmp(parent->info->name, name)) ? FAIL : OK;
}

static int compare_module_name(void const *in, void const *find)
{
	ratt_module_entry_t const *module = in;
	char const *name = find;
	return (strcmp(module->name, name)) ? FAIL : OK;
}

static int attach_module(char const *parname, char const *modname)
{
	ratt_module_entry_t *module = NULL;
	module_parent_t *parent = NULL;
	int retval;

	retval = ratt_table_search(&l_partab, (void **) &parent,
	    &compare_parent_name, parname);
	if (retval != OK) {
		debug("could not find parent `%s'", parname);
		debug("ratt_table_search() failed");
		return FAIL;
	}

	retval = ratt_table_search(&l_modtab, (void **) &module,
	    &compare_module_name, modname);
	if (retval != OK) {
		debug("could not find module `%s'", modname);
		debug("ratt_table_search() failed");
		return FAIL;
	}

	if (module->parinfo) {
		debug("module `%s' is already attached to parent `%s'",
		    modname, module->parinfo->name);
		return FAIL;
	}

	if (!(module->attach)) {
		debug("module `%s' provides no hook", module->name);
		return FAIL;
	}

	debug("attaching module `%s' to `%s'", modname, parname);
	retval = parent->attach(module);
	if (retval != OK) {
		debug("parent->attach() failed");
		return FAIL;
	} else
		module->parinfo = parent->info;

	return OK;
}

int module_attach(char const *parname, char const *modname)
{
	RATTLOG_TRACE();
	char realmodname[RATTMODNAMSIZ] = { '\0' };
	char const *separator = modname;
	size_t parlen, modlen;

	modlen = strlen(modname);
	if (!modlen) { /* module with no name */
		debug("reject module with no name");
		return FAIL;
	}

	parlen = strlen(parname);
	if (!parlen) { /* parent with no name */
		debug("reject parent with no name");
		return FAIL;
	}

	separator += parlen;

	if ((modlen > (parlen + 2)) && (!strncmp(parname, modname, parlen)
	    && (*separator == '_'))) /* modname with parname_ prefix ok */
		return attach_module(parname, modname);

	/* modname must be prefixed with parname_ */
	snprintf(realmodname, RATTMODNAMSIZ, "%s_%s", parname, modname);

	return attach_module(parname, realmodname);
}

int module_parent_detach(ratt_module_parent_t const *parinfo)
{
	RATTLOG_TRACE();
	module_parent_t *parent = NULL;
	int retval;

	retval = ratt_table_search(&l_partab, (void **) &parent,
	    &compare_parent_name, parinfo->name);
	if (retval != OK) {
		debug("ratt_table_search() failed");
		return FAIL;
	}

	/*XXX: should detach all modules attached */

	ratt_table_del_current(&l_partab);

	return OK;
}

int module_parent_attach(ratt_module_parent_t const *parinfo,
                         int (*attach)(ratt_module_entry_t const *))
{
	RATTLOG_TRACE();
	module_parent_t *parent, new_parent = { parinfo, attach };
	int retval;

	if (!parinfo) {
		debug("cannot register parent without information");
		return FAIL;
	} else if (!attach) {
		debug("cannot register parent without attach callback");
		return FAIL;
	}

	retval = ratt_table_search(&l_partab, (void **) &parent,
	    &compare_parent_name, parinfo->name);
	if (retval == OK) {
		debug("module parent `%s' exists in table", parinfo->name);
		return FAIL;
	}

	retval = ratt_table_insert(&l_partab, &new_parent);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		return FAIL;
	}

	debug("module parent `%s' added", parinfo->name);

	return OK;
}

int module_parent_unregister(ratt_module_entry_t const *entry)
{
	RATTLOG_TRACE();
	int retval;

	if (!entry) {
		debug("NULL module entry given");
		return FAIL;
	} else if (!entry->parinfo) {
		debug("reject module without parent information");
		return FAIL;
	}

	retval = module_parent_detach(entry->parinfo);
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

	if (!entry) {
		debug("NULL module entry given");
		return FAIL;
	} else if (!entry->parinfo) {
		debug("reject module without parent information");
		return FAIL;
	} else if (!entry->parinfo->attach) {
		debug("reject module without attach callback");
		return FAIL;
	}

	retval = module_parent_attach(entry->parinfo,
	    entry->parinfo->attach);
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

	if (!entry) {
		debug("NULL module entry given");
		return FAIL;
	}

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
	ratt_module_entry_t *module = NULL;
	int retval;

	if (!entry) {
		debug("NULL module entry given");
		return FAIL;
	}

	retval = ratt_table_search(&l_modtab, (void **) &module,
	    &compare_module_name, entry->name);
	if (retval == OK) {
		debug("module `%s' exists in table", entry->name);
		return FAIL;
	}

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
	ratt_table_destroy(&l_modtab);
	ratt_table_destroy(&l_partab);
	conf_release(l_conf);
}

int module_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = conf_parse(MODULE_CONF_NAME, l_conf);
	if (retval != OK) {
		debug("conf_parse() failed");
		return FAIL;
	}

	retval = ratt_table_create(&l_modtab,
	    MODULE_MODTABSIZ, sizeof(ratt_module_entry_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		conf_release(l_conf);
		return FAIL;
	} else
		debug("allocated module table of size `%u'",
		    MODULE_MODTABSIZ);

	retval = ratt_table_create(&l_partab,
	    MODULE_PARTABSIZ, sizeof(module_parent_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		ratt_table_destroy(&l_modtab);
		conf_release(l_conf);
		return FAIL;
	} else
		debug("allocated parent table of size `%u'",
		    MODULE_PARTABSIZ);

	/* load available modules */
	load_modules();

	retval = dtor_register(module_fini, NULL);
	if (retval != OK) {
		debug("dtor_register() failed");
		unload_modules();
		ratt_table_destroy(&l_partab);
		ratt_table_destroy(&l_modtab);
		conf_release(l_conf);
		return FAIL;
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

	debug("unloading module `%s'", entry->name);

	if (entry->destructor)
		entry->destructor();

	if (entry->flags & RATTMODFLPAR) { /* a module parent */
		retval = module_parent_unregister(entry);
		if (retval != OK) {
			debug("module_parent_unregister() failed");
			return FAIL;
		}
	} else { /* a module */
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

	if (entry->constructor) {
		retval = entry->constructor();
		if (retval != OK) {
			debug("`%s' constructor failed", entry->name);
			return FAIL;
		}
	}

	if (entry->flags & RATTMODFLPAR) { /* a module parent */
		retval = module_parent_register(entry);
		if (retval != OK) {
			debug("module_parent_register() failed");
			return FAIL;
		}
	} else { /* a module */
		retval = module_register(entry);
		if (retval != OK) {
			debug("module_register() failed");
			return FAIL;
		}
	}

	debug("loaded module `%s'", entry->name);

	return OK;
}

/**
 * \fn static inline int
 * ratt_module_attach_from_config(char const *parname,
 *                                rattconf_list_t *modules)
 * \brief attach modules from config setting
 *
 * Helper to attach modules found in a config list setting.
 *
 * \param parname	parent name to attach modules to
 * \param modules	list of modules name in a config setting
 * \return OK if at least one module attached to parent, FAIL otherwise.
 */
int ratt_module_attach_from_config(char const *parname, ratt_conf_list_t *modules)
{
	RATTLOG_TRACE();
	char **name = NULL;
	int retval, attached = 0;

	RATTCONF_LIST_FOREACH(modules, name)
	{
		retval = module_attach(parname, *name);
		if (retval != OK) {
			debug("module_attach() failed");
		} else
			attached++;
	}

	return (attached) ? OK : FAIL;
}
