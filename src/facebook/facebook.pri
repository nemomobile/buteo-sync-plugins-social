INCLUDEPATH += . ..

HEADERS += \
    $$PWD/facebookdatatypesyncadaptor.h \
    $$PWD/facebookcontactsyncadaptor.h \
    $$PWD/facebookimagesyncadaptor.h \
    $$PWD/facebooknotificationsyncadaptor.h \
    $$PWD/facebookpostsyncadaptor.h

SOURCES += \
    $$PWD/facebookdatatypesyncadaptor.cpp \
    $$PWD/facebookcontactsyncadaptor.cpp \
    $$PWD/facebookimagesyncadaptor.cpp \
    $$PWD/facebooknotificationsyncadaptor.cpp \
    $$PWD/facebookpostsyncadaptor.cpp

# lipstick event feed subview
facebook_eventfeed.files = $$PWD/eventfeed/facebook.qml
facebook_eventfeed.path = /usr/share/lipstick/eventfeed/

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

INSTALLS += facebook_eventfeed facebook_ts_install facebook_engineering_english_install
