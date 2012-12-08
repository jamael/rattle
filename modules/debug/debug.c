/*
 * RATTLE debugger core
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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>

#include <rattle/core/debug.h>

#include "conf.h"
#include "module.h"

/* globals */
extern int g_main_args_debug_enable;

/* configuration */
#define DEBUG_CONF_LABEL "debug"

#ifndef RATTD_DEBUG_MODULE
#define RATTD_DEBUG_MODULE "backtrace"
#endif
static RATT_CONF_DEFVAL(l_conf_module_defval, RATTD_DEBUG_MODULE);
static RATT_CONF_LIST_INIT(l_conf_module);

static ratt_conf_t l_conf[] = {
	{ "module", "debugger modules to use",
	    l_conf_module_defval, &l_conf_module,
	    RATTCONFDTSTR, RATTCONFFLLST },
	{ NULL }
};

/* process stuff */
static int l_pipe_fd[2] =  { 0 };
static int l_process_is_child = 0;
static pid_t l_process_id_of_child = 0;

/* core information */
RATT_TABLE_INIT(l_hooktab);
static ratt_module_core_t l_core_info = {
	.name = RATT_DEBUG_NAME,
	.ver_major = RATT_DEBUG_VER_MAJOR,
	.ver_minor = RATT_DEBUG_VER_MINOR,
	.ver_patch = RATT_DEBUG_VER_PATCH,
	.conf_label = DEBUG_CONF_LABEL,
	.hook_table = &l_hooktab,
	.hook_size = RATT_DEBUG_HOOK_SIZE,
	.hook_ver = RATT_DEBUG_HOOK_VERSION,
};

static inline int on_pipe_read()
{
	return FAIL;
}

static void destroy_pipe(void)
{
	int *fd = &(l_pipe_fd[1]);
	while (fd-- >= l_pipe_fd) {
		if (*fd >= 0) {
			close(*fd);
			*fd = -1;
		}
	}
}

static int create_pipe(void)
{
	if (pipe(l_pipe_fd) < 0) {
		debug("pipe() failed");
		return FAIL;
	}

	return OK;
}

static int fork_process(void)
{
	pid_t pid = 0;

	pid = fork();
	if (pid < 0) {			/* fork failed */
		debug("fork() failed");
		return FAIL;
	} else if (!pid) {		/* child process */
		close(l_pipe_fd[1]);
		l_pipe_fd[1] = -1;
		l_process_is_child = 1;
	} else {			/* core process */
		close(l_pipe_fd[0]);
		l_pipe_fd[0] = -1;
		l_process_id_of_child = pid;
	}
	return OK;
}

int debug_write(void)
{
	/* child process only allowed to read */
	if (l_process_is_child)
		return FAIL;

	debug("crashed process about to write to debugger");
	return OK;
}

void debug_fini(void *udata)
{
	RATTLOG_TRACE();
	int status, retval;

	if (l_process_is_child) {
		debug("debugger is going down");
	} else {
		retval = waitpid(l_process_id_of_child, &status, 0);
		if (retval < 0) {
			debug("waitpid() failed: %s", strerror(errno));
		} else
			debug("waitpid() status: %i", status);
	}

	destroy_pipe();
	module_core_detach(RATT_DEBUG_NAME);
	conf_release(l_conf);
}

int debug_init(void)
{
	RATTLOG_TRACE();
	char **module = NULL;
	int retval;

	if (!g_main_args_debug_enable)
		return NOSTART;

	retval = conf_parse(DEBUG_CONF_LABEL, l_conf);
	if (retval != OK) {
		debug("conf_parse() failed");
		return FAIL;
	}

	retval = module_core_attach(&l_core_info);
	if (retval != OK) {
		debug("module_core_attach() failed");
		conf_release(l_conf);
		return FAIL;
	}

	RATT_CONF_LIST_FOREACH(&l_conf_module, module)
	{
		ratt_module_attach(&l_core_info, *module);
	}

	if (ratt_table_isempty(&l_hooktab)) {
		warning("none of the debug modules attached");
		module_core_detach(RATT_DEBUG_NAME);
		conf_release(l_conf);
		return FAIL;
	}

	if (create_pipe() != OK) {
		debug("create_pipe() failed");
		module_core_detach(RATT_DEBUG_NAME);
		conf_release(l_conf);
		return FAIL;
	}

	if (fork_process() != OK) {
		debug("fork_process() failed");
		destroy_pipe();
		module_core_detach(RATT_DEBUG_NAME);
		conf_release(l_conf);
		return FAIL;
	}

	return OK;
}
