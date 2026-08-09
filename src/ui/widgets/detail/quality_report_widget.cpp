#include "omniface/ui/widgets/detail/quality_report_widget.h"
#include "omniface/ui/theme.h"

#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QPieSeries>
#include <QTextEdit>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace omniface::ui {

namespace {

QColor ChartLine(int idx) {
    switch (idx) {
    case 0: return QColor(0x4E, 0x9A, 0xF1);  // blue
    case 1: return QColor(0xF4, 0x96, 0x4B);  // orange
    case 2: return QColor(0x6C, 0xD4, 0x6C);  // green
    default: return QColor(0xBB, 0xBB, 0xBB);
    }
}

QColor GradeColor(char grade) {
    switch (grade) {
    case 'S': return QColor(0x21, 0x96, 0xF3);  // blue
    case 'A': return QColor(0x4C, 0xAF, 0x50);  // green
    case 'B': return QColor(0xFF, 0x98, 0x00);  // orange
    case 'C': return QColor(0xFF, 0x57, 0x22);  // deep orange
    default:  return QColor(0xF4, 0x43, 0x36);  // red
    }
}

QString GradeLabel(char grade) {
    switch (grade) {
    case 'S': return QStringLiteral("S — 卓越 (≥93分)");
    case 'A': return QStringLiteral("A — 优秀 (85-92分)");
    case 'B': return QStringLiteral("B — 良好 (70-84分)");
    case 'C': return QStringLiteral("C — 一般 (50-69分)");
    default:  return QStringLiteral("D — 较差 (<50分)");
    }
}

QLabel* MakeExplainLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral(
        "QLabel { color: #999; font-size: 11px; padding: 6px 8px;"
        "  background: rgba(255,255,255,0.03); border-radius: 3px;"
        "  border-left: 2px solid #444; }"));
    return label;
}

QChartView* MakeChartView(QChart* chart) {
    chart->setBackgroundBrush(Qt::transparent);
    chart->legend()->setLabelColor(QColor(0xCC, 0xCC, 0xCC));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    auto* view = new QChartView(chart);
    view->setRenderHint(QPainter::Antialiasing);
    view->setMinimumHeight(200);
    view->setMaximumHeight(320);
    return view;
}

}  // namespace

QualityReportWidget::QualityReportWidget(QWidget* parent)
    : QWidget(parent) {
    SetupUI();
}

void QualityReportWidget::SetupUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setStyleSheet(QStringLiteral(
        "QScrollArea { border: none; background: transparent; }"
        "QScrollBar:vertical { background: #1a1a2e; width: 6px; border-radius: 3px; }"
        "QScrollBar::handle:vertical { background: #333; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

    content_widget_ = new QWidget(this);
    auto* content = new QVBoxLayout(content_widget_);
    content->setContentsMargins(8, 8, 8, 16);
    content->setSpacing(12);

    // Summary header
    summary_label_ = new QLabel(content_widget_);
    summary_label_->setWordWrap(true);
    summary_label_->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 14px; padding: 8px; }"));
    content->addWidget(summary_label_);

    grade_dist_label_ = new QLabel(content_widget_);
    grade_dist_label_->setWordWrap(true);
    grade_dist_label_->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 12px; padding: 4px 8px; }"));
    content->addWidget(grade_dist_label_);

    // Charts will be added here at render time
    content->addStretch();

    scroll_area_->setWidget(content_widget_);
    root->addWidget(scroll_area_);
}

void QualityReportWidget::SetStatusMessage(const QString& msg) {
    Clear();
    summary_label_->setText(msg);
}

