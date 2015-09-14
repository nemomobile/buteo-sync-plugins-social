TARGET = onedrive-images-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=OneDriveImagesPlugin"
DEFINES += CLASSNAME_H=\\\"onedriveimagesplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../onedrive-common.pri)
include($$PWD/onedrive-images.pri)

CONFIG += link_pkgconfig
PKGCONFIG += mlite5

onedrive_images_sync_profile.path = /etc/buteo/profiles/sync
onedrive_images_sync_profile.files = $$PWD/onedrive.Images.xml
onedrive_images_client_plugin_xml.path = /etc/buteo/profiles/client
onedrive_images_client_plugin_xml.files = $$PWD/onedrive-images.xml

HEADERS += onedriveimagesplugin.h
SOURCES += onedriveimagesplugin.cpp

OTHER_FILES += \
    onedrive_images_sync_profile.files \
    onedrive_images_client_plugin_xml.files

INSTALLS += \
    target \
    onedrive_images_sync_profile \
    onedrive_images_client_plugin_xml
