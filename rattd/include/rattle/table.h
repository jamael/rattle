#ifndef RATTLE_TABLE_H
#define RATTLE_TABLE_H

#include <stdint.h>
#include <rattle/def.h>

#define RATTTABFLXIS	0x1	/* table exists */
#define RATTTABFLNRA	0x2	/* forbid realloc */
#define RATTTABFLNRU	0x4	/* forbid fragment reuse */

/* minimum table size; cannot be lower than 1 */
#ifndef RATTTABMINSIZ
#define RATTTABMINSIZ		1
#endif

/* maximum table size; cannot be higher than SIZE_MAX - 1 */
#ifndef RATTTABMAXSIZ
#define RATTTABMAXSIZ		SIZE_MAX - 1
#endif

typedef struct {
	void *head, *tail;	/* head and tail of table */
	size_t size, pos, last;	/* size and position of table */
	size_t chunk_size;	/* size of a chunk */
	size_t chunk_count;	/* number of chunks */
	size_t chunk_frag;	/* fragmented chunk number */
	uint8_t *frag_mask;	/* fragment mask */
	int flags;		/* table flags */
} ratt_table_t;

static inline size_t ratt_table_pos_last(ratt_table_t *table)
{
	return table->last;
}

static inline size_t ratt_table_pos_current(ratt_table_t *table)
{
	return table->pos;
}

static inline size_t ratt_table_size(ratt_table_t *table)
{
	return table->size;
}

static inline size_t ratt_table_count(ratt_table_t *table)
{
	return table->chunk_count;
}

static inline size_t ratt_table_frag_count(ratt_table_t *table)
{
	return table->chunk_frag;
}

static inline int ratt_table_exists(ratt_table_t *table)
{
	return (table->flags & RATTTABFLXIS);
}

static inline int ratt_table_isempty(ratt_table_t *table)
{
	/* true if table does not exist yet or no chunk pushed yet */
	return ((!ratt_table_exists(table)) || !table->chunk_count);
}

static inline int ratt_table_ishead(ratt_table_t *table, void *chunk)
{
	return (ratt_table_exists(table) && (chunk == table->head));
}

static inline int ratt_table_istail(ratt_table_t *table, void *chunk)
{
	return (ratt_table_exists(table) && (chunk == table->tail));
}

static inline int ratt_table_isfrag(ratt_table_t *table)
{
	return (table->chunk_frag);
}

static inline void *ratt_table_get_first(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)) {
		table->pos = 0;
		return table->head;
	}

	return NULL;
}

static inline void *ratt_table_get_last(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)) {
		table->pos = table->last;
		return table->tail;
	}

	return NULL;
}

static inline void *ratt_table_get_next(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)
	    && (table->pos + 1) <= table->last) {
		table->pos++;
		return table->head + (table->pos * table->chunk_size);
	}

	return NULL;
}

static inline void *ratt_table_get_current(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)
	    && table->pos <= table->last) {
		return table->head + (table->pos * table->chunk_size);
	}

	return NULL;
}

#define RATT_TABLE_FOREACH(tab, chunk) \
	for ((chunk) = (tab)->head, (tab)->pos = 0; \
	    (!ratt_table_isempty((tab)) && (void *) (chunk) <= (tab)->tail); \
	    ((tab)->pos)++, (chunk) = (void *) (chunk) + (tab)->chunk_size)

#define RATT_TABLE_FOREACH_REVERSE(tab, chunk) \
	for ((chunk) = (tab)->tail, (tab)->pos = (tab)->last; \
	    (!ratt_table_isempty((tab)) && (void *) (chunk) >= (tab)->head); \
	    ((tab)->pos)--, (chunk) = (void *) (chunk) - (tab)->chunk_size)

#define RATT_TABLE_INIT(tab) ratt_table_t (tab) = { 0 }

extern int ratt_table_create(ratt_table_t *, size_t, size_t, int);
extern int ratt_table_destroy(ratt_table_t *);
extern int ratt_table_push(ratt_table_t *, void const *);
extern int ratt_table_insert(ratt_table_t *, void const *);
extern int ratt_table_search(ratt_table_t *, void **,
    int (*)(void const *, void const *), void const *);
extern int ratt_table_get_tail_next(ratt_table_t *, void **);
extern int ratt_table_get_frag_first(ratt_table_t *, void **);
extern int ratt_table_del_current(ratt_table_t *);
extern int ratt_table_set_pos_frag_first(ratt_table_t *);

#endif /* RATTLE_TABLE_H */
