CONFIG += link_pkgconfig
PKGCONFIG += accounts-qt libsignon-qt nemonotifications libsailfishkeyprovider
lessThan(QT_MAJOR_VERSION, 5) {
    PKGCONFIG += QJson
}

CONFIG += mobility
MOBILITY += contacts

# possibly temporary? use DBus API instead of meventfeed.h ?
CONFIG += meegotouchevents

INCLUDEPATH += . ..

HEADERS += \
    $$PWD/twittersyncadaptor.h \
    $$PWD/twitterdatatypesyncadaptor.h \
    $$PWD/twittermentiontimelinesyncadaptor.h \
    $$PWD/twitterhometimelinesyncadaptor.h

SOURCES += \
    $$PWD/twittersyncadaptor.cpp \
    $$PWD/twitterdatatypesyncadaptor.cpp \
    $$PWD/twittermentiontimelinesyncadaptor.cpp \
    $$PWD/twitterhometimelinesyncadaptor.cpp
