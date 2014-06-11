TARGET = facebook-notifications-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=FacebookNotificationsPlugin"
DEFINES += CLASSNAME_H=\\\"facebooknotificationsplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../facebook-common.pri)
include($$PWD/facebook-notifications.pri)

facebook_notifications_sync_profile.path = /etc/buteo/profiles/sync
facebook_notifications_sync_profile.files = $$PWD/facebook.Notifications.xml
facebook_notifications_client_plugin_xml.path = /etc/buteo/profiles/client
facebook_notifications_client_plugin_xml.files = $$PWD/facebook-notifications.xml
facebook_notifications_notification_xml.path = /usr/share/lipstick/notificationcategories/
facebook_notifications_notification_xml.files = $$PWD/x-nemo.social.facebook.notification.conf

HEADERS += facebooknotificationsplugin.h
SOURCES += facebooknotificationsplugin.cpp

OTHER_FILES += \
    facebook_notifications_sync_profile.files \
    facebook_notifications_client_plugin_xml.files \
    facebook_notifications_notification_xml.files

INSTALLS += \
    target \
    facebook_notifications_sync_profile \
    facebook_notifications_client_plugin_xml \
    facebook_notifications_notification_xml
