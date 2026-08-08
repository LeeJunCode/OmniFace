#include "omniface/ui/dialogs/csv_reference_dialog.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "omniface/ui/theme.h"

namespace omniface {
namespace ui {

CsvReferenceDialog::CsvReferenceDialog(QWidget* parent)
    : QDialog(parent) {
    auto& tm = ThemeManager::Instance();
    setWindowTitle(QStringLiteral("CSV 数据参考 — 全量字段说明与映射关系"));
    resize(960, 680);
    setMinimumSize(860, 560);
    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; color: %2; }"
        "QPushButton {"
        "  background: %3; color: %2; border: none;"
        "  border-radius: 4px; padding: 8px 16px; font-size: 13px;"
        "}"
        "QPushButton:hover { background: %4; }"
        "QPushButton:checked { background: %5; border: 1px solid %6; }"
        "QListWidget {"
        "  background: %7; color: %2; border: 1px solid %8;"
        "  border-radius: 4px; font-size: 13px;"
        "}"
        "QListWidget::item { padding: 8px 12px; }"
        "QListWidget::item:selected { background: %5; color: %2; }"
        "QListWidget::item:hover { background: %3; }"
        "QTextEdit {"
        "  background: %7; color: %2; border: 1px solid %8;"
        "  border-radius: 4px; padding: 14px; font-size: 12px;"
        "}")
        .arg(tm.SurfaceBg(), tm.TextPrimary(),
             tm.HoverBg(), tm.HoverBg(),
             tm.SelectedBg(), tm.Primary(),
             tm.DeepBg(), tm.Border()));
    SetupUI();
    BuildContent();
    backend_list_->setCurrentRow(0);
    OnBackendChanged(0);
}

void CsvReferenceDialog::SetupUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // Top tab bar
    auto* tab_bar = new QHBoxLayout();
    tab_bar->setSpacing(4);
    auto makeTab = [this](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };
    auto* field_tab = makeTab(QStringLiteral("CSV 字段含义"));
    auto* mapping_tab = makeTab(QStringLiteral("数据映射关系"));
    field_tab->setChecked(true);

    connect(field_tab, &QPushButton::clicked, this, [this]() { OnTabChanged(0); });
    connect(mapping_tab, &QPushButton::clicked, this, [this]() { OnTabChanged(1); });
    connect(field_tab, &QPushButton::toggled, mapping_tab, [mapping_tab](bool checked) {
        if (checked) mapping_tab->setChecked(false);
    });
    connect(mapping_tab, &QPushButton::toggled, field_tab, [field_tab](bool checked) {
        if (checked) field_tab->setChecked(false);
    });

    tab_bar->addWidget(field_tab);
    tab_bar->addWidget(mapping_tab);
    tab_bar->addStretch();

    auto* close_btn = new QPushButton(QStringLiteral("关闭"), this);
    close_btn->setCursor(Qt::PointingHandCursor);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::close);
    tab_bar->addWidget(close_btn);
    root->addLayout(tab_bar);

    // Body
    auto* body = new QHBoxLayout();
    body->setSpacing(8);

    backend_list_ = new QListWidget(this);
    backend_list_->setFixedWidth(140);
    backend_list_->addItem(QStringLiteral("OpenFace"));
    backend_list_->addItem(QStringLiteral("MediaPipe"));
    backend_list_->addItem(QStringLiteral("L2CS-Net"));
    backend_list_->addItem(QStringLiteral("MEFARG-AU"));
    connect(backend_list_, &QListWidget::currentRowChanged,
            this, &CsvReferenceDialog::OnBackendChanged);
    body->addWidget(backend_list_);

    field_stack_ = new QStackedWidget(this);
    mapping_stack_ = new QStackedWidget(this);

    main_stack_ = new QStackedWidget(this);
    main_stack_->addWidget(field_stack_);
    main_stack_->addWidget(mapping_stack_);
    body->addWidget(main_stack_, 1);

    root->addLayout(body, 1);
}

void CsvReferenceDialog::OnTabChanged(int index) {
    if (index >= 0 && index < main_stack_->count())
        main_stack_->setCurrentIndex(index);
}

void CsvReferenceDialog::OnBackendChanged(int row) {
    if (row >= 0 && row < field_stack_->count()) {
        field_stack_->setCurrentIndex(row);
        mapping_stack_->setCurrentIndex(row);
    }
}

void CsvReferenceDialog::SelectBackend(const QString& backend) {
    int row = -1;
    QString lower = backend.toLower();
    if (lower == QStringLiteral("openface"))  row = 0;
    else if (lower == QStringLiteral("mediapipe")) row = 1;
    else if (lower == QStringLiteral("l2cs"))     row = 2;
    else if (lower == QStringLiteral("mefarg_au")) row = 3;
    if (row >= 0) {
        backend_list_->setCurrentRow(row);
        OnBackendChanged(row);
    }
}

// ============================================================================
// Helpers
// ============================================================================

static QTextEdit* MakeReadOnlyTextEdit(const QString& html) {
    auto* te = new QTextEdit();
    te->setReadOnly(true);
    te->setHtml(html);
    return te;
}

static QString H2(const QString& t) {
    const auto& tm = ThemeManager::Instance();
    return QStringLiteral("<h3 style='color:%1;margin:14px 0 6px 0;'>%2</h3>").arg(tm.Primary(), t);
}

static QString Note(const QString& t) {
    const auto& tm = ThemeManager::Instance();
    return QStringLiteral("<p style='color:%1;margin:6px 0;'>%2</p>").arg(tm.TextDim(), t);
}

static QString Code(const QString& t) {
    const auto& tm = ThemeManager::Instance();
    return QStringLiteral("<code style='background:%1;padding:1px 4px;border-radius:2px;'>%2</code>").arg(tm.SurfaceBg(), t);
}

static QString Tr(const QString& f, const QString& m) {
    return QStringLiteral("<tr><td style='padding:3px 10px;white-space:nowrap;font-family:monospace;'>%1</td>"
                          "<td style='padding:3px 10px;'>%2</td></tr>").arg(f, m);
}

static QString FieldTable(const QStringList& rows) {
    const auto& tm = ThemeManager::Instance();
    QString h = QStringLiteral(
        "<table cellspacing='0' style='width:100%;'>"
        "<tr style='background:%1;color:%2;font-weight:bold;'>"
        "<td style='padding:5px 10px;'>字段</td>"
        "<td style='padding:5px 10px;'>含义</td></tr>")
        .arg(tm.HoverBg(), tm.TextPrimary());
    for (int i = 0; i < rows.size(); i += 2) {
        QString bg = (i / 2) % 2 == 0 ? tm.DeepBg() : tm.AltRow();
        h += QStringLiteral("<tr style='background:%1;'>%2</tr>").arg(bg, Tr(rows[i], rows[i+1]));
    }
    h += QStringLiteral("</table>");
    return h;
}

