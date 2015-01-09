/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd. and/or its subsidiary(-ies).
 ** Contact: Chris Adams <chris.adams@jolla.com>
 **
 ** Contributors: Sateesh Kavuri <sateesh.kavuri@gmail.com>
 **               Mani Chandrasekar <maninc@gmail.com>
 **               Chris Adams <chris.adams@jolla.com>
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

#include "googlecontactstream.h"
#include "googlecontactatom.h"
#include "constants_p.h"
#include "trace.h"

#include <QDateTime>

GoogleContactStream::GoogleContactStream(bool response, int accountId, const QString &accountEmail, QObject* parent)
    : QObject(parent)
    , mXmlReader(0)
    , mAtom(0)
    , mAccountId(accountId)
    , mXmlWriter(0)
    , mAccountEmail(accountEmail)
{
    if (response == true) {
        initResponseFunctionMap();
    } else {
        initFunctionMap();
    }
}

GoogleContactStream::~GoogleContactStream()
{
}

GoogleContactAtom *GoogleContactStream::parse(const QByteArray &xmlBuffer)
{
    mXmlReader = new QXmlStreamReader(xmlBuffer);
    mAtom = new GoogleContactAtom;

    Q_CHECK_PTR(mXmlReader);
    Q_CHECK_PTR(mAtom);

    while (!mXmlReader->atEnd() && !mXmlReader->hasError()) {
        if (mXmlReader->readNextStartElement()) {
            Handler handler = mAtomFunctionMap.value(mXmlReader->name().toString());
            if (handler) {
                (*this.*handler)();
            }
        }
    }

    delete mXmlReader;
    return mAtom;
}

QByteArray GoogleContactStream::encode(const QMultiMap<GoogleContactStream::UpdateType, QPair<QContact, QStringList> > &updates)
{
    QByteArray xmlBuffer;
    mXmlWriter = new QXmlStreamWriter(&xmlBuffer);
    startBatchFeed();

    QList<QPair<QContact, QStringList> > removedContacts = updates.values(GoogleContactStream::Remove);
    for (int i = 0; i < removedContacts.size(); ++i) {
        encodeContactUpdate(removedContacts[i].first, removedContacts[i].second, GoogleContactStream::Remove, true); // batchmode = true
    }

    QList<QPair<QContact, QStringList> > addedContacts = updates.values(GoogleContactStream::Add);
    for (int i = 0; i < addedContacts.size(); ++i) {
        encodeContactUpdate(addedContacts[i].first, addedContacts[i].second, GoogleContactStream::Add, true); // batchmode = true
    }

    QList<QPair<QContact, QStringList> > modifiedContacts = updates.values(GoogleContactStream::Modify);
    for (int i = 0; i < modifiedContacts.size(); ++i) {
        encodeContactUpdate(modifiedContacts[i].first, modifiedContacts[i].second, GoogleContactStream::Modify, true); // batchmode = true
    }

    endBatchFeed();
    mXmlWriter->writeEndDocument();
    delete mXmlWriter;
    return xmlBuffer;
}

// ----------------------------------------

void GoogleContactStream::initAtomFunctionMap()
{
    mAtomFunctionMap.insert("updated", &GoogleContactStream::handleAtomUpdated);
    mAtomFunctionMap.insert("category", &GoogleContactStream::handleAtomCategory);
    mAtomFunctionMap.insert("author", &GoogleContactStream::handleAtomAuthor);
    mAtomFunctionMap.insert("totalResults", &GoogleContactStream::handleAtomOpenSearch);
    mAtomFunctionMap.insert("startIndex", &GoogleContactStream::handleAtomOpenSearch);
    mAtomFunctionMap.insert("itemsPerPage", &GoogleContactStream::handleAtomOpenSearch);
    mAtomFunctionMap.insert("link", &GoogleContactStream::handleAtomLink);
    mAtomFunctionMap.insert("entry", &GoogleContactStream::handleAtomEntry);
}

void GoogleContactStream::initResponseFunctionMap()
{
    initAtomFunctionMap();
    // TODO: move the batch request response handling stuff here.
}

void GoogleContactStream::initFunctionMap()
{
    initAtomFunctionMap();
    mContactFunctionMap.insert("content", &GoogleContactStream::handleEntryContent);
    mContactFunctionMap.insert("updated", &GoogleContactStream::handleEntryUpdated);
    mContactFunctionMap.insert("app:edited", &GoogleContactStream::handleEntryUpdated);
    mContactFunctionMap.insert("gContact:birthday", &GoogleContactStream::handleEntryBirthday);
    mContactFunctionMap.insert("gcontact::gender", &GoogleContactStream::handleEntryGender);
    mContactFunctionMap.insert("gContact:hobby", &GoogleContactStream::handleEntryHobby);
    mContactFunctionMap.insert("gContact:nickname", &GoogleContactStream::handleEntryNickname);
    mContactFunctionMap.insert("gContact:occupation", &GoogleContactStream::handleEntryOccupation);
    mContactFunctionMap.insert("gContact:website", &GoogleContactStream::handleEntryWebsite);
    mContactFunctionMap.insert("gd:comments", &GoogleContactStream::handleEntryComments);
    mContactFunctionMap.insert("gd:email", &GoogleContactStream::handleEntryEmail);
    mContactFunctionMap.insert("gd:im", &GoogleContactStream::handleEntryIm);
    mContactFunctionMap.insert("gd:name", &GoogleContactStream::handleEntryName);
    mContactFunctionMap.insert("gd:organization", &GoogleContactStream::handleEntryOrganization);
    mContactFunctionMap.insert("gd:phoneNumber", &GoogleContactStream::handleEntryPhoneNumber);
    mContactFunctionMap.insert("gd:structuredPostalAddress", &GoogleContactStream::handleEntryStructuredPostalAddress);
}

