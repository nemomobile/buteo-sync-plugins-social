/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Lucien Xu <lucien.xu@jollamobile.com>
 **
 ****************************************************************************/

#ifndef INTERNALDATABASEMANIPULATIONINTERFACE_H
#define INTERNALDATABASEMANIPULATIONINTERFACE_H

#include <QtSql/QSqlDatabase>

class InternalDatabaseManipulationInterface
{
public:
    virtual ~InternalDatabaseManipulationInterface();
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

#endif // INTERNALDATABASEMANIPULATIONINTERFACE_H
