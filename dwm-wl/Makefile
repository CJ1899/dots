include config.mk

SRC = drw.c dwm.c util.c wall.c

OBJ = ${SRC:.c=.o}

all: dwm

.c.o:
	${CC} -c ${CFLAGS} ${CPPFLAGS} $<

${OBJ}: config.h config.mk

# Use LIBS specifically to ensure correct order
dwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS} ${LIBS}

clean:
	rm -f dwm ${OBJ} wall.o dwm-${VERSION}.tar.gz

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm

.PHONY: all clean dist install uninstall
