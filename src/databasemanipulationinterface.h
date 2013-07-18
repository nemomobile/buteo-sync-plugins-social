/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef DATABASEMANIPULATIONINTERFACE_H
#define DATABASEMANIPULATIONINTERFACE_H

#include <QtSql/QSqlDatabase>

class DatabaseManipulationInterface
{
public:
    virtual ~DatabaseManipulationInterface();
protected:
    bool initDatabase(const QString &serviceName, const QString &dataType, const QString &baseDir,
                      const QString &dbFile, int userVersion);
    virtual bool dbCreateTables() = 0;
    virtual bool dbDropTables() = 0;
    bool createPragmaVersion(const QString &serviceName, const QString &dataType, int version);
    QSqlDatabase db;
private:
    int dbUserVersion(const QString &serviceName, const QString &dataType) const;
};

#endif // DATABASEMANIPULATIONINTERFACE_H
