#include "omniface/db/video_repo.h"

#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "omniface/db/snowflake.h"

namespace omniface {
namespace db {

namespace {

std::string EscapeSql(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '\'') r += "''";
        else r += c;
    }
    return r;
}

}  // namespace

VideoRepo::VideoRepo(QSqlDatabase& db) : db_(db) {}

int64_t VideoRepo::Insert(const VideoRecord& v) {
    int64_t id = v.id != 0 ? v.id : Snowflake::Instance().NextId();
    QString created = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "INSERT INTO videos (id, name, description, notes, duration, "
        "resolution_width, resolution_height, frame_rate, record_time, "
        "record_location, file_size, saved_path, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(static_cast<qlonglong>(id));
    q.addBindValue(QString::fromStdString(v.name));
    q.addBindValue(QString::fromStdString(v.description));
    q.addBindValue(QString::fromStdString(v.notes));
    q.addBindValue(v.duration);
    q.addBindValue(v.resolution_width);
    q.addBindValue(v.resolution_height);
    q.addBindValue(v.frame_rate);
    q.addBindValue(QString::fromStdString(v.record_time));
    q.addBindValue(QString::fromStdString(v.record_location));
    q.addBindValue(static_cast<qlonglong>(v.file_size));
    q.addBindValue(QString::fromStdString(v.saved_path));
    q.addBindValue(created);

    if (!q.exec()) {
        throw std::runtime_error("VideoRepo::Insert failed: " + q.lastError().text().toStdString());
    }
    return id;
}

std::optional<VideoRecord> VideoRepo::FindById(int64_t id) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT * FROM videos WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(id));
    if (!q.exec() || !q.next()) return {};
    return RowToRecord(q);
}

std::vector<VideoRecord> VideoRepo::FindAll(const std::optional<VideoFilter>& filter,
                                              const std::optional<VideoSort>& sort) {
    QString sql = QStringLiteral("SELECT * FROM videos");
    if (filter) {
        auto where = BuildWhereClause(*filter);
        if (!where.empty()) sql += QStringLiteral(" WHERE ") + QString::fromStdString(where);
    }
    if (sort) {
        sql += QStringLiteral(" ORDER BY ") + QString::fromStdString(sort->column);
        sql += sort->ascending ? QStringLiteral(" ASC") : QStringLiteral(" DESC");
    }

    QSqlQuery q(db_);
    if (!q.exec(sql)) {
        spdlog::error("VideoRepo::FindAll query failed: {}", q.lastError().text().toStdString());
        return {};
    }

    std::vector<VideoRecord> results;
    while (q.next()) results.push_back(RowToRecord(q));
    return results;
}

bool VideoRepo::Update(int64_t id, const VideoRecord& v) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "UPDATE videos SET name=?, description=?, notes=?, duration=?, "
        "resolution_width=?, resolution_height=?, frame_rate=?, record_time=?, "
        "record_location=?, file_size=?, saved_path=? WHERE id=?"));
    q.addBindValue(QString::fromStdString(v.name));
    q.addBindValue(QString::fromStdString(v.description));
    q.addBindValue(QString::fromStdString(v.notes));
    q.addBindValue(v.duration);
    q.addBindValue(v.resolution_width);
    q.addBindValue(v.resolution_height);
    q.addBindValue(v.frame_rate);
    q.addBindValue(QString::fromStdString(v.record_time));
    q.addBindValue(QString::fromStdString(v.record_location));
    q.addBindValue(static_cast<qlonglong>(v.file_size));
    q.addBindValue(QString::fromStdString(v.saved_path));
    q.addBindValue(static_cast<qlonglong>(id));
    return q.exec();
}

bool VideoRepo::Delete(int64_t id) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM videos WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(id));
    return q.exec() && q.numRowsAffected() > 0;
}

std::vector<VideoRecord> VideoRepo::SearchByName(const std::string& query) {
    VideoFilter f;
    f.name_search = query;
    return FindAll(f, {});
}

std::optional<VideoRecord> VideoRepo::FindBySavedPath(const std::string& saved_path) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT * FROM videos WHERE saved_path = ?"));
    q.addBindValue(QString::fromStdString(saved_path));
    if (!q.exec() || !q.next()) return {};
    return RowToRecord(q);
}

VideoRecord VideoRepo::RowToRecord(const QSqlQuery& q) {
    VideoRecord v;
    v.id                = q.value(0).toLongLong();
    v.name              = q.value(1).toString().toStdString();
    v.description       = q.value(2).toString().toStdString();
    v.notes             = q.value(3).toString().toStdString();
    v.duration          = q.value(4).toDouble();
    v.resolution_width  = q.value(5).toInt();
    v.resolution_height = q.value(6).toInt();
    v.frame_rate        = q.value(7).toDouble();
    v.record_time       = q.value(8).toString().toStdString();
    v.record_location   = q.value(9).toString().toStdString();
    v.file_size         = q.value(10).toLongLong();
    v.saved_path        = q.value(11).toString().toStdString();
    v.created_at        = q.value(12).toString().toStdString();
    return v;
}

std::string VideoRepo::BuildWhereClause(const VideoFilter& f) const {
    std::string where;
    if (f.name_search && !f.name_search->empty()) {
        where += "name LIKE '%" + EscapeSql(*f.name_search) + "%' ESCAPE '\\'";
    }
    if (f.record_time_from) {
        if (!where.empty()) where += " AND ";
        where += "record_time >= '" + EscapeSql(*f.record_time_from) + "'";
    }
    if (f.record_time_to) {
        if (!where.empty()) where += " AND ";
        where += "record_time <= '" + EscapeSql(*f.record_time_to) + "'";
    }
    return where;
}

}  // namespace db
}  // namespace omniface
