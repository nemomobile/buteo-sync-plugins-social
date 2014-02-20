/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#define QT_STATICPLUGIN

#include <QtGlobal>
#include <QTest>

#include "twitterhometimelinesyncadaptor.h"
#include "twittermentiontimelinesyncadaptor.h"

class tst_twitter : public QObject
{
    Q_OBJECT

public:
    tst_twitter();
    virtual ~tst_twitter();


public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void notifications();
    void posts();
};

// --------------------------------

tst_twitter::tst_twitter()
{
}

tst_twitter::~tst_twitter()
{
}

void tst_twitter::initTestCase()
{
}

void tst_twitter::cleanupTestCase()
{
}

void tst_twitter::init()
{
}

void tst_twitter::cleanup()
{
}

// --------------------------------

void tst_twitter::notifications()
{
    QScopedPointer<TwitterMentionTimelineSyncAdaptor> fbNotSa(new TwitterMentionTimelineSyncAdaptor(this));
    QSKIP("TODO: write unit tests for this");
}

void tst_twitter::posts()
{
    QScopedPointer<TwitterHomeTimelineSyncAdaptor> fbNotSa(new TwitterHomeTimelineSyncAdaptor(this));
    QSKIP("TODO: write unit tests for this");
}

// --------------------------------

QTEST_MAIN(tst_twitter)
#include "tst_twitter.moc"
