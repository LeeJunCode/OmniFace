#include "omniface/ui/pages/task_history_page.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include <cmath>

#include "omniface/db/image_repo.h"
#include "omniface/db/task_repo.h"
#include "omniface/db/video_repo.h"
#include "omniface/pipeline/framework/config.h"
#include "omniface/ui/theme.h"

namespace omniface {
namespace ui {

// ═══════════════════════════════════════════════════════════════
// TaskTableModel
// ═══════════════════════════════════════════════════════════════

TaskTableModel::TaskTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void TaskTableModel::SetRecords(std::vector<TaskDisplayInfo> records) {
    beginResetModel();
    records_ = std::move(records);
    checked_rows_.clear();
    endResetModel();
}

const TaskDisplayInfo* TaskTableModel::GetRecord(int row) const {
    if (row < 0 || static_cast<size_t>(row) >= records_.size())
        return nullptr;
    return &records_[row];
}

int TaskTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(records_.size());
}

int TaskTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant TaskTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};
    int row = index.row();
    if (row < 0 || static_cast<size_t>(row) >= records_.size()) return {};

    const auto& info = records_[row];
    const auto& t = info.task;

    if (role == Qt::CheckStateRole && index.column() == ColCheck)
        return checked_rows_.contains(row) ? Qt::Checked : Qt::Unchecked;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColName: {
            QString n = QString::fromStdString(t.name);
            return n.isEmpty() ? QStringLiteral("—") : n;
        }
        case ColDate:        return FormatDate(t.start_time);
        case ColVideoName:   return info.video_name.isEmpty() ? QStringLiteral("—") : info.video_name;
        case ColImageName:   return info.image_name.isEmpty() ? QStringLiteral("—") : info.image_name;
        case ColBackend:     return QString::fromUtf8(AnalysisBackendDisplayName(t.backend));
        case ColOutputTypes: return FormatOutputTypes(t.output_types);
        case ColStatus:      return StatusText(t.status);
        case ColFrames:      return FormatFrameCount(t.total_frames);
        case ColAvgFps:      return t.avg_fps > 0.0 ? QStringLiteral("%1").arg(t.avg_fps, 0, 'f', 1) : QStringLiteral("—");
        case ColDescription: {
            QString d = QString::fromStdString(t.description);
            return d.isEmpty() ? QStringLiteral("—") : d;
        }
        case ColNotes: {
            QString n = QString::fromStdString(t.notes);
            return n.isEmpty() ? QStringLiteral("—") : n;
        }
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColName:
        case ColVideoName:
        case ColImageName:
        case ColDescription:
        case ColNotes:
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        default:
            return Qt::AlignCenter;
        }
    }

    if (role == Qt::UserRole) {
        return static_cast<qlonglong>(t.id);
    }

    return {};
}

bool TaskTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid()) return false;

    if (index.column() == ColCheck && role == Qt::CheckStateRole) {
        if (value.toInt() == Qt::Checked)
            checked_rows_.insert(index.row());
        else
            checked_rows_.remove(index.row());
        emit dataChanged(index, index, {role});
        return true;
    }

    if (index.column() == ColName && role == Qt::EditRole) {
        int row = index.row();
        if (row < 0 || static_cast<size_t>(row) >= records_.size()) return false;
        records_[row].task.name = value.toString().toStdString();
        emit dataChanged(index, index, {Qt::DisplayRole});
        return true;
    }

    return false;
}

Qt::ItemFlags TaskTableModel::flags(const QModelIndex& index) const {
    auto f = QAbstractTableModel::flags(index);
    if (index.column() == ColCheck)
        f |= Qt::ItemIsUserCheckable;
    if (index.column() == ColName)
        f |= Qt::ItemIsEditable;
    return f;
}

QSet<int64_t> TaskTableModel::CheckedIds() const {
    QSet<int64_t> ids;
    for (int row : checked_rows_) {
        if (row >= 0 && static_cast<size_t>(row) < records_.size())
            ids.insert(records_[row].task.id);
    }
    return ids;
}

