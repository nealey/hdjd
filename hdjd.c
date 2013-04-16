#include <stdio.h>
#include <stdint.h>
#include <libusb.h>
#include <alsa/asoundlib.h>

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
	{ 0xb102, 0x83, 0x04 },
	{ 0xb105, 0x82, 0x03 },
	{ 0, 0, 0 }
};


int
main(int argc, char **argv)
{
	static snd_seq_t *handle;
	struct libusb_device_handle *dev;
	struct libusb_device_descriptor ddesc;
	char name[100];
	const struct device *d;
	snd_midi_event_t *midi_event_parser;
	int seq_port;
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
	snd_midi_event_new(256, &midi_event_parser);

	while (1) {
		uint8_t data[80];
		int transferred;
		int i;
		snd_seq_event_t ev;

		if ((ret = libusb_bulk_transfer(dev, 0x83, data, sizeof data, &transferred, 0))) {
			break;
		}

		for (i = 0; i < transferred; i += 1) {
			printf("%02x ", data[i]);
		}
		
		snd_midi_event_encode(midi_event_parser, data, transferred, &ev);
		snd_seq_ev_set_direct(&ev);
		snd_seq_ev_set_source(&ev, seq_port);
		snd_seq_ev_set_subs(&ev);
		snd_seq_event_output(handle, &ev);	
		printf("\n");
		snd_seq_drain_output(handle);
	}

	if (ret < 0) {
		printf("ERROR: %s\n", libusb_error_name(ret));
	}

	libusb_exit(NULL);

	return 0;
}
