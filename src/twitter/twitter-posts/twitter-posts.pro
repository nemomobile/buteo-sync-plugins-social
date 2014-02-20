TEMPLATE = lib

TARGET = twitter-posts-client
VERSION = 0.0.1
CONFIG += plugin

include($$PWD/../../common.pri)
include($$PWD/../twitter-common.pri)
include($$PWD/twitter-posts.pri)

target.path += /usr/lib/buteo-plugins-qt5
twitter_posts_sync_profile.path = /etc/buteo/profiles/sync
twitter_posts_sync_profile.files = $$PWD/twitter.Posts.xml
twitter_posts_client_plugin_xml.path = /etc/buteo/profiles/client
twitter_posts_client_plugin_xml.files = $$PWD/twitter-posts.xml
twitter_posts_notification_xml.path = /usr/share/lipstick/notificationcategories/
twitter_posts_notification_xml.files = $$PWD/x-nemo.social.twitter.tweet.conf

HEADERS += twitterpostsplugin.h
SOURCES += twitterpostsplugin.cpp

OTHER_FILES += \
    twitter_posts_sync_profile.files \
    twitter_posts_client_plugin_xml.files \
    twitter_posts_notification_xml.files

INSTALLS += \
    target \
    twitter_posts_sync_profile \
    twitter_posts_client_plugin_xml \
    twitter_posts_notification_xml
