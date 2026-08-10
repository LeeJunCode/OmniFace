#include "omniface/core/quality/l2cs/l2cs_quality_evaluator.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace omniface::quality {

namespace {

constexpr float kYawMax = 90.0f;
constexpr float kPitchMax = 70.0f;

}  // namespace

L2csQualityEvaluator::L2csQualityEvaluator(QObject* parent)
    : BaseQualityEvaluator() {
    setParent(parent);
}

bool L2csQualityEvaluator::LoadCsv(const QString& main_csv,
                                    const QString& /*quality_csv*/) {
    raw_frames_.clear();
    report_ = L2csQualityReport{};

    QFile file(main_csv);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit Error(QStringLiteral("无法打开L2CS CSV文件: %1").arg(main_csv));
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

bool L2csQualityEvaluator::IsReady() const {
    return loaded_ && evaluated_;
}

std::unordered_map<std::string, int> L2csQualityEvaluator::BuildColumnMap(
    const QStringList& header_cols) {
    std::unordered_map<std::string, int> m;
    for (int i = 0; i < header_cols.size(); ++i)
        m[header_cols[i].trimmed().toStdString()] = i;
    return m;
}

L2csQualityEvaluator::RawFrame L2csQualityEvaluator::ParseRow(
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
    f.yaw_maxp = get("yaw_maxp");
    f.pitch_maxp = get("pitch_maxp");
    f.yaw_deg = get("gaze_yaw");
    f.pitch_deg = get("gaze_pitch");

    return f;
}

void L2csQualityEvaluator::Evaluate() {
    if (!loaded_) {
        emit Error(QStringLiteral("请先加载CSV数据"));
        return;
    }

    report_ = L2csQualityReport{};
    report_.total_frames = static_cast<int>(raw_frames_.size());
    report_.frames.resize(raw_frames_.size());

    float sum_maxp = 0;
    float sum_av = 0, max_av = 0;

    for (size_t i = 0; i < raw_frames_.size(); ++i) {
        const auto& src = raw_frames_[i];
        auto& dst = report_.frames[i];

        dst.frame_number = src.frame_number;
        dst.yaw_deg = src.yaw_deg;
        dst.pitch_deg = src.pitch_deg;
        dst.yaw_maxp = src.yaw_maxp;
        dst.pitch_maxp = src.pitch_maxp;

        float mean_maxp = (src.yaw_maxp + src.pitch_maxp) * 0.5f;
        sum_maxp += mean_maxp;

        if (i > 0) {
            float dt = src.timestamp - raw_frames_[i - 1].timestamp;
            if (dt <= 0) dt = 1.0f / 30.0f;
            float dy = dst.yaw_deg - report_.frames[i - 1].yaw_deg;
            float dp = dst.pitch_deg - report_.frames[i - 1].pitch_deg;
            float angle = std::sqrt(dy * dy + dp * dp);
            dst.angular_velocity = angle / dt;
            sum_av += dst.angular_velocity;
            max_av = std::max(max_av, dst.angular_velocity);
        }
    }

    int n = static_cast<int>(raw_frames_.size());
    report_.mean_maxp = sum_maxp / n;
    if (n > 1) report_.mean_angular_velocity = sum_av / (n - 1);
    report_.max_angular_velocity = max_av;

    AssignFrameGrades();
    GenerateSummary();

    evaluated_ = true;
    emit Finished(report_);
}

void L2csQualityEvaluator::AssignFrameGrades() {
    auto& frames = report_.frames;

    // Fixed thresholds based on 90-bin softmax semantics:
    //   uniform distribution → max_prob ≈ 1/90 ≈ 0.011
    //   ≥ 0.50 → dominant single bin (highly confident)
    //   ≥ 0.30 → mass concentrated in few bins
    //   ≥ 0.15 → ~13× uniform baseline, moderate concentration
    //   ≥ 0.05 → just above near-uniform, weakly structured
    // These are absolute interpretations — independent of backbone model.

    float sum_conf = 0, sum_stab = 0;

    for (auto& f : frames) {
        int sc = 0, ss = 0;

        // Confidence — mean max_prob (max 50)
        float mean_maxp = (f.yaw_maxp + f.pitch_maxp) * 0.5f;
        if (mean_maxp >= 0.50f)          sc = 50;
        else if (mean_maxp >= 0.30f)     sc = 40;
        else if (mean_maxp >= 0.15f)     sc = 25;
        else if (mean_maxp >= 0.05f)     sc = 12;
        else                              sc = 0;

        // Stability — angular velocity + gaze range (max 50)
        int sv = 0, sr = 0;
        if (f.angular_velocity < 30.0f)        sv = 25;
        else if (f.angular_velocity < 60.0f)   sv = 20;
        else if (f.angular_velocity < 120.0f)  sv = 12;
        else if (f.angular_velocity < 200.0f)  sv = 6;
        else                                     sv = 0;

        float max_angle = std::max(std::abs(f.yaw_deg), std::abs(f.pitch_deg));
        if (max_angle < 30.0f)             sr = 25;
        else if (max_angle < 60.0f)        sr = 20;
        else if (max_angle < 90.0f)        sr = 12;
        else if (max_angle < 120.0f)       sr = 6;
        else                                sr = 0;

        ss = sv + sr;

        f.score_conf = sc;
        f.score_stab = ss;
        f.score = sc + ss;

        sum_conf += sc; sum_stab += ss;

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
    report_.mean_score_conf = sum_conf / n;
    report_.mean_score_stab = sum_stab / n;
}

void L2csQualityEvaluator::GenerateSummary() {
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
        std::string("L2CS-Net 视线数据质量评估报告\n")
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
        + "置信度均分: " + std::to_string(report_.mean_score_conf) + "/50\n"
        + "稳定性均分: " + std::to_string(report_.mean_score_stab) + "/50\n"
        + "平均max_prob: " + std::to_string(report_.mean_maxp) + "\n"
        + "平均视线角速度: " + std::to_string(report_.mean_angular_velocity) + "/s\n"
        + "最大视线角速度: " + std::to_string(report_.max_angular_velocity) + "/s\n";
}

}  // namespace omniface::quality
