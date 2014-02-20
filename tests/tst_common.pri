DEFINES += SOCIALD_TEST_DEFINE
DEFINES += 'PRIVILEGED_DATA_DIR=\'\"/tmp/\"\''

DEFINES += SOCIALD_USE_QTPIM
include($$PWD/../src/common.pri)

INCLUDEPATH += $$PWD
HEADERS += $$PWD/networkstubs_p.h
SOURCES += $$PWD/networkstubs_p.cpp

QT += testlib
TEMPLATE = app
CONFIG -= app_bundle

target.path = /opt/tests/sociald
INSTALLS += target
