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

#include <QtContacts/QContactManager>
#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContact>
#include <QtContacts/QContactSyncTarget>
#include <QtContacts/QContactGuid>
#include <QtContacts/QContactName>
#include <QtContacts/QContactAvatar>
#include <QtContacts/QContactUrl>
#include <QtContacts/QContactGender>
#include <QtContacts/QContactNote>
#include <QtContacts/QContactBirthday>

#include "constants_p.h"
#include <qtcontacts-extensions_impl.h>
#include <qcontactoriginmetadata_impl.h>

#include "facebookcalendarsyncadaptor.h"
#include "facebookcontactsyncadaptor.h"
#include "facebookimagesyncadaptor.h"
#include "facebooknotificationsyncadaptor.h"
#include "facebookpostsyncadaptor.h"

class tst_facebook : public QObject
{
    Q_OBJECT

public:
    tst_facebook();
    virtual ~tst_facebook();


public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private slots:
    void calendars();
    void contacts();
    void images();
    void notifications();
    void posts();

private:
    QContactManager m_manager;
};

// --------------------------------

class TestFacebookContactSyncAdaptor : public FacebookContactSyncAdaptor
{
    Q_OBJECT
public:
    TestFacebookContactSyncAdaptor(QObject *parent)
        : FacebookContactSyncAdaptor(parent), m_doFinalCleanup(false) {}
    void doBeginSync(int accountId, const QString &accessToken) { beginSync(accountId, accessToken); }
    void doPurge(int accountId) { purgeDataForOldAccounts(QList<int>() << accountId, SocialNetworkSyncAdaptor::SyncPurge); }
protected:
    // override this to avoid auto purging the fake account contacts when sync finishes.
    void finalCleanup() { if (m_doFinalCleanup) FacebookContactSyncAdaptor::finalCleanup(); }
public:
    bool m_doFinalCleanup;
};

// --------------------------------

tst_facebook::tst_facebook()
{
}

tst_facebook::~tst_facebook()
{
}

void tst_facebook::initTestCase()
{
}

void tst_facebook::cleanupTestCase()
{
}

void tst_facebook::init()
{
}

void tst_facebook::cleanup()
{
}

// --------------------------------

void tst_facebook::calendars()
{
    QScopedPointer<FacebookCalendarSyncAdaptor> fbCalSa(new FacebookCalendarSyncAdaptor(this));
    QSKIP("TODO: write unit tests for this");
}

