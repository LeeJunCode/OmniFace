#include "omniface/core/quality/mediapipe/mediapipe_quality_evaluator.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace omniface::quality {

namespace {

constexpr float kPitchMax = 60.0f;
constexpr float kYawMax = 90.0f;
constexpr float kRollMax = 45.0f;

// sampling stride for face-size bounding-box scan (every N-th landmark pair)
constexpr int kLandmarkStride = 4;

float ComputeFaceSize(const QStringList& cols, int lm_start, int landmark_count) {
    int end = std::min(lm_start + landmark_count * 2, static_cast<int>(cols.size()));
    float min_x = INFINITY, max_x = -INFINITY;
    float min_y = INFINITY, max_y = -INFINITY;
    for (int i = lm_start; i + 1 < end; i += 2 * kLandmarkStride) {
        float x = cols[i].toFloat();
        float y = cols[i + 1].toFloat();
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }
    if (min_x > max_x || min_y > max_y) return 0.0f;
    float dw = max_x - min_x;
    float dh = max_y - min_y;
    return std::sqrt(dw * dw + dh * dh);
}

}  // namespace

MediaPipeQualityEvaluator::MediaPipeQualityEvaluator(QObject* parent)
    : BaseQualityEvaluator() {
    setParent(parent);
}

bool MediaPipeQualityEvaluator::LoadCsv(const QString& main_csv,
                                         const QString& /*quality_csv*/) {
    raw_frames_.clear();
    report_ = MediaPipeQualityReport{};

    QFile file(main_csv);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit Error(QStringLiteral("无法打开MediaPipe CSV文件: %1").arg(main_csv));
        return false;
    }

    QTextStream in(&file);
    QString header_line = in.readLine();
    if (header_line.isEmpty()) {
        emit Error(QStringLiteral("CSV文件为空"));
        return false;
    }

    QStringList header_cols = header_line.split(QStringLiteral(","));
    auto col_map = BuildColumnMap(header_cols);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        QStringList cols = line.split(QStringLiteral(","));
        auto frame = ParseRow(cols, col_map);
        raw_frames_.push_back(frame);
    }
    file.close();

    if (raw_frames_.empty()) {
        emit Error(QStringLiteral("CSV中无有效数据行"));
        return false;
    }

    std::sort(raw_frames_.begin(), raw_frames_.end(),
              [](const RawFrame& a, const RawFrame& b) {
                  return a.frame_number < b.frame_number;
              });

    loaded_ = true;
    evaluated_ = false;
    return true;
}

bool MediaPipeQualityEvaluator::IsReady() const {
    return loaded_ && evaluated_;
}

std::unordered_map<std::string, int> MediaPipeQualityEvaluator::BuildColumnMap(
    const QStringList& header_cols) {
    std::unordered_map<std::string, int> m;
    for (int i = 0; i < header_cols.size(); ++i)
        m[header_cols[i].trimmed().toStdString()] = i;
    return m;
}

MediaPipeQualityEvaluator::RawFrame MediaPipeQualityEvaluator::ParseRow(
    const QStringList& cols,
    const std::unordered_map<std::string, int>& cm) {

    RawFrame f;
    auto get = [&](const char* name) -> float {
        auto it = cm.find(name);
        if (it != cm.end() && it->second < cols.size())
            return cols[it->second].toFloat();
        return 0.0f;
    };

    f.frame_number = static_cast<int>(get("frame"));
    f.timestamp = get("timestamp");
    f.reproj_rms = get("reproj_rms");
    f.pitch_deg = get("pitch");
    f.yaw_deg = get("yaw");
    f.roll_deg = get("roll");

    // Compute face_size from landmark bounding-box diagonal
    auto it = cm.find("x_0");
    if (it != cm.end()) {
        int lm_start = it->second;
        f.face_size = ComputeFaceSize(cols, lm_start, 468);
    }

    return f;
}

