#include <stdio.h>
#include <stdint.h>
#include <sys/select.h>
#include "alsa.h"
#include "usb.h"
#include "dump.h"

int
setup()
{
	char name[100];
	
	if (usb_setup(name, sizeof(name)) < 0) {
		return -1;
	}
	
	if (alsa_setup(name) < 0) {
		return -1;
	}
	
	return 0;
}

int
main(int argc, char *argv[])
{
	if (setup() < 0) {
		return 69;
	}
	
	for (;;) {
		fd_set rfds;
		fd_set wfds;
		int nfds = 0;
		int ret;
		
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		
		alsa_fd_setup(&nfds, &rfds, &wfds);
		usb_fd_setup(&nfds, &rfds, &wfds);
		
		ret = select(nfds + 1, &rfds, &wfds, NULL, NULL);
		if (-1 == ret) {
			DUMP();
		}
		
		alsa_check_fds(&rfds, &wfds);
		usb_check_fds(&rfds, &wfds);

		//DUMP();
	}

	return 0;
}