static QString MappingTable(const QStringList& rows) {
    const auto& tm = ThemeManager::Instance();
    QString h = QStringLiteral(
        "<table cellspacing='0' style='width:100%;'>"
        "<tr style='background:%1;color:%2;font-weight:bold;'>"
        "<td style='padding:5px 10px;'>字段</td>"
        "<td style='padding:5px 10px;'>数值范围</td>"
        "<td style='padding:5px 10px;'>中文描述</td></tr>")
        .arg(tm.HoverBg(), tm.TextPrimary());
    for (int i = 0; i < rows.size(); i += 3) {
        QString bg = (i / 3) % 2 == 0 ? tm.DeepBg() : tm.AltRow();
        h += QStringLiteral("<tr style='background:%1;'><td style='padding:3px 10px;font-family:monospace;'>%2</td>"
                            "<td style='padding:3px 10px;'>%3</td>"
                            "<td style='padding:3px 10px;'>%4</td></tr>")
                 .arg(bg, rows[i], rows[i+1], rows[i+2]);
    }
    h += QStringLiteral("</table>");
    return h;
}

// ============================================================================
// Field Documentation Builders
// ============================================================================

static QTextEdit* BuildOpenFaceFieldDoc() {
    QString html;
    html += QStringLiteral("<h2 style='color:%1;'>OpenFace — 最全面的传统面部分析</h2>").arg(ThemeManager::Instance().Primary());

    html += Note(QStringLiteral("<b>概述：</b>OpenFace 是本项目中最为全面的分析后端，"
        "使用 CE-CLM（Constrained Local Neural Field）面部关键点检测器定位 68 个 dlib 标准关键点，"
        "在此基础上进一步计算头部姿态、视线方向、面部动作单元（AU）等信息。"
        "OpenFace 是学术界广泛使用的开源面部行为分析框架，其分析方法在大量心理学和人机交互研究中得到验证。"));

    html += Note(QStringLiteral("CSV 文件由 OpenFace 原生的 RecorderOpenFace 模块生成，"
        "以逗号分隔，首行为列名（列名前有前导空格）。共包含约 700+ 列数据，分为以下几个功能类别："));

    html += H2(QStringLiteral("1. 基本检测信息（3 列）"));
    html += Note(QStringLiteral("<b>作用：</b>标注每帧的序号和检测质量，帮助判断分析结果是否可信。"));
    html += FieldTable({
        " frame", "帧序号（从 1 开始计数的视频帧编号）",
        " face_id", "检测到的面部编号（0 表示第一个面部）",
        " confidence", QString::fromUtf8("面部检测置信度（0~1）。<b>高于 0.8 表示检测结果可信</b>，低于 0.5 建议谨慎使用该帧数据"),
    });

    html += H2(QStringLiteral("2. 头部姿态（6 列）"));
    html += Note(QStringLiteral("<b>作用：</b>描述头部在三维空间中的朝向和在画面中的位置，"
        "可以判断被拍摄者是否正对镜头、是否在关注画面中的某个区域。"));
    html += FieldTable({
        " pose_Rx", QString::fromUtf8("头部俯仰角 / Pitch（度）。<b>正值 = 下俯</b>（脸部朝下），<b>负值 = 上扬</b>（脸部朝上）。绝对值超过 25° 时姿态变化明显"),
        " pose_Ry", QString::fromUtf8("头部偏航角 / Yaw（度）。<b>正值 = 向左转</b>（被观察者向其左侧转头），<b>负值 = 向右转</b>。绝对值超过 25° 时转向明显"),
        " pose_Rz", QString::fromUtf8("头部翻滚角 / Roll（度）。<b>正值 = 头部右倾</b>，<b>负值 = 头部左倾</b>。绝对值超过 15° 时倾斜明显"),
        " pose_Tx", QString::fromUtf8("头部在相机坐标系中的水平位移（mm）。<b>正值 = 人脸偏画面右侧</b>，表示被拍摄者未居中"),
        " pose_Ty", QString::fromUtf8("头部在相机坐标系中的垂直位移（mm）。<b>正值 = 人脸偏画面上方</b>"),
        " pose_Tz", QString::fromUtf8("头部到相机的距离 / 深度（mm）。数值越小表示人脸离镜头越近，<b>正常谈话距离约 500-800mm</b>"),
    });

    html += Note(QStringLiteral(
        "<b>头部姿态方向判断原理（OpenFace / MediaPipe 通用）：</b><br>"
        "OpenFace 使用 CE-CLM 检测面部关键点后，通过旋转矩阵 <code>R = Rx(pitch) * Ry(yaw) * Rz(roll)</code>"
        "（左手系正号约定）解算欧拉角。<br>"
        "<code>R(0,2) = sin(yaw)</code>，即面部法线在相机 X 轴的分量。<br>"
        "相机坐标系中 <b>+X = 画面右侧</b>（即被拍摄人物的左侧），"
        "因此 sin(yaw) > 0（法线朝画面右）→ 人物面部转向其<b>左侧</b>（yaw > 0 → 左转）；"
        "sin(yaw) < 0（法线朝画面左）→ 人物面部转向其<b>右侧</b>（yaw < 0 → 右转）。<br>"
        "MediaPipe FaceMesh 的头部姿态解算使用完全相同的 ZYX 欧拉角分解约定，因此符号含义一致。"));

    html += H2(QStringLiteral("3. 视线方向（14 列）"));
    html += Note(QStringLiteral("<b>作用：</b>判断被观察者正在看哪里。这是判断注意力的关键指标。"
        "3.1 节是综合视线方向（已扣除头部姿态影响），最能反映真实的注视方向。"));
    html += FieldTable({
        " gaze_angle_x",
            QString::fromUtf8("<b>头部姿态补偿后的水平注视角度</b>（弧度）。"
            "从被拍摄人物视角出发，<b>正值 = 向左看</b>，<b>负值 = 向右看</b>。这是判断水平注视方向的最重要字段"),
        " gaze_angle_y",
            QString::fromUtf8("<b>头部姿态补偿后的垂直注视角度</b>（弧度）。"
            "正值 = 向下看，负值 = 向上看"),
        " gaze_0_x / gaze_0_y / gaze_0_z",
            "左眼在三维空间中的注视方向向量（单位向量，长度≈1）",
        " gaze_1_x / gaze_1_y / gaze_1_z",
            "右眼在三维空间中的注视方向向量",
    });
    html += Note(QStringLiteral("<b>注意：</b>gaze_angle_x/y 已经扣除了头部转动的影响，反映的是<i>眼球在眼眶内的运动方向</i>。"
        "3D 向量 gaze_0/gaze_1 包含头部姿态，通常不直接用于判断看的方向。"));

    html += Note(QStringLiteral(
        "<b>方向判断原理（OpenFace 视线）：</b><br>"
        "<code>gaze_angle_x = atan2(gaze.x, -gaze.z)</code>，符号由 gaze 向量的 X 分量决定。<br>"
        "相机坐标系中 <b>+X = 画面右侧</b>（即被拍摄人物的左侧），"
        "因此 gaze.x > 0（画面右）→ gaze_angle_x > 0 → 人物在<b>向左看</b>；"
        "gaze.x < 0（画面左）→ gaze_angle_x < 0 → 人物在<b>向右看</b>。<br>"
        "<b>与其他后端的区别：</b>OpenFace 的视线角度符号与 3D 视线向量的 X 分量<b>同号</b>，"
        "与 L2CS-Net 的 gaze_yaw <b>符号相反</b>（L2CS 在 GazeTo3D 公式中有一个负号，见 L2CS-Net 原理说明）。"));

    html += H2(QStringLiteral("3. 视线方向（3.2 眼球关键点 — 280 列）"));
    html += Note(QStringLiteral("<b>作用：</b>提供眼球周围 56 个精细关键点的 2D 和 3D 坐标（每点 5 个值 = X₂D + Y₂D + X₃D + Y₃D + Z₃D，共 280 列），用于高精度视线追踪。"
        "前 28 个点属于左眼，后 28 个属于右眼。适合需要精确分析眼球运动的高级研究场景。"));

    html += H2(QStringLiteral("4. 面部 68 个关键点（340 列）"));
    html += Note(QStringLiteral("<b>作用：</b>提供 68 个 dlib 标准面部关键点的完整 2D 和 3D 坐标信息。"
        "这 68 个点覆盖了眉毛（1-17）、鼻子（28-36）、眼睛（37-48）、嘴巴（49-68）、下颌轮廓（1-17）等面部主要结构。"));
    html += FieldTable({
        " x_0 ~ x_67 / y_0 ~ y_67", "68 个点的 2D 坐标（像素），共 136 列。可直接用于在图像上绘制面部关键点",
        " X_0 ~ X_67 / Y_0 ~ Y_67 / Z_0 ~ Z_67", "68 个点的 3D 坐标（mm，相机坐标系），共 204 列。反映面部的三维结构",
    });

    html += H2(QStringLiteral("5. PDM 参数（40 列）"));
    html += Note(QStringLiteral("<b>作用：</b>Point Distribution Model（点分布模型）参数是 OpenFace 内部的数学模型参数，"
        "用于描述人脸形状的变化。包含 6 个刚性参数（缩放 + 旋转 + 平移）和 34 个非刚性参数（表情相关的形状变化）。"
        "<b>普通用户通常不需要关注这些参数</b>，它们主要供研究人员分析面部形变时使用。"));

    html += H2(QStringLiteral("6. 面部动作单元 / AU（约 35 列）"));
    html += Note(QStringLiteral("<b>作用：</b>Action Units（动作单元）是 FACS（面部动作编码系统）中的基本单位，"
        "每个 AU 对应一组面部肌肉的运动。AU 的激活组合可以判断被观察者的表情。"));
    html += Note(QStringLiteral(
        "<b>常用 AU 示例：</b><br>"
        "• AU01_r（眉心提升）+ AU02_r（眉梢提升）→ 眉部活动，常与惊讶或关注有关<br>"
        "• AU04_r（眉毛下压）+ AU07_r（眼睑收紧）→ 皱眉，常与困惑或愤怒有关<br>"
        "• AU06_r（脸颊提升）+ AU12_r（嘴角提升）→ <b>微笑</b><br>"
        "• AU09_r（鼻部皱起）+ AU10_r（上唇提升）→ 厌恶<br>"
        "• AU25_r（双唇分开）+ AU26_r（下巴下降）→ 张嘴<br>"
        "• AU45_r（眨眼）→ 眨眼动作"));
    html += Note(QStringLiteral(
        "AU 强度列（AU{n}_r）：数值 0~5，0 = 无激活，5 = 最大强度。<br>"
        "AU 激活列（AU{n}_c）：0 = 未激活，1 = 激活。<br>"
        "每个 AU 的详细含义请切换到【数据映射关系】标签页查看。"));

    return MakeReadOnlyTextEdit(html);
}