// ----------------------------------------

void GoogleContactStream::handleAtomUpdated()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "updated");
    mAtom->setUpdated(mXmlReader->readElementText());
}

void GoogleContactStream::handleAtomCategory()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "category");

    QXmlStreamAttributes attributes = mXmlReader->attributes();
    QString scheme, term;
    if (attributes.hasAttribute("scheme")) {
        scheme = attributes.value("scheme").toString();
    } else if (attributes.hasAttribute("term")) {
        term = attributes.value("term").toString();
    }

    mAtom->setCategory(scheme, term);
}

void GoogleContactStream::handleAtomAuthor()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "author");

    while (!(mXmlReader->tokenType() == QXmlStreamReader::EndElement && mXmlReader->name() == "author")) {
        if (mXmlReader->tokenType() == QXmlStreamReader::StartElement) {
            if (mXmlReader->name() == "name") {
                mAtom->setAuthorName(mXmlReader->readElementText());
            } else if (mXmlReader->name() == "email") {
                mAtom->setAuthorEmail(mXmlReader->readElementText());
            }
        }
        mXmlReader->readNextStartElement();
    }
}

void GoogleContactStream::handleAtomOpenSearch()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->prefix() == "openSearch");

    if (mXmlReader->name() == "totalResults") {
        mAtom->setTotalResults(mXmlReader->readElementText().toInt());
    } else if (mXmlReader->name() == "startIndex") {
        mAtom->setStartIndex(mXmlReader->readElementText().toInt());
    } else if (mXmlReader->name() == "itemsPerPage") {
        mAtom->setItemsPerPage(mXmlReader->readElementText().toInt());
    }
}

void GoogleContactStream::handleAtomLink()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "link");

    if (mXmlReader->attributes().hasAttribute("rel") && (mXmlReader->attributes().value("rel") == "next")) {
        mAtom->setNextEntriesUrl(mXmlReader->attributes().value("href").toString());
    }
}

void GoogleContactStream::handleAtomEntry()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "entry");

    // the entry will either be a contact, a group, or a response to a batch update request.
    // if it's a group, we need to store some information about it.
    QString systemGroupId;
    QString systemGroupAtomId;

    // the entry will be a contact if this is a response to a "read" request
    QContact entryContact;
    QString contactEtag;
    QStringList unsupportedElements;
    bool isInGroup = false;
    bool isDeleted = false;

    // or it will be a series of batch operation success/fail info
    // if this xml is the response to a batch update/delete request.
    bool isBatchOperationResponse = false;
    GoogleContactAtom::BatchOperationResponse response;

    while (!((mXmlReader->tokenType() == QXmlStreamReader::EndElement) && (mXmlReader->name() == "entry"))) {
        if (mXmlReader->tokenType() == QXmlStreamReader::StartElement) {
            DetailHandler handler = mContactFunctionMap.value(mXmlReader->qualifiedName().toString());
            if (handler) {
                QContactDetail convertedDetail = (*this.*handler)();
                if (convertedDetail != QContactDetail()) {
                    entryContact.saveDetail(&convertedDetail);
                }
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("gContact:groupMembershipInfo")) {
                isInGroup = true;
                QString unsupportedElement = handleEntryUnknownElement();
                if (!unsupportedElement.isEmpty()) {
                    unsupportedElements.append(unsupportedElement);
                }
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("gd:deleted")) {
                isDeleted = true;
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("batch:id")) {
                isBatchOperationResponse = true;
                handleEntryBatchId(&response);
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("batch:operation")) {
                isBatchOperationResponse = true;
                handleEntryBatchOperation(&response);
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("batch:status")) {
                isBatchOperationResponse = true;
                handleEntryBatchStatus(&response);
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("gd:extendedProperty")) {
                // It might be an extension property we don't support.
                // If we don't support it, we store the element text.
                QString unsupportedElement = handleEntryExtendedProperty();
                if (!unsupportedElement.isEmpty()) {
                    unsupportedElements.append(unsupportedElement);
                }
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("link")) {
                // There are several possible links:
                // Avatar Photo link
                // Self query link
                // Edit link
                // Batch link etc.

                // If it's an avatar, we grab it as a QContactAvatar detail
                QContactAvatar avatar;
                Q_FOREACH (const QContactAvatar &av, entryContact.details<QContactAvatar>()) {
                    if (av.value(QContactAvatar__FieldAvatarMetadata).toString() == QStringLiteral("picture")) {
                        avatar = av;
                        break;
                    }
                }
                bool isAvatar = false;
                QString unsupportedElement = handleEntryLink(&avatar, &isAvatar);
                if (isAvatar) {
                    entryContact.saveDetail(&avatar);
                }

                // Whether it's an avatar or not, we also store the element text.
                if (!unsupportedElement.isEmpty()) {
                    unsupportedElements.append(unsupportedElement);
                }
            } else if (mXmlReader->name().toString() == QStringLiteral("entry")) {
                // read the etag out of the entry.
                contactEtag = mXmlReader->attributes().value("gd:etag").toString();
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("gContact:systemGroup")) {
                systemGroupId = mXmlReader->attributes().value("id").toString();
            } else if (mXmlReader->qualifiedName().toString() == QStringLiteral("id")) {
                // either a contact id or a group id.
                QContactDetail guidDetail = handleEntryId(&systemGroupAtomId);
                entryContact.saveDetail(&guidDetail);
            } else {
                // This is some XML element which we don't handle.
                // We should store it, so that we can send it back when we upload changes.
                QString unsupportedElement = handleEntryUnknownElement();
                if (!unsupportedElement.isEmpty()) {
                    unsupportedElements.append(unsupportedElement);
                }
            }
        }
        mXmlReader->readNextStartElement();
    }

    if (!systemGroupId.isEmpty()) {
        // this entry was a group
        mAtom->addEntrySystemGroup(systemGroupId, systemGroupAtomId);
    } else {
        // this entry was a contact.
        // the etag is the "version identifier".  Save it into the QCOM detail.
        if (!contactEtag.isEmpty()) {
            QContactOriginMetadata omd = entryContact.detail<QContactOriginMetadata>();
            omd.setId(contactEtag);
            entryContact.saveDetail(&omd);
        }

        if (isInGroup) {
            // Only sync the contact if it is in a "real" group
            // as otherwise we get hundreds of "Other Contacts"
            // (random email addresses etc).
            if (isDeleted) {
                mAtom->addDeletedEntryContact(entryContact);
            } else {
                mAtom->addEntryContact(entryContact, unsupportedElements);
            }
        }
    }

    if (isBatchOperationResponse) {
        if (!entryContact.detail<QContactGuid>().guid().isEmpty()) {
            response.contactGuid = entryContact.detail<QContactGuid>().guid();
        }
        mAtom->addBatchOperationResponse(response.operationId, response);
    }
}

