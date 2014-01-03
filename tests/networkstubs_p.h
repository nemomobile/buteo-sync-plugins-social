/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
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