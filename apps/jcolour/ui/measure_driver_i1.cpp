#include "measure_driver_i1.h"

#include "../../../engine/src/i1pro3_driver.h"
#include "../../../engine/src/spectral.h"

#include <QMetaObject>
#include <QPointer>
#include <thread>

namespace jcolour {

using i1sdk::I1Pro3Driver;

I1Pro3MeasureDriver::I1Pro3MeasureDriver(QObject* parent) : MeasureDriver(parent) {}

bool I1Pro3MeasureDriver::connectDevice() {
    auto& d = I1Pro3Driver::instance();
    if (!d.available()) return false;              // 框架未安装 → 调用方用 Mock
    if (!d.isOpen() && !d.openFirst()) return false;  // 无设备
    model_ = QString::fromStdString(d.getOption("DeviceType"));
    if (model_.isEmpty()) model_ = QStringLiteral("i1Pro3");
    open_ = true;
    return true;
}

QString I1Pro3MeasureDriver::name() const {
    return open_ ? model_ + QStringLiteral(" · SDK 条带扫描")
                 : QStringLiteral("i1Pro3（未连接）");
}

void I1Pro3MeasureDriver::whiteCal(std::function<void(bool)> done) {
    QPointer<I1Pro3MeasureDriver> self(this);
    std::thread([self, done] {
        auto& d = I1Pro3Driver::instance();
        const bool ok = d.isOpen() && d.calibrate();   // 白瓷砖校准（阻塞）
        if (self)
            QMetaObject::invokeMethod(self, [done, ok] { done(ok); }, Qt::QueuedConnection);
    }).detach();
}

void I1Pro3MeasureDriver::readStrip(int /*stripIndex*/,
                                    const QVector<MeasurePatch>& patches,
                                    std::function<void(QVector<QVector3D>)> ok,
                                    std::function<void(MeasureError)> fail) {
    const int expect = int(patches.size());
    QPointer<I1Pro3MeasureDriver> self(this);
    std::thread([self, expect, ok, fail] {
        auto& d = I1Pro3Driver::instance();
        auto post = [&self](std::function<void()> f) {
            if (self) QMetaObject::invokeMethod(self, std::move(f), Qt::QueuedConnection);
        };
        auto failWith = [&post, fail](const char* code, const QString& msg, const QString& fix) {
            post([fail, code, msg, fix] { fail({ QString::fromLatin1(code), msg, fix }); });
        };

        if (!d.isOpen()) {
            failWith("NEEDS_CAL", QStringLiteral("仪器未连接"),
                     QStringLiteral("重新连接 i1Pro3 后再试"));
            return;
        }
        // 条带（扫描）模式；TriggerMeasurement 阻塞到用户扫完抬起
        d.setOption("MeasurementMode", "ReflectanceScan");
        if (!d.triggerMeasurement()) {
            const QString err = QString::fromStdString(d.lastError());
            failWith("INCOMPLETE", QStringLiteral("扫描未完成：%1").arg(err),
                     QStringLiteral("从条头匀速重扫到条尾"));
            return;
        }
        const int got = d.availableSamples();
        if (got != expect) {
            failWith(got < expect ? "TOO_FAST" : "WRONG_STRIP",
                     QStringLiteral("识别到 %1 块，本条应为 %2 块").arg(got).arg(expect),
                     got < expect ? QStringLiteral("匀速重扫本条（约 3–4 秒扫完）")
                                  : QStringLiteral("确认扫的是高亮行"));
            return;
        }
        QVector<QVector3D> out;
        out.reserve(expect);
        for (int i = 0; i < expect; ++i) {
            std::array<float, 36> spec{};
            if (!d.spectrum(spec, uint32_t(i))) {
                failWith("INCOMPLETE", QStringLiteral("第 %1 块光谱读取失败").arg(i + 1),
                         QStringLiteral("重扫本条"));
                return;
            }
            spectral::Spectrum s{};
            for (int b = 0; b < 36; ++b) s[size_t(b)] = double(spec[size_t(b)]);
            const auto lab = spectral::xyz_to_lab(spectral::spectrum_to_xyz(s));
            out.push_back({ float(lab.L), float(lab.a), float(lab.b) });
        }
        post([ok, out] { ok(out); });
    }).detach();
}

}  // namespace jcolour
