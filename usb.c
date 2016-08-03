#include <libusb.h>
#include <stdio.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include "usb.h"
#include "alsa.h"
#include "log.h"
#include "dump.h"

static struct libusb_device_handle *usb_dev;
static const struct device *d;

static int writes_pending = 0;
static int reads_pending = 0;

struct device {
	uint16_t product_id;
	uint8_t interface_number;
	uint8_t ep_in;
	uint8_t ep_out;
};

const struct device devices[] = {
	{ 0xb102, 1, 0x83, 0x04 }, // Steel
	{ 0xb105, 1, 0x82, 0x03 }, // MP3e2
	{ 0xb109, 4, 0x82, 0x03 }, // 4-MX
	{ 0, 0, 0, 0 }
};

void usb_xfer_done(struct libusb_transfer *transfer);

static void
usb_initiate_transfer()
{
	unsigned char *buf;
	
	buf = (unsigned char *)malloc(256);
	
	// Tell libusb we want to know about bulk transfers
	struct libusb_transfer *xfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(xfer, usb_dev, d->ep_in, buf, 256, usb_xfer_done, NULL, 0);
	libusb_submit_transfer(xfer);
	reads_pending += 1;
}

void
usb_xfer_done(struct libusb_transfer *xfer)
{
	uint8_t *data = xfer->buffer;
	int datalen = xfer->actual_length;
	
	reads_pending -= 1;

	alsa_write(data, datalen);
	free(data);
	libusb_free_transfer(xfer);
 	usb_initiate_transfer();
}

int
usb_setup(char *name, size_t namelen)
{
	if (libusb_init(NULL) < 0) {
		return -1; 
	}
	
	if (libusb_pollfds_handle_timeouts(NULL) == 0) {
		fatal("I'm too dumb to handle events on such an old system.");
		return -1;
	}
	
	for (d = devices; d->product_id; d += 1) {
		usb_dev = libusb_open_device_with_vid_pid(NULL, 0x6f8, d->product_id);
		if (usb_dev) {
			break;
		}
	}
	if (! usb_dev) {
		fatal("Couldn't find a controller.");
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
			warn("I can't figure out what to call this thing.");
		}
		printf("Opened [%s]\n", name);
	}

	if (0 != libusb_claim_interface(usb_dev, d->interface_number)) {
		fatal("Couldn't claim interface %d", d->interface_number);
	}
	
	
	usb_initiate_transfer();

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

	if (reads_pending + writes_pending > 10) {
		warn("%d(r)+%d(w) = %d outstanding USB transactions!", reads_pending, writes_pending, reads_pending + writes_pending);
	}

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
usb_write_done(struct libusb_transfer *xfer)
{
	if (xfer->status) {
		warn("USB Write status %d", xfer->status);
	}
	writes_pending -= 1;
	free(xfer->buffer);
	libusb_free_transfer(xfer);
}

void
usb_write(uint8_t *data, size_t datalen)
{
	struct libusb_transfer *xfer;
	unsigned char *buf;
		
	writes_pending += 1;
	xfer = libusb_alloc_transfer(0);
	buf = (unsigned char *)malloc(datalen);
	memcpy(buf, data, datalen);
	libusb_fill_bulk_transfer(xfer, usb_dev, d->ep_out, buf, datalen, usb_write_done, NULL, 0);
	libusb_submit_transfer(xfer);
}