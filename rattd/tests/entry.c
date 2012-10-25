/*
 * RATTLE tests library entry
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

#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <rattle/log.h>
#include <rattle/test.h>

#define FUNNAMEMAX	128

static char const *tests_ar_entry[] = {
	/* category, test name, ..., \0 */
	"table", "table_frag", '\0',
	'\0'	/* end of array */
};

static void *l_program_handle = NULL;

static void entry_foreach(char const *category,
                          void (*callback)(char const *),
                          int all_of_them)
{
	char const **entry = tests_ar_entry;
	int found = 0;

	for (;; entry++) {
		if (!found && *entry == '\0') {
			/* end of array */
			break;
		} else if (!found && (!category
		    || (strcmp(category, *entry) == 0))) {
			/* found category */
			found = 1;
		} else if (*entry == '\0' && found) {
			/* end of category */
			if (all_of_them) {
				found = 0;
				continue;
			}
			break;
		} else if (found) {
			callback(*entry);
		}
	}
}

static void entry_init(char const *entry)
{
	char init_func_name[FUNNAMEMAX] = { '\0' };
	void (*init)(void) = NULL;

	snprintf(init_func_name, FUNNAMEMAX, "__tests_%s", entry);
	init = dlsym(l_program_handle, init_func_name);
	if (!init) {
		debug("dlsym() failed on `%s'", init_func_name);
		debug("%s", dlerror());
		return;
	}

	debug("trying to initialize `%s'", init_func_name);
	init();
}

void tests_ar_register(char const *category, void (*attach)(char const *))
{
	int all_of_them = 0;

	if (!category)
		all_of_them = 1;

	/* get program's dlopen() handle */
	l_program_handle = dlopen(NULL, RTLD_NOW|RTLD_DEEPBIND);
	if (!l_program_handle) {
		debug("dlopen() failed");
		return;
	}

	/* first pass to register modules */
	entry_foreach(category, entry_init, all_of_them);

	/* second pass to attach them */
	entry_foreach(category, attach, all_of_them);
}
