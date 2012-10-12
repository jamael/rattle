/*
 * RATTD module loader
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

#include "conf.h"
#include "dtor.h"

static rattmod_entry_t **l_modtable = NULL;
static int l_modtable_index = -1;	/* module table index */

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
	rattmod_entry_t *entry = l_modtable[l_modtable_index];

	if (l_modtable_index < 0)
		return FAIL;	/* until table api */

	if (!handle) {
		debug("NULL handle given");
		return FAIL;
	} else if (entry && entry->handle) {
		debug("entry already got a handle");
		return FAIL;
	} else
		entry->handle = handle;
	return OK;
}

static int unload_modules()
{
	rattmod_entry_t *entry = NULL;
	int i;

	if (l_modtable_index < 0)
		return FAIL;	/* until table api */

	i = l_modtable_index;
	for (entry = l_modtable[i]; i >= 0; entry = l_modtable[--i])
		if (entry->handle)
			dlclose(entry->handle);
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

int rattmod_register(rattmod_entry_t *entry)
{
	rattmod_entry_t **next = NULL;
	if ((l_modtable_index + 1) >= l_conf_modtable_size) {
		error("module table is full with `%i' modules",
		    l_modtable_index + 1);
		notice("you might want to raise `module/table-size-init'");
		debug("XXX: should realloc");
		return FAIL;
	} else if (!entry) {
		debug("NULL entry given");
		return FAIL;
	}

	next = &(l_modtable[++l_modtable_index]);
	if (!next) {
		debug("could not get next slot");
		return FAIL;
	}

	*next = entry;
	debug("registered module entry %p, slot %i", entry, l_modtable_index);

	return OK;
}



void module_fini(void *udata)
{
	RATTLOG_TRACE();
	unload_modules();
	free(l_modtable);
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

	l_modtable = calloc(l_conf_modtable_size, sizeof(rattmod_entry_t *));
	if (l_modtable) {
		debug("allocated module table of size `%u'",
		    l_conf_modtable_size);
	} else {
		error("memory allocation failed");
		debug("calloc() failed");
		conf_table_release(l_conftable);
		return FAIL;
	}

	/* load available modules */
	load_modules();

	retval = dtor_register(module_fini, NULL);
	if (retval != OK) {
		debug("dtor_register() failed");
		unload_modules();
		free(l_modtable);
		conf_table_release(l_conftable);
		return FAIL;
	}

	return OK;
}
