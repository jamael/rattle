#ifndef SRC_PROC_H
#define SRC_PROC_H

void proc_fini(void *);
int proc_init(void);
int proc_attach(void);
int proc_stop(void);
int proc_start(void);

#endif /* SRC_PROC_H */