void TaskTableModel::ToggleAll(bool checked) {
    beginResetModel();
    checked_rows_.clear();
    if (checked) {
        for (int i = 0; i < static_cast<int>(records_.size()); ++i)
            checked_rows_.insert(i);
    }
    endResetModel();
}

QString TaskTableModel::FormatDate(const std::string& iso) {
    if (iso.empty()) return QStringLiteral("—");
    QDateTime dt = QDateTime::fromString(QString::fromStdString(iso), Qt::ISODateWithMs);
    if (dt.isValid())
        return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    // Fallback
    QString s = QString::fromStdString(iso);
    if (s.length() >= 19)
        return s.left(10) + QStringLiteral(" ") + s.mid(11, 8);
    if (s.length() >= 10)
        return s.left(10);
    return s;
}

QString TaskTableModel::StatusText(const std::string& status) {
    if (status == "completed") return QStringLiteral("已完成");
    if (status == "running")   return QStringLiteral("处理中");
    if (status == "failed")    return QStringLiteral("失败");
    if (status == "aborted")   return QStringLiteral("已中止");
    return QString::fromStdString(status);
}

QString TaskTableModel::FormatFrameCount(int frames) {
    if (frames <= 0) return QStringLiteral("—");
    if (frames >= 10000)
        return QStringLiteral("%1万").arg(frames / 10000.0, 0, 'f', 1);
    return QString::number(frames);
}

QString TaskTableModel::FormatOutputTypes(const std::string& json) {
    if (json.empty()) return QStringLiteral("—");

    static const QHash<QString, QString> kNames = {
        {"openface_native", QStringLiteral("OpenFace原生")},
        {"aligned_faces", QStringLiteral("对齐人脸")},
        {"landmarks_68", QStringLiteral("68点关键点")},
        {"head_pose", QStringLiteral("头部姿态")},
        {"gaze", QStringLiteral("视线估计")},
        {"au", QStringLiteral("AU")},
        {"landmarks_468", QStringLiteral("468点关键点")},
        {"gaze_yaw_pitch", QStringLiteral("视线偏航/俯仰")},
        {"gaze_3d_vector", QStringLiteral("3D视线向量")},
        {"au_41", QStringLiteral("41个AU")},
        {"face_detection", QStringLiteral("人脸检测")},
        {"tracking", QStringLiteral("人脸跟踪")},
        {"recognition", QStringLiteral("人脸识别")},
    };

    QJsonDocument doc = QJsonDocument::fromJson(QString::fromStdString(json).toUtf8());
    if (!doc.isArray()) return QString::fromStdString(json);

    QStringList names;
    for (const auto& v : doc.array()) {
        QString key = v.toString();
        names.append(kNames.value(key, key));
    }
    return names.isEmpty() ? QStringLiteral("—") : names.join(QStringLiteral("、"));
}

QVariant TaskTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColCheck:       return QStringLiteral("");
    case ColName:        return QStringLiteral("名称");
    case ColDate:        return QStringLiteral("日期");
    case ColVideoName:   return QStringLiteral("视频名");
    case ColImageName:   return QStringLiteral("图片名");
    case ColBackend:     return QStringLiteral("后端");
    case ColOutputTypes: return QStringLiteral("产出内容");
    case ColStatus:      return QStringLiteral("状态");
    case ColFrames:      return QStringLiteral("帧数");
    case ColAvgFps:      return QStringLiteral("平均FPS");
    case ColDescription: return QStringLiteral("描述");
    case ColNotes:       return QStringLiteral("备注");
    }
    return {};
}

// ═══════════════════════════════════════════════════════════════
// TaskFilterProxy
// ═══════════════════════════════════════════════════════════════

TaskFilterProxy::TaskFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent) {}

