TEMPLATE = lib

TARGET = twitter-notifications-client
VERSION = 0.0.1
CONFIG += plugin

include($$PWD/../../common.pri)
include($$PWD/../twitter-common.pri)
include($$PWD/twitter-notifications.pri)

target.path += /usr/lib/buteo-plugins-qt5
twitter_notifications_sync_profile.path = /etc/buteo/profiles/sync
twitter_notifications_sync_profile.files = $$PWD/twitter.Notifications.xml
twitter_notifications_client_plugin_xml.path = /etc/buteo/profiles/client
twitter_notifications_client_plugin_xml.files = $$PWD/twitter-notifications.xml
twitter_notifications_notification_xml.path = /usr/share/lipstick/notificationcategories/
twitter_notifications_notification_xml.files = $$PWD/x-nemo.social.twitter.mention.conf

HEADERS += twitternotificationsplugin.h
SOURCES += twitternotificationsplugin.cpp

OTHER_FILES += \
    twitter_notifications_sync_profile.files \
    twitter_notifications_client_plugin_xml.files \
    twitter_notifications_notification_xml.files

INSTALLS += \
    target \
    twitter_notifications_sync_profile \
    twitter_notifications_client_plugin_xml \
    twitter_notifications_notification_xml
