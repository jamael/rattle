/*
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


#include <dlfcn.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>

#ifndef RATTD_VERSION
#define RATTD_VERSION "0.1"
#endif

#ifndef CONFFILEPATH
#define CONFFILEPATH "/etc/rattle/rattd.conf"
#endif

static char l_conffile[PATH_MAX] = { '\0' };

static int parse_argv_opts(int argc, char * const argv[])
{
	int c;

	while ((c = getopt(argc, argv, "f:")) != -1)
	{
		switch (c)
		{
			case 'f':	/* explicit config file */
				if (strlen(optarg) >= PATH_MAX) {
					error("%s: path is too long", optarg);
					return FAIL;
				} else {
					strcpy(l_conffile, optarg);
					l_conffile[PATH_MAX-1] = '\0';
				}
				break;
		}
	}

	return OK;
}

static int load_config()
{
	size_t n;
	if (*l_conffile == '\0') {
		if ((n = strlen(CONFFILEPATH)) >= PATH_MAX) {
			error("default configuration path is too long");
			debug("CONFFILEPATH exceeds PATH_MAX by %i byte(s)",
			    n - PATH_MAX + 1);
			return FAIL;
		}
		strcpy(l_conffile, CONFFILEPATH);
		l_conffile[PATH_MAX-1] = '\0';
	}

	notice("configuring from `%s'", l_conffile);

	/* ratt_conf_load here */
	return OK;
}

static void show_startup_notice()
{
	fprintf(stdout,
	    "The RATTLE daemon is waking up!\n"
	    "rattd version %s\n"
	    "\n", RATTD_VERSION);
}

int main(int argc, char * const argv[])
{
	void *ptr;

	show_startup_notice();

	if (parse_argv_opts(argc, argv) != OK) {
		debug("parse_argv_opts() failed");
		exit(1);
	}

	if (load_config() != OK) {
		debug("load_config() failed");
		exit(1);
	}

//	ptr = dlopen("/usr/local/lib/rattd/hello.so", RTLD_NOW);

	return 0;
}
