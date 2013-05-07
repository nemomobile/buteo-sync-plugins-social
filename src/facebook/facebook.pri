CONFIG += link_pkgconfig
PKGCONFIG += accounts-qt libsignon-qt nemonotifications
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
    $$PWD/facebookdatatypesyncadaptor.h \
    $$PWD/facebookimagesyncadaptor.h \
    $$PWD/facebooknotificationsyncadaptor.h \
    $$PWD/facebookpostsyncadaptor.h

SOURCES += \
    $$PWD/facebooksyncadaptor.cpp \
    $$PWD/facebookdatatypesyncadaptor.cpp \
    $$PWD/facebookimagesyncadaptor.cpp \
    $$PWD/facebooknotificationsyncadaptor.cpp \
    $$PWD/facebookpostsyncadaptor.cpp
