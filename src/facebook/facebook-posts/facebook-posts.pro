TEMPLATE = lib

TARGET = facebook-posts-client
VERSION = 0.0.1
CONFIG += plugin

DEFINES += SOCIALD_USE_QTPIM
include($$PWD/../../common.pri)
include($$PWD/../facebook-common.pri)
include($$PWD/facebook-posts.pri)

target.path += /usr/lib/buteo-plugins-qt5
facebook_posts_sync_profile.path = /etc/buteo/profiles/sync
facebook_posts_sync_profile.files = $$PWD/facebook.Posts.xml
facebook_posts_client_plugin_xml.path = /etc/buteo/profiles/client
facebook_posts_client_plugin_xml.files = $$PWD/facebook-posts.xml
facebook_posts_notification_xml.path = /usr/share/lipstick/notificationcategories/
facebook_posts_notification_xml.files = $$PWD/x-nemo.social.facebook.statuspost.conf

HEADERS += facebookpostsplugin.h
SOURCES += facebookpostsplugin.cpp

OTHER_FILES += \
    facebook_posts_sync_profile.files \
    facebook_posts_client_plugin_xml.files \
    facebook_posts_notification_xml.files

INSTALLS += \
    facebook_posts_notification_xml
# for now, don't install the sync plugin or profiles - JB#13153
#    target \
#    facebook_posts_sync_profile \
#    facebook_posts_client_plugin_xml \
