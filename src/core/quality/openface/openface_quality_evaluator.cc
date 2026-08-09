#include "omniface/core/quality/openface/openface_quality_evaluator.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <unordered_set>

namespace omniface::quality {

namespace {

constexpr float kRadToDeg = 180.0f / 3.141592653589793f;
constexpr float kConfidenceThreshold = 0.725f;
constexpr float kGazeFailThreshold = 0.01f;

const std::vector<int> kAuOrder = {1,2,4,5,6,7,9,10,12,14,15,17,20,23,25,26,28,45};
const std::unordered_set<int> kAuSet(kAuOrder.begin(), kAuOrder.end());

}  // namespace

OpenFaceQualityEvaluator::OpenFaceQualityEvaluator(QObject* parent)
    : BaseQualityEvaluator() {
    setParent(parent);
}

bool OpenFaceQualityEvaluator::LoadCsv(const QString& main_csv,
                                        const QString& quality_csv) {
    raw_frames_.clear();
    report_ = OpenFaceQualityReport{};

    QFile file(main_csv);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit Error(QStringLiteral("无法打开主CSV文件: %1").arg(main_csv));
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
        auto frame = ParseMainRow(cols, col_map);
        if (frame.frame_number > 0)
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

    // Load quality CSV if available
    if (!quality_csv.isEmpty() && QFile::exists(quality_csv))
        quality_data_ = LoadQualityCsv(quality_csv);

    loaded_ = true;
    evaluated_ = false;
    return true;
}

bool OpenFaceQualityEvaluator::IsReady() const {
    return loaded_ && evaluated_;
}

std::unordered_map<std::string, int> OpenFaceQualityEvaluator::BuildColumnMap(
    const QStringList& header_cols) {
    std::unordered_map<std::string, int> m;
    for (int i = 0; i < header_cols.size(); ++i)
        m[header_cols[i].trimmed().toStdString()] = i;
    return m;
}

OpenFaceQualityEvaluator::RawFrame OpenFaceQualityEvaluator::ParseMainRow(
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
    f.confidence = get("confidence");
    f.success = (static_cast<int>(get("success")) == 1);
    f.pose_rx = get("pose_Rx");
    f.pose_ry = get("pose_Ry");
    f.pose_rz = get("pose_Rz");
    f.gaze_0_x = get("gaze_0_x");
    f.gaze_0_y = get("gaze_0_y");
    f.gaze_0_z = get("gaze_0_z");
    f.gaze_1_x = get("gaze_1_x");
    f.gaze_1_y = get("gaze_1_y");
    f.gaze_1_z = get("gaze_1_z");
    f.gaze_angle_x = get("gaze_angle_x");
    f.gaze_angle_y = get("gaze_angle_y");

    // AUs
    for (int au : kAuSet) {
        char cKey[16], rKey[16];
        std::snprintf(cKey, sizeof(cKey), "AU%02d_c", au);
        std::snprintf(rKey, sizeof(rKey), "AU%02d_r", au);
        auto cIt = cm.find(cKey);
        if (cIt != cm.end() && cIt->second < cols.size()) {
            if (static_cast<int>(cols[cIt->second].toFloat()) == 1) {
                f.active_aus.push_back(au);
                auto rIt = cm.find(rKey);
                float intensity = (rIt != cm.end() && rIt->second < cols.size())
                    ? cols[rIt->second].toFloat() : 0.0f;
                f.au_intensities.push_back(intensity);
            }
        }
    }

    // Eye landmarks (56 points)
    for (int i = 0; i < 56; ++i) {
        std::string xKey = "eye_lmk_x_" + std::to_string(i);
        std::string yKey = "eye_lmk_y_" + std::to_string(i);
        auto xIt = cm.find(xKey);
        auto yIt = cm.find(yKey);
        if (xIt != cm.end() && yIt != cm.end()) {
            f.eye_lmk_x.push_back(get(xKey.c_str()));
            f.eye_lmk_y.push_back(get(yKey.c_str()));
        }
    }

    // PDM params (p_0..p_33)
    for (int i = 0; i < 34; ++i) {
        std::string key = "p_" + std::to_string(i);
        f.pdm_params.push_back(get(key.c_str()));
    }

    return f;
}

OpenFaceQualityEvaluator::QualityCsvData OpenFaceQualityEvaluator::LoadQualityCsv(
    const QString& path) {
    QualityCsvData data;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return data;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList cols = line.split(QStringLiteral(","));
        if (cols.isEmpty()) continue;

        if (cols[0] == QStringLiteral("#eigen_values")) {
            for (int i = 1; i < cols.size(); ++i)
                data.eigenvalues.push_back(cols[i].toFloat());
            continue;
        }

        if (cols[0] == QStringLiteral("frame")) continue;  // header

        int frame = cols[0].toInt();
        if (cols.size() >= 2)
            data.frame_quality_score[frame] = cols[1].toFloat();
        if (cols.size() >= 3)
            data.frame_visible[frame] = cols[2].toInt();
    }

