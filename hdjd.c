#include <stdio.h>
#include <stdint.h>
#include <libusb.h>

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

// Steel: 0xb102, 0x83, 0x03
// MP3e2: 0x0b105, 0x82, 
// 4set: 0xb10c, 0x84, 0x02


int
main(int argc, char **argv)
{
	struct libusb_device_handle *handle;
	int ret;

	if (libusb_init(NULL) < 0) {
		return 69;
	}

	handle = libusb_open_device_with_vid_pid(NULL, 0x06f8, 0xb102);
	if (!handle) {
		printf("Couldn't find a controller\n");
		return 69;
	}

	while (1) {
		uint8_t data[80];
		int transferred;
		int i;

		if ((ret = libusb_bulk_transfer(handle, 0x83, data, sizeof data, &transferred, 0))) {
			break;
		}

		for (i = 0; i < transferred; i += 1) {
			printf("%02x ", data[i]);
		}
		printf("\n");

		// Cram it back out, to turn that light on
		if ((ret = libusb_bulk_transfer(handle, 0x04, data, transferred, &transferred, 0))) {
			break;
		}

	}

	if (ret < 0) {
		printf("ERROR: %s\n", libusb_error_name(ret));
	}

	libusb_exit(NULL);

	return 0;
}
