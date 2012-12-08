/*
 * RATTLE destructor register
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
#include <stdlib.h>

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/table.h>

typedef struct {
	void (*dtor)(void *);		/* destructor pointer */
	void *udata;			/* destructor user data */
} dtor_register_t;

/* destructor table initial size */
#ifndef DTOR_DTORTABSIZ
#define DTOR_DTORTABSIZ	4
#endif
static RATT_TABLE_INIT(l_dtortab);	/* destructor table */

void dtor_unregister(void (*dtor)(void *udata))
{
	RATTLOG_TRACE();
	dtor_register_t *entry = NULL;

	RATT_TABLE_FOREACH(&l_dtortab, entry)
	{
		if (entry->dtor == dtor)
			entry->dtor = entry->udata = NULL;
	}
}

int dtor_register(void (*dtor)(void *udata), void *udata)
{
	RATTLOG_TRACE();
	dtor_register_t entry = { dtor, udata };
	int retval;

	retval = ratt_table_push(&l_dtortab, &entry);
	if (retval != OK) {
		debug("ratt_table_push() failed");
		return FAIL;
	}

	debug("registered destructor %p, slot %i",
	    dtor, ratt_table_pos_last(&l_dtortab));
	return OK;
}

void dtor_callback(void)
{
	RATTLOG_TRACE();
	dtor_register_t *entry = NULL;

	RATT_TABLE_FOREACH_REVERSE(&l_dtortab, entry)
	{
		if (entry->dtor)
			entry->dtor(entry->udata);
	}
}

void dtor_fini(void *udata)
{
	RATTLOG_TRACE();
	ratt_table_destroy(&l_dtortab);
}

int dtor_init(void)
{
	RATTLOG_TRACE();
	int retval;
	
	retval = ratt_table_create(&l_dtortab,
	    DTOR_DTORTABSIZ, sizeof(dtor_register_t), RATTTABFLNRU);
	if (retval != OK) {
		debug("ratt_table_create() failed");
		return FAIL;
	} else
		debug("allocated destructor table of size `%u'",
		    DTOR_DTORTABSIZ);

	retval = dtor_register(dtor_fini, NULL);
	if (retval != OK) {
		debug("dtor_register() failed");
		ratt_table_destroy(&l_dtortab);
		return FAIL;
	}

	return OK;
}
