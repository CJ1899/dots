# Makefile for 'wl' wallpaper manager
CC = gcc
CFLAGS = -O2 -Wall
TARGET = wl

LIBS = -lX11 -lImlib2 -lXinerama
SRC = wl.c wall.c

WLR_PROTO_XML = /usr/share/wlr-protocols/unstable/wlr-layer-shell-unstable-v1.xml

ifdef WAYLAND
    SRC += render_wl.c wlr-layer-shell-unstable-v1-client-protocol.c
    LIBS += -lwayland-client
    CFLAGS += -DWAYLAND_SUPPORT

    GEN_HEADERS = wlr-layer-shell-unstable-v1-client-protocol.h
endif

OBJ = $(SRC:.c=.o)

all: $(GEN_HEADERS) $(TARGET)

wlr-layer-shell-unstable-v1-client-protocol.h:
	wayland-scanner client-header $(WLR_PROTO_XML) $@
	wayland-scanner private-code $(WLR_PROTO_XML) wlr-layer-shell-unstable-v1-client-protocol.c

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

clean:
	rm -f $(TARGET) *.o wlr-layer-shell-unstable-v1-client-protocol.[ch]

install: all
	mkdir -p /usr/local/bin
	cp -f $(TARGET) /usr/local/bin
	chmod 755 /usr/local/bin/$(TARGET)

.PHONY: all clean install
