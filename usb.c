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

#define MAX_PFDS 20

struct device {
  uint16_t product_id;
  uint8_t interface_number;
  uint8_t ep_in;
  uint8_t ep_out;
  uint8_t ep_in2;
  uint8_t interface_number2;
};
const struct device devices[] = {
  { 0xb102, 1, 0x83, 0x04, 0x0, 0 }, // Steel
  { 0xb105, 1, 0x82, 0x03, 0x0, 0 }, // MP3e2
  { 0xb120, 1, 0x82, 0x03, 0x0, 0 }, // Hercules MP3 LE / Glow
  { 0xb107, 5, 0x83, 0x03, 0x0, 0 }, // Hercules Mk4
  { 0xb100, 1, 0x86, 0x06, 0x0, 0 }, // Hercules Mk2
  { 0xb109, 5, 0x83, 0x03, 0x84, 0}, // 4-Mx
  { 0xb10c, 5, 0x83, 0x03, 0x84, 0}, // Hercules DJ 4Set
  { 0xb10d, 5, 0x83, 0x03, 0x84, 0}, // Hercules DJ 4Set, second variant(?)
  { 0, 0, 0, 0 }
};

static const int MANUFACTURER_HERCULES = 0x6f8;
static libusb_context *context = NULL;
static struct libusb_device_handle *usb_dev = NULL;
static const struct device *dev_info;
const struct libusb_pollfd **usb_fds = NULL;
struct libusb_transfer *xfer_in;
struct libusb_transfer *xfer_in2;

static int writes_pending = 0;
static int reads_pending = 0;

void
usb_debug_msg(char *action, int ep, uint8_t data[], size_t datalen)
{
#ifdef DEBUG
  fprintf(stderr, "%s on ep%02x:", action, ep);
  for (int i = 0; i < datalen; i += 1) {
    fprintf(stderr, " %02x", data[i]);
  }
  fprintf(stderr, "\n");
#endif
}

void usb_xfer_done(struct libusb_transfer *xfer);
void usb_xfer_done_additional(struct libusb_transfer *xfer);

void 
usb_interrupting()
{
  libusb_cancel_transfer(xfer_in);
  if (dev_info->ep_in2 != 0x0) {
    libusb_cancel_transfer(xfer_in2);
  }
}
static void
usb_initiate_transfer()
{
  static const int buffsize = 256;
  unsigned char *buf;
	
  buf = (unsigned char *)malloc(buffsize);
	
  // Tell libusb we want to know about bulk transfers
  xfer_in = libusb_alloc_transfer(0);
  //timeout if in 1000 milliseconds it hasn't been sent
  xfer_in->timeout=1000;
  xfer_in->flags |=LIBUSB_TRANSFER_ADD_ZERO_PACKET;
  libusb_fill_bulk_transfer(xfer_in, usb_dev, dev_info->ep_in, buf, buffsize, usb_xfer_done, NULL, 0);
  libusb_submit_transfer(xfer_in);
  reads_pending += 1;
}

static void
usb_initiate_transfer_additional()
{
  static const int buffsize = 256;
  unsigned char *buf;
	
  buf = (unsigned char *)malloc(buffsize);
	
  // Tell libusb we want to know about bulk transfers
  xfer_in2 = libusb_alloc_transfer(0);
  //timeout if in 1000 milliseconds it hasn't been sent
  xfer_in2->timeout=1000;
  xfer_in2->flags |=LIBUSB_TRANSFER_ADD_ZERO_PACKET;
  libusb_fill_bulk_transfer(xfer_in2, usb_dev, dev_info->ep_in2, buf, buffsize, usb_xfer_done_additional, NULL, 0);
  libusb_submit_transfer(xfer_in2);
}

