#ifndef RATTLE_TABLE_H
#define RATTLE_TABLE_H

#include <stdint.h>
#include <string.h>	/* ffs() */
#include <rattle/def.h>

#define RATTTABFLXIS	0x1	/* table exists */
#define RATTTABFLNRA	0x2	/* disable realloc */
#define RATTTABFLNRU	0x4	/* disable fragment reuse */

/* minimum table size; cannot be lower than 1 */
#ifndef RATTTABMINSIZ
#define RATTTABMINSIZ		1
#endif

/* maximum table size; cannot be higher than SIZE_MAX - 1 */
#ifndef RATTTABMAXSIZ
#define RATTTABMAXSIZ		SIZE_MAX - 1
#endif

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

typedef struct {
	void *head, *tail;	/* head and tail of table */
	size_t size, pos, last;	/* size and position of table */

	size_t chunk_size;	/* chunk size */
	size_t chunk_count;	/* chunk counter */

	size_t frag_count;	/* fragment counter */
	uint8_t *frag_mask;	/* fragment mask */

	int flags;		/* table flags */

	/* constrains callback */
	int (*constrains)(void const *, void const *);
	int (*on_constrains)(void *, void const *);
} ratt_table_t;

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
	return table->frag_count;
}

static inline int ratt_table_fragmented(ratt_table_t *table)
{
	return (ratt_table_frag_count(table));
}

static inline int ratt_table_exists(ratt_table_t *table)
{
	return (table->flags & RATTTABFLXIS);
}

static inline int ratt_table_isempty(ratt_table_t *table)
{
	/* true if table does not exist yet or no chunk pushed yet */
	return (!ratt_table_exists(table) || !ratt_table_count(table));
}

static inline size_t ratt_table_pos_last(ratt_table_t *table)
{
	return table->last;
}

static inline size_t ratt_table_pos_current(ratt_table_t *table)
{
	return table->pos;
}

static inline int ratt_table_pos_isfrag(ratt_table_t *table, size_t pos)
{
	uint8_t const *slot = table->frag_mask;
	if (!ratt_table_isempty(table)
	    && pos <= table->last)
		shift_mask_slot(slot, pos);
		return (*slot & shift_mask(pos));
	return 0;
}

static inline int ratt_table_ishead(ratt_table_t *table, void *chunk)
{
	return (ratt_table_exists(table) && (chunk == table->head));
}

static inline int ratt_table_istail(ratt_table_t *table, void *chunk)
{
	return (ratt_table_exists(table) && (chunk == table->tail));
}

static inline void *ratt_table_first(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)) {
		table->pos = 0;
		return table->head;
	}

	return NULL;
}

static inline void *ratt_table_last(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)) {
		table->pos = table->last;
		return table->tail;
	}

	return NULL;
}

static inline void *ratt_table_prev(ratt_table_t *table)
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

static inline void *ratt_table_next(ratt_table_t *table)
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

static inline void *ratt_table_first_next(ratt_table_t *table)
{
	void *chunk = NULL;
	if ((chunk = ratt_table_first(table)) != NULL) {
		if (!ratt_table_fragmented(table)
		    || !ratt_table_pos_isfrag(table, table->pos))
			return chunk;
	}
	return ratt_table_next(table);
}

static inline void *ratt_table_circular_next(ratt_table_t *table)
{
	void *chunk = NULL;
	if ((chunk = ratt_table_next(table)) != NULL) {
		return chunk;
	}
	return ratt_table_first_next(table);
}

static inline void *ratt_table_current(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)
	    && table->pos <= table->last) {
		return (char *) table->head
		    + (table->pos * table->chunk_size);
	}
	return NULL;
}

static inline void *ratt_table_chunk(ratt_table_t *table, size_t pos)
{
	if (!ratt_table_isempty(table)
	    && pos <= table->last) {
		table->pos = pos;
		return (char *) table->head
		    + (table->pos * table->chunk_size);
	}

	return NULL;
}

static inline void
ratt_table_set_constrains(
    ratt_table_t *table,
    int (*compare)(void const *, void const *))
{
	table->constrains = compare;
}

static inline void
ratt_table_set_on_constrains(
    ratt_table_t *table,
    int (*resolve)(void *, void const *))
{
	table->on_constrains = resolve;
}

#define RATT_TABLE_FOREACH(tab, chunk) \
	for ((chunk) = ratt_table_first_next((tab)); \
	    (chunk) != NULL; (chunk) = ratt_table_next((tab)))

#define RATT_TABLE_FOREACH_REVERSE(tab, chunk) \
	for ((chunk) = ratt_table_last((tab)); \
	    (chunk) != NULL; (chunk) = ratt_table_prev((tab)))

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
extern int ratt_table_satisfy_constrains(ratt_table_t *, void const *);

#endif /* RATTLE_TABLE_H */
