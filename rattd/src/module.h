#ifndef SRC_MODULE_H
#define SRC_MODULE_H

#include <rattle/module.h>

void module_fini(void *);
int module_init(void);
int module_attach(char const *, char const *);
int module_parent_attach(char const *, int (*)(rattmod_entry_t *));
int module_parent_detach(char const *);

#endif /* SRC_MODULE_H */
