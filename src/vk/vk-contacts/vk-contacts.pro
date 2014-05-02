TARGET = vk-contacts-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=VKContactsPlugin"
DEFINES += CLASSNAME_H=\\\"vkcontactsplugin.h\\\"
DEFINES += SOCIALD_USE_QTPIM
include($$PWD/../../common.pri)
include($$PWD/../vk-common.pri)
include($$PWD/vk-contacts.pri)

vk_contacts_sync_profile.path = /etc/buteo/profiles/sync
vk_contacts_sync_profile.files = $$PWD/vk.Contacts.xml
vk_contacts_client_plugin_xml.path = /etc/buteo/profiles/client
vk_contacts_client_plugin_xml.files = $$PWD/vk-contacts.xml

HEADERS += vkcontactsplugin.h
SOURCES += vkcontactsplugin.cpp

OTHER_FILES += \
    vk.Contacts.xml \
    vk-contacts.xml

INSTALLS += \
    target \
    vk_contacts_sync_profile \
    vk_contacts_client_plugin_xml
