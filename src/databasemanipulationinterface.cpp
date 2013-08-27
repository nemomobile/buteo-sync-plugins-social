/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Lucien Xu <lucien.xu@jollamobile.com>
 **
 ****************************************************************************/

#include "databasemanipulationinterface.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include "trace.h"
#include "syncservice.h"

DatabaseManipulationInterface::~DatabaseManipulationInterface()
{
}

bool DatabaseManipulationInterface::initDatabase(const QString &serviceName,
                                                 const QString &dataType, const QString &baseDir,
                                                 const QString &dbFile, int userVersion)
{
    if (!QFile::exists(QString(QLatin1String("%1/%2/%3")).arg(baseDir, dataType, dbFile))) {
        QDir dir(QString(QLatin1String("%1/%2")).arg(baseDir, dataType));
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        QString absolutePath = dir.absoluteFilePath(dbFile);
        QFile dbfile(absolutePath);
        if (!dbfile.open(QIODevice::ReadWrite)) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: unable to create database %1 - "\
                                        "%2 %3 sync will be inactive")).arg(dbFile, serviceName,
                                                                            dataType));
            return false;
        }
        dbfile.close();
    }

    QString connectionName
            = QString(QLatin1String("sociald/%1/%2")).arg(serviceName, dataType);

    // open the database in which we store our synced image information
    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(QString("%1/%2/%3").arg(baseDir, dataType, dbFile));
    if (!db.open()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to open database %1 - "\
                                    "%2 %3 sync will be inactive")).arg(dbFile, serviceName,
                                                                        dataType));
        return false;
    }

    if (dbUserVersion(serviceName, dataType) < userVersion) {
        // DB needs to be recreated
        if (!dbDropTables()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: failed to update database %1 ! "\
                                        "Remove it manually and restart sociald")).arg(dbFile));
            db.close();
            return false;
        }
    }

    if (!dbCreateTables()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: failed to create tables for database %1 ! "\
                                  "Remove it manually and restart sociald")).arg(dbFile));
        db.close();
        return false;
    }

    return true;
}

bool DatabaseManipulationInterface::createPragmaVersion(const QString &serviceName,
                                                        const QString &dataType, int version)
{
    QSqlQuery query (db);
    query.prepare(QString(QLatin1String("PRAGMA user_version=%1")).arg(version));
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to create pragma user_version: - "\
                                  "%1 %2 sync will be inactive. Error: %3"))
            .arg(serviceName, dataType, query.lastError().text()));
        return false;
    }
    return true;
}

int DatabaseManipulationInterface::dbUserVersion(const QString &serviceName,
                                                 const QString &dataType) const
{
    const QString queryStr = QString("PRAGMA user_version");

    QSqlQuery query(db);
    if (!query.exec(queryStr)) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to query db version - "\
                                    "%1 %2 sync will be inactive. Error: %3"))
              .arg(serviceName, dataType, query.lastError().text()));
        return -1;
    }
    QSqlRecord record = query.record();
    if (query.isActive() && query.isSelect()) {
        query.first();
        QString value = query.value(record.indexOf("user_version")).toString();
        if (value.isEmpty()) {
            return -1;
        }
        return value.toInt();
    }
    return -1;
}
