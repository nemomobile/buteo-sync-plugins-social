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

#include "googlecalendarsyncadaptor.h"
#include "googlecalendarincidencecomparator.h"
#include "trace.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QByteArray>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QSettings>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <ksystemtimezone.h>
#include <kdatetime.h>

//----------------------------------------------

#define RFC3339_FORMAT      "%Y-%m-%dT%H:%M:%S%:z"
#define RFC3339_FORMAT_NTZC "%Y-%m-%dT%H:%M:%S%z"
#define RFC3339_QDATE_FORMAT_MS "yyyy-MM-ddThh:mm:ss.zzzZ"
#define RFC3339_QDATE_FORMAT_MS_NTZC "yyyy-MM-ddThh:mm:ss.zzz"
#define KDATEONLY_FORMAT    "%Y-%m-%d"
#define QDATEONLY_FORMAT    "yyyy-MM-dd"
#define KLONGTZ_FORMAT      "%:Z"
#define RFC5545_KDATETIME_FORMAT "%Y%m%dT%H%M%SZ"
#define RFC5545_KDATETIME_FORMAT_NTZC "%Y%m%dT%H%M%S"
#define RFC5545_QDATE_FORMAT "yyyyMMdd"

namespace {

static int GOOGLE_CAL_SYNC_PLUGIN_VERSION = 2;

void errorDumpStr(const QString &str)
{
    // Dump the entire string to the log.
    // Note that the log cannot handle newlines,
    // so we separate the string into chunks.
    Q_FOREACH (const QString &chunk, str.split('\n', QString::SkipEmptyParts)) {
        SOCIALD_LOG_ERROR(chunk);
    }
}

void traceDumpStr(const QString &str)
{
    // 8 is the minimum log level for TRACE logs
    // as defined in Buteo's LogMacros.h
    if (Buteo::Logger::instance()->getLogLevel() < 8) {
        return;
    }

    // Dump the entire string to the log.
    // Note that the log cannot handle newlines,
    // so we separate the string into chunks.
    Q_FOREACH (const QString &chunk, str.split('\n', QString::SkipEmptyParts)) {
        SOCIALD_LOG_TRACE(chunk);
    }
}

// returns true if the ghost-event cleanup sync has been performed.
bool ghostEventCleanupPerformed()
{
    QString settingsFileName = QString::fromLatin1("%1/%2/gcal.ini")
            .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR))
            .arg(QString::fromLatin1(SYNC_DATABASE_DIR));
    QSettings settingsFile(settingsFileName, QSettings::IniFormat);
    return settingsFile.value(QString::fromLatin1("cleaned"), QVariant::fromValue<bool>(false)).toBool();
}
void setGhostEventCleanupPerformed()
{
    QString settingsFileName = QString::fromLatin1("%1/%2/gcal.ini")
            .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR))
            .arg(QString::fromLatin1(SYNC_DATABASE_DIR));
    QSettings settingsFile(settingsFileName, QSettings::IniFormat);
    settingsFile.setValue(QString::fromLatin1("cleaned"), QVariant::fromValue<bool>(true));
    settingsFile.sync();
}

QString gCalEventId(KCalCore::Incidence::Ptr event)
{
    // we abuse the comments field to store our gcal-id.
    // we should use a custom property, but those are deleted on incidence deletion.
    const QStringList &comments(event->comments());
    Q_FOREACH (const QString &comment, comments) {
        if (comment.startsWith("jolla-sociald:gcal-id:")) {
            return comment.mid(22);
        }
    }
    return QString();
}
void setGCalEventId(KCalCore::Incidence::Ptr event, const QString &id)
{
    // we abuse the comments field to store our gcal-id.
    // we should use a custom property, but those are deleted on incidence deletion.
    const QStringList &comments(event->comments());
    Q_FOREACH (const QString &comment, comments) {
        if (comment.startsWith("jolla-sociald:gcal-id:")) {
            // remove any existing gcal-id comment.
            event->removeComment(comment);
            break;
        }
    }
    event->addComment(QStringLiteral("jolla-sociald:gcal-id:%1").arg(id));
}

QString gCalETag(KCalCore::Incidence::Ptr event)
{
    return event->customProperty("jolla-sociald", "gcal-etag");
}
void setGCalETag(KCalCore::Incidence::Ptr event, const QString &etag)
{
    // note: custom properties are purged on incidence deletion.
    event->setCustomProperty("jolla-sociald", "gcal-etag", etag);
}

KDateTime datetimeFromUpdateStr(const QString &update)
{
    // generally, this is an RFC3339 date with timezone information, like:
    // 2015-04-25T12:02:40.988Z
    // However, our version of KDateTime is old enough that we don't support this
    // date format directly.
    bool tzIncluded = update.endsWith('Z');
    QDateTime qdt = tzIncluded
                  ? QDateTime::fromString(update, RFC3339_QDATE_FORMAT_MS)
                  : QDateTime::fromString(update, RFC3339_QDATE_FORMAT_MS_NTZC);
    if (tzIncluded) {
        qdt.setTimeSpec(Qt::UTC);
    }
    return KDateTime(qdt, tzIncluded ? KDateTime::UTC : KDateTime::ClockTime);
}

QList<KDateTime> datetimesFromExRDateStr(const QString &exrdatestr, bool *isDateOnly)
{
    // possible forms:
    // RDATE:19970714T123000Z
    // RDATE;VALUE=DATE-TIME:19970714T123000Z
    // RDATE;VALUE=DATE-TIME:19970714T123000Z,19970715T123000Z
    // RDATE;TZID=America/New_York:19970714T083000
    // RDATE;VALUE=PERIOD:19960403T020000Z/19960403T040000Z,19960404T010000Z/PT3H
    // RDATE;VALUE=DATE:19970101,19970120

    QList<KDateTime> retn;
    QString str = exrdatestr;
    *isDateOnly = false; // by default.

    if (str.startsWith(QStringLiteral("exdate"), Qt::CaseInsensitive)) {
        str.remove(0, 6);
    } else if (str.startsWith(QStringLiteral("rdate"), Qt::CaseInsensitive)) {
        str.remove(0, 5);
    } else {
        SOCIALD_LOG_ERROR("not an ex/rdate string:" << exrdatestr);
        return retn;
    }

    if (str.startsWith(';')) {
        str.remove(0,1);
        if (str.startsWith("DATE-TIME:", Qt::CaseInsensitive)) {
            str.remove(0, 10);
            QStringList dts = str.split(',');
            Q_FOREACH (const QString &dtstr, dts) {
                if (dtstr.endsWith('Z')) {
                    // UTC
                    KDateTime kdt = KDateTime::fromString(dtstr, RFC5545_KDATETIME_FORMAT);
                    kdt.setTimeSpec(KDateTime::Spec::UTC());
                    retn.append(kdt);
                } else {
                    // Floating time
                    KDateTime kdt = KDateTime::fromString(dtstr, RFC5545_KDATETIME_FORMAT_NTZC);
                    kdt.setTimeSpec(KDateTime::Spec::ClockTime());
                    retn.append(kdt);
                }
            }
        } else if (str.startsWith("DATE:", Qt::CaseInsensitive)) {
            str.remove(0, 5);
            QStringList dts = str.split(',');
            Q_FOREACH(const QString &dstr, dts) {
                QDate date = QDate::fromString(dstr, RFC5545_QDATE_FORMAT);
                KDateTime kdt(date, KDateTime::Spec::ClockTime());
                retn.append(kdt);
            }
        } else if (str.startsWith("PERIOD:", Qt::CaseInsensitive)) {
            SOCIALD_LOG_ERROR("unsupported parameter in ex/rdate string:" << exrdatestr);
            // TODO: support PERIOD formats, or just switch to CalDAV for Google sync...
        } else if (str.startsWith("TZID=") && str.contains(':')) {
            str.remove(0, 5);
            QString tzidstr = str.mid(0, str.indexOf(':')); // something like: "Australia/Brisbane"
            KTimeZone tz = KSystemTimeZones::zone(tzidstr);
            str.remove(0, tzidstr.size()+1);
            QStringList dts = str.split(',');
            Q_FOREACH (const QString &dtstr, dts) {
                KDateTime kdt = KDateTime::fromString(dtstr, RFC5545_KDATETIME_FORMAT_NTZC);
                if (!kdt.isValid()) {
                    // try parsing from alternate formats
                    kdt = KDateTime::fromString(dtstr, RFC3339_FORMAT_NTZC);
                }
                if (!kdt.isValid()) {
                    SOCIALD_LOG_ERROR("unable to parse datetime from ex/rdate string:" << exrdatestr);
                } else {
                    if (tz.isValid()) {
                        kdt.setTimeSpec(tz);
                    } else {
                        kdt.setTimeSpec(KDateTime::Spec::ClockTime());
                        SOCIALD_LOG_INFO("WARNING: unknown tzid:" << tzidstr << "; assuming clock-time instead!");
                    }
                    retn.append(kdt);
                }
            }
        } else {
            SOCIALD_LOG_ERROR("invalid parameter in ex/rdate string:" << exrdatestr);
        }
    } else if (str.startsWith(':')) {
        str.remove(0,1);
        QStringList dts = str.split(',');
        Q_FOREACH (const QString &dtstr, dts) {
            if (dtstr.endsWith('Z')) {
                // UTC
                KDateTime kdt = KDateTime::fromString(dtstr, RFC5545_KDATETIME_FORMAT);
                if (!kdt.isValid()) {
                    // try parsing from alternate formats
                    kdt = KDateTime::fromString(dtstr, RFC3339_FORMAT);
                }
                if (!kdt.isValid()) {
                    SOCIALD_LOG_ERROR("unable to parse datetime from ex/rdate string:" << exrdatestr);
                } else {
                    // parsed successfully
                    kdt.setTimeSpec(KDateTime::Spec::UTC());
                    retn.append(kdt);
                }
            } else {
                // Floating time
                KDateTime kdt = KDateTime::fromString(dtstr, RFC5545_KDATETIME_FORMAT_NTZC);
                if (!kdt.isValid()) {
                    // try parsing from alternate formats
                    kdt = KDateTime::fromString(dtstr, RFC3339_FORMAT_NTZC);
                }
                if (!kdt.isValid()) {
                    SOCIALD_LOG_ERROR("unable to parse datetime from ex/rdate string:" << exrdatestr);
                } else {
                    // parsed successfully
                    kdt.setTimeSpec(KDateTime::Spec::ClockTime());
                    retn.append(kdt);
                }
            }
        }
    } else {
        SOCIALD_LOG_ERROR("not a valid ex/rdate string:" << exrdatestr);
    }

    return retn;
}

QJsonArray recurrenceArray(KCalCore::Event::Ptr event, KCalCore::ICalFormat &icalFormat)
{
    QJsonArray retn;

    // RRULE
    KCalCore::Recurrence *kcalRecurrence = event->recurrence();
    Q_FOREACH (KCalCore::RecurrenceRule *rrule, kcalRecurrence->rRules()) {
        QString rruleStr = icalFormat.toString(rrule);
        rruleStr.replace("\r\n", "");
        retn.append(QJsonValue(rruleStr));
    }

    // EXRULE
    Q_FOREACH (KCalCore::RecurrenceRule *exrule, kcalRecurrence->exRules()) {
        QString exruleStr = icalFormat.toString(exrule);
        exruleStr.replace("RRULE", "EXRULE");
        exruleStr.replace("\r\n", "");
        retn.append(QJsonValue(exruleStr));
    }

    // RDATE (date)
    QString rdates;
    Q_FOREACH (const QDate &rdate, kcalRecurrence->rDates()) {
        rdates.append(rdate.toString(RFC5545_QDATE_FORMAT));
        rdates.append(',');
    }
    if (rdates.size()) {
        rdates.chop(1); // trailing comma
        retn.append(QJsonValue(QString::fromLatin1("RDATE;VALUE=DATE:%1").arg(rdates)));
    }

    // RDATE (date-time)
    QString rdatetimes;
    Q_FOREACH (const KDateTime &rdatetime, kcalRecurrence->rDateTimes()) {
        if (rdatetime.timeSpec() == KDateTime::Spec::ClockTime()) {
            rdatetimes.append(rdatetime.toString(RFC5545_KDATETIME_FORMAT_NTZC));
        } else {
            rdatetimes.append(rdatetime.toUtc().toString(RFC5545_KDATETIME_FORMAT));
        }
        rdatetimes.append(',');
    }
    if (rdatetimes.size()) {
        rdatetimes.chop(1); // trailing comma
        retn.append(QJsonValue(QString::fromLatin1("RDATE;VALUE=DATE-TIME:%1").arg(rdatetimes)));
    }

    // EXDATE (date)
    QString exdates;
    Q_FOREACH (const QDate &exdate, kcalRecurrence->exDates()) {
        exdates.append(exdate.toString(RFC5545_QDATE_FORMAT));
        exdates.append(',');
    }
    if (exdates.size()) {
        exdates.chop(1); // trailing comma
        retn.append(QJsonValue(QString::fromLatin1("EXDATE;VALUE=DATE:%1").arg(exdates)));
    }

    // EXDATE (date-time)
    QString exdatetimes;
    Q_FOREACH (const KDateTime &exdatetime, kcalRecurrence->exDateTimes()) {
        if (exdatetime.timeSpec() == KDateTime::Spec::ClockTime()) {
            exdatetimes.append(exdatetime.toString(RFC5545_KDATETIME_FORMAT_NTZC));
        } else {
            exdatetimes.append(exdatetime.toUtc().toString(RFC5545_KDATETIME_FORMAT));
        }
        exdatetimes.append(',');
    }
    if (exdatetimes.size()) {
        exdatetimes.chop(1); // trailing comma
        retn.append(QJsonValue(QString::fromLatin1("EXDATE;VALUE=DATE-TIME:%1").arg(exdatetimes)));
    }

    return retn;
}