    data.valid = true;
    return data;
}

void OpenFaceQualityEvaluator::Evaluate() {
    if (!loaded_) {
        emit Error(QStringLiteral("请先加载CSV数据"));
        return;
    }

    report_ = OpenFaceQualityReport{};
    report_.total_frames = static_cast<int>(raw_frames_.size());

    // Transfer eigenvalues
    if (!quality_data_.eigenvalues.empty()) {
        report_.pdm_eigenvalues = quality_data_.eigenvalues;
        report_.eigenvalues_valid = true;
    }

    EvaluateLandmarkQuality();
    EvaluatePoseQuality();
    EvaluateGazeQuality();
    EvaluateAuQuality();
    AssignFrameGrades();
    GenerateSummary();

    evaluated_ = true;
    emit Finished(report_);
}

void OpenFaceQualityEvaluator::EvaluateLandmarkQuality() {
    auto& frames = report_.frames;
    frames.resize(raw_frames_.size());

    float sum_conf = 0, sum_sq = 0;
    float sum_qs = 0, sum_vis = 0;
    int n = 0;

    for (size_t i = 0; i < raw_frames_.size(); ++i) {
        const auto& src = raw_frames_[i];
        auto& dst = frames[i];
        dst.frame_number = src.frame_number;
        dst.confidence = src.confidence;
        dst.success = src.success;

        // Quality score and visible count from supplement
        if (quality_data_.valid) {
            auto qit = quality_data_.frame_quality_score.find(src.frame_number);
            dst.quality_score = (qit != quality_data_.frame_quality_score.end())
                ? qit->second : (src.confidence * 0.5f + 0.5f);
            auto vit = quality_data_.frame_visible.find(src.frame_number);
            dst.visible_count = (vit != quality_data_.frame_visible.end())
                ? vit->second : 68;
        } else {
            dst.quality_score = src.confidence * 0.5f + 0.5f;
            dst.visible_count = 68;
        }

        dst.decision_margin = std::abs(src.confidence - kConfidenceThreshold);

        sum_conf += src.confidence;
        sum_sq += src.confidence * src.confidence;
        sum_qs += dst.quality_score;
        sum_vis += dst.visible_count;
        ++n;
    }

    if (n > 0) {
        report_.mean_confidence = sum_conf / n;
        report_.std_confidence = std::sqrt(sum_sq / n - report_.mean_confidence * report_.mean_confidence);
        report_.mean_quality_score = sum_qs / n;
        report_.mean_visible_count = sum_vis / n;
    }

    // Confidence histogram (10 bins)
    report_.confidence_histogram.resize(10, 0);
    for (const auto& f : frames) {
        int bin = std::clamp(static_cast<int>(f.confidence * 10.0f), 0, 9);
        report_.confidence_histogram[bin]++;
    }
}

