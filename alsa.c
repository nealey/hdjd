#include <stdio.h>
#include <poll.h>
#include <sys/select.h>
#include <alsa/asoundlib.h>
#include "alsa.h"
#include "usb.h"
#include "log.h"
#include "dump.h"

#define ALSA_BUFSIZE 4096
#define MAX_PFDS 20

static snd_seq_t *snd_handle = NULL;
static int seq_port;
static snd_midi_event_t *midi_event_parser = NULL;

int npfd;
struct pollfd pfd[MAX_PFDS];

void alsa_interrupting()
{
  //Currently nothing to do.
}

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

  // Allocate parser object for converting to and from MIDI
  if (snd_midi_event_new(ALSA_BUFSIZE, &midi_event_parser) < 0) {
    fatal("ALSA cannot allocate MIDI event parser");
    return -1;
  } else {
    snd_midi_event_no_status(midi_event_parser,1);
    return 0;
  }
}

void
alsa_close()
{
  if (midi_event_parser) {
    snd_midi_event_free(midi_event_parser);
  }
  if (snd_handle) {
    snd_seq_close(snd_handle);
  }
}


void
alsa_fd_setup(int *nfds, fd_set *rfds, fd_set *wfds)
{
  npfd = snd_seq_poll_descriptors_count(snd_handle, POLLIN);
  int i;
	
  if (npfd > MAX_PFDS) {
    fatal("ALSA wants too many file descriptors");
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
  snd_midi_event_init(midi_event_parser);
	
  for (;;) {
    snd_seq_event_t *ev;
    unsigned char buf[ALSA_BUFSIZE];
    long converted;
    int r;
		
    r = snd_seq_event_input(snd_handle, &ev);
		
    if (r == -EAGAIN) {
      // input queue empty
      break;
    } else if (r == -ENOSPC) {
      warn("Input queue overflow");
      continue;
    } else if (r < 0) {
      warn("snd_seq_event_input() = %d", r);
      continue;
    }
        
    converted = snd_midi_event_decode(midi_event_parser, buf, ALSA_BUFSIZE, ev);
    if (converted < 0) {
      if (ev->type == SND_SEQ_EVENT_CLIENT_START ) {
	DUMPf("Client started \n");
	continue;
      } else if (ev->type == SND_SEQ_EVENT_CLIENT_EXIT) {
	DUMPf("Client exited \n");
	continue;
      } else if (ev->type == SND_SEQ_EVENT_CLIENT_CHANGE) {
	DUMPf("Client status/info changed \n");
	continue;
      } else if (ev->type == SND_SEQ_EVENT_PORT_START) {
	DUMPf("Port created \n");
	continue;
      } else if (ev->type == SND_SEQ_EVENT_PORT_EXIT) {
	DUMPf("Port deleted \n");
	continue;
      } else if (ev->type == SND_SEQ_EVENT_PORT_CHANGE) {
	DUMPf("Port status/info changed \n");
	continue;
      } else if (ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED) {
	DUMPf("Port connected \n");
	continue;
      } else if (ev->type == SND_SEQ_EVENT_PORT_UNSUBSCRIBED) {
	DUMPf("Port disconnected \n");
	continue;
      } else {
	warn("Can't decode MIDI event type 0x%02x", ev->type);
      }
      continue;
    }
		
    if (converted < 3) {
      int i;
			
      warn("Uh oh, weird MIDI packet with length %ld (does this make sense if prefixed with 90?)", converted);
			
      for (i = 0; i < converted; i += 1) {
	fprintf(stderr, "  %02x", buf[i]);
      }
      fprintf(stderr, "\n");
    }
		
    usb_write(buf, converted);
  }
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
  size_t offset = 0;
	
  snd_midi_event_init(midi_event_parser);
	
  while( datalen > offset) {
    snd_seq_event_t ev;
    long encoded;
    int r;
		
    encoded = snd_midi_event_encode(midi_event_parser, data + offset, datalen - offset, &ev);
    if (encoded <= 1) {
      int i;
	
      warn("Unable to encode MIDI message (%ld < %ld)", encoded, (long int)datalen);
      fprintf(stderr, "    ");
      for (i = offset; i < (long int)datalen; i += 1) {
	fprintf(stderr, "%02x ", data[i]);
      }
      fprintf(stderr, "\n");
			
      return;
    }
		
    snd_seq_ev_set_direct(&ev);
    snd_seq_ev_set_source(&ev, seq_port);
    snd_seq_ev_set_subs(&ev);
    r = snd_seq_event_output(snd_handle, &ev);
		
    if (r < 0) {
      warn("Couldn't write an output event");
    }
    if (r > 200) {
      warn("Output buffer size %d", r);
    }
		
    offset += encoded;
  }
  snd_seq_drain_output(snd_handle);
}
