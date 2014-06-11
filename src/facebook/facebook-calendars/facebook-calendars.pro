TARGET = facebook-calendars-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=FacebookCalendarsPlugin"
DEFINES += CLASSNAME_H=\\\"facebookcalendarsplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../facebook-common.pri)
include($$PWD/facebook-calendars.pri)

facebook_calendars_sync_profile.path = /etc/buteo/profiles/sync
facebook_calendars_sync_profile.files = $$PWD/facebook.Calendars.xml
facebook_calendars_client_plugin_xml.path = /etc/buteo/profiles/client
facebook_calendars_client_plugin_xml.files = $$PWD/facebook-calendars.xml

HEADERS += facebookcalendarsplugin.h
SOURCES += facebookcalendarsplugin.cpp

OTHER_FILES += \
    facebook_calendars_sync_profile.files \
    facebook_calendars_client_plugin_xml.files

INSTALLS += \
    target \
    facebook_calendars_sync_profile \
    facebook_calendars_client_plugin_xml