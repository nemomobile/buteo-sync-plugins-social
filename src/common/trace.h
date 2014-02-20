/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef TRACE_H
#define TRACE_H

#include <QtDebug>

#define SOCIALD_DEBUG 3
#define SOCIALD_INFORMATION 2
#define SOCIALD_ERROR 1

#define TRACE(level, message)                                       \
    do {                                                            \
        QByteArray llba = qgetenv("SOCIALD_LOGGING_LEVEL");         \
        QString llstr = QString::fromLocal8Bit(llba.constData());   \
        bool ok = false;                                            \
        int llint = llstr.toInt(&ok);                               \
        if (!ok) {                                                  \
            llint = -1;                                             \
        }                                                           \
        if (level <= llint) {                                       \
            qWarning() << Q_FUNC_INFO << message;                   \
        }                                                           \
    } while (0)

#endif // TRACE_H
