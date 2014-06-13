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

#ifndef SOCIALD_TESTS_NETWORKSTUBS_P_H
#define SOCIALD_TESTS_NETWORKSTUBS_P_H

#include <QByteArray>
#include <QNetworkReply>
#include <QUrl>
#include <QString>
#include <QtDebug>

#include "socialdnetworkaccessmanager_p.h"

class TestNetworkReply : public QNetworkReply
{
    Q_OBJECT

public:
    bool isSequential() const { return true; }
    void abort() { qWarning() << "TNR::abort"; }
    qint64 bytesAvailable() const { return m_readAllData.size() + QIODevice::bytesAvailable(); }
    qint64 size() const { return bytesAvailable(); }
    bool atEnd() const { return m_readAllData.size() == 0; }
    bool canReadLine() const { return !atEnd(); }
    qint64 readData(char *buf, qint64 sz) {
        qint64 i = 0;
        while (!m_readAllData.isEmpty() && i++ < sz) {
            *buf++ = m_readAllData.at(0);
            m_readAllData.remove(0,1);
        }
        return i;
    }

protected:
    TestNetworkReply(QObject *parent = 0) : QNetworkReply(parent) {}

private:
    static QByteArray generateData(const QUrl &requestUrl, const QString &generator);
    QByteArray m_readAllData;
    friend class SocialdNetworkAccessManager;
};

#endif