#ifndef SRC_MODULE_H
#define SRC_MODULE_H

#include <rattle/module.h>

void module_fini(void *);
int module_init(void);
int module_attach(char const *, char const *);
int module_parent_attach(ratt_module_parent_t const *);
int module_parent_detach(char const *);
int module_parent_register(ratt_module_entry_t const *);
int module_parent_unregister(ratt_module_entry_t const *);
int module_register(ratt_module_entry_t const *);
int module_unregister(ratt_module_entry_t const *);

#endif /* SRC_MODULE_H */