static QTextEdit* BuildMediaPipeFieldDoc() {
    QString html;
    html += QStringLiteral("<h2 style='color:%1;'>MediaPipe FaceMesh — 最密集的面部网格</h2>").arg(ThemeManager::Instance().Primary());

    html += Note(QStringLiteral("<b>概述：</b>MediaPipe FaceMesh 是 Google 开发的高精度面部网格模型，"
        "使用 468 个密集关键点覆盖整个面部区域，包括面部轮廓、五官细节和前额区域。"
        "相比传统 68 点模型，468 点的覆盖范围更广、密度更高，尤其适合 AR 特效渲染、虚拟化妆、"
        "面部表情驱动（Vtuber 虚拟形象）等需要精确面部网格的应用场景。"));

    html += Note(QStringLiteral("本项目的 MediaPipe 后端使用 PINTO0309 转换的 ONNX 版本模型（face_landmark.tflite → ONNX 转换），"
        "输入人脸裁剪图像后直接输出 468 个关键点的 2D 坐标，然后通过 PnP 算法从 3D 关键点解算出头部姿态角度。"
        "CSV 文件共 941 列。"));

    html += H2(QStringLiteral("1. 基本检测信息（5 列）"));
    html += Note(QStringLiteral("<b>作用：</b>标注帧序号、时间戳和计算出的头部姿态。"));
    html += FieldTable({
        " frame", "帧序号（从 1 开始）",
        " timestamp", "时间戳（秒）。计算公式：timestamp = (frame - 1) / 帧率",
        " pitch", QString::fromUtf8("头部俯仰角（度）。通过 PnP 从 468 个 3D 关键点解算。正值 = 下俯（低头），负值 = 上扬（抬头）"),
        " yaw", QString::fromUtf8("头部偏航角（度）。PnP 解算。正值 = 左转，负值 = 右转"),
        " roll", QString::fromUtf8("头部翻滚角（度）。PnP 解算。正值 = 右倾，负值 = 左倾"),
    });

    html += Note(QStringLiteral(
        "<b>头部姿态方向判断原理（MediaPipe）：</b><br>"
        "通过 solvePnP 从 468 个 3D 关键点到 2D 图像坐标解算旋转矩阵，"
        "再通过 <code>canonical_to_headpose = diag(1, -1, -1)</code> 变换将规范模型坐标系对齐到头部位姿坐标系，"
        "最后使用 ZYX 欧拉角分解 <code>R = Rz(roll) * Ry(yaw) * Rx(pitch)</code> 得到角度。<br>"
        "yaw 的符号判定：<code>yaw = atan2(-R(2,0), cy)</code>。"
        "当 yaw > 0 时，面部法线在相机 X 轴的投影 <code>R(2,0) = -sin(yaw) < 0</code>（朝画面左侧 = 人物右侧），"
        "人物面部转向其<b>左侧</b>（yaw > 0 → 左转）。<br>"
        "MediaPipe 与 OpenFace 使用<b>完全相同的符号约定</b>，因此头部姿态的映射规则一致。"));

    html += H2(QStringLiteral("2. 468 个 FaceMesh 面部关键点（936 列 = 468×2）"));
    html += Note(QStringLiteral("<b>作用：</b>提供面部每个像素级位置的精确坐标，"
        "这是 MediaPipe 的核心输出，也是后续所有分析的基础。"));
    html += FieldTable({
        " x_0 ~ x_467", "468 个关键点的 X 坐标（像素）",
        " y_0 ~ y_467", "468 个关键点的 Y 坐标（像素）",
    });

    html += Note(QStringLiteral(
        "<b>关键点分布说明（按索引范围）：</b><br>"
        "• 0-32（33 个）：<b>面部轮廓及下颌线</b> — 定义整个面部的轮廓边界<br>"
        "• 33-132（100 个）：<b>嘴唇及口周区域</b> — 嘴唇外轮廓 + 口腔内部，可精确刻画唇形和开合<br>"
        "• 133-209（77 个）：<b>左眼及左眉</b> — 左眼轮廓 + 左眉形态，含虹膜（瞳孔）位置<br>"
        "• 210-276（67 个）：<b>右眼及右眉</b> — 右眼轮廓 + 右眉形态<br>"
        "• 277-467（191 个）：<b>前额、鼻梁及面颊</b> — 额头到面颊的完整覆盖<br><br>"
        "<b>总列数 = 5（元数据）+ 468×2（坐标对）= 941 列</b>"
    ));

    html += Note(QStringLiteral("<b>适用场景：</b>"
        "MediaPipe 的密集网格特别适合虚拟化妆（口红、腮红贴合）、AR 面具、"
        "面部表情动作捕捉（通过关键点位移推断表情）等应用。"
        "但 MediaPipe 本身不提供视线方向或面部动作单元（AU）信息。"));

    return MakeReadOnlyTextEdit(html);
}

