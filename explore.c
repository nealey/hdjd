#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <libusb.h>
#include <string.h>

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
	char *name;
	uint16_t product_id;
	uint8_t ep_in;
	uint8_t ep_out;
};

const struct device devices[] = {
	{"Steel", 0xb102, 0x83, 0x04},
	{"MP3e2", 0xb105, 0x82, 0x03},
	{"4Set", 0xb10c, 0x84, 0x02},
	{"4-MX", 0xb109, 0x82, 0x03},
	{0, 0, 0, 0}
};


int
main(int argc, char **argv)
{
	struct libusb_device_handle *dev;
	struct libusb_device_descriptor ddesc;
	unsigned char name[100];
	const struct device *d;
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
		unsigned char *p = name;

		ret = libusb_get_string_descriptor_ascii(dev, ddesc.iManufacturer, name, sizeof(name));
		if (ret > 0) {
			p = name + ret;
			*p = ' ';
			p += 1;
			ret = libusb_get_string_descriptor_ascii(dev, ddesc.iProduct, p, sizeof(name) - ret - 1);
		}

		if (ret < 0) {
			printf("%s: can't figure out what to call this thing.\n", libusb_error_name(ret));
			return 69;
		}
	}
	printf("Opened a %s\n", name);


	while (1) {
		uint8_t data[256];
		int transferred;
		int i;

		if ((ret = libusb_bulk_transfer(dev, d->ep_in, data, sizeof data, &transferred, 0))) {
			break;
		}

		for (i = 0; i < transferred; i += 1) {
			printf("%02x ", data[i]);
		}
		printf("\n");

		{
			uint8_t data[16];
			
			memset(data, 0xff, sizeof data);
			libusb_bulk_transfer(dev, d->ep_out, data, sizeof data, &transferred, 0);
		}

	}

	if (ret < 0) {
		printf("ERROR: %s\n", libusb_error_name(ret));
	}

	libusb_exit(NULL);

	return 0;
}
