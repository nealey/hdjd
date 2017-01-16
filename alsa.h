#ifndef _ALSA_H_
#define _ALSA_H_

#include <stdint.h>
#include <sys/select.h>

int alsa_setup(const char *name);
void alsa_close();
void alsa_interrupting();
void alsa_fd_setup(int *nfds, fd_set *rfds, fd_set *wfds);
void alsa_read_ready();
void alsa_check_fds(fd_set *rfds, fd_set *wfds);
void alsa_write(uint8_t *data, size_t datalen);
#endif
