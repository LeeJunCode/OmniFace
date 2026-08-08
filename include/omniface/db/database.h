#pragma once

#include <QSqlDatabase>
#include <QString>

namespace omniface {
namespace db {

class Database {
public:
    explicit Database(const QString& db_path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    QSqlDatabase& Get();
    bool IsOpen() const;

private:
    void CreateSchema();
    void CreateIndexes();
    void RunMigrations();

    QSqlDatabase db_;
    QString connection_name_;
    bool is_open_ = false;
};

}  // namespace db
}  // namespace omniface
