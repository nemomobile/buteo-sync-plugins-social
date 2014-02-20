TEMPLATE = lib

TARGET = google-contacts-client
VERSION = 0.0.1
CONFIG += plugin

DEFINES += SOCIALD_USE_QTPIM
include($$PWD/../../common.pri)
include($$PWD/../google-common.pri)
include($$PWD/google-contacts.pri)

target.path += /usr/lib/buteo-plugins-qt5
google_contacts_sync_profile.path = /etc/buteo/profiles/sync
google_contacts_sync_profile.files = $$PWD/google.Contacts.xml
google_contacts_client_plugin_xml.path = /etc/buteo/profiles/client
google_contacts_client_plugin_xml.files = $$PWD/google-contacts.xml

HEADERS += googlecontactsplugin.h
SOURCES += googlecontactsplugin.cpp

OTHER_FILES += \
    google_contacts_sync_profile.files \
    google_contacts_client_plugin_xml.files

INSTALLS += \
    target \
    google_contacts_sync_profile \
    google_contacts_client_plugin_xml