void LIBUSB_CALL
usb_xfer_done(struct libusb_transfer *xfer)
{
  uint8_t *data = xfer->buffer;
  int datalen = xfer->actual_length;
  reads_pending -= 1;
  if ( xfer->status == LIBUSB_TRANSFER_COMPLETED ) {
    usb_debug_msg("Receiving", dev_info->ep_in, data, datalen);
    alsa_write(data, datalen);
  }
  if ( xfer->status == LIBUSB_TRANSFER_COMPLETED ) {
    usb_initiate_transfer();
  } else if ( xfer->status != LIBUSB_TRANSFER_CANCELLED ) {
    fatal("Stopping EP_IN, because of status %d.\nSoftware needs restarting", xfer->status);
  }

  free(data);
  libusb_free_transfer(xfer);
}
void LIBUSB_CALL
usb_xfer_done_additional(struct libusb_transfer *xfer)
{
  if ( xfer->status == LIBUSB_TRANSFER_COMPLETED ) {
    // We don't need to use the information of this call, but it is needed that we
    // poll this, or else it hangs and doesn't receive more data.
    usb_debug_msg("Receiving", dev_info->ep_in2, xfer->buffer, xfer->actual_length);
  }

  if ( xfer->status == LIBUSB_TRANSFER_COMPLETED ) {
    usb_initiate_transfer_additional();
  } else if ( xfer->status != LIBUSB_TRANSFER_CANCELLED ) {
    fatal("Stopping EP_IN2, because of status %d\nSoftware needs restarting", xfer->status);
  }

  free(xfer->buffer);
  libusb_free_transfer(xfer);
}

