/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
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
