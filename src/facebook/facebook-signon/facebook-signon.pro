TARGET = facebook-signon-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=FacebookSignonPlugin"
DEFINES += CLASSNAME_H=\\\"facebooksignonplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../facebook-common.pri)
include($$PWD/facebook-signon.pri)

facebook_signon_sync_profile.path = /etc/buteo/profiles/sync
facebook_signon_sync_profile.files = $$PWD/facebook.Signon.xml
facebook_signon_client_plugin_xml.path = /etc/buteo/profiles/client
facebook_signon_client_plugin_xml.files = $$PWD/facebook-signon.xml

HEADERS += facebooksignonplugin.h
SOURCES += facebooksignonplugin.cpp

OTHER_FILES += \
    facebook_signon_sync_profile.files \
    facebook_signon_client_plugin_xml.files

INSTALLS += \
    target \
    facebook_signon_sync_profile \
    facebook_signon_client_plugin_xml