KDateTime parseRecurrenceId(const QJsonObject &originalStartTime)
{
    QString recurrenceIdStr = originalStartTime.value(QLatin1String("dateTime")).toVariant().toString();
    QString recurrenceIdTzStr = originalStartTime.value(QLatin1String("timeZone")).toVariant().toString();
    KDateTime recurrenceId = KDateTime::fromString(recurrenceIdStr, RFC3339_FORMAT);
    if (!recurrenceIdTzStr.isEmpty()) {
        recurrenceId = recurrenceId.toTimeSpec(KTimeZone(recurrenceIdTzStr));
    }
    return recurrenceId;
}

QJsonObject kCalToJson(KCalCore::Event::Ptr event, KCalCore::ICalFormat &icalFormat, bool setUidProperty = false)
{
    QString eventId = gCalEventId(event);
    QJsonObject start, end;

    // insert the date/time and timeZone information into the Json object.
    // note that timeZone is required for recurring events, for some reason.
    if (event->dtStart().isDateOnly() || (event->allDay() && event->dtStart().time() == QTime(0,0,0))) {
        start.insert(QLatin1String("date"), event->dtStart().date().toString(QDATEONLY_FORMAT));
    } else {
        start.insert(QLatin1String("dateTime"), event->dtStart().toString(RFC3339_FORMAT));
        start.insert(QLatin1String("timeZone"), QJsonValue(event->dtStart().toString(KLONGTZ_FORMAT)));
    }
    if (event->dtEnd().isDateOnly() || (event->allDay() && event->dtEnd().time() == QTime(0,0,0))) {
        // note: for iCal spec, allDay events need to have an end date of real-end-date+1 as end date is exclusive.
        end.insert(QLatin1String("date"), event->dateEnd().addDays(1).toString(QDATEONLY_FORMAT));
    } else {
        end.insert(QLatin1String("dateTime"), event->dtEnd().toString(RFC3339_FORMAT));
        end.insert(QLatin1String("timeZone"), QJsonValue(event->dtEnd().toString(KLONGTZ_FORMAT)));
    }

    QJsonObject retn;
    if (!eventId.isEmpty()) retn.insert(QLatin1String("id"), eventId);
    if (event->recurrence()) retn.insert(QLatin1String("recurrence"), recurrenceArray(event, icalFormat));
    retn.insert(QLatin1String("summary"), event->summary());
    retn.insert(QLatin1String("description"), event->description());
    retn.insert(QLatin1String("location"), event->location());
    retn.insert(QLatin1String("start"), start);
    retn.insert(QLatin1String("end"), end);
    retn.insert(QLatin1String("sequence"), QString::number(event->revision()+1));
    //retn.insert(QLatin1String("locked"), event->readOnly()); // only allow locking server-side.
    // we may wish to support locking/readonly from local side also, in the future.

    if (setUidProperty) {
        // now we store private extended properties: local uid.
        // this allows us to detect partially-upsynced artifacts during subsequent syncs.
        // usually this codepath will be hit for localAdditions being upsynced,
        // but sometimes also if we need to update the mapping due to clean-sync.
        QJsonObject privateExtendedProperties;
        privateExtendedProperties.insert(QLatin1String("x-jolla-sociald-mkcal-uid"), event->uid());
        QJsonObject extendedProperties;
        extendedProperties.insert(QLatin1String("private"), privateExtendedProperties);
        retn.insert(QLatin1String("extendedProperties"), extendedProperties);
    }

    return retn;
}

void extractStartAndEnd(const QJsonObject &eventData,
                        bool *startExists,
                        bool *endExists,
                        bool *startIsDateOnly,
                        bool *endIsDateOnly,
                        bool *isAllDay,
                        KDateTime *start,
                        KDateTime *end)
{
    *startIsDateOnly = false, *endIsDateOnly = false;
    QString startTimeString, endTimeString;
    QJsonObject startTimeData = eventData.value(QLatin1String("start")).toObject();
    QJsonObject endTimeData = eventData.value(QLatin1String("end")).toObject();
    if (!startTimeData.value(QLatin1String("date")).toVariant().toString().isEmpty()) {
        *startExists = true;
        *startIsDateOnly = true; // all-day event.
        startTimeString = startTimeData.value(QLatin1String("date")).toVariant().toString();
    } else if (!startTimeData.value(QLatin1String("dateTime")).toVariant().toString().isEmpty()) {
        *startExists = true;
        startTimeString = startTimeData.value(QLatin1String("dateTime")).toVariant().toString();
    } else {
        *startExists = false;
    }
    if (!endTimeData.value(QLatin1String("date")).toVariant().toString().isEmpty()) {
        *endExists = true;
        *endIsDateOnly = true; // all-day event.
        endTimeString = endTimeData.value(QLatin1String("date")).toVariant().toString();
    } else if (!endTimeData.value(QLatin1String("dateTime")).toVariant().toString().isEmpty()) {
        *endExists = true;
        endTimeString = endTimeData.value(QLatin1String("dateTime")).toVariant().toString();
    } else {
        *endExists = false;
    }

    if (*startExists) {
        if (!*startIsDateOnly) {
            KDateTime parsedStartTime = KDateTime::fromString(startTimeString, RFC3339_FORMAT);
            KDateTime ntzcStartTime = KDateTime::fromString(startTimeString, RFC3339_FORMAT_NTZC);
            if (ntzcStartTime.time() > parsedStartTime.time()) parsedStartTime = ntzcStartTime;

            // different format?  let KDateTime detect the format automatically.
            if (parsedStartTime.isNull()) {
                parsedStartTime = KDateTime::fromString(startTimeString);
            }

            *start = parsedStartTime.toLocalZone();
        } else {
            *start = KDateTime(QDate::fromString(startTimeString, QDATEONLY_FORMAT), QTime(), KDateTime::ClockTime);
            // note: don't call start->setDateOnly(true); or mkcal doesn't like it.
        }
    }

    if (*endExists) {
        if (!*endIsDateOnly) {
            KDateTime parsedEndTime = KDateTime::fromString(endTimeString, RFC3339_FORMAT);
            KDateTime ntzcEndTime = KDateTime::fromString(endTimeString, RFC3339_FORMAT_NTZC);
            if (ntzcEndTime.time() > parsedEndTime.time()) parsedEndTime = ntzcEndTime;

            // different format?  let KDateTime detect the format automatically.
            if (parsedEndTime.isNull()) {
                parsedEndTime = KDateTime::fromString(endTimeString);
            }

            *end = parsedEndTime.toLocalZone();
        } else {
            // Special handling for all-day events is required.
            if (*startExists && *startIsDateOnly) {
                if (QDate::fromString(startTimeString, QDATEONLY_FORMAT)
                        == QDate::fromString(endTimeString, QDATEONLY_FORMAT)) {
                    // single-day all-day event
                    *endExists = false;
                    *isAllDay = true;
                } else if (QDate::fromString(startTimeString, QDATEONLY_FORMAT)
                        == QDate::fromString(endTimeString, QDATEONLY_FORMAT).addDays(-1)) {
                    // Google will send a single-day all-day event has having an end-date
                    // of startDate+1 to conform to iCal spec.  Hence, this is actually
                    // a single-day all-day event, despite the difference in end-date.
                    *endExists = false;
                    *isAllDay = true;
                } else {
                    // multi-day all-day event.
                    // as noted above, Google will send all-day events as having an end-date
                    // of real-end-date+1 in order to conform to iCal spec (exclusive end dt).
                    *start = KDateTime(QDate::fromString(startTimeString, QDATEONLY_FORMAT), QTime(), KDateTime::ClockTime);
                    *end = KDateTime(QDate::fromString(endTimeString, QDATEONLY_FORMAT).addDays(-1), QTime(), KDateTime::ClockTime);
                    *isAllDay = true;
                }
            } else {
                *end = KDateTime(QDate::fromString(endTimeString, QDATEONLY_FORMAT).addDays(-1), QTime(), KDateTime::ClockTime);
                // note: don't call end->setDateOnly(true); or mkcal doesn't like it.
                *isAllDay = false;
            }
        }
    }
}

void extractRecurrence(const QJsonArray &recurrence, KCalCore::Event::Ptr event, KCalCore::ICalFormat &icalFormat)
{
    KCalCore::Recurrence *kcalRecurrence = event->recurrence();
    kcalRecurrence->clear(); // avoid adding duplicate recurrence information
    for (int i = 0; i < recurrence.size(); ++i) {
        QString ruleStr = recurrence.at(i).toString();
        if (ruleStr.startsWith(QString::fromLatin1("rrule"), Qt::CaseInsensitive)) {
            KCalCore::RecurrenceRule *rrule = new KCalCore::RecurrenceRule;
            if (!icalFormat.fromString(rrule, ruleStr.mid(6))) {
                SOCIALD_LOG_DEBUG("unable to parse RRULE information:" << ruleStr);
                traceDumpStr(QString::fromUtf8(QJsonDocument(recurrence).toJson()));
            } else {
                kcalRecurrence->addRRule(rrule);
            }
        } else if (ruleStr.startsWith(QString::fromLatin1("exrule"), Qt::CaseInsensitive)) {
            KCalCore::RecurrenceRule *exrule = new KCalCore::RecurrenceRule;
            if (!icalFormat.fromString(exrule, ruleStr.mid(7))) {
                SOCIALD_LOG_DEBUG("unable to parse EXRULE information:" << ruleStr);
                traceDumpStr(QString::fromUtf8(QJsonDocument(recurrence).toJson()));
            } else {
                kcalRecurrence->addExRule(exrule);
            }
        } else if (ruleStr.startsWith(QString::fromLatin1("rdate"), Qt::CaseInsensitive)) {
            bool isDateOnly = false;
            QList<KDateTime> rdatetimes = datetimesFromExRDateStr(ruleStr, &isDateOnly);
            if (!rdatetimes.size()) {
                SOCIALD_LOG_DEBUG("unable to parse RDATE information:" << ruleStr);
                traceDumpStr(QString::fromUtf8(QJsonDocument(recurrence).toJson()));
            } else {
                Q_FOREACH (const KDateTime &kdt, rdatetimes) {
                    if (isDateOnly) {
                        kcalRecurrence->addRDate(kdt.date());
                    } else {
                        kcalRecurrence->addRDateTime(kdt);
                    }
                }
            }
        } else if (ruleStr.startsWith(QString::fromLatin1("exdate"), Qt::CaseInsensitive)) {
            bool isDateOnly = false;
            QList<KDateTime> exdatetimes = datetimesFromExRDateStr(ruleStr, &isDateOnly);
            if (!exdatetimes.size()) {
                SOCIALD_LOG_DEBUG("unable to parse EXDATE information:" << ruleStr);
                traceDumpStr(QString::fromUtf8(QJsonDocument(recurrence).toJson()));
            } else {
                Q_FOREACH (const KDateTime &kdt, exdatetimes) {
                    if (isDateOnly) {
                        kcalRecurrence->addExDate(kdt.date());
                    } else {
                        kcalRecurrence->addExDateTime(kdt);
                    }
                }
            }
        } else {
          SOCIALD_LOG_DEBUG("unknown recurrence information:" << ruleStr);
          traceDumpStr(QString::fromUtf8(QJsonDocument(recurrence).toJson()));
        }
    }
}

int nearestNemoReminderStartOffset(int googleStartOffset)
{
    // Google supports arbitrary start offsets, whereas in Nemo's UI
    // we only allow specific reminder offsets.
    // See nemo-qml-plugin-calendar::NemoCalendarEvent::Reminder for
    // those offset definitions.
    // Also, Nemo reminder offsets are negative and in seconds,
    // whereas Google start offsets are positive and in minutes.
    if (googleStartOffset >= 0 && googleStartOffset <= 5) {
        return -5 * 60;     // 5 minutes before event start
    } else if (googleStartOffset > 5 && googleStartOffset <= 15) {
        return -15 * 60;    // 15 minutes before event start
    } else if (googleStartOffset > 15 && googleStartOffset <= 30) {
        return -30 * 60;    // 30 minutes before event start
    } else if (googleStartOffset > 30 && googleStartOffset <= 60) {
        return -60 * 60;    // 1 hour before event start
    } else if (googleStartOffset > 60 && googleStartOffset <= 120) {
        return -120 * 60;   // 2 hours before event start
    } else if (googleStartOffset > 120 && googleStartOffset <= 1440) {
        return -1440 * 60;  // 1 day before event start (24 hours)
    } else if (googleStartOffset > 1440) {
        return -2880 * 60;  // 2 days before event start (48 hours)
    }

    // default reminder: 15 minutes before event start.
    return -15 * 60;
}

