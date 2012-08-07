all: hdjd aac123

hdjd: CFLAGS += $(shell pkg-config --cflags libusb-1.0)
hdjd: LDFLAGS += $(shell pkg-config --libs libusb-1.0)


aac123: CFLAGS += $(shell pkg-config --cflags alsa)
aac123: LDLIBS += $(shell pkg-config --libs alsa)
aac123: LDLIBS += -lfaad -lmp4ff
