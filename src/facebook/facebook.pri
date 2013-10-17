INCLUDEPATH += . ..

HEADERS += \
    $$PWD/facebookdatatypesyncadaptor.h \
    $$PWD/facebookcontactsyncadaptor.h \
    $$PWD/facebookimagesyncadaptor.h \
    $$PWD/facebooknotificationsyncadaptor.h \
    $$PWD/facebookpostsyncadaptor.h \
    $$PWD/facebookcalendartypesyncadaptor.h

SOURCES += \
    $$PWD/facebookdatatypesyncadaptor.cpp \
    $$PWD/facebookcontactsyncadaptor.cpp \
    $$PWD/facebookimagesyncadaptor.cpp \
    $$PWD/facebooknotificationsyncadaptor.cpp \
    $$PWD/facebookpostsyncadaptor.cpp \
    $$PWD/facebookcalendartypesyncadaptor.cpp

OTHER_FILES += \
    facebook_sync_profiles.files \
    facebook_eventfeed.files \
    facebook_notification_category.files

# facebook buteo sync profiles
facebook_sync_profiles.path = /etc/buteo/profiles/sync
facebook_sync_profiles.files = \
    $$PWD/../xml/sync/facebook.Calendars.xml \
    $$PWD/../xml/sync/facebook.Contacts.xml \
    $$PWD/../xml/sync/facebook.Images.xml \
    $$PWD/../xml/sync/facebook.Notifications.xml \
    $$PWD/../xml/sync/facebook.Posts.xml

# lipstick event feed subview
facebook_eventfeed.files = $$PWD/eventfeed/FacebookPostPage.qml \
                           $$PWD/eventfeed/facebook-update.qml \
                           $$PWD/eventfeed/facebook-delegate.qml \
                           $$PWD/eventfeed/FacebookFeedItem.qml \
                           $$PWD/eventfeed/facebook-feed.qml
facebook_eventfeed.path = /usr/share/lipstick/eventfeed/

# lipstick notification category
facebook_notification_category.files = facebook/x-nemo.social.facebook.notification.conf
facebook_notification_category.path = /usr/share/lipstick/notificationcategories/

# translations
FACEBOOK_TS_FILE = $$OUT_PWD/lipstick-jolla-home-facebook.ts
FACEBOOK_EE_QM = $$OUT_PWD/lipstick-jolla-home-facebook_eng_en.qm
facebook_ts.commands += lupdate $$PWD/eventfeed -ts $$FACEBOOK_TS_FILE
facebook_ts.CONFIG += no_check_exist
facebook_ts.output = $$FACEBOOK_TS_FILE
facebook_ts.input = $$PWD
facebook_ts_install.files = $$FACEBOOK_TS_FILE
facebook_ts_install.path = /usr/share/translations/source
facebook_ts_install.CONFIG += no_check_exist

# should add -markuntranslated "-" when proper translations are in place (or for testing)
facebook_engineering_english.commands += lrelease -idbased $$FACEBOOK_TS_FILE -qm $$FACEBOOK_EE_QM
facebook_engineering_english.CONFIG += no_check_exist
facebook_engineering_english.depends = facebook_ts
facebook_engineering_english.input = $$FACEBOOK_TS_FILE
facebook_engineering_english.output = $$FACEBOOK_EE_QM
facebook_engineering_english_install.path = /usr/share/translations
facebook_engineering_english_install.files = $$FACEBOOK_EE_QM
facebook_engineering_english_install.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS += facebook_ts facebook_engineering_english
PRE_TARGETDEPS += facebook_ts facebook_engineering_english

INSTALLS += facebook_sync_profiles facebook_eventfeed facebook_notification_category facebook_ts_install facebook_engineering_english_install
