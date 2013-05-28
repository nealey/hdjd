#include <stdio.h>
#include <poll.h>
#include <sys/select.h>
#include <alsa/asoundlib.h>
#include "dump.h"

static snd_seq_t *snd_handle;
static int seq_port;
static snd_midi_event_t *midi_event_parser;

int
alsa_setup(const char *name)
{
	if (snd_seq_open(&snd_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		perror("snd_seq_open");
		return -1;
	}
	snd_seq_nonblock(snd_handle, 1);
	snd_seq_set_client_name(snd_handle, name);
	seq_port =
	    snd_seq_create_simple_port(snd_handle, name,
				       SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_READ |
				       SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_MIDI_GENERIC);

	if (snd_seq_event_input(handle, &ev) < 0) {
		return -1;
	}

	if (snd_midi_event_new(256, &midi_event_parser) < 0) {
		return -1;
	}

	return 0;
}

#define MAX_PFDS 20
int npfd;
struct pollfd pfd[MAX_PFDS];

void
alsa_fd_setup(int *nfds, fd_set *rfds, fd_set *wfds)
{
	npfd = snd_seq_poll_descriptors_count(snd_handle, POLLIN);
	int i;
	
	if (npfd > MAX_PFDS) {
		fprintf(stderr, "ALSA wants too many file descriptors\n");
		abort();
	}
	
	snd_seq_poll_descriptors(snd_handle, pfd, npfd, POLLIN);
	for (i = 0; i < npfd; i += 1) {
		if (pfd[i].fd > *nfds) {
			*nfds = pfd[i].fd;
		}
		if (pfd[i].events & POLLIN) {
			FD_SET(pfd[i].fd, rfds);
		}
	}
}


void
alsa_read_ready()
{
	snd_seq_event_t *ev;
	int ret = 1;

	for (;;) {
		char buf[256];
		long converted;
		int i;

		if (snd_seq_event_input(handle, &ev) < 0) {
			break;
		}
		if (!midi_event_parser) {
			if (snd_midi_event_new(256, &midi_event_parser) < 0) {
				continue;
			}
		}
		converted = snd_midi_event_decode(midi_event_parser, buf, 256, ev);
		printf(" << ");
		for (i = 0; i < converted; i += 1) {
			printf("%02x ", buf[i]);
		}
		printf(":\n");
	}
}

void
alsa_check_fds(fd_set *rfds, fd_set *wfds)
{
	int i;
	
	for (i = 0; i < npfd; i += 1) {
		int fd = pfds[i]->fd;

		if (FD_ISSET(fd, &rfds) || FD_ISSET(fd, &wfds)) {
			alsa_read_ready();
			return;
		}
	}
}
