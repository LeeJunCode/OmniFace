#include "omniface/db/task_repo.h"

#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QDir>
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

TaskRepo::TaskRepo(QSqlDatabase& db) : db_(db) {}

int64_t TaskRepo::Insert(const TaskRecord& t) {
    int64_t id = t.id != 0 ? t.id : Snowflake::Instance().NextId();
    QString start_time = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "INSERT INTO tasks (id, video_id, image_id, name, description, notes, "
        "analysis_backend, params_snapshot, start_time, end_time, "
        "total_frames, avg_fps, output_dir, output_video_path, csv_path, "
        "output_types, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(static_cast<qlonglong>(id));
    q.addBindValue(static_cast<qlonglong>(t.video_id));
    q.addBindValue(static_cast<qlonglong>(t.image_id));
    q.addBindValue(QString::fromStdString(t.name));
    q.addBindValue(QString::fromStdString(t.description));
    q.addBindValue(QString::fromStdString(t.notes));
    q.addBindValue(QString::fromStdString(t.backend));
    q.addBindValue(QByteArray::fromStdString(t.params_yaml));
    q.addBindValue(start_time);
    q.addBindValue(t.end_time.empty() ? QVariant() : QVariant(QString::fromStdString(t.end_time)));
    q.addBindValue(t.total_frames);
    q.addBindValue(t.avg_fps);
    q.addBindValue(QString::fromStdString(t.output_dir));
    q.addBindValue(t.output_video_path.empty() ? QVariant() : QVariant(QString::fromStdString(t.output_video_path)));
    q.addBindValue(t.csv_path.empty() ? QVariant() : QVariant(QString::fromStdString(t.csv_path)));
    q.addBindValue(QString::fromStdString(t.output_types));
    q.addBindValue(QString::fromStdString(t.status));

    if (!q.exec()) {
        throw std::runtime_error("TaskRepo::Insert failed: " + q.lastError().text().toStdString());
    }
    return id;
}

std::optional<TaskRecord> TaskRepo::FindById(int64_t id) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT * FROM tasks WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(id));
    if (!q.exec() || !q.next()) return {};
    return RowToRecord(q);
}

std::vector<TaskRecord> TaskRepo::FindAll(const std::optional<TaskFilter>& filter,
                                            const std::optional<TaskSort>& sort) {
    QString sql = QStringLiteral("SELECT * FROM tasks");
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
        spdlog::error("TaskRepo::FindAll query failed: {}", q.lastError().text().toStdString());
        return {};
    }

    std::vector<TaskRecord> results;
    while (q.next()) results.push_back(RowToRecord(q));
    return results;
}

bool TaskRepo::Update(int64_t id, const TaskRecord& t) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "UPDATE tasks SET video_id=?, image_id=?, name=?, description=?, notes=?, "
        "analysis_backend=?, params_snapshot=?, start_time=?, end_time=?, "
        "total_frames=?, avg_fps=?, output_dir=?, output_video_path=?, "
        "csv_path=?, output_types=?, status=? WHERE id=?"));
    q.addBindValue(static_cast<qlonglong>(t.video_id));
    q.addBindValue(static_cast<qlonglong>(t.image_id));
    q.addBindValue(QString::fromStdString(t.name));
    q.addBindValue(QString::fromStdString(t.description));
    q.addBindValue(QString::fromStdString(t.notes));
    q.addBindValue(QString::fromStdString(t.backend));
    q.addBindValue(QByteArray::fromStdString(t.params_yaml));
    q.addBindValue(QString::fromStdString(t.start_time));
    q.addBindValue(t.end_time.empty() ? QVariant() : QVariant(QString::fromStdString(t.end_time)));
    q.addBindValue(t.total_frames);
    q.addBindValue(t.avg_fps);
    q.addBindValue(QString::fromStdString(t.output_dir));
    q.addBindValue(t.output_video_path.empty() ? QVariant() : QVariant(QString::fromStdString(t.output_video_path)));
    q.addBindValue(t.csv_path.empty() ? QVariant() : QVariant(QString::fromStdString(t.csv_path)));
    q.addBindValue(QString::fromStdString(t.output_types));
    q.addBindValue(QString::fromStdString(t.status));
    q.addBindValue(static_cast<qlonglong>(id));
    return q.exec();
}

bool TaskRepo::Delete(int64_t id, bool delete_output_files) {
    if (delete_output_files) {
        auto task = FindById(id);
        if (task && !task->output_dir.empty()) {
            QDir dir(QString::fromStdString(task->output_dir));
            if (dir.exists()) dir.removeRecursively();
        }
    }

    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM tasks WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(id));
    return q.exec() && q.numRowsAffected() > 0;
}

TaskRecord TaskRepo::RowToRecord(const QSqlQuery& q) {
    TaskRecord t;
    t.id          = q.value(0).toLongLong();
    t.video_id    = q.value(1).toLongLong();
    t.image_id    = q.value(2).toLongLong();
    t.name        = q.value(3).toString().toStdString();
    t.description = q.value(4).toString().toStdString();
    t.notes       = q.value(5).toString().toStdString();
    t.backend     = q.value(6).toString().toStdString();
    t.params_yaml = q.value(7).toByteArray().toStdString();
    t.start_time  = q.value(8).toString().toStdString();
    t.end_time    = q.value(9).toString().toStdString();
    t.total_frames = q.value(10).toInt();
    t.avg_fps     = q.value(11).toDouble();
    t.output_dir  = q.value(12).toString().toStdString();
    t.output_video_path = q.value(13).toString().toStdString();
    t.csv_path    = q.value(14).toString().toStdString();
    t.output_types = q.value(15).toString().toStdString();
    t.status      = q.value(16).toString().toStdString();
    return t;
}

std::string TaskRepo::BuildWhereClause(const TaskFilter& f) const {
    std::string where;
    if (f.backend && !f.backend->empty()) {
        where += "analysis_backend = '" + EscapeSql(*f.backend) + "'";
    }
    if (f.status && !f.status->empty()) {
        if (!where.empty()) where += " AND ";
        where += "status = '" + EscapeSql(*f.status) + "'";
    }
    if (f.start_time_from) {
        if (!where.empty()) where += " AND ";
        where += "start_time >= '" + EscapeSql(*f.start_time_from) + "'";
    }
    if (f.start_time_to) {
        if (!where.empty()) where += " AND ";
        where += "start_time <= '" + EscapeSql(*f.start_time_to) + "'";
    }
    return where;
}

}  // namespace db
}  // namespace omniface
