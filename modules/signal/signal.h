#ifndef SRC_SIGNAL_H
#define SRC_SIGNAL_H

#include <signal.h>

void signal_fini(void *);
int signal_init(void);
void signal_unregister(int, void (*)(int, siginfo_t const *, void *));
int signal_register(int, void (*)(int, siginfo_t const *, void *), void *);
void signal_wait(void);

#endif /* SRC_SIGNAL_H */