void OpenFaceQualityEvaluator::EvaluatePoseQuality() {
    auto& frames = report_.frames;
    int anomaly = 0;
    float sum_av = 0, max_av = 0;

    for (size_t i = 0; i < raw_frames_.size(); ++i) {
        const auto& src = raw_frames_[i];
        auto& dst = frames[i];

        dst.pitch_deg = src.pose_rx * kRadToDeg;
        dst.yaw_deg   = src.pose_ry * kRadToDeg;
        dst.roll_deg  = src.pose_rz * kRadToDeg;

        if (std::abs(dst.pitch_deg) > 60.0f ||
            std::abs(dst.yaw_deg) > 90.0f ||
            std::abs(dst.roll_deg) > 45.0f) {
            dst.pose_anomaly = true;
            ++anomaly;
        }

        if (i > 0) {
            float dp = dst.pitch_deg - frames[i - 1].pitch_deg;
            float dy = dst.yaw_deg - frames[i - 1].yaw_deg;
            float dr = dst.roll_deg - frames[i - 1].roll_deg;
            dst.angular_velocity = std::sqrt(dp * dp + dy * dy + dr * dr);
            sum_av += dst.angular_velocity;
            max_av = std::max(max_av, dst.angular_velocity);
        }
    }

    report_.frames_pose_anomaly = anomaly;
    int n = static_cast<int>(frames.size());
    if (n > 1) {
        report_.mean_angular_velocity = sum_av / (n - 1);
    }
    report_.max_angular_velocity = max_av;
}

void OpenFaceQualityEvaluator::EvaluateGazeQuality() {
    auto& frames = report_.frames;
    float sum_diff = 0, max_diff = 0;
    int failed = 0, n_valid = 0;

    for (size_t i = 0; i < raw_frames_.size(); ++i) {
        const auto& src = raw_frames_[i];
        auto& dst = frames[i];

        // Check gaze failure (zero vector)
        float g0_len2 = src.gaze_0_x * src.gaze_0_x +
                        src.gaze_0_y * src.gaze_0_y +
                        src.gaze_0_z * src.gaze_0_z;
        float g1_len2 = src.gaze_1_x * src.gaze_1_x +
                        src.gaze_1_y * src.gaze_1_y +
                        src.gaze_1_z * src.gaze_1_z;

        if (g0_len2 < kGazeFailThreshold || g1_len2 < kGazeFailThreshold) {
            dst.gaze_failed = true;
            ++failed;
            continue;
        }

        // Normalize and compute angle between eyes
        float g0_len = std::sqrt(g0_len2);
        float g1_len = std::sqrt(g1_len2);
        float dot = (src.gaze_0_x / g0_len) * (src.gaze_1_x / g1_len) +
                    (src.gaze_0_y / g0_len) * (src.gaze_1_y / g1_len) +
                    (src.gaze_0_z / g0_len) * (src.gaze_1_z / g1_len);
        dot = std::clamp(dot, -1.0f, 1.0f);
        dst.gaze_angle_diff = std::acos(dot) * kRadToDeg;
        sum_diff += dst.gaze_angle_diff;
        max_diff = std::max(max_diff, dst.gaze_angle_diff);
        ++n_valid;
    }

    report_.gaze_fail_rate = (frames.size() > 0)
        ? static_cast<float>(failed) / frames.size() : 0;
    if (n_valid > 0) {
        report_.mean_gaze_diff = sum_diff / n_valid;
    }
    report_.max_gaze_diff = max_diff;
}