void MediaPipeQualityEvaluator::Evaluate() {
    if (!loaded_) {
        emit Error(QStringLiteral("请先加载CSV数据"));
        return;
    }

    report_ = MediaPipeQualityReport{};
    report_.total_frames = static_cast<int>(raw_frames_.size());
    report_.frames.resize(raw_frames_.size());

    float sum_reproj = 0, max_reproj = 0;
    float sum_nre = 0, max_nre = 0;
    float sum_av = 0, max_av = 0;
    int anomaly = 0;

    for (size_t i = 0; i < raw_frames_.size(); ++i) {
        const auto& src = raw_frames_[i];
        auto& dst = report_.frames[i];

        dst.frame_number = src.frame_number;
        dst.reproj_rms = src.reproj_rms;
        dst.face_size = src.face_size;
        dst.pitch_deg = src.pitch_deg;
        dst.yaw_deg = src.yaw_deg;
        dst.roll_deg = src.roll_deg;

        sum_reproj += src.reproj_rms;
        max_reproj = std::max(max_reproj, src.reproj_rms);

        float nre = (src.face_size > 0) ? 100.0f * src.reproj_rms / src.face_size : 0;
        sum_nre += nre;
        max_nre = std::max(max_nre, nre);

        if (std::abs(dst.pitch_deg) > kPitchMax ||
            std::abs(dst.yaw_deg) > kYawMax ||
            std::abs(dst.roll_deg) > kRollMax) {
            dst.pose_anomaly = true;
            ++anomaly;
        }

        if (i > 0) {
            float dt = src.timestamp - raw_frames_[i - 1].timestamp;
            if (dt <= 0) dt = 1.0f / 30.0f;
            float dp = dst.pitch_deg - report_.frames[i - 1].pitch_deg;
            float dy = dst.yaw_deg - report_.frames[i - 1].yaw_deg;
            float dr = dst.roll_deg - report_.frames[i - 1].roll_deg;
            float angle = std::sqrt(dp * dp + dy * dy + dr * dr);
            dst.angular_velocity = angle / dt;
            sum_av += dst.angular_velocity;
            max_av = std::max(max_av, dst.angular_velocity);
        }
    }

    int n = static_cast<int>(raw_frames_.size());
    report_.mean_reproj_rms = sum_reproj / n;
    report_.max_reproj_rms = max_reproj;
    report_.mean_nre = sum_nre / n;
    report_.max_nre = max_nre;
    report_.frames_pose_anomaly = anomaly;
    if (n > 1) report_.mean_angular_velocity = sum_av / (n - 1);
    report_.max_angular_velocity = max_av;

    // NRE histogram (8 bins, percentage)
    report_.nre_histogram.resize(8, 0);
    for (const auto& f : report_.frames) {
        float nre = (f.face_size > 0) ? 100.0f * f.reproj_rms / f.face_size : 0;
        if (nre < 0.5f)       report_.nre_histogram[0]++;
        else if (nre < 1.0f)  report_.nre_histogram[1]++;
        else if (nre < 2.0f)  report_.nre_histogram[2]++;
        else if (nre < 3.5f)  report_.nre_histogram[3]++;
        else if (nre < 5.0f)  report_.nre_histogram[4]++;
        else if (nre < 8.0f)  report_.nre_histogram[5]++;
        else if (nre < 12.0f) report_.nre_histogram[6]++;
        else                   report_.nre_histogram[7]++;
    }

    AssignFrameGrades();
    GenerateSummary();

    evaluated_ = true;
    emit Finished(report_);
}