static QTextEdit* BuildL2csFieldDoc() {
    QString html;
    html += QStringLiteral("<h2 style='color:%1;'>L2CS-Net — 最专注的视线方向估计</h2>").arg(ThemeManager::Instance().Primary());

    html += Note(QStringLiteral("<b>概述：</b>L2CS-Net（L2 Coordinate System Network）是专门用于视线方向估计的深度学习模型。"
        "它以 MobileNetV2 为主干网络，输入 448×448 的人脸裁剪图像，直接输出两个维度的视线角度（俯仰角 pitch 和偏航角 yaw）。"
        "与 OpenFace 的眼球跟踪不同，L2CS-Net 直接从整张面部图像预测视线方向，不需要检测眼球关键点。"));

    html += Note(QStringLiteral("L2CS-Net 的核心优势："
        "能够区分【头部朝向】和【眼球注视方向】。"
        "即使被观察者的头转向一侧，模型仍能推断出眼球真实的注视方向。"
        "这是判断一个人真正在关注什么的非常直接的指标。"));

    html += Note(QStringLiteral("CSV 文件仅 7 列，输出最为精简。"));

    html += H2(QStringLiteral("全部字段（7 列）"));
    html += FieldTable({
        " frame", "帧序号（从 1 开始）",
        " timestamp", "时间戳（秒）",
        " gaze_yaw",
            QString::fromUtf8("<b>视线偏航角</b>（度）。经 EMA 指数平滑处理后输出。<br>"
            "从被拍摄人物视角出发，<b>正值 = 向右看</b>，<b>负值 = 向左看</b>。<br>"
            "原始模型从人脸裁剪图中预测 90 个角度区间（覆盖 -180° ~ +180°）的概率，"
            "然后通过加权平均计算出精确的角度值"),
        " gaze_pitch",
            QString::fromUtf8("<b>视线俯仰角</b>（度）。解码方式同 yaw。<br>"
            "正值 = 向上看，负值 = 向下看"),
        " gaze_x", QString::fromUtf8("<b>3D 视线单位向量的 X 分量</b>。由 yaw/pitch 计算得到。正值 = 视线偏右（相机 X 轴正方向）"),
        " gaze_y", QString::fromUtf8("<b>3D 视线单位向量的 Y 分量</b>。由 pitch 计算得到。正值 = 视线偏下（相机 Y 轴正方向），负值 = 视线偏上"),
        " gaze_z", QString::fromUtf8("<b>3D 视线单位向量的 Z 分量</b>。由 yaw/pitch 计算。负值 = 视线朝前（朝向相机），绝对值接近 1"),
    });
    html += Note(QStringLiteral(
        "<b>方向判断原理（L2CS-Net 视线）：</b><br>"
        "L2CS-Net 使用 GazeTo3D 公式将角度转为 3D 向量：<br>"
        "<code>gaze = (-cos(pitch)·sin(yaw), -sin(pitch), -cos(pitch)·cos(yaw))</code><br>"
        "当 yaw > 0 时，sin(yaw) > 0，gaze.x = -cos(p)·sin(yaw) < 0（画面左→人物向右看）；"
        "当 yaw < 0 时，gaze.x > 0（画面右→人物向左看）。<br>"
        "<b>与 OpenFace 的区别：</b>L2CS 的 gaze_yaw 通过 GazeTo3D 公式中的<b>负号</b>与 gaze.x 反号，"
        "而 OpenFace 的 gaze_angle_x = atan2(gaze.x, -gaze.z) 与 gaze.x 同号。"
        "因此对于同一个注视方向，L2CS 的 gaze_yaw 与 OpenFace 的 gaze_angle_x <b>符号恰好相反</b>，这是由各自的数学模型决定的，不是错误。"));

    html += Note(QStringLiteral(
        "<b>3D 视线向量说明：</b>gaze_x、gaze_y、gaze_z 是由视线角度（yaw、pitch）计算出的 3D 单位向量分量，"
        "三个值满足：x² + y² + z² ≈ 1。这个向量可用于在 3D 场景中投射视线射线。<br><br>"
        "<b>何时使用 3D 向量而非角度：</b>当需要精确计算视线落在空间中的哪个位置时，"
        "使用 3D 向量做射线投射（ray casting）比角度值更方便和准确。"));

    return MakeReadOnlyTextEdit(html);
}

