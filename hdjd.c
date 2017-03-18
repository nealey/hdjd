#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/select.h>
#include "alsa.h"
#include "usb.h"
#include "dump.h"


static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
    usb_interrupting();
    alsa_interrupting();
}

int
setup()
{
	char name[100];
	
	if (usb_setup(name, sizeof(name)) < 0) {
        usb_finish();
		return -1;
	}
	
	if (alsa_setup(name) < 0) {
        alsa_close();
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
    signal(SIGINT, intHandler);
	while (keepRunning) {
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
	}
    printf("Exiting...\n");
    usb_finish();
    alsa_close();

	return 0;
}
