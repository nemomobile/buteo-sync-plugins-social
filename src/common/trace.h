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

#include "buteosyncfw_p.h"

#define SOCIALD_LOG_TRACE(message)   LOG_TRACE("trace: "   << message) /* MSYNCD_LOGGING_LEVEL >= 8 */
#define SOCIALD_LOG_DEBUG(message)   LOG_DEBUG("debug: "   << message) /* MSYNCD_LOGGING_LEVEL >= 7 */
#define SOCIALD_LOG_INFO(message)    LOG_INFO("info : "    << message) /* MSYNCD_LOGGING_LEVEL >= 6 */
#define SOCIALD_LOG_ERROR(message)   LOG_WARNING("ERROR: " << message) /* MSYNCD_LOGGING_LEVEL == * */

#endif // TRACE_H
