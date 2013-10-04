/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd. and/or its subsidiary(-ies).
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ** Contributors: Sateesh Kavuri <sateesh.kavuri@gmail.com>
 **               Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef GOOGLECONTACTATOM_H
#define GOOGLECONTACTATOM_H

#include <QMetaEnum>
#include <QMap>
#include <QList>
#include <QXmlStreamWriter>

#include <QContact>

USE_CONTACTS_NAMESPACE

class GoogleContactAtom {
public:
    GoogleContactAtom ();

    typedef enum
    {
        text,
        html,
        xhtml
    } TYPE;

    void setAuthorName(const QString &authorName);
    QString authorName() const;

    void setAuthorEmail(const QString &authorEmail);
    QString authorEmail() const;

    void setId(const QString &id);
    QString id() const;

    void setUpdated(const QString &updated);
    QString updated() const;

    void setCategory (const QString &schema = QLatin1String("http://schemas.google.com/g/2005#kind"),
                      const QString &term = QLatin1String("http://schemas.google.com/contact/2008#contact"));

    void setTitle(const QString &title);
    QString title() const;

    void setContent (const QString &note, const QString &type = QLatin1String("text"));

    void setGenerator(const QString &name = QLatin1String("Contacts"),
                      const QString &version = QLatin1String("1.0"),
                      const QString &uri = QLatin1String("http://sailfish.org"));
    QString generatorName() const;
    QString generatorVersion() const;
    QString generatorUri() const;

    void setTotalResults(int totalResults);
    int totalResults() const;

    void setStartIndex(int startIndex);
    int startIndex() const;

    void setItemsPerPage(int itemsPerPage);
    int itemsPerPage() const;

    void addEntryContact(const QContact &contact);
    QList<QContact> entryContacts() const;

    void setNextEntriesUrl (const QString &nextUrl);
    QString nextEntriesUrl() const;

private:
    QString mAuthorEmail;
    QString mAuthorName;
    QString mCategory;
    QString mCategoryTerm;
    QString mSchema;
    QString mContributor;
    QString mGeneratorName;
    QString mGeneratorVersion;
    QString mGeneratorUri;
    QString mIcon;
    QString mId;
    QString mLink;
    QString mLogo;
    QString mRights;
    QString mSubtitle;
    QString mTitle;
    QString mUpdated;

    int mTotalResults;
    int mStartIndex;
    int mItemsPerPage;

    QList<QContact> mContactList;
    QString mNextEntriesUrl;
};

#endif // GOOGLECONTACTATOM_H
