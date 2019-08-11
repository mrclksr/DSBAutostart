PROGRAM = dsbautostart

isEmpty(PREFIX) {
    PREFIX=/usr/local
}

isEmpty(DATADIR) {
	DATADIR=$${PREFIX}/share/$${PROGRAM}
}

QT	    += widgets
TEMPLATE     = app
TARGET	     = $${PROGRAM}
DEPENDPATH  += . lib qt-helper
INCLUDEPATH += . lib qt-helper
LANGUAGES    = de
APPSDIR	     = $${PREFIX}/share/applications
DEFINES	    += PROGRAM=\\\"$${PROGRAM}\\\" LOCALE_PATH=\\\"$${DATADIR}\\\"
INSTALLS     = target locales desktopfile
QMAKE_POST_LINK = $(STRIP) $(TARGET)
QMAKE_EXTRA_TARGETS += distclean cleanqm readme readmemd

target.files	  = $${PROGRAM}
target.path	  = $${PREFIX}/bin

desktopfile.path  = $${APPSDIR}
desktopfile.files = $${PROGRAM}.desktop

HEADERS += src/list.h \
	   src/listwidget.h \
           src/mainwin.h \
	   src/desktopfile.h \
	   lib/dsbautostart.h \
           lib/qt-helper/qt-helper.h \
           lib/dsbcfg/dsbcfg.h
SOURCES += src/list.cpp \
	   src/listwidget.cpp \
           src/main.cpp \
           src/mainwin.cpp \
	   src/desktopfile.cpp \
	   lib/dsbautostart.c \
           lib/qt-helper/qt-helper.cpp \
           lib/dsbcfg/dsbcfg.c

locales.path = $${DATADIR}


readme.target = readme
readme.files = readme.mdoc
readme.commands = mandoc -mdoc readme.mdoc | perl -e \'foreach (<STDIN>) { \
		\$$_ =~ s/(.)\x08\1/\$$1/g; \$$_ =~ s/_\x08(.)/\$$1/g; \
		print \$$_ \
	}\' | sed \'1,1d; \$$,\$$d\' > README

readmemd.target = readmemd
readmemd.files = readme.mdoc
readmemd.commands = mandoc -mdoc -Tmarkdown readme.mdoc | \
			sed -e \'1,1d; \$$,\$$d\' > README.md

qtPrepareTool(LRELEASE, lrelease)
for(a, LANGUAGES) {
	in = locale/$${PROGRAM}_$${a}.ts
	out = locale/$${PROGRAM}_$${a}.qm
	locales.files += $$out
	cmd = $$LRELEASE $$in -qm $$out
	system($$cmd)
}
cleanqm.commands  = rm -f $${locales.files}
distclean.depends = cleanqm

