TEMPLATE = lib

TARGET = sociald-client
VERSION = 0.0.29
CONFIG += link_pkgconfig plugin

CONFIG += meegotouchevents-qt5
PKGCONFIG += Qt5Contacts \
    libsailfishkeyprovider \
    sailfishaccounts \
    libsignon-qt5 \
    nemonotifications-qt5 \
    buteosyncfw5 \
    libmkcal-qt5 \
    libkcalcoren-qt5 \
    socialcache

DEFINES *= USING_QTPIM
DEFINES *= BEGIN_CONTACTS_NAMESPACE=QT_BEGIN_NAMESPACE_CONTACTS
DEFINES *= END_CONTACTS_NAMESPACE=QT_END_NAMESPACE_CONTACTS
DEFINES *= USE_CONTACTS_NAMESPACE=QTCONTACTS_USE_NAMESPACE

HEADERS += constants_p.h \
    databasemanipulationinterface.h

target.path += /usr/lib/buteo-plugins-qt5

client.path = /etc/buteo/profiles/client
client.files = xml/sociald.xml

sync.path = /etc/buteo/profiles/sync
sync.files = xml/sync/*

QT += \
    network \
    dbus \
    sql

include(facebook/facebook.pri)
include(twitter/twitter.pri)

DEFINES += 'PRIVILEGED_DATA_DIR=\'\"/home/nemo/.local/share/system/privileged/\"\''
DEFINES += 'SYNC_DATABASE_DIR=\'\"Sync\"\''
DEFINES += 'SOCIALD_SYNC_DATABASE_NAME=\'\"sociald.db\"\''

HEADERS += \
    $$PWD/buteosocialsync.h \
    $$PWD/eventfeedhelper_p.h \
    $$PWD/socialnetworksyncadaptor.h \
    $$PWD/syncservice.h \
    $$PWD/syncservice_p.h \
    $$PWD/trace.h \
    $$PWD/internaldatabasemanipulationinterface.h

SOURCES += \
    $$PWD/buteosocialsync.cpp \
    $$PWD/socialnetworksyncadaptor.cpp \
    $$PWD/syncservice.cpp \
    $$PWD/internaldatabasemanipulationinterface.cpp

MOC_DIR = $$PWD/../.moc
OBJECTS_DIR = $$PWD/../.obj

# lipstick event feed subview shared components
shared_eventfeed.files =    $$PWD/eventfeed/SocialAvatar.qml \
                            $$PWD/eventfeed/SocialBody.qml \
                            $$PWD/eventfeed/SocialButton.qml \
                            $$PWD/eventfeed/SocialContent.qml \
                            $$PWD/eventfeed/SocialMediaRow.qml \
                            $$PWD/eventfeed/SocialInfoLabel.qml \
                            $$PWD/eventfeed/SocialComment.qml \
                            $$PWD/eventfeed/SocialReplyField.qml \
                            $$PWD/eventfeed/SocialAccountPullDownMenu.qml \
                            $$PWD/eventfeed/SocialAccountPage.qml
shared_eventfeed.path = /usr/share/lipstick/eventfeed/shared/


# translations
TS_FILE = $$OUT_PWD/sociald.ts
EE_QM = $$OUT_PWD/sociald_eng_en.qm
ts.commands += lupdate -no-recursive $$PWD/facebook $$PWD/twitter -ts $$TS_FILE
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

# lipstick notification categories
notification_categories.files = facebook/x-nemo.social.facebook.notification.conf twitter/x-nemo.social.twitter.mention.conf
notification_categories.path = /usr/share/lipstick/notificationcategories/

INSTALLS += target client sync shared_eventfeed notification_categories ts_install engineering_english_install

