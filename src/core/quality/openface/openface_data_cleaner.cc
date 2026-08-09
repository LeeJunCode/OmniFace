#include "omniface/core/quality/openface/openface_data_cleaner.h"
#include "omniface/core/quality/openface/openface_quality_evaluator.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace omniface::clean {

OpenFaceDataCleaner::OpenFaceDataCleaner(QObject* parent)
    : BaseDataCleaner() {
    setParent(parent);
}

bool OpenFaceDataCleaner::LoadCsv(const QString& main_csv,
                                   const QString& quality_csv) {
    main_csv_path_ = main_csv;
    quality_csv_path_ = quality_csv;

    quality::OpenFaceQualityEvaluator eval;
    QObject::connect(&eval, &quality::OpenFaceQualityEvaluator::Error,
                     this, &BaseDataCleaner::Error);

    if (!eval.LoadCsv(main_csv, quality_csv))
        return false;

    eval.Evaluate();
    report_ = eval.Report();
    loaded_ = true;
    return true;
}

namespace {

int GradeToMinScore(GradeCut g) {
    switch (g) {
    case GradeCut::kS:    return 93;
    case GradeCut::kA:    return 85;
    case GradeCut::kB:    return 70;
    case GradeCut::kC:    return 50;
    case GradeCut::kNone: return 0;
    }
    return 0;
}

}  // namespace

CleanResult OpenFaceDataCleaner::Execute(const CleanConfig& cfg) {
    CleanResult result;
    if (!loaded_) {
        emit Error(QStringLiteral("请先加载CSV数据"));
        return result;
    }

    const auto& frames = report_.frames;
    result.total_original = report_.total_frames;

    int min_score = GradeToMinScore(cfg.min_grade);

    std::vector<size_t> keep_indices;
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].score < min_score) {
            ++result.dropped_by_grade;
            continue;
        }
        keep_indices.push_back(i);
    }

    result.total_clean = static_cast<int>(keep_indices.size());

    auto pct = [&](int c) -> float {
        return (result.total_original > 0) ?
            100.0f * c / result.total_original : 0;
    };

    const char* grade_names[] = {"S", "A", "B", "C", "不限制"};
    result.summary_text =
        std::string("数据清洗统计\n===========\n")
        + "清洗条件: " + grade_names[static_cast<int>(cfg.min_grade)] + " 级及以上\n"
        + "原始帧数: " + std::to_string(result.total_original) + "\n"
        + "清洗后:   " + std::to_string(result.total_clean)
            + " (" + std::to_string(static_cast<int>(pct(result.total_clean))) + "%)\n"
        + "丢弃帧数: " + std::to_string(result.dropped_by_grade)
            + " (" + std::to_string(static_cast<int>(pct(result.dropped_by_grade))) + "%)\n";

    if (!keep_indices.empty()) {
        namespace fs = std::filesystem;
        std::string stem = fs::path(main_csv_path_.toStdString())
                               .filename().replace_extension("").string();
        QString output = QString::fromStdString(
            (fs::path(main_csv_path_.toStdString()).parent_path()
             / (stem + "_clean.csv")).string());
        WriteCleanCsv(keep_indices, output);
    }

    emit Finished(result);
    return result;
}

void OpenFaceDataCleaner::WriteCleanCsv(
    const std::vector<size_t>& keep_indices,
    const QString& output_path) {

    QFile in_file(main_csv_path_);
    if (!in_file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QFile out_file(output_path);
    if (!out_file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream in(&in_file);
    QTextStream out(&out_file);

    // Write header
    QString header = in.readLine();
    out << header << "\n";

    // Index frames to keep by their frame_number
    const auto& frames = report_.frames;
    std::unordered_set<int> keep_frames;
    for (size_t idx : keep_indices)
        keep_frames.insert(frames[idx].frame_number);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        int comma = line.indexOf(',');
        if (comma <= 0) continue;
        int fnum = line.left(comma).toInt();
        if (keep_frames.count(fnum))
            out << line << "\n";
    }
}

}  // namespace omniface::clean
