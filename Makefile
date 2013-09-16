CFLAGS += -Wall
CFLAGS += -Werror
TARGETS = hdjd aac123 explore
CFLAGS += -g

all: $(TARGETS)

hdjd: LDFLAGS += $(shell pkg-config --libs libusb-1.0)
hdjd: LDFLAGS += $(shell pkg-config --libs alsa)
hdjd: hdjd.o usb.o alsa.o

explore: LDFLAGS += $(shell pkg-config --libs libusb-1.0)
explore.o: CFLAGS += $(shell pkg-config --cflags libusb-1.0)

alsa.o: CFLAGS += $(shell pkg-config --cflags alsa)
usb.o: CFLAGS += $(shell pkg-config --cflags libusb-1.0)

aac123: CFLAGS += $(shell pkg-config --cflags alsa)
aac123: LDLIBS += $(shell pkg-config --libs alsa)
aac123: LDLIBS += -lfaad -lmp4v2

aactest: CFLAGS += $(shell pkg-config --cflags alsa)
aactest: LDLIBS += $(shell pkg-config --libs alsa)
aactest: LDLIBS += -lfaad -lmp4v2

clean:
	rm -f $(TARGETS) *.o
