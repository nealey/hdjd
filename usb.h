#ifndef _USB_H_
#define _USB_H_

#include <stdint.h>
#include <sys/select.h>

int usb_setup(char *name, size_t namelen);
void usb_fd_setup(int *nfds, fd_set *rfds, fd_set *wfds);
void usb_check_fds(fd_set *rfds, fd_set *wfds);
void usb_write(uint8_t *data, size_t datalen);

#endif