void QualityReportWidget::Clear() {
    // Remove all chart views and scorecard, keep summary/grade_dist
    auto* layout = content_widget_->layout();
    while (layout->count() > 3) {
        auto* item = layout->takeAt(2);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    summary_label_->clear();
    grade_dist_label_->clear();
}

void QualityReportWidget::RenderReport(const quality::OpenFaceQualityReport& r) {
    Clear();

    auto& tm = ThemeManager::Instance();

    auto pct = [&](int c) { return (r.total_frames > 0) ? 100.0f * c / r.total_frames : 0; };

    // Compute mean score and overall grade
    float mean_score = 0;
    for (const auto& f : r.frames) mean_score += f.score;
    if (r.total_frames > 0) mean_score /= r.total_frames;

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
    QColor grColor = GradeColor(grade);

    summary_label_->setText(QStringLiteral(
        "<h2 style='margin:0;color:%1;'>综合评分: %2 (均分 %3/100)</h2>"
        "<p style='margin:4px 0;'>总帧数: %4 | 置信度均值: %5 | 姿态异常: %6帧 | 视线失败: %7%</p>")
        .arg(grColor.name(), GradeLabel(grade))
        .arg(mean_score, 0, 'f', 1)
        .arg(r.total_frames)
        .arg(r.mean_confidence, 0, 'f', 2)
        .arg(r.frames_pose_anomaly)
        .arg(r.gaze_fail_rate * 100.0f, 0, 'f', 1));

    grade_dist_label_->setText(QStringLiteral(
        "<span style='color:#2196F3;'>S级: %1 (%2%)</span>  "
        "<span style='color:#4CAF50;'>A级: %3 (%4%)</span>  "
        "<span style='color:#FF9800;'>B级: %5 (%6%)</span>  "
        "<span style='color:#FF5722;'>C级: %7 (%8%)</span>  "
        "<span style='color:#F44336;'>D级: %9 (%10%)</span>")
        .arg(r.grade_s).arg(pct(r.grade_s), 0, 'f', 1)
        .arg(r.grade_a).arg(pct(r.grade_a), 0, 'f', 1)
        .arg(r.grade_b).arg(pct(r.grade_b), 0, 'f', 1)
        .arg(r.grade_c).arg(pct(r.grade_c), 0, 'f', 1)
        .arg(r.grade_d).arg(pct(r.grade_d), 0, 'f', 1));

    auto* layout = content_widget_->layout();

    // Remove stretch before adding charts
    auto* stretch = layout->itemAt(layout->count() - 1);
    if (stretch && stretch->spacerItem()) {
        layout->takeAt(layout->count() - 1);
        delete stretch;
    }

    // Build charts
    BuildConfidenceChart(r);
    BuildConfidenceHistogram(r);
    BuildPoseChart(r);
    BuildGazeChart(r);
    BuildAuBarChart(r);
    BuildGradePie(r);
    BuildScorecard(r);

    static_cast<QBoxLayout*>(layout)->addStretch();
}

void QualityReportWidget::BuildConfidenceChart(const quality::OpenFaceQualityReport& r) {
    auto* series = new QLineSeries();
    series->setName(QStringLiteral("置信度"));
    series->setColor(ChartLine(0));
    for (const auto& f : r.frames)
        series->append(f.frame_number, f.confidence);

    auto* threshold = new QLineSeries();
    threshold->setName(QStringLiteral("阈值 (0.725)"));
    threshold->setColor(QColor(0xF4, 0x43, 0x36));
    threshold->setPen(QPen(QColor(0xF4, 0x43, 0x36), 1, Qt::DashLine));
    if (!r.frames.empty()) {
        threshold->append(r.frames.front().frame_number, 0.725);
        threshold->append(r.frames.back().frame_number, 0.725);
    }

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->addSeries(threshold);
    chart->createDefaultAxes();
    chart->setTitle(QStringLiteral("检测置信度时间线"));
    chart->axes(Qt::Vertical).front()->setRange(0, 1.05);

    auto* gb = new QGroupBox(content_widget_);
    auto* gl = new QVBoxLayout(gb);
    gl->addWidget(MakeChartView(chart));
    gl->addWidget(MakeExplainLabel(QStringLiteral(
        "说明：检测置信度（confidence）反映OpenFace对当前帧人脸跟踪的确定程度，"
        "值越接近1.0越可靠。红色虚线为OpenFace内部判定阈值0.725——"
        "低于此线的帧通常表示跟踪失败或人脸质量不足以进行可靠分析。"
        "波动剧烈或持续低置信度表明视频中的人脸检测可靠性整体较差，"
        "常见原因包括：光照过暗、面部遮挡严重、运动模糊或人脸角度过大。"), gb));
    content_widget_->layout()->addWidget(gb);
}

void QualityReportWidget::BuildConfidenceHistogram(const quality::OpenFaceQualityReport& r) {
    auto* set = new QBarSet(QStringLiteral("帧数"));
    set->setColor(ChartLine(0));
    for (float v : r.confidence_histogram)
        *set << v;

    auto* series = new QBarSeries();
    series->append(set);

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("置信度分布"));
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setVisible(false);

    auto* axisX = new QValueAxis();
    axisX->setRange(0, 9);
    axisX->setLabelFormat(QStringLiteral("%.1f"));
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis();
    axisY->setLabelFormat(QStringLiteral("%d"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    auto* gb = new QGroupBox(content_widget_);
    auto* gl = new QVBoxLayout(gb);
    gl->addWidget(MakeChartView(chart));
    gl->addWidget(MakeExplainLabel(QStringLiteral(
        "说明：置信度分布直方图展示所有帧的confidence值在各区间的分布情况。"
        "理想情况下绝大部分帧应集中在0.9以上的高区间。若大量帧分布在低置信度区间"
        "（<0.7），说明视频中人脸检测的可靠性整体偏低。"
        "分布越偏左、越分散，数据质量越差。"), gb));
    content_widget_->layout()->addWidget(gb);
}

void QualityReportWidget::BuildPoseChart(const quality::OpenFaceQualityReport& r) {
    const char* names[] = {"pitch", "yaw", "roll"};
    auto* chart = new QChart();

    for (int i = 0; i < 3; ++i) {
        auto* series = new QLineSeries();
        series->setName(QString::fromLatin1(names[i]));
        series->setColor(ChartLine(i));
        for (const auto& f : r.frames) {
            float val = (i == 0) ? f.pitch_deg : (i == 1) ? f.yaw_deg : f.roll_deg;
            series->append(f.frame_number, val);
        }
        chart->addSeries(series);
    }

    chart->createDefaultAxes();
    chart->setTitle(QStringLiteral("头部姿态时间线 (度)"));

    auto* gb = new QGroupBox(content_widget_);
    auto* gl = new QVBoxLayout(gb);
    gl->addWidget(MakeChartView(chart));

    auto* stat = new QLabel(QStringLiteral(
        "姿态异常帧: %1 (%2%)  |  最大角速度: %3°/s  |  平均角速度: %4°/s")
        .arg(r.frames_pose_anomaly)
        .arg((r.total_frames > 0) ? 100.0f * r.frames_pose_anomaly / r.total_frames : 0, 0, 'f', 1)
        .arg(r.max_angular_velocity, 0, 'f', 1)
        .arg(r.mean_angular_velocity, 0, 'f', 1), gb);
    stat->setStyleSheet(QStringLiteral("QLabel { color: #aaa; font-size: 11px; padding: 2px 8px; }"));
    gl->addWidget(stat);
    gl->addWidget(MakeExplainLabel(QStringLiteral(
        "说明：三条曲线分别表示pitch（俯仰角，蓝色）、yaw（偏航角，橙色）、roll（翻滚角，绿色），"
        "单位为度。姿态角反映人脸在3D空间中的朝向。超出正常生理范围"
        "（|pitch|>60°、|yaw|>90°、|roll|>45°）的帧被标记为姿态异常帧，"
        "通常意味着姿态估计出现错误。帧间角速度突然剧烈变化也暗示跟踪不稳定。"), gb));
    content_widget_->layout()->addWidget(gb);
}

void QualityReportWidget::BuildGazeChart(const quality::OpenFaceQualityReport& r) {
    auto* series = new QLineSeries();
    series->setName(QStringLiteral("双眼视线夹角"));
    series->setColor(ChartLine(0));
    for (const auto& f : r.frames)
        series->append(f.frame_number, f.gaze_angle_diff);

    auto* chart = new QChart();
    chart->addSeries(series);

    for (auto [val, label] : {std::pair{5.0f, "5°"}, {15.0f, "15°"}}) {
        auto* t = new QLineSeries();
        t->setPen(QPen(val > 10.0f ? QColor(0xF4, 0x43, 0x36) : QColor(0xFF, 0x98, 0x00), 1, Qt::DashLine));
        if (!r.frames.empty()) {
            t->append(r.frames.front().frame_number, val);
            t->append(r.frames.back().frame_number, val);
        }
        chart->addSeries(t);
    }

    chart->createDefaultAxes();
    chart->setTitle(QStringLiteral("双眼视线一致性 (度)"));

    auto* gb = new QGroupBox(content_widget_);
    auto* gl = new QVBoxLayout(gb);
    gl->addWidget(MakeChartView(chart));

    auto* stat = new QLabel(QStringLiteral(
        "平均视线夹角: %1°  |  最大: %2°  |  视线失败: %3%")
        .arg(r.mean_gaze_diff, 0, 'f', 1)
        .arg(r.max_gaze_diff, 0, 'f', 1)
        .arg(r.gaze_fail_rate * 100.0f, 0, 'f', 1), gb);
    stat->setStyleSheet(QStringLiteral("QLabel { color: #aaa; font-size: 11px; padding: 2px 8px; }"));
    gl->addWidget(stat);
    gl->addWidget(MakeExplainLabel(QStringLiteral(
        "说明：双眼视线一致性反映左右眼视线方向估计的吻合程度。正常注视时双眼视线"
        "几乎平行（接近0°）。黄色虚线（5°）标注轻微不一致，红色虚线（15°）"
        "标注严重不一致。角度越大表示视线估计越不可靠，视线失败帧（输出(0,0,-1)占位值）"
        "不计入统计。"), gb));
    content_widget_->layout()->addWidget(gb);
}

void QualityReportWidget::BuildGradePie(const quality::OpenFaceQualityReport& r) {
    auto* series = new QPieSeries();
    series->setPieSize(0.45);  // compact pie to leave room for labels

    auto addSlice = [&](const QString& name, int count, char g) {
        if (count <= 0) return;
        auto* slice = series->append(name, count);
        slice->setColor(GradeColor(g));
        float pct = (r.total_frames > 0) ? 100.0f * count / r.total_frames : 0;
        // Only attach outer labels to slices ≥ 5% to avoid overlap;
        // smaller slices are still visible via legend + hover
        if (pct >= 5.0f) {
            slice->setLabelVisible(true);
            slice->setLabelPosition(QPieSlice::LabelOutside);
            slice->setLabel(QStringLiteral("%1: %2帧 (%3%)")
                .arg(name).arg(count).arg(pct, 0, 'f', 1));
        } else {
            slice->setLabelVisible(false);
        }
    };

    addSlice(QStringLiteral("S级"), r.grade_s, 'S');
    addSlice(QStringLiteral("A级"), r.grade_a, 'A');
    addSlice(QStringLiteral("B级"), r.grade_b, 'B');
    addSlice(QStringLiteral("C级"), r.grade_c, 'C');
    addSlice(QStringLiteral("D级"), r.grade_d, 'D');

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("帧质量等级分布"));
    chart->legend()->setAlignment(Qt::AlignRight);

    auto* gb = new QGroupBox(content_widget_);
    auto* gl = new QVBoxLayout(gb);

    auto* view = MakeChartView(chart);
    view->setMinimumHeight(260);
    view->setMaximumHeight(380);
    gl->addWidget(view);

    gl->addWidget(MakeExplainLabel(QStringLiteral(
        "评判标准：每帧按6个维度加权评分（满分100），分项权重为：置信度30分、"
        "可见关键点数15分、姿态合理性20分、视线一致性15分、"
        "帧间角速度10分、AU质量10分。各维度内部按经验阈值分段给分（如置信度"
        "≥0.98得30分，≥0.95得28分，≥0.90得25分，≥0.85得22分，≥0.80得18分，"
        "≥0.75得13分，≥0.725得8分，<0.725得0分），加权求和后按总分映射等级："
        "S≥93, A≥85, B≥70, C≥50, D<50。"
        "仅占比≥5%的等级显示外围标签，所有等级均可通过右侧图例识别。"
        "注意：等级阈值基于OpenFace官方文档推荐及FACS生理约束设定，不同场景下"
        "可通过调整各维度分段阈值来适配特定数据特征。"), gb));
    content_widget_->layout()->addWidget(gb);
}

void QualityReportWidget::BuildAuBarChart(const quality::OpenFaceQualityReport& r) {
    auto* set = new QBarSet(QStringLiteral("翻转率"));
    set->setColor(ChartLine(1));

    int max_flips = 1;
    for (int c : r.au_flip_counts)
        max_flips = std::max(max_flips, c);

    int total_frames = std::max(1, r.total_frames);
    for (int c : r.au_flip_counts)
        *set << (static_cast<float>(c) / ((total_frames > 1) ? (total_frames - 1) : 1));

    auto* series = new QBarSeries();
    series->append(set);

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QStringLiteral("AU 翻转率 (按AU)"));
    chart->legend()->setVisible(false);

    QStringList au_labels;
    for (int au : {1,2,4,5,6,7,9,10,12,14,15,17,20,23,25,26,28,45})
        au_labels << QStringLiteral("AU%1").arg(au, 2, 10, QChar('0'));

    auto* axisX = new QValueAxis();
    axisX->setRange(0, static_cast<int>(au_labels.size()) - 1);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* gb = new QGroupBox(content_widget_);
    auto* gl = new QVBoxLayout(gb);
    gl->addWidget(MakeChartView(chart));

    auto* stat = new QLabel(QStringLiteral(
        "总体AU翻转率: %1  |  FACS违规帧: %2 (%3%)")
        .arg(r.overall_au_flip_rate, 0, 'f', 3)
        .arg(r.frames_facs_violation)
        .arg((r.total_frames > 0) ? 100.0f * r.frames_facs_violation / r.total_frames : 0, 0, 'f', 1), gb);
    stat->setStyleSheet(QStringLiteral("QLabel { color: #aaa; font-size: 11px; padding: 2px 8px; }"));
    gl->addWidget(stat);
    gl->addWidget(MakeExplainLabel(QStringLiteral(
        "说明：AU翻转率（Action Unit Flip Rate）反映面部动作单元检测结果的帧间稳定性。"
        "翻转率越高表示AU检测在相邻帧之间频繁振荡（'开关效应'），通常是信号噪声的表现。"
        "FACS约束违规指检测到了物理上不可能同时发生的AU组合"
        "（如AU12嘴角提升与AU15嘴角下压同时激活、AU1眉心上提与AU4眉毛下压同时激活、"
        "AU26下颌下沉但AU25嘴唇未分离）。"), gb));
    content_widget_->layout()->addWidget(gb);
}

