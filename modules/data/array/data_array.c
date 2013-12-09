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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <rattle.h>
#include <stdlib.h>
#include <string.h>


/*
 * The following macro defines the bit flag mathematics
 * used to compute the table->frag_mask member which is laid out
 * on a dynamically allocated array of 8bits slot.
 *
 * A frag_mask bit toggled on means there is fragmentation at
 * the corresponding table->pos which is zero-based.
 *
 * frag_mask_size macro gives the size of a (x) flags array
 * fsbset() macro finds the first significant bit set in the slot
 * shift_mask() macro computes the bit flags position in the slot
 * shift_mask_slot() macro computes the slot position in the array
 *
 * Note: fsbset() uses ffs() to take advantages of hardware support
 *       for find first bit set operation *but* ffs() is one-based.
 *       Thus, fsbset() substracts one so that results are zero-based.
 *
 *       Care must be taken to verify that bits are set before using
 *       fsbset() as ffs() uses return value of 0 to indicate that
 *       no bit is set.
 */
#define frag_mask_size(x) ((1 + ((x) / 8)) * sizeof(uint8_t))
#define fsbset(x) (ffs((x)) - 1)
#define shift_mask(pos) (1 << ((pos) & 7))
#define shift_mask_slot(head, pos)			\
	do {						\
		if ((head)) {				\
			(head) += (pos) / 8;		\
		}					\
	} while (0)

static inline int is_frag(uint8_t const *slot, size_t pos)
{
	shift_mask_slot(slot, pos);
	return (*slot & shift_mask(pos));
}

static inline int frag_mask_set(uint8_t *slot, size_t pos, size_t *cnt)
{
	uint8_t bitmask = shift_mask(pos);

	shift_mask_slot(slot, pos);
	if (*slot & bitmask) {
		debug("bitmask 0x%x already set in mask slot", bitmask);
		return FAIL;
	}

	*slot |= bitmask;
	(*cnt)++;

	return OK;
}

static inline int frag_mask_unset(uint8_t *slot, size_t pos, size_t *cnt)
{
	uint8_t bitmask = shift_mask(pos);

	shift_mask_slot(slot, pos);
	if (!(*slot & bitmask)) {
		debug("bitmask 0x%x absent from mask slot", bitmask);
		return FAIL;
	}

	*slot &= ~bitmask;
	(*cnt)--;

	return OK;
}

static inline int write_chunk(ratt_table_t *table, void const *src,
                              int (*getdst)(ratt_table_t *, void **))
{
	void *dst = NULL, *curr = NULL;
	int retval;

	if (!getdst) {
		debug("getdst is NULL, using get_tail_next");
		getdst = ratt_table_get_tail_next;
	}

	retval = ratt_table_satisfy_constrains(table, src);
	if (retval != OK && !table->on_constrains) {
		debug("ratt_table_satisfy_constrains() failed");
		return FAIL;
	} else if (retval != OK) {
		curr = ratt_table_current(table);
		if (!curr) {
			debug("ratt_table_current() failed");
			return FAIL;
		}
		retval = table->on_constrains(curr, src);
		if (retval != OK) {
			debug("table->on_constrains() failed");
			return FAIL;
		}
		debug("constrains have been resolved");
		return OK;
	}

	retval = getdst(table, &dst);
	if (retval != OK) {
		debug("getdst() failed");
		return FAIL;
	}

	memcpy(dst, src, table->chunk_size);

	debug("chunk at %p written to %p, slot %u", src, dst,
	    ratt_table_pos_current(table));
	return OK;
}

