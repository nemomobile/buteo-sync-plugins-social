TARGET = facebook-images-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=FacebookImagesPlugin"
DEFINES += CLASSNAME_H=\\\"facebookimagesplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../facebook-common.pri)
include($$PWD/facebook-images.pri)

CONFIG += link_pkgconfig
PKGCONFIG += mlite5

facebook_images_sync_profile.path = /etc/buteo/profiles/sync
facebook_images_sync_profile.files = $$PWD/facebook.Images.xml
facebook_images_client_plugin_xml.path = /etc/buteo/profiles/client
facebook_images_client_plugin_xml.files = $$PWD/facebook-images.xml

HEADERS += facebookimagesplugin.h
SOURCES += facebookimagesplugin.cpp

OTHER_FILES += \
    facebook_images_sync_profile.files \
    facebook_images_client_plugin_xml.files

INSTALLS += \
    target \
    facebook_images_sync_profile \
    facebook_images_client_plugin_xml
