DEFINES += SOCIALD_TEST_DEFINE
DEFINES += 'PRIVILEGED_DATA_DIR=\'\"/tmp/\"\''

INCLUDEPATH += \
    $$PWD/../src/ \
    $$PWD/../src/facebook \
    $$PWD/../src/twitter \
    $$PWD/../src/google

include($$PWD/../src/common.pri)

HEADERS += $$PWD/networkstubs_p.h
SOURCES += $$PWD/networkstubs_p.cpp

QT += testlib
TEMPLATE = app
CONFIG -= app_bundle

target.path = /opt/tests/sociald
INSTALLS += target
