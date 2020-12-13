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
TRANSLATIONS = locale/dsbautostart_de.ts \
               locale/dsbautostart_fr.ts
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
	   src/editwin.h \
	   src/listwidget.h \
           src/mainwin.h \
	   src/desktopfile.h \
	   lib/dsbautostart.h \
           lib/qt-helper/qt-helper.h 
SOURCES += src/list.cpp \
	   src/editwin.cpp \
	   src/listwidget.cpp \
           src/main.cpp \
           src/mainwin.cpp \
	   src/desktopfile.cpp \
	   lib/dsbautostart.c \
           lib/qt-helper/qt-helper.cpp

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
for(a, TRANSLATIONS) {
	cmd = $$LRELEASE $${a}
	system($$cmd)
}
locales.files += locale/*.qm

cleanqm.commands  = rm -f $${locales.files}
distclean.depends = cleanqm