void TaskFilterProxy::SetBackendFilter(const QString& backend) {
    backend_filter_ = backend;
    beginFilterChange();
    endFilterChange();
}

void TaskFilterProxy::SetStatusFilter(const QString& status) {
    status_filter_ = status;
    beginFilterChange();
    endFilterChange();
}

void TaskFilterProxy::SetDateFrom(const QDate& date) {
    date_from_ = date;
    beginFilterChange();
    endFilterChange();
}

void TaskFilterProxy::SetDateTo(const QDate& date) {
    date_to_ = date;
    beginFilterChange();
    endFilterChange();
}

void TaskFilterProxy::SetTextFilter(const QString& text) {
    text_filter_ = text.trimmed();
    beginFilterChange();
    endFilterChange();
}

bool TaskFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    auto* model = qobject_cast<TaskTableModel*>(sourceModel());
    if (!model) return true;

    const auto* info = model->GetRecord(sourceRow);
    if (!info) return false;

    const auto& t = info->task;

    if (!backend_filter_.isEmpty()) {
        if (QString::fromStdString(t.backend) != backend_filter_)
            return false;
    }

    if (!status_filter_.isEmpty()) {
        if (QString::fromStdString(t.status) != status_filter_)
            return false;
    }

    if (!text_filter_.isEmpty()) {
        QString name = QString::fromStdString(t.name);
        QString backend = QString::fromStdString(t.backend);
        QString desc = QString::fromStdString(t.description);
        QString notes = QString::fromStdString(t.notes);
        if (!name.contains(text_filter_, Qt::CaseInsensitive) &&
            !backend.contains(text_filter_, Qt::CaseInsensitive) &&
            !desc.contains(text_filter_, Qt::CaseInsensitive) &&
            !notes.contains(text_filter_, Qt::CaseInsensitive) &&
            !info->video_name.contains(text_filter_, Qt::CaseInsensitive) &&
            !info->image_name.contains(text_filter_, Qt::CaseInsensitive))
            return false;
    }

    if (date_from_.isValid() || date_to_.isValid()) {
        QString ts = QString::fromStdString(t.start_time);
        QDate taskDate;
        if (ts.length() >= 10)
            taskDate = QDate::fromString(ts.left(10), QStringLiteral("yyyy-MM-dd"));
        if (!taskDate.isValid())
            taskDate = QDate::fromString(ts.left(10), QStringLiteral("yyyy/MM/dd"));
        if (taskDate.isValid()) {
            if (date_from_.isValid() && taskDate < date_from_)
                return false;
            if (date_to_.isValid() && taskDate > date_to_)
                return false;
        }
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════
// TaskHistoryPage
// ═══════════════════════════════════════════════════════════════

TaskHistoryPage::TaskHistoryPage(std::shared_ptr<db::TaskRepo> task_repo,
                                 std::shared_ptr<db::VideoRepo> video_repo,
                                 std::shared_ptr<db::ImageRepo> image_repo,
                                 QWidget* parent)
    : QWidget(parent)
    , task_repo_(std::move(task_repo))
    , video_repo_(std::move(video_repo))
    , image_repo_(std::move(image_repo)) {
    SetupUI();
    LoadTasks();
    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged,
            this, &TaskHistoryPage::ReapplyTheme);
}

