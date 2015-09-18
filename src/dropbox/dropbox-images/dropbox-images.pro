TARGET = dropbox-images-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=DropboxImagesPlugin"
DEFINES += CLASSNAME_H=\\\"dropboximagesplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../dropbox-common.pri)
include($$PWD/dropbox-images.pri)

CONFIG += link_pkgconfig
PKGCONFIG += mlite5

dropbox_images_sync_profile.path = /etc/buteo/profiles/sync
dropbox_images_sync_profile.files = $$PWD/dropbox.Images.xml
dropbox_images_client_plugin_xml.path = /etc/buteo/profiles/client
dropbox_images_client_plugin_xml.files = $$PWD/dropbox-images.xml

HEADERS += dropboximagesplugin.h
SOURCES += dropboximagesplugin.cpp

OTHER_FILES += \
    dropbox_images_sync_profile.files \
    dropbox_images_client_plugin_xml.files

INSTALLS += \
    target \
    dropbox_images_sync_profile \
    dropbox_images_client_plugin_xml
