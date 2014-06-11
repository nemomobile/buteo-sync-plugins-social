TARGET = google-signon-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=GoogleSignonPlugin"
DEFINES += CLASSNAME_H=\\\"googlesignonplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../google-common.pri)
include($$PWD/google-signon.pri)

google_signon_sync_profile.path = /etc/buteo/profiles/sync
google_signon_sync_profile.files = $$PWD/google.Signon.xml
google_signon_client_plugin_xml.path = /etc/buteo/profiles/client
google_signon_client_plugin_xml.files = $$PWD/google-signon.xml

HEADERS += googlesignonplugin.h
SOURCES += googlesignonplugin.cpp

OTHER_FILES += \
    google_signon_sync_profile.files \
    google_signon_client_plugin_xml.files

INSTALLS += \
    target \
    google_signon_sync_profile \
    google_signon_client_plugin_xml
