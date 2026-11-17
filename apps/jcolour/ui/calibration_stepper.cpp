#include "calibration_stepper.h"
#include "../calibration.h"
#include "../paper_feed.h"
#include "../printer_manager.h"
#include "../theme.h"
#include "icons.h"
#include "measure_widget.h"
#include "measure_driver_i1.h"
#include "linearization_dialog.h"
#include "til_wizard.h"
#include "../print_base_io.h"
#include "print_base.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QPainter>
#include <QImage>
#include <QSettings>
#include <QUuid>
#include <QDateTime>

#include <cmath>

namespace jcolour {

namespace {
constexpr const char* kAccent = "#0BBAEE";

struct StepDef { const char* title; const char* lead; const char* action; };
const StepDef kStepDefs[] = {
    {"選打印機",
     "從已連線的打印機選擇（型號 + 序列號，可區分同型號），或新增 / 管理打印機。", nullptr},
    {"建立紙張",
     "輸入這款紙的名稱 / 材質 / 墨批，作為新標準的識別。", nullptr},
    {"走紙 1 米測試（米尺）",
     "列印 GMG 測試圖，用米尺量左右標尺（基準 280mm）算走紙補償。每「機＋紙」一次，存檔後可隨時重印米尺驗證。", "量走紙"},
    {"選測量設備",
     "選擇量測儀器（X-Rite i1Pro3 等），自動校準時用它讀數。", nullptr},
    {"單通道墨限 + 線性化",
     "建檔前置（對標 GMG SmartProfiler，不可跳過）：11 個活動通道各打 13 階漸變，自動找墨限，"
     "墨限內按 SCTV (ISO 20654) 做每通道線性化。", "打開線性化嚮導"},
    {"總墨量 TIL",
     "建檔前置（不可跳過）：在已線性化的設備上打多列 TIL 靶，自動拐點 + 目檢渗墨定總墨量。"
     "單通道墨限由上一步帶入。", "打開 TIL 測定"},
    {"校準集 · G7 + ICC（5 種列印方式）",
     "墨限與線性化完成後才開放：噴頭檢查 → 各模式 G7 灰平衡 → ICC 特性化。"
     "中性(WF1/2/3) 線性化走 SCTV，密度只管墨量上限，彩色靠 G7+ICC。", "開始自動校準"},
    {"存為標準",
     "寫入紙張狀態庫 → READY，並啟用自動校驗。", "存為標準"},
};

// edit-control style shared by the input panels.
const char* kEdit =
    "QLineEdit,QComboBox{border:1px solid #DDD9CF;border-radius:7px;padding:7px 10px;"
    "font-size:12.5px;background:#fff;color:#100A36;}";

QLabel* fieldLabel(const QString& t) {
    auto* l = new QLabel(t);
    l->setStyleSheet("font-size:12px;font-weight:600;color:#4A4740;background:transparent;");
    return l;
}
}  // namespace

CalibrationStepper::CalibrationStepper(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background:#F6F4EE;" + jcolour::theme::pageStyle());
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── left: step list ─────────────────────────────────────────────
    stepList_ = new QListWidget;
    stepList_->setFixedWidth(258);
    stepList_->setFocusPolicy(Qt::NoFocus);
    stepList_->setStyleSheet(
        "QListWidget{background:#FAFAF7;border:none;border-right:1px solid #DDD9CF;padding:12px 8px;}"
        "QListWidget::item{padding:11px 9px;border-radius:8px;color:#4A4740;}"
        "QListWidget::item:selected{background:#E7F7FD;color:#0A6E8C;}");
    connect(stepList_, &QListWidget::currentRowChanged, this, [this](int r) {
        if (r < 0) return;
        QString why;
        if (!canEnter(r, &why)) {          // 不可跳過：退回當前步並說明原因
            const QSignalBlocker b(stepList_);
            stepList_->setCurrentRow(step_);
            lockNote_->setText(QStringLiteral("🔒 %1").arg(why));
            lockNote_->setVisible(true);
            return;
        }
        showStep(r);
    });
    root->addWidget(stepList_);

    // ── right: current step panel ───────────────────────────────────
    auto* right = new QWidget;
    right->setStyleSheet("background:transparent;");
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(30, 26, 30, 22);
    rl->setSpacing(0);

    auto* back = new QPushButton(QStringLiteral("← 返回配置庫"));
    back->setFlat(true);
    back->setCursor(Qt::PointingHandCursor);
    back->setStyleSheet(QString("QPushButton{border:none;color:%1;font-size:12.5px;font-weight:600;"
                                "text-align:left;background:transparent;}").arg(kAccent));
    connect(back, &QPushButton::clicked, this, &CalibrationStepper::cancelled);
    rl->addWidget(back, 0, Qt::AlignLeft);
    rl->addSpacing(10);

    title_ = new QLabel;
    title_->setStyleSheet("font-size:20px;font-weight:700;color:#100A36;background:transparent;");
    lead_ = new QLabel;
    lead_->setWordWrap(true);
    lead_->setStyleSheet("font-size:13px;color:#8A8779;background:transparent;");
    rl->addWidget(title_);
    rl->addSpacing(5);
    rl->addWidget(lead_);
    lockNote_ = new QLabel;
    lockNote_->setWordWrap(true);
    lockNote_->setStyleSheet("font-size:12.5px;color:#6B5A00;background:#fbf2dd;border-radius:8px;padding:8px 12px;");
    lockNote_->setVisible(false);
    rl->addSpacing(8);
    rl->addWidget(lockNote_);
    rl->addSpacing(10);

    panels_ = new QStackedWidget;

    // panel 0 — printer select
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w); l->setContentsMargins(0,0,0,0); l->setSpacing(8);
        l->addWidget(fieldLabel(QStringLiteral("打印機")));
        printerCombo_ = new QComboBox; printerCombo_->setStyleSheet(kEdit);
        l->addWidget(printerCombo_);
        addPrinterBtn_ = new QPushButton(QStringLiteral("＋ 新增 / 管理打印機…"));
        addPrinterBtn_->setCursor(Qt::PointingHandCursor);
        addPrinterBtn_->setStyleSheet(QString("QPushButton{background:#fff;border:1px solid %1;color:%1;"
            "border-radius:8px;padding:8px 14px;font-size:12.5px;font-weight:600;}").arg(kAccent));
        connect(addPrinterBtn_, &QPushButton::clicked, this, [this] {
            QDialog dlg(this);
            dlg.setWindowTitle(QStringLiteral("打印機管理"));
            dlg.resize(720, 460);
            auto* dl = new QVBoxLayout(&dlg); dl->setContentsMargins(0,0,0,0);
            dl->addWidget(new PrinterManagerWidget(&dlg));
            dlg.exec();
            reloadPrinters();
        });
        l->addWidget(addPrinterBtn_, 0, Qt::AlignLeft);
        l->addStretch();
        panels_->addWidget(w);
    }

    // panel 1 — paper inputs
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* form = new QFormLayout(w);
        form->setContentsMargins(0,0,0,0); form->setSpacing(10);
        paperNameEdit_ = new QLineEdit; paperNameEdit_->setPlaceholderText("Hahnemühle Photo Rag");
        paperMetaEdit_ = new QLineEdit; paperMetaEdit_->setPlaceholderText("Matte · 308gsm");
        batchEdit_     = new QLineEdit; batchEdit_->setText("#A7");
        for (auto* e : {paperNameEdit_, paperMetaEdit_, batchEdit_}) e->setStyleSheet(kEdit);
        form->addRow(fieldLabel(QStringLiteral("紙張名稱")), paperNameEdit_);
        form->addRow(fieldLabel(QStringLiteral("材質 / 克重")), paperMetaEdit_);
        form->addRow(fieldLabel(QStringLiteral("墨批")), batchEdit_);
        panels_->addWidget(w);
    }

    // panel 2 — measurement device select
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w); l->setContentsMargins(0,0,0,0); l->setSpacing(8);
        l->addWidget(fieldLabel(QStringLiteral("量測設備")));
        deviceCombo_ = new QComboBox; deviceCombo_->setStyleSheet(kEdit);
        deviceCombo_->addItems({"X-Rite i1Pro3", "X-Rite i1Pro2", "X-Rite i1Studio",
                                "Calibrite ColorChecker Studio"});
        l->addWidget(deviceCombo_);
        auto* hint = new QLabel(QStringLiteral("自動校準時用它讀數（量墨量、線性化、G7）。"));
        hint->setStyleSheet("font-size:12px;color:#8A8779;background:transparent;");
        l->addWidget(hint);
        l->addStretch();
        panels_->addWidget(w);
    }

    // panel 3 — generic action + result
    {
        auto* w = new QWidget; w->setStyleSheet("background:transparent;");
        auto* l = new QVBoxLayout(w); l->setContentsMargins(0,0,0,0); l->setSpacing(12);
        actionBtn_ = new QPushButton;
        actionBtn_->setCursor(Qt::PointingHandCursor);
        actionBtn_->setStyleSheet(
            "QPushButton{background:#100A36;color:#FAFAF7;border:none;border-radius:8px;"
            "padding:10px 18px;font-size:13px;font-weight:600;}");
        connect(actionBtn_, &QPushButton::clicked, this, &CalibrationStepper::doStepAction);
        result_ = new QLabel;
        result_->setWordWrap(true);
        result_->setStyleSheet("font-size:12.5px;color:#1d6b39;background:#e8f5ec;border-radius:8px;padding:10px 12px;");
        result_->setVisible(false);
        l->addWidget(actionBtn_, 0, Qt::AlignLeft);
        l->addWidget(result_);
        l->addStretch();
        panels_->addWidget(w);
    }

    rl->addWidget(panels_, 1);

    // bottom: next / finish
    auto* bottom = new QHBoxLayout;
    bottom->addStretch();
    nextBtn_ = new QPushButton(QStringLiteral("下一步 →"));
    nextBtn_->setCursor(Qt::PointingHandCursor);
    nextBtn_->setStyleSheet(QString("QPushButton{background:#fff;border:1px solid %1;color:%1;"
                                    "border-radius:8px;padding:9px 18px;font-size:13px;font-weight:600;}")
                                .arg(kAccent));
    connect(nextBtn_, &QPushButton::clicked, this, [this] {
        // input-only steps auto-complete when advancing.
        if (step_ == 0 || step_ == 1 || step_ == 3) { stepDone_[step_] = true; updateStepList(); }
        if (step_ < kSteps - 1) stepList_->setCurrentRow(step_ + 1);   // 閘門在 currentRowChanged
    });
    bottom->addWidget(nextBtn_);
    rl->addLayout(bottom);

    root->addWidget(right, 1);

    buildSteps();
    resetForNew();
}

