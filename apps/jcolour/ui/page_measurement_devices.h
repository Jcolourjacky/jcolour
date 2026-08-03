#pragma once

#include <QWidget>

class QListWidget;
class QComboBox;
class QLabel;

namespace jcolour {

// 设置 › 量测设备：选择 i1 的接入方式（两条真实路径，二选一或自动）
//   ① X-Rite SDK — i1Pro3.framework 运行时加载（engine/src/i1pro3_driver）
//   ② Argyll     — spotread 子进程（i1Pro3 走 -N 跳过强制白校准）
// 选择持久化到 QSettings("measurement/i1Backend")，并立即写入引擎
// jcolour::i1sdk::set_preferred_backend()。
class PageMeasurementDevices : public QWidget {
    Q_OBJECT
public:
    explicit PageMeasurementDevices(QWidget* parent = nullptr);

    static void applySavedBackend();   // app 启动时调用：QSettings → engine

private:
    void refreshStatus();              // probe_backends() → 状态卡

    QComboBox*   backendCombo_ = nullptr;
    QLabel*      sdkStatus_    = nullptr;
    QLabel*      argyllStatus_ = nullptr;
    QListWidget* deviceList_   = nullptr;
};

}  // namespace jcolour
