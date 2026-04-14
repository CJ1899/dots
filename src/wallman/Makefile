CC = gcc
CFLAGS = -O2 -Wall
TARGET = wallman
SRC = wallman.c wall.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

clean:
	rm -f $(TARGET) $(OBJ)

install: all
	mkdir -p /usr/local/bin
	cp -f $(TARGET) /usr/local/bin
	chmod 755 /usr/local/bin/$(TARGET)

.PHONY: all clean install
