TEMPLATE = lib

TARGET = vk-calendars-client
VERSION = 0.0.1
CONFIG += plugin

include($$PWD/../../common.pri)
include($$PWD/../vk-common.pri)
include($$PWD/vk-calendars.pri)

target.path += /usr/lib/buteo-plugins-qt5
vk_calendars_sync_profile.path = /etc/buteo/profiles/sync
vk_calendars_sync_profile.files = $$PWD/vk.Calendars.xml
vk_calendars_client_plugin_xml.path = /etc/buteo/profiles/client
vk_calendars_client_plugin_xml.files = $$PWD/vk-calendars.xml

HEADERS += vkcalendarsplugin.h
SOURCES += vkcalendarsplugin.cpp

OTHER_FILES += \
    vk_calendars_sync_profile.files \
    vk_calendars_client_plugin_xml.files

INSTALLS += \
    target \
    vk_calendars_sync_profile \
    vk_calendars_client_plugin_xml