system(qdbusxml2cpp org.nemomobile.sociald.Sync.xml -a syncdbusadaptor -c SyncDBusAdaptor -l SyncService -i syncservice.h)

TEMPLATE = app

TARGET = sociald
VERSION = 0.0.1
CONFIG += eventfeed

target.path += /usr/bin
INSTALLS = target ts_install engineering_english_install

QT += \
    network \
    dbus \
    sql

include(facebook/facebook.pri)

DEFINES += 'SOCIALD_DATABASE_DIR=\'\"/home/nemo/.config/sociald\"\''
DEFINES += 'SOCIALD_DATABASE_NAME=\'\"sociald.db\"\''

HEADERS += \
    $$PWD/socialnetworksyncadaptor.h \
    $$PWD/syncservice.h \
    $$PWD/syncservice_p.h \
    $$PWD/trace.h

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/socialnetworksyncadaptor.cpp \
    $$PWD/syncservice.cpp

MOC_DIR = $$PWD/../.moc
OBJECTS_DIR = $$PWD/../.obj

# translations
TS_FILE = $$OUT_PWD/sociald.ts
EE_QM = $$OUT_PWD/sociald_eng_en.qm
ts.commands += lupdate $$PWD -ts $$TS_FILE
ts.CONFIG += no_check_exist
ts.output = $$TS_FILE
ts.input = .
ts_install.files = $$TS_FILE
ts_install.path = /usr/share/translations/source
ts_install.CONFIG += no_check_exist

# should add -markuntranslated "-" when proper translations are in place (or for testing)
engineering_english.commands += lrelease -idbased $$TS_FILE -qm $$EE_QM
engineering_english.CONFIG += no_check_exist
engineering_english.depends = ts
engineering_english.input = $$TS_FILE
engineering_english.output = $$EE_QM
engineering_english_install.path = /usr/share/translations
engineering_english_install.files = $$EE_QM
engineering_english_install.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS += ts engineering_english
PRE_TARGETDEPS += ts engineering_english
