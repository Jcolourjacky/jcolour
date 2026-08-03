#pragma once

// I1Pro3MeasureDriver — MeasureWidget 的真仪器驱动（Spike 3）。
//
// 走 engine/src/i1pro3_driver（运行时 dlopen i1Pro3.framework，无头文件
// 依赖），条带模式：SetOption("MeasurementMode","ReflectanceScan") →
// TriggerMeasurement →（用户拉动仪器扫完一条）→ GetNumberOfAvailableSamples
// 校验色块数 → 逐块 GetSpectrum → 引擎 spectral 转 Lab。
//
// SDK 调用全部阻塞，放 std::thread 跑，QueuedConnection 回主线程。
// 框架不可用 / 无设备时 connect() 返回 false —— 调用方回落 Mock（诚实降级）。

#include "measure_widget.h"

namespace jcolour {

class I1Pro3MeasureDriver : public MeasureDriver {
    Q_OBJECT
public:
    explicit I1Pro3MeasureDriver(QObject* parent = nullptr);

    // 尝试 dlopen + 打开第一台设备；false = 无 SDK/无设备（用 Mock）
    bool connectDevice();

    QString name() const override;
    bool needsWhiteCal() const override { return true; }
    void whiteCal(std::function<void(bool ok)> done) override;
    void readStrip(int stripIndex,
                   const QVector<MeasurePatch>& patches,
                   std::function<void(QVector<QVector3D>)> ok,
                   std::function<void(MeasureError)> fail) override;

private:
    bool open_ = false;
    QString model_;
};

}  // namespace jcolour
