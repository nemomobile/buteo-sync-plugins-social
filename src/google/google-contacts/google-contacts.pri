CONFIG += link_pkgconfig
PKGCONFIG += Qt5Contacts qtcontacts-sqlite-qt5-extensions
QT += contacts-private

SOURCES += \
    $$PWD/googletwowaycontactsyncadaptor.cpp \
    $$PWD/googlecontactsyncadaptor.cpp \
    $$PWD/googlecontactstream.cpp \
    $$PWD/googlecontactatom.cpp \
    $$PWD/googlecontactimagedownloader.cpp

HEADERS += \
    $$PWD/googletwowaycontactsyncadaptor.h \
    $$PWD/googlecontactsyncadaptor.h \
    $$PWD/googlecontactstream.h \
    $$PWD/googlecontactatom.h \
    $$PWD/googlecontactimagedownloader.h

INCLUDEPATH += $$PWD