void TaskHistoryPage::SetupUI() {
    auto& tm = ThemeManager::Instance();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(8);

    // ── Top bar ──────────────────────────────────────────────

    auto* top_bar = new QHBoxLayout();
    top_bar->setSpacing(8);

    auto makeLabel = [this, &tm](const QString& text) {
        auto* label = new QLabel(text, this);
        label->setStyleSheet(QString("QLabel { color: %1; }").arg(tm.TextMuted()));
        return label;
    };

    auto makeCombo = [this, &tm]() {
        auto* combo = new QComboBox(this);
        combo->setFixedWidth(130);
        combo->setStyleSheet(QString(
            "QComboBox {"
            "  background: %1; border: 1px solid %2;"
            "  border-radius: 4px; padding: 5px 8px; color: %3;"
            "}"
            "QComboBox:focus { border-color: %4; }"
            "QComboBox QAbstractItemView {"
            "  background: %1; color: %3;"
            "  selection-background-color: %5; border: 1px solid %2;"
            "}"
            "QComboBox::drop-down { border: none; width: 18px; }").arg(
                tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary(), tm.SelectedBg()));
        return combo;
    };

    search_box_ = new QLineEdit(this);
    search_box_->setPlaceholderText(QStringLiteral("搜索任务..."));
    search_box_->setClearButtonEnabled(true);
    search_box_->setFixedWidth(200);
    search_box_->setStyleSheet(QString(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 8px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }").arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));
    connect(search_box_, &QLineEdit::textChanged, this, &TaskHistoryPage::OnSearchTextChanged);
    top_bar->addWidget(search_box_);

    top_bar->addWidget(makeLabel(QStringLiteral("后端:")));
    backend_combo_ = makeCombo();
    backend_combo_->addItem(QStringLiteral("全部"), QString());
    backend_combo_->addItem(QStringLiteral("面部综合分析 (OpenFace)"), QStringLiteral("openface"));
    backend_combo_->addItem(QStringLiteral("人脸关键点 (MediaPipe)"), QStringLiteral("mediapipe"));
    backend_combo_->addItem(QStringLiteral("视线追踪 (L2CS-Net)"), QStringLiteral("l2cs"));
    backend_combo_->addItem(QStringLiteral("表情识别 (MEFARG-AU)"), QStringLiteral("mefarg_au"));
    backend_combo_->addItem(QStringLiteral("仅检测跟踪"), QStringLiteral("none"));
    connect(backend_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TaskHistoryPage::OnBackendFilterChanged);
    top_bar->addWidget(backend_combo_);

    top_bar->addWidget(makeLabel(QStringLiteral("状态:")));
    status_combo_ = makeCombo();
    status_combo_->addItem(QStringLiteral("全部"), QString());
    status_combo_->addItem(QStringLiteral("已完成"), QStringLiteral("completed"));
    status_combo_->addItem(QStringLiteral("处理中"), QStringLiteral("running"));
    status_combo_->addItem(QStringLiteral("失败"), QStringLiteral("failed"));
    status_combo_->addItem(QStringLiteral("已中止"), QStringLiteral("aborted"));
    connect(status_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TaskHistoryPage::OnStatusFilterChanged);
    top_bar->addWidget(status_combo_);

    top_bar->addWidget(makeLabel(QStringLiteral("日期:")));

    date_from_ = new DatePicker(this);
    date_from_->setSpecialValueText(QStringLiteral("从"));
    date_from_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    date_from_->blockSignals(true);
    date_from_->setDate(QDate());
    date_from_->blockSignals(false);
    connect(date_from_, &DatePicker::dateChanged, this, &TaskHistoryPage::OnDateFromChanged);
    top_bar->addWidget(date_from_);

    date_to_ = new DatePicker(this);
    date_to_->setSpecialValueText(QStringLiteral("至"));
    date_to_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    date_to_->blockSignals(true);
    date_to_->setDate(QDate());
    date_to_->blockSignals(false);
    connect(date_to_, &DatePicker::dateChanged, this, &TaskHistoryPage::OnDateToChanged);
    top_bar->addWidget(date_to_);

    auto* reset_btn = new QPushButton(QStringLiteral("重置"), this);
    reset_btn_ = reset_btn;
    reset_btn->setCursor(Qt::PointingHandCursor);
    reset_btn->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 12px;"
        "}"
        "QPushButton:hover { background: %3; }").arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(reset_btn, &QPushButton::clicked, this, &TaskHistoryPage::OnResetFilters);
    top_bar->addWidget(reset_btn);

    top_bar->addStretch();

    auto* refresh_btn = new QPushButton(QStringLiteral("刷新"), this);
    refresh_btn_ = refresh_btn;
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 16px;"
        "}"
        "QPushButton:hover { background: %3; }").arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));
    connect(refresh_btn, &QPushButton::clicked, this, &TaskHistoryPage::Refresh);
    top_bar->addWidget(refresh_btn);

    root->addLayout(top_bar);

    // ── Table ────────────────────────────────────────────────

    model_ = new TaskTableModel(this);
    proxy_ = new TaskFilterProxy(this);
    proxy_->setSourceModel(model_);
    proxy_->setSortRole(Qt::DisplayRole);
    proxy_->setDynamicSortFilter(true);

    table_view_ = new QTableView(this);
    table_view_->setModel(proxy_);
    table_view_->setSortingEnabled(true);
    table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_view_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_view_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    table_view_->verticalHeader()->setVisible(false);
    table_view_->setShowGrid(true);

    auto* hdr = table_view_->horizontalHeader();
    hdr->setStretchLastSection(true);
    hdr->setSectionResizeMode(QHeaderView::Interactive);
    hdr->setDefaultAlignment(Qt::AlignCenter);
    hdr->setMinimumSectionSize(50);
    hdr->resizeSection(TaskTableModel::ColCheck,       36);
    hdr->resizeSection(TaskTableModel::ColName,        130);
    hdr->resizeSection(TaskTableModel::ColDate,        140);
    hdr->resizeSection(TaskTableModel::ColVideoName,   110);
    hdr->resizeSection(TaskTableModel::ColImageName,   110);
    hdr->resizeSection(TaskTableModel::ColBackend,      90);
    hdr->resizeSection(TaskTableModel::ColOutputTypes, 140);
    hdr->resizeSection(TaskTableModel::ColStatus,       70);
    hdr->resizeSection(TaskTableModel::ColFrames,       70);
    hdr->resizeSection(TaskTableModel::ColAvgFps,       80);
    hdr->resizeSection(TaskTableModel::ColDescription, 120);
    hdr->resizeSection(TaskTableModel::ColNotes,       120);

    // Header checkbox for select-all
    connect(hdr, &QHeaderView::sectionClicked, this, [this](int logicalIndex) {
        if (logicalIndex == TaskTableModel::ColCheck) {
            bool allChecked = !model_->CheckedIds().isEmpty()
                           && model_->CheckedIds().size() == static_cast<size_t>(model_->rowCount());
            model_->ToggleAll(!allChecked);
            UpdateBatchDeleteButton();
        }
    });

    table_view_->setStyleSheet(QString(
        "QTableView {"
        "  background: %1; border: 1px solid %2;"
        "  gridline-color: %2;"
        "  selection-background-color: %3;"
        "  selection-color: %4;"
        "}"
        "QTableView::item { padding: 6px 8px; }"
        "QHeaderView::section {"
        "  background: %5; color: %6;"
        "  border: none; border-right: 1px solid %2;"
        "  border-bottom: 2px solid %2;"
        "  padding: 6px 8px; font-weight: bold;"
        "}").arg(tm.DeepBg(), tm.Border(), tm.SelectedBg(),
                 tm.TextPrimary(), tm.SurfaceBg(), tm.TextMuted()));

    connect(table_view_, &QTableView::customContextMenuRequested,
            this, &TaskHistoryPage::OnContextMenu);
    connect(table_view_, &QTableView::doubleClicked,
            this, &TaskHistoryPage::OnDoubleClicked);

    // Track checkbox changes
    connect(model_, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& topLeft, const QModelIndex&) {
        if (topLeft.column() == TaskTableModel::ColCheck)
            UpdateBatchDeleteButton();
        if (topLeft.column() == TaskTableModel::ColName) {
            // Persist name edit
            int row = topLeft.row();
            const auto* info = model_->GetRecord(row);
            if (info) {
                db::TaskRecord update;
                update.name = info->task.name;
                task_repo_->Update(info->task.id, update);
            }
        }
    });

    root->addWidget(table_view_, 1);

    // ── Bottom bar ───────────────────────────────────────────

    auto* bottom_bar = new QHBoxLayout();

    count_label_ = new QLabel(this);
    count_label_->setStyleSheet(QString("QLabel { color: %1; padding: 2px 0; }").arg(tm.TextDim()));
    bottom_bar->addWidget(count_label_);

    bottom_bar->addStretch();

    batch_delete_btn_ = new QPushButton(QStringLiteral("删除选中"), this);
    batch_delete_btn_->setEnabled(false);
    batch_delete_btn_->setCursor(Qt::PointingHandCursor);
    batch_delete_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { background: %5; color: %6; }").arg(
            tm.Error(), tm.White(), tm.Border(), tm.ErrorBg(), tm.HoverBg(), tm.TextDim()));
    connect(batch_delete_btn_, &QPushButton::clicked, this, &TaskHistoryPage::OnBatchDelete);
    bottom_bar->addWidget(batch_delete_btn_);

    root->addLayout(bottom_bar);

    UpdateCountLabel();
}

