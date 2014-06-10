QMAKE_CXXFLAGS += -Werror
CONFIG += link_pkgconfig
PKGCONFIG += \
    libsailfishkeyprovider \
    libsignon-qt5 \
    accounts-qt5 \
    buteosyncfw5 \
    socialcache

QT += \
    network \
    dbus \
    sql

# the unit tests need to provide a custom QNAM and uses a different database directory
HEADERS += $$PWD/common/socialdnetworkaccessmanager_p.h
!contains(DEFINES, 'SOCIALD_TEST_DEFINE') {
    SOURCES += $$PWD/common/socialdnetworkaccessmanager_p.cpp
    DEFINES += 'PRIVILEGED_DATA_DIR=\'\"/home/nemo/.local/share/system/privileged/\"\''
}

DEFINES += 'SYNC_DATABASE_DIR=\'\"Sync\"\''
DEFINES += 'SOCIALD_SYNC_DATABASE_NAME=\'\"sociald.db\"\''

INCLUDEPATH += . $$PWD/common/

HEADERS += \
    $$PWD/common/buteosyncfw_p.h \
    $$PWD/common/socialdbuteoplugin.h \
    $$PWD/common/socialnetworksyncadaptor.h \
    $$PWD/common/trace.h

SOURCES += \
    $$PWD/common/socialdbuteoplugin.cpp \
    $$PWD/common/socialnetworksyncadaptor.cpp

contains(DEFINES, 'SOCIALD_USE_QTPIM') {
    DEFINES *= USE_CONTACTS_NAMESPACE=QTCONTACTS_USE_NAMESPACE
    PKGCONFIG += Qt5Contacts qtcontacts-sqlite-qt5-extensions
    HEADERS += $$PWD/common/constants_p.h
}

#NOTE: This causes issues with the unit tests ?
#MOC_DIR = $$PWD/../.moc
#OBJECTS_DIR = $$PWD/../.obj
