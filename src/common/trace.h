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
