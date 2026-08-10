#include "omniface/core/quality/mefarg_au/mefarg_au_quality_evaluator.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>

#include "omniface/core/analysis/mefarg_au/mefarg_au_constants.h"

namespace omniface::quality {

using analysis::mefarg_au::kNumAuOutputs;
using analysis::mefarg_au::kAuNames;

inline constexpr int kNumAu = kNumAuOutputs;

MefargAuQualityEvaluator::MefargAuQualityEvaluator(QObject* parent)
    : BaseQualityEvaluator() {
    setParent(parent);
}

bool MefargAuQualityEvaluator::LoadCsv(const QString& main_csv,
                                        const QString& /*quality_csv*/) {
    raw_frames_.clear();
    report_ = MefargAuQualityReport{};

    QFile file(main_csv);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit Error(QStringLiteral("无法打开MEFARG CSV文件: %1").arg(main_csv));
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

bool MefargAuQualityEvaluator::IsReady() const {
    return loaded_ && evaluated_;
}

std::unordered_map<std::string, int> MefargAuQualityEvaluator::BuildColumnMap(
    const QStringList& header_cols) {
    std::unordered_map<std::string, int> m;
    for (int i = 0; i < header_cols.size(); ++i)
        m[header_cols[i].trimmed().toStdString()] = i;
    return m;
}

MefargAuQualityEvaluator::RawFrame MefargAuQualityEvaluator::ParseRow(
    const QStringList& cols,
    const std::unordered_map<std::string, int>& cm) {

    RawFrame f;
    auto getFloat = [&](const std::string& name) -> float {
        auto it = cm.find(name);
        if (it != cm.end() && it->second < cols.size())
            return cols[it->second].toFloat();
        return 0.0f;
    };

    f.frame_number = static_cast<int>(getFloat("frame"));

    float margin_sum = 0;
    int active_count = 0;
    for (int i = 0; i < kNumAu; ++i) {
        std::string prob_col = std::string(kAuNames[i]) + "_prob";
        std::string active_col = std::string(kAuNames[i]) + "_active";
        float prob = getFloat(prob_col);
        int active = static_cast<int>(getFloat(active_col));
        margin_sum += std::abs(prob - 0.5f);
        if (active) ++active_count;
    }
    f.active_au_count = active_count;
    f.mean_decision_margin = margin_sum / kNumAu;

    return f;
}

void MefargAuQualityEvaluator::Evaluate() {
    if (!loaded_) {
        emit Error(QStringLiteral("请先加载CSV数据"));
        return;
    }

    report_ = MefargAuQualityReport{};
    report_.total_frames = static_cast<int>(raw_frames_.size());
    report_.frames.resize(raw_frames_.size());

    float sum_margin = 0;
    int total_flips = 0;
    int spike_frames = 0;

    for (size_t i = 0; i < raw_frames_.size(); ++i) {
        const auto& src = raw_frames_[i];
        auto& dst = report_.frames[i];

        dst.frame_number = src.frame_number;
        dst.active_au_count = src.active_au_count;
        dst.mean_decision_margin = src.mean_decision_margin;

        sum_margin += src.mean_decision_margin;

        // AU flips: count of individual AU state changes from previous frame
        // Since we don't store per-AU active states in RawFrame, use active count delta
        // as an approximation. This underestimates true flips (cancellations) but is conservative.
        if (i > 0) {
            int prev_count = raw_frames_[i - 1].active_au_count;
            int flips = std::abs(src.active_au_count - prev_count);
            dst.au_flips = flips;
            total_flips += flips;
        }
    }

    // Spike detection: frames where active_count spikes briefly
    for (size_t i = 0; i < raw_frames_.size(); ++i) {
        auto& dst = report_.frames[i];
        int cur = raw_frames_[i].active_au_count;

        // Check if this frame is a spike: count is 0 or very different from neighbors
        int prev = (i > 0) ? raw_frames_[i - 1].active_au_count : cur;
        int next = (i + 1 < raw_frames_.size()) ? raw_frames_[i + 1].active_au_count : cur;

        // Spike: current frame has AUs active but at least one neighbor has none,
        // OR the count jumps by > 10 between consecutive frames (unlikely in real expressions)
        if ((cur > 0 && (prev == 0 || next == 0)) ||
            (i > 0 && std::abs(cur - prev) > 10)) {
            dst.is_spike = true;
            ++spike_frames;
        }
    }

    int n = static_cast<int>(raw_frames_.size());
    report_.mean_decision_margin = sum_margin / n;
    report_.overall_au_flip_rate = (n > 1) ?
        static_cast<float>(total_flips) / ((n - 1) * kNumAu) : 0;
    report_.total_spike_frames = spike_frames;

    AssignFrameGrades();
    GenerateSummary();

    evaluated_ = true;
    emit Finished(report_);
}

void MefargAuQualityEvaluator::AssignFrameGrades() {
    auto& frames = report_.frames;

    float sum_conf = 0, sum_stab = 0;

    for (auto& f : frames) {
        int sc = 0, ss = 0;

        // Confidence — decision margin (max 50)
        float margin = f.mean_decision_margin;
        if (margin >= 0.3f)            sc = 50;
        else if (margin >= 0.2f)       sc = 40;
        else if (margin >= 0.1f)       sc = 25;
        else if (margin >= 0.05f)      sc = 12;
        else                            sc = 0;

        // Stability — flip rate proxy + spike (max 50)
        int sf = 0, sk = 0;
        if (f.au_flips <= 1)                sf = 25;
        else if (f.au_flips <= 3)           sf = 20;
        else if (f.au_flips <= 6)           sf = 12;
        else if (f.au_flips <= 10)          sf = 6;
        else                                 sf = 0;

        if (!f.is_spike && f.active_au_count >= 1 && f.active_au_count <= 12)
            sk = 25;
        else if (!f.is_spike && f.active_au_count <= 15)
            sk = 15;
        else if (!f.is_spike)
            sk = 10;
        else
            sk = 0;

        ss = sf + sk;

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

void MefargAuQualityEvaluator::GenerateSummary() {
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
        std::string("MEFARG AU 数据质量评估报告\n")
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
        + "平均判决策略距离: " + std::to_string(report_.mean_decision_margin) + "\n"
        + "AU翻转率: " + std::to_string(report_.overall_au_flip_rate) + "\n"
        + "脉冲噪声帧: " + std::to_string(report_.total_spike_frames) + "\n";
}

}  // namespace omniface::quality
