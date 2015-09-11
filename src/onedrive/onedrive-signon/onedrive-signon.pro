TARGET = onedrive-signon-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=OneDriveSignonPlugin"
DEFINES += CLASSNAME_H=\\\"onedrivesignonplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../onedrive-common.pri)
include($$PWD/onedrive-signon.pri)

onedrive_signon_sync_profile.path = /etc/buteo/profiles/sync
onedrive_signon_sync_profile.files = $$PWD/onedrive.Signon.xml
onedrive_signon_client_plugin_xml.path = /etc/buteo/profiles/client
onedrive_signon_client_plugin_xml.files = $$PWD/onedrive-signon.xml

HEADERS += onedrivesignonplugin.h
SOURCES += onedrivesignonplugin.cpp

OTHER_FILES += \
    onedrive_signon_sync_profile.files \
    onedrive_signon_client_plugin_xml.files

INSTALLS += \
    target \
    onedrive_signon_sync_profile \
    onedrive_signon_client_plugin_xml
