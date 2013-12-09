#ifndef RATTLE_TEST_H
#define RATTLE_TEST_H

#define RATT_TEST		"test"	/* name of this module parent */
#define RATT_TEST_VER_MAJOR	0	/* major version */
#define RATT_TEST_VER_MINOR	1	/* minor version */

typedef struct {
	void *udata;		/* test user data */
	int retval;		/* test return value */
	int expect;		/* test expectation */
	double cpu_time_used;	/* test execution time */
} ratt_test_data_t;

static inline void ratt_test_set_udata(ratt_test_data_t *data, void *udata)
{
	data->udata = udata;
}

static inline void *ratt_test_get_udata(ratt_test_data_t *data)
{
	return data->udata;
}

static inline int ratt_test_get_retval(ratt_test_data_t *data)
{
	return data->retval;
}

typedef struct {
	int (*on_register)(ratt_test_data_t *);
	void (*on_unregister)(void *);
	int (*on_expect)(ratt_test_data_t *);
	int (*on_run)(void *);
	void (*on_summary)(void const *);
} ratt_test_hook_t;

#endif /* RATTLE_TEST_H */
