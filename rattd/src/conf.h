#ifndef SRC_CONF_H
#define SRC_CONF_H

#include <rattle/conf.h>

void conf_fini(void *);
int conf_init(void);
void conf_close(void);
int conf_open(const char *);
int conf_open_builtin(const char *);
int conf_parse(char const *, rattconf_decl_t *);
void conf_release(rattconf_decl_t *);
void conf_release_reverse(rattconf_decl_t const * const, rattconf_decl_t *);

#endif /* SRC_CONF_H */