#define START_EVENT_UPDATES_IF_REQUIRED(event, changed) \
    if (*changed == false) {                            \
        event->startUpdates();                          \
    }                                                   \
    *changed = true;

#define UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, getter, setter, newValue, changed) \
    if (event->getter() != newValue) {                                              \
        START_EVENT_UPDATES_IF_REQUIRED(event, changed)                             \
        event->setter(newValue);                                                    \
    }

#define END_EVENT_UPDATES_IF_REQUIRED(event, changed, startedUpdates)               \
    if (*changed == false) {                                                        \
        SOCIALD_LOG_DEBUG("Ignoring spurious change reported for:" <<               \
                          event->uid() << event->revision() << event->summary());   \
    } else if (startedUpdates) {                                                    \
        event->endUpdates();                                                        \
    }

void extractAlarms(const QJsonObject &json, KCalCore::Event::Ptr event, int defaultReminderStartOffset, bool *changed)
{
    int startOffset = -1;
    if (json.contains(QStringLiteral("reminders"))) {
        QJsonObject reminders = json.value(QStringLiteral("reminders")).toObject();
        if (reminders.value(QStringLiteral("useDefault")).toBool()) {
            if (defaultReminderStartOffset > 0) {
                startOffset = defaultReminderStartOffset;
            } else {
                SOCIALD_LOG_DEBUG("not adding default reminder even though requested: not popup or invalid start offset.");
            }
        } else {
            QJsonArray overrides = reminders.value(QStringLiteral("overrides")).toArray();
            for (int i = 0; i < overrides.size(); ++i) {
                QJsonObject override = overrides.at(i).toObject();
                if (override.value(QStringLiteral("method")).toString() == QStringLiteral("popup")) {
                    startOffset = override.value(QStringLiteral("minutes")).toInt();
                }
            }
        }
        if (startOffset > -1) {
            startOffset = nearestNemoReminderStartOffset(startOffset);
            SOCIALD_LOG_DEBUG("event needs reminder with start offset (seconds):" << startOffset);
            KCalCore::Alarm::List alarms = event->alarms();
            int alarmCount = 0;
            // check that we have only one non-procedure alarm,
            // and then check to see if its start offset is correct.
            for (int i = 0; i < alarms.count(); ++i) {
                // we don't count Procedure type alarms.
                if (alarms.at(i)->type() != KCalCore::Alarm::Procedure) {
                    alarmCount += 1;
                    if (alarms.at(i)->startOffset().asSeconds() == startOffset) {
                        // no change required to this alarm.
                    } else {
                        alarmCount += 1; // this will cause alarm modification.
                    }
                }
            }
            if (alarmCount == 1) {
                // no need to modify alarms for this event
                SOCIALD_LOG_DEBUG("event already has reminder with start offset (seconds):" << startOffset);
            } else {
                SOCIALD_LOG_DEBUG("setting event reminder with start offset (seconds):" << startOffset);
                START_EVENT_UPDATES_IF_REQUIRED(event, changed);
                for (int i = 0; i < alarms.count(); ++i) {
                    if (alarms.at(i)->type() != KCalCore::Alarm::Procedure) {
                        event->removeAlarm(alarms.at(i));
                    }
                }
                KCalCore::Alarm::Ptr alarm = event->newAlarm();
                alarm->setEnabled(true);
                alarm->setStartOffset(KCalCore::Duration(startOffset));
            }
        }
    }
    if (startOffset == -1) {
        // no reminders were defined in the json received from Google.
        // remove any alarms as required from the local event.
        KCalCore::Alarm::List alarms = event->alarms();
        for (int i = 0; i < alarms.count(); ++i) {
            if (alarms.at(i)->type() != KCalCore::Alarm::Procedure) {
                SOCIALD_LOG_DEBUG("removing event reminder with start offset (seconds):" << alarms.at(i)->startOffset().asSeconds());
                START_EVENT_UPDATES_IF_REQUIRED(event, changed);
                event->removeAlarm(alarms.at(i));
            }
        }
    }
}

void jsonToKCal(const QJsonObject &json, KCalCore::Event::Ptr event, int defaultReminderStartOffset, KCalCore::ICalFormat &icalFormat, bool *changed)
{
    bool alreadyStarted = *changed; // if this is true, we don't need to call startUpdates/endUpdates() in this function.
    if (!alreadyStarted && gCalETag(event) == json.value(QLatin1String("etag")).toVariant().toString()) {
        SOCIALD_LOG_DEBUG("Ignoring non-remote-changed:" << event->uid() << ","
                          << gCalETag(event) << "==" << json.value(QLatin1String("etag")).toVariant().toString());
        return; // this event has not changed server-side since we last saw it.
    }

    KDateTime start, end;
    bool startExists = false, endExists = false;
    bool startIsDateOnly = false, endIsDateOnly = false;
    bool isAllDay = false;
    extractStartAndEnd(json, &startExists, &endExists, &startIsDateOnly, &endIsDateOnly, &isAllDay, &start, &end);
    if (gCalEventId(event) != json.value(QLatin1String("id")).toVariant().toString()) {
        START_EVENT_UPDATES_IF_REQUIRED(event, changed);
        setGCalEventId(event, json.value(QLatin1String("id")).toVariant().toString());
    }
    if (gCalETag(event) != json.value(QLatin1String("etag")).toVariant().toString()) {
        START_EVENT_UPDATES_IF_REQUIRED(event, changed);
        setGCalETag(event, json.value(QLatin1String("etag")).toVariant().toString());
    }
    extractRecurrence(json.value(QLatin1String("recurrence")).toArray(), event, icalFormat);
    UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, isReadOnly, setReadOnly, json.value(QLatin1String("locked")).toVariant().toBool(), changed)
    UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, summary, setSummary, json.value(QLatin1String("summary")).toVariant().toString(), changed)
    UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, description, setDescription, json.value(QLatin1String("description")).toVariant().toString(), changed)
    UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, location, setLocation, json.value(QLatin1String("location")).toVariant().toString(), changed)
    UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, revision, setRevision, json.value(QLatin1String("revision")).toVariant().toInt(), changed)
    if (startExists) {
        UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, dtStart, setDtStart, start, changed)
    }
    if (endExists) {
        if (!event->hasEndDate() || event->dtEnd() != end) {
            START_EVENT_UPDATES_IF_REQUIRED(event, changed);
            event->setHasEndDate(true);
            event->setDtEnd(end);
        }
    } else {
        UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, hasEndDate, setHasEndDate, false, changed)
    }
    if (isAllDay) {
        UPDATE_EVENT_PROPERTY_IF_REQUIRED(event, allDay, setAllDay, false, changed)
    }
    extractAlarms(json, event, defaultReminderStartOffset, changed);
    END_EVENT_UPDATES_IF_REQUIRED(event, changed, !alreadyStarted);
}

bool remoteModificationIsReal(const QJsonObject &json, KCalCore::Event::Ptr event)
{
    if (gCalEventId(event) != json.value(QLatin1String("id")).toVariant().toString()) {
        return true; // this event is either a partial-upsync-artifact or a new remote addition.
    }
    if (gCalETag(event) != json.value(QLatin1String("etag")).toVariant().toString()) {
        return true; // this event has changed server-side since we last saw it.
    }
    return false; // this event has not changed server-side since we last saw it.
}

bool localModificationIsReal(const QJsonObject &local, const QJsonObject &remote, int defaultReminderStartOffset, KCalCore::ICalFormat &icalFormat)
{
    bool changed = true;
    KCalCore::Event::Ptr localEvent = KCalCore::Event::Ptr(new KCalCore::Event);
    KCalCore::Event::Ptr remoteEvent = KCalCore::Event::Ptr(new KCalCore::Event);
    jsonToKCal(local, localEvent, defaultReminderStartOffset, icalFormat, &changed);
    jsonToKCal(remote, remoteEvent, defaultReminderStartOffset, icalFormat, &changed);
    if (GoogleCalendarIncidenceComparator::eventsEqual(localEvent, remoteEvent, true)) {
        return false; // they're equal, so the local modification is not real.
    }
    return true;
}

// returns true if the last sync was marked as successful, and then marks the current
// sync as being unsuccessful.  The sync adapter should set it to true manually
// once sync succeeds.
bool wasLastSyncSuccessful(int accountId, bool *needCleanSync)
{
    QString settingsFileName = QString::fromLatin1("%1/%2/gcal.ini")
            .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR))
            .arg(QString::fromLatin1(SYNC_DATABASE_DIR));
    if (!QFile::exists(settingsFileName)) {
        SOCIALD_LOG_DEBUG("gcal.ini settings file does not exist, triggering clean sync");
        *needCleanSync = true;
        return false;
    }

    QSettings settingsFile(settingsFileName, QSettings::IniFormat);
    // needCleanSync will be true if and only if an unrecoverable error occurred during the previous sync.
    *needCleanSync = settingsFile.value(QString::fromLatin1("%1-needCleanSync").arg(accountId), QVariant::fromValue<bool>(false)).toBool();
    bool retn = settingsFile.value(QString::fromLatin1("%1-success").arg(accountId), QVariant::fromValue<bool>(false)).toBool();
    settingsFile.setValue(QString::fromLatin1("%1-success").arg(accountId), QVariant::fromValue<bool>(false));
    int pluginVersion = settingsFile.value(QString::fromLatin1("%1-pluginVersion").arg(accountId), QVariant::fromValue<int>(1)).toInt();
    if (pluginVersion != GOOGLE_CAL_SYNC_PLUGIN_VERSION) {
        settingsFile.setValue(QString::fromLatin1("%1-pluginVersion").arg(accountId), GOOGLE_CAL_SYNC_PLUGIN_VERSION);
        SOCIALD_LOG_DEBUG("Google cal sync plugin version mismatch, force clean sync");
        retn = false;
    }
    settingsFile.sync();
    return retn;
}

void setLastSyncSuccessful(QList<int> accountIds)
{
    QString settingsFileName = QString::fromLatin1("%1/%2/gcal.ini")
            .arg(QString::fromLatin1(PRIVILEGED_DATA_DIR))
            .arg(QString::fromLatin1(SYNC_DATABASE_DIR));
    QSettings settingsFile(settingsFileName, QSettings::IniFormat);
    Q_FOREACH(int accountId, accountIds) {
        settingsFile.setValue(QString::fromLatin1("%1-needCleanSync").arg(accountId), QVariant::fromValue<bool>(false));
        settingsFile.setValue(QString::fromLatin1("%1-success").arg(accountId), QVariant::fromValue<bool>(true));
    }
    settingsFile.sync();
}

}

GoogleCalendarSyncAdaptor::GoogleCalendarSyncAdaptor(QObject *parent)
    : GoogleDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Calendars, parent)
    , m_calendar(mKCal::ExtendedCalendar::Ptr(new mKCal::ExtendedCalendar(QLatin1String("UTC"))))
    , m_storage(mKCal::ExtendedCalendar::defaultStorage(m_calendar))
    , m_storageNeedsSave(false)
{
    setInitialActive(true);
}

GoogleCalendarSyncAdaptor::~GoogleCalendarSyncAdaptor()
{
}

QString GoogleCalendarSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("google-calendars");
}

void GoogleCalendarSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    m_storage->open(); // we close it in finalCleanup()
    m_prevSinceTimestamp[accountId] = lastSyncTimestamp(QLatin1String("google"),
                                                        SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Calendars),
                                                        accountId);
    GoogleDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void GoogleCalendarSyncAdaptor::finalCleanup()
{
    if (m_syncSucceeded.size()) {
        // there is only one account per sync run, even though we haven't fully
        // cleaned up the multi-account-isms from the member variables / API.
        int accountId = m_syncSucceeded.keys().first();
        if (m_syncSucceeded[accountId]) {
            applyRemoteChangesLocally(accountId);
            // only update the local last sync timestamp if sync succeeded
            // otherwise, reset it back to the previous last sync timestamp.
            QDateTime newSyncTimestamp = m_syncSucceeded[accountId]
                                       ? m_newSinceTimestamp[accountId]
                                       : m_prevSinceTimestamp[accountId];
            updateLastSyncTimestamp(QLatin1String("google"),
                                    SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Calendars),
                                    accountId,
                                    newSyncTimestamp);
            if (!m_syncSucceeded[accountId]) {
                SOCIALD_LOG_INFO("Error occurred while applying remote changes locally; reset last sync timestamp to:" << newSyncTimestamp);
            } else {
                // also update the remote sync timestamp in each notebook.
                Q_FOREACH (const QString &updatedCalendarId, m_calendarsFinishedRequested.keys()) {
                    // update the sync date for the notebook, to the timestamp reported by Google
                    // in the calendar request for the remote calendar associated with the notebook.
                    QString updateTimestamp = m_calendarsFinishedRequested.value(updatedCalendarId);
                    mKCal::Notebook::Ptr notebook = notebookForCalendarId(accountId, updatedCalendarId);
                    if (!notebook) {
                        // may have been deleted due to a purge operation.
                        continue;
                    }
                    KDateTime syncDate = datetimeFromUpdateStr(updateTimestamp);
                    KDateTime oldSyncDate = notebook->syncDate();
                    if (oldSyncDate < syncDate) {
                        notebook->setSyncDate(syncDate);
                    }
                    m_storage->updateNotebook(notebook);
                    m_storageNeedsSave = true;
                }
            }
        }
    }

    if (m_storageNeedsSave) {
        m_storage->save();
    }
    m_storageNeedsSave = false;

    // set the success status for each of our account settings.
    QList<int> succeededAccounts;
    Q_FOREACH (int accountId, m_syncSucceeded.keys()) {
        if (m_syncSucceeded.value(accountId)) {
            succeededAccounts.append(accountId);
        }
    }
    if (succeededAccounts.size()) {
        setLastSyncSuccessful(succeededAccounts);
    }

    if (!ghostEventCleanupPerformed()) {
        // Delete any events which are not associated with a notebook.
        // These events are ghost events, caused by a bug which previously
        // existed in the sync adapter code due to mkcal deleteNotebook semantics.
        // The mkcal API doesn't allow us to determine which notebook a
        // given incidence belongs to, so we have to instead load
        // everything and then find the ones which are ophaned.
        // Note: we do this separately / after the commit above, because
        // loading all events from the database is expensive.
        SOCIALD_LOG_INFO("performing ghost event cleanup");
        m_storage->load();
        KCalCore::Incidence::List allIncidences;
        m_storage->allIncidences(&allIncidences);
        mKCal::Notebook::List allNotebooks = m_storage->notebooks();
        QSet<QString> notebookIncidenceUids;
        foreach (mKCal::Notebook::Ptr notebook, allNotebooks) {
            KCalCore::Incidence::List currNbIncidences;
            m_storage->allIncidences(&currNbIncidences, notebook->uid());
            foreach (KCalCore::Incidence::Ptr incidence, currNbIncidences) {
                notebookIncidenceUids.insert(incidence->uid());
            }
        }
        int foundOrphans = 0;
        foreach (const KCalCore::Incidence::Ptr incidence, allIncidences) {
            if (!notebookIncidenceUids.contains(incidence->uid())) {
                // orphan/ghost incidence.  must be deleted.
                SOCIALD_LOG_DEBUG("deleting orphan event with uid:" << incidence->uid());
                m_calendar->deleteIncidence(m_calendar->incidence(incidence->uid(), incidence->recurrenceId()));
                foundOrphans++;
            }
        }
        if (foundOrphans == 0) {
            setGhostEventCleanupPerformed();
            SOCIALD_LOG_INFO("orphan cleanup completed without finding orphans!");
        } else if (m_storage->save()) {
            setGhostEventCleanupPerformed();
            SOCIALD_LOG_INFO("orphan cleanup deleted" << foundOrphans << "; storage save completed!");
        } else {
            SOCIALD_LOG_ERROR("orphan cleanup found" << foundOrphans << "; but storage save failed!");
        }
    }

    m_storage->close();
}

void GoogleCalendarSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode)
{
    if (mode == SocialNetworkSyncAdaptor::CleanUpPurge) {
        // need to initialise the database
        m_storage->open(); // we close it in finalCleanup()
    }

    // We clean all the entries in the calendar
    // Delete the notebooks from the storage
    foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        if (notebook->pluginName().startsWith(QString(QLatin1String("google-")))
                && notebook->account() == QString::number(oldId)) {
            // remove the incidences and delete the notebook
            notebook->setIsReadOnly(false);
            m_storage->deleteNotebook(notebook);
            m_storageNeedsSave = true;
        }
    }

    if (mode == SocialNetworkSyncAdaptor::CleanUpPurge) {
        // and commit any changes made.
        finalCleanup();
    }
}

void GoogleCalendarSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    SOCIALD_LOG_DEBUG("beginning Calendar sync for Google, account" << accountId);
    bool needCleanSync = false;
    bool lastSyncSuccessful = wasLastSyncSuccessful(accountId, &needCleanSync);
    if (needCleanSync) {
        SOCIALD_LOG_INFO("performing clean sync");
    } else if (!lastSyncSuccessful) {
        SOCIALD_LOG_INFO("last sync was not successful, attempting to recover without clean sync");
    }
    m_serverCalendarIdToCalendarInfo[accountId].clear();
    m_calendarIdToEventObjects[accountId].clear();
    m_syncSucceeded[accountId] = true; // set to false on error
    requestCalendars(accountId, accessToken, needCleanSync);
}

void GoogleCalendarSyncAdaptor::requestCalendars(int accountId, const QString &accessToken, bool needCleanSync, const QString &pageToken)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("key"), accessToken));
    if (!pageToken.isEmpty()) { // continuation request
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("pageToken"),
                                                  pageToken));
    }

    QUrl url(QLatin1String("https://www.googleapis.com/calendar/v3/users/me/calendarList"));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("GData-Version", "3.0");
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ") + accessToken).toUtf8());

    QNetworkReply *reply = m_networkAccessManager->get(request);

    // we're requesting data.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("needCleanSync", QVariant::fromValue<bool>(needCleanSync));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(calendarsFinishedHandler()));

        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request calendars from Google account with id" << accountId);
        m_syncSucceeded[accountId] = false;
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::calendarsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    bool needCleanSync = reply->property("needCleanSync").toBool();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    // parse the calendars' metadata from the response.
    bool fetchingNextPage = false;
    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok) {
        // first, check to see if there are more pages of calendars to fetch
        if (parsed.find(QLatin1String("nextPageToken")) != parsed.end()
                && !parsed.value(QLatin1String("nextPageToken")).toVariant().toString().isEmpty()) {
            fetchingNextPage = true;
            requestCalendars(accountId, accessToken, needCleanSync,
                             parsed.value(QLatin1String("nextPageToken")).toVariant().toString());
        }

        // second, parse the calendars' metadata
        QJsonArray items = parsed.value(QStringLiteral("items")).toArray();
        for (int i = 0; i < items.count(); ++i) {
            QJsonObject currCalendar = items.at(i).toObject();
            if (!currCalendar.isEmpty() && currCalendar.find(QStringLiteral("id")) != currCalendar.end()) {
                // we only sync calendars which the user owns (ie, not autogenerated calendars)
                QString accessRole = currCalendar.value(QStringLiteral("accessRole")).toString();
                if (accessRole == QStringLiteral("owner")) {
                    GoogleCalendarSyncAdaptor::CalendarInfo currCalendarInfo;
                    currCalendarInfo.color = currCalendar.value(QStringLiteral("backgroundColor")).toString();
                    currCalendarInfo.summary = currCalendar.value(QStringLiteral("summary")).toString();
                    currCalendarInfo.description = currCalendar.value(QStringLiteral("description")).toString();
                    currCalendarInfo.change = NoChange; // we detect the appropriate change type (if required) later.
                    QString currCalendarId = currCalendar.value(QStringLiteral("id")).toString();
                    m_serverCalendarIdToCalendarInfo[accountId].insert(currCalendarId, currCalendarInfo);
                }
            }
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse calendar data from request with account" << accountId <<
                          "; got:" << QString::fromLatin1(replyData.constData()));
        m_syncSucceeded[accountId] = false;
    }

    if (!fetchingNextPage) {
        // we've finished loading all pages of calendar information
        // we now need to process the loaded information to determine
        // which calendars need to be added/updated/removed locally.
        updateLocalCalendarNotebooks(accountId, accessToken, needCleanSync);
    }

    // we're finished with this request.
    decrementSemaphore(accountId);
}


void GoogleCalendarSyncAdaptor::updateLocalCalendarNotebooks(int accountId, const QString &accessToken, bool needCleanSync)
{
    if (syncAborted()) {
        SOCIALD_LOG_DEBUG("sync aborted, skipping updating local calendar notebooks");
        return;
    }

    // any calendars which exist on the device but not the server need to be purged.
    QStringList calendarsToDelete;
    QStringList deviceCalendarIds;
    foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        // notebook pluginName is of form: google-calendarId
        // where the calendarId comes from the server.
        if (notebook->pluginName().startsWith(QStringLiteral("google-"))
                && notebook->account() == QString::number(accountId)) {
            QString currDeviceCalendarId = notebook->pluginName().mid(7);
            if (m_serverCalendarIdToCalendarInfo[accountId].contains(currDeviceCalendarId)) {
                // the server-side calendar exists on the device.
                if (needCleanSync) {
                    // we are performing a clean sync cycle.
                    // we will eventually delete and then insert this notebook.
                    SOCIALD_LOG_DEBUG("queueing clean sync of local calendar" << notebook->name() << currDeviceCalendarId << "for Google account:" << accountId);
                    deviceCalendarIds.append(currDeviceCalendarId);
                    m_serverCalendarIdToCalendarInfo[accountId][currDeviceCalendarId].change = GoogleCalendarSyncAdaptor::CleanSync;
                } else {
                    // we don't need to purge it, but we may need to update its summary/color details.
                    deviceCalendarIds.append(currDeviceCalendarId);
                    if (notebook->name() != m_serverCalendarIdToCalendarInfo[accountId].value(currDeviceCalendarId).summary
                            || notebook->color() != m_serverCalendarIdToCalendarInfo[accountId].value(currDeviceCalendarId).color
                            || notebook->description() != m_serverCalendarIdToCalendarInfo[accountId].value(currDeviceCalendarId).description
                            || notebook->isReadOnly()) {
                        // calendar information changed server-side.
                        SOCIALD_LOG_DEBUG("queueing modification of local calendar" << notebook->name() << currDeviceCalendarId << "for Google account:" << accountId);
                        m_serverCalendarIdToCalendarInfo[accountId][currDeviceCalendarId].change = GoogleCalendarSyncAdaptor::Modify;
                    } else {
                        // the calendar information is unchanged server-side.
                        // no need to change anything locally.
                        SOCIALD_LOG_DEBUG("No modification required for local calendar" << notebook->name() << currDeviceCalendarId << "for Google account:" << accountId);
                        m_serverCalendarIdToCalendarInfo[accountId][currDeviceCalendarId].change = GoogleCalendarSyncAdaptor::NoChange;
                    }
                }
            } else {
                // the calendar has been removed from the server.
                // we need to purge it from the device.
                SOCIALD_LOG_DEBUG("queueing removal of local calendar" << notebook->name() << currDeviceCalendarId << "for Google account:" << accountId);
                calendarsToDelete.append(currDeviceCalendarId);
            }
        }
    }

    // any calendarIds which exist on the server but not the device need to be created.
    foreach (const QString &serverCalendarId, m_serverCalendarIdToCalendarInfo[accountId].keys()) {
        if (!deviceCalendarIds.contains(serverCalendarId)) {
            SOCIALD_LOG_DEBUG("queueing addition of local calendar" << serverCalendarId
                              << m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).summary
                              << "for Google account:" << accountId);
            m_serverCalendarIdToCalendarInfo[accountId][serverCalendarId].change = GoogleCalendarSyncAdaptor::Insert;
        }
    }

    SOCIALD_LOG_DEBUG("Syncing calendar events for Google account: " << accountId << " CleanSync: " << needCleanSync);

    foreach (const QString &calendarId, m_serverCalendarIdToCalendarInfo[accountId].keys()) {
        requestEvents(accountId, accessToken, calendarId, needCleanSync);
        m_calendarsBeingRequested.append(calendarId);
    }

    // now we can queue the calendars which need deletion.
    // note: we have to do it after the previous foreach loop, otherwise we'd attempt to retrieve events for them.
    foreach (const QString &currDeviceCalendarId, calendarsToDelete) {
        m_serverCalendarIdToCalendarInfo[accountId][currDeviceCalendarId].change = GoogleCalendarSyncAdaptor::Delete;
    }
}

