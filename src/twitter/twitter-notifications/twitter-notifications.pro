TARGET = twitter-notifications-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=TwitterNotificationsPlugin"
DEFINES += CLASSNAME_H=\\\"twitternotificationsplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../twitter-common.pri)
include($$PWD/twitter-notifications.pri)

twitter_notifications_sync_profile.path = /etc/buteo/profiles/sync
twitter_notifications_sync_profile.files = $$PWD/twitter.Notifications.xml
twitter_notifications_client_plugin_xml.path = /etc/buteo/profiles/client
twitter_notifications_client_plugin_xml.files = $$PWD/twitter-notifications.xml
twitter_notifications_notification_xml.path = /usr/share/lipstick/notificationcategories/
twitter_notifications_notification_xml.files = \
    $$PWD/x-nemo.social.twitter.mention.conf \
    $$PWD/x-nemo.social.twitter.retweet.conf \
    $$PWD/x-nemo.social.twitter.follower.conf

HEADERS += twitternotificationsplugin.h
SOURCES += twitternotificationsplugin.cpp

OTHER_FILES += \
    twitter_notifications_sync_profile.files \
    twitter_notifications_client_plugin_xml.files \
    twitter_notifications_notification_xml.files

# translations
TWITTER_TS_FILE = $$OUT_PWD/lipstick-jolla-home-twitter-notif.ts
TWITTER_EE_QM = $$OUT_PWD/lipstick-jolla-home-twitter-notif_eng_en.qm
twitter_ts.commands += lupdate $$PWD -ts $$TWITTER_TS_FILE
twitter_ts.CONFIG += no_check_exist
twitter_ts.output = $$TWITTER_TS_FILE
twitter_ts.input = $$PWD
twitter_ts_install.files = $$TWITTER_TS_FILE
twitter_ts_install.path = /usr/share/translations/source
twitter_ts_install.CONFIG += no_check_exist

# should add -markuntranslated "-" when proper translations are in place (or for testing)
twitter_engineering_english.commands += lrelease -idbased $$TWITTER_TS_FILE -qm $$TWITTER_EE_QM
twitter_engineering_english.CONFIG += no_check_exist
twitter_engineering_english.depends = twitter_ts
twitter_engineering_english.input = $$TWITTER_TS_FILE
twitter_engineering_english.output = $$TWITTER_EE_QM
twitter_engineering_english_install.path = /usr/share/translations
twitter_engineering_english_install.files = $$TWITTER_EE_QM
twitter_engineering_english_install.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS += twitter_ts twitter_engineering_english
PRE_TARGETDEPS += twitter_ts twitter_engineering_english

INSTALLS += \
    target \
    twitter_notifications_sync_profile \
    twitter_notifications_client_plugin_xml \
    twitter_notifications_notification_xml \
    twitter_ts_install \
    twitter_engineering_english_install
