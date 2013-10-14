INCLUDEPATH += . ..

PKGCONFIG += qtcontacts-sqlite-qt5-extensions

SOURCES += \
    $$PWD/googledatatypesyncadaptor.cpp \
    $$PWD/googlecontactsyncadaptor.cpp \
    $$PWD/googlecontactstream.cpp \
    $$PWD/googlecontactatom.cpp

HEADERS += \
    $$PWD/googledatatypesyncadaptor.h \
    $$PWD/googlecontactsyncadaptor.h \
    $$PWD/googlecontactstream.h \
    $$PWD/googlecontactatom.h

OTHER_FILES += google_sync_profiles.files

# google buteo sync profiles
google_sync_profiles.path = /etc/buteo/profiles/sync
google_sync_profiles.files = $$PWD/../xml/sync/google.Contacts.xml

INSTALLS += google_sync_profiles
