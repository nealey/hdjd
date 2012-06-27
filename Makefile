CFLAGS=$(shell pkg-config --cflags libusb-1.0)
LDFLAGS=$(shell pkg-config --libs libusb-1.0)

all: hdjd