void MediaPipeQualityEvaluator::AssignFrameGrades() {
    auto& frames = report_.frames;

    float sum_geo = 0, sum_pose = 0;

    for (auto& f : frames) {
        int sg = 0, sp = 0;

        // Geometry — normalized reprojection error (max 50)
        float nre = (f.face_size > 0) ? 100.0f * f.reproj_rms / f.face_size : 100.0f;
        if (nre < 1.0f)             sg = 50;
        else if (nre < 2.0f)        sg = 40;
        else if (nre < 3.5f)        sg = 25;
        else if (nre < 5.0f)        sg = 12;
        else                         sg = 0;

        // Pose — head angle + smoothness (max 50)
        if (f.pose_anomaly) {
            sp = 0;
        } else {
            float max_angle = std::max({std::abs(f.pitch_deg),
                                        std::abs(f.yaw_deg),
                                        std::abs(f.roll_deg)});
            if (max_angle < 15.0f)           sp += 25;
            else if (max_angle < 30.0f)      sp += 20;
            else if (max_angle < 45.0f)      sp += 12;
            else if (max_angle < 60.0f)      sp += 6;

            if (f.angular_velocity < 30.0f)   sp += 25;
            else if (f.angular_velocity < 60.0f)  sp += 20;
            else if (f.angular_velocity < 120.0f) sp += 12;
            else if (f.angular_velocity < 200.0f) sp += 6;
        }

        f.score_geo = sg;
        f.score_pose = sp;
        f.score = sg + sp;

        sum_geo += sg; sum_pose += sp;

        if (f.score >= 93)       f.grade = 'S';
        else if (f.score >= 85)  f.grade = 'A';
        else if (f.score >= 70)  f.grade = 'B';
        else if (f.score >= 50)  f.grade = 'C';
        else                      f.grade = 'D';

        switch (f.grade) {
        case 'S': report_.grade_s++; break;
        case 'A': report_.grade_a++; break;
        case 'B': report_.grade_b++; break;
        case 'C': report_.grade_c++; break;
        default:  report_.grade_d++; break;
        }
    }

    int n = std::max(1, static_cast<int>(frames.size()));
    report_.mean_score_geo   = sum_geo / n;
    report_.mean_score_pose  = sum_pose / n;
}

void MediaPipeQualityEvaluator::GenerateSummary() {
    float mean_score = 0;
    for (const auto& f : report_.frames)
        mean_score += f.score;
    if (report_.total_frames > 0)
        mean_score /= report_.total_frames;

    char grade;
    if (mean_score >= 93.0f)       grade = 'S';
    else if (mean_score >= 85.0f)  grade = 'A';
    else if (mean_score >= 70.0f)  grade = 'B';
    else if (mean_score >= 50.0f)  grade = 'C';
    else                            grade = 'D';

    auto pct = [&](int count) -> float {
        return (report_.total_frames > 0) ?
            100.0f * count / report_.total_frames : 0;
    };

    report_.summary_text =
        std::string("MediaPipe 数据质量评估报告\n")
        + "================================\n"
        + "综合等级: " + grade + "\n"
        + "平均得分: " + std::to_string(static_cast<int>(mean_score)) + "/100\n"
        + "总帧数: " + std::to_string(report_.total_frames) + "\n"
        + "S级(卓越 >=93分): " + std::to_string(report_.grade_s)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_s))) + "%)\n"
        + "A级(优秀 85-92分): " + std::to_string(report_.grade_a)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_a))) + "%)\n"
        + "B级(良好 70-84分): " + std::to_string(report_.grade_b)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_b))) + "%)\n"
        + "C级(一般 50-69分): " + std::to_string(report_.grade_c)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_c))) + "%)\n"
        + "D级(较差 <50分): " + std::to_string(report_.grade_d)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_d))) + "%)\n"
        + "几何精度均分: " + std::to_string(report_.mean_score_geo) + "/50\n"
        + "姿态合理性均分: " + std::to_string(report_.mean_score_pose) + "/50\n"
        + "平均重投影误差: " + std::to_string(report_.mean_reproj_rms) + "px\n"
        + "最大重投影误差: " + std::to_string(report_.max_reproj_rms) + "px\n"
        + "平均NRE: " + std::to_string(report_.mean_nre) + "%\n"
        + "最大NRE: " + std::to_string(report_.max_nre) + "%\n"
        + "平均角速度: " + std::to_string(report_.mean_angular_velocity) + "/s\n"
        + "最大角速度: " + std::to_string(report_.max_angular_velocity) + "/s\n"
        + "姿态异常帧: " + std::to_string(report_.frames_pose_anomaly) + "\n";
}

}  // namespace omniface::quality
