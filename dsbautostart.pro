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
QMAKE_EXTRA_TARGETS += distclean cleanqm

target.files	  = $${PROGRAM}
target.path	  = $${PREFIX}/bin

desktopfile.path  = $${APPSDIR}
desktopfile.files = $${PROGRAM}.desktop

HEADERS += src/list.h \
           src/mainwin.h \
           lib/dsbautostart.h \
           lib/qt-helper/qt-helper.h \
           ../dsbcfg/dsbcfg.h
SOURCES += src/list.cpp \
           src/main.cpp \
           src/mainwin.cpp \
           lib/dsbautostart.c \
           lib/qt-helper/qt-helper.cpp \
           ../dsbcfg/dsbcfg.c

locales.path = $${DATADIR}

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