void GoogleCalendarSyncAdaptor::requestEvents(int accountId, const QString &accessToken, const QString &calendarId,
                                              bool needCleanSync, const QString &pageToken)
{
    // get the last sync date stored into the notebook (if it exists).
    QString updatedMin;
    KDateTime syncDate;
    mKCal::Notebook::Ptr notebook = notebookForCalendarId(accountId, calendarId);
    if (notebook) {
        syncDate = notebook->syncDate();
    }

    if (!needCleanSync && !syncDate.isNull() && syncDate.isValid()) {
        // we will use an updated-min parameter to reduce the amount of data
        // we request from Google.  Note that we do not want to limit it
        // exactly to the syncDate, since we then would not receive enough
        // remote events to determine correct delta from.  We want at least
        // any modifications which also occurred during the PREVIOUS sync period.
        updatedMin = syncDate.addDays(-7).toString();
        SOCIALD_LOG_DEBUG("Previous update timestamp for Google account:" << accountId <<
                          "Calendar Id:" << calendarId <<
                          "- Timestamp:" << syncDate.toString());
    } else if (needCleanSync) {
        SOCIALD_LOG_DEBUG("Clean sync required for Google account:" << accountId <<
                          "Calendar Id:" << calendarId <<
                          "- Ignoring last sync timestamp:" << syncDate.toString());
    } else {
        SOCIALD_LOG_DEBUG("Invalid previous update timestamp for Google account:" << accountId <<
                          "Calendar Id:" << calendarId <<
                          "- Timestamp:" << syncDate.toString());
    }

    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("key"), accessToken));
    if (!needCleanSync && !updatedMin.isEmpty()) {
        // we're doing a delta update.  We set the "updatedMin" and request deletions be shown.
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("showDeleted"), QString::fromLatin1("true")));
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("updatedMin"), updatedMin));
    }
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("timeMin"),
                                              QDateTime::currentDateTimeUtc().addYears(-1).toString(Qt::ISODate)));
    queryItems.append(QPair<QString, QString>(QString::fromLatin1("timeMax"),
                                              QDateTime::currentDateTimeUtc().addYears(2).toString(Qt::ISODate)));
    if (!pageToken.isEmpty()) { // continuation request
        queryItems.append(QPair<QString, QString>(QString::fromLatin1("pageToken"), pageToken));
    }

    QUrl url(QString::fromLatin1("https://www.googleapis.com/calendar/v3/calendars/%1/events").arg(calendarId));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("GData-Version", "3.0");
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ") + accessToken).toUtf8());

    QNetworkReply *reply = m_networkAccessManager->get(request);

    // we're requesting data.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("calendarId", calendarId);
        reply->setProperty("needCleanSync", needCleanSync);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(eventsFinishedHandler()));

        SOCIALD_LOG_DEBUG("requesting calendar events for Google account:" << accountId << ":" << url.toString());

        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request events for calendar" << calendarId <<
                          "from Google account with id" << accountId);
        m_syncSucceeded[accountId] = false;
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::eventsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString calendarId = reply->property("calendarId").toString();
    QString accessToken = reply->property("accessToken").toString();
    bool needCleanSync = reply->property("needCleanSync").toBool();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    QString replyString = QString::fromUtf8(replyData);
    SOCIALD_LOG_TRACE("-------------------------------");
    SOCIALD_LOG_TRACE("Events response for calendar:" << calendarId << "from account:" << accountId);
    SOCIALD_LOG_TRACE("HTTP CODE:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
    Q_FOREACH (QString line, replyString.split('\n', QString::SkipEmptyParts)) {
        SOCIALD_LOG_TRACE(line.replace('\r', ' '));
    }
    SOCIALD_LOG_TRACE("-------------------------------");

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool fetchingNextPage = false;
    bool ok = false;
    QString updated;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok) {
        // If there are more pages of results to fetch, ensure we fetch them
        if (parsed.find(QLatin1String("nextPageToken")) != parsed.end()
                && !parsed.value(QLatin1String("nextPageToken")).toVariant().toString().isEmpty()) {
            fetchingNextPage = true;
            requestEvents(accountId, accessToken, calendarId, needCleanSync,
                          parsed.value(QLatin1String("nextPageToken")).toVariant().toString());
        }

        updated = parsed.value(QLatin1String("updated")).toVariant().toString();

        // parse the default reminders data to find the default popup reminder start offset.
        if (parsed.find(QStringLiteral("defaultReminders")) != parsed.end()) {
            QJsonArray defaultReminders = parsed.value(QStringLiteral("defaultReminders")).toArray();
            for (int i = 0; i < defaultReminders.size(); ++i) {
                QJsonObject defaultReminder = defaultReminders.at(i).toObject();
                if (defaultReminder.value(QStringLiteral("method")).toString() == QStringLiteral("popup")) {
                    m_serverCalendarIdToDefaultReminderTimes[accountId][calendarId] = defaultReminder.value(QStringLiteral("minutes")).toInt();
                }
            }
        }

        // Parse the event list
        QJsonArray dataList = parsed.value(QLatin1String("items")).toArray();
        foreach (const QJsonValue &item, dataList) {
            QJsonObject eventData = item.toObject();

            // otherwise, we queue the event for insertion into the database.
            m_calendarIdToEventObjects[accountId].insertMulti(calendarId, eventData);
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse event data from request with account" << accountId <<
                          "; got: " << QString::fromUtf8(replyData.constData()));
        m_syncSucceeded[accountId] = false;
    }

    if (!fetchingNextPage) {
        // we've finished loading all pages of event information
        // we now need to process the loaded information to determine
        // which events need to be added/updated/removed locally.
        QDateTime since = needCleanSync ? QDateTime() : m_prevSinceTimestamp[accountId];
        finishedRequestingRemoteEvents(accountId, accessToken, calendarId, since, updated);
        // note that the updated timestamp string will be empty in the error case,
        // however we only use the updated timestamp string if m_syncSucceeded is true.
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}


mKCal::Notebook::Ptr GoogleCalendarSyncAdaptor::notebookForCalendarId(int accountId, const QString &calendarId) const
{
    foreach (mKCal::Notebook::Ptr notebook, m_storage->notebooks()) {
        if (notebook->pluginName() == QString::fromLatin1("google-%1").arg(calendarId)
                && notebook->account() == QString::number(accountId)) {
            return notebook;
        }
    }

    return mKCal::Notebook::Ptr();
}

void GoogleCalendarSyncAdaptor::finishedRequestingRemoteEvents(int accountId, const QString &accessToken,
                                                               const QString &calendarId, const QDateTime &since,
                                                               const QString &updateTimestampStr)
{
    m_calendarsBeingRequested.removeAll(calendarId);
    m_calendarsFinishedRequested.insert(calendarId, updateTimestampStr);
    if (!m_calendarsBeingRequested.isEmpty()) {
        return; // still waiting for more requests to finish.
    }

    if (syncAborted()) {
        return; // sync was aborted before we received all remote data, and before we could upsync local changes.
    }

    // determine local changes to upsync.
    Q_FOREACH (const QString &finishedCalendarId, m_calendarsFinishedRequested.keys()) {
        // now upsync the local changes to the remote server
        QList<UpsyncChange> changesToUpsync = determineSyncDelta(accountId, accessToken, finishedCalendarId, since);
        if (changesToUpsync.size()) {
            if (syncAborted()) {
                SOCIALD_LOG_DEBUG("skipping upsync of queued upsync changes due to sync being aborted");
            } else if (m_syncSucceeded[accountId] == false) {
                SOCIALD_LOG_DEBUG("skipping upsync of queued upsync changes due to previous error during sync");
            } else {
                SOCIALD_LOG_DEBUG("upsyncing" << changesToUpsync.size() << "local changes to the remote server");
                for (int i = 0; i < changesToUpsync.size(); ++i) {
                    upsyncChanges(changesToUpsync[i].accountId,
                                  changesToUpsync[i].accessToken,
                                  changesToUpsync[i].upsyncType,
                                  changesToUpsync[i].kcalEventId,
                                  changesToUpsync[i].recurrenceId,
                                  changesToUpsync[i].calendarId,
                                  changesToUpsync[i].eventId,
                                  changesToUpsync[i].eventData);
                }
            }
        } else {
            // no local changes to upsync.
            // we can apply the remote changes and we are finished.
        }
    }
}

