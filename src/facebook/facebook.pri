CONFIG += link_pkgconfig
PKGCONFIG += accounts-qt libsignon-qt
lessThan(QT_MAJOR_VERSION, 5) {
    PKGCONFIG += QJson
}

CONFIG += mobility
MOBILITY += contacts

# possibly temporary? use DBus API instead of meventfeed.h ?
CONFIG += meegotouchevents

INCLUDEPATH += . ..

HEADERS += \
    $$PWD/facebooksyncadaptor.h \
    $$PWD/facebooknotificationsyncadaptor.h

SOURCES += \
    $$PWD/facebooksyncadaptor.cpp \
    $$PWD/facebooknotificationsyncadaptor.cpp