void OpenFaceQualityEvaluator::EvaluateAuQuality() {
    auto& frames = report_.frames;
    int total_flips = 0;
    int facs_violations = 0;

    // Per-AU flip counting
    std::unordered_map<int, int> au_present_prev;
    std::unordered_map<int, int> au_flip_count;

    for (size_t i = 0; i < raw_frames_.size(); ++i) {
        const auto& src = raw_frames_[i];
        auto& dst = frames[i];

        std::unordered_set<int> au_set(src.active_aus.begin(),
                                        src.active_aus.end());
        dst.active_au_count = static_cast<int>(src.active_aus.size());
        dst.active_au_ids = src.active_aus;

        // FACS constraints
        bool has_au12 = au_set.count(12);
        bool has_au15 = au_set.count(15);
        bool has_au1 = au_set.count(1);
        bool has_au4 = au_set.count(4);
        bool has_au25 = au_set.count(25);
        bool has_au26 = au_set.count(26);

        if ((has_au12 && has_au15) || (has_au1 && has_au4) ||
            (has_au26 && !has_au25)) {
            dst.facs_violation = true;
            ++facs_violations;
        }

        // Flip counting
        if (i > 0) {
            for (int au : kAuOrder) {
                bool prev = au_present_prev.count(au);
                bool curr = au_set.count(au);
                if (prev != curr)
                    au_flip_count[au]++;
            }
        }
        au_present_prev.clear();
        for (int au : src.active_aus)
            au_present_prev[au] = 1;
    }

    // Overall flip rate
    int total_au_checks = (frames.size() > 1) ?
        static_cast<int>(frames.size() - 1) * static_cast<int>(kAuOrder.size()) : 0;
    for (const auto& [au, count] : au_flip_count)
        total_flips += count;
    report_.overall_au_flip_rate = (total_au_checks > 0) ?
        static_cast<float>(total_flips) / total_au_checks : 0;

    report_.frames_facs_violation = facs_violations;

    // Per-AU flip counts
    for (int au : kAuOrder)
        report_.au_flip_counts.push_back(au_flip_count[au]);

    // Per-AU mean intensity
    std::unordered_map<int, float> au_intensity_sum;
    std::unordered_map<int, int> au_intensity_count;
    for (const auto& src : raw_frames_) {
        for (size_t j = 0; j < src.active_aus.size() && j < src.au_intensities.size(); ++j) {
            int au = src.active_aus[j];
            au_intensity_sum[au] += src.au_intensities[j];
            au_intensity_count[au]++;
        }
    }
    for (int au : kAuOrder) {
        float mean = (au_intensity_count[au] > 0) ?
            au_intensity_sum[au] / au_intensity_count[au] : 0;
        report_.au_intensity_means.push_back(mean);
    }
}

void OpenFaceQualityEvaluator::AssignFrameGrades() {
    auto& frames = report_.frames;

    float sum_sc = 0, sum_sv = 0, sum_sp = 0, sum_sg = 0, sum_sa = 0, sum_sau = 0;

    for (auto& f : frames) {
        int sc = 0, sv = 0, sp = 0, sg = 0, sa = 0, sau = 0;

        // --- Confidence (max 30) ---
        if (!f.success || f.confidence < 0.725f) {
            sc = 0;
        } else if (f.confidence >= 0.98f) {
            sc = 30;
        } else if (f.confidence >= 0.95f) {
            sc = 28;
        } else if (f.confidence >= 0.90f) {
            sc = 25;
        } else if (f.confidence >= 0.85f) {
            sc = 22;
        } else if (f.confidence >= 0.80f) {
            sc = 18;
        } else if (f.confidence >= 0.75f) {
            sc = 13;
        } else {
            sc = 8;
        }

        // --- Visible landmarks (max 15) ---
        if (f.visible_count >= 65) {
            sv = 15;
        } else if (f.visible_count >= 55) {
            sv = 12;
        } else if (f.visible_count >= 45) {
            sv = 9;
        } else if (f.visible_count >= 30) {
            sv = 5;
        } else {
            sv = 0;
        }

        // --- Pose quality (max 20) ---
        float max_angle = std::max({std::abs(f.pitch_deg),
                                    std::abs(f.yaw_deg),
                                    std::abs(f.roll_deg)});
        if (f.pose_anomaly) {
            sp = 0;
        } else if (max_angle < 15.0f) {
            sp = 20;
        } else if (max_angle < 30.0f) {
            sp = 16;
        } else if (max_angle < 45.0f) {
            sp = 10;
        } else {
            sp = 5;
        }

        // --- Gaze quality (max 15) ---
        if (f.gaze_failed) {
            sg = 0;
        } else if (f.gaze_angle_diff < 5.0f) {
            sg = 15;
        } else if (f.gaze_angle_diff < 10.0f) {
            sg = 12;
        } else if (f.gaze_angle_diff < 15.0f) {
            sg = 9;
        } else if (f.gaze_angle_diff < 25.0f) {
            sg = 5;
        } else {
            sg = 0;
        }

        // --- Angular velocity (max 10) ---
        if (f.angular_velocity < 30.0f) {
            sa = 10;
        } else if (f.angular_velocity < 60.0f) {
            sa = 8;
        } else if (f.angular_velocity < 100.0f) {
            sa = 5;
        } else if (f.angular_velocity < 200.0f) {
            sa = 2;
        } else {
            sa = 0;
        }

        // --- AU quality (max 10) ---
        if (f.facs_violation) {
            sau = 2;
        } else if (f.active_au_count >= 1 && f.active_au_count <= 8) {
            sau = 10;
        } else if (f.active_au_count == 0) {
            sau = 8;
        } else if (f.active_au_count <= 12) {
            sau = 6;
        } else {
            sau = 3;
        }

        f.score = sc + sv + sp + sg + sa + sau;

        // Accumulate for dimension-level means
        sum_sc += sc; sum_sv += sv; sum_sp += sp;
        sum_sg += sg; sum_sa += sa; sum_sau += sau;

        // Score → grade mapping
        if (f.score >= 93) {
            f.grade = 'S';
        } else if (f.score >= 85) {
            f.grade = 'A';
        } else if (f.score >= 70) {
            f.grade = 'B';
        } else if (f.score >= 50) {
            f.grade = 'C';
        } else {
            f.grade = 'D';
        }

        switch (f.grade) {
        case 'S': report_.grade_s++; break;
        case 'A': report_.grade_a++; break;
        case 'B': report_.grade_b++; break;
        case 'C': report_.grade_c++; break;
        default:  report_.grade_d++; break;
        }
    }

    int n = std::max(1, static_cast<int>(frames.size()));
    report_.mean_score_confidence = sum_sc / n;
    report_.mean_score_visible    = sum_sv / n;
    report_.mean_score_pose       = sum_sp / n;
    report_.mean_score_gaze       = sum_sg / n;
    report_.mean_score_angular    = sum_sa / n;
    report_.mean_score_au         = sum_sau / n;
}

