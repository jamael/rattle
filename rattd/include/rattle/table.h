#ifndef RATTLE_TABLE_H
#define RATTLE_TABLE_H

#define RATTTABFLZER	0x1	/* table is empty */
#define RATTTABFLNRL	0x2	/* forbid realloc */

/* minimum table size; cannot be lower than 2 */
#define RATTTAB_MINSIZ 2

typedef struct {
	void *head, *tail;	/* head and tail of table */
	size_t size, pos, last;	/* size and position of table */
	size_t chunk_size;	/* size of a chunk */
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

#define RATT_TABLE_FOREACH(tab, chunk) \
	for (chunk = (tab)->head, (tab)->pos = 0; \
	    (void *) (chunk) <= (tab)->tail; (chunk)++, ((tab)->pos)++)

#define RATT_TABLE_FOREACH_REVERSE(tab, chunk) \
	for (chunk = (tab)->tail, (tab)->pos = (tab)->last; \
	    (void *) (chunk) >= (tab)->head; (chunk)-- , ((tab)->pos)--)

#define RATT_TABLE_INIT(tab) ratt_table_t (tab) = { 0 }

extern int ratt_table_create(ratt_table_t *, size_t, size_t, int);
extern int ratt_table_destroy(ratt_table_t *);
extern int ratt_table_push(ratt_table_t *, void const *);

#endif /* RATTLE_TABLE_H */
