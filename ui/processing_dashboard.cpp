#include "omniface/ui/processing_dashboard.h"

#include <QFontMetrics>
#include <QPainter>

namespace omniface::ui {

FpsPanel::FpsPanel(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(28);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void FpsPanel::updateData(const DashboardData& data) {
    // 指数移动平均(EMA)平滑FPS显示，避免数值剧烈跳动
    if (smooth_pipe_ == 0.0) {
        smooth_pipe_ = data.pipeline_fps;
        smooth_rec_ = data.recognition_fps;
        smooth_ana_ = data.analysis_fps;
    } else {
        smooth_pipe_ = kAlpha * data.pipeline_fps + (1.0 - kAlpha) * smooth_pipe_;
        smooth_rec_ = kAlpha * data.recognition_fps + (1.0 - kAlpha) * smooth_rec_;
        // 仅当OpenFace实际在运行时才更新其FPS平滑值
        if (data.analysis_running && data.analysis_fps > 0.0)
            smooth_ana_ = kAlpha * data.analysis_fps + (1.0 - kAlpha) * smooth_ana_;
    }
    analysis_running_ = data.analysis_running;
    if (!analysis_running_) smooth_ana_ = 0.0;  // 未运行时清零
    update();
}

QSize FpsPanel::minimumSizeHint() const { return QSize(300, 28); }
QSize FpsPanel::sizeHint() const { return QSize(600, 34); }

void FpsPanel::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // FPS颜色编码：>=20绿色(流畅)，10-20橙色(一般)，<10红色(卡顿)，0灰色
    auto fpsColor = [](double fps) -> QColor {
        if (fps <= 0.0) return QColor(0xb0, 0xb0, 0xb0);
        if (fps >= 20.0) return QColor(0x2e, 0x7d, 0x32);
        if (fps >= 10.0) return QColor(0xf5, 0x7f, 0x17);
        return QColor(0xc6, 0x28, 0x28);
    };

    struct Block { QString label; double val; QColor color; };
    Block blocks[] = {
        {QStringLiteral("\u7ba1\u7ebf"),    smooth_pipe_, fpsColor(smooth_pipe_)},
        {QStringLiteral("\u4eba\u8138\u8bc6\u522b"), smooth_rec_,  fpsColor(smooth_rec_)},
        {QStringLiteral("OpenFace"), smooth_ana_, analysis_running_ ? fpsColor(smooth_ana_) : QColor(0xb0, 0xb0, 0xb0)},
    };

    int bw = width() / 3;
    QFont label_font("Microsoft YaHei", 9);
    QFont value_font("Microsoft YaHei", 12);
    value_font.setBold(true);

    for (int i = 0; i < 3; ++i) {
        QRect col(i * bw, 0, bw, height());

        p.setFont(label_font);
        p.setPen(QColor(0x88, 0x88, 0x88));
        p.drawText(QRect(col.left(), col.top(), col.width(), 13),
                   Qt::AlignHCenter | Qt::AlignBottom, blocks[i].label);

        p.setFont(value_font);
        p.setPen(blocks[i].color);
        QString text = (!blocks[i].color.isValid() || (!analysis_running_ && i == 2))
            ? QStringLiteral("\u2014")
            : QString::number(blocks[i].val, 'f', 1) + " fps";
        p.drawText(QRect(col.left(), col.top() + 13, col.width(), col.height() - 13),
                   Qt::AlignHCenter | Qt::AlignTop, text);
    }
}

CounterPanel::CounterPanel(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(36);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void CounterPanel::updateData(const DashboardData& data) {
    data_ = data;
    update();
}

QSize CounterPanel::minimumSizeHint() const { return QSize(300, 36); }
QSize CounterPanel::sizeHint() const { return QSize(600, 44); }

void CounterPanel::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setFont(QFont("Microsoft YaHei", 9));
    p.setPen(QColor(0x55, 0x55, 0x55));

    auto fmt = [](const QString& label, int64_t n) {
        return label + ": " + QString::number(n);
    };
    auto fmtTime = [](int64_t sec) {
        int h = static_cast<int>(sec / 3600);
        int m = static_cast<int>((sec % 3600) / 60);
        int s = static_cast<int>(sec % 60);
        return QString("%1:%2:%3").arg(h, 2, 10, QChar('0'))
                                  .arg(m, 2, 10, QChar('0'))
                                  .arg(s, 2, 10, QChar('0'));
    };

    QString row1 = fmt(QStringLiteral("\u5df2\u5904\u7406"), data_.processed_frames)
                   + QStringLiteral(" \u5e27  |  ")
                   + fmt(QStringLiteral("\u4eba\u8138\u68c0\u6d4b"), data_.face_detections)
                   + QStringLiteral(" \u6b21  |  ")
                   + fmt(QStringLiteral("\u76ee\u6807\u5339\u914d"), data_.target_matches)
                   + QStringLiteral(" \u5e27  |  ")
                   + fmt(QStringLiteral("OpenFace\u5206\u6790"), data_.openface_analyses)
                   + QStringLiteral(" \u5e27  |  ")
                   + QStringLiteral("\u8fd0\u884c: ") + fmtTime(data_.elapsed_seconds);

    int row_h = height() / 2;
    QRect r1(4, 0, width() - 8, height());
    QRect r2(4, row_h, width() - 8, 0);  // unused

    auto fm = p.fontMetrics();
    p.drawText(r1, Qt::AlignVCenter | Qt::AlignLeft,
               fm.elidedText(row1, Qt::ElideRight, r1.width()));
}

}  // namespace omniface::ui
