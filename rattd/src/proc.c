/*
 * RATTLE processor
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

#include <stdint.h>

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/proc.h>

#include "conf.h"
#include "module.h"

/* attached process module */
static ratt_module_entry_t *l_proc_module = NULL;
static ratt_proc_hook_t *l_proc_hook = NULL;

/* name of the parent config declaration */
#define PROC_CONF_NAME	"process"

#ifndef RATTD_PROC_MODULE
#define RATTD_PROC_MODULE "serial"
#endif
static RATTCONF_DEFVAL(l_conf_module_defval, RATTD_PROC_MODULE);
static RATTCONF_LIST_INIT(l_conf_module);

static ratt_conf_t l_conf[] = {
	{ "module", "process module to use. first load, first use.",
	    l_conf_module_defval, &l_conf_module,
	    RATTCONFDTSTR, RATTCONFFLLST },
	{ NULL }
};

static ratt_module_parent_t l_parent_info = {
	RATT_PROC, RATT_PROC_VER_MAJOR, RATT_PROC_VER_MINOR,
};

static int attach_module(ratt_module_entry_t *entry)
{
	ratt_proc_hook_t *hook = NULL;
	int retval;

	/* cannot attach multiple processor */
	if (l_proc_module) {
		debug("module `%s' attached first, reject `%s'",
		    l_proc_module->name, entry->name);
		return FAIL;
	}

	/* first, module may need to check its configuration */
	if (entry->config) {
		retval = conf_parse(PROC_CONF_NAME, entry->config);
		if (retval != OK) {
			debug("conf_parse() failed for `%s'", entry->name);
			return FAIL;
		}
	}

	/* second, let the module decide wheter it attaches or not */
	hook = entry->attach(&l_parent_info);
	if (!hook) {
		debug("`%s' chose not to attach", entry->name);
		return FAIL;
	}

	l_proc_module = entry;
	l_proc_hook = hook;

	debug("`%s' hooked", entry->name);
	return OK;
}

static inline int on_start()
{
	if (l_proc_hook && l_proc_hook->on_start)
		return l_proc_hook->on_start();
	debug("on_start() undefined");
	return FAIL;
}

static inline int on_stop()
{
	if (l_proc_hook && l_proc_hook->on_stop)
		return l_proc_hook->on_stop();
	debug("on_stop() undefined");
	return FAIL;
}

static void
on_unregister(int (*process)(void *), ratt_proc_attr_t *attr, void *udata)
{
	if (l_proc_hook && l_proc_hook->on_unregister) {
		l_proc_hook->on_unregister(process, attr, udata);
	} else
		debug("on_unregister() undefined");
}

static int
on_register(int (*process)(void *), ratt_proc_attr_t *attr, void *udata)
{
	if (l_proc_hook && l_proc_hook->on_register)
		return l_proc_hook->on_register(process, attr, udata);
	debug("on_register() undefined");
	return FAIL;
}

int proc_stop()
{
	RATTLOG_TRACE();
	return on_stop();
}

int proc_start()
{
	RATTLOG_TRACE();
	return on_start();
}

void proc_detach(void *udata)
{
	RATTLOG_TRACE();
	l_proc_module = NULL;
	l_proc_hook = NULL;
	module_parent_detach(&l_parent_info);
}

int proc_attach(void)
{
	int retval;

	retval = module_parent_attach(&l_parent_info, &attach_module);
	if (retval != OK) {
		debug("module_parent_attach() failed");
		return FAIL;
	}

	retval = ratt_module_attach_from_config(RATT_PROC, &l_conf_module);
	if (retval != OK) {
		debug("ratt_module_attach_from_config() failed");
		error("no processor module attached");
		module_parent_detach(&l_parent_info);
		return FAIL;
	}

	return OK;
}

void proc_fini(void *udata)
{
	RATTLOG_TRACE();
	conf_release(l_conf);
}

int proc_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = conf_parse(PROC_CONF_NAME, l_conf);
	if (retval != OK) {
		debug("conf_parse() failed");
		return FAIL;
	}

	return OK;
}

void ratt_proc_unregister(int (*process)(void *),
                          ratt_proc_attr_t *attr,
                          void *udata)
{
	RATTLOG_TRACE();
	on_unregister(process, attr, udata);
}

int ratt_proc_register(int (*process)(void *),
                       ratt_proc_attr_t *attr,
                       void *udata)
{
	RATTLOG_TRACE();
	return on_register(process, attr, udata);
}