static QTextEdit* BuildMefargAuFieldDoc() {
    QString html;
    html += QStringLiteral("<h2 style='color:%1;'>MEFARG-AU — 最精细的面部表情分析</h2>").arg(ThemeManager::Instance().Primary());

    html += Note(QStringLiteral("<b>概述：</b>MEFARG-AU（Multi-scale Edge Features Aggregated Graph Network for Action Unit Recognition）"
        "是目前最先进的面部动作单元（AU）识别模型之一。"
        "它以 ResNet18 或 Swin Transformer 提取面部特征，"
        "再通过图神经网络（GNN）建模面部不同区域之间的关联关系，"
        "最终输出 41 个面部动作单元（AU）的激活概率。"));

    html += Note(QStringLiteral("<b>什么是动作单元（AU）：</b>"
        "AU 是 FACS（面部动作编码系统）的基本单位。每个 AU 对应一组面部肌肉的运动。"
        "例如 AU12（嘴角提升）是微笑的核心动作，AU4（眉毛下压）是皱眉的核心动作。"
        "人类的表情是由多个 AU 组合而成的，"
        "MEFARG-AU 可以同时识别 41 种 AU 动作，从而精确地分析面部表情的每一个细节。"));

    html += Note(QStringLiteral("CSV 文件共 84 列（2 列元数据 + 41 组 AU × 每组 2 列：概率 + 激活状态）。"));

    html += H2(QStringLiteral("1. 基本检测信息（2 列）"));
    html += FieldTable({
        " frame", "帧序号（从 1 开始）",
        " timestamp", "时间戳（秒）",
    });

    html += H2(QStringLiteral("2. AU 概率与激活状态（41 组 = 82 列）"));
    html += Note(QStringLiteral(
        "<b>数据格式：</b>每两个连续列组成一组，交替排列：<br>"
        "• AU{n}_prob：概率值（0~1），表示该 AU 的激活概率。<b>值越大，该面部动作越明显</b><br>"
        "• AU{n}_active：二值激活状态（0 = 未激活 / 1 = 激活）。当概率 ≥ 0.5 时判定为激活<br><br>"
        "<b>如何使用：</b><br>"
        "• 想判断某个表情是否出现：查 AU{n}_active 列（1 = 该动作出现）<br>"
        "• 想了解动作的强烈程度：查 AU{n}_prob 列（接近 1.0 = 动作非常明显）<br>"
        "• 想判断微表情：关注 prob 值在 0.5~0.7 之间的 AU（可能表示被抑制或短暂的表情）"));

    html += H2(QStringLiteral("3. 全部 41 个 AU 的详细说明"));
    html += Note(QStringLiteral("共 27 个全局 AU + 14 个左右不对称 AU。以下是完整列表："));
    QStringList auRows;
    auRows << "AU1" << QString::fromUtf8("眉心提升（Inner Brow Raiser）— 眉毛内侧向上抬起")
           << "AU2" << QString::fromUtf8("眉梢提升（Outer Brow Raiser）— 眉毛外侧向上抬起")
           << "AU4" << QString::fromUtf8("眉毛下压（Brow Lowerer）— 眉毛整体向下压低")
           << "AU5" << QString::fromUtf8("上眼睑提升（Upper Lid Raiser）— 上眼皮向上抬，眼睛变大")
           << "AU6" << QString::fromUtf8("脸颊提升（Cheek Raiser）— 脸颊肌肉收缩向上推，是微笑的关键动作")
           << "AU7" << QString::fromUtf8("眼睑收紧（Lid Tightener）— 眼轮匝肌收缩，眼睛眯起")
           << "AU9" << QString::fromUtf8("鼻部皱起（Nose Wrinkler）— 鼻梁皮肤起皱，常见于厌恶表情")
           << "AU10" << QString::fromUtf8("上唇提升（Upper Lip Raiser）— 上唇向上抬，可露出上牙")
           << "AU11" << QString::fromUtf8("鼻唇沟加深（Nasolabial Deepener）— 法令纹加深")
           << "AU12" << QString::fromUtf8("嘴角提升（Lip Corner Puller）— 嘴角向上拉，<b>微笑的核心动作</b>")
           << "AU13" << QString::fromUtf8("脸颊鼓起（Sharp Lip Puller）— 嘴角向外拉，面颊膨起")
           << "AU14" << QString::fromUtf8("嘴角收紧（Dimpler）— 嘴角肌肉收紧，可能出现酒窝")
           << "AU15" << QString::fromUtf8("嘴角下压（Lip Corner Depressor）— 嘴角向下拉，常见于悲伤表情")
           << "AU16" << QString::fromUtf8("下唇下压（Lower Lip Depressor）— 下唇向下翻，露下牙")
           << "AU17" << QString::fromUtf8("下巴提升（Chin Raiser）— 下巴向上推，下唇随之上移")
           << "AU18" << QString::fromUtf8("嘴唇缩拢（Lip Pucker）— 嘴唇向前突出呈圆形")
           << "AU19" << QString::fromUtf8("舌头露出（Tongue Show）— 舌头从嘴唇间伸出")
           << "AU20" << QString::fromUtf8("嘴角拉伸（Lip Stretcher）— 嘴角水平向外拉伸，常见于恐惧表情")
           << "AU22" << QString::fromUtf8("嘴唇漏斗状（Lip Funneler）— 嘴唇向外翻转呈漏斗形")
           << "AU23" << QString::fromUtf8("嘴唇收紧（Lip Tightener）— 嘴唇变薄收紧，常见于愤怒表情")
           << "AU24" << QString::fromUtf8("嘴唇互压（Lip Pressor）— 上下唇互相压紧")
           << "AU25" << QString::fromUtf8("双唇分开（Lips Part）— 上下唇分离，可见牙齿间隙")
           << "AU26" << QString::fromUtf8("下巴下降（Jaw Drop）— 下颌骨向下打开，嘴巴张大")
           << "AU27" << QString::fromUtf8("嘴巴拉伸（Mouth Stretch）— 嘴巴大幅张开拉伸")
           << "AU32" << QString::fromUtf8("咬嘴唇（Lip Bite）— 牙齿轻咬下唇")
           << "AU38" << QString::fromUtf8("鼻孔扩张（Nostril Dilator）— 鼻孔向外扩大")
           << "AU39" << QString::fromUtf8("鼻孔收缩（Nostril Compressor）— 鼻孔向内缩小");

    // Asymmetric AUs
    auRows << "AUL1 / AUR1" << QString::fromUtf8("左侧/右侧眉心提升 — 单侧眉毛内侧抬起（不对称表情的关键指标）")
           << "AUL2 / AUR2" << QString::fromUtf8("左侧/右侧眉梢提升 — 单侧眉梢抬起")
           << "AUL4 / AUR4" << QString::fromUtf8("左侧/右侧眉毛下压 — 单侧眉毛压低")
           << "AUL6 / AUR6" << QString::fromUtf8("左侧/右侧脸颊提升 — 单侧脸颊提升（不对称微笑）")
           << "AUL10 / AUR10" << QString::fromUtf8("左侧/右侧上唇提升 — 单侧上唇抬起（轻蔑表情的典型特征）")
           << "AUL12 / AUR12" << QString::fromUtf8("左侧/右侧嘴角提升 — 单侧嘴角上拉（不对称微笑）")
           << "AUL14 / AUR14" << QString::fromUtf8("左侧/右侧嘴角收紧 — 单侧嘴角收紧");

    html += FieldTable(auRows);
    html += Note(QStringLiteral(
        "<b>左右不对称 AU 的特别说明：</b>"
        "这是 MEFARG-AU 模型的一个独特优势。大部分 AU 识别模型只检测对称的面部动作，"
        "但 MEFARG-AU 能区分左右两侧独立的面部动作。这对于检测<b>不对称表情</b>"
        "（如单侧嘴角上拉的轻蔑表情、单侧眉梢提升的怀疑表情）非常有价值。"));

    html += Note(QStringLiteral("<b>总列数 = 2（元数据）+ 41×2（概率 + 激活）= 84 列</b>"));

    return MakeReadOnlyTextEdit(html);
}

