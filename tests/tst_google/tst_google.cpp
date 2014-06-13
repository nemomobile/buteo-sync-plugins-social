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

#include "constants_p.h"
#include <qtcontacts-extensions_impl.h>
#include <qcontactoriginmetadata_impl.h>

#include "googlecalendarsyncadaptor.h"
#include "googlecontactsyncadaptor.h"

class tst_google : public QObject
{
    Q_OBJECT

public:
    tst_google();
    virtual ~tst_google();


public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void calendars();
    void contacts();
};

// --------------------------------

tst_google::tst_google()
{
}

tst_google::~tst_google()
{
}

void tst_google::initTestCase()
{
}

void tst_google::cleanupTestCase()
{
}

void tst_google::init()
{
}

void tst_google::cleanup()
{
}

// --------------------------------

void tst_google::calendars()
{
    QScopedPointer<GoogleCalendarSyncAdaptor> ggCalSa(new GoogleCalendarSyncAdaptor(this));
    QSKIP("TODO: write unit tests for this");
}

void tst_google::contacts()
{
    QScopedPointer<GoogleContactSyncAdaptor> ggConSa(new GoogleContactSyncAdaptor(this));
    QSKIP("TODO: write unit tests for this");
}

// --------------------------------

QTEST_MAIN(tst_google)
#include "tst_google.moc"
