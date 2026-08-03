#include "g7_wizard.h"
#include "g7_validation.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QFileDialog>
#include <QFontDatabase>
#include <QColor>
#include <QPdfWriter>
#include <QTextDocument>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <random>
#include <algorithm>
#include <cmath>

namespace jcolour {
namespace ui {

namespace cal = jcolour::calibration;

struct G7Wizard::Held {
    cal::G7CalibrationResult result;
};

namespace {
constexpr const char* kAccent = "#2e7dd7";
constexpr double kTolerance = 1.5;   // gray ΔE target for B&W G7+

QString badgeStyle(const QString& bg, const QString& fg) {
    return QString("background:%1;color:%2;border-radius:8px;padding:8px 16px;"
                   "font-size:15px;font-weight:700;").arg(bg, fg);
}
}  // namespace

G7Wizard::G7Wizard(QWidget* parent) : QWidget(parent), held_(new Held) {
    setStyleSheet("background:#f6f7f9;");
    setMinimumSize(560, 480);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* title = new QLabel(QStringLiteral("G7+ 灰階向導"));
    title->setStyleSheet("font-size:20px;font-weight:700;color:#23262b;");
    root->addWidget(title);

    stack_ = new QStackedWidget;
    root->addWidget(stack_, 1);
    buildPrintPage();
    buildMeasurePage();
    buildResultPage();
    buildReportPage();

    auto* nav = new QHBoxLayout;
    backBtn_ = new QPushButton(QStringLiteral("← 上一步"));
    backBtn_->setCursor(Qt::PointingHandCursor);
    backBtn_->setStyleSheet("QPushButton{background:#fff;border:1px solid #c4c9cf;border-radius:8px;"
                            "padding:9px 18px;font-size:13px;}");
    nextBtn_ = new QPushButton(QStringLiteral("下一步 →"));
    nextBtn_->setCursor(Qt::PointingHandCursor);
    nextBtn_->setStyleSheet(QString("QPushButton{background:%1;color:#fff;border:none;border-radius:8px;"
                                    "padding:9px 20px;font-size:13px;font-weight:600;}").arg(kAccent));
    nav->addWidget(backBtn_);
    nav->addStretch();
    nav->addWidget(nextBtn_);
    root->addLayout(nav);

    connect(backBtn_, &QPushButton::clicked, this, [this] { goTo(stack_->currentIndex() - 1); });
    connect(nextBtn_, &QPushButton::clicked, this, [this] {
        int i = stack_->currentIndex();
        if (i == 1 && !measured_) runMeasurement();   // measure before showing results
        goTo(i + 1);
    });

    goTo(0);
}

void G7Wizard::buildPrintPage() {
    auto* page = new QWidget;
    auto* l = new QVBoxLayout(page);
    auto spec = cal::G7TestPattern::extended_g7_pattern();
    auto patches = cal::G7TestPattern::get_required_patches(spec);
    auto* lead = new QLabel(QStringLiteral(
        "步驟 1 · 列印 G7 靶\n\n列印 P2P 灰階 + 灰平衡靶（%1×%2 色塊，含灰階梯與灰平衡塊），"
        "晾乾後以 i1Pro3 量測。")
        .arg(spec.width_patches).arg(spec.height_patches));
    lead->setWordWrap(true);
    lead->setStyleSheet("font-size:13px;color:#3b3f46;");
    l->addWidget(lead);
    auto* count = new QLabel(QStringLiteral("靶塊總數：%1").arg(static_cast<int>(patches.size())));
    count->setStyleSheet("font-size:12px;color:#868c95;");
    l->addWidget(count);
    auto* printBtn = new QPushButton(QStringLiteral("列印 G7 靶"));
    printBtn->setCursor(Qt::PointingHandCursor);
    printBtn->setStyleSheet(QString("QPushButton{background:%1;color:#fff;border:none;border-radius:8px;"
                                    "padding:9px 18px;font-size:13px;font-weight:600;}").arg(kAccent));
    l->addWidget(printBtn, 0, Qt::AlignLeft);
    l->addStretch();
    stack_->addWidget(page);
}

void G7Wizard::buildMeasurePage() {
    auto* page = new QWidget;
    auto* l = new QVBoxLayout(page);
    auto* lead = new QLabel(QStringLiteral(
        "步驟 2 · 量測\n\n以 i1Pro3 掃描靶圖。按「下一步」開始量測並評估灰平衡。"));
    lead->setWordWrap(true);
    lead->setStyleSheet("font-size:13px;color:#3b3f46;");
    l->addWidget(lead);
    auto* note = new QLabel(QStringLiteral(
        "（尚未接上 i1Pro3：此處使用示範量測資料，評估與報告均跑真實引擎程式碼。）"));
    note->setWordWrap(true);
    note->setStyleSheet("font-size:12px;color:#8a6310;background:#fbf2dd;border-radius:8px;padding:10px 12px;");
    l->addWidget(note);
    l->addStretch();
    stack_->addWidget(page);
}

void G7Wizard::buildResultPage() {
    auto* page = new QWidget;
    auto* l = new QVBoxLayout(page);
    resultBadge_ = new QLabel;
    resultBadge_->setAlignment(Qt::AlignCenter);
    l->addWidget(resultBadge_, 0, Qt::AlignLeft);
    resultMetrics_ = new QLabel;
    resultMetrics_->setStyleSheet("font-size:13px;color:#3b3f46;");
    l->addWidget(resultMetrics_);

    patchTable_ = new QTableWidget;
    patchTable_->setColumnCount(6);
    patchTable_->setHorizontalHeaderLabels(
        {QStringLiteral("#"), QStringLiteral("名稱"), QStringLiteral("目標 L*"),
         QStringLiteral("量測 L*"), QStringLiteral("ΔE"), QStringLiteral("判定")});
    patchTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    patchTable_->verticalHeader()->setVisible(false);
    patchTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    patchTable_->setStyleSheet("QTableWidget{background:#fff;border:1px solid #d2d5d9;border-radius:6px;}");
    l->addWidget(patchTable_, 1);
    stack_->addWidget(page);
}

void G7Wizard::buildReportPage() {
    auto* page = new QWidget;
    auto* l = new QVBoxLayout(page);
    auto* lead = new QLabel(QStringLiteral("步驟 4 · 匯出報告\n\n輸出 G7 驗證報告（HTML），含通過率、灰平衡與逐塊 ΔE。"));
    lead->setWordWrap(true);
    lead->setStyleSheet("font-size:13px;color:#3b3f46;");
    l->addWidget(lead);
    auto* exportBtn = new QPushButton(QStringLiteral("匯出 HTML 報告…"));
    exportBtn->setCursor(Qt::PointingHandCursor);
    exportBtn->setStyleSheet(QString("QPushButton{background:%1;color:#fff;border:none;border-radius:8px;"
                                     "padding:9px 18px;font-size:13px;font-weight:600;}").arg(kAccent));
    connect(exportBtn, &QPushButton::clicked, this, &G7Wizard::exportReport);
    l->addWidget(exportBtn, 0, Qt::AlignLeft);
    reportStatus_ = new QLabel;
    reportStatus_->setWordWrap(true);
    reportStatus_->setStyleSheet("font-size:12.5px;color:#1d6b39;");
    l->addWidget(reportStatus_);
    l->addStretch();
    stack_->addWidget(page);
}

void G7Wizard::runMeasurement() {
    // Synthesise a P2P gray-ramp result (demo). Deterministic for reproducibility.
    std::mt19937 rng(20260605u);
    std::normal_distribution<double> jitter(0.0, 0.35);

    cal::G7CalibrationResult& r = held_->result;
    r = cal::G7CalibrationResult{};
    r.profile_name = "B&W Multi-K G7+";
    r.measurement_date = std::chrono::system_clock::now();

    const int kPatches = 11;   // 0,10,...,100% gray
    double sum = 0.0, maxDe = 0.0, minDe = 1e9, sumGB = 0.0;
    for (int i = 0; i < kPatches; ++i) {
        double pct = i * 10.0;
        double targetL = 100.0 - pct;                  // gray ramp target L*
        double da = jitter(rng), db = jitter(rng);
        double dL = jitter(rng) * 0.5;
        double de = std::sqrt(dL * dL + da * da + db * db);

        cal::G7PatchResult p;
        p.patch_number = i + 1;
        p.patch_name = "Gray " + std::to_string(static_cast<int>(pct)) + "%";
        p.target_L = targetL; p.target_a = 0.0; p.target_b = 0.0;
        p.measured_L = targetL + dL; p.measured_a = da; p.measured_b = db;
        p.delta_E = de;
        p.passes = de <= kTolerance;
        r.patch_results.push_back(p);

        sum += de; maxDe = std::max(maxDe, de); minDe = std::min(minDe, de);
        sumGB += std::sqrt(da * da + db * db);
        if (p.passes) r.passing_patches++; else r.failing_patches++;
    }
    r.total_patches = kPatches;
    r.average_delta_E = sum / kPatches;
    r.max_delta_E = maxDe;
    r.min_delta_E = minDe;
    r.gray_balance_error = sumGB / kPatches;
    r.tonal_response_error = 0.0;
    r.calibration_achieved = cal::G7ComplianceChecker::check_conformance(r, kTolerance);

    // Engine grading
    auto level = cal::G7ComplianceChecker::determine_compliance_level(r, kTolerance);
    double gbScore = cal::G7ComplianceChecker::calculate_gray_balance_score(r);
    passed_ = r.calibration_achieved;
    avgDeltaE_ = r.average_delta_E;
    measured_ = true;

    // Badge
    QString bg = "#9aa0a6", fg = "#fff", text;
    switch (level) {
        case cal::G7ComplianceChecker::ComplianceLevel::Perfect:
            bg = "#1d6b39"; text = QStringLiteral("PERFECT · 完美達標"); break;
        case cal::G7ComplianceChecker::ComplianceLevel::Acceptable:
            bg = "#2e7dd7"; text = QStringLiteral("ACCEPTABLE · 合格"); break;
        case cal::G7ComplianceChecker::ComplianceLevel::Warning:
            bg = "#b8860b"; text = QStringLiteral("WARNING · 警告"); break;
        case cal::G7ComplianceChecker::ComplianceLevel::Failed:
            bg = "#b3261e"; text = QStringLiteral("FAILED · 未通過"); break;
    }
    resultBadge_->setText(text);
    resultBadge_->setStyleSheet(badgeStyle(bg, fg));
    resultMetrics_->setText(QStringLiteral(
        "平均 ΔE %1　最大 ΔE %2　灰平衡誤差 %3　灰平衡分數 %4\n通過 %5 / %6 塊")
        .arg(r.average_delta_E, 0, 'f', 2).arg(r.max_delta_E, 0, 'f', 2)
        .arg(r.gray_balance_error, 0, 'f', 2).arg(gbScore, 0, 'f', 2)
        .arg(r.passing_patches).arg(r.total_patches));

    // Table
    patchTable_->setRowCount(static_cast<int>(r.patch_results.size()));
    for (int i = 0; i < static_cast<int>(r.patch_results.size()); ++i) {
        const auto& p = r.patch_results[i];
        auto set = [&](int c, const QString& s, const QColor& fgc = QColor("#23262b")) {
            auto* it = new QTableWidgetItem(s);
            it->setForeground(fgc);
            patchTable_->setItem(i, c, it);
        };
        set(0, QString::number(p.patch_number));
        set(1, QString::fromStdString(p.patch_name));
        set(2, QString::number(p.target_L, 'f', 1));
        set(3, QString::number(p.measured_L, 'f', 1));
        set(4, QString::number(p.delta_E, 'f', 2));
        set(5, p.passes ? QStringLiteral("✓") : QStringLiteral("✗"),
            p.passes ? QColor("#1d6b39") : QColor("#b3261e"));
    }

    emit validated(passed_, avgDeltaE_);
}

void G7Wizard::exportReport() {
    if (!measured_) { reportStatus_->setText(QStringLiteral("請先完成量測。")); return; }
    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("匯出 G7 報告"), "g7-report.pdf",
        QStringLiteral("PDF (*.pdf);;HTML (*.html)"), &selectedFilter);
    if (path.isEmpty()) return;

