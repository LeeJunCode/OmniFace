#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QSqlDatabase>

namespace omniface {
namespace db {

struct VideoRecord {
    int64_t id = 0;
    std::string name;
    std::string description;
    std::string notes;
    double duration = 0.0;
    int resolution_width = 0;
    int resolution_height = 0;
    double frame_rate = 0.0;
    std::string record_time;
    std::string record_location;
    int64_t file_size = 0;
    std::string saved_path;
    std::string created_at;
};

struct VideoFilter {
    std::optional<std::string> name_search;
    std::optional<std::string> record_time_from;
    std::optional<std::string> record_time_to;
};

struct VideoSort {
    std::string column = "created_at";
    bool ascending = false;
};

class VideoRepo {
public:
    explicit VideoRepo(QSqlDatabase& db);

    int64_t Insert(const VideoRecord& video);
    std::optional<VideoRecord> FindById(int64_t id);
    std::vector<VideoRecord> FindAll(const std::optional<VideoFilter>& filter = {},
                                     const std::optional<VideoSort>& sort = {});
    bool Update(int64_t id, const VideoRecord& fields);
    bool Delete(int64_t id);
    std::vector<VideoRecord> SearchByName(const std::string& query);
    std::optional<VideoRecord> FindBySavedPath(const std::string& saved_path);

private:
    VideoRecord RowToRecord(const class QSqlQuery& query);
    std::string BuildWhereClause(const VideoFilter& filter) const;

    QSqlDatabase& db_;
};

}  // namespace db
}  // namespace omniface
