/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd. and/or its subsidiary(-ies).
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ** Contributors: Sateesh Kavuri <sateesh.kavuri@gmail.com>
 **               Mani Chandrasekar <maninc@gmail.com>
 **               Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "googlecontactatom.h"
#include <LogMacros.h>

GoogleContactAtom::BatchOperationResponse::BatchOperationResponse()
    : isError(false)
{
}

GoogleContactAtom::GoogleContactAtom()
{
}

void GoogleContactAtom::setAuthorEmail(const QString &authorEmail)
{
    mAuthorEmail = authorEmail;
}

QString GoogleContactAtom::authorEmail() const
{
    return mAuthorEmail;
}

void GoogleContactAtom::setAuthorName(const QString &authorName)
{
    mAuthorName = authorName;
}

QString GoogleContactAtom::authorName() const
{
    return mAuthorName;
}

void GoogleContactAtom::setId(const QString &id)
{
    mId = id;
}

QString GoogleContactAtom::id() const
{
    return mId;
}

void GoogleContactAtom::setUpdated(const QString &updated)
{
    mUpdated = updated;
}

QString GoogleContactAtom::updated() const
{
    return mUpdated;
}

void GoogleContactAtom::setCategory(const QString &schema, const QString &term)
{
    mCategorySchema = schema;
    mCategoryTerm = term;
}

QString GoogleContactAtom::categorySchema() const
{
    return mCategorySchema;
}

QString GoogleContactAtom::categoryTerm() const
{
    return mCategoryTerm;
}

void GoogleContactAtom::setTitle(const QString &title)
{
    mTitle = title;
}

QString GoogleContactAtom::title() const
{
    return mTitle;
}

void GoogleContactAtom::setGenerator(const QString &name, const QString &version, const QString &uri)
{
    mGeneratorName = name;
    mGeneratorVersion = version;
    mGeneratorUri = uri;
}

void GoogleContactAtom::setContent(const QString &note, const QString &type)
{
    Q_UNUSED(note)
    Q_UNUSED(type)
}

QString GoogleContactAtom::generatorName() const
{
    return mGeneratorName;
}

QString GoogleContactAtom::generatorVersion() const
{
    return mGeneratorVersion;
}

QString GoogleContactAtom::generatorUri() const
{
    return mGeneratorUri;
}

void GoogleContactAtom::setTotalResults(int totalResults)
{
    mTotalResults = totalResults;
}

int GoogleContactAtom::totalResults() const
{
    return mTotalResults;
}

void GoogleContactAtom::setStartIndex(int startIndex)
{
    mStartIndex = startIndex;
}

int GoogleContactAtom::startIndex() const
{
    return mStartIndex;
}

void GoogleContactAtom::setItemsPerPage(int itemsPerPage)
{
    mItemsPerPage = itemsPerPage;
}

int GoogleContactAtom::itemsPerPage() const
{
    return mItemsPerPage;
}

void GoogleContactAtom::addBatchOperationResponse(const QString &operationId, GoogleContactAtom::BatchOperationResponse response)
{
    mBatchOperationResponses.insert(operationId, response);
}

QMap<QString, GoogleContactAtom::BatchOperationResponse> GoogleContactAtom::batchOperationResponses() const
{
    return mBatchOperationResponses;
}

void GoogleContactAtom::addEntryContact(const QContact &entryContact, const QStringList &unsupportedElements)
{
    mContactList.append(qMakePair(entryContact, unsupportedElements));
}

QList<QPair<QContact, QStringList> > GoogleContactAtom::entryContacts() const
{
    return mContactList;
}

void GoogleContactAtom::addDeletedEntryContact(const QContact &deletedContact)
{
    mDeletedContactList.append(deletedContact);
}

QList<QContact> GoogleContactAtom::deletedEntryContacts() const
{
    return mDeletedContactList;
}

void GoogleContactAtom::addEntrySystemGroup(const QString &systemGroupId, const QString &systemGroupAtomId)
{
    mSystemGroupAtomIds.insert(systemGroupId, systemGroupAtomId);
}

QMap<QString, QString> GoogleContactAtom::entrySystemGroups() const
{
    return mSystemGroupAtomIds;
}

void GoogleContactAtom::setNextEntriesUrl(const QString &nextUrl)
{
    mNextEntriesUrl = nextUrl;
}

QString GoogleContactAtom::nextEntriesUrl() const
{
    return mNextEntriesUrl;
}
