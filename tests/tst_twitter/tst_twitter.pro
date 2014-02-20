TARGET = tst_twitter

include(../tst_common.pri)

include($$PWD/../../src/twitter/twitter-common.pri)
include($$PWD/../../src/twitter/twitter-notifications/twitter-notifications.pri)
include($$PWD/../../src/twitter/twitter-posts/twitter-posts.pri)

SOURCES += \
    tst_twitter.cpp \
    tst_twitternetworkstubs_p.cpp
