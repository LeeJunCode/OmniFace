#include "omniface/ui/detail_page.h"
#include "omniface/ui/image_utils.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <QVBoxLayout>
#include <QTimer>

namespace omniface::ui {

DetailPage::DetailPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background-color: #f5f5f5;");
    setupUI();
}

void DetailPage::setupUI() {
    auto* dl = new QVBoxLayout(this);
    dl->setContentsMargins(8, 6, 8, 6);
    dl->setSpacing(6);

    auto* top = new QHBoxLayout;
    auto* back_btn = new QPushButton(QStringLiteral("\u2190 \u8fd4\u56de"));
    back_btn->setFixedWidth(100);
    connect(back_btn, &QPushButton::clicked, this, [this]() {
        detail_video_->stop();
        detail_orig_video_->stop();
        disconnect(detail_video_, &VideoPlayerWidget::positionChanged,
                   this, &DetailPage::onDetailFrameSync);
        csv_headers_.clear();
        csv_rows_.clear();
        frame_to_row_.clear();
        aligned_paths_.clear();
        tracked_fps_ = 0.0;
        detail_face_thumb_->setText(QStringLiteral("\u5bf9\u9f50\u4eba\u8138"));
        detail_data_label_->setText(
            QStringLiteral("<span style='color:#999'>\u64ad\u653e\u89c6\u9891\u4ee5\u67e5\u770b\u6bcf\u5e27\u6570\u636e</span>"));
        if (from_history_)
            emit backToHistory();
        else
            emit backRequested();
    });
    top->addWidget(back_btn);
    top->addStretch();
    dl->addLayout(top);

    auto* split = new QSplitter(Qt::Horizontal);

    auto* left = new QWidget;
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);
    detail_video_ = new VideoPlayerWidget;
    detail_video_->setMinimumSize(400, 260);
    ll->addWidget(detail_video_, 1);
    split->addWidget(left);

    auto* right_split = new QSplitter(Qt::Vertical);
    auto panelStyle = QStringLiteral(
        "QFrame { background: #ffffff; border: 1px solid #e0e5ec; border-radius: 6px; }");
    auto panelTitleStyle = QStringLiteral(
        "font-weight: bold; font-size: 13px; color: #333; border: none;");

    // Panel 1: Video info
    auto* panel1 = new QFrame;
    panel1->setStyleSheet(panelStyle);
    auto* p1l = new QVBoxLayout(panel1);
    p1l->setContentsMargins(10, 8, 10, 8);
    p1l->setSpacing(6);

    auto* p1title_row = new QHBoxLayout;
    auto* p1title = new QLabel(QStringLiteral("\u89c6\u9891\u4fe1\u606f"));
    p1title->setStyleSheet(panelTitleStyle);
    p1title_row->addWidget(p1title);
    p1title_row->addStretch();
    auto* detail_del_btn = new QPushButton(QStringLiteral("\u5220\u9664"));
    detail_del_btn->setFixedHeight(26);
    detail_del_btn->setStyleSheet(
        "QPushButton { background-color: #e53935; color: white; border: none; "
        "border-radius: 3px; padding: 4px 12px; font-size: 11px; }"
        "QPushButton:hover { background-color: #c62828; }");
    connect(detail_del_btn, &QPushButton::clicked, this, [this]() {
        detail_video_->stop();
        detail_orig_video_->stop();
        QTimer::singleShot(200, this, [this]() {
            emit deleteRequested(current_dir_);
        });
    });
    p1title_row->addWidget(detail_del_btn);
    p1l->addLayout(p1title_row);

    detail_info_label_ = new QLabel;
    detail_info_label_->setTextFormat(Qt::RichText);
    detail_info_label_->setWordWrap(true);
    detail_info_label_->setStyleSheet("border: none; font-size: 12px; color: #555; padding: 0;");
    p1l->addWidget(detail_info_label_);

    detail_orig_video_ = new VideoPlayerWidget;
    detail_orig_video_->setMinimumHeight(120);
    detail_orig_video_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    p1l->addWidget(detail_orig_video_, 1);
    right_split->addWidget(panel1);

    // Panel 2: Reference face
    auto* panel2 = new QFrame;
    panel2->setStyleSheet(panelStyle);
    auto* p2l = new QVBoxLayout(panel2);
    p2l->setContentsMargins(10, 8, 10, 8);
    p2l->setSpacing(6);

    auto* p2title = new QLabel(QStringLiteral("\u76ee\u6807\u4eba\u8138"));
    p2title->setStyleSheet(panelTitleStyle);
    p2l->addWidget(p2title);

    auto* ref_row = new QHBoxLayout;
    auto* orig_col = new QVBoxLayout;
    detail_ref_thumb_ = new QLabel;
    detail_ref_thumb_->setFixedSize(120, 120);
    detail_ref_thumb_->setAlignment(Qt::AlignCenter);
    detail_ref_thumb_->setStyleSheet(
        "background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px; color: #999; font-size:11px;");
    detail_ref_thumb_->setText(QStringLiteral("\u539f\u59cb\u56fe\u50cf"));
    orig_col->addWidget(detail_ref_thumb_, 0, Qt::AlignHCenter);
    auto* orig_cap = new QLabel(QStringLiteral("\u539f\u59cb\u56fe\u50cf"));
    orig_cap->setAlignment(Qt::AlignCenter);
    orig_cap->setStyleSheet("border: none; font-size: 11px; color: #888;");
    orig_col->addWidget(orig_cap);
    ref_row->addLayout(orig_col);

    auto* det_col = new QVBoxLayout;
    detail_ref_detected_ = new QLabel;
    detail_ref_detected_->setFixedSize(120, 120);
    detail_ref_detected_->setAlignment(Qt::AlignCenter);
    detail_ref_detected_->setStyleSheet(
        "background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px; color: #999; font-size:11px;");
    detail_ref_detected_->setText(QStringLiteral("\u68c0\u6d4b\u4eba\u8138"));
    det_col->addWidget(detail_ref_detected_, 0, Qt::AlignHCenter);
    auto* det_cap = new QLabel(QStringLiteral("\u68c0\u6d4b\u4eba\u8138"));
    det_cap->setAlignment(Qt::AlignCenter);
    det_cap->setStyleSheet("border: none; font-size: 11px; color: #888;");
    det_col->addWidget(det_cap);
    ref_row->addLayout(det_col);
    p2l->addLayout(ref_row);
    p2l->addStretch();
    right_split->addWidget(panel2);

    // Panel 3: Analysis results
    auto* panel3 = new QFrame;
    panel3->setStyleSheet(panelStyle);
    auto* p3l = new QVBoxLayout(panel3);
    p3l->setContentsMargins(10, 8, 10, 8);
    p3l->setSpacing(4);

    auto* p3title = new QLabel(QStringLiteral("\u5206\u6790\u7ed3\u679c"));
    p3title->setStyleSheet(panelTitleStyle);
    p3l->addWidget(p3title);

    detail_face_thumb_ = new QLabel(QStringLiteral("\u5bf9\u9f50\u4eba\u8138"));
    detail_face_thumb_->setFixedSize(112, 112);
    detail_face_thumb_->setAlignment(Qt::AlignCenter);
    detail_face_thumb_->setStyleSheet(
        "background-color: #fafafa; border: 1px solid #d0d0d0; border-radius: 4px; color: #999;");
    p3l->addWidget(detail_face_thumb_, 0, Qt::AlignHCenter);

    auto* align_cap = new QLabel(QStringLiteral("\u5f53\u524d\u5e27\u5bf9\u9f50\u4eba\u8138"));
    align_cap->setAlignment(Qt::AlignCenter);
    align_cap->setStyleSheet("border: none; font-size: 11px; color: #888;");
    p3l->addWidget(align_cap);

    auto* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("QFrame { border: none; border-top: 1px solid #e8e8e8; background: transparent; }");
    p3l->addWidget(sep2);

    detail_data_label_ = new QLabel(
        QStringLiteral("<span style='color:#999'>\u64ad\u653e\u89c6\u9891\u4ee5\u67e5\u770b\u6bcf\u5e27\u6570\u636e</span>"));
    detail_data_label_->setWordWrap(true);
    detail_data_label_->setTextFormat(Qt::RichText);
    detail_data_label_->setMinimumWidth(280);
    detail_data_label_->setAlignment(Qt::AlignTop);

    auto* sc = new QScrollArea;
    sc->setWidgetResizable(true);
    sc->setWidget(detail_data_label_);
    sc->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    p3l->addWidget(sc, 1);
    right_split->addWidget(panel3);

    right_split->setStretchFactor(0, 0);
    right_split->setStretchFactor(1, 0);
    right_split->setStretchFactor(2, 1);

    split->addWidget(right_split);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    dl->addWidget(split, 1);
}

