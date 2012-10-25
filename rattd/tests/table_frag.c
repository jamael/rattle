/*
 * RATTLE table fragmentation test
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

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/module.h>
#include <rattle/table.h>
#include <rattle/test.h>

#define MODULE_NAME	RATT_TEST "_table_frag"
#define MODULE_DESC	"table fragmentation"
#define MODULE_VERSION	"0.1"

#define TABLESIZ	128

typedef struct {
	size_t delete;		/* number of deletions */
	size_t insert;		/* number of insertions */
	size_t pos;		/* last position */
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
	table_data_t *test_data = NULL;
	int retval;

	retval = ratt_test_get_retval(test);
	if (retval == FAIL) {	/* that is expected */
		test_data = ratt_test_get_udata(test);
		if (test_data->pos == TABLESIZ) {
			/* table was full */
			return OK;
		}
	}

	debug("retval is %i, pos at %u", retval, test_data->pos);

	/*
	 * table should have filled up to the maximum it can hold
	 * before failing.
	 */

	return FAIL;
}

static int on_run(void *udata)
{
	ratt_table_t mytable;
	size_t *chunk, insert = 0, delete = 0;
	table_data_t *test_data = udata;
	int retval, do_del = 0;

	ratt_table_create(&mytable, TABLESIZ, sizeof(size_t), RATTTABFLNRA);

	do {
		do_del ^= 1;
		if (insert > 8 && insert & 3 && do_del) {
			mytable.pos = mytable.pos / (insert & 3);
			chunk = ratt_table_get_current(&mytable);
			debug("chunk %u holds %u, deleting",
			    mytable.pos, *chunk);
			retval = ratt_table_del_current(&mytable);
			if (retval != OK) {
				debug("ratt_table_del_current() failed");
			} else
				delete++;
		} else {
			insert++;
			debug("inserting %u", insert);
			retval = ratt_table_insert(&mytable, &insert);
		}
	} while (retval == OK);

	test_data->delete = delete;
	test_data->insert = insert;
	test_data->pos = ratt_table_pos_current(&mytable);

	ratt_table_destroy(&mytable);

	return FAIL;
}

static void on_summary(void const *udata)
{
	/* empty */
}

static ratt_test_callback_t test_callbacks = {
	.on_register = &on_register,
	.on_unregister = &on_unregister,
	.on_run = &on_run,
	.on_expect = &on_expect,
	.on_summary = &on_summary,
};

static rattmod_entry_t module_entry = {
	.name = MODULE_NAME,
	.desc = MODULE_DESC,
	.version = MODULE_VERSION,
	.callbacks = &test_callbacks
};

void __attribute__ ((destructor)) __tests_table_frag_fini(void)
{
	rattmod_unregister(&module_entry);
}

void __tests_table_frag(void)
{
	rattmod_register(&module_entry);
}
