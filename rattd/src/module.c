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

#ifndef MODULE_PARENT_TABLESIZ
#define MODULE_PARENT_TABLESIZ 16
#endif

static RATT_TABLE_INIT(l_modtable);	/* module table */
static RATT_TABLE_INIT(l_partable);	/* parent table */

typedef struct {
	char const * const name;	/* parent name */
	int (*attach)(rattmod_entry_t *);	/* attach callback */
} module_parent_t;

#ifndef RATTD_MODULE_PATH
#define RATTD_MODULE_PATH "/usr/lib/rattd", "/usr/local/lib/rattd"
#define RATTD_MODULE_PATH_COUNT 2
#elif !defined RATTD_MODULE_PATH_COUNT
#error "RATTD_MODULE_PATH requires RATTD_MODULE_PATH_COUNT"
#endif
static char **l_conf_modpath_lst = NULL;
static size_t l_conf_modpath_cnt = 0;

#ifndef RATTD_MODULE_TABLESIZ
#define RATTD_MODULE_TABLESIZ 16
#endif
static uint16_t l_conf_modtable_size = 0;	/* module table size */

static conf_decl_t l_conftable[] = {
	{ "module/path",
	    "list of path to search for modules",
	    .defval.lst.str = { RATTD_MODULE_PATH },
	    .defval_lstcnt = RATTD_MODULE_PATH_COUNT,
	    .val.lst.str = &l_conf_modpath_lst,
	    .val_cnt = &l_conf_modpath_cnt,
	    .datatype = RATTCONFDTSTR, .flags = RATTCONFFLLST },
	{ "module/table-size-init",
	    "set the initial size of the module table",
	    .defval.num = RATTD_MODULE_TABLESIZ,
	    .val.num = (long long *)&l_conf_modtable_size,
	    .datatype = RATTCONFDTNUM16, .flags = RATTCONFFLUNS },
	{ NULL }
};

static int register_module_handle(void *handle)
{
	rattmod_entry_t *entry = NULL;

	entry = ratt_table_get_last(&l_modtable);
	if (entry && (entry->handle == NULL)) {
		entry->handle = handle;
		return OK;
	}
	debug("none or wrong module entry");
	return FAIL;
}

static int unload_modules()
{
	rattmod_entry_t *entry = NULL;

	RATT_TABLE_FOREACH(&l_modtable, entry)
	{
		if (entry->handle)
			dlclose(entry->handle);
	}

	return OK;
}

static int load_modules()
{
	DIR *dir = NULL;
	struct dirent *entry = NULL;
	void *handle = NULL;
	char sofile[PATH_MAX] = { '\0' };
	int retval, i;

	for (i = 0; i < l_conf_modpath_cnt; ++i) {
		debug("looking for modules inside `%s'",
		    l_conf_modpath_lst[i]);

		dir = opendir(l_conf_modpath_lst[i]);
		if (!dir) {
			warning("could not load modules from `%s': %s",
			    l_conf_modpath_lst[i], strerror(errno));
			continue;
		}

		while ((entry = readdir(dir)) != NULL
		    && entry->d_type == DT_REG) {
			snprintf(sofile, PATH_MAX, "%s/%s",
			    l_conf_modpath_lst[i], entry->d_name);

			handle = dlopen(sofile, RTLD_LAZY);
			if (!handle) {
				warning("`%s' is not a module", entry->d_name);
				debug("dlopen() tells %s", dlerror());
				continue;
			}

			retval = register_module_handle(handle);
			if (retval != OK) {
				warning("`%s' failed to register properly",
				    entry->d_name);
				debug("register_module_handle() failed");
				dlclose(handle);
				continue;
			}
		}

		closedir(dir);
	}
	return OK;
}

static int parent_compare(void const *in, void const *find)
{
	module_parent_t const *parent = in;
	char const *name = find;
	return (strcmp(parent->name, name)) ? FAIL : OK;
}

static int module_compare(void const *in, void const *find)
{
	rattmod_entry_t const *module = in;
	char const *name = find;
	return (strcmp(module->name, name)) ? FAIL : OK;
}

