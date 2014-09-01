TARGET = vk-notifications-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=VKNotificationsPlugin"
DEFINES += CLASSNAME_H=\\\"vknotificationsplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../vk-common.pri)
include($$PWD/vk-notifications.pri)

vk_notifications_sync_profile.path = /etc/buteo/profiles/sync
vk_notifications_sync_profile.files = $$PWD/vk.Notifications.xml
vk_notifications_client_plugin_xml.path = /etc/buteo/profiles/client
vk_notifications_client_plugin_xml.files = $$PWD/vk-notifications.xml
vk_notifications_notification_xml.path = /usr/share/lipstick/notificationcategories/
vk_notifications_notification_xml.files = $$PWD/x-nemo.social.vk.notification.conf

HEADERS += vknotificationsplugin.h
SOURCES += vknotificationsplugin.cpp

OTHER_FILES += \
    vk_notifications_sync_profile.files \
    vk_notifications_client_plugin_xml.files \
    vk_notifications_notification_xml.files

INSTALLS += \
    target \
    vk_notifications_sync_profile \
    vk_notifications_client_plugin_xml \
    vk_notifications_notification_xml