void TaskHistoryPage::LoadTasks() {
    auto tasks = task_repo_->FindAll({}, db::TaskSort{"start_time", false});

    std::vector<TaskDisplayInfo> display_list;
    display_list.reserve(tasks.size());
    for (auto& task : tasks) {
        TaskDisplayInfo info;
        info.task = std::move(task);
        if (info.task.video_id > 0) {
            auto video = video_repo_->FindById(info.task.video_id);
            if (video)
                info.video_name = QString::fromStdString(video->name);
        }
        if (info.task.image_id > 0) {
            auto image = image_repo_->FindById(info.task.image_id);
            if (image)
                info.image_name = QString::fromStdString(image->name);
        }
        display_list.push_back(std::move(info));
    }

    model_->SetRecords(std::move(display_list));
    UpdateCountLabel();
    UpdateBatchDeleteButton();
}

void TaskHistoryPage::UpdateCountLabel() {
    int filtered = proxy_ ? proxy_->rowCount() : 0;
    count_label_->setText(QStringLiteral("共 %1 条任务记录").arg(filtered));
}

void TaskHistoryPage::UpdateBatchDeleteButton() {
    int checked = model_->CheckedIds().size();
    batch_delete_btn_->setEnabled(checked > 0);
    batch_delete_btn_->setText(QStringLiteral("删除选中 (%1)").arg(checked));
}

