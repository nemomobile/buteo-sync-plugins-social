TARGET = vk-images-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=VKImagesPlugin"
DEFINES += CLASSNAME_H=\\\"vkimagesplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../vk-common.pri)
include($$PWD/vk-images.pri)

vk_images_sync_profile.path = /etc/buteo/profiles/sync
vk_images_sync_profile.files = $$PWD/vk.Images.xml
vk_images_client_plugin_xml.path = /etc/buteo/profiles/client
vk_images_client_plugin_xml.files = $$PWD/vk-images.xml

HEADERS += vkimagesplugin.h
SOURCES += vkimagesplugin.cpp

OTHER_FILES += \
    vk_images_sync_profile.files \
    vk_images_client_plugin_xml.files

INSTALLS += \
    target \
    vk_images_sync_profile \
    vk_images_client_plugin_xml
