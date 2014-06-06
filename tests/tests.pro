TEMPLATE = subdirs

SUBDIRS = \
    tst_facebook \
    tst_google \
    tst_twitter

QMAKE_EXTRA_TARGETS += check
