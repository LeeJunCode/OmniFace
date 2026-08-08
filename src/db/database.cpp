#include "omniface/db/database.h"

#include <stdexcept>

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <spdlog/spdlog.h>

namespace omniface {
namespace db {

Database::Database(const QString& db_path) {
    connection_name_ = QStringLiteral("omniface_") + QUuid::createUuid().toString(QUuid::Id128);
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_name_);
    db_.setDatabaseName(db_path);
    if (!db_.open()) {
        throw std::runtime_error("Failed to open database: " + db_.lastError().text().toStdString());
    }

    {
        QSqlQuery q(db_);
        q.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    }

    CreateSchema();
    CreateIndexes();
    RunMigrations();
    is_open_ = true;
}

Database::~Database() {
    if (db_.isOpen()) db_.close();
    QSqlDatabase::removeDatabase(connection_name_);
}

QSqlDatabase& Database::Get() { return db_; }

bool Database::IsOpen() const { return is_open_; }

void Database::CreateSchema() {
    QSqlQuery q(db_);

    const auto exec = [&](const QString& sql, const char* table_name) {
        if (!q.exec(sql)) {
            throw std::runtime_error(
                std::string("Failed to create table ") + table_name + ": " + q.lastError().text().toStdString());
        }
    };

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS videos ("
        "  id INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  description TEXT,"
        "  notes TEXT,"
        "  duration REAL NOT NULL,"
        "  resolution_width INTEGER NOT NULL,"
        "  resolution_height INTEGER NOT NULL,"
        "  frame_rate REAL NOT NULL,"
        "  record_time TEXT,"
        "  record_location TEXT,"
        "  file_size INTEGER NOT NULL,"
        "  saved_path TEXT NOT NULL UNIQUE,"
        "  created_at TEXT NOT NULL"
        ")"), "videos");

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS images ("
        "  id INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  description TEXT,"
        "  notes TEXT,"
        "  saved_path TEXT NOT NULL UNIQUE,"
        "  face_confidence REAL NOT NULL,"
        "  bbox_x1 REAL NOT NULL,"
        "  bbox_y1 REAL NOT NULL,"
        "  bbox_x2 REAL NOT NULL,"
        "  bbox_y2 REAL NOT NULL,"
        "  keypoints BLOB NOT NULL,"
        "  embedding BLOB NOT NULL,"
        "  created_at TEXT NOT NULL"
        ")"), "images");

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS tasks ("
        "  id INTEGER PRIMARY KEY,"
        "  video_id INTEGER NOT NULL,"
        "  image_id INTEGER NOT NULL,"
        "  name TEXT,"
        "  description TEXT,"
        "  notes TEXT,"
        "  analysis_backend TEXT NOT NULL,"
        "  params_snapshot BLOB NOT NULL,"
        "  start_time TEXT NOT NULL,"
        "  end_time TEXT,"
        "  total_frames INTEGER NOT NULL DEFAULT 0,"
        "  avg_fps REAL,"
        "  output_dir TEXT NOT NULL,"
        "  output_video_path TEXT,"
        "  csv_path TEXT,"
        "  output_types TEXT NOT NULL,"
        "  status TEXT NOT NULL DEFAULT 'running',"
        "  FOREIGN KEY(video_id) REFERENCES videos(id) ON DELETE RESTRICT,"
        "  FOREIGN KEY(image_id) REFERENCES images(id) ON DELETE RESTRICT"
        ")"), "tasks");
}

void Database::CreateIndexes() {
    QSqlQuery q(db_);

    const char* indexes[] = {
        "CREATE INDEX IF NOT EXISTS idx_videos_name ON videos(name)",
        "CREATE INDEX IF NOT EXISTS idx_videos_record_time ON videos(record_time)",
        "CREATE INDEX IF NOT EXISTS idx_videos_created_at ON videos(created_at)",
        "CREATE INDEX IF NOT EXISTS idx_images_name ON images(name)",
        "CREATE INDEX IF NOT EXISTS idx_images_created_at ON images(created_at)",
        "CREATE INDEX IF NOT EXISTS idx_tasks_video_id ON tasks(video_id)",
        "CREATE INDEX IF NOT EXISTS idx_tasks_image_id ON tasks(image_id)",
        "CREATE INDEX IF NOT EXISTS idx_tasks_backend ON tasks(analysis_backend)",
        "CREATE INDEX IF NOT EXISTS idx_tasks_status ON tasks(status)",
        "CREATE INDEX IF NOT EXISTS idx_tasks_start_time ON tasks(start_time)",
    };

    for (const char* idx : indexes) {
        if (!q.exec(QString::fromLatin1(idx))) {
            spdlog::warn("Failed to create index: {}", q.lastError().text().toStdString());
        }
    }
}

void Database::RunMigrations() {
    QSqlQuery q(db_);
    // Add name column to tasks (safe to ignore error if already exists)
    q.exec(QStringLiteral("ALTER TABLE tasks ADD COLUMN name TEXT"));
}

}  // namespace db
}  // namespace omniface