// Determine the sync delta, and then cache the required downsynced changes and return the required changes to upsync.
QList<GoogleCalendarSyncAdaptor::UpsyncChange> GoogleCalendarSyncAdaptor::determineSyncDelta(int accountId, const QString &accessToken,
                                                                                             const QString &calendarId, const QDateTime &since)
{
    Q_UNUSED(accessToken) // in the future, we might need it to download images/data associated with the event.

    QList<UpsyncChange> changesToUpsync;

    // Search for the device Notebook matching this CalendarId.
    // Only upsync changes if we're doing a delta sync, and upsync is enabled.
    bool upsyncEnabled = true;
    mKCal::Notebook::Ptr googleNotebook = notebookForCalendarId(accountId, calendarId);
    if (googleNotebook.isNull()) {
        // this is a new, never before seen calendar.
        SOCIALD_LOG_INFO("No local calendar exists for:" << calendarId <<
                         "for account:" << accountId << ".  No upsync possible.");
        upsyncEnabled = false;
    } else if (!m_accountSyncProfile || m_accountSyncProfile->syncDirection() == Buteo::SyncProfile::SYNC_DIRECTION_FROM_REMOTE) {
        SOCIALD_LOG_INFO("skipping upload of local calendar changes to" << calendarId <<
                         "due to profile direction setting for account" << accountId);
        upsyncEnabled = false;
    } else if (!since.isValid()) {
        SOCIALD_LOG_INFO("Delta upsync with Google calendar" << calendarId <<
                         "for account" << accountId << "not required due to clean sync");
        upsyncEnabled = false;
    } else {
        SOCIALD_LOG_INFO("Delta upsync with Google calendar" << calendarId <<
                         "for account" << accountId << "is enabled.");
        upsyncEnabled = true;
    }


    // re-order the list of remote events so that base recurring events will precede occurrences.
    QList<QJsonObject> eventObjects;
    foreach (const QJsonObject &eventData, m_calendarIdToEventObjects[accountId].values(calendarId)) {
        if (eventData.value(QLatin1String("recurringEventId")).toVariant().toString().isEmpty()) {
            // base event; prepend to list.
            eventObjects.prepend(eventData);
        } else {
            // occurrence; append to list.
            eventObjects.append(eventData);
        }
    }

    // parse that list to look for partial-upsync-artifacts.
    // if we upsynced some local addition, and then lost connectivity,
    // the remote server-side will report that as a remote addition upon
    // the next sync - but really it isn't.
    QHash<QString, QString> upsyncedUidMapping;
    QSet<QString> partialUpsyncArtifactsNeedingUpdate; // set of gcalIds
    foreach (const QJsonObject &eventData, eventObjects) {
        QString eventId = eventData.value(QLatin1String("id")).toVariant().toString();
        QString upsyncedUid = eventData.value(QLatin1String("extendedProperties")).toObject()
                                       .value(QLatin1String("private")).toObject()
                                       .value("x-jolla-sociald-mkcal-uid").toVariant().toString();
        if (!upsyncedUid.isEmpty() && !eventId.isEmpty()) {
            upsyncedUidMapping.insert(upsyncedUid, eventId);
        }
    }

    // load local event changes from the database.
    KCalCore::Incidence::List deletedList, addedList, updatedList, allList;
    QMap<QString, KCalCore::Event::Ptr> allMap, updatedMap;
    QMap<QString, QPair<QString, KDateTime> > deletedMap; // gcalId to incidenceUid,recurrenceId
    QSet<QString> cleanSyncDeletionAdditions; // gcalIds
    if (since.isValid()) {
        // delta sync.  populate our lists.
        SOCIALD_LOG_TRACE("Loading existing data given delta sync method");
        if (googleNotebook.isNull()) {
            SOCIALD_LOG_TRACE("No local notebook exists for remote; no existing data to load.");
        } else {
            m_storage->loadNotebookIncidences(googleNotebook->uid());
            m_storage->allIncidences(&allList, googleNotebook->uid());
            m_storage->deletedIncidences(&deletedList, KDateTime(since), googleNotebook->uid());
            m_storage->insertedIncidences(&addedList, KDateTime(since), googleNotebook->uid());
            m_storage->modifiedIncidences(&updatedList, KDateTime(since), googleNotebook->uid());
            Q_FOREACH(const KCalCore::Incidence::Ptr incidence, allList) {
                KCalCore::Event::Ptr eventPtr = m_calendar->event(incidence->uid(), incidence->recurrenceId());
                QString gcalId = gCalEventId(incidence);
                if (gcalId.isEmpty() && upsyncedUidMapping.contains(incidence->uid())) {
                    // partially upsynced artifact.  It may need to be updated with gcalId comment field.
                    partialUpsyncArtifactsNeedingUpdate.insert(gcalId);
                    gcalId = upsyncedUidMapping.value(incidence->uid());
                }
                if (gcalId.size() && eventPtr) {
                    SOCIALD_LOG_TRACE("Have local event:" << gcalId << "," << eventPtr->uid() << ":" << eventPtr->recurrenceId().toString());
                    allMap.insert(gcalId, eventPtr);
                } // else, newly added locally, no gcalId yet.
            }
            Q_FOREACH(const KCalCore::Incidence::Ptr incidence, updatedList) {
                KCalCore::Event::Ptr eventPtr = m_calendar->event(incidence->uid(), incidence->recurrenceId());
                QString gcalId = gCalEventId(incidence);
                if (gcalId.isEmpty() && upsyncedUidMapping.contains(incidence->uid())) {
                    // TODO: can this codepath be hit?  If it was a partial upsync artifact,
                    //       shouldn't it be reported as a local+remote addition, not local modification?
                    // partially upsynced artifact
                    partialUpsyncArtifactsNeedingUpdate.remove(gcalId); // will already update due to local change.
                    gcalId = upsyncedUidMapping.value(incidence->uid());
                }
                if (gcalId.size() && eventPtr) {
                    SOCIALD_LOG_DEBUG("Have local modification:" << incidence->uid() << "in" << calendarId);
                    updatedMap.insert(gcalId, eventPtr);
                } // else, newly added+updated locally, no gcalId yet.
            }
            Q_FOREACH(const KCalCore::Incidence::Ptr incidence, deletedList) {
                QString gcalId = gCalEventId(incidence);
                if (gcalId.isEmpty() && upsyncedUidMapping.contains(incidence->uid())) {
                    // TODO: can this codepath be hit?  If it was a partial upsync artifact,
                    //       shouldn't it be reported as a local+remote addition, not local deletion?
                    // partially upsynced artifact
                    partialUpsyncArtifactsNeedingUpdate.remove(gcalId); // doesn't need update due to deletion.
                    gcalId = upsyncedUidMapping.value(incidence->uid());
                }
                if (gcalId.size()) {
                    // Now we check to see whether this event was deleted due to a clean-sync (notebook removal).
                    // If so, then another event (with the same gcalId association) should have been ADDED at the
                    // same time, to fulfil clean-sync semantics (because the notebook uid is maintained).
                    // If so, we treat it as a modification rather than delete+add pair.
                    if (allMap.contains(gcalId)) {
                        // note: this works because gcalId is different for base series vs persistent occurrence of series.
                        SOCIALD_LOG_DEBUG("Have local deletion+addition from cleansync:" << gcalId << "in" << calendarId);
                        cleanSyncDeletionAdditions.insert(gcalId);
                    } else {
                        // otherwise, it's a real local deletion.
                        SOCIALD_LOG_DEBUG("Have local deletion:" << incidence->uid() << "in" << calendarId);
                        deletedMap.insert(gcalId, qMakePair(incidence->uid(), incidence->recurrenceId()));
                        updatedMap.remove(gcalId); // don't upsync updates to deleted events.
                    }
                } // else, newly added+deleted locally, no gcalId yet.
            }
        }
    }

    // apply the conflict resolution strategy to remove any local or remote changes which should be dropped.
    int discardedLocalAdditions = 0, discardedLocalModifications = 0, discardedLocalRemovals = 0;
    int remoteAdditions = 0, remoteModifications = 0, remoteRemovals = 0, discardedRemoteModifications = 0, discardedRemoteRemovals = 0;
    QHash<QString, QJsonObject> unchangedRemoteModifications; // gcalId to eventData.

    // For each each of the events downloaded from the server, determine
    // if the remote change invalidates a local change, or if a local
    // deletion invalidates the remote change.
    // Otherwise, cache the remote change for later storage to local db.
    foreach (const QJsonObject &eventData, eventObjects) {
        QString eventId = eventData.value(QLatin1String("id")).toVariant().toString();
        QString parentId = eventData.value(QLatin1String("recurringEventId")).toVariant().toString();
        bool eventWasDeletedRemotely = eventData.value(QLatin1String("status")).toVariant().toString() == QString::fromLatin1("cancelled");
        if (eventWasDeletedRemotely) {
            // if modified locally and deleted on server side, don't upsync modifications
            if (allMap.contains(eventId)) {
                // currently existing base event or persistent occurrence which was deleted server-side
                remoteRemovals++;
                SOCIALD_LOG_DEBUG("Have remote series deletion:" << eventId << "in" << calendarId);
                m_changesFromDownsync.insertMulti(calendarId, qMakePair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject>(GoogleCalendarSyncAdaptor::Delete, eventData));
                if (updatedMap.contains(eventId)) {
                    SOCIALD_LOG_DEBUG("Discarding local event modification:" << eventId << "due to remote deletion");
                    updatedMap.remove(eventId); // discard any local modifications to this event, don't upsync.
                    discardedLocalModifications++;
                }
                // also discard the event from the locally added list if it is reported there.
                // this can happen due to cleansync, or the overlap in the sync date due to mkcal resolution issue.
                for (int i = 0; i < addedList.size(); ++i) {
                    const QString &gcalId(gCalEventId(addedList[i]));
                    if (gcalId == eventId) {
                        SOCIALD_LOG_DEBUG("Discarding local event addition:" << addedList[i]->uid() << "due to remote deletion");
                        addedList.remove(i);
                        discardedLocalAdditions++;
                        break;
                    }
                }
            } else if (!parentId.isEmpty() && allMap.contains(parentId)) {
                // this is a non-persistent occurrence deletion, we need to add an EXDATE to the base event.
                // we treat this as a remote modification of the base event (ie, the EXDATE addition)
                // and thus will discard any local modifications to the base event, and not upsync them.
                // TODO: use a more optimal conflict resolution strategy for this case!
                remoteRemovals++;
                SOCIALD_LOG_DEBUG("Have remote occurrence deletion:" << eventId << "in" << calendarId);
                m_changesFromDownsync.insertMulti(calendarId, qMakePair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject>(GoogleCalendarSyncAdaptor::DeleteOccurrence, eventData));
                if (updatedMap.contains(parentId)) {
                    SOCIALD_LOG_DEBUG("Discarding local modification to recurrence series:" << parentId << "due to remote EXDATE addition. Sub-optimal resolution strategy!");
                    updatedMap.remove(parentId);
                    discardedLocalModifications++;
                }
                // also discard the event from the locally added list if it is reported there.
                // this can happen due to cleansync, or the overlap in the sync date due to mkcal resolution issue.
                for (int i = 0; i < addedList.size(); ++i) {
                    const QString &gcalId(gCalEventId(addedList[i]));
                    if (gcalId == eventId) {
                        SOCIALD_LOG_DEBUG("Discarding local event addition:" << addedList[i]->uid() << "due to remote EXDATE addition.  Sub-optimal resolution strategy!");
                        addedList.remove(i);
                        discardedLocalAdditions++;
                        break;
                    }
                }
            } else {
                if (deletedMap.contains(eventId)) {
                    // remote deleted event was also deleted locally, can ignore.
                    SOCIALD_LOG_DEBUG("Event deleted remotely:" << eventId << "was already deleted locally; discarding both local and remote deletion");
                    deletedMap.remove(eventId); // discard local deletion.
                    discardedLocalRemovals++;
                    discardedRemoteRemovals++;
                } else {
                    // remote deleted event never existed locally.
                    // this can happen due to the increased updatedMin window
                    // extending to prior to the account existing on the device.
                    SOCIALD_LOG_DEBUG("Event deleted remotely:" << eventId << "was never downsynced to device; discarding");
                    discardedRemoteRemovals++;
                }
            }
        } else if (deletedMap.contains(eventId)) {
            // remote change will be discarded due to local deletion.
            SOCIALD_LOG_DEBUG("Discarding remote event modification:" << eventId << "due to local deletion");
            discardedRemoteModifications++;
        } else if (allMap.contains(eventId)) {
            // remote modification of an existing event.
            KCalCore::Event::Ptr event = allMap.value(eventId);
            bool changed = false;
            if (partialUpsyncArtifactsNeedingUpdate.contains(eventId)) {
                // This event was partially upsynced and then connectivity died before we committed
                // and updated its comment field with the gcalId it was given by the remote server.
                // During this sync cycle, we will update it by assuming remote modification.
                // Note: this will lose any local changes made since it was partially-upsynced,
                // however the alternative is to lose remote changes made since then...
                // So we stick with our "prefer-remote" conflict resolution strategy here.
                SOCIALD_LOG_DEBUG("Reloading partial upsync artifact:" << eventId << "from server as a modification");
                changed = true;
            } else {
                changed = remoteModificationIsReal(eventData, event);
            }
            if (!changed) {
                // Not a real change.  We discard this remote modification,
                // but we track it so that we can detect spurious local modifications.
                SOCIALD_LOG_DEBUG("Discarding remote event modification:" << eventId << "in" << calendarId << "as spurious");
                unchangedRemoteModifications.insert(eventId, eventData);
                discardedRemoteModifications++;
            } else {
                SOCIALD_LOG_DEBUG("Have remote modification:" << eventId << "in" << calendarId);
                remoteModifications++;
                m_changesFromDownsync.insertMulti(calendarId, qMakePair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject>(GoogleCalendarSyncAdaptor::Modify, eventData));
                if (updatedMap.contains(eventId)) {
                    // if both local and server were modified, prefer server.
                    SOCIALD_LOG_DEBUG("Discarding local event modification:" << eventId << "due to remote modification");
                    updatedMap.remove(eventId);
                    discardedLocalModifications++;
                }
                // also discard the event from the locally added list if it is reported there.
                // this can happen due to cleansync, or the overlap in the sync date due to mkcal resolution issue.
                for (int i = 0; i < addedList.size(); ++i) {
                    const QString &gcalId(gCalEventId(addedList[i]));
                    if (gcalId == eventId) {
                        SOCIALD_LOG_DEBUG("Discarding local event addition:" << addedList[i]->uid() << "due to remote modification");
                        addedList.remove(i);
                        discardedLocalAdditions++;
                        break;
                    }
                }
            }
        } else {
            // pure remote addition. remote additions cannot invalidate local changes.
            // note that we have already detected (and dealt with) partial-upsync-artifacts
            // which would have been reported from the remote server as additions.
            SOCIALD_LOG_DEBUG("Have remote addition:" << eventId << "in" << calendarId);
            remoteAdditions++;
            m_changesFromDownsync.insertMulti(calendarId, qMakePair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject>(GoogleCalendarSyncAdaptor::Insert, eventData));
        }
    }

    SOCIALD_LOG_INFO("Delta downsync from Google calendar" << calendarId << "for account" << accountId << ":" <<
                     "remote A/M/R: " << remoteAdditions << "/" << remoteModifications << "/" << remoteRemovals <<
                     "after discarding M/R:" << discardedRemoteModifications << "/" << discardedRemoteRemovals <<
                     "due to local deletions or identical data");

    if (upsyncEnabled) {
        // Now build the local-changes-to-upsync data structures.
        int localAdded = 0, localModified = 0, localRemoved = 0;

        // first, queue up deletions.
        Q_FOREACH (const QString &deletedGcalId, deletedMap.keys()) {
            QString incidenceUid = deletedMap.value(deletedGcalId).first;
            KDateTime recurrenceId = deletedMap.value(deletedGcalId).second;
            localRemoved++;
            SOCIALD_LOG_TRACE("queueing upsync deletion for gcal id:" << deletedGcalId);
            UpsyncChange deletion;
            deletion.accountId = accountId;
            deletion.accessToken = accessToken;
            deletion.upsyncType = GoogleCalendarSyncAdaptor::Delete;
            deletion.kcalEventId = incidenceUid;
            deletion.recurrenceId = recurrenceId;
            deletion.calendarId = calendarId;
            deletion.eventId = deletedGcalId;
            changesToUpsync.append(deletion);
        }

        // second, queue up modifications.
        Q_FOREACH (const QString &updatedGcalId, updatedMap.keys()) {
            KCalCore::Event::Ptr event = updatedMap.value(updatedGcalId);
            if (event) {
                QJsonObject localEventData = kCalToJson(event, m_icalFormat);
                if (unchangedRemoteModifications.contains(updatedGcalId)
                        && !localModificationIsReal(localEventData, unchangedRemoteModifications.value(updatedGcalId), m_serverCalendarIdToDefaultReminderTimes[accountId].value(calendarId), m_icalFormat)) {
                    // this local modification is spurious.  It may have been reported
                    // due to the timestamp resolution issue, but in any case the
                    // event does not differ from the remote one.
                    SOCIALD_LOG_DEBUG("Discarding local event modification:" << event->uid() << event->recurrenceId().toString() << "as spurious, for gcalId:" << updatedGcalId);
                    discardedLocalModifications++;
                    continue;
                }
                localModified++;
                QByteArray eventBlob = QJsonDocument(localEventData).toJson();
                SOCIALD_LOG_TRACE("queueing upsync modification for gcal id:" << updatedGcalId);
                traceDumpStr(QString::fromUtf8(eventBlob));
                UpsyncChange modification;
                modification.accountId = accountId;
                modification.accessToken = accessToken;
                modification.upsyncType = GoogleCalendarSyncAdaptor::Modify;
                modification.kcalEventId = event->uid();
                modification.recurrenceId = event->recurrenceId();
                modification.calendarId = calendarId;
                modification.eventId = updatedGcalId;
                modification.eventData = eventBlob;
                changesToUpsync.append(modification);
            }
        }

        // finally, queue up insertions.
        Q_FOREACH (KCalCore::Incidence::Ptr incidence, addedList) {
            KCalCore::Event::Ptr event = m_calendar->event(incidence->uid(), incidence->recurrenceId());
            if (event) {
                if (upsyncedUidMapping.contains(incidence->uid())) {
                    const QString &eventId(upsyncedUidMapping.value(incidence->uid()));
                    if (partialUpsyncArtifactsNeedingUpdate.contains(eventId)) {
                        // We have already handled this one, by treating it as a remote modification, above.
                        SOCIALD_LOG_DEBUG("Discarding partial upsync artifact local addition:" << eventId);
                        discardedLocalAdditions++;
                        continue;
                    }
                }
                QString gcalId = gCalEventId(event);
                if (!gcalId.isEmpty()) {
                    if (cleanSyncDeletionAdditions.contains(gcalId)) {
                        // this event was deleted+re-added due to clean sync.  treat it as a local modification
                        // of the remote event.  Note: we cannot update the extended UID property in the remote
                        // event, because multiple other devices may depend on it.  When we downsynced the event
                        // for the re-add, we should have re-used the old uid.
                        SOCIALD_LOG_DEBUG("Converting local addition to modification due to clean-sync semantics");
                    } else {
                        // this event was previously downsynced from the remote in the last sync cycle.
                        // we treat it as a local modification (as it may have changed locally since).
                        // TODO: detect whether any actual change has occurred since it was downsynced.  How?
                        SOCIALD_LOG_DEBUG("Converting local addition to modification due to it being a previously downsynced event");
                    }
                    // convert the local event to a JSON object.
                    QJsonObject localEventData = kCalToJson(event, m_icalFormat);
                    // check to see if this differs from some discarded remote modification.
                    // if it does not, then the remote and local are identical, and it's only
                    // being reported as a local addition/modification due to the "since" timestamp
                    // overlap.
                    if (unchangedRemoteModifications.contains(gcalId)
                            && !localModificationIsReal(localEventData, unchangedRemoteModifications.value(gcalId), m_serverCalendarIdToDefaultReminderTimes[accountId].value(calendarId), m_icalFormat)) {
                        // this local addition is spurious.  It may have been reported
                        // due to the timestamp resolution issue, but in any case the
                        // event does not differ from the remote one which is already updated.
                        SOCIALD_LOG_DEBUG("Discarding local event modification:" << event->uid() << event->recurrenceId().toString() << "as spurious, for gcalId:" << gcalId);
                        discardedLocalModifications++;
                        continue;
                    }
                    localModified++;
                    QByteArray eventBlob = QJsonDocument(localEventData).toJson();
                    SOCIALD_LOG_TRACE("queueing upsync modification for gcal id:" << gcalId);
                    traceDumpStr(QString::fromUtf8(eventBlob));
                    UpsyncChange modification;
                    modification.accountId = accountId;
                    modification.accessToken = accessToken;
                    modification.upsyncType = GoogleCalendarSyncAdaptor::Modify;
                    modification.kcalEventId = event->uid();
                    modification.recurrenceId = event->recurrenceId();
                    modification.calendarId = calendarId;
                    modification.eventId = gcalId;
                    modification.eventData = eventBlob;
                    changesToUpsync.append(modification);
                } else {
                    localAdded++;
                    QByteArray eventBlob = QJsonDocument(kCalToJson(event, m_icalFormat, true)).toJson(); // true = insert extended UID property
                    SOCIALD_LOG_TRACE("queueing up insertion for local id:" << incidence->uid());
                    traceDumpStr(QString::fromUtf8(eventBlob));
                    UpsyncChange insertion;
                    insertion.accountId = accountId;
                    insertion.accessToken = accessToken;
                    insertion.upsyncType = GoogleCalendarSyncAdaptor::Insert;
                    insertion.kcalEventId = event->uid();
                    insertion.recurrenceId = event->recurrenceId();
                    insertion.calendarId = calendarId;
                    insertion.eventId = QString();
                    insertion.eventData = eventBlob;
                    changesToUpsync.append(insertion);
                }
            }
        }

        SOCIALD_LOG_INFO("Delta upsync with Google calendar" << calendarId << "for account" << accountId << ":" <<
                         "local A/M/R:" << localAdded << "/" << localModified << "/" << localRemoved <<
                         "after discarding A/M/R:" << discardedLocalAdditions << "/" << discardedLocalModifications << "/" << discardedLocalRemovals <<
                         "due to remote changes or identical data");
    }

    return changesToUpsync;
}