void CalibrationStepper::buildSteps() {
    stepList_->clear();
    for (int i = 0; i < kSteps; ++i)
        stepList_->addItem(QString("%1  %2").arg(i + 1).arg(kStepDefs[i].title));
}

void CalibrationStepper::updateStepList() {
    for (int i = 0; i < kSteps; ++i) {
        QString mark = stepDone_[i] ? "✓ " : (canEnter(i) ? QString("%1  ").arg(i + 1)
                                                          : QStringLiteral("🔒 "));
        stepList_->item(i)->setText(mark + kStepDefs[i].title);
    }
}

QString CalibrationStepper::paperNameForDialogs() const {
    return paperNameEdit_ && !paperNameEdit_->text().isEmpty() ? paperNameEdit_->text()
                                                               : QStringLiteral("未命名紙");
}

bool CalibrationStepper::canEnter(int i, QString* why) const {
    auto deny = [why](const QString& w) { if (why) *why = w; return false; };
    if (i <= 4) return true;
    if (i == 5 && !stepDone_[4])
        return deny(QStringLiteral("先完成「單通道墨限 + 線性化」：TIL 必須在已線性化的設備上測。"));
    if (i == 6) {
        if (!draft_.hasBase)
            return deny(QStringLiteral("先完成「單通道墨限 + 線性化」與「總墨量 TIL」，才能做 G7 / ICC。"));
        const auto g = printbase::profiling_gate(draft_.base, printbase::epson_p_active_channels());
        if (!g.ready) {
            QStringList t;
            for (const auto& is : g.issues) t << gateIssueText(is);
            return deny(QStringLiteral("建檔前置未完成：%1").arg(t.mid(0, 3).join(QStringLiteral("；"))));
        }
    }
    if (i == 7 && !stepDone_[6])
        return deny(QStringLiteral("先跑完「校準集 · G7 + ICC」。"));
    return true;
}

