#ifndef SRC_MODULE_H
#define SRC_MODULE_H

#include <rattle/module.h>

void module_fini(void *);
int module_init(void);
int module_core_attach(ratt_module_core_t const *);
int module_core_detach(char const *);
int module_core_register(ratt_module_entry_t const *);
int module_core_unregister(ratt_module_entry_t const *);
int module_register(ratt_module_entry_t const *);
int module_unregister(ratt_module_entry_t const *);

#endif /* SRC_MODULE_H */
