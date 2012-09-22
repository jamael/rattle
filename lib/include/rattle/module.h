#ifndef RATTLE_MODULE_H
#define RATTLE_MODULE_H

#include <rattle/conf.h>

typedef struct {
	char *name;
	char *version;
	ratt_conf_t **conf;
} ratt_module_t;

int ratt_module_attach(ratt_module_t *);
int ratt_module_detach(ratt_module_t *);

#endif /* RATTLE_MODULE_H */
