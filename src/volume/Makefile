CC      = gcc
CFLAGS  = -O3 -march=native -Wall `pkg-config --cflags libnotify`
LIBS    = -lasound `pkg-config --libs libnotify`
PREFIX  = /usr/local
SRC     = volume.c
OBJ     = volume

all: $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OBJ) $(LIBS)

install: all
	@echo "Installing to $(PREFIX)/bin"
	@install -Dm755 $(OBJ) $(PREFIX)/bin/$(OBJ)

uninstall:
	@echo "Removing from $(PREFIX)/bin"
	@rm -f $(PREFIX)/bin/$(OBJ)

clean:
	@echo "Cleaning build artifacts"
	@rm -f $(OBJ)

.PHONY: all install uninstall clean
