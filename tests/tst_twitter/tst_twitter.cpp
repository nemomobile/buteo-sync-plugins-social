/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#define QT_STATICPLUGIN

#include <QtGlobal>
#include <QTest>

#include "syncservice.h"

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
    QScopedPointer<SyncService> twNotSs(new SyncService(QString::fromLatin1("twitter.Notifications"), this));
    QScopedPointer<TwitterMentionTimelineSyncAdaptor> fbNotSa(new TwitterMentionTimelineSyncAdaptor(twNotSs.data(), this));
    QSKIP("TODO: write unit tests for this");
}

void tst_twitter::posts()
{
    QScopedPointer<SyncService> twPstSs(new SyncService(QString::fromLatin1("twitter.Posts"), this));
    QScopedPointer<TwitterHomeTimelineSyncAdaptor> fbNotSa(new TwitterHomeTimelineSyncAdaptor(twPstSs.data(), this));
    QSKIP("TODO: write unit tests for this");
}

// --------------------------------

QTEST_MAIN(tst_twitter)
#include "tst_twitter.moc"