void tst_facebook::contacts()
{
    // step one: check database state -> current count of facebook contacts
    // step two: trigger sync
    // step three: check database state -> should have gotten the "new" contacts / ensure data matches
    // step four: trigger the purge, and ensure that the data is removed (but no other data)
    QScopedPointer<TestFacebookContactSyncAdaptor> fbConSa(new TestFacebookContactSyncAdaptor(this));

    QContactDetailFilter facebookFilter;
    facebookFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    facebookFilter.setValue("facebook");

    QList<QContact> currentContacts = m_manager.contacts(facebookFilter);
    int oldCount = currentContacts.count();
    QSet<QContactId> oldIds;
    foreach (const QContact &c, currentContacts) {
        oldIds.insert(c.id());
    }

    // perform sync and check results.
    // we do it several times to ensure that we
    // don't get unwanted duplicated data etc.
    for (int loop = 5; loop > 0; loop--) {
        fbConSa->doBeginSync(7357, "testAccessToken");
        QTest::qWait(3000); // wait for all data retrieval + save operations to finish.

        QList<QContact> syncedContacts = m_manager.contacts(facebookFilter);
        int newCount = syncedContacts.count();
        QCOMPARE(newCount, oldCount+2);
        for (int i = syncedContacts.size() - 1; i >= 0; --i) {
            if (oldIds.contains(syncedContacts[i].id())) {
                syncedContacts.removeAt(i);
            }
        }
        QCOMPARE(syncedContacts.size(), 2);

        // ensure that the data is what we expect.
        QContact testfriendOne, testfriendTwo;
        if (syncedContacts.at(0).detail<QContactName>().lastName() == QString::fromLatin1("One") &&
            syncedContacts.at(1).detail<QContactName>().lastName() == QString::fromLatin1("Two")) {
            testfriendOne = syncedContacts.at(0);
            testfriendTwo = syncedContacts.at(1);
        } else if (syncedContacts.at(1).detail<QContactName>().lastName() == QString::fromLatin1("One") &&
                   syncedContacts.at(0).detail<QContactName>().lastName() == QString::fromLatin1("Two")) {
            testfriendOne = syncedContacts.at(1);
            testfriendTwo = syncedContacts.at(0);
        } else {
            qWarning() << syncedContacts.at(0).detail<QContactName>() << "\n" << syncedContacts.at(1).detail<QContactName>();
            QFAIL("Unexpected contacts synced for test");
        }

        QCOMPARE(testfriendOne.details<QContactUrl>().count(), 2);     // link, website
        QCOMPARE(testfriendOne.details<QContactName>().count(), 1);    // name
        QCOMPARE(testfriendOne.details<QContactBirthday>().count(), 1);// birthday
        QCOMPARE(testfriendOne.details<QContactNote>().count(), 1);    // bio
        QCOMPARE(testfriendOne.details<QContactGender>().count(), 1);  // gender
        QCOMPARE(testfriendOne.details<QContactAvatar>().count(), 2);  // picture, cover
        QCOMPARE(testfriendOne.details<QContactGuid>().count(), 1);    // id

        QCOMPARE(testfriendOne.detail<QContactName>().firstName(), QString::fromLatin1("Testfriend"));
        QCOMPARE(testfriendOne.detail<QContactName>().middleName(), QString::fromLatin1(""));
        QCOMPARE(testfriendOne.detail<QContactName>().lastName(), QString::fromLatin1("One"));
        QCOMPARE(testfriendOne.detail<QContactBirthday>().date(), QDate(2001, 01, 01));
        QCOMPARE(testfriendOne.detail<QContactNote>().note(), QString::fromLatin1("Fairly ordinary, average, test friend."));
        QCOMPARE(testfriendOne.detail<QContactGender>().gender(), QContactGender::GenderMale);
        QContactUrl linkUrl = testfriendOne.details<QContactUrl>().at(0).subType() == QContactUrl::SubTypeBlog
                            ? testfriendOne.details<QContactUrl>().at(0)
                            : testfriendOne.details<QContactUrl>().at(1);
        QContactUrl websiteUrl = testfriendOne.details<QContactUrl>().at(0).subType() == QContactUrl::SubTypeBlog
                            ? testfriendOne.details<QContactUrl>().at(1)
                            : testfriendOne.details<QContactUrl>().at(0);
        QCOMPARE(linkUrl.url(), QString::fromLatin1("https://www.facebook.com/testfriendone"));
        QCOMPARE(websiteUrl.url(), QString::fromLatin1("http://friendone.test.com/"));
        QContactAvatar coverAvatar = testfriendOne.details<QContactAvatar>().at(0).value(QContactAvatar__FieldAvatarMetadata) == QString::fromLatin1("cover")
                                   ? testfriendOne.details<QContactAvatar>().at(0)
                                   : testfriendOne.details<QContactAvatar>().at(1);
        QContactAvatar pictureAvatar = testfriendOne.details<QContactAvatar>().at(0).value(QContactAvatar__FieldAvatarMetadata) == QString::fromLatin1("cover")
                                   ? testfriendOne.details<QContactAvatar>().at(1)
                                   : testfriendOne.details<QContactAvatar>().at(0);
        QVERIFY(!coverAvatar.imageUrl().isEmpty());   // it'll be something generated.
        QVERIFY(!pictureAvatar.imageUrl().isEmpty()); // it'll be something generated.
        QCOMPARE(testfriendOne.detail<QContactGuid>().guid(), QString::fromLatin1("123456123456"));

        QCOMPARE(testfriendTwo.details<QContactUrl>().count(), 1);     // link
        QCOMPARE(testfriendTwo.details<QContactName>().count(), 1);    // name
        QCOMPARE(testfriendTwo.details<QContactNote>().count(), 1);    // bio
        QCOMPARE(testfriendTwo.details<QContactGender>().count(), 1);  // gender
        QCOMPARE(testfriendTwo.details<QContactAvatar>().count(), 1);  // picture
        QCOMPARE(testfriendTwo.details<QContactGuid>().count(), 1);    // id
        QCOMPARE(testfriendTwo.details<QContactBirthday>().count(), 0);// birthday won't be parsed, month-day only.

        QCOMPARE(testfriendTwo.detail<QContactName>().firstName(), QString::fromLatin1("Test"));
        QCOMPARE(testfriendTwo.detail<QContactName>().middleName(), QString::fromLatin1("Friend"));
        QCOMPARE(testfriendTwo.detail<QContactName>().lastName(), QString::fromLatin1("Two"));
        QCOMPARE(testfriendTwo.detail<QContactNote>().note(), QString::fromLatin1("Another ordinary, average, test friend."));
        QCOMPARE(testfriendTwo.detail<QContactGender>().gender(), QContactGender::GenderFemale);
        QVERIFY(!testfriendTwo.detail<QContactAvatar>().imageUrl().isEmpty()); // it'll be something generated.
        QCOMPARE(testfriendTwo.detail<QContactUrl>().url(), QString::fromLatin1("https://www.facebook.com/testfriendtwo"));
        QCOMPARE(testfriendTwo.detail<QContactGuid>().guid(), QString::fromLatin1("7898778987"));
    }

    // test purging.
    fbConSa->doPurge(7357);
    QList<QContact> afterPurge = m_manager.contacts(facebookFilter);
    QCOMPARE(afterPurge, currentContacts); // should have purged all of the test contacts.

    // Now do a sync after enabling the "finalCleanup()" code.
    // That will ensure that any contacts belonging to an account which
    // was removed while sync was ongoing, will be purged after sync finishes.
    fbConSa->m_doFinalCleanup = true;
    fbConSa->doBeginSync(7357, "testAccessToken");
    QTest::qWait(3000); // wait for all data retrieval + save operations to finish.

    QList<QContact> syncedContacts = m_manager.contacts(facebookFilter);
    int newCount = syncedContacts.count();
    QCOMPARE(newCount, oldCount); // should have been purged due to finalCleanup().
    for (int i = syncedContacts.size() - 1; i >= 0; --i) {
        if (oldIds.contains(syncedContacts[i].id())) {
            syncedContacts.removeAt(i);
        }
    }
    QCOMPARE(syncedContacts.size(), 0); // should have been purged due to finalCleanup().
}

void tst_facebook::images()
{
    QScopedPointer<FacebookImageSyncAdaptor> fbConSa(new FacebookImageSyncAdaptor(this));
    QSKIP("TODO: write unit tests for this");
}

void tst_facebook::notifications()
{
    QScopedPointer<FacebookNotificationSyncAdaptor> fbNotSa(new FacebookNotificationSyncAdaptor(this));
    QSKIP("TODO: write unit tests for this");
}

void tst_facebook::posts()
{
    QSKIP("we no longer sync posts");
}

// --------------------------------

QTEST_MAIN(tst_facebook)
#include "tst_facebook.moc"