void QualityReportWidget::BuildScorecard(const quality::OpenFaceQualityReport& r) {
    auto& tm = ThemeManager::Instance();

    // Rate a dimension contribution score against its max
    // ratio >= 93% → S, >= 85% → A, >= 70% → B, >= 50% → C, else D
    auto dimGrade = [](float score, float max_score) -> QString {
        if (max_score <= 0) return QStringLiteral("<span style='color:#888;'>N/A</span>");
        float ratio = score / max_score;
        if (ratio >= 0.93f) return QStringLiteral("<span style='color:#2196F3;'>S</span>");
        if (ratio >= 0.85f) return QStringLiteral("<span style='color:#4CAF50;'>A</span>");
        if (ratio >= 0.70f) return QStringLiteral("<span style='color:#FF9800;'>B</span>");
        if (ratio >= 0.50f) return QStringLiteral("<span style='color:#FF5722;'>C</span>");
        return QStringLiteral("<span style='color:#F44336;'>D</span>");
    };

    // Compute mean total score
    float mean_score = 0;
    for (const auto& f : r.frames) mean_score += f.score;
    if (r.total_frames > 0) mean_score /= r.total_frames;

    QString html;
    html += QStringLiteral("<h3 style='color:%1;margin-bottom:6px;'>详细评分卡</h3>").arg(tm.Primary());
    html += QStringLiteral("<table style='width:100%;border-collapse:collapse;color:%1;font-size:13px;'>")
        .arg(tm.TextPrimary());
    html += QStringLiteral("<tr style='border-bottom:1px solid %1;'>"
        "<th style='text-align:left;padding:6px 24px 6px 8px;'>维度</th>"
        "<th style='text-align:right;padding:6px 24px 6px 8px;'>平均得分 / 满分</th>"
        "<th style='text-align:center;padding:6px 16px 6px 8px;'>评级</th></tr>")
        .arg(tm.Border());

    auto row = [&](const QString& name, float score, float maxv) {
        QString val_str = QStringLiteral("%1 / %2").arg(score, 0, 'f', 1).arg(static_cast<int>(maxv));
        html += QStringLiteral("<tr style='border-bottom:1px solid %1;'>"
            "<td style='padding:5px 24px 5px 8px;'>%2</td>"
            "<td style='text-align:right;padding:5px 24px 5px 8px;'>%3</td>"
            "<td style='text-align:center;padding:5px 16px 5px 8px;'>%4</td></tr>")
            .arg(tm.Border(), name, val_str, dimGrade(score, maxv));
    };
    auto section = [&](const QString& title) {
        html += QStringLiteral("<tr><td colspan='3' style='padding:8px 8px 3px 8px;"
            "color:%1;font-weight:bold;font-size:12px;'>%2</td></tr>")
            .arg(tm.Primary(), title);
    };

    // === Overall ===
    section(QStringLiteral("综合"));
    row(QStringLiteral("  逐帧加权平均"), mean_score, 100.0f);

    // === Layer 1: Landmark quality (45 pts) ===
    section(QStringLiteral("第1层：关键点质量（满分45）"));
    row(QStringLiteral("  置信度贡献"), r.mean_score_confidence, 30.0f);
    row(QStringLiteral("  可见关键点贡献"), r.mean_score_visible, 15.0f);

    // === Layer 2: Pose quality (30 pts) ===
    section(QStringLiteral("第2层：头部姿态质量（满分30）"));
    row(QStringLiteral("  姿态贡献"), r.mean_score_pose, 20.0f);
    row(QStringLiteral("  角速度贡献"), r.mean_score_angular, 10.0f);

    // === Layer 3: Gaze quality (15 pts) ===
    section(QStringLiteral("第3层：视线质量（满分15）"));
    row(QStringLiteral("  视线贡献"), r.mean_score_gaze, 15.0f);

    // === Layer 4: AU quality (10 pts) ===
    section(QStringLiteral("第4层：AU质量（满分10）"));
    row(QStringLiteral("  AU贡献"), r.mean_score_au, 10.0f);

    // === Detail rows: raw metric values for diagnosis ===
    section(QStringLiteral("明细数据（原始统计值，供诊断参考）"));
    auto detailRow = [&](const QString& name, const QString& val) {
        html += QStringLiteral("<tr style='border-bottom:1px solid %1;'>"
            "<td style='padding:3px 24px 3px 16px;color:#999;font-size:11px;'>%2</td>"
            "<td style='text-align:right;padding:3px 24px 3px 8px;color:#aaa;font-size:11px;'>%3</td>"
            "<td></td></tr>")
            .arg(tm.Border(), name, val);
    };

    auto pct_of = [&](int count) -> float {
        return (r.total_frames > 0) ? 100.0f * count / r.total_frames : 0;
    };

    detailRow(QStringLiteral("平均置信度"), QString::number(r.mean_confidence, 'f', 3));
    detailRow(QStringLiteral("置信度标准差"), QString::number(r.std_confidence, 'f', 3));
    detailRow(QStringLiteral("平均可见关键点数"), QString::number(r.mean_visible_count, 'f', 1) + QStringLiteral(" / 68"));
    detailRow(QStringLiteral("平均帧间角速度"), QString::number(r.mean_angular_velocity, 'f', 1) + QStringLiteral(" °/帧"));
    detailRow(QStringLiteral("姿态异常帧占比"), QString::number(pct_of(r.frames_pose_anomaly), 'f', 1) + QStringLiteral("%"));
    detailRow(QStringLiteral("平均视线夹角"), QString::number(r.mean_gaze_diff, 'f', 1) + QStringLiteral("°"));
    detailRow(QStringLiteral("视线失败率"), QString::number(r.gaze_fail_rate * 100.0f, 'f', 1) + QStringLiteral("%"));
    detailRow(QStringLiteral("AU翻转率"), QString::number(r.overall_au_flip_rate, 'f', 3));
    detailRow(QStringLiteral("FACS违规帧占比"), QString::number(pct_of(r.frames_facs_violation), 'f', 1) + QStringLiteral("%"));

    html += QStringLiteral("</table>");

    auto* edit = new QTextEdit(content_widget_);
    edit->setReadOnly(true);
    edit->setHtml(html);
    edit->setMaximumHeight(320);
    edit->setStyleSheet(QStringLiteral(
        "QTextEdit { background: transparent; border: 1px solid %1;"
        "  border-radius: 4px; padding: 8px; }").arg(tm.Border()));
    content_widget_->layout()->addWidget(edit);
}

