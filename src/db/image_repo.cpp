#include "omniface/db/image_repo.h"

#include <cstring>

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

namespace {

QByteArray SerializeFloats(const float* data, size_t count) {
    return QByteArray(reinterpret_cast<const char*>(data),
                      static_cast<int>(count * sizeof(float)));
}

void DeserializeFloats(const QByteArray& blob, float* out, size_t count) {
    size_t bytes = std::min(static_cast<size_t>(blob.size()), count * sizeof(float));
    std::memcpy(out, blob.constData(), bytes);
}

}  // namespace

ImageRepo::ImageRepo(QSqlDatabase& db) : db_(db) {}

int64_t ImageRepo::Insert(const ImageRecord& img) {
    int64_t id = img.id != 0 ? img.id : Snowflake::Instance().NextId();
    QString created = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "INSERT INTO images (id, name, description, notes, saved_path, "
        "face_confidence, bbox_x1, bbox_y1, bbox_x2, bbox_y2, "
        "keypoints, embedding, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(static_cast<qlonglong>(id));
    q.addBindValue(QString::fromStdString(img.name));
    q.addBindValue(QString::fromStdString(img.description));
    q.addBindValue(QString::fromStdString(img.notes));
    q.addBindValue(QString::fromStdString(img.saved_path));
    q.addBindValue(img.face_confidence);
    q.addBindValue(img.bbox_x1);
    q.addBindValue(img.bbox_y1);
    q.addBindValue(img.bbox_x2);
    q.addBindValue(img.bbox_y2);
    q.addBindValue(SerializeFloats(img.keypoints.data(), img.keypoints.size()));
    q.addBindValue(SerializeFloats(img.embedding.data(), img.embedding.size()));
    q.addBindValue(created);

    if (!q.exec()) {
        throw std::runtime_error("ImageRepo::Insert failed: " + q.lastError().text().toStdString());
    }
    return id;
}

std::optional<ImageRecord> ImageRepo::FindById(int64_t id) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT * FROM images WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(id));
    if (!q.exec() || !q.next()) return {};
    return RowToRecord(q);
}

std::vector<ImageRecord> ImageRepo::FindAll(const std::optional<ImageFilter>& filter,
                                              const std::optional<ImageSort>& sort) {
    QString sql = QStringLiteral("SELECT * FROM images");
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
        spdlog::error("ImageRepo::FindAll query failed: {}", q.lastError().text().toStdString());
        return {};
    }

    std::vector<ImageRecord> results;
    while (q.next()) results.push_back(RowToRecord(q));
    return results;
}

bool ImageRepo::Update(int64_t id, const ImageRecord& img) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "UPDATE images SET name=?, description=?, notes=?, saved_path=?, "
        "face_confidence=?, bbox_x1=?, bbox_y1=?, bbox_x2=?, bbox_y2=?, "
        "keypoints=?, embedding=? WHERE id=?"));
    q.addBindValue(QString::fromStdString(img.name));
    q.addBindValue(QString::fromStdString(img.description));
    q.addBindValue(QString::fromStdString(img.notes));
    q.addBindValue(QString::fromStdString(img.saved_path));
    q.addBindValue(img.face_confidence);
    q.addBindValue(img.bbox_x1);
    q.addBindValue(img.bbox_y1);
    q.addBindValue(img.bbox_x2);
    q.addBindValue(img.bbox_y2);
    q.addBindValue(SerializeFloats(img.keypoints.data(), img.keypoints.size()));
    q.addBindValue(SerializeFloats(img.embedding.data(), img.embedding.size()));
    q.addBindValue(static_cast<qlonglong>(id));
    return q.exec();
}

bool ImageRepo::Delete(int64_t id) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM images WHERE id = ?"));
    q.addBindValue(static_cast<qlonglong>(id));
    return q.exec() && q.numRowsAffected() > 0;
}

std::vector<ImageRecord> ImageRepo::SearchByName(const std::string& query) {
    ImageFilter f;
    f.name_search = query;
    return FindAll(f, {});
}

std::optional<ImageRecord> ImageRepo::FindBySavedPath(const std::string& saved_path) {
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT * FROM images WHERE saved_path = ?"));
    q.addBindValue(QString::fromStdString(saved_path));
    if (!q.exec() || !q.next()) return {};
    return RowToRecord(q);
}

ImageRecord ImageRepo::RowToRecord(const QSqlQuery& q) {
    ImageRecord img;
    img.id              = q.value(0).toLongLong();
    img.name            = q.value(1).toString().toStdString();
    img.description     = q.value(2).toString().toStdString();
    img.notes           = q.value(3).toString().toStdString();
    img.saved_path      = q.value(4).toString().toStdString();
    img.face_confidence = static_cast<float>(q.value(5).toDouble());
    img.bbox_x1         = static_cast<float>(q.value(6).toDouble());
    img.bbox_y1         = static_cast<float>(q.value(7).toDouble());
    img.bbox_x2         = static_cast<float>(q.value(8).toDouble());
    img.bbox_y2         = static_cast<float>(q.value(9).toDouble());
    DeserializeFloats(q.value(10).toByteArray(), img.keypoints.data(), img.keypoints.size());
    DeserializeFloats(q.value(11).toByteArray(), img.embedding.data(), img.embedding.size());
    img.created_at      = q.value(12).toString().toStdString();
    return img;
}

std::string ImageRepo::BuildWhereClause(const ImageFilter& f) const {
    std::string where;
    if (f.name_search && !f.name_search->empty()) {
        where += "name LIKE '%" + EscapeSql(*f.name_search) + "%' ESCAPE '\\'";
    }
    if (f.min_confidence && *f.min_confidence > 0.0f) {
        if (!where.empty()) where += " AND ";
        where += "face_confidence >= " + std::to_string(*f.min_confidence);
    }
    return where;
}

}  // namespace db
}  // namespace omniface