    cal::G7ValidationReport report;
    cal::G7ReportMetadata meta;
    meta.software_version = "jcolour EA";
    meta.notes = "B&W Multi-K G7+ gray balance";
    report.set_metadata(meta);
    report.set_calibration_result(held_->result);

    const bool wantPdf = path.endsWith(".pdf", Qt::CaseInsensitive) ||
                         selectedFilter.startsWith("PDF");
    bool ok = false;
    if (wantPdf) {
        if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";
        ok = exportPdf(report, path);
    } else {
        ok = report.generate(cal::ReportFormat::HTML, path.toStdString());
    }
    reportStatus_->setStyleSheet(ok ? "font-size:12.5px;color:#1d6b39;"
                                    : "font-size:12.5px;color:#b3261e;");
    reportStatus_->setText(ok ? QStringLiteral("✓ 已匯出：%1").arg(path)
                              : QStringLiteral("✗ 匯出失敗"));
}

bool G7Wizard::exportPdf(cal::G7ValidationReport& report, const QString& path) {
    // The engine emits styled HTML; render it to a real PDF via Qt so the output
    // is a genuine paginated document (not HTML bytes in a .pdf wrapper).
    QString tmp = QDir::temp().filePath("jcolour-g7-XXXXXX.html");
    QTemporaryFile tf(tmp);
    tf.setAutoRemove(true);
    if (!tf.open()) return false;
    const QString htmlPath = tf.fileName();
    tf.close();
    if (!report.generate(cal::ReportFormat::HTML, htmlPath.toStdString())) return false;

    QFile in(htmlPath);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString html = QString::fromUtf8(in.readAll());
    in.close();

    QTextDocument doc;
    doc.setHtml(html);
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    doc.setPageSize(QSizeF(writer.width(), writer.height()));
    doc.print(&writer);
    return QFile::exists(path) && QFileInfo(path).size() > 0;
}

void G7Wizard::goTo(int page) {
    page = std::clamp(page, 0, stack_->count() - 1);
    stack_->setCurrentIndex(page);
    backBtn_->setEnabled(page > 0);
    nextBtn_->setVisible(page < stack_->count() - 1);
}

}  // namespace ui
}  // namespace jcolour
