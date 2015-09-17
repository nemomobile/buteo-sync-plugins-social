TARGET = onedrive-backup-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=OneDriveBackupPlugin"
DEFINES += CLASSNAME_H=\\\"onedrivebackupplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../onedrive-common.pri)
include($$PWD/onedrive-backup.pri)

onedrive_backup_sync_profile.path = /etc/buteo/profiles/sync
onedrive_backup_sync_profile.files = $$PWD/onedrive.Backup.xml
onedrive_backup_client_plugin_xml.path = /etc/buteo/profiles/client
onedrive_backup_client_plugin_xml.files = $$PWD/onedrive-backup.xml

HEADERS += onedrivebackupplugin.h
SOURCES += onedrivebackupplugin.cpp

OTHER_FILES += \
    onedrive_backup_sync_profile.files \
    onedrive_backup_client_plugin_xml.files

INSTALLS += \
    target \
    onedrive_backup_sync_profile \
    onedrive_backup_client_plugin_xml