QImage DetailPage::matToQImage(const cv::Mat& bgr) {
    return MatToQImage(bgr, qt_image_cache_);
}

void DetailPage::showTask(const std::string& output_dir, bool from_history) {
    namespace fs = std::filesystem;
    fs::path out_dir(output_dir);
    if (!fs::is_directory(out_dir)) return;

    from_history_ = from_history;
    current_dir_ = output_dir;

    csv_headers_.clear();
    csv_rows_.clear();
    frame_to_row_.clear();
    aligned_paths_.clear();
    tracked_fps_ = 0.0;

    loadCSV(output_dir);

    // Find aligned face images
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        if (entry.is_directory() && entry.path().filename().string().find("_aligned") != std::string::npos) {
            for (const auto& f : fs::directory_iterator(entry.path())) {
                auto ext = f.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".bmp")
                    aligned_paths_.push_back(f.path().string());
            }
            std::sort(aligned_paths_.begin(), aligned_paths_.end());
            break;
        }
    }

    // Find tracked video
    std::string tracked_path;
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        if (entry.path().extension() == ".avi") { tracked_path = entry.path().string(); break; }
    }
    int video_w = 0, video_h = 0;
    if (!tracked_path.empty()) {
        cv::VideoCapture cap(tracked_path);
        if (cap.isOpened()) {
            tracked_fps_ = cap.get(cv::CAP_PROP_FPS);
            video_w = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
            video_h = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
            cap.release();
        }
        detail_video_->setVideoPath(tracked_path);
    }

    // Parse directory name
    std::string dirname = out_dir.filename().string();
    std::string video_name = dirname;
    std::string time_display;
    if (dirname.size() >= 16) {
        size_t last_underscore = dirname.rfind('_');
        size_t date_underscore = (last_underscore != std::string::npos)
            ? dirname.rfind('_', last_underscore - 1) : std::string::npos;
        if (date_underscore != std::string::npos && last_underscore != std::string::npos) {
            video_name = dirname.substr(0, date_underscore);
            std::string date_part = dirname.substr(date_underscore + 1, 8);
            std::string time_part = dirname.substr(last_underscore + 1, 6);
            time_display = date_part.substr(0, 4) + "-" + date_part.substr(4, 2) + "-"
                         + date_part.substr(6, 2) + " "
                         + time_part.substr(0, 2) + ":" + time_part.substr(2, 2) + ":"
                         + time_part.substr(4, 2);
        }
    }

    // Find original video
    std::string orig_video_path;
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        auto fname = entry.path().filename().string();
        if (fname.find("original") == 0 && entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov") {
                orig_video_path = entry.path().string();
                break;
            }
        }
    }
    if (orig_video_path.empty()) orig_video_path = tracked_path;
    detail_orig_video_->setVideoPath(orig_video_path);

    // Load reference images
    detail_ref_thumb_->setText(QStringLiteral("\u76ee\u6807\u4eba\u7269"));
    detail_ref_detected_->setText(QStringLiteral("\u68c0\u6d4b\u4eba\u8138"));
    for (const auto& entry : fs::directory_iterator(out_dir)) {
        auto fname = entry.path().filename().string();
        if (!entry.is_regular_file()) continue;
        if (fname.find("reference") != 0) continue;

        cv::Mat img = cv::imread(entry.path().string());
        if (img.empty()) continue;
        QImage qimg = matToQImage(img);
        QPixmap pixmap = QPixmap::fromImage(qimg);
        if (pixmap.isNull()) continue;

        QPixmap scaled = pixmap.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (fname.find("_detected") != std::string::npos)
            detail_ref_detected_->setPixmap(scaled);
        else
            detail_ref_thumb_->setPixmap(scaled);
    }

    // Build info HTML
    int64_t file_size = 0;
    int total_frames = 0;
    if (!orig_video_path.empty()) {
        try { file_size = fs::file_size(orig_video_path); } catch (...) {}
        cv::VideoCapture cap(orig_video_path);
        if (cap.isOpened()) {
            total_frames = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
            cap.release();
        }
    }

    auto fmtSize = [](int64_t bytes) -> QString {
        if (bytes >= 1024 * 1024 * 1024)
            return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
        if (bytes >= 1024 * 1024)
            return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
        if (bytes >= 1024)
            return QString::number(bytes / 1024.0, 'f', 1) + " KB";
        return QString::number(bytes) + " B";
    };

    QString info_html;
    info_html += "<div style='font-size:12px; line-height:1.6; color:#555;'>";
    info_html += QStringLiteral(
        "<table>"
        "<tr><td style='color:#999; padding-right:8px;'>\u89c6\u9891\u540d\u79f0</td>"
        "<td><b>%1</b></td></tr>"
        "<tr><td style='color:#999; padding-right:8px;'>\u89c6\u9891\u5927\u5c0f</td>"
        "<td>%2</td></tr>"
        "<tr><td style='color:#999; padding-right:8px;'>\u5904\u7406\u65f6\u95f4</td>"
        "<td>%3</td></tr>"
        "<tr><td style='color:#999; padding-right:8px;'>\u5206\u8fa8\u7387</td>"
        "<td>%4\u00d7%5</td></tr>"
        "<tr><td style='color:#999; padding-right:8px;'>\u603b\u5e27\u6570</td>"
        "<td>%6</td></tr>"
        "</table>")
        .arg(QString::fromStdString(video_name).toHtmlEscaped())
        .arg(fmtSize(file_size))
        .arg(QString::fromStdString(time_display))
        .arg(video_w).arg(video_h)
        .arg(total_frames);
    info_html += "</div>";
    detail_info_label_->setText(info_html);

    disconnect(detail_video_, &VideoPlayerWidget::positionChanged,
               this, &DetailPage::onDetailFrameSync);
    connect(detail_video_, &VideoPlayerWidget::positionChanged,
            this, &DetailPage::onDetailFrameSync);

    if (!tracked_path.empty()) detail_video_->play();
}

