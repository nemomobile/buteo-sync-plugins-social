TEMPLATE=aux

# lipstick event feed subview shared components
OTHER_FILES += shared_eventfeed.files

shared_eventfeed.path = /usr/share/lipstick/eventfeed/shared/
shared_eventfeed.files = \
    $$PWD/SocialAvatar.qml \
    $$PWD/SocialBody.qml \
    $$PWD/SocialButton.qml \
    $$PWD/SocialContent.qml \
    $$PWD/SocialMediaRow.qml \
    $$PWD/SocialImage.qml \
    $$PWD/SocialInfoLabel.qml \
    $$PWD/SocialComment.qml \
    $$PWD/SocialReplyField.qml \
    $$PWD/SocialAccountPullDownMenu.qml \
    $$PWD/SocialAccountPage.qml \
    $$PWD/SocialStatusUpdater.qml \
    $$PWD/SocialMediaFeedPage.qml \
    $$PWD/SocialMediaFeedItem.qml \
    $$PWD/SocialMediaAccountDelegate.qml \
    $$PWD/SocialMediaIndicator.qml \
    $$PWD/SocialMediaPreviewRow.qml

# translations
TS_FILE = $$OUT_PWD/sociald.ts
EE_QM = $$OUT_PWD/sociald_eventfeed_eng_en.qm
ts.commands += lupdate -no-recursive $$PWD -ts $$TS_FILE
ts.CONFIG += no_check_exist
ts.output = $$TS_FILE
ts.input = .
ts_install.files = $$TS_FILE
ts_install.path = /usr/share/translations/source
ts_install.CONFIG += no_check_exist

# should add -markuntranslated "-" when proper translations are in place (or for testing)
engineering_english.commands += lrelease -idbased $$TS_FILE -qm $$EE_QM
engineering_english.CONFIG += no_check_exist
engineering_english.depends = ts
engineering_english.input = $$TS_FILE
engineering_english.output = $$EE_QM
engineering_english_install.path = /usr/share/translations
engineering_english_install.files = $$EE_QM
engineering_english_install.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS += ts engineering_english
PRE_TARGETDEPS += ts engineering_english

INSTALLS += shared_eventfeed ts_install engineering_english_install
