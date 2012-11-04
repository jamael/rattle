/*
 * RATTLE table resize (realloc) test
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

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/table.h>
#include <rattle/test.h>

#define MODULE_NAME	RATT_TEST "_table_resize"
#define MODULE_DESC	"table dynamic resize"
#define MODULE_VERSION	"0.1"

#define TABLESIZ	1	/* table initial size */
#define TABLEINS	10000000	/* expected insertions count */

typedef struct {
	size_t insert;		/* number of insertions */
	size_t size;		/* final size of table */
} table_data_t;

static table_data_t l_table_data = { 0 };

static int on_register(ratt_test_data_t *test)
{
	ratt_test_set_udata(test, &l_table_data);
	return OK;
}

static void on_unregister(void *udata)
{
	/* empty */
}

static int on_expect(ratt_test_data_t *test)
{
	table_data_t *data = NULL;
	int retval;

	retval = ratt_test_get_retval(test);
	if (retval == OK) {
		data = ratt_test_get_udata(test);
		if (data->insert == TABLEINS) {
			/* table holds TABLEINS chunks, good. */
			return OK;
		}
	}

	/*
	 * table should have had room for inserting TABLEINS chunks.
	 */

	return FAIL;
}

static int on_run(void *udata)
{
	ratt_table_t mytable;
	size_t insert = 0;
	table_data_t *data = udata;
	int retval = OK;

	ratt_table_create(&mytable, TABLESIZ, sizeof(int), RATTTABFLNRU);

	do {
		debug("inserting %u", insert);
		retval = ratt_table_insert(&mytable, &insert);
		if (retval != OK) {
			debug("ratt_table_insert() failed");
			break;
		}
	} while (++insert < TABLEINS);

	data->insert = insert;
	data->size = ratt_table_size(&mytable);

	ratt_table_destroy(&mytable);

	return retval;
}

static void on_summary(void const *udata)
{
	table_data_t const *data = udata;

	notice("initial size of `%u'; final size of `%u'; `%u' insertions",
	    TABLESIZ, data->size, data->insert);
}

static ratt_test_hook_t test_table_resize_hook = {
	.on_register = &on_register,
	.on_unregister = &on_unregister,
	.on_run = &on_run,
	.on_expect = &on_expect,
	.on_summary = &on_summary,
};

static void *attach_hook(ratt_module_parent_t const *parinfo)
{
	return &test_table_resize_hook;
}

static ratt_module_entry_t module_entry = {
	.name = MODULE_NAME,
	.desc = MODULE_DESC,
	.version = MODULE_VERSION,
	.attach = &attach_hook,
};

void __tests_table_resize(void)
{
	ratt_module_register(&module_entry);
}
