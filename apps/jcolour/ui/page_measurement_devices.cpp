#include "page_measurement_devices.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QGroupBox>
#include <QComboBox>
#include <QSettings>

#include "../translations.h"
#include "i1sdk.h"   // engine: Backend / probe_backends / set_preferred_backend

namespace jcolour {

namespace {

constexpr const char* kSettingsKey = "measurement/i1Backend";

i1sdk::Backend backendFromInt(int v) {
    switch (v) {
        case 1:  return i1sdk::Backend::XRiteSdk;
        case 2:  return i1sdk::Backend::Argyll;
        default: return i1sdk::Backend::Auto;
    }
}

}  // namespace

void PageMeasurementDevices::applySavedBackend() {
    const int v = QSettings().value(kSettingsKey, 0).toInt();
    i1sdk::set_preferred_backend(backendFromInt(v));
}

PageMeasurementDevices::PageMeasurementDevices(QWidget* parent) : QWidget(parent) {
    setObjectName("pageDevices");
    setStyleSheet("#pageDevices { background:#f8f9fa; }");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("量测设备"));
    QFont tf = title->font(); tf.setPointSize(18); tf.setBold(true);
    title->setFont(tf);
    root->addWidget(title);

    // ---- i1 接入方式（两种方法，二选一或自动） ----
    auto* backendGroup = new QGroupBox(QStringLiteral("i1 接入方式"));
    auto* bl = new QVBoxLayout(backendGroup);

    auto* row = new QHBoxLayout;
    row->addWidget(new QLabel(QStringLiteral("接入方法")));
    backendCombo_ = new QComboBox;
    backendCombo_->addItem(QStringLiteral("自动（优先 SDK，其次 Argyll）"));   // 0
    backendCombo_->addItem(QStringLiteral("X-Rite SDK（i1Pro3.framework 直连）")); // 1
    backendCombo_->addItem(QStringLiteral("Argyll（spotread 子进程）"));        // 2
    backendCombo_->setCurrentIndex(QSettings().value(kSettingsKey, 0).toInt());
    row->addWidget(backendCombo_, 1);
    auto* refresh = new QPushButton(QStringLiteral("重新侦测"));
    row->addWidget(refresh);
    bl->addLayout(row);

    sdkStatus_ = new QLabel;
    sdkStatus_->setWordWrap(true);
    sdkStatus_->setTextFormat(Qt::RichText);
    bl->addWidget(sdkStatus_);

    argyllStatus_ = new QLabel;
    argyllStatus_->setWordWrap(true);
    argyllStatus_->setTextFormat(Qt::RichText);
    bl->addWidget(argyllStatus_);

    auto* note = new QLabel(QStringLiteral(
        "ⓘ i1Pro3 在 Argyll 下以 spotread <b>-N</b> 跳过强制白校准（避免 "
        "Wrong Sensor Position）；切换方法即刻生效，量测时按所选方法接入。"));
    note->setWordWrap(true);
    note->setStyleSheet("color:#777;font-size:11px;");
    bl->addWidget(note);

    root->addWidget(backendGroup);

    connect(backendCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        QSettings().setValue(kSettingsKey, idx);
        i1sdk::set_preferred_backend(backendFromInt(idx));
        refreshStatus();
    });
    connect(refresh, &QPushButton::clicked, this, [this] { refreshStatus(); });

    // ---- 支持的设备清单（原样保留） ----
    auto* group = new QGroupBox(TR("Supported Devices"));
    auto* gl = new QVBoxLayout(group);

    deviceList_ = new QListWidget;
    deviceList_->setSelectionMode(QAbstractItemView::SingleSelection);
    const QStringList devices = {
        "X-Rite i1Pro 2", "X-Rite i1Pro 3", "X-Rite i1iO 2",
        "X-Rite i1iO 3",  "X-Rite ISIS",    "X-Rite ISIS 2",
        "Barbieri Spectro",
    };
    for (const auto& d : devices) {
        auto* item = new QListWidgetItem(d);
        item->setData(Qt::UserRole, d);
        deviceList_->addItem(item);
    }
    gl->addWidget(deviceList_);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    btnRow->addWidget(new QPushButton(TR("Test Connection")));
    btnRow->addWidget(new QPushButton(TR("Set as Active")));
    gl->addLayout(btnRow);

    root->addWidget(group);
    root->addStretch(1);

    refreshStatus();
}

void PageMeasurementDevices::refreshStatus() {
    const auto s = i1sdk::probe_backends();

    if (s.sdk_available) {
        sdkStatus_->setText(QStringLiteral(
            "<span style='color:#1d6b39'>●</span> <b>X-Rite SDK</b> 可用 · 版本 %1 · 设备 %2 台"
            "<br><span style='color:#999;font-size:11px'>%3</span>")
            .arg(QString::fromStdString(s.sdk_version))
            .arg(s.sdk_devices)
            .arg(QString::fromStdString(s.sdk_path)));
    } else {
        sdkStatus_->setText(QStringLiteral(
            "<span style='color:#9a2418'>●</span> <b>X-Rite SDK</b> 不可用 — 未找到 "
            "i1Pro3.framework（可设 JCOLOUR_I1PRO3_FRAMEWORK 指定路径）"));
    }

    if (s.argyll_available) {
        argyllStatus_->setText(QStringLiteral(
            "<span style='color:#1d6b39'>●</span> <b>Argyll</b> 可用 · spotread %1"
            "<br><span style='color:#999;font-size:11px'>%2</span>")
            .arg(QString::fromStdString(s.argyll_version))
            .arg(QString::fromStdString(s.argyll_spotread_path)));
    } else {
        argyllStatus_->setText(QStringLiteral(
            "<span style='color:#9a2418'>●</span> <b>Argyll</b> 不可用 — 未找到 spotread"
            "（brew install argyll-cms）"));
    }
}

}  // namespace jcolour