void GoogleCalendarSyncAdaptor::upsyncChanges(int accountId, const QString &accessToken,
                                              GoogleCalendarSyncAdaptor::ChangeType upsyncType,
                                              const QString &kcalEventId, const KDateTime &recurrenceId,
                                              const QString &calendarId, const QString &eventId,
                                              const QByteArray &eventData)
{
    QUrl requestUrl = upsyncType == GoogleCalendarSyncAdaptor::Insert
                    ? QUrl(QString::fromLatin1("https://www.googleapis.com/calendar/v3/calendars/%1/events").arg(calendarId))
                    : QUrl(QString::fromLatin1("https://www.googleapis.com/calendar/v3/calendars/%1/events/%2").arg(calendarId).arg(eventId));

    QNetworkRequest request(requestUrl);
    request.setRawHeader("GData-Version", "3.0");
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ") + accessToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QVariant::fromValue<QString>(QString::fromLatin1("application/json")));

    QNetworkReply *reply = 0;

    QString upsyncTypeStr;
    switch (upsyncType) {
        case GoogleCalendarSyncAdaptor::Insert:
            upsyncTypeStr = QString::fromLatin1("Insert");
            reply = m_networkAccessManager->post(request, eventData);
            break;
        case GoogleCalendarSyncAdaptor::Modify:
            upsyncTypeStr = QString::fromLatin1("Modify");
            reply = m_networkAccessManager->put(request, eventData);
            break;
        case GoogleCalendarSyncAdaptor::Delete:
            upsyncTypeStr = QString::fromLatin1("Delete");
            reply = m_networkAccessManager->deleteResource(request);
            break;
        default:
            SOCIALD_LOG_ERROR("UNREACHBLE - upsyncing non-change"); // always an error.
            m_syncSucceeded[accountId] = false;
            return;
    }

    // we're performing a request.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("kcalEventId", kcalEventId);
        reply->setProperty("recurrenceId", recurrenceId.toString());
        reply->setProperty("calendarId", calendarId);
        reply->setProperty("eventId", eventId);
        reply->setProperty("upsyncType", static_cast<int>(upsyncType));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(upsyncFinishedHandler()));

        setupReplyTimeout(accountId, reply);

        SOCIALD_LOG_DEBUG("upsyncing change:" << upsyncTypeStr <<
                          "to calendarId:" << calendarId <<
                          "of account" << accountId << "to" <<
                          request.url().toString());
        traceDumpStr(QString::fromUtf8(eventData));
    } else {
        SOCIALD_LOG_ERROR("unable to request upsync for calendar" << calendarId <<
                          "from Google account with id" << accountId);
        m_syncSucceeded[accountId] = false;
        decrementSemaphore(accountId);
    }
}

void GoogleCalendarSyncAdaptor::upsyncFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString kcalEventId = reply->property("kcalEventId").toString();
    KDateTime recurrenceId = KDateTime::fromString(reply->property("recurrenceId").toString());
    QString calendarId = reply->property("calendarId").toString();
    int upsyncType = reply->property("upsyncType").toInt();
    QByteArray replyData = reply->readAll();
    bool isError = reply->property("isError").toBool();

    // QNetworkReply can report an error even if there isn't one...
    if (isError && reply->error() == QNetworkReply::UnknownContentError
            && upsyncType == GoogleCalendarSyncAdaptor::Delete) {
        isError = false; // not a real error; Google returns an empty response.
    }

    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    // parse the calendars' metadata from the response.
    if (isError) {
        // error occurred during request.
        SOCIALD_LOG_ERROR("error occurred while upsyncing calendar data to Google account" << accountId <<
                          "; got:" << QString::fromLatin1(replyData.constData()));
        m_syncSucceeded[accountId] = false;
    } else if (upsyncType == GoogleCalendarSyncAdaptor::Delete) {
        // we expect an empty response body on success for Delete operations
        if (!replyData.isEmpty()) {
            SOCIALD_LOG_ERROR("error occurred while upsyncing calendar event deletion to Google account" << accountId << "; got:");
            errorDumpStr(QString::fromLatin1(replyData.constData()));
            m_syncSucceeded[accountId] = false;
        }
    } else {
        // we expect an event resource body on success for Insert/Modify requests.
        bool ok = false;
        QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
        if (!ok) {
            QString typeStr = upsyncType == GoogleCalendarSyncAdaptor::Insert
                            ? QString::fromLatin1("insertion")
                            : QString::fromLatin1("modification");
            SOCIALD_LOG_ERROR("error occurred while upsyncing calendar event" << typeStr <<
                              "to Google account" << accountId << "; got:");
            errorDumpStr(QString::fromLatin1(replyData.constData()));
            m_syncSucceeded[accountId] = false;
        } else {
            // TODO: reduce code duplication between here and the other function.
            // Search for the device Notebook matching this CalendarId
            mKCal::Notebook::Ptr googleNotebook = notebookForCalendarId(accountId, calendarId);
            if (googleNotebook.isNull()) {
                SOCIALD_LOG_ERROR("calendar" << calendarId << "doesn't have a notebook for Google account with id" << accountId);
                m_syncSucceeded[accountId] = false;
            } else {
                // cache the update to this event in the local calendar
                m_storage->loadNotebookIncidences(googleNotebook->uid());
                KCalCore::Event::Ptr event = m_calendar->event(kcalEventId, recurrenceId);
                if (!event) {
                    SOCIALD_LOG_ERROR("event" << kcalEventId << recurrenceId.toString() << "was deleted locally during sync of Google account with id" << accountId);
                    m_syncSucceeded[accountId] = false;
                } else {
                    SOCIALD_LOG_TRACE("Local upsync response json:");
                    traceDumpStr(QString::fromUtf8(replyData));
                    m_changesFromUpsync.insertMulti(calendarId, qMakePair<KCalCore::Event::Ptr,QJsonObject>(event, parsed));
                }
            }
        }
    }

    // we're finished with this request.
    decrementSemaphore(accountId);
}

void GoogleCalendarSyncAdaptor::applyRemoteChangesLocally(int accountId)
{
    SOCIALD_LOG_DEBUG("applying all remote changes to local database");
    foreach (const QString &serverCalendarId, m_serverCalendarIdToCalendarInfo[accountId].keys()) {
        switch (m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).change) {
            case GoogleCalendarSyncAdaptor::NoChange: {
                // No changes required.  Note that this just applies to the notebook metadata;
                // there may be incidences belonging to this notebook which need modification.
                SOCIALD_LOG_DEBUG("No metadata changes required for local notebook for server calendar:" << serverCalendarId);
            } break;
            case GoogleCalendarSyncAdaptor::Insert: {
                SOCIALD_LOG_DEBUG("Adding local notebook for new server calendar:" << serverCalendarId);
                mKCal::Notebook::Ptr notebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
                notebook->setIsReadOnly(false);
                notebook->setName(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).summary);
                notebook->setColor(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).color);
                notebook->setDescription(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).description);
                notebook->setPluginName(QStringLiteral("google-") + serverCalendarId);
                notebook->setAccount(QString::number(accountId));
                m_storage->addNotebook(notebook);
                m_storageNeedsSave = true;
            } break;
            case GoogleCalendarSyncAdaptor::Modify: {
                SOCIALD_LOG_DEBUG("Modifications required for local notebook for server calendar:" << serverCalendarId);
                mKCal::Notebook::Ptr notebook = notebookForCalendarId(accountId, serverCalendarId);
                if (notebook.isNull()) {
                    SOCIALD_LOG_ERROR("unable to modify non-existent calendar:" << serverCalendarId << "for account:" << accountId);
                    m_syncSucceeded[accountId] = false; // we don't return immediately, as we want to at least attempt to
                                                        // apply other database modifications if possible, in order to leave
                                                        // the local database in a usable state even after failed sync.
                } else {
                    notebook->setIsReadOnly(false);
                    notebook->setName(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).summary);
                    notebook->setColor(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).color);
                    notebook->setDescription(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).description);
                    m_storage->updateNotebook(notebook);
                    m_storageNeedsSave = true;
                }
            } break;
            case GoogleCalendarSyncAdaptor::Delete: {
                SOCIALD_LOG_DEBUG("Deleting local notebook for deleted server calendar:" << serverCalendarId);
                mKCal::Notebook::Ptr notebook = notebookForCalendarId(accountId, serverCalendarId);
                if (notebook.isNull()) {
                    SOCIALD_LOG_ERROR("unable to delete non-existent calendar:" << serverCalendarId << "for account:" << accountId);
                    // m_syncSucceeded[accountId] = false; // don't mark as failed, since the outcome is identical.
                } else {
                    notebook->setIsReadOnly(false);
                    m_storage->deleteNotebook(notebook);
                    m_storageNeedsSave = true;
                }
            } break;
            case GoogleCalendarSyncAdaptor::DeleteOccurrence: {
                // this codepath should never be hit.
                SOCIALD_LOG_ERROR("invalid DeleteOccurrence change reported for calendar:" << serverCalendarId << "from account:" << accountId);
            } break;
            case GoogleCalendarSyncAdaptor::CleanSync: {
                SOCIALD_LOG_DEBUG("Deleting and recreating local notebook for clean-sync server calendar:" << serverCalendarId);
                QString notebookUid; // reuse the old notebook Uid after recreating it due to clean sync.
                // delete
                mKCal::Notebook::Ptr notebook = notebookForCalendarId(accountId, serverCalendarId);
                if (!notebook.isNull()) {
                    SOCIALD_LOG_DEBUG("deleting notebook:" << notebook->uid() << "due to clean sync");
                    notebookUid = notebook->uid();
                    notebook->setIsReadOnly(false);
                    m_storage->deleteNotebook(notebook);
                } else {
                    SOCIALD_LOG_DEBUG("could not find local notebook corresponding to server calendar:" << serverCalendarId);
                }
                // and then recreate.
                SOCIALD_LOG_DEBUG("recreating notebook:" << notebook->uid() << "due to clean sync");
                notebook = mKCal::Notebook::Ptr(new mKCal::Notebook);
                notebook->setIsReadOnly(false);
                if (!notebookUid.isEmpty()) {
                    notebook->setUid(notebookUid);
                }
                notebook->setName(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).summary);
                notebook->setColor(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).color);
                notebook->setDescription(m_serverCalendarIdToCalendarInfo[accountId].value(serverCalendarId).description);
                notebook->setPluginName(QStringLiteral("google-") + serverCalendarId);
                notebook->setAccount(QString::number(accountId));
                m_storage->addNotebook(notebook);
                m_storageNeedsSave = true;
            } break;
        }
    }

    SOCIALD_LOG_DEBUG("finished updating local notebooks, about to apply remote event delta locally");
    QStringList calendarsNeedingLocalChanges = m_changesFromDownsync.keys() + m_changesFromUpsync.keys();
    calendarsNeedingLocalChanges.removeDuplicates();
    Q_FOREACH (const QString &updatedCalendarId, calendarsNeedingLocalChanges) {
        // save any required changes to the local database
        updateLocalCalendarNotebookEvents(accountId, updatedCalendarId);
        m_storageNeedsSave = true;
    }
}