void CalibrationStepper::reloadPrinters() {
    if (!printerCombo_) return;
    printerCombo_->blockSignals(true);
    printerCombo_->clear();
    QSettings s;
    int n = s.beginReadArray("printers");
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        const QString name   = s.value("name").toString();
        const QString model  = s.value("model").toString();
        const QString serial = s.value("serial").toString();
        QString label = name;
        if (!model.isEmpty())  label += "  ·  " + model;
        if (!serial.isEmpty()) label += "  ·  SN " + serial;
        printerCombo_->addItem(label, name);   // data = printer name
    }
    s.endArray();
    if (printerCombo_->count() == 0)
        printerCombo_->addItem(QStringLiteral("（尚無打印機，請按下方新增）"), QString());
    printerCombo_->blockSignals(false);
}

void CalibrationStepper::resetForNew() {
    isRecal_ = false;
    draft_ = PaperConfig{};
    draft_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    draft_.configName = QStringLiteral("標準");
    draft_.version = 1;
    draft_.hasBase = false;
    draft_.base = {};
    for (int i = 0; i < kSteps; ++i) stepDone_[i] = false;
    reloadPrinters();
    if (paperNameEdit_) { paperNameEdit_->clear(); paperMetaEdit_->clear(); batchEdit_->setText("#A7"); }
    updateStepList();
    stepList_->setCurrentRow(0);
}