void QualityReportWidget::RenderMediaPipeReport(const quality::MediaPipeQualityReport& r) {
    Clear();

    auto& tm = ThemeManager::Instance();

    auto pct = [&](int c) { return (r.total_frames > 0) ? 100.0f * c / r.total_frames : 0; };

    float mean_score = 0;
    for (const auto& f : r.frames) mean_score += f.score;
    if (r.total_frames > 0) mean_score /= r.total_frames;

    char grade;
    if (mean_score >= 93.0f)       grade = 'S';
    else if (mean_score >= 85.0f)  grade = 'A';
    else if (mean_score >= 70.0f)  grade = 'B';
    else if (mean_score >= 50.0f)  grade = 'C';
    else                            grade = 'D';

    QColor grColor = GradeColor(grade);

    summary_label_->setText(QStringLiteral(
        "<h2 style='margin:0;color:%1;'>综合评分: %2 (均分 %3/100)</h2>"
        "<p style='margin:4px 0;'>总帧数: %4 | NRE: %5% | 重投影: %6px | 姿态异常: %7帧</p>")
        .arg(grColor.name(), GradeLabel(grade))
        .arg(mean_score, 0, 'f', 1)
        .arg(r.total_frames)
        .arg(r.mean_nre, 0, 'f', 2)
        .arg(r.mean_reproj_rms, 0, 'f', 2)
        .arg(r.frames_pose_anomaly));

    grade_dist_label_->setText(QStringLiteral(
        "<span style='color:#2196F3;'>S级: %1 (%2%)</span>  "
        "<span style='color:#4CAF50;'>A级: %3 (%4%)</span>  "
        "<span style='color:#FF9800;'>B级: %5 (%6%)</span>  "
        "<span style='color:#FF5722;'>C级: %7 (%8%)</span>  "
        "<span style='color:#F44336;'>D级: %9 (%10%)</span>")
        .arg(r.grade_s).arg(pct(r.grade_s), 0, 'f', 1)
        .arg(r.grade_a).arg(pct(r.grade_a), 0, 'f', 1)
        .arg(r.grade_b).arg(pct(r.grade_b), 0, 'f', 1)
        .arg(r.grade_c).arg(pct(r.grade_c), 0, 'f', 1)
        .arg(r.grade_d).arg(pct(r.grade_d), 0, 'f', 1));

    auto* layout = content_widget_->layout();

    auto* stretch = layout->itemAt(layout->count() - 1);
    if (stretch && stretch->spacerItem()) {
        layout->takeAt(layout->count() - 1);
        delete stretch;
    }

    // Build MediaPipe-specific report as HTML
    auto* edit = new QTextEdit(content_widget_);
    edit->setReadOnly(true);

    QString html;
    html += QStringLiteral("<h3 style='color:%1;margin:12px 0 6px;'>维度评分明细</h3>").arg(tm.Primary());
    html += QStringLiteral("<table style='width:100%;border-collapse:collapse;color:%1;font-size:12px;'>")
        .arg(tm.TextPrimary());
    html += QStringLiteral("<tr style='background:rgba(255,255,255,0.04);'>"
        "<th style='padding:6px 8px;text-align:left;'>评估维度</th>"
        "<th style='padding:6px 8px;text-align:right;'>满分</th>"
        "<th style='padding:6px 8px;text-align:right;'>均值</th>"
        "<th style='padding:6px 8px;text-align:right;'>得分率</th></tr>");

    auto row = [&](const QString& name, int max_score, float mean) {
        float rate = (max_score > 0) ? 100.0f * mean / max_score : 0;
        QString color = (rate >= 90) ? "#4CAF50" : (rate >= 70) ? "#FFC107" : "#F44336";
        html += QStringLiteral("<tr><td style='padding:6px 8px;'>%1</td>"
            "<td style='padding:6px 8px;text-align:right;'>%2</td>"
            "<td style='padding:6px 8px;text-align:right;'>%3</td>"
            "<td style='padding:6px 8px;text-align:right;color:%4;'>%5%</td></tr>")
            .arg(name).arg(max_score).arg(mean, 0, 'f', 1).arg(color).arg(rate, 0, 'f', 0);
    };

    row(QStringLiteral("几何精度 (NRE)"), 50, r.mean_score_geo);
    row(QStringLiteral("姿态合理性 (范围+角速度)"), 50, r.mean_score_pose);
    html += QStringLiteral("</table>");

    // Metrics detail table
    html += QStringLiteral("<h3 style='color:%1;margin:16px 0 6px;'>关键指标统计</h3>").arg(tm.Primary());
    html += QStringLiteral("<table style='width:100%;border-collapse:collapse;color:%1;font-size:12px;'>")
        .arg(tm.TextPrimary());

    auto met = [&](const QString& name, const QString& value, const QString& comment) {
        html += QStringLiteral("<tr><td style='padding:4px 8px;'>%1</td>"
            "<td style='padding:4px 8px;text-align:right;font-weight:bold;'>%2</td>"
            "<td style='padding:4px 8px;color:%3;font-size:11px;'>%4</td></tr>")
            .arg(name, value, tm.TextMuted(), comment);
    };

    met(QStringLiteral("几何精度均分"), QString::number(r.mean_score_geo, 'f', 1) + QStringLiteral("/50"),
        r.mean_score_geo >= 40.0f ? QStringLiteral("优秀") : r.mean_score_geo >= 25.0f ? QStringLiteral("良好") : QStringLiteral("较差"));
    met(QStringLiteral("姿态合理性均分"), QString::number(r.mean_score_pose, 'f', 1) + QStringLiteral("/50"),
        r.mean_score_pose >= 40.0f ? QStringLiteral("优秀") : r.mean_score_pose >= 25.0f ? QStringLiteral("正常") : QStringLiteral("需关注"));
    met(QStringLiteral("平均NRE"), QString::number(r.mean_nre, 'f', 2) + QStringLiteral("%"),
        r.mean_nre < 1.0f ? QStringLiteral("几何精度高") : r.mean_nre < 2.0f ? QStringLiteral("几何精度良好") : r.mean_nre < 3.5f ? QStringLiteral("几何精度一般") : QStringLiteral("几何精度差"));
    met(QStringLiteral("平均重投影误差"), QString::number(r.mean_reproj_rms, 'f', 2) + QStringLiteral("px"),
        r.mean_reproj_rms < 3.0f ? QStringLiteral("几何精度高") : r.mean_reproj_rms < 8.0f ? QStringLiteral("几何精度一般") : QStringLiteral("几何精度差"));
    met(QStringLiteral("最大重投影误差"), QString::number(r.max_reproj_rms, 'f', 2) + QStringLiteral("px"), QString());
    met(QStringLiteral("最大NRE"), QString::number(r.max_nre, 'f', 2) + QStringLiteral("%"), QString());
    met(QStringLiteral("平均角速度"), QString::number(r.mean_angular_velocity, 'f', 1) + QStringLiteral("°/s"),
        r.mean_angular_velocity < 120.0f ? QStringLiteral("姿态平滑") : QStringLiteral("姿态抖动"));
    met(QStringLiteral("最大角速度"), QString::number(r.max_angular_velocity, 'f', 1) + QStringLiteral("°/s"), QString());
    met(QStringLiteral("姿态异常帧"), QString::number(r.frames_pose_anomaly)
        + QStringLiteral(" (%1%)").arg(pct(r.frames_pose_anomaly), 0, 'f', 1),
        r.frames_pose_anomaly == 0 ? QStringLiteral("无异常") : QStringLiteral("需关注"));
    html += QStringLiteral("</table>");

    edit->setHtml(html);
    edit->setStyleSheet(QStringLiteral(
        "QTextEdit { background: transparent; border: none; }"));
    layout->addWidget(edit);

    static_cast<QVBoxLayout*>(layout)->addStretch();
}