int
usb_setup(char *name, size_t namelen)
{
  int ret;
  ret=libusb_init(&context);
  if (ret < 0) {
    fatal("ERROR: %s\n%s", libusb_error_name(ret), libusb_strerror(ret));
    return -1; 
  }
  //----------------------------------------------------------------------------
  // Enable debug
  //----------------------------------------------------------------------------
#ifdef DEBUG
  libusb_set_debug(context, LIBUSB_LOG_LEVEL_WARNING); 
#endif

  if (libusb_pollfds_handle_timeouts(context) == 0) {
    fatal("I'm too dumb to handle events on such an old system.");
    return -1;
  }
  //----------------------------------------------------------------------------
  // Get device list
  //----------------------------------------------------------------------------
  struct libusb_device_descriptor founddesc = {0};
  {
    libusb_device        **devs;
    ssize_t              count; //holding number of devices in list
    printf("Locating Hercules USB devices...\n(You can also use the lsusb command to locate this information)\n");
    count = libusb_get_device_list(context, &devs);
    if ( count < 0) {
      fatal("Error getting the device list: %s\n%s", libusb_error_name(count), libusb_strerror(count));
      return -1;
    } else if (count == 0) {
      warn("Seems that the USB device list is empty. Is the controller connected? ");
    }
    size_t idx;
    for (idx = 0; idx < count; idx+=1) {
      libusb_device *device = devs[idx];
      struct libusb_device_descriptor listdesc = {0};

      ret = libusb_get_device_descriptor(device, &listdesc);
      if ( ret  != 0) {
	warn("Could not get descriptor for device index %ld: %s\n%s", 
	     (long int)idx, libusb_error_name(count), libusb_strerror(count));
      } else if (listdesc.idVendor == MANUFACTURER_HERCULES) {
	printf("Vendor:Device = %04x:%04x\n", listdesc.idVendor, listdesc.idProduct);
	founddesc = listdesc;
      }
    }

    libusb_free_device_list(devs, 1); //free the list, unref the devices in it
    //----------------------------------------------------------------------------
  }
  for (dev_info = devices; dev_info->product_id; dev_info += 1) {
    usb_dev = libusb_open_device_with_vid_pid(context, MANUFACTURER_HERCULES, dev_info->product_id);
    if (usb_dev) {
      break;
    } else if (dev_info->product_id == founddesc.idProduct) {
      fatal("The controller %04x:%04x could not be opened.\n"
            "Check that you have enough permissions over /dev/bus/usb/ subfolder elements.\n."
            "You might need to create an udev rule at /etc/udev/rules.d/", founddesc.idVendor,founddesc.idProduct);
    }
  }
  if (! usb_dev) {
    if (founddesc.idVendor != MANUFACTURER_HERCULES) {
      fatal("Couldn't find a Hercules controller.");
    }
    else {
      fatal("The controller %04x:%04x is not supported.", founddesc.idVendor,founddesc.idProduct);
    }
    return -1;
  }
	
  // Figure out what it's called
  {
    struct libusb_device_descriptor ddesc;
    name[0]='\0';
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

  ret = libusb_claim_interface(usb_dev, dev_info->interface_number);
  if (ret == 0 ) {
    if (dev_info->ep_in2 != 0x0) {
      libusb_claim_interface(usb_dev, dev_info->interface_number2);
      usb_initiate_transfer_additional();
    }
    usb_initiate_transfer();
    return 0;
  } else {
    if (ret == LIBUSB_ERROR_BUSY) {
      fatal("Couldn't claim interface %d. Already in use?", dev_info->interface_number);
    } else {
      fatal("Couldn't claim interface %d. %s\n%s", dev_info->interface_number,
	    libusb_error_name(ret), libusb_strerror(ret));
    }
    return -1;
  }
}

void
usb_finish() {
  int ret;
  if (usb_dev) {
    ret = libusb_release_interface(usb_dev, dev_info->interface_number);
    if (ret != 0 && ret != LIBUSB_ERROR_NOT_FOUND) {
      warn("Couldn't release interface %d. %s\n%s", dev_info->interface_number,
	   libusb_error_name(ret), libusb_strerror(ret));
    }
    libusb_close(usb_dev);
    usb_dev=NULL;
  }
  libusb_exit(context);
}

void
usb_fd_setup(int *nfds, fd_set *rfds, fd_set *wfds)
{
  usb_fds = libusb_get_pollfds(context);
  if (usb_fds == NULL) {
    warn("could not get the filedescriptors! This is unexpected");
  }

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
 
  if (reads_pending + writes_pending > 100) {
    warn("%d(r)+%d(w) = %d outstanding USB transactions!", reads_pending, writes_pending, reads_pending + writes_pending);
  }
}

void 
usb_check_fds(fd_set *rfds, fd_set *wfds)
{
  if (usb_fds == NULL) {
    return;
  }

  int i;
  for (i = 0; usb_fds[i]; i += 1) {
    int fd = usb_fds[i]->fd;

    if (FD_ISSET(fd, rfds) || FD_ISSET(fd, wfds)) {
      libusb_handle_events(context);
      return;
    }
  }
 
#if LIBUSB_API_VERSION >= 0x01000104
  libusb_free_pollfds(usb_fds);
  usb_fds = NULL;
#endif // LIBUSB_API_VERSION >= 0x01000104
}


void LIBUSB_CALL
usb_write_done(struct libusb_transfer *xfer)
{
  if ( xfer->status == LIBUSB_TRANSFER_TIMED_OUT ) {
    warn("Send timed out");
  } else if ( xfer->status && xfer->status != LIBUSB_TRANSFER_CANCELLED) {
    warn("USB Write status %d", xfer->status);
  }
  writes_pending -= 1;
  usb_debug_msg("Sent", dev_info->ep_out, xfer->buffer, xfer->actual_length);
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
  //timeout if in 1000 milliseconds it hasn't been sent
  xfer->timeout=1000;
  buf = (unsigned char *)malloc(datalen);
  memcpy(buf, data, datalen);
  libusb_fill_bulk_transfer(xfer, usb_dev, dev_info->ep_out, buf, datalen, usb_write_done, NULL, 0);
  libusb_submit_transfer(xfer);

  usb_debug_msg("Preparing to send", dev_info->ep_out, data, datalen);
}
