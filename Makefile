PROGRAM	   = dsbautostart
PREFIX	   = /usr/local
BINDIR	   = ${PREFIX}/bin
DATADIR	   = ${PREFIX}/share/${PROGRAM}
LOCALE_DIR = ${PREFIX}/share/locale
SOURCES	   = ${PROGRAM}.c gtk-helper/gtk-helper.c dsbcfg/dsbcfg.c
CFLAGS	   = -Wall `pkg-config gtk+-2.0 --cflags --libs`
CFLAGS	  += -DPROGRAM=\"${PROGRAM}\" -DPATH_LOCALE=\"${LOCALE_DIR}\"
CFLAGS	  += -DPATH_BACKEND=\"${BINDIR}/${PROGRAM}_backend\"

.if !defined(WITHOUT_GETTEXT)
CFLAGS += -DWITH_GETTEXT
.endif

all: ${PROGRAM}

${PROGRAM}: ${SOURCES}
	${CC} -o ${PROGRAM} ${CFLAGS} ${SOURCES}

install: ${PROGRAM}
	install -g wheel -m 0755 -o root ${PROGRAM} ${BINDIR}
	install -g wheel -m 0644 -o root ${PROGRAM}.desktop \
		${PREFIX}/share/applications
	for i in locale/*.po; do \
		msgfmt -c -v -o locale/${PROGRAM}.mo $$i; \
		install -g wheel -m 0644 -o root locale/${PROGRAM}.mo \
		${LOCALE_DIR}/`basename $${i%.po}`/LC_MESSAGES/${PROGRAM}.mo; \
        done
clean:
	-rm -f ${PROGRAM}
	-rm -f locale/${PROGRAM}.mo