void GoogleCalendarSyncAdaptor::updateLocalCalendarNotebookEvents(int accountId, const QString &calendarId)
{
    QList<QPair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject> > changesFromDownsyncForCalendar = m_changesFromDownsync.values(calendarId);
    QList<QPair<KCalCore::Event::Ptr, QJsonObject> > changesFromUpsyncForCalendar = m_changesFromUpsync.values(calendarId);
    if (changesFromDownsyncForCalendar.isEmpty() && changesFromUpsyncForCalendar.isEmpty()) {
        SOCIALD_LOG_DEBUG("No remote changes to apply for calendar:" << calendarId << "for Google account:" << accountId);
        return; // no remote changes to apply.
    }

    // Set notebook writeable locally.
    mKCal::Notebook::Ptr googleNotebook = notebookForCalendarId(accountId, calendarId);
    if (!googleNotebook) {
        SOCIALD_LOG_ERROR("no local notebook associated with calendar:" << calendarId << "from account:" << accountId << "to update!");
        m_syncSucceeded[accountId] = false;
        return;
    }

    // write changes required to complete downsync to local database
    googleNotebook->setIsReadOnly(false);
    if (!changesFromDownsyncForCalendar.isEmpty()) {
        // build the partial-upsync-artifact mapping for this set of changes.
        QHash<QString, QString> upsyncedUidMapping;
        for (int i = 0; i < changesFromDownsyncForCalendar.size(); ++i) {
            const QPair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject> &remoteChange(changesFromDownsyncForCalendar[i]);
            QString gcalId = remoteChange.second.value(QLatin1String("id")).toVariant().toString();
            QString upsyncedUid = remoteChange.second.value(QLatin1String("extendedProperties")).toObject()
                                                     .value(QLatin1String("private")).toObject()
                                                     .value("x-jolla-sociald-mkcal-uid").toVariant().toString();
            if (!upsyncedUid.isEmpty() && !gcalId.isEmpty()) {
                upsyncedUidMapping.insert(upsyncedUid, gcalId);
            }
        }

        // build up map of gcalIds to local events for this change set
        QMap<QString, KCalCore::Event::Ptr> allLocalEventsMap;
        KCalCore::Incidence::List allLocalEventsList;
        m_storage->loadNotebookIncidences(googleNotebook->uid());
        m_storage->allIncidences(&allLocalEventsList, googleNotebook->uid());
        Q_FOREACH(const KCalCore::Incidence::Ptr incidence, allLocalEventsList) {
            KCalCore::Event::Ptr eventPtr = m_calendar->event(incidence->uid(), incidence->recurrenceId());
            QString gcalId = gCalEventId(incidence);
            if (gcalId.isEmpty()) {
                gcalId = upsyncedUidMapping.value(incidence->uid());
            }
            if (gcalId.size() && eventPtr) {
                allLocalEventsMap.insert(gcalId, eventPtr);
            }
        }

        // re-order remote changes so that additions of recurring series happen before additions of exception occurrences.
        // otherwise, the parent event may not exist when we attempt to insert the exception.
        // similarly, re-order remote deletions of exceptions so that they occur before remote deletions of series.
        QList<QPair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject> > reorderedChangesFromDownsyncForCalendar;
        for (int i = 0; i < changesFromDownsyncForCalendar.size(); ++i) {
            const QPair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject> &remoteChange(changesFromDownsyncForCalendar[i]);
            QString parentId = remoteChange.second.value(QLatin1String("recurringEventId")).toVariant().toString();
            if (parentId.isEmpty()) {
                reorderedChangesFromDownsyncForCalendar.append(remoteChange);
            }
        }
        for (int i = 0; i < changesFromDownsyncForCalendar.size(); ++i) {
            const QPair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject> &remoteChange(changesFromDownsyncForCalendar[i]);
            QString parentId = remoteChange.second.value(QLatin1String("recurringEventId")).toVariant().toString();
            if (!parentId.isEmpty()) {
                if (remoteChange.first == GoogleCalendarSyncAdaptor::Delete || remoteChange.first == GoogleCalendarSyncAdaptor::DeleteOccurrence) {
                    reorderedChangesFromDownsyncForCalendar.prepend(remoteChange);
                } else {
                    reorderedChangesFromDownsyncForCalendar.append(remoteChange);
                }
            }
        }

        // apply the remote changes locally.
        for (int i = 0; i < reorderedChangesFromDownsyncForCalendar.size(); ++i) {
            const QPair<GoogleCalendarSyncAdaptor::ChangeType, QJsonObject> &remoteChange(reorderedChangesFromDownsyncForCalendar[i]);
            const QJsonObject eventData(remoteChange.second);
            const QString eventId = eventData.value(QLatin1String("id")).toVariant().toString();
            QString parentId = eventData.value(QLatin1String("recurringEventId")).toVariant().toString();
            KDateTime recurrenceId = parseRecurrenceId(eventData.value("originalStartTime").toObject());
            switch (remoteChange.first) {
                case GoogleCalendarSyncAdaptor::Delete: {
                    // currently existing base event or persistent occurrence which needs deletion
                    SOCIALD_LOG_DEBUG("Event deleted remotely:" << eventId);
                    m_calendar->deleteEvent(allLocalEventsMap.value(eventId));
                } break;
                case GoogleCalendarSyncAdaptor::DeleteOccurrence: {
                    // this is a non-persistent occurrence, we need to add an EXDATE to the base event.
                    SOCIALD_LOG_DEBUG("Occurrence deleted remotely:" << eventId << "for recurrenceId:" << recurrenceId.toString());
                    KCalCore::Event::Ptr event = allLocalEventsMap.value(parentId);
                    event->startUpdates();
                    event->recurrence()->addExDateTime(recurrenceId);
                    event->endUpdates();
                } break;
                case GoogleCalendarSyncAdaptor::Modify: {
                    SOCIALD_LOG_DEBUG("Event modified remotely:" << eventId);
                    KCalCore::Event::Ptr event = allLocalEventsMap.value(eventId);
                    bool changed = false; // modification, not insert, so initially changed = "false".
                    jsonToKCal(eventData, event, m_serverCalendarIdToDefaultReminderTimes[accountId].value(calendarId), m_icalFormat, &changed);
                } break;
                case GoogleCalendarSyncAdaptor::Insert: {
                    // add a new local event for the remote addition.
                    KCalCore::Event::Ptr event;
                    if (recurrenceId.isValid()) {
                        // this is a persistent occurrence for an already-existing series.
                        SOCIALD_LOG_DEBUG("Persistent occurrence added remotely:" << eventId);
                        KCalCore::Event::Ptr parentEvent = allLocalEventsMap.value(parentId);
                        if (parentEvent.isNull()) {
                            // it might have been newly added in this sync cycle.  Look for it from the calendar.
                            QString parentEventUid = m_recurringEventIdToKCalUid.value(accountId).value(parentId);
                            parentEvent = parentEventUid.isEmpty() ? parentEvent : m_calendar->event(parentEventUid, KDateTime());
                            if (parentEvent.isNull()) {
                                SOCIALD_LOG_ERROR("Cannot find parent event:" << parentId << "for persistent occurrence:" << eventId);
                                m_syncSucceeded[accountId] = false;
                                continue; // we don't return, but instead attempt to finish other event modifications
                            }
                        }

                        // dissociate the persistent occurrence
                        event = m_calendar->dissociateSingleOccurrence(parentEvent, recurrenceId, recurrenceId.timeSpec()).staticCast<KCalCore::Event>();
                        if (event.isNull()) {
                            SOCIALD_LOG_ERROR("Could not dissociate occurrence from recurring event:" << parentId << recurrenceId.toString());
                            m_syncSucceeded[accountId] = false;
                            continue; // we don't return, but instead attempt to finish other event modifications
                        }
                    } else {
                        // this is a new event in its own right.
                        SOCIALD_LOG_DEBUG("Event added remotely:" << eventId);
                        event = KCalCore::Event::Ptr(new KCalCore::Event);
                        // check to see if another Jolla device uploaded this event.
                        // if so, we want to use the same local UID it did.
                        QString localUid = eventData.value(QLatin1String("extendedProperties")).toObject()
                                                    .value(QLatin1String("private")).toObject()
                                                    .value("x-jolla-sociald-mkcal-uid").toVariant().toString();
                        if (localUid.size()) {
                            SOCIALD_LOG_DEBUG("Event" << eventId << "was synced by another Jolla device, reusing local uid:" << localUid);
                            event->setUid(localUid);
                        }
                    }
                    bool changed = true; // set to true as it's an addition, no need to check for delta.
                    jsonToKCal(eventData, event, m_serverCalendarIdToDefaultReminderTimes[accountId].value(calendarId), m_icalFormat, &changed); // direct conversion
                    if (!m_calendar->addEvent(event, googleNotebook->uid())) {
                        SOCIALD_LOG_ERROR("Could not add dissociated occurrence to calendar:" << parentId << recurrenceId.toString());
                        m_syncSucceeded[accountId] = false;
                        continue; // we don't return, but instead attempt to finish other event modifications
                    }
                    m_recurringEventIdToKCalUid[accountId].insert(eventId, event->uid());
                } break;
                default: break;
            }
        }
    }

    // write changes required to complete upsync to the local database
    for (int i = 0; i <changesFromUpsyncForCalendar.size(); ++i) {
        const QPair<KCalCore::Event::Ptr, QJsonObject> &remoteChange(changesFromUpsyncForCalendar[i]);
        KCalCore::Event::Ptr event(remoteChange.first);
        const QJsonObject eventData(remoteChange.second);
        // all changes are modifications to existing events, since it was an upsync response.
        bool changed = false;
        jsonToKCal(eventData, event, m_serverCalendarIdToDefaultReminderTimes[accountId].value(calendarId), m_icalFormat, &changed);
        if (changed) {
            SOCIALD_LOG_DEBUG("Two-way calendar sync with account" << accountId << ": re-updating event:" << event->summary());
            m_storageNeedsSave = true;
        }
    }

    // this becomes our new sync anchor.  In theory there could be lost updates because this timestamp will be greater
    // than the point at which we requested local changes; but the alternative is cache the timestamp at the point
    // just before we request local changes, and in that case, on the next sync we would get local changes (including additions)
    // reported for every remote change which was applied above...
    m_newSinceTimestamp[accountId] = QDateTime::currentDateTimeUtc(); // next sync should get all local changes made after this point in time.
}
