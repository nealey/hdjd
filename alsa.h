#ifndef _ALSA_H_
#define _ALSA_H_

#include <sys/select.h>

int alsa_setup(const char *name);
void alsa_fd_setup(int *nfds, fd_set *rfds, fd_set *wfds);
void alsa_read_ready();

#endif
