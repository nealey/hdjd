#ifndef _USB_H_
#define _USB_H_

#include <sys/select.h>

int usb_setup(const char *name, size_t namelen);
void usb_fd_setup(int *nfds, fd_set *rfds, fd_set *wfds);
void usb_check_fds(fd_set *rfds, fd_set *wfds);

#endif