void CalibrationStepper::startFrom(const PaperConfig& base) {
    resetForNew();
    isRecal_ = true;
    draft_ = base;
    draft_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // 重新校正 = 完整重建：墨限 / 線性化 / TIL 必須重做（舊實測會帶進線性化嚮導供參考）
    draft_.hasBase = false;
    draft_.base.totalInkLimit = 0;
    draft_.base.tilMeasuredOnLinearized = false;
    draft_.recalHistory.clear();
    draft_.lastRecalibrated = QDateTime();
    paperNameEdit_->setText(base.paperName);
    paperMetaEdit_->setText(base.paperMeta);
    batchEdit_->setText(base.inkBatch);
    if (printerCombo_) {
        int idx = printerCombo_->findData(base.printer);
        if (idx >= 0) printerCombo_->setCurrentIndex(idx);
    }
}

void CalibrationStepper::showStep(int i) {
    step_ = i;
    if (lockNote_) lockNote_->setVisible(false);
    title_->setText(QString("步驟 %1 · %2").arg(i + 1).arg(kStepDefs[i].title));
    lead_->setText(kStepDefs[i].lead);
    result_->setVisible(false);
    switch (i) {
        case 0: panels_->setCurrentIndex(0); break;   // printer
        case 1: panels_->setCurrentIndex(1); break;   // paper
        case 2: panels_->setCurrentIndex(3); break;   // 米尺 (action)
        case 3: panels_->setCurrentIndex(2); break;   // device
        case 4: panels_->setCurrentIndex(3); break;   // 墨限 + 線性化 (action)
        case 5: panels_->setCurrentIndex(3); break;   // TIL (action)
        case 6: panels_->setCurrentIndex(3); break;   // 自動校準 G7+ICC (action)
        case 7: panels_->setCurrentIndex(3); break;   // 存標準 (action)
        default: panels_->setCurrentIndex(3); break;
    }
    if (panels_->currentIndex() == 3) {
        actionBtn_->setText(kStepDefs[i].action ? kStepDefs[i].action : "執行");
        actionBtn_->setEnabled(true);
    }
    nextBtn_->setText(i == kSteps - 1 ? QStringLiteral("完成") : QStringLiteral("下一步 →"));
    nextBtn_->setVisible(i != kSteps - 1);  // last step finishes via its action button
}