// ============================================================================
// Mapping Documentation Builders
// ============================================================================

static QTextEdit* BuildOpenFaceMappingDoc() {
    QString html;
    html += QStringLiteral("<h2 style='color:%1;'>OpenFace — 数值 → 文字描述对照</h2>").arg(ThemeManager::Instance().Primary());
    html += Note(QStringLiteral("以下说明了如何将 OpenFace 输出的数值转换为通俗易懂的文字描述。"
        "这些映射关系在任务详情界面的分析数据展示中被使用。"));

    html += H2(QStringLiteral("头部姿态 → 通俗描述"));
    html += Note(QStringLiteral("<b>用途：</b>判断被拍摄者面部的朝向。"
        "可用于分析被拍摄者是否在看画面中的某个位置，或者是否注意力偏移。"));
    html += MappingTable({
        "pose_Rx（俯仰）", "< -25°", "明显上扬（头部明显后仰）",
        "pose_Rx（俯仰）", "-25° ~ -10°", "略微上扬（头部稍向后）",
        "pose_Rx（俯仰）", "-10° ~ 10°", "基本水平（自然平视状态）",
        "pose_Rx（俯仰）", "10° ~ 25°", "略微下俯（头部稍向前倾）",
        "pose_Rx（俯仰）", "> 25°", "明显下俯（头部明显低垂）",

        "pose_Ry（偏航）", "< -25°", "明显右转（面部大幅向右）",
        "pose_Ry（偏航）", "-25° ~ -10°", "略微右转（面部稍向右）",
        "pose_Ry（偏航）", "-10° ~ 10°", "正面朝向（基本正对镜头）",
        "pose_Ry（偏航）", "10° ~ 25°", "略微左转（面部稍向左）",
        "pose_Ry（偏航）", "> 25°", "明显左转（面部大幅向左）",

        "pose_Rz（翻滚）", "< -15°", "明显左倾（头部明显向左肩倾斜）",
        "pose_Rz（翻滚）", "-15° ~ -6°", "略微左倾（头部稍向左肩倾斜）",
        "pose_Rz（翻滚）", "-6° ~ 6°", "基本水平（头部端正）",
        "pose_Rz（翻滚）", "6° ~ 15°", "略微右倾（头部稍向右肩倾斜）",
        "pose_Rz（翻滚）", "> 15°", "明显右倾（头部明显向右肩倾斜）",
    });

    html += H2(QStringLiteral("视线方向 → 通俗描述"));
    html += Note(QStringLiteral("<b>用途：</b>判断被观察者真正在看哪里。"
        "注意 gaze_angle_x/y 是扣除头部姿态后的眼球注视方向，更能反映真实的注意力方向。"));
    html += MappingTable({
        "gaze_angle_x（水平注视）", "< -0.3 rad", "明显向右看（视线大幅偏右）",
        "gaze_angle_x（水平注视）", "-0.3 ~ -0.1 rad", "略微向右看（视线稍偏右）",
        "gaze_angle_x（水平注视）", "-0.1 ~ 0.1 rad", "正视前方（水平方向无偏转）",
        "gaze_angle_x（水平注视）", "0.1 ~ 0.3 rad", "略微向左看（视线稍偏左）",
        "gaze_angle_x（水平注视）", "> 0.3 rad", "明显向左看（视线大幅偏左）",

        "gaze_angle_y（垂直注视）", "< -0.3 rad", "明显向上看（视线大幅向上）",
        "gaze_angle_y（垂直注视）", "-0.3 ~ -0.1 rad", "略微向上看（视线稍向上）",
        "gaze_angle_y（垂直注视）", "-0.1 ~ 0.1 rad", "水平视线（垂直方向无偏转）",
        "gaze_angle_y（垂直注视）", "0.1 ~ 0.3 rad", "略微向下看（视线稍向下）",
        "gaze_angle_y（垂直注视）", "> 0.3 rad", "明显向下看（视线大幅向下）",
    });

    html += H2(QStringLiteral("AU 强度 → 通俗描述"));
    html += Note(QStringLiteral("<b>用途：</b>判断某个面部动作的剧烈程度。"));
    html += MappingTable({
        "AU{n}_r", "0.0 ~ 0.4", "未激活 — 该面部动作未发生，对应肌肉处于放松状态",
        "AU{n}_r", "0.5 ~ 1.9", "轻微激活 — 面部动作刚刚出现，强度较弱，可能为微表情",
        "AU{n}_r", "2.0 ~ 3.9", "中等激活 — 面部动作明显，肉眼可清晰观察到",
        "AU{n}_r", "4.0 ~ 5.0", "强烈激活 — 面部动作非常明显，幅度达到最大范围",
    });

    html += H2(QStringLiteral("检测置信度 → 通俗描述"));
    html += MappingTable({
        "confidence", "< 0.5", "较低 — 面部检测不确定，该帧的分析结果仅供参考，建议谨慎使用",
        "confidence", "0.5 ~ 0.7", "一般 — 检测可信度中等，基本分析结果可用",
        "confidence", "0.7 ~ 0.85", "较高 — 检测较为可靠，分析结果值得信赖",
        "confidence", "> 0.85", "极高 — 检测非常可靠，关键点定位准确，适合精细分析",
    });

    return MakeReadOnlyTextEdit(html);
}

