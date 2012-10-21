/*
 * RATTLE table helper
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <rattle/def.h>
#include <rattle/log.h>
#include <rattle/table.h>

static int realloc_and_move(ratt_table_t *table)
{
	void *head = NULL;
	size_t newsiz = 0, growsiz = 0;

	growsiz = table->size / 2;
	if (!growsiz) /* cannot be 0 */
		growsiz = 1;

	if ((table->size + growsiz) > RATTTAB_MAXSIZ) {
		debug("increment is over maximum size (%u)", RATTTAB_MAXSIZ);
		newsiz = RATTTAB_MAXSIZ;
	} else
		newsiz = table->size + growsiz;

	if (newsiz <= table->size) {
		debug("not growing... %u vs %u", newsiz, table->size);
		return FAIL;
	}

	head = realloc(table->head, newsiz * table->chunk_size);
	if (!head) {
		error("table resize operation failed: %s", strerror(errno));
		debug("realloc() failed");
		return FAIL;
	}

	if (table->head != head) {	/* realloc moved it, recompute */
		debug("head is now at %p, was %p", head, table->head);
		table->head = head;
		table->tail = head + (table->last * table->chunk_size);
	}

	memset(table->tail + table->chunk_size, 0,
	    (newsiz - table->size) * table->chunk_size);
	table->size = newsiz;

	return OK;
}

int ratt_table_search(ratt_table_t *table, void **retchunk,
                      int (*comp)(void const *, void const *),
                      void const *compdata)
{
	RATTLOG_TRACE();
	void *chunk = NULL;

	chunk = ratt_table_get_current(table);
	if (chunk && (comp(chunk, compdata) == OK)) {
		*retchunk = chunk;
		return OK;
	} else
		RATT_TABLE_FOREACH(table, chunk)
		{
			if (chunk && (comp(chunk, compdata) == OK)) {
				*retchunk = chunk;
				return OK;
			}
		}

	return FAIL;
}

int ratt_table_del_current(ratt_table_t *table)
{
	RATTLOG_TRACE();
	void *chunk = NULL;

	chunk = ratt_table_get_current(table);
	if (!chunk) {
		debug("ratt_table_get_current() failed");
		return FAIL;
	}

	debug("deleting chunk at %p", chunk);
	memset(chunk, 0, table->chunk_size);
	table->chunk_count--;

	/* if chunk is the tail, move the tail back */
	if (ratt_table_istail(table, chunk)
	    && !ratt_table_ishead(table, chunk)) {
		table->tail = chunk - table->chunk_size;
		table->last--;
		debug("moved tail back to %p", table->tail);
	}

	return OK;
}

int ratt_table_get_tail_next(ratt_table_t *table, void **tail)
{
	RATTLOG_TRACE();
	void *next = NULL;
	int retval;

#ifdef DEBUG
	size_t oldsiz = 0;
#endif
	if (!ratt_table_isempty(table) && (table->last + 1) >= table->size) {
		if (table->flags & RATTTABFLNRL) { /* forbid realloc */
			warning("table is full with %i chunks",
			    table->last + 1);
			return FAIL;
		}

#ifdef DEBUG
		oldsiz = table->size;
#endif
		retval = realloc_and_move(table);
		if (retval != OK) {
			debug("could not realloc table at %p", table->head);
			return FAIL;
		} else
			debug("table size at %p grew from %u to %u chunks",
			    table->head, oldsiz, table->size);
	}

	if (!ratt_table_isempty(table)) { /* table is not empty */
		next = table->tail + table->chunk_size;
		table->pos = ++(table->last);	/* push resets position */
	} else
		next = table->head;

	table->chunk_count++;
	*tail = table->tail = next;

	return OK;
}

int ratt_table_push(ratt_table_t *table, void const *chunk)
{
	RATTLOG_TRACE();
	void *tail = NULL;
	int retval;

	retval = ratt_table_get_tail_next(table, &tail);
	if (retval != OK) {
		debug("ratt_table_get_tail_next() failed");
		return FAIL;
	}

	memcpy(tail, chunk, table->chunk_size);

	debug("pushed new chunk at %p, slot %u", tail, table->pos);
	return OK;
}

int ratt_table_destroy(ratt_table_t *table)
{
	RATTLOG_TRACE();
	if (table && table->head) {
		debug("freeing table at %p", table->head);
		free(table->head);
		memset(table, 0, sizeof(ratt_table_t));
		return OK;
	}

	debug("table at %p is already freed", table);
	return FAIL;
}

int ratt_table_create(ratt_table_t *table, size_t cnt, size_t size, int flags)
{
	RATTLOG_TRACE();
	void *head = NULL;

	if (cnt < RATTTAB_MINSIZ) {
		warning("initial table size of %u is not enough", cnt);
		debug("asked for size %u when minimum is %u",
		    cnt, RATTTAB_MINSIZ);
		return FAIL;
	} else if (cnt > RATTTAB_MAXSIZ) {
		warning("initial table size of %u is too much", cnt);
		debug("asked for size %u when maximum is %u",
		    cnt, RATTTAB_MAXSIZ);
		return FAIL;
	}

	head = calloc(cnt, size);
	if (!head) {
		error("memory allocation failed");
		debug("calloc() failed");
		return FAIL;
	}

	debug("created new table with head at %p", head);

	memset(table, 0, sizeof(ratt_table_t));
	table->head = table->tail = head;
	table->size = cnt;
	table->chunk_size = size;

	/* table exists now */
	table->flags = RATTTABFLXIS | flags;

	return OK;
}
