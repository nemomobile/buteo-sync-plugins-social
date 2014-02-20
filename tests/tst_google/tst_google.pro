TARGET = tst_google

include(../tst_common.pri)

include($$PWD/../../src/google/google-common.pri)
include($$PWD/../../src/google/google-calendars/google-calendars.pri)
include($$PWD/../../src/google/google-contacts/google-contacts.pri)

SOURCES += \
    tst_google.cpp \
    tst_googlenetworkstubs_p.cpp
