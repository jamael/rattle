#ifndef RATTD_CONF_H
#define RATTD_CONF_H

#include <rattle/conf.h>

int conf_fini(void);
int conf_init(void);
int conf_open(const char *);
int conf_table_parse(ratt_conf_t *);
void conf_table_release(ratt_conf_t *);

#endif /* RATTD_CONF_H */
