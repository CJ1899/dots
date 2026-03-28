CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lX11
TARGET = x
SRC = x.c
PREFIX = /usr/local/bin

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

install: $(TARGET)
	install -Dm755 $(TARGET) $(PREFIX)/$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all install clean
