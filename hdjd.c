#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sysexits.h>
#include <unistd.h>
#include "alsa.h"
#include "usb.h"
#include "dump.h"


static volatile int keepRunning = 1;
bool enablePhono = false;

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
	int opt;
	while ((opt = getopt(argc, argv, "p")) != -1) {
		switch (opt) {
		case 'p':
			enablePhono = true;
			break;
		default:
			printf("Usage: %s [-p]\n", argv[0]);
			printf("\n");
			printf("-p	Inputs 1-2 to phono levels on 4-MX\n");
			return EX_USAGE;
		}
	}

	if (setup() < 0) {
		return 69;
	}

	if (enablePhono) {
		hercules_4mx_set_phono();
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
