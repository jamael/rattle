#ifndef SRC_ARGS_H
#define SRC_ARGS_H

#include <rattle/args.h>

/* main section identifier */
#define ARGSSECMAIN '0'

void args_fini(void *);
int args_init(void);
void args_show(void);
int args_unregister(int, char const *);
int args_register(int, char const *, ratt_args_t *);

#endif /* SRC_ARGS_H */
