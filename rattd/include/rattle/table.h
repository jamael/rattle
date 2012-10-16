#ifndef RATTLE_TABLE_H
#define RATTLE_TABLE_H

#include <rattle/def.h>

#define RATTTABFLXIS	0x1	/* table exists */
#define RATTTABFLNRL	0x2	/* forbid realloc */

/* minimum table size; cannot be lower than 1 */
#define RATTTAB_MINSIZ 1
/* maximum table size; cannot be higher than SIZE_MAX - 1 */
#define RATTTAB_MAXSIZ SIZE_MAX - 1

typedef struct {
	void *head, *tail;	/* head and tail of table */
	size_t size, pos, last;	/* size and position of table */
	size_t chunk_size;	/* size of a chunk */
	size_t chunk_count;	/* number of chunks pushed */
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

static inline int ratt_table_exists(ratt_table_t *table)
{
	return (table->flags & RATTTABFLXIS);
}

static inline int ratt_table_isempty(ratt_table_t *table)
{
	/* true if table does not exist yet or no chunk pushed yet */
	return ((!ratt_table_exists(table)) || !table->chunk_count);
}

static inline void *ratt_table_get_first(ratt_table_t *table)
{
	return (ratt_table_isempty(table)) ? NULL : table->head;
}

static inline void *ratt_table_get_last(ratt_table_t *table)
{
	return (ratt_table_isempty(table)) ? NULL : table->tail;
}

static inline void *ratt_table_get_current(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)
	    && table->pos <= table->last) {
		return table->head + (table->pos * table->chunk_size);
	} else
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
extern int ratt_table_search(ratt_table_t *, void **,
    int (*)(void const *, void const *), void const *);
extern int ratt_table_get_tail_next(ratt_table_t *, void **);

#endif /* RATTLE_TABLE_H */