void QualityReportWidget::RenderL2csReport(const quality::L2csQualityReport& r) {
    Clear();

    auto& tm = ThemeManager::Instance();

    auto pct = [&](int c) { return (r.total_frames > 0) ? 100.0f * c / r.total_frames : 0; };

    float mean_score = 0;
    for (const auto& f : r.frames) mean_score += f.score;
    if (r.total_frames > 0) mean_score /= r.total_frames;

    char grade;
    if (mean_score >= 93.0f)       grade = 'S';
    else if (mean_score >= 85.0f)  grade = 'A';
    else if (mean_score >= 70.0f)  grade = 'B';
    else if (mean_score >= 50.0f)  grade = 'C';
    else                            grade = 'D';

    QColor grColor = GradeColor(grade);

    summary_label_->setText(QStringLiteral(
        "<h2 style='margin:0;color:%1;'>综合评分: %2 (均分 %3/100)</h2>"
        "<p style='margin:4px 0;'>总帧数: %4 | 平均max_prob: %5 | 平均角速度: %6/s</p>")
        .arg(grColor.name(), GradeLabel(grade))
        .arg(mean_score, 0, 'f', 1)
        .arg(r.total_frames)
        .arg(r.mean_maxp, 0, 'f', 3)
        .arg(r.mean_angular_velocity, 0, 'f', 1));

    grade_dist_label_->setText(QStringLiteral(
        "<span style='color:#2196F3;'>S级: %1 (%2%)</span>  "
        "<span style='color:#4CAF50;'>A级: %3 (%4%)</span>  "
        "<span style='color:#FF9800;'>B级: %5 (%6%)</span>  "
        "<span style='color:#FF5722;'>C级: %7 (%8%)</span>  "
        "<span style='color:#F44336;'>D级: %9 (%10%)</span>")
        .arg(r.grade_s).arg(pct(r.grade_s), 0, 'f', 1)
        .arg(r.grade_a).arg(pct(r.grade_a), 0, 'f', 1)
        .arg(r.grade_b).arg(pct(r.grade_b), 0, 'f', 1)
        .arg(r.grade_c).arg(pct(r.grade_c), 0, 'f', 1)
        .arg(r.grade_d).arg(pct(r.grade_d), 0, 'f', 1));

    auto* layout = content_widget_->layout();

    auto* stretch = layout->itemAt(layout->count() - 1);
    if (stretch && stretch->spacerItem()) {
        layout->takeAt(layout->count() - 1);
        delete stretch;
    }

    // Build maxp time series chart
    {
        auto* yawSeries = new QLineSeries();
        yawSeries->setName(QStringLiteral("yaw_maxp"));
        yawSeries->setColor(ChartLine(0));
        auto* pitchSeries = new QLineSeries();
        pitchSeries->setName(QStringLiteral("pitch_maxp"));
        pitchSeries->setColor(ChartLine(1));
        for (const auto& f : r.frames) {
            yawSeries->append(f.frame_number, f.yaw_maxp);
            pitchSeries->append(f.frame_number, f.pitch_maxp);
        }

        auto* chart = new QChart();
        chart->addSeries(yawSeries);
        chart->addSeries(pitchSeries);
        chart->createDefaultAxes();
        chart->setTitle(QStringLiteral("Softmax 峰值概率时间线"));

        auto* gb = new QGroupBox(content_widget_);
        auto* gl = new QVBoxLayout(gb);
        gl->addWidget(MakeChartView(chart));
        gl->addWidget(MakeExplainLabel(QStringLiteral(
            "说明：yaw_maxp和pitch_maxp是90-bin softmax分布的峰值概率。"
            "注意90分类的均匀分布基线 ≈ 0.011（1/90），因此0.15就已将概率质量集中了约13倍。"
            "不同模型（MobileNetV2、ResNet50、MobileOne-S0）的max_prob分布差异较大，"
            "质量评级采用百分位阈值自动适配，无需关心具体数值。"), gb));
        layout->addWidget(gb);
    }

    // Build gaze time series chart
    {
        auto* yawSeries = new QLineSeries();
        yawSeries->setName(QStringLiteral("gaze_yaw"));
        yawSeries->setColor(ChartLine(0));
        auto* pitchSeries = new QLineSeries();
        pitchSeries->setName(QStringLiteral("gaze_pitch"));
        pitchSeries->setColor(ChartLine(1));
        for (const auto& f : r.frames) {
            yawSeries->append(f.frame_number, f.yaw_deg);
            pitchSeries->append(f.frame_number, f.pitch_deg);
        }

        auto* chart = new QChart();
        chart->addSeries(yawSeries);
        chart->addSeries(pitchSeries);
        chart->createDefaultAxes();
        chart->setTitle(QStringLiteral("视线角度时间线 (度)"));

        auto* gb = new QGroupBox(content_widget_);
        auto* gl = new QVBoxLayout(gb);
        gl->addWidget(MakeChartView(chart));
        gl->addWidget(MakeExplainLabel(QStringLiteral(
            "说明：gaze_yaw（水平偏转角）和gaze_pitch（垂直偏转角）经过双重EMA平滑"
            "（bbox α=0.25 + gaze α=0.2）。注意模型输出是视线方向（gaze direction），"
            "而非头部姿态。生理范围约|yaw|<60°, |pitch|<50°。"), gb));
        layout->addWidget(gb);
    }

    // Build angular velocity chart
    {
        auto* series = new QLineSeries();
        series->setName(QStringLiteral("视线角速度"));
        series->setColor(ChartLine(2));
        for (const auto& f : r.frames)
            series->append(f.frame_number, f.angular_velocity);

        auto* chart = new QChart();
        chart->addSeries(series);
        chart->createDefaultAxes();
        chart->setTitle(QStringLiteral("视线角速度 (°/s)"));

        auto* gb = new QGroupBox(content_widget_);
        auto* gl = new QVBoxLayout(gb);
        gl->addWidget(MakeChartView(chart));
        gl->addWidget(MakeExplainLabel(QStringLiteral(
            "说明：视线角速度反映相邻帧之间视线方向的变化速率。低角速度表示视线稳定，"
            "高角速度可能表示快速扫视（saccade）或模型输出噪声。"
            "EMA平滑会导致角速度被低估约5帧（α=0.2时）。"), gb));
        layout->addWidget(gb);
    }

    // Scorecard HTML
    {
        auto* edit = new QTextEdit(content_widget_);
        edit->setReadOnly(true);

        QString html;
        html += QStringLiteral("<h3 style='color:%1;margin:12px 0 6px;'>维度评分明细</h3>").arg(tm.Primary());
        html += QStringLiteral("<table style='width:100%;border-collapse:collapse;color:%1;font-size:12px;'>")
            .arg(tm.TextPrimary());
        html += QStringLiteral("<tr style='background:rgba(255,255,255,0.04);'>"
            "<th style='padding:6px 8px;text-align:left;'>评估维度</th>"
            "<th style='padding:6px 8px;text-align:right;'>满分</th>"
            "<th style='padding:6px 8px;text-align:right;'>均值</th>"
            "<th style='padding:6px 8px;text-align:right;'>得分率</th></tr>");

        auto row = [&](const QString& name, int max_score, float mean) {
            float rate = (max_score > 0) ? 100.0f * mean / max_score : 0;
            QString color = (rate >= 90) ? "#4CAF50" : (rate >= 70) ? "#FFC107" : "#F44336";
            html += QStringLiteral("<tr><td style='padding:6px 8px;'>%1</td>"
                "<td style='padding:6px 8px;text-align:right;'>%2</td>"
                "<td style='padding:6px 8px;text-align:right;'>%3</td>"
                "<td style='padding:6px 8px;text-align:right;color:%4;'>%5%</td></tr>")
                .arg(name).arg(max_score).arg(mean, 0, 'f', 1).arg(color).arg(rate, 0, 'f', 0);
        };

        row(QStringLiteral("预测置信度 (max_prob)"), 50, r.mean_score_conf);
        row(QStringLiteral("视线稳定性 (角速度+范围)"), 50, r.mean_score_stab);
        html += QStringLiteral("</table>");

        html += QStringLiteral("<h3 style='color:%1;margin:16px 0 6px;'>关键指标统计</h3>").arg(tm.Primary());
        html += QStringLiteral("<table style='width:100%;border-collapse:collapse;color:%1;font-size:12px;'>")
            .arg(tm.TextPrimary());

        auto met = [&](const QString& name, const QString& value, const QString& comment) {
            html += QStringLiteral("<tr><td style='padding:4px 8px;'>%1</td>"
                "<td style='padding:4px 8px;text-align:right;font-weight:bold;'>%2</td>"
                "<td style='padding:4px 8px;color:%3;font-size:11px;'>%4</td></tr>")
                .arg(name, value, tm.TextMuted(), comment);
        };

        met(QStringLiteral("置信度均分"), QString::number(r.mean_score_conf, 'f', 1) + QStringLiteral("/50"),
            r.mean_score_conf >= 40.0f ? QStringLiteral("优秀") : r.mean_score_conf >= 25.0f ? QStringLiteral("良好") : QStringLiteral("较差"));
        met(QStringLiteral("稳定性均分"), QString::number(r.mean_score_stab, 'f', 1) + QStringLiteral("/50"),
            r.mean_score_stab >= 40.0f ? QStringLiteral("优秀") : r.mean_score_stab >= 25.0f ? QStringLiteral("正常") : QStringLiteral("需关注"));
        met(QStringLiteral("平均max_prob"), QString::number(r.mean_maxp, 'f', 3),
            r.mean_maxp >= 0.3f ? QStringLiteral("预测置信度高") : r.mean_maxp >= 0.1f ? QStringLiteral("中等") : QStringLiteral("低置信度"));
        met(QStringLiteral("平均视线角速度"), QString::number(r.mean_angular_velocity, 'f', 1) + QStringLiteral("°/s"),
            r.mean_angular_velocity < 60.0f ? QStringLiteral("视线稳定") : QStringLiteral("视线波动"));
        met(QStringLiteral("最大视线角速度"), QString::number(r.max_angular_velocity, 'f', 1) + QStringLiteral("°/s"), QString());
        html += QStringLiteral("</table>");

        edit->setHtml(html);
        edit->setStyleSheet(QStringLiteral(
            "QTextEdit { background: transparent; border: none; }"));
        layout->addWidget(edit);
    }

    static_cast<QVBoxLayout*>(layout)->addStretch();
}

