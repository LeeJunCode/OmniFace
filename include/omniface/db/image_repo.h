#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QSqlDatabase>

namespace omniface {
namespace db {

struct ImageRecord {
    int64_t id = 0;
    std::string name;
    std::string description;
    std::string notes;
    std::string saved_path;
    float face_confidence = 0.0f;
    float bbox_x1 = 0.0f, bbox_y1 = 0.0f, bbox_x2 = 0.0f, bbox_y2 = 0.0f;
    std::array<float, 10> keypoints{};
    std::array<float, 512> embedding{};
    std::string created_at;
};

struct ImageFilter {
    std::optional<std::string> name_search;
    std::optional<float> min_confidence;
};

struct ImageSort {
    std::string column = "created_at";
    bool ascending = false;
};

class ImageRepo {
public:
    explicit ImageRepo(QSqlDatabase& db);

    int64_t Insert(const ImageRecord& image);
    std::optional<ImageRecord> FindById(int64_t id);
    std::vector<ImageRecord> FindAll(const std::optional<ImageFilter>& filter = {},
                                     const std::optional<ImageSort>& sort = {});
    bool Update(int64_t id, const ImageRecord& fields);
    bool Delete(int64_t id);
    std::vector<ImageRecord> SearchByName(const std::string& query);
    std::optional<ImageRecord> FindBySavedPath(const std::string& saved_path);

private:
    ImageRecord RowToRecord(const class QSqlQuery& query);
    std::string BuildWhereClause(const ImageFilter& filter) const;

    QSqlDatabase& db_;
};

}  // namespace db
}  // namespace omniface
