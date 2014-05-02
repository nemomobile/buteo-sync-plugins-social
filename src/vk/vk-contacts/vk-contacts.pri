CONFIG += link_pkgconfig
PKGCONFIG += Qt5Contacts qtcontacts-sqlite-qt5-extensions
QT += contacts-private
SOURCES += $$PWD/vkcontactsyncadaptor.cpp $$PWD/vkcontactimagedownloader.cpp
HEADERS += $$PWD/vkcontactsyncadaptor.h $$PWD/vkcontactimagedownloader.h
INCLUDEPATH += $$PWD

