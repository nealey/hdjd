#include <libusb.h>
#include <stdio.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include "usb.h"
#include "alsa.h"
#include "dump.h"

static struct libusb_device_handle *usb_dev;
static struct libusb_transfer *xfer;
static const struct device *d;
uint8_t data[80];

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

void usb_xfer_done(struct libusb_transfer *transfer);

static void
usb_initiate_transfer()
{
	// Tell libusb we want to know about bulk transfers
	xfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(xfer, usb_dev, d->ep_in, data, sizeof(data), usb_xfer_done, NULL, 0);
	libusb_submit_transfer(xfer);
}

void
usb_xfer_done(struct libusb_transfer *transfer)
{
	uint8_t *data = transfer->buffer;
	int datalen = transfer->actual_length;

	alsa_write(data, datalen);
	
	usb_initiate_transfer();
}

int
usb_setup(char *name, size_t namelen)
{
	if (libusb_init(NULL) < 0) {
		return -1;
	}
	
	if (libusb_pollfds_handle_timeouts(NULL) == 0) {
		printf("I'm too dumb to handle events on such an old system.\n");
		return -1;
	}
	
	for (d = devices; d->product_id; d += 1) {
		usb_dev = libusb_open_device_with_vid_pid(NULL, 0x6f8, d->product_id);
		if (usb_dev) {
			break;
		}
	}
	if (! usb_dev) {
		printf("Couldn't find a controller.\n");
		return -1;
	}
	
	// Figure out what it's called
	{
		int ret;
		struct libusb_device_descriptor ddesc;
		
		libusb_get_device_descriptor(libusb_get_device(usb_dev), &ddesc);
		ret = libusb_get_string_descriptor_ascii(usb_dev, ddesc.iManufacturer, (unsigned char *)name, namelen);
		if (ret > 0) {
			char *p = name + ret;
			
			*p = ' ';
			p += 1;
			ret = libusb_get_string_descriptor_ascii(usb_dev, ddesc.iProduct, (unsigned char *)p, namelen - ret - 1);
		}
		if (ret < 0) {
			printf("Warning: I can't figure out what to call this thing.\n");
		}
		printf("Opened [%s]\n", name);
	}
	

	return 0;
}

void
usb_fd_setup(int *nfds, fd_set *rfds, fd_set *wfds)
{
	const struct libusb_pollfd **usb_fds = libusb_get_pollfds(NULL);
	int i;

	for (i = 0; usb_fds[i]; i += 1) {
		const struct libusb_pollfd *ufd = usb_fds[i];
		
		if (ufd->fd > *nfds) {
			*nfds = ufd->fd;
		}
		if (ufd->events & POLLIN) {
			FD_SET(ufd->fd, rfds);
		}
		if (ufd->events & POLLOUT) {
			FD_SET(ufd->fd, wfds);
		}
	}

	usb_initiate_transfer();
}

void 
usb_check_fds(fd_set *rfds, fd_set *wfds)
{
	const struct libusb_pollfd **usb_fds = libusb_get_pollfds(NULL);
	int i;
	
	for (i = 0; usb_fds[i]; i += 1) {
		int fd = usb_fds[i]->fd;

		if (FD_ISSET(fd, rfds) || FD_ISSET(fd, wfds)) {
			libusb_handle_events(NULL);
			return;
		}
	}
}

void
usb_write(uint8_t *data, size_t datalen)
{
	int i;
	
	printf("U> ");
	for (i = 0; i < datalen; i += 1) {
		printf("%02x ", data[i]);
	}
	printf("\n");
}