void TaskHistoryPage::Refresh() {
    LoadTasks();
}

void TaskHistoryPage::ReapplyTheme() {
    auto& tm = ThemeManager::Instance();

    auto comboStyle = [&]() {
        return QString(
            "QComboBox {"
            "  background: %1; border: 1px solid %2;"
            "  border-radius: 4px; padding: 5px 8px; color: %3;"
            "}"
            "QComboBox:focus { border-color: %4; }"
            "QComboBox QAbstractItemView {"
            "  background: %1; color: %3;"
            "  selection-background-color: %5; border: 1px solid %2;"
            "}"
            "QComboBox::drop-down { border: none; width: 18px; }")
            .arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary(), tm.SelectedBg());
    };

    backend_combo_->setStyleSheet(comboStyle());
    status_combo_->setStyleSheet(comboStyle());

    search_box_->setStyleSheet(QString(
        "QLineEdit {"
        "  background: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 5px 8px; color: %3;"
        "}"
        "QLineEdit:focus { border-color: %4; }").arg(tm.SurfaceBg(), tm.Border(), tm.TextPrimary(), tm.Primary()));

    reset_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 12px;"
        "}"
        "QPushButton:hover { background: %3; }").arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));

    refresh_btn_->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1; border: 1px solid %2;"
        "  border-radius: 4px; padding: 6px 16px;"
        "}"
        "QPushButton:hover { background: %3; }").arg(tm.TextPrimary(), tm.Border(), tm.HoverBg()));

    table_view_->setStyleSheet(QString(
        "QTableView {"
        "  background: %1; border: 1px solid %2;"
        "  gridline-color: %2;"
        "  selection-background-color: %3;"
        "  selection-color: %4;"
        "}"
        "QTableView::item { padding: 6px 8px; }"
        "QHeaderView::section {"
        "  background: %5; color: %6;"
        "  border: none; border-right: 1px solid %2;"
        "  border-bottom: 2px solid %2;"
        "  padding: 6px 8px; font-weight: bold;"
        "}").arg(tm.DeepBg(), tm.Border(), tm.SelectedBg(),
                 tm.TextPrimary(), tm.SurfaceBg(), tm.TextMuted()));

    count_label_->setStyleSheet(QString("QLabel { color: %1; padding: 2px 0; }").arg(tm.TextDim()));

    batch_delete_btn_->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 6px 16px; font-weight: bold; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { background: %5; color: %6; }").arg(
            tm.Error(), tm.White(), tm.Border(), tm.ErrorBg(), tm.HoverBg(), tm.TextDim()));
}