QString GoogleContactStream::handleEntryLink(QContactAvatar *avatar, bool *isAvatar)
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "link");

    if (mXmlReader->attributes().hasAttribute("gd:etag")
            && (mXmlReader->attributes().value("rel") == "http://schemas.google.com/contacts/2008/rel#photo")) {
        // this is an avatar photo for the contact entry
        avatar->setImageUrl(mXmlReader->attributes().value("href").toString());
        avatar->setValue(QContactAvatar__FieldAvatarMetadata, QVariant::fromValue<QString>(QStringLiteral("picture")));
        *isAvatar = true;
    }

    return handleEntryUnknownElement();
}

QString GoogleContactStream::handleEntryExtendedProperty()
{
    Q_ASSERT(mXmlReader->isStartElement());
    return handleEntryUnknownElement();
}

QString GoogleContactStream::handleEntryUnknownElement()
{
    Q_ASSERT(mXmlReader->isStartElement());

    QXmlStreamAttributes attributes = mXmlReader->attributes();
    QString attributesString;
    for (int i = 0; i < attributes.size(); ++i) {
        QString extra = QStringLiteral(" %1=\"%2\"")
            .arg(attributes[i].qualifiedName().toString())
            .arg(attributes[i].value().toString().toHtmlEscaped());
        attributesString.append(extra);
    }

    QString unknownElement = QStringLiteral("<%1%2>%3</%1>")
                                .arg(mXmlReader->qualifiedName().toString())
                                .arg(attributesString)
                                .arg(mXmlReader->text().toString());

    return unknownElement;
}

void GoogleContactStream::handleEntryBatchStatus(GoogleContactAtom::BatchOperationResponse *response)
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "status");

    response->code = mXmlReader->attributes().value("code").toString();
    response->reason = mXmlReader->attributes().value("reason").toString();
    response->reasonDescription = mXmlReader->readElementText();
    response->isError = true;
    if (response->code == QStringLiteral("200")           // No error.
            || response->code == QStringLiteral("201")    // Created without error.
            || response->code == QStringLiteral("304")) { // Not modified (no change since time specified)
        // according to Google Data API these response codes signify success cases.
        response->isError = false;
    }
}

void GoogleContactStream::handleEntryBatchOperation(GoogleContactAtom::BatchOperationResponse *response)
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "operation");
    response->type = mXmlReader->attributes().value("type").toString();
}

void GoogleContactStream::handleEntryBatchId(GoogleContactAtom::BatchOperationResponse *response)
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->name() == "id");
    response->operationId = mXmlReader->readElementText();
}

QContactDetail GoogleContactStream::handleEntryId(QString *rawId)
{
    *rawId = mXmlReader->readElementText();
    QString idUrl = *rawId;
    QContactGuid guid;
    guid.setGuid(QStringLiteral("%1:%2").arg(mAccountId).arg(idUrl.remove(0, idUrl.lastIndexOf('/') + 1)));
    return guid;
}

QContactDetail GoogleContactStream::handleEntryContent()
{
    QContactNote note;
    QString content = mXmlReader->readElementText();
    note.setNote(content);
    return note;
}

QContactDetail GoogleContactStream::handleEntryBirthday()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gContact:birthday");

    QContactBirthday birthday;
    birthday.setDate(QDate::fromString(mXmlReader->attributes().value("when").toString(), Qt::ISODate));
    return birthday;
}

