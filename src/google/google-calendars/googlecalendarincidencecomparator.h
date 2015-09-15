/****************************************************************************
 **
 ** Copyright (C) 2015 Jolla Ltd.
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

#ifndef GOOGLECALENDARINCIDENCECOMPARATOR_H
#define GOOGLECALENDARINCIDENCECOMPARATOR_H

#include <QtDebug>

#include <memorycalendar.h>
#include <extendedcalendar.h>
#include <extendedstorage.h>
#include <icalformat.h>
#include <incidence.h>
#include <event.h>
#include <todo.h>
#include <journal.h>
#include <attendee.h>
#include <kdatetime.h>

#include "trace.h"

#define SOCIALD_LOG_DEBUG_MAYBE(msg) if (printDebug) SOCIALD_LOG_DEBUG(msg)

#define GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, func, desc) {\
    if (a->func != b->func) {\
        SOCIALD_LOG_DEBUG_MAYBE("Incidence" << desc << "" << "properties are not equal:" << a->func << b->func); \
        return false;\
    }\
}

#define GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(failureCheck, desc, debug) {\
    if (failureCheck) {\
        SOCIALD_LOG_DEBUG_MAYBE("Incidence" << desc << "properties are not equal:" << desc << debug); \
        return false;\
    }\
}

namespace GoogleCalendarIncidenceComparator {
    void normalizePersonEmail(KCalCore::Person *p)
    {
        QString email = p->email().replace(QStringLiteral("mailto:"), QString(), Qt::CaseInsensitive);
        if (email != p->email()) {
            p->setEmail(email);
        }
    }

    template <typename T>
    bool pointerDataEqual(const QVector<QSharedPointer<T> > &vectorA, const QVector<QSharedPointer<T> > &vectorB)
    {
        if (vectorA.count() != vectorB.count()) {
            return false;
        }
        for (int i=0; i<vectorA.count(); i++) {
            if (vectorA[i].data() != vectorB[i].data()) {
                return false;
            }
        }
        return true;
    }

    bool eventsEqual(const KCalCore::Event::Ptr &a, const KCalCore::Event::Ptr &b, bool printDebug)
    {
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dateEnd() != b->dateEnd(), "dateEnd", (a->dateEnd().toString() + " != " + b->dateEnd().toString()));
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, transparency(), "transparency");

        // some special handling for dtEnd() depending on whether it's an all-day event or not.
        if (a->allDay() && b->allDay()) {
            GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtEnd().date() != b->dtEnd().date(), "dtEnd", (a->dtEnd().toString() + " != " + b->dtEnd().toString()));
        } else {
            GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtEnd() != b->dtEnd(), "dtEnd", (a->dtEnd().toString() + " != " + b->dtEnd().toString()));
        }

        // some special handling for isMultiday() depending on whether it's an all-day event or not.
        if (a->allDay() && b->allDay()) {
            // here we assume that both events are in "export form" (that is, exclusive DTEND)
            if (a->dtEnd().date() != b->dtEnd().date()) {
                SOCIALD_LOG_DEBUG_MAYBE("have a->dtStart()" << a->dtStart().toString() << ", a->dtEnd()" << a->dtEnd().toString());
                SOCIALD_LOG_DEBUG_MAYBE("have b->dtStart()" << b->dtStart().toString() << ", b->dtEnd()" << b->dtEnd().toString());
                SOCIALD_LOG_DEBUG_MAYBE("have a->isMultiDay()" << a->isMultiDay() << ", b->isMultiDay()" << b->isMultiDay());
                return false;
            }
        } else {
            GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, isMultiDay(), "multiday");
        }

        // Don't compare hasEndDate() as Event(Event*) does not initialize it based on the validity of
        // dtEnd(), so it could be false when dtEnd() is valid. The dtEnd comparison above is sufficient.

        return true;
    }

    bool todosEqual(const KCalCore::Todo::Ptr &a, const KCalCore::Todo::Ptr &b, bool printDebug)
    {
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, hasCompletedDate(), "hasCompletedDate");
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtRecurrence() != b->dtRecurrence(), "dtRecurrence", (a->dtRecurrence().toString() + " != " + b->dtRecurrence().toString()));
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, hasDueDate(), "hasDueDate");
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtDue() != b->dtDue(), "dtDue", (a->dtDue().toString() + " != " + b->dtDue().toString()));
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, hasStartDate(), "hasStartDate");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, isCompleted(), "isCompleted");
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->completed() != b->completed(), "completed", (a->completed().toString() + " != " + b->completed().toString()));
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, isOpenEnded(), "isOpenEnded");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, percentComplete(), "percentComplete");
        return true;
    }

    bool journalsEqual(const KCalCore::Journal::Ptr &, const KCalCore::Journal::Ptr &, bool)
    {
        // no journal-specific properties; it only uses the base incidence properties
        return true;
    }

    // Checks whether a specific set of properties are equal.
    bool copiedPropertiesAreEqual(const KCalCore::Incidence::Ptr &a, const KCalCore::Incidence::Ptr &b, bool printDebug)
    {
        if (!a || !b) {
            qWarning() << "Invalid paramters! a:" << a << "b:" << b;
            return false;
        }

        // Do not compare created() or lastModified() because we don't update these fields when
        // an incidence is updated by copyIncidenceProperties(), so they are guaranteed to be unequal.
        // TODO compare deref alarms and attachment lists to compare them also.
        // Don't compare resources() for now because KCalCore may insert QStringList("") as the resources
        // when in fact it should be QStringList(), which causes the comparison to fail.
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, type(), "type");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, duration(), "duration");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, hasDuration(), "hasDuration");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, isReadOnly(), "isReadOnly");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, comments(), "comments");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, contacts(), "contacts");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, altDescription(), "altDescription");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, categories(), "categories");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, customStatus(), "customStatus");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, description(), "description");
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(!qFuzzyCompare(a->geoLatitude(), b->geoLatitude()), "geoLatitude", (QString("%1 != %2").arg(a->geoLatitude()).arg(b->geoLatitude())));
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(!qFuzzyCompare(a->geoLongitude(), b->geoLongitude()), "geoLongitude", (QString("%1 != %2").arg(a->geoLongitude()).arg(b->geoLongitude())));
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, hasGeo(), "hasGeo");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, location(), "location");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, secrecy(), "secrecy");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, status(), "status");
        GIC_RETURN_FALSE_IF_NOT_EQUAL(a, b, summary(), "summary");

        // check recurrence information. Note that we only need to check the recurrence rules for equality if they both recur.
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->recurs() != b->recurs(), "recurs", a->recurs() + " != " + b->recurs());
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->recurs() && *(a->recurrence()) != *(b->recurrence()), "recurrence", "...");

        // some special handling for dtStart() depending on whether it's an all-day event or not.
        if (a->allDay() && b->allDay()) {
            GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtStart().date() != b->dtStart().date(), "dtStart", (a->dtStart().toString() + " != " + b->dtStart().toString()));
        } else {
            GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(a->dtStart() != b->dtStart(), "dtStart", (a->dtStart().toString() + " != " + b->dtStart().toString()));
        }

        // Some servers insert a mailto: in the organizer email address, so ignore this when comparing organizers
        KCalCore::Person personA(*a->organizer().data());
        KCalCore::Person personB(*b->organizer().data());
        normalizePersonEmail(&personA);
        normalizePersonEmail(&personB);
        GIC_RETURN_FALSE_IF_NOT_EQUAL_CUSTOM(personA != personB, "organizer", (personA.fullName() + " != " + personB.fullName()));

        switch (a->type()) {
        case KCalCore::IncidenceBase::TypeEvent:
            if (!eventsEqual(a.staticCast<KCalCore::Event>(), b.staticCast<KCalCore::Event>(), printDebug)) {
                return false;
            }
            break;
        case KCalCore::IncidenceBase::TypeTodo:
            if (!todosEqual(a.staticCast<KCalCore::Todo>(), b.staticCast<KCalCore::Todo>(), printDebug)) {
                return false;
            }
            break;
        case KCalCore::IncidenceBase::TypeJournal:
            if (!journalsEqual(a.staticCast<KCalCore::Journal>(), b.staticCast<KCalCore::Journal>(), printDebug)) {
                return false;
            }
            break;
        case KCalCore::IncidenceBase::TypeFreeBusy:
        case KCalCore::IncidenceBase::TypeUnknown:
            SOCIALD_LOG_DEBUG_MAYBE("Unable to compare FreeBusy or Unknown incidence, assuming equal");
            break;
        }
        return true;
    }
}

#endif // GOOGLECALENDARINCIDENCECOMPARATOR_H