static int realloc_and_move(ratt_table_t *table)
{
	void *head = NULL;
	uint8_t *frag_mask = NULL;
	size_t newsiz = 0, growsiz = 0;

	growsiz = table->size / 2;
	if (!growsiz) /* cannot be 0 */
		growsiz = 1;

	if ((table->size + growsiz) > RATTTABMAXSIZ) {
		debug("increment is over maximum size (%u)", RATTTABMAXSIZ);
		newsiz = RATTTABMAXSIZ;
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

	frag_mask = realloc(table->frag_mask, frag_mask_size(newsiz));
	if (!frag_mask) {
		error("memory allocation failed");
		debug("realloc() failed");
		return FAIL;
	}

	debug("reallocated frag_mask at %p", frag_mask);
	table->frag_mask = frag_mask;
	shift_mask_slot(frag_mask, table->last);
	frag_mask++;	/* entering uninitialized memory */
	memset(frag_mask, 0,
	    frag_mask_size(newsiz) - frag_mask_size(table->size));

	if (table->head != head) {	/* realloc moved it, recompute */
		debug("head is now at %p, was %p", head, table->head);
		table->head = head;
		table->tail = (char *) head
		    + (table->last * table->chunk_size);
	}

	memset((char *) table->tail + table->chunk_size, 0,
	    (newsiz - table->size) * table->chunk_size);
	table->size = newsiz;

	return OK;
}

int ratt_table_pos_isfrag(ratt_table_t *table, size_t pos)
{
	uint8_t const *slot = table->frag_mask;
	if (!ratt_table_isempty(table)
	    && pos <= table->last)
		shift_mask_slot(slot, pos);
		return (*slot & shift_mask(pos));
	return 0;
}

void *ratt_table_prev(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)) {
		while (table->pos) {
			table->pos--;

			if (ratt_table_fragmented(table)
			    && ratt_table_pos_isfrag(table, table->pos))
				continue;

			return (char *) table->head
			    + (table->pos * table->chunk_size);
		}
	}

	return NULL;
}

void *ratt_table_next(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)) {
		while ((table->pos + 1) <= table->last) {
			table->pos++;
			if (ratt_table_fragmented(table)
			    && ratt_table_pos_isfrag(table, table->pos))
				continue;

			return (char *) table->head
			    + (table->pos * table->chunk_size);
		}
	}
	return NULL;
}

void *ratt_table_first_next(ratt_table_t *table)
{
	void *chunk = NULL;
	if ((chunk = ratt_table_first(table)) != NULL) {
		if (!ratt_table_fragmented(table)
		    || !ratt_table_pos_isfrag(table, table->pos))
			return chunk;
	}
	return ratt_table_next(table);
}

void *ratt_table_circular_next(ratt_table_t *table)
{
	void *chunk = NULL;
	if ((chunk = ratt_table_next(table)) != NULL) {
		return chunk;
	}
	return ratt_table_first_next(table);
}

int ratt_table_search(ratt_table_t *table, void **retchunk,
                      int (*comp)(void const *, void const *),
                      void const *compdata)
{
	RATTLOG_TRACE();
	void *chunk = NULL;

	chunk = ratt_table_current(table);
	if (chunk && (comp(chunk, compdata) == MATCH)) {
		*retchunk = chunk;
		return OK;
	} else
		RATT_TABLE_FOREACH(table, chunk)
		{
			if (chunk && (comp(chunk, compdata) == MATCH)) {
				*retchunk = chunk;
				return OK;
			}
		}

	*retchunk = NULL;
	return FAIL;
}

int ratt_table_satisfy_constrains(ratt_table_t *table, void const *chunk)
{
	RATTLOG_TRACE();
	void *match = NULL;

	if (!table->constrains)	/* table has no constrain */
		return OK;

	ratt_table_search(table, &match, table->constrains, chunk);
	if (!match)	/* constrains did not match */
		return OK;

	return FAIL;
}

int ratt_table_set_pos_frag_first(ratt_table_t *table)
{
	RATTLOG_TRACE();
	uint8_t *slot, *slot_last = table->frag_mask;
	size_t i, pos = 0;

	if (!ratt_table_fragmented(table)) {
		debug("table is not fragmented");
		return FAIL;
	}

	shift_mask_slot(slot_last, table->last);
	for (i = 0, slot = table->frag_mask; slot <= slot_last; slot++, i++) {
		if (*slot) {
			pos = fsbset(*slot);
			debug("fsbset() gives %u in slot_mask(%u)", pos, i);
			break;
		}
	}

	table->pos = pos + i * 8;

	return OK;
}

