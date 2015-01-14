TARGET = tst_facebook

include(../tst_common.pri)

include($$PWD/../../src/facebook/facebook-common.pri)
include($$PWD/../../src/facebook/facebook-calendars/facebook-calendars.pri)
include($$PWD/../../src/facebook/facebook-contacts/facebook-contacts.pri)
include($$PWD/../../src/facebook/facebook-images/facebook-images.pri)
include($$PWD/../../src/facebook/facebook-notifications/facebook-notifications.pri)

SOURCES += \
    tst_facebook.cpp \
    tst_facebooknetworkstubs_p.cpp
