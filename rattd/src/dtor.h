#ifndef SRC_DTOR_H
#define SRC_DTOR_H

void dtor_fini(void *);
int dtor_init(void);
void dtor_callback(void);
int dtor_register(void (*)(void *), void *);
void dtor_unregister(void (*)(void *));

#endif /* SRC_DTOR_H */
