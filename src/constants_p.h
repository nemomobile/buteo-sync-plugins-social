/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Raine Makelainen <raine.makelainen@jollamobile.com>
 **
 ****************************************************************************/

#ifndef SOCIALD_CONSTANTS_P_H
#define SOCIALD_CONSTANTS_P_H

#include <QContactName>
#include <QContactAvatar>

BEGIN_CONTACTS_NAMESPACE
// some custom fields supported by qtcontacts-sqlite.
static const int QContactName__FieldCustomLabel = (QContactName::FieldSuffix+1);
static const int QContactAvatar__FieldAvatarMetadata = (QContactAvatar::FieldVideoUrl+1);
END_CONTACTS_NAMESPACE

#endif
