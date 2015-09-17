TARGET = dropbox-backup-client
VERSION = 0.0.1

DEFINES += "CLASSNAME=DropboxBackupPlugin"
DEFINES += CLASSNAME_H=\\\"dropboxbackupplugin.h\\\"
include($$PWD/../../common.pri)
include($$PWD/../dropbox-common.pri)
include($$PWD/dropbox-backup.pri)

dropbox_backup_sync_profile.path = /etc/buteo/profiles/sync
dropbox_backup_sync_profile.files = $$PWD/dropbox.Backup.xml
dropbox_backup_client_plugin_xml.path = /etc/buteo/profiles/client
dropbox_backup_client_plugin_xml.files = $$PWD/dropbox-backup.xml

HEADERS += dropboxbackupplugin.h
SOURCES += dropboxbackupplugin.cpp

OTHER_FILES += \
    dropbox_backup_sync_profile.files \
    dropbox_backup_client_plugin_xml.files

INSTALLS += \
    target \
    dropbox_backup_sync_profile \
    dropbox_backup_client_plugin_xml
