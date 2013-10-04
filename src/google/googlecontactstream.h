/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd. and/or its subsidiary(-ies).
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ** Contributors: Sateesh Kavuri <sateesh.kavuri@gmail.com>
 **               Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef GOOGLESTREAM_H
#define GOOGLESTREAM_H

#include <QObject>

#include <QXmlStreamReader>
#include <QMap>

#include <QContact>
#include <QContactDetail>

USE_CONTACTS_NAMESPACE

class GoogleContactAtom;
class GoogleContactStream : public QObject
{
    Q_OBJECT

public:
    explicit GoogleContactStream(bool response, QObject* parent = 0);
    explicit GoogleContactStream(const QByteArray &xmlStream, QObject *parent = 0);
    ~GoogleContactStream();

    GoogleContactAtom* parse(const QByteArray &xmlBuffer);

signals:
    void parseDone(bool);

private:
    void initAtomFunctionMap();
    void initResponseFunctionMap();
    void initFunctionMap();

    // Atom feed elements handler methods
    void handleAtomUpdated();
    void handleAtomCategory();
    void handleAtomAuthor();
    void handleAtomOpenSearch();
    void handleAtomEntry();
    void handleAtomLink();

    // Following are for the response received from the server
    // incase of failures
    //void handleEntryBatchStatus();
    //void handleEntryBatchOperation();

    // gContact:xxx schema handler methods
    QContactDetail handleEntryId();
    QContactDetail handleEntryContent();
    QContactDetail handleEntryLink();
    QContactDetail handleEntryBirthday();
    QContactDetail handleEntryGender();
    QContactDetail handleEntryHobby();
    QContactDetail handleEntryNickname();
    QContactDetail handleEntryOccupation();
    QContactDetail handleEntryWebsite();
    QContactDetail handleEntryComments();
    QContactDetail handleEntryEmail();
    QContactDetail handleEntryIm();
    QContactDetail handleEntryName();
    QContactDetail handleEntryOrganization();
    QContactDetail handleEntryPhoneNumber();
    QContactDetail handleEntryStructuredPostalAddress();

    QXmlStreamReader *mXml;

    typedef void (GoogleContactStream::*Handler)();
    typedef QContactDetail (GoogleContactStream::*DetailHandler)();
    QMap<QString, GoogleContactStream::Handler> mAtomFunctionMap;
    QMap<QString, GoogleContactStream::DetailHandler> mContactFunctionMap;

    GoogleContactAtom *mAtom;
};

#endif // GOOGLECONTACTSTREAM_H
