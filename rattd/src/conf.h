#ifndef SRC_CONF_H
#define SRC_CONF_H

#include <rattle/conf.h>

void conf_fini(void *);
int conf_init(void);
void conf_close(void);
int conf_open(const char *);
int conf_open_builtin(const char *);
int conf_parse(char const *, ratt_conf_t *);
void conf_release(ratt_conf_t *);
void conf_release_reverse(ratt_conf_t const * const, ratt_conf_t *);

#endif /* SRC_CONF_H */
