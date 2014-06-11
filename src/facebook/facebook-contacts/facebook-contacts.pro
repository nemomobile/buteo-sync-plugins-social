TARGET = facebook-contacts-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=FacebookContactsPlugin"
DEFINES += CLASSNAME_H=\\\"facebookcontactsplugin.h\\\"
DEFINES += SOCIALD_USE_QTPIM
include($$PWD/../../common.pri)
include($$PWD/../facebook-common.pri)
include($$PWD/facebook-contacts.pri)

facebook_contacts_sync_profile.path = /etc/buteo/profiles/sync
facebook_contacts_sync_profile.files = $$PWD/facebook.Contacts.xml
facebook_contacts_client_plugin_xml.path = /etc/buteo/profiles/client
facebook_contacts_client_plugin_xml.files = $$PWD/facebook-contacts.xml

HEADERS += facebookcontactsplugin.h
SOURCES += facebookcontactsplugin.cpp

OTHER_FILES += \
    facebook_contacts_sync_profile.files \
    facebook_contacts_client_plugin_xml.files

INSTALLS += \
    target \
    facebook_contacts_sync_profile \
    facebook_contacts_client_plugin_xml