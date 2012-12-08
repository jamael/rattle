/*
 * RATTLE debug trace module
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
 *
 */


#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>

#include <rattle/core/debug.h>

#define MODULE_NAME	RATT_DEBUG_NAME "_trace"
#define MODULE_DESC	"debugger backtrace"
#define MODULE_VERSION	"1.0"

static int on_fork(unsigned int flags)
{
	return OK;
}

static void detach_module(void)
{
}

static int attach_module(ratt_module_hook_t *hookinfo)
{
	ratt_debug_hook_t *debug_hook = hookinfo->hook;
	switch (hookinfo->version) {
	default:
		hookinfo->version = 1;
	case 1:
		(*debug_hook).v1.on_fork = on_fork;
		break;
	}
	return OK;
}

static ratt_module_entry_t module_entry = {
	.name = MODULE_NAME,
	.desc = MODULE_DESC,
	.version = MODULE_VERSION,
	.attach = attach_module,
	.detach = detach_module,
	.hook_size = RATT_DEBUG_HOOK_SIZE,
};

RATT_MODULE_INIT(debug_trace, &module_entry)
