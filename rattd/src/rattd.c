/*
 * The RATTLE daemon
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

#include <stdint.h>

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>

#include "conf.h"
#include "dtor.h"

#ifndef RATTD_LISTEN
#define RATTD_LISTEN "any"
#endif
static RATTCONF_DEFVAL(l_conf_listen_defval, RATTD_LISTEN);
static RATTCONF_LIST_INIT(l_conf_listen);

#ifndef RATTD_PROTO
#define RATTD_PROTO "udp", "tcp"
#endif
static RATTCONF_DEFVAL(l_conf_proto_defval, RATTD_PROTO);
static RATTCONF_LIST_INIT(l_conf_proto);

#ifndef RATTD_PORT
#define RATTD_PORT "2194"
#endif
static RATTCONF_DEFVAL(l_conf_port_defval, RATTD_PORT);
static RATTCONF_LIST_INIT(l_conf_port);

static conf_decl_t l_conftable[] = {
	{ "listen", "IP, FQDN, interfaces to listen on or `any' for all",
	    l_conf_listen_defval, &l_conf_listen,
	    RATTCONFDTSTR, RATTCONFFLLST },
	{ "proto", "use the specified transport protocols",
	    l_conf_proto_defval, &l_conf_proto,
	    RATTCONFDTSTR, RATTCONFFLLST },
	{ "port", "listen to the specified ports",
	    l_conf_port_defval, &l_conf_port,
	    RATTCONFDTNUM16, RATTCONFFLLST|RATTCONFFLUNS },
	{ NULL }
};

void rattd_fini(void *udata)
{
	RATTLOG_TRACE();
	conf_release(l_conftable);
}

int rattd_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = conf_parse(NULL, l_conftable);
	if (retval != OK) {
		debug("conf_parse() failed");
		return FAIL;
	}

	retval = dtor_register(rattd_fini, NULL);
	if (retval != OK) {
		debug("dtor_register() failed");
		rattd_fini(NULL);
		return FAIL;
	}

	return OK;
}
