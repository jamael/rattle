#ifndef SRC_CONF_H
#define SRC_CONF_H

#include <rattd/conf.h>

void conf_fini(void *);
int conf_init(void);
int conf_open(const char *);
int conf_table_parse(conf_decl_t *);
void conf_table_release(conf_decl_t *);

#endif /* SRC_CONF_H */
