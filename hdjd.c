#include <stdio.h>
#include <stdint.h>
#include <libusb.h>
#include <sys/select.h>
#include <alsa/asoundlib.h>
#include "dump.h"

/*
 * Some things I use for debugging 
 */
#ifdef NODUMP
#define DUMPf(fmt, args...)
#else
#define DUMPf(fmt, args...) fprintf(stderr, "%s:%s:%d " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ##args)
#endif
#define DUMP() DUMPf("")
#define DUMP_d(v) DUMPf("%s = %d", #v, v)
#define DUMP_x(v) DUMPf("%s = 0x%x", #v, v)
#define DUMP_s(v) DUMPf("%s = %s", #v, v)
#define DUMP_c(v) DUMPf("%s = '%c' (0x%02x)", #v, v, v)
#define DUMP_p(v) DUMPf("%s = %p", #v, v)

struct device {
	uint16_t product_id;
	uint8_t ep_in;
	uint8_t ep_out;
};

const struct device devices[] = {
	{ 0xb102, 0x83, 0x04 }, // Steel
	{ 0xb105, 0x82, 0x03 }, // MP3e2
	{ 0, 0, 0 }
};

// Handle to ALSA sequencer
static snd_seq_t *handle;

// Descriptor of our fake handle
int seq_port;

void
midi_send(uint8_t *data, size_t datalen)
{
	snd_seq_event_t ev;
	snd_midi_event_t *midi_event_parser;

	snd_midi_event_new(datalen, &midi_event_parser);
	
	snd_midi_event_encode(midi_event_parser, data, datalen, &ev);
	snd_seq_ev_set_direct(&ev);
	snd_seq_ev_set_source(&ev, seq_port);
	snd_seq_ev_set_subs(&ev);
	snd_seq_event_output(handle, &ev);	
	snd_seq_drain_output(handle);
	
	snd_midi_event_free(midi_event_parser);
}

void
usb_xfer_done(struct libusb_transfer *transfer)
{
	uint8_t *data = transfer->buffer;
	int datalen = transfer->actual_length;
	int i;
	
	for (i = 0; i < datalen; i += 1) {
		printf("%02x ", data[i]);
	}
	printf("\n");
	
	midi_send(data, datalen);
}

void
handle_alsa()
{
	snd_midi_event_t *midi_event_parser;
	snd_seq_event_t *ev;
	int ret = 1;

	for (ret = 1; ret > 0; ) {
		char buf[512];
		long converted;
		int i;
		
		ret = snd_seq_event_input(handle, &ev);
		snd_midi_event_new(512, &midi_event_parser);
		converted = snd_midi_event_decode(midi_event_parser, buf, 512, ev);
		printf(" << ");
		for (i = 0; i < converted; i += 1) {
			printf("%02x ", buf[i]);
		}
		printf("\n");
		snd_midi_event_free(midi_event_parser);
	}	
}

int
main(int argc, char **argv)
{
	struct libusb_device_handle *dev;
	struct libusb_device_descriptor ddesc;
	char name[100];
	const struct device *d;
	nfds_t nfds = 0;
	int ret;

	if (libusb_init(NULL) < 0) {
		return 69;
	}

	for (d = devices; d->product_id; d += 1) {
		dev = libusb_open_device_with_vid_pid(NULL, 0x6f8, d->product_id);
		if (dev) {
			break;
		}
	}
	if (!dev) {
		printf("Couldn't find a controller\n");
		return 69;
	}

	// Figure out what this thing is called
	libusb_get_device_descriptor(libusb_get_device(dev), &ddesc);
	{
		char *p = name;

		ret = libusb_get_string_descriptor_ascii(dev, ddesc.iManufacturer, name, sizeof(name));
		if (ret > 0) {
			p = name + ret;
			*p = ' ';
			p += 1;
			ret = libusb_get_string_descriptor_ascii(dev, ddesc.iProduct, p, sizeof(name) - ret - 1);
		}

		if (ret < 0) {
			printf("Can't figure out what to call this thing.\n");
			return 69;
		}
	}
	printf("Opened a %s\n", name);

	// Initialize ALSA
	if (snd_seq_open(&handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
		perror("snd_seq_open");
		return(69);
	}
	snd_seq_nonblock(handle, 1);
	snd_seq_set_client_name(handle, name);
	seq_port = snd_seq_create_simple_port(handle, name,
					      SND_SEQ_PORT_CAP_READ |
					      SND_SEQ_PORT_CAP_WRITE |
					      SND_SEQ_PORT_CAP_SUBS_READ |
					      SND_SEQ_PORT_CAP_SUBS_WRITE,
					      SND_SEQ_PORT_TYPE_MIDI_GENERIC);

	while (1) {
		struct libusb_transfer *xfer = libusb_alloc_transfer(0);
		uint8_t data[80];
		int transferred;
		int i;

		if ((ret = libusb_bulk_transfer(dev, d->ep_in, data, sizeof data, &transferred, 0))) {
			break;
		}

		// Set up transfer
		libusb_fill_bulk_transfer(xfer, dev, d->ep_in, data, sizeof data, usb_xfer_done, NULL, 0);
		libusb_submit_transfer(xfer);
			
		// Select on our file descriptors
		{
			const struct libusb_pollfd **usb_fds = libusb_get_pollfds(NULL);
			struct pollfd *pfd;
			int npfd;
			struct timeval tv;
			struct timeval *timeout;
			fd_set rfds, wfds;
			int nfds = 0;
			
			FD_ZERO(&rfds);
			FD_ZERO(&wfds);
			
			// ALSA shit
			{
				
				npfd = snd_seq_poll_descriptors_count(handle, POLLIN);
				pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
				snd_seq_poll_descriptors(handle, pfd, npfd, POLLIN);
				
				for (i = 0; i < npfd; i += 1) {
					if (pfd[i].fd > nfds) {
						nfds = pfd[i].fd;
					}
					if (pfd[i].events & POLLIN) {
						FD_SET(pfd[i].fd, &rfds);
					}
				}
			}
			
			// USB shit
			{
	
				ret = libusb_get_next_timeout(NULL, &tv);
				if (0 == ret) {
					timeout = NULL;
				} else {
					timeout = &tv;
				}
				
				for (i = 0; usb_fds[i]; i += 1) {
					const struct libusb_pollfd *ufd = usb_fds[i];
		
					if (ufd->fd > nfds) {
						nfds = ufd->fd;
					}
					if (ufd->events & POLLIN) {
						FD_SET(ufd->fd, &rfds);
					}
					if (ufd->events & POLLOUT) {
						FD_SET(ufd->fd, &wfds);
					}
				}
			}
			
			ret = select(nfds + 1, &rfds, &wfds, NULL, timeout);
			
			for (i = 0; usb_fds[i]; i += 1) {
				int fd = usb_fds[i]->fd;
				
				if (FD_ISSET(fd, &rfds) || FD_ISSET(fd, &wfds)) {
					libusb_handle_events(NULL);
				}
			}
			
			for (i = 0; i < npfd; i += 1) {
				int fd = pfd[i].fd;
				
				if (FD_ISSET(fd, &rfds)) {
					handle_alsa();
				}
			}
		}
		
		libusb_free_transfer(xfer);
	}

	if (ret < 0) {
		printf("ERROR: %s\n", libusb_error_name(ret));
	}

	libusb_exit(NULL);

	return 0;
}
