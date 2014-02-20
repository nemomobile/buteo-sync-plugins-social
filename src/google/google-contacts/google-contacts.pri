CONFIG += link_pkgconfig
PKGCONFIG += Qt5Contacts qtcontacts-sqlite-qt5-extensions

SOURCES += \
    $$PWD/googlecontactsyncadaptor.cpp \
    $$PWD/googlecontactstream.cpp \
    $$PWD/googlecontactatom.cpp

HEADERS += \
    $$PWD/googlecontactsyncadaptor.h \
    $$PWD/googlecontactstream.h \
    $$PWD/googlecontactatom.h

INCLUDEPATH += $$PWD