// ─── Slots ───────────────────────────────────────────────────

void TaskHistoryPage::OnBackendFilterChanged(int index) {
    proxy_->SetBackendFilter(backend_combo_->itemData(index).toString());
    UpdateCountLabel();
}

void TaskHistoryPage::OnStatusFilterChanged(int index) {
    proxy_->SetStatusFilter(status_combo_->itemData(index).toString());
    UpdateCountLabel();
}

void TaskHistoryPage::OnDateFromChanged(const QDate& date) {
    proxy_->SetDateFrom(date == date_from_->minimumDate() ? QDate() : date);
    UpdateCountLabel();
}

void TaskHistoryPage::OnDateToChanged(const QDate& date) {
    proxy_->SetDateTo(date == date_to_->minimumDate() ? QDate() : date);
    UpdateCountLabel();
}

void TaskHistoryPage::OnSearchTextChanged(const QString& text) {
    proxy_->SetTextFilter(text);
    UpdateCountLabel();
}

void TaskHistoryPage::OnResetFilters() {
    search_box_->clear();
    backend_combo_->setCurrentIndex(0);
    status_combo_->setCurrentIndex(0);

    date_from_->blockSignals(true);
    date_from_->setDate(QDate());
    date_from_->blockSignals(false);
    proxy_->SetDateFrom(QDate());

    date_to_->blockSignals(true);
    date_to_->setDate(QDate());
    date_to_->blockSignals(false);
    proxy_->SetDateTo(QDate());

    UpdateCountLabel();
}

int64_t TaskHistoryPage::SelectedTaskId() const {
    auto indexes = table_view_->selectionModel()->selectedRows();
    if (indexes.isEmpty()) return 0;
    return indexes.first().data(Qt::UserRole).toLongLong();
}

void TaskHistoryPage::OnDoubleClicked(const QModelIndex& index) {
    int64_t id = index.data(Qt::UserRole).toLongLong();
    if (id > 0)
        emit ViewTaskDetailRequested(id);
}