void QualityReportWidget::RenderMefargAuReport(const quality::MefargAuQualityReport& r) {
    Clear();

    auto& tm = ThemeManager::Instance();

    auto pct = [&](int c) { return (r.total_frames > 0) ? 100.0f * c / r.total_frames : 0; };

    float mean_score = 0;
    for (const auto& f : r.frames) mean_score += f.score;
    if (r.total_frames > 0) mean_score /= r.total_frames;

    char grade;
    if (mean_score >= 93.0f)       grade = 'S';
    else if (mean_score >= 85.0f)  grade = 'A';
    else if (mean_score >= 70.0f)  grade = 'B';
    else if (mean_score >= 50.0f)  grade = 'C';
    else                            grade = 'D';

    QColor grColor = GradeColor(grade);

    summary_label_->setText(QStringLiteral(
        "<h2 style='margin:0;color:%1;'>综合评分: %2 (均分 %3/100)</h2>"
        "<p style='margin:4px 0;'>总帧数: %4 | 平均判决策略距离: %5 | AU翻转率: %6 | 脉冲帧: %7</p>")
        .arg(grColor.name(), GradeLabel(grade))
        .arg(mean_score, 0, 'f', 1)
        .arg(r.total_frames)
        .arg(r.mean_decision_margin, 0, 'f', 3)
        .arg(r.overall_au_flip_rate, 0, 'f', 3)
        .arg(r.total_spike_frames));

    grade_dist_label_->setText(QStringLiteral(
        "<span style='color:#2196F3;'>S级: %1 (%2%)</span>  "
        "<span style='color:#4CAF50;'>A级: %3 (%4%)</span>  "
        "<span style='color:#FF9800;'>B级: %5 (%6%)</span>  "
        "<span style='color:#FF5722;'>C级: %7 (%8%)</span>  "
        "<span style='color:#F44336;'>D级: %9 (%10%)</span>")
        .arg(r.grade_s).arg(pct(r.grade_s), 0, 'f', 1)
        .arg(r.grade_a).arg(pct(r.grade_a), 0, 'f', 1)
        .arg(r.grade_b).arg(pct(r.grade_b), 0, 'f', 1)
        .arg(r.grade_c).arg(pct(r.grade_c), 0, 'f', 1)
        .arg(r.grade_d).arg(pct(r.grade_d), 0, 'f', 1));

    auto* layout = content_widget_->layout();

    auto* stretch = layout->itemAt(layout->count() - 1);
    if (stretch && stretch->spacerItem()) {
        layout->takeAt(layout->count() - 1);
        delete stretch;
    }

    // Decision margin time series
    {
        auto* series = new QLineSeries();
        series->setName(QStringLiteral("判决策略距离"));
        series->setColor(ChartLine(0));
        for (const auto& f : r.frames)
            series->append(f.frame_number, f.mean_decision_margin);

        auto* threshold = new QLineSeries();
        threshold->setPen(QPen(QColor(0xF4, 0x43, 0x36), 1, Qt::DashLine));
        if (!r.frames.empty()) {
            threshold->append(r.frames.front().frame_number, 0.1f);
            threshold->append(r.frames.back().frame_number, 0.1f);
        }

        auto* chart = new QChart();
        chart->addSeries(series);
        chart->addSeries(threshold);
        chart->createDefaultAxes();
        chart->setTitle(QStringLiteral("判决策略距离时间线 |cosine - 0.5|"));

        auto* gb = new QGroupBox(content_widget_);
        auto* gl = new QVBoxLayout(gb);
        gl->addWidget(MakeChartView(chart));
        gl->addWidget(MakeExplainLabel(QStringLiteral(
            "说明：判决策略距离 = |cosine - 0.5|（41个AU的均值），反映余弦相似度到激活阈值0.5的几何距离。"
            "值越大表示AU激活判定越确定。红色虚线标注0.1（低置信度阈值）。"
            "接近0表示余弦值在阈值附近，AU激活判定对输入噪声高度敏感。"), gb));
        layout->addWidget(gb);
    }

    // Active AU count time series
    {
        auto* series = new QLineSeries();
        series->setName(QStringLiteral("激活AU数"));
        series->setColor(ChartLine(1));
        for (const auto& f : r.frames)
            series->append(f.frame_number, f.active_au_count);

        auto* chart = new QChart();
        chart->addSeries(series);
        chart->createDefaultAxes();
        chart->setTitle(QStringLiteral("每帧激活AU数量"));

        auto* gb = new QGroupBox(content_widget_);
        auto* gl = new QVBoxLayout(gb);
        gl->addWidget(MakeChartView(chart));
        gl->addWidget(MakeExplainLabel(QStringLiteral(
            "说明：每帧激活的AU数量（余弦值≥0.5）。正常表情通常激活2-8个AU。"
            "0-1个表示中性脸或检测失效；>15个极可能是过检测/噪声。"
            "帧间大幅跳变（>10个AU变化）被标记为脉冲噪声。"), gb));
        layout->addWidget(gb);
    }

    // AU flips chart
    {
        auto* series = new QLineSeries();
        series->setName(QStringLiteral("AU翻转数"));
        series->setColor(ChartLine(2));
        for (const auto& f : r.frames)
            series->append(f.frame_number, f.au_flips);

        auto* chart = new QChart();
        chart->addSeries(series);
        chart->createDefaultAxes();
        chart->setTitle(QStringLiteral("帧间AU状态翻转数"));

        auto* gb = new QGroupBox(content_widget_);
        auto* gl = new QVBoxLayout(gb);
        gl->addWidget(MakeChartView(chart));
        gl->addWidget(MakeExplainLabel(QStringLiteral(
            "说明：相邻帧之间AU激活状态发生变化的数量（近似）。高翻转数表示AU检测"
            "存在严重的\"开关效应\"——模型在连续帧间反复改变判定。正常表情变化"
            "通常每帧只有0-3个AU状态翻转。"), gb));
        layout->addWidget(gb);
    }

    // Scorecard HTML
    {
        auto* edit = new QTextEdit(content_widget_);
        edit->setReadOnly(true);

        QString html;
        html += QStringLiteral("<h3 style='color:%1;margin:12px 0 6px;'>维度评分明细</h3>").arg(tm.Primary());
        html += QStringLiteral("<table style='width:100%;border-collapse:collapse;color:%1;font-size:12px;'>")
            .arg(tm.TextPrimary());
        html += QStringLiteral("<tr style='background:rgba(255,255,255,0.04);'>"
            "<th style='padding:6px 8px;text-align:left;'>评估维度</th>"
            "<th style='padding:6px 8px;text-align:right;'>满分</th>"
            "<th style='padding:6px 8px;text-align:right;'>均值</th>"
            "<th style='padding:6px 8px;text-align:right;'>得分率</th></tr>");

        auto row = [&](const QString& name, int max_score, float mean) {
            float rate = (max_score > 0) ? 100.0f * mean / max_score : 0;
            QString color = (rate >= 90) ? "#4CAF50" : (rate >= 70) ? "#FFC107" : "#F44336";
            html += QStringLiteral("<tr><td style='padding:6px 8px;'>%1</td>"
                "<td style='padding:6px 8px;text-align:right;'>%2</td>"
                "<td style='padding:6px 8px;text-align:right;'>%3</td>"
                "<td style='padding:6px 8px;text-align:right;color:%4;'>%5%</td></tr>")
                .arg(name).arg(max_score).arg(mean, 0, 'f', 1).arg(color).arg(rate, 0, 'f', 0);
        };

        row(QStringLiteral("预测置信度 (decision_margin)"), 50, r.mean_score_conf);
        row(QStringLiteral("时序稳定性 (翻转+脉冲)"), 50, r.mean_score_stab);
        html += QStringLiteral("</table>");

        html += QStringLiteral("<h3 style='color:%1;margin:16px 0 6px;'>关键指标统计</h3>").arg(tm.Primary());
        html += QStringLiteral("<table style='width:100%;border-collapse:collapse;color:%1;font-size:12px;'>")
            .arg(tm.TextPrimary());

        auto met = [&](const QString& name, const QString& value, const QString& comment) {
            html += QStringLiteral("<tr><td style='padding:4px 8px;'>%1</td>"
                "<td style='padding:4px 8px;text-align:right;font-weight:bold;'>%2</td>"
                "<td style='padding:4px 8px;color:%3;font-size:11px;'>%4</td></tr>")
                .arg(name, value, tm.TextMuted(), comment);
        };

        met(QStringLiteral("置信度均分"), QString::number(r.mean_score_conf, 'f', 1) + QStringLiteral("/50"),
            r.mean_score_conf >= 40.0f ? QStringLiteral("优秀") : r.mean_score_conf >= 25.0f ? QStringLiteral("良好") : QStringLiteral("较差"));
        met(QStringLiteral("稳定性均分"), QString::number(r.mean_score_stab, 'f', 1) + QStringLiteral("/50"),
            r.mean_score_stab >= 40.0f ? QStringLiteral("优秀") : r.mean_score_stab >= 25.0f ? QStringLiteral("正常") : QStringLiteral("需关注"));
        met(QStringLiteral("平均判决策略距离"), QString::number(r.mean_decision_margin, 'f', 3),
            r.mean_decision_margin >= 0.2f ? QStringLiteral("判定置信度高") : r.mean_decision_margin >= 0.1f ? QStringLiteral("中等") : QStringLiteral("边界模糊"));
        met(QStringLiteral("AU翻转率"), QString::number(r.overall_au_flip_rate, 'f', 3),
            r.overall_au_flip_rate < 0.05f ? QStringLiteral("时序稳定") : r.overall_au_flip_rate < 0.15f ? QStringLiteral("正常波动") : QStringLiteral("振荡严重"));
        met(QStringLiteral("脉冲噪声帧"), QString::number(r.total_spike_frames)
            + QStringLiteral(" (%1%)").arg(pct(r.total_spike_frames), 0, 'f', 1),
            r.total_spike_frames == 0 ? QStringLiteral("无噪声") : QStringLiteral("需关注"));
        html += QStringLiteral("</table>");

        edit->setHtml(html);
        edit->setStyleSheet(QStringLiteral(
            "QTextEdit { background: transparent; border: none; }"));
        layout->addWidget(edit);
    }

    static_cast<QVBoxLayout*>(layout)->addStretch();
}

}  // namespace omniface::ui
