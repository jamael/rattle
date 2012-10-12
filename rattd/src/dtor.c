/*
 * RATTD destructor register
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
#include <stdlib.h>

#include <rattle/conf.h>
#include <rattle/def.h>
#include <rattle/log.h>

#include "conf.h"

#ifndef DTORTABSIZMIN
#define DTORTABSIZMIN 16
#endif

typedef struct dtor_table dtor_table_t;
static struct dtor_table {
	void (*dtor)(void *);		/* destructor pointer */
	void *udata;			/* destructor user data */
} *l_dtortable = NULL;
static int l_dtortable_index = -1;	/* destructor table index */

static uint16_t l_conf_dtortable_size = 0;	/* destructor table size */
static conf_decl_t l_conftable[] = {
	{ "destructor/table-size-init",
	    "set the initial size of the destructor table",
	    .defval.num = 64, .val.num = (long long *)&l_conf_dtortable_size,
	    .datatype = RATTCONFDTNUM16, .flags = RATTCONFFLUNS },
	{ NULL }
};

void dtor_unregister(void (*dtor)(void *udata))
{
	RATTLOG_TRACE();
	dtor_table_t *p = NULL;
	int i;

	/* note that unregister does not free the slot;
	   it just removes the pointer */
	i = l_dtortable_index;
	for (p = &(l_dtortable[i]); i >= 0; p = &(l_dtortable[--i])) {
		if (p->dtor == dtor) {
			p->dtor = NULL;
			p->udata = NULL;
		}
	}
}

int dtor_register(void (*dtor)(void *udata), void *udata)
{
	RATTLOG_TRACE();
	dtor_table_t *p = NULL;
	if ((l_dtortable_index + 1) >= l_conf_dtortable_size) {
		error("destructor table is full with `%i' callbacks",
		    l_dtortable_index + 1);
		notice("you might want to raise destructor/table-size-init");
		debug("XXX: should realloc");
		return FAIL;
	} else if (!dtor) {
		debug("NULL callback given");
		return FAIL;
	}

	p = &(l_dtortable[++l_dtortable_index]);
	p->dtor = dtor;
	p->udata = udata;
	debug("registered destructor %p, slot %i", dtor, l_dtortable_index);
	return OK;
}

void dtor_callback(void)
{
	RATTLOG_TRACE();
	dtor_table_t *p = NULL;
	int i;

	i = l_dtortable_index;
	for (p = &(l_dtortable[i]); i >= 0; p = &(l_dtortable[--i])) {
		if (p->dtor)
			p->dtor(p->udata);
	}
}

void dtor_fini(void *udata)
{
	RATTLOG_TRACE();
	conf_table_release(l_conftable);
	free(l_dtortable);
}

int dtor_init(void)
{
	RATTLOG_TRACE();
	int retval;

	retval = conf_table_parse(l_conftable);
	if (retval != OK) {
		debug("conf_table_parse() failed");
		return FAIL;
	} else if (l_conf_dtortable_size <= DTORTABSIZMIN) {
		error("destructor register of size `%i' "
		    "is clearly not enough", l_conf_dtortable_size);
		return FAIL;
	}

	l_dtortable = calloc(l_conf_dtortable_size, sizeof(dtor_table_t));
	if (l_dtortable) {
		debug("allocated destructor table of size `%u'",
		    l_conf_dtortable_size);
	} else {
		error("memory allocation failed");
		debug("calloc() failed");
		conf_table_release(l_conftable);
		return FAIL;
	}

	retval = dtor_register(dtor_fini, NULL);
	if (retval != OK) {
		error("cannot register destructor");
		debug("dtor_register() failed");
		free(l_dtortable);
		conf_table_release(l_conftable);
		return FAIL;
	}

	return OK;
}
