#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QSqlDatabase>

namespace omniface {
namespace db {

struct TaskRecord {
    int64_t id = 0;
    int64_t video_id = 0;
    int64_t image_id = 0;
    std::string name;
    std::string description;
    std::string notes;
    std::string backend;
    std::string params_yaml;
    std::string start_time;
    std::string end_time;
    int total_frames = 0;
    double avg_fps = 0.0;
    std::string output_dir;
    std::string output_video_path;
    std::string csv_path;
    std::string output_types;
    std::string status = "running";
};

struct TaskFilter {
    std::optional<std::string> backend;
    std::optional<std::string> status;
    std::optional<std::string> start_time_from;
    std::optional<std::string> start_time_to;
};

struct TaskSort {
    std::string column = "start_time";
    bool ascending = false;
};

class TaskRepo {
public:
    explicit TaskRepo(QSqlDatabase& db);

    int64_t Insert(const TaskRecord& task);
    std::optional<TaskRecord> FindById(int64_t id);
    std::vector<TaskRecord> FindAll(const std::optional<TaskFilter>& filter = {},
                                    const std::optional<TaskSort>& sort = {});
    bool Update(int64_t id, const TaskRecord& fields);
    bool Delete(int64_t id, bool delete_output_files = false);

private:
    TaskRecord RowToRecord(const class QSqlQuery& query);
    std::string BuildWhereClause(const TaskFilter& filter) const;

    QSqlDatabase& db_;
};

}  // namespace db
}  // namespace omniface
