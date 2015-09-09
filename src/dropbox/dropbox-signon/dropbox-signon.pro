TARGET = dropbox-signon-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=DropboxSignonPlugin"
DEFINES += CLASSNAME_H=\\\"dropboxsignonplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../dropbox-common.pri)
include($$PWD/dropbox-signon.pri)

dropbox_signon_sync_profile.path = /etc/buteo/profiles/sync
dropbox_signon_sync_profile.files = $$PWD/dropbox.Signon.xml
dropbox_signon_client_plugin_xml.path = /etc/buteo/profiles/client
dropbox_signon_client_plugin_xml.files = $$PWD/dropbox-signon.xml

HEADERS += dropboxsignonplugin.h
SOURCES += dropboxsignonplugin.cpp

OTHER_FILES += \
    dropbox_signon_sync_profile.files \
    dropbox_signon_client_plugin_xml.files

INSTALLS += \
    target \
    dropbox_signon_sync_profile \
    dropbox_signon_client_plugin_xml