static int __module_attach(char const *parname, char const *modname)
{
	RATTLOG_TRACE();
	rattmod_entry_t *module = NULL;
	module_parent_t *parent = NULL;
	int retval;

	retval = ratt_table_search(&l_partable, (void **) &parent,
	    &parent_compare, parname);
	if (retval != OK) {
		debug("could not find parent `%s'", parname);
		debug("ratt_table_search() failed");
		return FAIL;
	}

	retval = ratt_table_search(&l_modtable, (void **) &module,
	    &module_compare, modname);
	if (retval != OK) {
		debug("could not find module `%s'", modname);
		debug("ratt_table_search() failed");
		return FAIL;
	}

	if (module->parent_name) {
		debug("module `%s' is already attached to parent `%s'",
		    modname, module->parent_name);
		return FAIL;
	}

	debug("attaching module `%s' to `%s'", modname, parname);
	parent->attach(module);
	module->parent_name = parent->name;

	return OK;
}

int rattmod_register(rattmod_entry_t const *entry)
{
	RATTLOG_TRACE();
	rattmod_entry_t *module = NULL;
	int retval;

	if (!entry) {
		debug("cannot register a NULL module entry");
		return FAIL;
	}

	retval = ratt_table_search(&l_modtable, (void **) &module,
	    &module_compare, entry->name);
	if (retval == OK) {
		debug("module `%s' exists in table", entry->name);
		return FAIL;
	}

	retval = ratt_table_push(&l_modtable, entry);
	if (retval != OK) {
		warning("failed to register module `%s'", entry->name);
		debug("ratt_table_push() failed");
		return FAIL;
	}

	notice("loaded module `%s'", entry->name);
	debug("registered module entry %p, slot %i",
	    entry, ratt_table_pos_last(&l_modtable));

	return OK;
}

int module_attach(char const *parname, char const *modname)
{
	RATTLOG_TRACE();
	char realmodname[RATTMOD_NAMEMAXSIZ] = { '\0' };
	char const *separator = modname;
	size_t parlen, modlen;

	parlen = strlen(parname);
	modlen = strlen(modname);
	separator += parlen;

	if ((modlen > (parlen + 2)) && (!strncmp(parname, modname, parlen)
	    && (*separator == '_'))) /* modname with parname_ prefix ok */
		return __module_attach(parname, modname);

	/* modname must be prefixed with parname_ */
	snprintf(realmodname, RATTMOD_NAMEMAXSIZ, "%s_%s", parname, modname);

	return __module_attach(parname, realmodname);
}

int module_parent_detach(char const *parname)
{
	RATTLOG_TRACE();
	module_parent_t *parent = NULL;
	int retval;

	retval = ratt_table_search(&l_partable, (void **) &parent,
	    &parent_compare, parname);
	if (retval != OK) {
		debug("ratt_table_search() failed");
		return FAIL;
	}

	/* clearing the chunk is enough to detach */
	memset(parent, 0, sizeof(module_parent_t));

	return OK;
}

int module_parent_attach(char const *parname, int (*attach)(rattmod_entry_t *))
{
	RATTLOG_TRACE();
	module_parent_t parent = { parname, attach };
	int retval;

	if (!parname) {
		debug("cannot register parent without a name");
		return FAIL;
	}

	retval = ratt_table_push(&l_partable, &parent);
	if (retval != OK) {
		warning("failed to register module parent `%s'", parname);
		debug("ratt_table_push() failed");
		return FAIL;
	}

	debug("registered module parent `%s'", parname);

	return OK;
}

void module_fini(void *udata)
{
	RATTLOG_TRACE();
	unload_modules();
	ratt_table_destroy(&l_modtable);
	ratt_table_destroy(&l_partable);
	conf_table_release(l_conftable);
}

int module_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = conf_table_parse(l_conftable);
	if (retval != OK) {
		debug("conf_table_parse() failed");
		return FAIL;
	}

	retval = ratt_table_create(&l_modtable,
	    l_conf_modtable_size, sizeof(rattmod_entry_t), 0);
	if (retval != OK) {
		if (l_conf_modtable_size < RATTTAB_MINSIZ)
			error("module/table-size-init is too low");
		debug("ratt_table_create() failed");
		conf_table_release(l_conftable);
		return FAIL;
	} else
		debug("allocated module table of size `%u'",
		    l_conf_modtable_size);

	retval = ratt_table_create(&l_partable,
	    MODULE_PARENT_TABLESIZ, sizeof(module_parent_t), 0);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		ratt_table_destroy(&l_modtable);
		conf_table_release(l_conftable);
		return FAIL;
	} else
		debug("allocated parent table of size `%u'",
		    MODULE_PARENT_TABLESIZ);

	/* load available modules */
	load_modules();

	retval = dtor_register(module_fini, NULL);
	if (retval != OK) {
		debug("dtor_register() failed");
		unload_modules();
		ratt_table_destroy(&l_modtable);
		conf_table_release(l_conftable);
		return FAIL;
	}

	return OK;
}