QContactDetail GoogleContactStream::handleEntryGender()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gContact:gender");

    QString genderStr = mXmlReader->attributes().value("value").toString().toLower();
    QContactGender gender;
    if (genderStr.startsWith('m')) {
        gender.setGender(QContactGender::GenderMale);
    } else if (genderStr.startsWith('f')) {
        gender.setGender(QContactGender::GenderFemale);
    } else {
        gender.setGender(QContactGender::GenderUnspecified);
    }

    return gender;
}

QContactDetail GoogleContactStream::handleEntryHobby()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gContact:hobby");

    QContactHobby hobby;
    hobby.setHobby(mXmlReader->readElementText());
    return hobby;
}

QContactDetail GoogleContactStream::handleEntryNickname()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gContact:nickname");

    QContactNickname nickname;
    nickname.setNickname(mXmlReader->readElementText());
    return nickname;
}

QContactDetail GoogleContactStream::handleEntryOccupation()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gContact:occupation");

    QContactOrganization org;
    org.setRole(mXmlReader->readElementText());
    return org;
}

QContactDetail GoogleContactStream::handleEntryWebsite()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gContact:website");

    QContactUrl url;
    url.setUrl(mXmlReader->attributes().value("href").toString());
    return url;
}

QContactDetail GoogleContactStream::handleEntryComments()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gd:comments");

    QContactNote note;
    note.setNote(mXmlReader->readElementText());
    return note;
}

QContactDetail GoogleContactStream::handleEntryEmail()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gd:email");

    QContactEmailAddress email;
    email.setEmailAddress(mXmlReader->attributes().value("address").toString());

    QString rel = mXmlReader->attributes().hasAttribute("rel")
                ? mXmlReader->attributes().value("rel").toString()
                : QString();

    if (rel == QStringLiteral("http://schemas.google.com/g/2005#home")) {
        email.setContexts(QContactDetail::ContextHome);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#work")) {
        email.setContexts(QContactDetail::ContextWork);
    } else if (!rel.isEmpty()) {
        email.setContexts(QContactDetail::ContextOther);
    }
    return email;
}

QContactDetail GoogleContactStream::handleEntryIm()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gd:im");

    QContactOnlineAccount onlineAccount;
    onlineAccount.setAccountUri(mXmlReader->attributes().value("address").toString());
    //if (mXml->attributes().hasAttribute("protocol")) {
    //    QString protocolUrl = mXml->attributes().value("protocol").toString();
    //    onlineAccount.setProtocol(protocolUrl.right(protocolUrl.lastIndexOf("#")));
    //}

    return onlineAccount;
}