static QTextEdit* BuildMediaPipeMappingDoc() {
    QString html;
    html += QStringLiteral("<h2 style='color:%1;'>MediaPipe FaceMesh — 数值 → 文字描述对照</h2>").arg(ThemeManager::Instance().Primary());
    html += Note(QStringLiteral("MediaPipe 的头部姿态映射规则与 OpenFace 相同，但使用了自己的 PnP 解算角度。"));

    html += H2(QStringLiteral("头部姿态 → 通俗描述"));
    html += MappingTable({
        "pitch（俯仰）", "< -25°", "明显上扬（脸朝上）",
        "pitch（俯仰）", "-25° ~ -10°", "略微上扬",
        "pitch（俯仰）", "-10° ~ 10°", "基本水平",
        "pitch（俯仰）", "10° ~ 25°", "略微下俯",
        "pitch（俯仰）", "> 25°", "明显下俯（脸朝下）",

        "yaw（偏航）", "< -25°", "明显右转",
        "yaw（偏航）", "-25° ~ -10°", "略微右转",
        "yaw（偏航）", "-10° ~ 10°", "正面朝向",
        "yaw（偏航）", "10° ~ 25°", "略微左转",
        "yaw（偏航）", "> 25°", "明显左转",

        "roll（翻滚）", "< -15°", "明显左倾",
        "roll（翻滚）", "-15° ~ -6°", "略微左倾",
        "roll（翻滚）", "-6° ~ 6°", "基本水平",
        "roll（翻滚）", "6° ~ 15°", "略微右倾",
        "roll（翻滚）", "> 15°", "明显右倾",
    });

    html += H2(QStringLiteral("468 个关键点坐标"));
    html += Note(QStringLiteral(
        "<b>说明：</b>468 个 FaceMesh 关键点的 x/y 坐标（像素）<b>不进行数值→文字的映射</b>，"
        "而是直接用于可视化目的：在图像上绘制面部线框网格、面颊轮廓曲线、"
        "虹膜（瞳孔）位置标记等。关键点坐标是原始图像空间的像素值，"
        "其含义由 FaceMesh 标准拓扑结构定义（见字段含义标签页中的分布说明）。"));

    html += Note(QStringLiteral(
        "<b>实际应用参考：</b><br>"
        "• 想要判断面部朝向 → 使用 pitch/yaw/roll 的映射规则<br>"
        "• 想要绘制面部网格 → 使用 x_i / y_i 坐标直接在画面上绘制<br>"
        "• 想要计算眼睛开合度 → 使用眼部关键点坐标计算上下眼睑的垂直距离<br>"
        "• 想要计算嘴巴宽度 → 使用嘴角关键点坐标的水平距离"));

    return MakeReadOnlyTextEdit(html);
}

static QTextEdit* BuildL2csMappingDoc() {
    QString html;
    html += QStringLiteral("<h2 style='color:%1;'>L2CS-Net — 数值 → 文字描述对照</h2>").arg(ThemeManager::Instance().Primary());
    html += Note(QStringLiteral("L2CS-Net 输出的是纯粹的视线方向数据。以下说明如何根据角度值和 3D 向量值判断视线方向。"));

    html += H2(QStringLiteral("视线方向（角度值）→ 通俗描述"));
    html += Note(QStringLiteral("<b>用途：</b>最直观地判断被观察者正在看哪里。正前方即镜头方向。"));
    html += MappingTable({
        "gaze_yaw（水平视线）", "< -25°", "明显向左看 — 被观察者正在关注画面或环境左侧",
        "gaze_yaw（水平视线）", "-25° ~ -10°", "略微向左看 — 视线稍偏画面左侧",
        "gaze_yaw（水平视线）", "-10° ~ 10°", "正视前方 — 视线基本水平，指向画面中央",
        "gaze_yaw（水平视线）", "10° ~ 25°", "略微向右看 — 视线稍偏画面右侧",
        "gaze_yaw（水平视线）", "> 25°", "明显向右看 — 被观察者正在关注画面或环境右侧",

        "gaze_pitch（垂直视线）", "< -25°", "明显向下看 — 被观察者正在关注下方（如桌面或双手）",
        "gaze_pitch（垂直视线）", "-25° ~ -10°", "略微向下看 — 视线稍偏下方",
        "gaze_pitch（垂直视线）", "-10° ~ 10°", "水平正视 — 视线在垂直方向无偏转",
        "gaze_pitch（垂直视线）", "10° ~ 25°", "略微向上看 — 视线稍偏上方",
        "gaze_pitch（垂直视线）", "> 25°", "明显向上看 — 被观察者正在关注上方（如天花板或屏幕上方）",
    });

    html += H2(QStringLiteral("3D 视线向量 → 通俗描述"));
    html += Note(QStringLiteral("<b>用途：</b>用于计算视线在 3D 空间中的精确投射方向。"
        "向量分量的大小直接反映视线在各个方向上的偏转程度。"));
    html += MappingTable({
        "gaze_x", "≈ 0（范围 -0.1 ~ 0.1）", "水平方向无偏转，正视前方",
        "gaze_x", "> 0.2", "视线偏右（相机 X 轴正方向）",
        "gaze_x", "< -0.2", "视线偏左（相机 X 轴负方向）",
        "gaze_y", "≈ 0（范围 -0.1 ~ 0.1）", "垂直方向无偏转，平视",
        "gaze_y", "> 0.2", "视线偏下（相机 Y 轴正方向）",
        "gaze_y", "< -0.2", "视线偏上（相机 Y 轴负方向）",
        "gaze_z", "≈ -1", "视线朝前（相机 Z 轴负方向 = 朝向相机）",
    });
    html += Note(QStringLiteral("<b>提示：</b>3D 向量三个分量满足 x² + y² + z² ≈ 1（单位向量）。"
        "大多数帧的 gaze_z 都在 -0.95 到 -1.0 之间（几乎完全朝前），"
        "这是因为人类大多数的注视方向都接近正前方。"));

    html += H2(QStringLiteral("视线分析 → 通俗理解"));
    html += Note(QStringLiteral(
        "<b>综合判断规则：</b><br>"
        "• gaze_yaw 和 gaze_pitch 都在 ±8° 以内 → 被观察者<b>正视画面中央</b><br>"
        "• 只有 gaze_yaw 偏转超过 15° → 被观察者在<b>水平方向扫视</b>（左右阅读行为常见）<br>"
        "• 只有 gaze_pitch 偏转超过 15° → 被观察者在<b>垂直方向扫视</b>（上下浏览内容）<br>"
        "• 两者同时偏转 → 被观察者在<b>斜向注视</b>画面或环境的某个区域<br>"
        "• 偏转角超过 30° → 视线方向已<b>接近视野边缘</b>，被观察者的注意力可能已偏离屏幕"));

    return MakeReadOnlyTextEdit(html);
}