void TaskHistoryPage::OnContextMenu(const QPoint& pos) {
    auto index = table_view_->indexAt(pos);
    if (!index.isValid()) return;

    table_view_->selectRow(index.row());
    int64_t task_id = SelectedTaskId();
    if (task_id == 0) return;

    QMenu menu(this);
    auto& tm = ThemeManager::Instance();
    menu.setStyleSheet(QString(
        "QMenu { background: %1; color: %2; border: 1px solid %3; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; }"
        "QMenu::item:selected { background: %4; }"
        "QMenu::separator { background: %3; height: 1px; margin: 4px 8px; }").arg(
            tm.SurfaceBg(), tm.TextPrimary(), tm.Border(), tm.SelectedBg()));

    auto* view_action = menu.addAction(QStringLiteral("查看任务详情"));
    auto* edit_action = menu.addAction(QStringLiteral("编辑信息"));
    auto* csv_action = menu.addAction(QStringLiteral("查看CSV"));
    auto* open_dir_action = menu.addAction(QStringLiteral("打开文件所在位置"));
    menu.addSeparator();
    auto* delete_action = menu.addAction(QStringLiteral("删除任务"));

    QAction* chosen = menu.exec(table_view_->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == view_action) {
        emit ViewTaskDetailRequested(task_id);
    } else if (chosen == edit_action) {
        emit EditRequested(task_id);
    } else if (chosen == csv_action) {
        emit ViewCsvRequested(task_id);
    } else if (chosen == open_dir_action) {
        auto rec = task_repo_->FindById(task_id);
        if (rec) {
            QString dir;
            if (!rec->output_dir.empty())
                dir = QString::fromStdString(rec->output_dir);
            else if (!rec->csv_path.empty())
                dir = QFileInfo(QString::fromStdString(rec->csv_path)).absolutePath();
            if (!dir.isEmpty())
                QProcess::startDetached(QStringLiteral("explorer"), {QDir::toNativeSeparators(dir)});
        }
    } else if (chosen == delete_action) {
        OnDeleteTask();
    }
}

void TaskHistoryPage::OnDeleteTask() {
    int64_t task_id = SelectedTaskId();
    if (task_id == 0) return;

    auto rec = task_repo_->FindById(task_id);
    if (!rec) return;

    if (rec->status == "running") {
        QMessageBox::warning(this, QStringLiteral("无法删除"),
            QStringLiteral("该任务正在运行中，请先停止处理后再删除。"));
        return;
    }

    auto result = QMessageBox::question(this,
        QStringLiteral("删除任务"),
        QStringLiteral("确定要删除此任务吗？\n\n将同时删除记录和输出文件，此操作不可撤销。"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (result != QMessageBox::Yes) return;

    task_repo_->Delete(task_id, true);
    emit TaskDeleted(task_id);
    Refresh();
}

void TaskHistoryPage::OnBatchDelete() {
    QSet<int64_t> ids = model_->CheckedIds();
    if (ids.isEmpty()) return;

    int running = 0;
    for (int64_t id : ids) {
        auto rec = task_repo_->FindById(id);
        if (rec && rec->status == "running")
            running++;
    }
    if (running > 0) {
        auto result = QMessageBox::question(this,
            QStringLiteral("批量删除"),
            QStringLiteral("选中的 %1 条任务中有 %2 条正在运行中，将被跳过。\n\n确定删除其余 %3 条任务吗？\n将同时删除记录和输出文件，此操作不可撤销。")
                .arg(ids.size()).arg(running).arg(ids.size() - running),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    } else {
        auto result = QMessageBox::question(this,
            QStringLiteral("批量删除"),
            QStringLiteral("确定要删除选中的 %1 条任务记录吗？\n\n将同时删除记录和输出文件，此操作不可撤销。").arg(ids.size()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    }

    int deleted = 0;
    int skipped = 0;
    for (int64_t id : ids) {
        auto rec = task_repo_->FindById(id);
        if (rec && rec->status == "running") {
            skipped++;
            continue;
        }
        if (task_repo_->Delete(id, true)) {
            emit TaskDeleted(id);
            deleted++;
        }
    }
    Refresh();
    QMessageBox::information(this, QStringLiteral("删除完成"),
        QStringLiteral("成功删除 %1/%2 条任务%3。")
            .arg(deleted).arg(ids.size())
            .arg(skipped > 0 ? QStringLiteral("（跳过 %1 条运行中）").arg(skipped) : QString()));
}

}  // namespace ui
}  // namespace omniface
