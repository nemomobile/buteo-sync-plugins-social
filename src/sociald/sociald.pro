TARGET = sociald-client
VERSION = 0.0.81

DEFINES += "CLASSNAME=SocialdPlugin"
DEFINES += CLASSNAME_H=\\\"socialdplugin.h\\\"
include($$PWD/../common.pri)

HEADERS += socialdplugin.h
SOURCES += socialdplugin.cpp

sociald_sync_profile.path = /etc/buteo/profiles/sync
sociald_sync_profile.files = $$PWD/sociald.All.xml
sociald_client_plugin_xml.path = /etc/buteo/profiles/client
sociald_client_plugin_xml.files = $$PWD/sociald.xml

OTHER_FILES += \
    sociald_sync_profile.files \
    sociald_client_plugin_xml.files

INSTALLS += \
    target \
    sociald_sync_profile \
    sociald_client_plugin_xml