static QTextEdit* BuildMefargAuMappingDoc() {
    QString html;
    html += QStringLiteral("<h2 style='color:%1;'>MEFARG-AU — 数值 → 文字描述对照</h2>").arg(ThemeManager::Instance().Primary());
    html += Note(QStringLiteral("以下说明了如何将 MEFARG-AU 输出的概率值转换为对面部表情的通俗理解。"));

    html += H2(QStringLiteral("AU 激活状态判断"));
    html += Note(QStringLiteral("<b>用途：</b>快速判断某个面部动作是否正在发生。这是最常用的判断方法。"));
    html += MappingTable({
        "AU{n}_prob", "> 0.5", QStringLiteral("<span style='color:%1;'>● 激活</span> — 该面部动作正在发生，对应肌肉处于收缩状态").arg(ThemeManager::Instance().Success()),
        "AU{n}_prob", "≤ 0.5", QStringLiteral("<span style='color:%1;'>○ 未激活</span> — 该面部动作未发生，对应肌肉处于放松状态").arg(ThemeManager::Instance().TextDim()),
    });

    html += H2(QStringLiteral("AU 强度层级"));
    html += Note(QStringLiteral("<b>用途：</b>判断面部动作的明显程度。对于分析微表情（概率 0.5~0.7）特别有用。"));
    html += MappingTable({
        "AU{n}_prob", "0.0 ~ 0.3", "极弱 — 基本确定该 AU 未激活，脸部对应区域自然放松",
        "AU{n}_prob", "0.3 ~ 0.5", "弱 — 可能处于临界状态，或该 AU 以极微弱幅度出现",
        "AU{n}_prob", "0.5 ~ 0.7", "中等激活 — 面部动作已经出现，但幅度有限（<b>可能为微表情</b>）",
        "AU{n}_prob", "0.7 ~ 1.0", "强激活 — 面部动作明显，肉眼可以清晰观察到对应的肌肉运动",
    });

    html += H2(QStringLiteral("AU 按面部区域的分类"));
    html += Note(QStringLiteral("<b>用途：</b>按面部区域理解不同的 AU 组合代表的含义。"
        "在任务详情界的面部表情分析中，AU 就是按照这些区域分组展示的。"));
    html += Note(QStringLiteral(
        "<b>眉毛及眼部区域</b>（AU1, AU2, AU4, AU5, AU7, AUL1/R1, AUL2/R2, AUL4/R4）<br>"
        "控制眉毛的提升/下压和眼睛的睁开/收紧。这些 AU 常见于情绪表达的上半脸部分：<br>"
        "• 眉部动作（AU1+AU2+AU4）主导惊讶、关注或困惑的表情<br>"
        "• 眼部动作（AU5+AU7）主导瞪眼或眯眼的表情"));

    html += Note(QStringLiteral(
        "<b>鼻部区域</b>（AU9, AU38, AU39）<br>"
        "控制鼻子的皱起和鼻孔的变化。鼻部皱起（AU9）是<b>厌恶表情的标志性动作</b>。"));

    html += Note(QStringLiteral(
        "<b>口唇及下颌区域</b>（AU10~AU27, AU32, AUL10/R10, AUL12/R12, AUL14/R14）<br>"
        "这是 AU 数量最多的区域，控制嘴巴的各种形态变化：<br>"
        "• 嘴角提升（AU12）= 微笑核心动作<br>"
        "• 嘴角下压（AU15）= 悲伤/沮丧<br>"
        "• 嘴唇收紧（AU23+AU24）= 愤怒/决心<br>"
        "• 下巴下降（AU26）+ 双唇分开（AU25）= 张嘴/惊讶<br>"
        "• 嘴唇缩拢（AU18）= 思考/犹豫"));

    html += Note(QStringLiteral(
        "<b>面颊区域</b>（AU6, AUL6, AUR6）<br>"
        "控制脸颊的提拉和鼓起。脸颊提升（AU6）配合嘴角提升（AU12）构成<b>真正的微笑（Duchenne smile）</b>。<br>"
        "如果只有 AU12 激活而 AU6 未激活，可能是<b>社交性的假笑</b>。"));

    html += H2(QStringLiteral("常见表情的 AU 组合"));
    html += Note(QStringLiteral("<b>用途：</b>将多个 AU 的激活状态综合起来判断整体表情。"
        "这是最接近人类理解表情的方式——不是看单个 AU，而是看多个 AU 的组合模式。"));

    html += Note(QStringLiteral(
        "<b>😊 微笑（Happiness / Smile）</b><br>"
        "核心组合：AU6（脸颊提升）+ AU12（嘴角提升）<br>"
        "说明：自然的微笑需要面颊和嘴角同时参与。仅有嘴角提升而无面颊提升可能是假笑。<br><br>"

        "<b>😲 惊讶（Surprise）</b><br>"
        "核心组合：AU1+AU2（眉部提升）+ AU5（上眼睑提升）+ AU26（下巴下降/张嘴）<br>"
        "说明：惊讶时眉毛会上扬，眼睛会睁大，嘴巴会自然张开。<br><br>"

        "<b>😠 愤怒（Anger）</b><br>"
        "核心组合：AU4（眉毛下压）+ AU5（上眼睑提升）+ AU7（眼睑收紧）+ AU23（嘴唇收紧）<br>"
        "说明：愤怒时眉头会压低，眼睛会瞪视（眼睑收紧），嘴唇会变薄收紧。<br><br>"

        "<b>😢 悲伤（Sadness）</b><br>"
        "核心组合：AU1（眉心提升）+ AU4（眉毛下压）+ AU15（嘴角下压）<br>"
        "说明：悲伤时眉心会向上聚拢，同时眉毛压低，嘴角向下拉。<br><br>"

        "<b>😨 恐惧（Fear）</b><br>"
        "核心组合：AU1+AU2+AU4（眉部综合）+ AU5（上眼睑提升）+ AU7（眼睑收紧）+ AU20（嘴角拉伸）+ AU26（下巴下降）<br>"
        "说明：恐惧涉及面部最多的 AU，眉毛可能同时提升和下压、眼睛睁大、嘴角向外拉伸、嘴巴张开。<br><br>"

        "<b>🤢 厌恶（Disgust）</b><br>"
        "核心组合：AU9（鼻部皱起）+ AU10（上唇提升）+ AU17（下巴提升）<br>"
        "说明：厌恶的标志性动作是鼻子皱起，同时上唇上提，下巴向上推。"));

    html += Note(QStringLiteral(
        "<b>特别提示：</b>以上是基于 FACS 理论的理想化组合模式。"
        "在真实场景中，表情往往是多种情绪的混合，且受到文化背景、个人习惯和情境因素的影响。"
        "MEFARG-AU 提供的 41 个 AU 数据可以进行比上述列表更加精细和个性化的表情分析。"));

    return MakeReadOnlyTextEdit(html);
}

// ============================================================================
// BuildContent
// ============================================================================

void CsvReferenceDialog::BuildContent() {
    field_stack_->addWidget(BuildOpenFaceFieldDoc());
    field_stack_->addWidget(BuildMediaPipeFieldDoc());
    field_stack_->addWidget(BuildL2csFieldDoc());
    field_stack_->addWidget(BuildMefargAuFieldDoc());

    mapping_stack_->addWidget(BuildOpenFaceMappingDoc());
    mapping_stack_->addWidget(BuildMediaPipeMappingDoc());
    mapping_stack_->addWidget(BuildL2csMappingDoc());
    mapping_stack_->addWidget(BuildMefargAuMappingDoc());
}

}  // namespace ui
}  // namespace omniface
