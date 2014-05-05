TARGET = vk-calendars-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=VKCalendarsPlugin"
DEFINES += CLASSNAME_H=\\\"vkcalendarsplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../vk-common.pri)
include($$PWD/vk-calendars.pri)

vk_calendars_sync_profile.path = /etc/buteo/profiles/sync
vk_calendars_sync_profile.files = $$PWD/vk.Calendars.xml
vk_calendars_client_plugin_xml.path = /etc/buteo/profiles/client
vk_calendars_client_plugin_xml.files = $$PWD/vk-calendars.xml

HEADERS += vkcalendarsplugin.h
SOURCES += vkcalendarsplugin.cpp

OTHER_FILES += \
    vk.Calendars.xml \
    vk-calendars.xml

INSTALLS += \
    target \
    vk_calendars_sync_profile \
    vk_calendars_client_plugin_xml
