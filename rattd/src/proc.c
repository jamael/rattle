/*
 * RATTLE processor module parent
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


#include <config.h>

#include <signal.h>
#include <stdint.h>

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/proc.h>

#include "conf.h"
#include "dtor.h"
#include "module.h"
#include "signal.h"

/* attached process module */
static rattmod_entry_t *l_proc_module = NULL;
static rattproc_callback_t *l_callbacks = NULL;

/* name of the parent config declaration */
#define PROC_CONF_NAME	"process"

#ifndef RATTD_PROC_MODULE
#define RATTD_PROC_MODULE "serial"
#endif
static RATTCONF_DEFVAL(l_conf_module_defval, RATTD_PROC_MODULE);
static RATTCONF_LIST_INIT(l_conf_module);

static rattconf_decl_t l_conftable[] = {
	{ "module", "process module to use. first load, first use.",
	    l_conf_module_defval, &l_conf_module,
	    RATTCONFDTSTR, RATTCONFFLLST },
	{ NULL }
};

static int attach_module(rattmod_entry_t *modentry)
{
	/* process does not support module chaining */
	if (l_proc_module) {
		debug("module `%s' attached first, reject `%s'",
		    l_proc_module->name, modentry->name);
		return FAIL;
	}

	l_proc_module = modentry;
	l_callbacks = modentry->callbacks;
	debug("attached callbacks for `%s'", modentry->name);

	return OK;
}

static inline int on_start()
{
	if (l_callbacks && l_callbacks->on_start)
		return l_callbacks->on_start();
	debug("on_start() undefined");
	return FAIL;
}

static inline int on_stop()
{
	if (l_callbacks && l_callbacks->on_stop)
		return l_callbacks->on_stop();
	debug("on_stop() undefined");
	return FAIL;
}

static void on_interrupt(int signum, siginfo_t const *siginfo, void *udata)
{
	if (l_callbacks && l_callbacks->on_interrupt) {
		l_callbacks->on_interrupt(signum, siginfo, udata);
	} else
		debug("on_interrupt() undefined");
}

static void on_unregister(void (*process)(void *))
{
	if (l_callbacks && l_callbacks->on_unregister) {
		l_callbacks->on_unregister(process);
	} else
		debug("on_unregister() undefined");
}

static int on_register(void (*process)(void *), uint32_t flags, void *udata)
{
	if (l_callbacks && l_callbacks->on_register)
		return l_callbacks->on_register(process, flags, udata);
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

void proc_fini(void *udata)
{
	RATTLOG_TRACE();
	l_proc_module = NULL;
	l_callbacks = NULL;
	module_parent_detach(RATTPROC);
	signal_unregister(&on_interrupt);
	conf_release(l_conftable);
}

int proc_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = conf_parse(PROC_CONF_NAME, l_conftable);
	if (retval != OK) {
		debug("conf_parse() failed");
		return FAIL;
	}

	retval = signal_register(SIGINT, &on_interrupt, NULL);
	if (retval != OK) {
		debug("signal_register() failed");
		return FAIL;
	}

	retval = module_parent_attach(RATTPROC, &attach_module);
	if (retval != OK) {
		debug("module_parent_attach() failed");
		signal_unregister(&on_interrupt);
		conf_release(l_conftable);
		return FAIL;
	}

	retval = rattmod_attach_from_config(RATTPROC, &l_conf_module);
	if (retval != OK) {
		debug("rattmod_attach_from_config() failed");
		error("no processor module attached");
		module_parent_detach(RATTPROC);
		signal_unregister(&on_interrupt);
		conf_release(l_conftable);
		return FAIL;
	}

	retval = dtor_register(proc_fini, NULL);
	if (retval != OK) {
		debug("proc_register() failed");
		module_parent_detach(RATTPROC);
		signal_unregister(&on_interrupt);
		conf_release(l_conftable);
		return FAIL;
	}

	return OK;
}

void rattproc_unregister(int (*process)(void *))
{
	RATTLOG_TRACE();
	on_unregister(process);
}

int rattproc_register(int (*process)(void *), uint32_t flags, void *udata)
{
	RATTLOG_TRACE();
	return on_register(process, flags, udata);
}
