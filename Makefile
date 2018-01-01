CFLAGS += -Wall
CFLAGS += -Werror
TARGETS = hdjd explore

ifdef DEBUG
CFLAGS += -g -DDEBUG
endif

all: $(TARGETS)

hdjd: LDLIBS += $(shell pkg-config --libs libusb-1.0)
hdjd: LDLIBS += $(shell pkg-config --libs alsa)
hdjd: hdjd.o usb.o alsa.o

explore: LDLIBS += $(shell pkg-config --libs libusb-1.0)
explore.o: CFLAGS += $(shell pkg-config --cflags libusb-1.0)

alsa.o: CFLAGS += $(shell pkg-config --cflags alsa)
usb.o: CFLAGS += $(shell pkg-config --cflags libusb-1.0)

clean:
	rm -f $(TARGETS) *.o
