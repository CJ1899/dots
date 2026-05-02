CC = gcc
# Added -D_GNU_SOURCE to ensure all system features are available
CFLAGS = -march=native -flto -O3 -Wall $(shell pkg-config --cflags imlib2 x11 xinerama)
LIBS = $(shell pkg-config --libs imlib2 x11 xinerama) -lXext

TARGET = wallman
SRC = wallman.c wall.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

# Generic rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link the target - CRITICAL: $(LIBS) must be at the end
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJ)

install: all
	mkdir -p /usr/local/bin
	cp -f $(TARGET) /usr/local/bin
	chmod 755 /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