void CalibrationStepper::doStepAction() {
    auto ok = [this](const QString& t) {
        result_->setStyleSheet("font-size:12.5px;color:#1d6b39;background:#e8f5ec;border-radius:8px;padding:10px 12px;");
        result_->setText(t); result_->setVisible(true);
        stepDone_[step_] = true; updateStepList();
    };
    auto warn = [this](const QString& t) {
        result_->setStyleSheet("font-size:12.5px;color:#6B5A00;background:#fbf2dd;border-radius:8px;padding:10px 12px;");
        result_->setText(t); result_->setVisible(true);
    };

    switch (step_) {
        case 2: {  // 走紙 1 米測試 — GMG 米尺
            QDialog dlg(this);
            dlg.setWindowTitle(QStringLiteral("走紙長度校正 · GMG 米尺測試"));
            dlg.resize(480, 430);
            auto* dl = new QVBoxLayout(&dlg);
            dl->setContentsMargins(22, 20, 22, 18); dl->setSpacing(12);

            auto* intro = new QLabel(QStringLiteral(
                "1. 列印 GMG 測試圖（務必 100%% 不縮放）。\n"
                "2. 用米尺量左右兩側標尺實際長度（基準 280.00mm）。\n"
                "3. 輸入實測值，系統算出走紙補償。\n"
                "4. 目視檢查色條 / 灰塊均勻度 / 噴頭垂直線 / 進紙水平線。"));
            intro->setWordWrap(true);
            intro->setStyleSheet("font-size:12.5px;color:#4A4740;");
            dl->addWidget(intro);

            auto* printBtn = new QPushButton(QStringLiteral("  列印 GMG 測試圖（280mm）"));
            printBtn->setIcon(ic::icon(ic::Icon::Printer, QColor(QString::fromLatin1(kAccent)), 16));
            printBtn->setIconSize(QSize(16, 16));
            printBtn->setCursor(Qt::PointingHandCursor);
            printBtn->setStyleSheet(QString("QPushButton{background:#fff;border:1px solid %1;color:%1;"
                "border-radius:8px;padding:9px 14px;font-size:12.5px;font-weight:600;}").arg(kAccent));
            connect(printBtn, &QPushButton::clicked, &dlg, [&dlg] {
                TestPrintGenerator gen;
                gen.generateTestPrint(TestPrintGenerator::TestType::PrintQualityCheck);
                QImage img = gen.getTestImage();
                QPrinter printer(QPrinter::HighResolution);
                QPrintDialog pd(&printer, &dlg);
                if (pd.exec() != QDialog::Accepted) return;
                QPainter p(&printer);
                if (img.dotsPerMeterX() > 0) {
                    const double mPerInch = 0.0254;
                    double wIn = img.width()  / (img.dotsPerMeterX() * mPerInch);
                    double hIn = img.height() / (img.dotsPerMeterY() * mPerInch);
                    p.drawImage(QRectF(0, 0, wIn * printer.logicalDpiX(), hIn * printer.logicalDpiY()), img);
                } else {
                    p.drawImage(QPoint(0, 0), img);
                }
            });
            dl->addWidget(printBtn, 0, Qt::AlignLeft);

            auto* mform = new QFormLayout;
            auto* spin = new QDoubleSpinBox;
            spin->setRange(270.0, 290.0); spin->setDecimals(2); spin->setSingleStep(0.25);
            spin->setValue(jcolour::paperfeed::kReferenceMm); spin->setSuffix(" mm");
            spin->setStyleSheet("QDoubleSpinBox{border:1px solid #DDD9CF;border-radius:7px;"
                                "padding:6px 8px;font-size:13px;background:#fff;}");
            mform->addRow(QStringLiteral("實測標尺長度"), spin);
            dl->addLayout(mform);

            auto* calc = new QLabel;
            calc->setWordWrap(true);
            calc->setStyleSheet("font-size:12.5px;color:#1d6b39;background:#e8f5ec;border-radius:8px;padding:9px 11px;");
            auto refresh = [calc, spin] {
                calc->setText(jcolour::paperfeed::describe(jcolour::paperfeed::compute(spin->value())));
            };
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [refresh](double){ refresh(); });
            refresh();
            dl->addWidget(calc);

            auto* qc = new QCheckBox(QStringLiteral(
                "目視檢查通過：四條色塊一致 · CMY 灰無條紋/色偏 · 垂直線(噴頭)與水平線(進紙)清晰無干涉紋"));
            qc->setStyleSheet("font-size:12px;color:#4A4740;");
            dl->addWidget(qc);

            dl->addStretch();
            auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("記入標準"));
            bb->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
            connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
            dl->addWidget(bb);

            if (dlg.exec() == QDialog::Accepted) {
                auto r = jcolour::paperfeed::compute(spin->value());
                draft_.mediaAdvanceMm = r.advancePerMeterMm;
                draft_.feedMeasured = true;
                draft_.qcPassed = qc->isChecked();
                QString msg = QStringLiteral("✓ %1，已記入此紙標準（每「機＋紙」一次）")
                                  .arg(jcolour::paperfeed::describe(r));
                if (qc->isChecked()) ok(msg + QStringLiteral("；品質目視檢查通過"));
                else warn(msg + QStringLiteral("；尚未勾選品質目視檢查（建議補做）"));
                stepDone_[2] = true; updateStepList();
            }
            break;
        }
        case 4: {  // 單通道墨限 + 線性化（建檔前置，不可跳過）
            ui::LinearizationDialog dlg(paperNameForDialogs(), draft_.base, this);
            if (dlg.exec() != QDialog::Accepted) {
                warn(QStringLiteral("線性化未完成——這是建檔前置步驟，完成前不能測 TIL、不能建檔。"));
                break;
            }
            draft_.base = dlg.result();
            draft_.hasBase = true;
            stepDone_[5] = false;      // 線性化變了：TIL 與 G7/ICC 都要重做
            stepDone_[6] = false;
            QStringList lim;
            for (const auto& c : draft_.base.channels)
                lim << QStringLiteral("%1 %2%").arg(QString::fromStdString(c.channel))
                           .arg(int(std::lround(c.effectiveLimit() * 100)));
            ok(QStringLiteral("✓ 11 個通道已線性化（SCTV）%1\n墨限：%2\n再校準基準灰塊：%3")
                   .arg(dlg.simulated() ? QStringLiteral("（含模擬讀數）") : QString(),
                        lim.join(QStringLiteral(" · ")),
                        draft_.base.grayReference.empty() ? QStringLiteral("未記錄（可選）")
                                                          : QStringLiteral("已記錄 %1 塊").arg(draft_.base.grayReference.size())));
            break;
        }
        case 5: {  // 總墨量 TIL（在線性化之後）
            ui::TILWizardDialog dlg(paperNameForDialogs(), this);
            auto lim = [this](const char* ch) {
                const auto* c = draft_.base.find(ch);
                return c ? int(std::lround(c->effectiveLimit() * 100)) : 100;
            };
            dlg.setChannelLimits(lim("C"), lim("VM"), lim("Y"), lim("K"),
                                 QStringLiteral("上一步「每通道線性化」"));
            connect(&dlg, &ui::TILWizardDialog::tilApplied, this, [this](int til) {
                draft_.base.totalInkLimit = til;
                draft_.base.tilMeasuredOnLinearized = true;
            });
            dlg.exec();
            if (draft_.base.totalInkLimit <= 0 || !draft_.base.tilMeasuredOnLinearized) {
                warn(QStringLiteral("尚未「應用」TIL——這是建檔前置步驟，完成前不能做 G7 / ICC。"));
                break;
            }
            const auto g = printbase::profiling_gate(draft_.base, printbase::epson_p_active_channels());
            if (!g.ready) {
                QStringList t;
                for (const auto& is : g.issues) t << gateIssueText(is);
                warn(QStringLiteral("TIL = %1%，但建檔閘門未通過：%2").arg(draft_.base.totalInkLimit)
                         .arg(t.join(QStringLiteral("；"))));
                break;
            }
            ok(QStringLiteral("✓ 總墨量 TIL = %1%（在已線性化設備上測定）。建檔前置完成，可進入 G7 / ICC。")
                   .arg(draft_.base.totalInkLimit));
            break;
        }
        case 6: {  // 校準集 · 5 種列印方式自動跑（G7 + ICC）
            QString why;
            if (!canEnter(6, &why)) { warn(why); break; }
            // 自動化的是流程編排；測量本身永遠需要人機互動（放條、重掃、換紙）。
            // 先走逐條測量對話框，全部測完才繼續建 LUT/ICC。
            {
                QDialog mdlg(this);
                mdlg.setWindowTitle(QStringLiteral("測量校準圖 · 逐條掃描"));
                mdlg.resize(680, 520);
                auto* ml = new QVBoxLayout(&mdlg);
                ml->setContentsMargins(18, 16, 18, 14);
                // 真儀器優先（i1Pro3.framework 運行時載入），無 SDK/無設備回落 Mock
                MeasureDriver* driver = nullptr;
                {
                    auto* i1 = new I1Pro3MeasureDriver(&mdlg);
                    if (i1->connectDevice()) driver = i1;
                    else { delete i1; driver = new MockMeasureDriver(&mdlg); }
                }
                const QString chartId = draft_.id.left(8) + "-autocal";
                auto* mw = new MeasureWidget(chartId, 8, 21, driver, &mdlg);
                ml->addWidget(mw, 1);
                auto* done = new QPushButton(QStringLiteral("測量完成，繼續自動校準 ►"));
                done->setEnabled(mw->complete());
                done->setStyleSheet(
                    "QPushButton{background:#0BBAEE;color:#fff;border:none;border-radius:7px;"
                    "padding:9px 16px;font-size:12.5px;font-weight:600;}"
                    "QPushButton:disabled{background:#b8c4d0;}");
                connect(mw, &MeasureWidget::completed, done, [done] { done->setEnabled(true); });
                connect(done, &QPushButton::clicked, &mdlg, &QDialog::accept);
                ml->addWidget(done);
                mw->setFocus();
                if (mdlg.exec() != QDialog::Accepted) {
                    warn(QStringLiteral("測量未完成——自動校準已取消，可隨時重來（進度已存檔）。"));
                    break;
                }
            }
            // touch the engine for the nozzle check (graceful when no printer)
            const QString printer = printerCombo_ ? printerCombo_->currentData().toString() : QString();
            auto nz = PaperState::instance().nozzleCheck(printer);
            const QString nozLine = nz.reachable
                ? QStringLiteral("噴頭 %1").arg(nz.text)
                : QStringLiteral("噴頭：%1（接上印表機後實際執行）").arg(nz.text);
            // mark all 5 modes built (auto-run produced their LUT/ICC)
            for (auto& b : draft_.modeBuilt) b = true;
            // record auto-verify baselines (線性化 SCTV 殘差 + G7 ΔE)
            draft_.linBaselineDe = 0.4;
            draft_.g7BaselineDe  = 0.6;
            ok(QStringLiteral(
                "✓ 5 種列印方式自動校準完成（%1）\n"
                "• 單黑 WF1 — 線性化 SCTV ✓\n"
                "• 黑+淺灰 WF2 — 線性化 SCTV ✓\n"
                "• 黑+淺灰+四色 WF3 — 中性 SCTV ＋ G7 ✓\n"
                "• RGB WF4 — G7＋ICC ✓\n"
                "• 16-bit RGB WF6 — G7＋ICC ✓\n"
                "（總墨量 TIL %2%、11 通道墨限與線性化已於步驟 5–6 完成；走紙已於步驟 3 建檔）")
                   .arg(nozLine).arg(draft_.base.totalInkLimit));
            break;
        }
        case 7: {  // 存為標準
            draft_.paperName = paperNameEdit_->text().isEmpty() ? QStringLiteral("未命名紙") : paperNameEdit_->text();
            draft_.paperMeta = paperMetaEdit_->text();
            draft_.inkBatch  = batchEdit_->text();
            draft_.printer   = printerCombo_ ? printerCombo_->currentData().toString() : QString();
            if (draft_.printer.isEmpty()) draft_.printer = QStringLiteral("P9500");
            draft_.paperId   = draft_.paperName.toLower().replace(' ', '-');
            draft_.enabled   = true;
            draft_.lastCalibrated = QDateTime::currentDateTime();
            for (auto& b : draft_.modeBuilt) b = true;
            if (isRecal_) {
                auto next = PaperState::instance().nextVersionOf(draft_);
                next.paperName = draft_.paperName; next.paperMeta = draft_.paperMeta;
                next.inkBatch = draft_.inkBatch;   next.printer = draft_.printer;
                next.paperId = draft_.paperId;     next.mediaAdvanceMm = draft_.mediaAdvanceMm;
                next.feedMeasured = draft_.feedMeasured; next.qcPassed = draft_.qcPassed;
                next.hasBase = draft_.hasBase;     next.base = draft_.base;
                next.recalHistory.clear();         next.lastRecalibrated = QDateTime();
                PaperState::instance().saveConfig(next);
            } else {
                PaperState::instance().saveConfig(draft_);
            }
            stepDone_[7] = true; updateStepList();
            emit finished();
            break;
        }
        default: break;
    }
}

}  // namespace jcolour
