CONFIG += link_pkgconfig
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

QT += \
    network \
    dbus \
    sql

include($$PWD/facebook/facebook.pri)
include($$PWD/twitter/twitter.pri)
include($$PWD/google/google.pri)

# the unit tests need to provide a custom QNAM and uses a different database directory
HEADERS += $$PWD/socialdnetworkaccessmanager_p.h
!contains(DEFINES, 'SOCIALD_TEST_DEFINE') {
    SOURCES += $$PWD/socialdnetworkaccessmanager_p.cpp
    DEFINES += 'PRIVILEGED_DATA_DIR=\'\"/home/nemo/.local/share/system/privileged/\"\''
}

DEFINES += 'SYNC_DATABASE_DIR=\'\"Sync\"\''
DEFINES += 'SOCIALD_SYNC_DATABASE_NAME=\'\"sociald.db\"\''

HEADERS += \
    $$PWD/constants_p.h \
    $$PWD/buteosocialsync.h \
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

#This causes issues with the unit tests
#MOC_DIR = $$PWD/../.moc
#OBJECTS_DIR = $$PWD/../.obj

target.path += /usr/lib/buteo-plugins-qt5
client.path = /etc/buteo/profiles/client
client.files = xml/sociald.xml

OTHER_FILES += shared_eventfeed.files

# lipstick event feed subview shared components
shared_eventfeed.files = \
    $$PWD/eventfeed/SocialAvatar.qml \
    $$PWD/eventfeed/SocialBody.qml \
    $$PWD/eventfeed/SocialButton.qml \
    $$PWD/eventfeed/SocialContent.qml \
    $$PWD/eventfeed/SocialMediaRow.qml \
    $$PWD/eventfeed/SocialImage.qml \
    $$PWD/eventfeed/SocialInfoLabel.qml \
    $$PWD/eventfeed/SocialComment.qml \
    $$PWD/eventfeed/SocialReplyField.qml \
    $$PWD/eventfeed/SocialAccountPullDownMenu.qml \
    $$PWD/eventfeed/SocialAccountPage.qml \
    $$PWD/eventfeed/SocialStatusUpdater.qml \
    $$PWD/eventfeed/SocialMediaFeedPage.qml \
    $$PWD/eventfeed/SocialMediaFeedItem.qml \
    $$PWD/eventfeed/SocialMediaAccountDelegate.qml \
    $$PWD/eventfeed/SocialMediaIndicator.qml \
    $$PWD/eventfeed/SocialMediaPreviewRow.qml
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

INSTALLS += target client shared_eventfeed ts_install engineering_english_install

