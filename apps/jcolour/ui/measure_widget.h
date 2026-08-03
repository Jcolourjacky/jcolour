#pragma once

// 逐条测量交互组件（与 gcolour Web 版同一套交互模型）：
//   · 当前条高亮，Enter 读取，↑↓/F/B 换条，R 重扫，W 白校准
//   · 测完的色块立刻用实测 Lab 上色；失败条标红给原因，停在原地等重扫
//   · 每条测完即存档（QSettings），中断可恢复
// 驱动抽象 MeasureDriver 之后由 engine/i1sdk (WF4MeasurementBackend) 实现；
// MockMeasureDriver 让无仪器时交互可完整演示。

#include <QWidget>
#include <QVector>
#include <QVector3D>
#include <functional>

class QLabel;
class QPushButton;
class QProgressBar;

namespace jcolour {

struct MeasurePatch {
    float c = 0, m = 0, y = 0, k = 0;   // 0..100 设备值（图表定义）
    bool measured = false;
    double L = 0, a = 0, b = 0;         // 实测 Lab
};

struct MeasureStripState {
    enum class S { Pending, Reading, Ok, Failed };
    S state = S::Pending;
    QString errMsg, errFix;
};

// ── 驱动契约 ────────────────────────────────────────────────────────
// 错误码与 i1Pro3 SDK 层对应（TOO_FAST / WRONG_STRIP / INCOMPLETE / NEEDS_CAL）
struct MeasureError { QString code, msg, fix; };

class MeasureDriver : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QString name() const = 0;
    virtual bool needsWhiteCal() const = 0;
    // 异步：完成后回调实测 Lab（每块一个 (L,a,b)）或错误
    virtual void whiteCal(std::function<void(bool ok)> done) = 0;
    virtual void readStrip(int stripIndex,
                           const QVector<MeasurePatch>& patches,
                           std::function<void(QVector<QVector3D>)> ok,
                           std::function<void(MeasureError)> fail) = 0;
};

class MockMeasureDriver : public MeasureDriver {
    Q_OBJECT
public:
    explicit MockMeasureDriver(QObject* parent = nullptr) : MeasureDriver(parent) {}
    QString name() const override { return QStringLiteral("模拟仪器 (Mock) · 条带扫描"); }
    bool needsWhiteCal() const override { return true; }
    void whiteCal(std::function<void(bool)> done) override;
    void readStrip(int stripIndex, const QVector<MeasurePatch>& patches,
                   std::function<void(QVector<QVector3D>)> ok,
                   std::function<void(MeasureError)> fail) override;
private:
    bool calibrated_ = false;
};

class MeasureWidget;

// 图表绘制区（一行一条；点击选条 / 点击已测色块看读数）
class MeasureChartArea : public QWidget {
    Q_OBJECT
public:
    explicit MeasureChartArea(MeasureWidget* owner);
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
private:
    MeasureWidget* w_;
};

// ── 组件 ────────────────────────────────────────────────────────────
class MeasureWidget : public QWidget {
    Q_OBJECT
public:
    // chartId 用于中断存档的键；strips × patchesPerStrip 定义图表
    MeasureWidget(const QString& chartId, int strips, int patchesPerStrip,
                  MeasureDriver* driver, QWidget* parent = nullptr);

    bool complete() const;

signals:
    void completed();          // 全部条测完

protected:
    void keyPressEvent(QKeyEvent*) override;

private:
    friend class MeasureChartArea;
    void readCurrent();
    void rescan(int strip);
    void whiteCal();
    void persist();
    void tryResume();
    void refreshUi();

    QString chartId_;
    int nStrips_, nPatches_;
    MeasureDriver* driver_;
    QVector<QVector<MeasurePatch>> chart_;
    QVector<MeasureStripState> strips_;
    int cur_ = 0;
    int inspectStrip_ = -1, inspectPatch_ = -1;
    bool calibrated_ = false, busy_ = false;

    MeasureChartArea* area_;
    QLabel *devLabel_, *errBanner_, *inspect_, *progressText_;
    QPushButton *readBtn_, *rescanBtn_, *skipBtn_, *calBtn_;
    QProgressBar* progress_;
};

}  // namespace jcolour
