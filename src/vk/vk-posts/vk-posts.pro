TARGET = vk-posts-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=VKPostsPlugin"
DEFINES += CLASSNAME_H=\\\"vkpostsplugin.h\\\"
DEFINES += SOCIALD_USE_QTPIM
include($$PWD/../../common.pri)
include($$PWD/../vk-common.pri)
include($$PWD/vk-posts.pri)

PKGCONFIG += mlite5

vk_posts_sync_profile.path = /etc/buteo/profiles/sync
vk_posts_sync_profile.files = $$PWD/vk.Posts.xml
vk_posts_client_plugin_xml.path = /etc/buteo/profiles/client
vk_posts_client_plugin_xml.files = $$PWD/vk-posts.xml
vk_posts_notification_xml.path = /usr/share/lipstick/notificationcategories/
vk_posts_notification_xml.files = $$PWD/x-nemo.social.vk.statuspost.conf

HEADERS += vkpostsplugin.h
SOURCES += vkpostsplugin.cpp

OTHER_FILES += \
    vk.Posts.xml \
    vk-posts.xml \
    x-nemo.social.vk.statuspost.conf

INSTALLS += \
    target \
    vk_posts_sync_profile \
    vk_posts_client_plugin_xml \
    vk_posts_notification_xml
