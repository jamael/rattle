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


#include <stdint.h>

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>

#include "conf.h"
#include "dtor.h"

#ifndef RATTD_LISTEN
#define RATTD_LISTEN "any"
#define RATTD_LISTEN_COUNT 1
#elif !defined RATTD_LISTEN_COUNT
#error "RATTD_LISTEN requires RATTD_LISTEN_COUNT"
#endif
static char **l_conf_listen_lst = NULL;
static size_t l_conf_listen_cnt = 0;

#ifndef RATTD_PROTO
#define RATTD_PROTO "udp", "tcp"
#define RATTD_PROTO_COUNT 2
#elif !defined RATTD_PROTO_COUNT
#error "RATTD_PROTO requires RATTD_PROTO_COUNT"
#endif
static char **l_conf_proto_lst = NULL;
static size_t l_conf_proto_cnt = 0;

#ifndef RATTD_PORT
#define RATTD_PORT 2194
#endif
static uint16_t l_conf_port = 0;

static conf_decl_t l_conftable[] = {
	{ "listen",
	    "list of IP or FQDN to listen on",
	    .defval.lst.str = { RATTD_LISTEN },
	    .defval_lstcnt = RATTD_LISTEN_COUNT,
	    .val.lst.str = &l_conf_listen_lst,
	    .val_cnt = &l_conf_listen_cnt,
	    .datatype = RATTCONFDTSTR, .flags = RATTCONFFLLST },
	{ "proto",
	    "use the specified transport protocols",
	    .defval.lst.str = { RATTD_PROTO },
	    .defval_lstcnt = RATTD_PROTO_COUNT,
	    .val.lst.str = &l_conf_proto_lst,
	    .val_cnt = &l_conf_proto_cnt,
	    .datatype = RATTCONFDTSTR, .flags = RATTCONFFLLST },
	{ "port",
	    "bind to the specified port",
	    .defval.num = RATTD_PORT, .val.num = (long long *)&l_conf_port,
	    .datatype = RATTCONFDTNUM16, .flags = RATTCONFFLUNS },
	{ NULL }
};

void rattd_fini(void *udata)
{
	RATTLOG_TRACE();
	conf_table_release(l_conftable);
}

int rattd_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = conf_table_parse(l_conftable);
	if (retval != OK) {
		debug("conf_table_parse() failed");
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
