#ifndef SRC_PROC_H
#define SRC_PROC_H

void proc_fini(void *);
int proc_init(void);
void proc_start(void);
void proc_unregister(void (*)(void *));
int proc_register(void (*)(void *), void *);

#endif /* SRC_PROC_H */
