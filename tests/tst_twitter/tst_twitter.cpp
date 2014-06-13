/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ** This program/library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation.
 **
 ** This program/library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this program/library; if not, write to the Free
 ** Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 ** 02110-1301 USA
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