void OpenFaceQualityEvaluator::GenerateSummary() {
    // Compute mean score
    float mean_score = 0;
    for (const auto& f : report_.frames)
        mean_score += f.score;
    if (report_.total_frames > 0)
        mean_score /= report_.total_frames;

    // Overall grade from mean score, same thresholds as frame grading
    char grade;
    if (mean_score >= 93.0f)
        grade = 'S';
    else if (mean_score >= 85.0f)
        grade = 'A';
    else if (mean_score >= 70.0f)
        grade = 'B';
    else if (mean_score >= 50.0f)
        grade = 'C';
    else
        grade = 'D';

    auto pct = [&](int count) -> float {
        return (report_.total_frames > 0) ?
            100.0f * count / report_.total_frames : 0;
    };

    report_.summary_text =
        std::string("OpenFace 数据质量评估报告\n")
        + "================================\n"
        + "综合等级: " + grade + "\n"
        + "平均得分: " + std::to_string(static_cast<int>(mean_score)) + "/100\n"
        + "总帧数: " + std::to_string(report_.total_frames) + "\n"
        + "S级(卓越 ≥93分): " + std::to_string(report_.grade_s)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_s))) + "%)\n"
        + "A级(优秀 85-92分): " + std::to_string(report_.grade_a)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_a))) + "%)\n"
        + "B级(良好 70-84分): " + std::to_string(report_.grade_b)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_b))) + "%)\n"
        + "C级(一般 50-69分): " + std::to_string(report_.grade_c)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_c))) + "%)\n"
        + "D级(较差 <50分): " + std::to_string(report_.grade_d)
            + " (" + std::to_string(static_cast<int>(pct(report_.grade_d))) + "%)\n"
        + "平均置信度: " + std::to_string(report_.mean_confidence) + "\n"
        + "置信度标准差: " + std::to_string(report_.std_confidence) + "\n"
        + "平均质量分: " + std::to_string(report_.mean_quality_score) + "\n"
        + "姿态异常帧: " + std::to_string(report_.frames_pose_anomaly) + "\n"
        + "平均双眼视线夹角: " + std::to_string(report_.mean_gaze_diff) + "°\n"
        + "视线失败率: " + std::to_string(report_.gaze_fail_rate * 100) + "%\n"
        + "AU翻转率: " + std::to_string(report_.overall_au_flip_rate) + "\n"
        + "FACS违规帧: " + std::to_string(report_.frames_facs_violation) + "\n";
}

}  // namespace omniface::quality
