#ifndef RATT_DATA_ARRAY_H
#define RATT_DATA_ARRAY_H

/* maximum value a size_t can hold */
#define RATTSIZMAX	(~((size_t) 0))

/* table flags */
#define RATTTABFLXIS	0x1	/* table exists */
#define RATTTABFLNRA	0x2	/* disable realloc */
#define RATTTABFLNRU	0x4	/* disable fragment reuse */

/* minimum table size; cannot be lower than 1 */
#ifndef RATTTABSIZMIN
#define RATTTABSIZMIN		1
#endif

/* maximum table size; cannot be higher than RATTSIZMAX - 1 */
#ifndef RATTTABSIZMAX
#define RATTTABSIZMAX		RATTSIZMAX - 1
#endif

/* table information */
struct ratt_table {
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
};

typedef struct ratt_table ratt_table_t;

static inline
size_t ratt_table_size(ratt_table_t *table)
{
	return table->size;
}

static inline
size_t ratt_table_count(ratt_table_t *table)
{
	return table->chunk_count;
}

static inline
size_t ratt_table_frag_count(ratt_table_t *table)
{
	return table->frag_count;
}

static inline
int ratt_table_fragmented(ratt_table_t *table)
{
	return (ratt_table_frag_count(table));
}

static inline
int ratt_table_exists(ratt_table_t *table)
{
	return (table->flags & RATTTABFLXIS);
}

static inline
int ratt_table_isempty(ratt_table_t *table)
{
	/* true if table does not exist yet or no chunk pushed yet */
	return (!ratt_table_exists(table) || !ratt_table_count(table));
}

static inline
size_t ratt_table_pos_last(ratt_table_t *table)
{
	return table->last;
}

static inline
size_t ratt_table_pos_current(ratt_table_t *table)
{
	return table->pos;
}

static inline
int ratt_table_ishead(ratt_table_t *table, void *chunk)
{
	return (ratt_table_exists(table) && (chunk == table->head));
}

static inline
int ratt_table_istail(ratt_table_t *table, void *chunk)
{
	return (ratt_table_exists(table) && (chunk == table->tail));
}

static inline
void *ratt_table_first(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)) {
		table->pos = 0;
		return table->head;
	}

	return NULL;
}

static inline
void *ratt_table_last(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)) {
		table->pos = table->last;
		return table->tail;
	}

	return NULL;
}

static inline
void *ratt_table_current(ratt_table_t *table)
{
	if (!ratt_table_isempty(table)
	    && table->pos <= table->last) {
		return (char *) table->head
		    + (table->pos * table->chunk_size);
	}
	return NULL;
}

static inline
void *ratt_table_chunk(ratt_table_t *table, size_t pos)
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
extern int ratt_table_pos_isfrag(ratt_table_t *, size_t);
extern void *ratt_table_prev(ratt_table_t *);
extern void *ratt_table_next(ratt_table_t *);
extern void *ratt_table_first_next(ratt_table_t *);
extern void *ratt_table_circular_next(ratt_table_t *);

#endif /* RATT_DATA_ARRAY_H */
