TEMPLATE = subdirs

SUBDIRS = \
    tst_facebook \
    tst_google \
    tst_twitter
SUBDIRS += auto

check.commands += cd auto && qmltestrunner
QMAKE_EXTRA_TARGETS += check
