TEMPLATE=aux

OTHER_FILES += vk_eventfeed.files

# lipstick event feed subview
vk_eventfeed.path = /usr/share/lipstick/eventfeed/
vk_eventfeed.files = $$PWD/VKPost.qml \
                     $$PWD/VKPostPage.qml \
                     $$PWD/VKFeedItem.qml \
                     $$PWD/vk-feed.qml \
                     $$PWD/vk-update.qml \
                     $$PWD/vk-delegate.qml \

# translations
VK_TS_FILE = $$OUT_PWD/lipstick-jolla-home-vk.ts
VK_EE_QM = $$OUT_PWD/lipstick-jolla-home-vk_eng_en.qm
vk_ts.commands += lupdate $$PWD -ts $$VK_TS_FILE
vk_ts.CONFIG += no_check_exist
vk_ts.output = $$VK_TS_FILE
vk_ts.input = $$PWD
vk_ts_install.files = $$VK_TS_FILE
vk_ts_install.path = /usr/share/translations/source
vk_ts_install.CONFIG += no_check_exist

# should add -markuntranslated "-" when proper translations are in place (or for testing)
vk_engineering_english.commands += lrelease -idbased $$VK_TS_FILE -qm $$VK_EE_QM
vk_engineering_english.CONFIG += no_check_exist
vk_engineering_english.depends = vk_ts
vk_engineering_english.input = $$VK_TS_FILE
vk_engineering_english.output = $$VK_EE_QM
vk_engineering_english_install.path = /usr/share/translations
vk_engineering_english_install.files = $$VK_EE_QM
vk_engineering_english_install.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS += vk_ts vk_engineering_english
PRE_TARGETDEPS += vk_ts vk_engineering_english

INSTALLS += vk_eventfeed vk_ts_install vk_engineering_english_install