int ratt_table_del_current(ratt_table_t *table)
{
	RATTLOG_TRACE();
	void *chunk = NULL;

	chunk = ratt_table_current(table);
	if (!chunk) {
		debug("ratt_table_current() failed");
		return FAIL;
	} else if (is_frag(table->frag_mask, table->pos)) {
		debug("chunk at %p already freed", chunk);
		return FAIL;
	}

	debug("deleting chunk at %p", chunk);
	memset(chunk, 0, table->chunk_size);
	table->chunk_count--;

	/* if chunk is the tail, move the tail back */
	if (ratt_table_istail(table, chunk)
	    && !ratt_table_ishead(table, chunk)) {
		table->tail = (char *) chunk - table->chunk_size;
		table->pos = --table->last;
		debug("moved tail back to %p", table->tail);
	} else	/* handle fragmentation */
		frag_mask_set(table->frag_mask,
		    table->pos, &(table->frag_count));

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
		if (table->flags & RATTTABFLNRA) { /* forbid realloc */
			debug("table is full with %i chunks",
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
		next = (char *) table->tail + table->chunk_size;
		table->pos = ++(table->last);	/* push resets position */
	} else
		next = table->head;

	table->chunk_count++;
	*tail = table->tail = next;

	return OK;
}

int ratt_table_get_frag_first(ratt_table_t *table, void **next)
{
	RATTLOG_TRACE();
	int retval;

	if ((!ratt_table_fragmented(table))	/* table not fragmented */
	    || (table->flags & RATTTABFLNRU))	/* forbid fragment reuse */
		return ratt_table_get_tail_next(table, next);

	retval = ratt_table_set_pos_frag_first(table);
	if (retval != OK) {
		debug("ratt_table_set_pos_frag_first() failed");
		return FAIL;
	}

	*next = ratt_table_current(table);
	if (!(*next)) {
		debug("ratt_table_current() failed");
		return FAIL;
	}

	retval = frag_mask_unset(table->frag_mask,
	    table->pos, &(table->frag_count));
	if (retval != OK) {
		debug("frag_mask_unset() failed");
		return FAIL;
	}

	table->chunk_count++;

	return OK;
}

int ratt_table_push(ratt_table_t *table, void const *chunk)
{
	RATTLOG_TRACE();
	return write_chunk(table, chunk, ratt_table_get_tail_next);
}

int ratt_table_insert(ratt_table_t *table, void const *chunk)
{
	RATTLOG_TRACE();
	return write_chunk(table, chunk, ratt_table_get_frag_first);
}

int ratt_table_destroy(ratt_table_t *table)
{
	RATTLOG_TRACE();
	if (table && table->head) {
		if (table->frag_mask) {
			debug("freeing frag_mask at %p", table->frag_mask);
			free(table->frag_mask);
		}
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

	if (ratt_table_exists(table)) {
		debug("table at %p exists already", table);
		return FAIL;
	} else if (cnt < RATTTABMINSIZ) {
		debug("asked for size %u when minimum is %u",
		    cnt, RATTTABMINSIZ);
		return FAIL;
	} else if (cnt > RATTTABMAXSIZ) {
		debug("asked for size %u when maximum is %u",
		    cnt, RATTTABMAXSIZ);
		return FAIL;
	}

	memset(table, 0, sizeof(ratt_table_t));

	table->head = calloc(cnt, size);
	if (!table->head) {
		error("memory allocation failed");
		debug("calloc() failed");
		return FAIL;
	}

	/* always have frag_mask, even if no reuse flag given */
	table->frag_mask = calloc(1, frag_mask_size(cnt));
	if (!table->frag_mask) {
		error("memory allocation failed");
		debug("calloc() failed");
		free(table->head);
		return FAIL;
	}

	table->tail = table->head;
	table->size = cnt;
	table->chunk_size = size;

	/* table exists now */
	table->flags = RATTTABFLXIS | flags;

	debug("created new table at %p with head at %p", table, table->head);

	return OK;
}