void DetailPage::loadCSV(const std::string& output_dir) {
    namespace fs = std::filesystem;
    std::string csv_path;
    for (const auto& entry : fs::directory_iterator(output_dir)) {
        if (entry.path().extension() == ".csv") { csv_path = entry.path().string(); break; }
    }
    if (csv_path.empty()) return;

    std::ifstream csv_file(csv_path);
    if (!csv_file.is_open()) return;

    auto parseRow = [](const std::string& line) {
        std::vector<std::string> cols;
        std::string cell;
        bool in_quotes = false;
        for (size_t i = 0; i < line.size(); ++i) {
            char ch = line[i];
            if (ch == '"') { in_quotes = !in_quotes; }
            else if (ch == ',' && !in_quotes) { cols.push_back(cell); cell.clear(); }
            else { cell += ch; }
        }
        cols.push_back(cell);
        return cols;
    };

    std::string line;
    bool first = true;
    while (std::getline(csv_file, line)) {
        if (line.empty()) continue;
        if (first) { csv_headers_ = parseRow(line); first = false; }
        else { csv_rows_.push_back(parseRow(line)); }
    }

    int frame_col = -1;
    for (size_t i = 0; i < csv_headers_.size(); ++i) {
        if (csv_headers_[i] == "frame") { frame_col = static_cast<int>(i); break; }
    }
    if (frame_col >= 0) {
        for (size_t r = 0; r < csv_rows_.size(); ++r) {
            try {
                int fnum = std::stoi(csv_rows_[r][frame_col]);
                frame_to_row_[fnum] = static_cast<int>(r);
            } catch (...) {}
        }
    }
}