QContactDetail GoogleContactStream::handleEntryName()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gd:name");

    QContactName name;
    while (!(mXmlReader->tokenType() == QXmlStreamReader::EndElement && mXmlReader->qualifiedName() == "gd:name")) {
        if (mXmlReader->tokenType() == QXmlStreamReader::StartElement) {
            if (mXmlReader->qualifiedName() == "gd:givenName") {
                name.setFirstName(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:additionalName") {
                name.setMiddleName(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:familyName") {
                name.setLastName(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:namePrefix") {
                name.setPrefix(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:nameSuffix") {
                name.setSuffix(mXmlReader->readElementText());
            }
        }
        mXmlReader->readNextStartElement();
    }

    return name;
}

QContactDetail GoogleContactStream::handleEntryOrganization()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gd:organization");

    QContactOrganization org;

    while (!(mXmlReader->tokenType() == QXmlStreamReader::EndElement && mXmlReader->qualifiedName() == "gd:organization")) {
        if (mXmlReader->tokenType() == QXmlStreamReader::StartElement) {
            if (mXmlReader->qualifiedName() == "gd:orgDepartment") {
                QStringList dept = org.department();
                dept.append(mXmlReader->readElementText());
                org.setDepartment(dept);
            } else if (mXmlReader->qualifiedName() == "gd:orgJobDescription") {
                org.setRole(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:orgName") {
                org.setName(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:orgSymbol") {
                org.setLogoUrl(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:orgTitle") {
                org.setTitle(mXmlReader->readElementText());
            }
        }
        mXmlReader->readNextStartElement();
    }

    return org;
}

QContactDetail GoogleContactStream::handleEntryPhoneNumber()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gd:phoneNumber");

    QContactPhoneNumber phone;
    QString rel = mXmlReader->attributes().hasAttribute("rel")
                ? mXmlReader->attributes().value("rel").toString()
                : QString();

    if (rel == QStringLiteral("http://schemas.google.com/g/2005#home")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeLandline);
        phone.setContexts(QContactDetail::ContextHome);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#work")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeLandline);
        phone.setContexts(QContactDetail::ContextWork);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#mobile")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeMobile);
        phone.setContexts(QContactDetail::ContextHome);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#work_mobile")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeMobile);
        phone.setContexts(QContactDetail::ContextWork);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#home_fax")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeFax);
        phone.setContexts(QContactDetail::ContextHome);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#work_fax")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeFax);
        phone.setContexts(QContactDetail::ContextWork);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#other_fax")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeFax);
        phone.setContexts(QContactDetail::ContextOther);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#pager")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypePager);
        phone.setContexts(QContactDetail::ContextHome);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#work_pager")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypePager);
        phone.setContexts(QContactDetail::ContextWork);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#tty_tdd")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeModem);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#car")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeCar);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#telex")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeBulletinBoardSystem);
    } else if (rel == QStringLiteral("http://schemas.google.com/g/2005#assistant")) {
        phone.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeAssistant);
    } // else ignore it, malformed output from Google.

    phone.setNumber(mXmlReader->readElementText());
    return phone;
}

QContactDetail GoogleContactStream::handleEntryStructuredPostalAddress()
{
    Q_ASSERT(mXmlReader->isStartElement() && mXmlReader->qualifiedName() == "gd:structuredPostalAddress");

    QContactAddress address;

    while (!(mXmlReader->tokenType() == QXmlStreamReader::EndElement && mXmlReader->qualifiedName() == "gd:structuredPostalAddress")) {
        if (mXmlReader->tokenType() == QXmlStreamReader::StartElement) {
            if (mXmlReader->qualifiedName() == "gd:street") {
                address.setStreet(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:pobox") {
                address.setPostOfficeBox(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:neighborhood") {
                address.setLocality(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:city") {
                address.setLocality(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:region") {
                address.setRegion(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:postcode") {
                address.setPostcode(mXmlReader->readElementText());
            } else if (mXmlReader->qualifiedName() == "gd:country") {
                address.setCountry(mXmlReader->readElementText());
            }
        }
        mXmlReader->readNextStartElement();
    }

    return address;
}

QContactDetail GoogleContactStream::handleEntryUpdated()
{
    Q_ASSERT(mXmlReader->isStartElement() &&
        (mXmlReader->qualifiedName() == "updated" ||
         mXmlReader->qualifiedName() == "app:edited"));

    QDateTime modTs = QDateTime::fromString(mXmlReader->readElementText(), Qt::ISODate);
    if (modTs.isValid()) {
        QContactTimestamp ts;
        ts.setLastModified(modTs);
        // We don't actually return a timestamp.
        // The qtcontacts-sqlite backend will set one automatically.
    }

    return QContactDetail();
}

// ----------------------------------------

void GoogleContactStream::encodeContactUpdate(const QContact &qContact,
                                              const QStringList &unsupportedElements,
                                              const GoogleContactStream::UpdateType updateType,
                                              const bool batch)
{
    QList<QContactDetail> allDetails = qContact.details ();

    mXmlWriter->writeStartElement("atom:entry");
    if (batch == true) {
        // Etag encoding has to immediately succeed writeStartElement("atom:entry"),
        // since etag is an attribute of this element.
        encodeEtag(qContact, updateType == GoogleContactStream::Remove || updateType == GoogleContactStream::Modify);
        encodeBatchTag(updateType, qContact.id().toString());
    } else {
        mXmlWriter->writeAttribute("xmlns:atom", "http://www.w3.org/2005/Atom");
        mXmlWriter->writeAttribute("xmlns:gd", "http://schemas.google.com/g/2005");
        mXmlWriter->writeAttribute("xmlns:gContact", "http://schemas.google.com/contact/2008");
    }

    if (updateType == GoogleContactStream::Remove) {
        encodeId(qContact, true);
        mXmlWriter->writeEndElement();
        return;
    }

    encodeCategory();
    if (updateType == GoogleContactStream::Modify) {
        encodeId(qContact, true);
        encodeUpdatedTimestamp(qContact);
    }
    encodeUnknownElements(unsupportedElements); // for an Add, this is just group membership.

    Q_FOREACH (const QContactDetail &detail, allDetails) {
        switch(detail.type()) {
            case QContactDetail::TypeName: {
                encodeName(detail);
            }   break;
            case QContactDetail::TypePhoneNumber: {
                encodePhoneNumber(detail);
            }   break;
            case QContactDetail::TypeEmailAddress: {
                encodeEmailAddress(detail);
            }   break;
            case QContactDetail::TypeAddress: {
                encodeAddress(detail);
            }   break;
            case QContactDetail::TypeUrl: {
                encodeUrl(detail);
            }   break;
            case QContactDetail::TypeBirthday: {
                encodeBirthday(detail);
            }   break;
            case QContactDetail::TypeNote: {
                encodeNote(detail);
            }   break;
            case QContactDetail::TypeHobby: {
                encodeHobby(detail);
            }   break;
            case QContactDetail::TypeOrganization: {
                encodeOrganization(detail);
            }   break;
            case QContactDetail::TypeAvatar: {
                encodeAvatar(detail, qContact);
            }   break;
            case QContactDetail::TypeAnniversary: {
                encodeAnniversary(detail);
            }   break;
            case QContactDetail::TypeNickname: {
                encodeNickname(detail);
            }   break;
            case QContactDetail::TypeGender: {
                encodeGender(detail);
            }   break;
            case QContactDetail::TypeOnlineAccount: {
                encodeOnlineAccount(detail);
            }   break;
            case QContactDetail::TypeFamily: {
                encodeFamily(detail);
            }   break;
            // TODO: handle the custom detail fields.
            default: {
            }   break;
        }
    }

    mXmlWriter->writeEndElement();
}

void GoogleContactStream::startBatchFeed()
{
    mXmlWriter->writeStartElement("atom:feed");
    mXmlWriter->writeAttribute("xmlns:atom", "http://www.w3.org/2005/Atom");
    mXmlWriter->writeAttribute("xmlns:gContact", "http://schemas.google.com/contact/2008");
    mXmlWriter->writeAttribute("xmlns:gd", "http://schemas.google.com/g/2005");
    mXmlWriter->writeAttribute("xmlns:batch", "http://schemas.google.com/gdata/batch");
}

void GoogleContactStream::endBatchFeed()
{
    mXmlWriter->writeEndElement ();
}

void GoogleContactStream::encodeBatchTag(const GoogleContactStream::UpdateType type, const QString &batchElementId)
{
    mXmlWriter->writeTextElement("batch:id", batchElementId);
    if (type == GoogleContactStream::Add) {
        mXmlWriter->writeEmptyElement("batch:operation");
        mXmlWriter->writeAttribute("type", "insert");
    } else if (type == GoogleContactStream::Modify) {
        mXmlWriter->writeEmptyElement("batch:operation");
        mXmlWriter->writeAttribute("type", "update");
    } else if (type == GoogleContactStream::Remove) {
        mXmlWriter->writeEmptyElement("batch:operation");
        mXmlWriter->writeAttribute("type", "delete");
    }
}

void GoogleContactStream::encodeCategory()
{
    mXmlWriter->writeEmptyElement("atom:category");
    mXmlWriter->writeAttribute("schema", "http://schemas.google.com/g/2005#kind");
    mXmlWriter->writeAttribute("term", "http://schemas.google.com/contact/2008#contact");
}

void GoogleContactStream::encodeId(const QContact &qContact, bool isUpdate)
{
    QString guid = qContact.detail(QContactGuid::Type).value(QContactGuid::FieldGuid).toString();
    if (!guid.isEmpty()) {
        QString remoteId = guid.mid(guid.indexOf(":")+1);
        if (isUpdate) {
            // according to the docs, this should be "base" instead of "full" -- but that actually fails.
            if (mAccountEmail.isEmpty()) {
                SOCIALD_LOG_ERROR("account email not known - unable to build batch edit id!");
            } else {
                mXmlWriter->writeTextElement("atom:id", "http://www.google.com/m8/feeds/contacts/" + mAccountEmail + "/full/" + remoteId);
            }
        } else {
            mXmlWriter->writeTextElement("atom:id", "http://www.google.com/m8/feeds/contacts/default/full/" + remoteId);
        }
    }
}

void GoogleContactStream::encodeUpdatedTimestamp(const QContact &qContact)
{
    QContactTimestamp timestamp = qContact.detail<QContactTimestamp>();
    QDateTime updatedTimestamp = timestamp.lastModified(); // will be UTC from database.
    if (!updatedTimestamp.isValid()) {
        updatedTimestamp = QDateTime::currentDateTimeUtc();
    }

    QString updatedStr = updatedTimestamp.toString(QStringLiteral("yyyy-MM-ddThh:mm:ss.zzzZ"));
    mXmlWriter->writeTextElement("updated", updatedStr);
}

void GoogleContactStream::encodeEtag(const QContact &qContact, bool needed)
{
    QContactOriginMetadata etagDetail = qContact.detail<QContactOriginMetadata>();
    QString etag = etagDetail.id();
    if (!etag.isEmpty()) {
        mXmlWriter->writeAttribute("gd:etag", etag);
    } else if (needed) {
        // we're trying to delete a contact in a batch operation
        // but we don't know the etag of the deleted contact.
        SOCIALD_LOG_ERROR("etag needed but not available! caller needs to prefill for deletion updates!");
    }
}

void GoogleContactStream::encodeName(const QContactName &name)
{
    mXmlWriter->writeStartElement("gd:name");
    if (!name.firstName().isEmpty())
        mXmlWriter->writeTextElement("gd:givenName", name.firstName());
    if (!name.middleName().isEmpty())
        mXmlWriter->writeTextElement("gd:additionalName", name.middleName());
    if (!name.lastName().isEmpty())
        mXmlWriter->writeTextElement("gd:familyName", name.lastName());
    if (!name.prefix().isEmpty())
        mXmlWriter->writeTextElement("gd:namePrefix", name.prefix());
    if (!name.suffix().isEmpty())
        mXmlWriter->writeTextElement("gd:nameSuffix", name.suffix());
    mXmlWriter->writeEndElement();
}

void GoogleContactStream::encodePhoneNumber(const QContactPhoneNumber &phoneNumber)
{
    if (phoneNumber.number().isEmpty()) {
        return;
    }

    bool isHome = phoneNumber.contexts().contains(QContactDetail::ContextHome);
    bool isWork = phoneNumber.contexts().contains(QContactDetail::ContextWork);
    int subType = phoneNumber.subTypes().isEmpty()
                ? QContactPhoneNumber::SubTypeMobile // default to mobile
                : phoneNumber.subTypes().first();

    QString rel = "http://schemas.google.com/g/2005#";
    switch (subType) {
        case QContactPhoneNumber::SubTypeLandline: {
                if (isHome) {
                   rel += QString::fromLatin1("home");
                } else if (isWork) {
                   rel += QString::fromLatin1("work");
                } else {
                   rel += QString::fromLatin1("other");
                }
            } break;
        case QContactPhoneNumber::SubTypeMobile: {
                if (isHome) {
                   rel += QString::fromLatin1("mobile");
                } else if (isWork) {
                   rel += QString::fromLatin1("work_mobile");
                } else {
                   rel += QString::fromLatin1("mobile"); // we lose the non-homeness in roundtrip.
                }
            } break;
        case QContactPhoneNumber::SubTypeFax: {
                if (isHome) {
                   rel += QString::fromLatin1("home_fax");
                } else if (isWork) {
                   rel += QString::fromLatin1("work_fax");
                } else {
                   rel += QString::fromLatin1("other_fax");
                }
            } break;
        case QContactPhoneNumber::SubTypePager: {
                if (isHome) {
                   rel += QString::fromLatin1("pager");
                } else if (isWork) {
                   rel += QString::fromLatin1("work_pager");
                } else {
                   rel += QString::fromLatin1("pager"); // we lose the non-homeness in roundtrip.
                }
            } break;
        case QContactPhoneNumber::SubTypeModem: {
               rel += QString::fromLatin1("tty_tdd"); // we lose context in roundtrip.
            } break;
        case QContactPhoneNumber::SubTypeCar: {
               rel += QString::fromLatin1("car"); // we lose context in roundtrip.
            } break;
        case QContactPhoneNumber::SubTypeBulletinBoardSystem: {
               rel += QString::fromLatin1("telex"); // we lose context in roundtrip.
            } break;
        case QContactPhoneNumber::SubTypeAssistant: {
               rel += QString::fromLatin1("assistant");
            } break;
        default: {
                rel += QString::fromLatin1("other");
            } break;
    }

    mXmlWriter->writeStartElement("gd:phoneNumber");
    mXmlWriter->writeAttribute("rel", rel);
    mXmlWriter->writeCharacters(phoneNumber.number());
    mXmlWriter->writeEndElement();
}

void GoogleContactStream::encodeEmailAddress(const QContactEmailAddress &emailAddress)
{
    if (!emailAddress.emailAddress().isEmpty()) {
        mXmlWriter->writeEmptyElement("gd:email");
        if (emailAddress.contexts().contains(QContactDetail::ContextHome)) {
            mXmlWriter->writeAttribute("rel", "http://schemas.google.com/g/2005#home");
        } else if (emailAddress.contexts().contains(QContactDetail::ContextWork)) {
            mXmlWriter->writeAttribute("rel", "http://schemas.google.com/g/2005#work");
        } else {
            mXmlWriter->writeAttribute("rel", "http://schemas.google.com/g/2005#other");
        }
        mXmlWriter->writeAttribute("address", emailAddress.emailAddress());
    }
}

void GoogleContactStream::encodeAddress(const QContactAddress &address)
{
    mXmlWriter->writeStartElement("gd:structuredPostalAddress");
    // https://developers.google.com/google-apps/contacts/v3/reference#structuredPostalAddressRestrictions
    // we cannot use mailClass attribute (for postal/parcel etc)
    mXmlWriter->writeAttribute("rel", "http://schemas.google.com/g/2005#other");
    if (!address.street().isEmpty())
        mXmlWriter->writeTextElement("gd:street", address.street());
    if (!address.locality().isEmpty())
        mXmlWriter->writeTextElement("gd:neighborhood", address.locality());
    if (!address.postOfficeBox().isEmpty())
        mXmlWriter->writeTextElement("gd:pobox", address.postOfficeBox());
    if (!address.region().isEmpty())
        mXmlWriter->writeTextElement("gd:region", address.region());
    if (!address.postcode().isEmpty())
        mXmlWriter->writeTextElement("gd:postcode", address.postcode());
    if (!address.country().isEmpty())
        mXmlWriter->writeTextElement("gd:country", address.country());
    mXmlWriter->writeEndElement();
}

void GoogleContactStream::encodeUrl(const QContactUrl &url)
{
    if (!url.url().isEmpty()) {
        mXmlWriter->writeEmptyElement("gContact:website");
        switch (url.subType()) {
            case QContactUrl::SubTypeHomePage: {
                mXmlWriter->writeAttribute("rel", "home-page");
            } break;
            case QContactUrl::SubTypeBlog: {
                mXmlWriter->writeAttribute("rel", "blog");
            } break;
            default: {
                mXmlWriter->writeAttribute("rel", "other");
            } break;
        }
        mXmlWriter->writeAttribute("href", url.url());
    }
}

void GoogleContactStream::encodeBirthday(const QContactBirthday &birthday)
{
    if (birthday.date().isValid()) {
        mXmlWriter->writeEmptyElement("gContact:birthday");
        mXmlWriter->writeAttribute("when", birthday.date().toString(Qt::ISODate));
    }
}

void
GoogleContactStream::encodeNote(const QContactNote &note)
{
    if (!note.note().isEmpty()) {
        mXmlWriter->writeStartElement("atom:content");
        mXmlWriter->writeAttribute("type", "text");
        mXmlWriter->writeCharacters(note.note());
        mXmlWriter->writeEndElement();
    }
}

void GoogleContactStream::encodeHobby(const QContactHobby &hobby)
{
    if (!hobby.hobby().isEmpty()) {
        mXmlWriter->writeTextElement ("gContact:hobby", hobby.hobby());
    }
}

void GoogleContactStream::encodeGeoLocation(const QContactGeoLocation &geolocation)
{
    Q_UNUSED(geolocation);
    SOCIALD_LOG_INFO("skipping geolocation");
}

void GoogleContactStream::encodeOrganization(const QContactOrganization &organization)
{
    mXmlWriter->writeStartElement("gd:organization");
    mXmlWriter->writeAttribute("rel", "http://schemas.google.com/g/2005#work");
    if (organization.title().length() > 0)
        mXmlWriter->writeTextElement("gd:orgTitle", organization.title());
    if (organization.name().length() > 0)
        mXmlWriter->writeTextElement("gd:orgName", organization.name());
    if (organization.department().length() > 0)
        mXmlWriter->writeTextElement("gd:orgDepartment", organization.department().join(','));
    mXmlWriter->writeEndElement();
}

void GoogleContactStream::encodeAvatar(const QContactAvatar &avatar, const QContact &qContact)
{
    // XXX TODO: determine if it's a new local avatar, if so, push it up.
    QUrl imageUrl(avatar.imageUrl());
    if (imageUrl.isLocalFile()) {
        SOCIALD_LOG_INFO("have avatar:" << imageUrl << "but not upsyncing avatars");
        mEncodedContactsWithAvatars << qContact.id();
    }
}

void GoogleContactStream::encodeGender(const QContactGender &gender)
{
    switch(gender.gender()) {
        case QContactGender::GenderMale: {
            mXmlWriter->writeEmptyElement ("gContact:gender");
            mXmlWriter->writeAttribute ("value", "male");
        } break;
        case QContactGender::GenderFemale: {
            mXmlWriter->writeEmptyElement ("gContact:gender");
            mXmlWriter->writeAttribute ("value", "female");
        } break;
        default: return;
    }
}

void GoogleContactStream::encodeNickname(const QContactNickname &nickname)
{
    if (!nickname.nickname().isEmpty()) {
        mXmlWriter->writeTextElement("gContact:nickname", nickname.nickname());
    }
}

void GoogleContactStream::encodeAnniversary(const QContactAnniversary &anniversary)
{
    if (!anniversary.event().isEmpty() && !anniversary.originalDate().isNull()) {
        mXmlWriter->writeStartElement ("gContact:event");
        mXmlWriter->writeAttribute("rel", "anniversary");
        QString label;
        switch (anniversary.subType()) {
            case QContactAnniversary::SubTypeEngagement: {
                label = QString::fromLatin1("engagement");
            } break;
            case QContactAnniversary::SubTypeEmployment: {
                label = QString::fromLatin1("employment");
            } break;
            case QContactAnniversary::SubTypeMemorial: {
                label = QString::fromLatin1("memorial");
            } break;
            case QContactAnniversary::SubTypeHouse: {
                label = QString::fromLatin1("house");
            } break;
            case QContactAnniversary::SubTypeWedding: // fall through
            default: {
                label = QString::fromLatin1("wedding");
            } break;
        }
        mXmlWriter->writeAttribute("label", label);
        mXmlWriter->writeEmptyElement("gd:when");
        mXmlWriter->writeAttribute("startTime", anniversary.originalDate().toString(Qt::ISODate));
        mXmlWriter->writeEndElement();
    }
}

void GoogleContactStream::encodeOnlineAccount(const QContactOnlineAccount &onlineAccount)
{
    if (onlineAccount.accountUri().isEmpty()) {
        return;
    }

    QContactOnlineAccount::Protocol protocol = onlineAccount.protocol();
    QString protocolName;
    if (protocol == QContactOnlineAccount::ProtocolJabber) {
        protocolName = QString::fromLatin1("JABBER");
    } else if (protocol == QContactOnlineAccount::ProtocolAim) {
        protocolName = QString::fromLatin1("AIM");
    } else if (protocol == QContactOnlineAccount::ProtocolIcq) {
        protocolName = QString::fromLatin1("ICQ");
    } else if (protocol == QContactOnlineAccount::ProtocolMsn) {
        protocolName = QString::fromLatin1("MSN");
    } else if (protocol == QContactOnlineAccount::ProtocolQq) {
        protocolName = QString::fromLatin1("QQ");
    } else if (protocol == QContactOnlineAccount::ProtocolYahoo) {
        protocolName = QString::fromLatin1("YAHOO");
    } else if (protocol == QContactOnlineAccount::ProtocolSkype) {
        protocolName = QString::fromLatin1("SKYPE");
    } else {
        return;
    }

    mXmlWriter->writeEmptyElement ("gd:im");
    mXmlWriter->writeAttribute ("protocol", "http://schemas.google.com/g/2005#" + protocolName);
    // FIXME: The 'rel' value should be properly stored and retrieved
    mXmlWriter->writeAttribute ("rel", "http://schemas.google.com/g/2005#home");
    mXmlWriter->writeAttribute ("address", onlineAccount.accountUri());
}

void GoogleContactStream::encodeFamily(const QContactFamily &family)
{
    if (family.spouse().length() > 0) {
        mXmlWriter->writeEmptyElement("gContact:relation");
        mXmlWriter->writeAttribute("rel", "spouse");
        mXmlWriter->writeCharacters(family.spouse());
    }

    Q_FOREACH (const QString member, family.children()) {
        mXmlWriter->writeEmptyElement("gContact:relation");
        mXmlWriter->writeAttribute("rel", "child");
        mXmlWriter->writeCharacters(member);
    }
}

void GoogleContactStream::encodeUnknownElements(const QStringList &unknownElements)
{
    // ugly hack to get the QXmlStreamWriter to write a pre-formatted element...
    foreach (const QString &unknownElement, unknownElements) {
        QString concat;
        concat.append("<?xml version=\"1.0\"?>");
        concat.append("<container>");
        concat.append(unknownElement);
        concat.append("</container>");

        QXmlStreamReader tokenizer(concat);
        tokenizer.readNextStartElement(); // read past the xml document element start.
        QString text = tokenizer.readElementText();
        mXmlWriter->writeStartElement(tokenizer.qualifiedName().toString());
        mXmlWriter->writeAttributes(tokenizer.attributes());
        if (!text.isEmpty()) {
            mXmlWriter->writeCharacters(text);
        }
        mXmlWriter->writeEndElement();
    }
}

