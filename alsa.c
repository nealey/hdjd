#include <stdio.h>
#include <poll.h>
#include <sys/select.h>
#include <alsa/asoundlib.h>
#include "alsa.h"
#include "usb.h"
#include "dump.h"

static snd_seq_t *snd_handle;
static int seq_port;
static snd_seq_event_t *ev;

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

#define ALSA_BUFSIZE 4096

void
alsa_read_ready()
{
	static snd_midi_event_t *midi_event_parser;

	if (snd_midi_event_new(ALSA_BUFSIZE, &midi_event_parser) < 0) {
		fprintf(stderr, "ALSA cannot allocate MIDI event parser\n");
		abort();
	}

	for (;;) {
		unsigned char buf[ALSA_BUFSIZE];
		long converted;

		if (snd_seq_event_input(snd_handle, &ev) < 0) {
			break;
		}

		converted = snd_midi_event_decode(midi_event_parser, buf, ALSA_BUFSIZE, ev);
		if (converted < 0) {
			fprintf(stderr, "Can't decode MIDI event type %d\n", ev->type);
		} else {
			usb_write(buf, converted);
		}
	}
	snd_midi_event_free(midi_event_parser);
}

void
alsa_check_fds(fd_set *rfds, fd_set *wfds)
{
	int i;
	
	for (i = 0; i < npfd; i += 1) {
		int fd = pfd[i].fd;

		if (FD_ISSET(fd, rfds) || FD_ISSET(fd, wfds)) {
			alsa_read_ready();
			return;
		}
	}
}

void
alsa_write(uint8_t *data, size_t datalen)
{
	static snd_midi_event_t *midi_event_parser;
	snd_seq_event_t ev;
	long r;

	if (snd_midi_event_new(ALSA_BUFSIZE, &midi_event_parser) < 0) {
		fprintf(stderr, "ALSA cannot allocate MIDI event parser\n");
		abort();
	}
	
	r = snd_midi_event_encode(midi_event_parser, data, datalen, &ev);
	if (r < datalen) {
		fprintf(stderr, "ALSA didn't parse that message\n");
		abort();
	}

	snd_seq_ev_set_direct(&ev);
	snd_seq_ev_set_source(&ev, seq_port);
	snd_seq_ev_set_subs(&ev);
	if ((r = snd_seq_event_output_direct(snd_handle, &ev)) < 0) {
		fprintf(stderr, "ALSA couldn't write an event: %ld\n", r);
		abort();
	}	
	
	snd_midi_event_free(midi_event_parser);
}