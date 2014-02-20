TEMPLATE = lib

TARGET = google-calendars-client
VERSION = 0.0.1
CONFIG += plugin

include($$PWD/../../common.pri)
include($$PWD/../google-common.pri)
include($$PWD/google-calendars.pri)

target.path += /usr/lib/buteo-plugins-qt5
google_calendars_sync_profile.path = /etc/buteo/profiles/sync
google_calendars_sync_profile.files = $$PWD/google.Calendars.xml
google_calendars_client_plugin_xml.path = /etc/buteo/profiles/client
google_calendars_client_plugin_xml.files = $$PWD/google-calendars.xml

HEADERS += googlecalendarsplugin.h
SOURCES += googlecalendarsplugin.cpp

OTHER_FILES += \
    google_calendars_sync_profile.files \
    google_calendars_client_plugin_xml.files

INSTALLS += \
    target \
    google_calendars_sync_profile \
    google_calendars_client_plugin_xml