void DetailPage::onDetailFrameSync(qint64 positionMs) {
    if (csv_rows_.empty() || frame_to_row_.empty() || tracked_fps_ <= 0.0) return;

    int video_frame = static_cast<int>(positionMs * tracked_fps_ / 1000.0 + 0.5);
    int csv_frame_num = video_frame + 1;

    auto it = frame_to_row_.find(csv_frame_num);
    if (it == frame_to_row_.end()) {
        it = frame_to_row_.find(csv_frame_num - 1);
        if (it == frame_to_row_.end())
            it = frame_to_row_.find(csv_frame_num + 1);
    }
    if (it == frame_to_row_.end()) return;

    int row_idx = it->second;
    const auto& row = csv_rows_[row_idx];
    if (row.empty() || csv_headers_.empty()) return;

    auto col = [this](const std::string& name) -> int {
        for (size_t i = 0; i < csv_headers_.size(); ++i)
            if (csv_headers_[i] == name) return static_cast<int>(i);
        return -1;
    };
    auto val = [&row](int idx) -> std::string {
        if (idx < 0 || idx >= static_cast<int>(row.size())) return "0";
        return row[idx];
    };
    auto valf = [&val](int idx) -> double {
        try { return std::stod(val(idx)); } catch (...) { return 0.0; }
    };

    double pitch = valf(col("pose_Rx"));
    double yaw   = valf(col("pose_Ry"));
    double roll  = valf(col("pose_Rz"));
    double gaze_x = valf(col("gaze_angle_x"));
    double gaze_y = valf(col("gaze_angle_y"));
    int success = static_cast<int>(valf(col("success")));

    double face_min_x = 1e9, face_max_x = -1e9;
    double face_min_y = 1e9, face_max_y = -1e9;
    int lm_found = 0;
    for (int i = 0; i < 68; ++i) {
        int cx = col("x_" + std::to_string(i));
        int cy = col("y_" + std::to_string(i));
        if (cx < 0 || cy < 0) continue;
        double px = valf(cx), py = valf(cy);
        if (px == 0.0 && py == 0.0) continue;
        ++lm_found;
        if (px < face_min_x) face_min_x = px;
        if (px > face_max_x) face_max_x = px;
        if (py < face_min_y) face_min_y = py;
        if (py > face_max_y) face_max_y = py;
    }

    double pose_dev = std::sqrt(pitch * pitch + yaw * yaw);
    double frontal_score = std::max(0.0, 1.0 - pose_dev / 35.0);
    double face_area = (face_max_x - face_min_x) * (face_max_y - face_min_y);
    double size_score = (lm_found >= 10) ? std::min(1.0, face_area / 40000.0) : 0.0;
    double quality_val = frontal_score * 0.6 + size_score * 0.4;

    QString quality;
    if (success == 0 || lm_found < 10)
        quality = QStringLiteral("<span style='color:#e53935;'>\u68c0\u6d4b\u5931\u8d25</span>");
    else if (quality_val > 0.80)
        quality = QStringLiteral("<span style='color:#2e7d32;'>\u4f18\u79c0</span>");
    else if (quality_val > 0.55)
        quality = QStringLiteral("<span style='color:#4caf50;'>\u826f\u597d</span>");
    else if (quality_val > 0.30)
        quality = QStringLiteral("<span style='color:#f9a825;'>\u4e00\u822c</span>");
    else
        quality = QStringLiteral("<span style='color:#e53935;'>\u8f83\u5dee</span>");

    QStringList pose_parts;
    if (pitch > 15.0) pose_parts << QStringLiteral("\u660e\u663e\u62ac\u5934");
    else if (pitch > 5.0) pose_parts << QStringLiteral("\u7565\u5fae\u62ac\u5934");
    else if (pitch < -15.0) pose_parts << QStringLiteral("\u660e\u663e\u4f4e\u5934");
    else if (pitch < -5.0) pose_parts << QStringLiteral("\u7565\u5fae\u4f4e\u5934");
    if (yaw > 15.0) pose_parts << QStringLiteral("\u660e\u663e\u53f3\u8f6c");
    else if (yaw > 5.0) pose_parts << QStringLiteral("\u5fae\u5fae\u53f3\u8f6c");
    else if (yaw < -15.0) pose_parts << QStringLiteral("\u660e\u663e\u5de6\u8f6c");
    else if (yaw < -5.0) pose_parts << QStringLiteral("\u5fae\u5fae\u5de6\u8f6c");
    if (roll > 10.0) pose_parts << QStringLiteral("\u5934\u90e8\u53f3\u503e");
    else if (roll < -10.0) pose_parts << QStringLiteral("\u5934\u90e8\u5de6\u503e");

    QString pose_desc = pose_parts.isEmpty()
        ? QStringLiteral("\u6b63\u5bf9\u955c\u5934")
        : pose_parts.join(QStringLiteral("\u3001"));

    QString h_dir, v_dir;
    if (gaze_x > 0.3) h_dir = QStringLiteral("\u5411\u53f3");
    else if (gaze_x < -0.3) h_dir = QStringLiteral("\u5411\u5de6");
    if (gaze_y > 0.3) v_dir = QStringLiteral("\u5411\u4e0a");
    else if (gaze_y < -0.3) v_dir = QStringLiteral("\u5411\u4e0b");
    QString gaze_desc;
    if (h_dir.isEmpty() && v_dir.isEmpty())
        gaze_desc = QStringLiteral("\u6b63\u89c6\u524d\u65b9");
    else if (h_dir.isEmpty())
        gaze_desc = QStringLiteral("\u770b%1\u65b9").arg(v_dir);
    else if (v_dir.isEmpty())
        gaze_desc = QStringLiteral("\u770b%1\u65b9").arg(h_dir);
    else
        gaze_desc = QStringLiteral("\u770b%1%2\u65b9").arg(v_dir, h_dir);

    static const std::unordered_map<std::string, QString> kAuNames = {
        {"AU01", QStringLiteral("\u7709\u6bdb\u4e0a\u626c")},
        {"AU02", QStringLiteral("\u7709\u6bdb\u5916\u4fa7\u4e0a\u626c")},
        {"AU04", QStringLiteral("\u76b1\u7709")},
        {"AU05", QStringLiteral("\u7741\u5927\u773c")},
        {"AU06", QStringLiteral("\u8138\u988a\u63d0\u5347")},
        {"AU07", QStringLiteral("\u772f\u773c")},
        {"AU09", QStringLiteral("\u76b1\u9f3b")},
        {"AU10", QStringLiteral("\u4e0a\u5507\u4e0a\u63d0")},
        {"AU12", QStringLiteral("\u5634\u89d2\u4e0a\u626c")},
        {"AU14", QStringLiteral("\u62bf\u5634")},
        {"AU15", QStringLiteral("\u5634\u89d2\u4e0b\u6487")},
        {"AU17", QStringLiteral("\u4e0b\u5df4\u6536\u7d27")},
        {"AU20", QStringLiteral("\u5634\u89d2\u62c9\u4f38")},
        {"AU23", QStringLiteral("\u5634\u5507\u6536\u7d27")},
        {"AU25", QStringLiteral("\u5f20\u5634")},
        {"AU26", QStringLiteral("\u4e0b\u5df4\u4e0b\u5760")},
        {"AU28", QStringLiteral("\u5438\u5507")},
        {"AU45", QStringLiteral("\u7728\u773c")},
    };

    QStringList au_parts;
    for (size_t h = 0; h < csv_headers_.size(); ++h) {
        const auto& hdr = csv_headers_[h];
        if (hdr.size() > 3 && hdr.substr(0, 2) == "AU" &&
            hdr[hdr.size() - 2] == '_' && hdr.back() == 'r') {
            double intensity = valf(static_cast<int>(h));
            if (intensity > 0.3) {
                std::string au_code = hdr.substr(0, hdr.size() - 2);
                auto au_it = kAuNames.find(au_code);
                if (au_it != kAuNames.end()) au_parts << au_it->second;
                if (au_parts.size() >= 6) break;
            }
        }
    }
    QString au_desc = au_parts.isEmpty()
        ? QStringLiteral("\u65e0\u660e\u663e\u8868\u60c5")
        : au_parts.join(QStringLiteral("\u3001"));

    QString html;
    html += "<div style='font-size:13px; line-height:1.8; color:#333;'>";
    html += QStringLiteral(
        "<div>"
        "<span style='color:#888;'>\u7b2c </span><b>%1</b>"
        "<span style='color:#888;'> \u5e27 &nbsp;|&nbsp; \u65f6\u95f4 </span><b>%2 \u79d2</b>"
        "<span style='color:#888;'> &nbsp;|&nbsp; \u8d28\u91cf </span><b>%3</b>"
        "</div>")
        .arg(QString::fromStdString(val(col("frame"))).toHtmlEscaped())
        .arg(QString::fromStdString(val(col("timestamp"))).toHtmlEscaped())
        .arg(quality);
    html += QStringLiteral(
        "<div style='margin-top:4px;'>"
        "<span style='color:#888;'>\u5934\u90e8\u59ff\u6001\uff1a</span><b>%1</b>"
        "</div>").arg(pose_desc);
    html += QStringLiteral(
        "<div>"
        "<span style='color:#888;'>\u89c6\u7ebf\u65b9\u5411\uff1a</span><b>%1</b>"
        "</div>").arg(gaze_desc);
    html += QStringLiteral(
        "<div>"
        "<span style='color:#888;'>\u9762\u90e8\u8868\u60c5\uff1a</span><b>%1</b>"
        "</div>").arg(au_desc);
    html += "</div>";
    detail_data_label_->setText(html);

    if (row_idx < static_cast<int>(aligned_paths_.size())) {
        cv::Mat face = cv::imread(aligned_paths_[row_idx]);
        if (!face.empty()) {
            cv::Mat rgb;
            cv::cvtColor(face, rgb, cv::COLOR_BGR2RGB);
            QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
            detail_face_thumb_->setPixmap(QPixmap::fromImage(qimg.copy())
                .scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }
    detail_face_thumb_->setText(QStringLiteral("\u5bf9\u9f50\u4eba\u8138"));
}

}  // namespace omniface::ui
