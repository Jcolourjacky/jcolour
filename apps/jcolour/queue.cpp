#include "queue.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QFile>
#include <QUuid>

namespace {
using jcolour::queue::PrintJob;
using jcolour::queue::BlackInk;

// Resolve a job's required black. Prefer the explicit field; fall back to a
// preset-name heuristic for jobs created before the field was populated.
BlackInk requiredBlack(const PrintJob& job) {
    if (job.blackInk != BlackInk::Either) return job.blackInk;
    if (job.presetName.contains("Matte", Qt::CaseInsensitive)) return BlackInk::MK;
    if (job.presetName.contains("Photo", Qt::CaseInsensitive))  return BlackInk::PK;
    return BlackInk::Either;
}

// Count physical PK↔MK swaps for a given ordering. Either-ink jobs print with
// whatever is loaded and never force a swap; the first real ink is a load, not
// a switch.
int countSwitches(const QVector<PrintJob>& jobs) {
    int switches = 0;
    BlackInk loaded = BlackInk::Either;
    for (const auto& job : jobs) {
        BlackInk need = requiredBlack(job);
        if (need == BlackInk::Either) continue;
        if (loaded != BlackInk::Either && need != loaded) ++switches;
        loaded = need;
    }
    return switches;
}

// Batch like-ink jobs: all PK, then ink-agnostic, then all MK — so at most one
// swap occurs and Either jobs ride whichever black is already loaded. Relative
// order within each group is preserved.
QVector<PrintJob> groupByInk(const QVector<PrintJob>& jobs) {
    QVector<PrintJob> pk, either, mk;
    for (const auto& job : jobs) {
        switch (requiredBlack(job)) {
            case BlackInk::PK: pk.append(job); break;
            case BlackInk::MK: mk.append(job); break;
            default:           either.append(job); break;
        }
    }
    return pk + either + mk;
}
}  // namespace

PrintQueueWidget::PrintQueueWidget(QWidget* parent)
    : QWidget(parent)
    , isProcessing_(false)
{
    auto* layout = new QVBoxLayout(this);

    // Stats bar
    auto* statsLayout = new QHBoxLayout;
    statsLayout->addWidget(new QLabel("Pending:"));
    auto* pendingLabel = new QLabel("0");
    statsLayout->addWidget(pendingLabel);
    statsLayout->addWidget(new QLabel("Completed:"));
    auto* completedLabel = new QLabel("0");
    statsLayout->addWidget(completedLabel);
    statsLayout->addWidget(new QLabel("PK/MK Switches:"));
    auto* switchesLabel = new QLabel("0");
    statsLayout->addWidget(switchesLabel);
    statsLayout->addWidget(new QLabel("Est. Time:"));
    auto* timeLabel = new QLabel("--");
    statsLayout->addWidget(timeLabel);
    statsLayout->addStretch();
    layout->addLayout(statsLayout);

    // Queue table
    jobTable_ = new QTableWidget;
    jobTable_->setColumnCount(5);
    jobTable_->setHorizontalHeaderLabels({"#", "Image", "Preset", "Status", "Actions"});
    jobTable_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(jobTable_);

    // Controls
    auto* controlsLayout = new QHBoxLayout;
    auto* addBtn = new QPushButton("Add Jobs...");
    auto* removeBtn = new QPushButton("Remove");
    auto* moveUpBtn = new QPushButton("Move Up");
    auto* moveDownBtn = new QPushButton("Move Down");
    auto* optimizeBtn = new QPushButton("Optimize Order");
    controlsLayout->addWidget(addBtn);
    controlsLayout->addWidget(removeBtn);
    controlsLayout->addWidget(moveUpBtn);
    controlsLayout->addWidget(moveDownBtn);
    controlsLayout->addWidget(optimizeBtn);
    controlsLayout->addStretch();

    auto* processLayout = new QHBoxLayout;
    auto* startBtn = new QPushButton("Start Processing");
    auto* pauseBtn = new QPushButton("Pause");
    processLayout->addWidget(startBtn);
    processLayout->addWidget(pauseBtn);
    processLayout->addStretch();
    controlsLayout->addLayout(processLayout);

    layout->addLayout(controlsLayout);

    // Connect signals
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(this, "Select Images",
                                                         QString(),
                                                         "Images (*.tif *.tiff *.png *.jpg)");
        for (const QString& file : files) {
            jcolour::queue::PrintJob job;
            job.id = QUuid::createUuid().toString();
            job.imagePath = file;
            job.status = "pending";
            job.submittedAt = QDateTime::currentDateTime();
            addJob(job);
        }
    });
    connect(removeBtn, &QPushButton::clicked, this, &PrintQueueWidget::onRemoveJob);
    connect(moveUpBtn, &QPushButton::clicked, this, &PrintQueueWidget::onMoveUp);
    connect(moveDownBtn, &QPushButton::clicked, this, &PrintQueueWidget::onMoveDown);
    connect(optimizeBtn, &QPushButton::clicked, this, [this]() {
        optimizeQueueOrder();
        updateQueueDisplay();
    });
    connect(startBtn, &QPushButton::clicked, this, &PrintQueueWidget::startProcessing);
    connect(pauseBtn, &QPushButton::clicked, this, &PrintQueueWidget::pauseProcessing);
}

PrintQueueWidget::~PrintQueueWidget() = default;

void PrintQueueWidget::addJob(const jcolour::queue::PrintJob& job) {
    pendingJobs_.enqueue(job);
    updateQueueDisplay();
}

void PrintQueueWidget::removeJob(const QString& jobId) {
    QQueue<jcolour::queue::PrintJob> newQueue;
    while (!pendingJobs_.isEmpty()) {
        auto job = pendingJobs_.dequeue();
        if (job.id != jobId) {
            newQueue.enqueue(job);
        }
    }
    pendingJobs_ = newQueue;
    updateQueueDisplay();
}

void PrintQueueWidget::reorderJobs() {
    optimizeQueueOrder();
    updateQueueDisplay();
    emit queueReordered();
}

void PrintQueueWidget::startProcessing() {
    if (isProcessing_) return;
    isProcessing_ = true;
    processNextJob();
}

void PrintQueueWidget::pauseProcessing() {
    isProcessing_ = false;
}

jcolour::queue::QueueStats PrintQueueWidget::getStats() const {
    jcolour::queue::QueueStats stats{};
    stats.totalJobs = pendingJobs_.size() + completedJobs_.size();
    stats.pendingJobs = pendingJobs_.size();
    stats.completedJobs = completedJobs_.size();
    stats.pkMkSwitches = countPkMkSwitches();
    return stats;
}

void PrintQueueWidget::updateQueueDisplay() {
    jobTable_->setRowCount(pendingJobs_.size());
    int row = 0;
    for (const auto& job : pendingJobs_) {
        jobTable_->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        jobTable_->setItem(row, 1, new QTableWidgetItem(QFileInfo(job.imagePath).fileName()));
        jobTable_->setItem(row, 2, new QTableWidgetItem(job.presetName));
        jobTable_->setItem(row, 3, new QTableWidgetItem(job.status));
        row++;
    }
}

void PrintQueueWidget::optimizeQueueOrder() {
    // Batch pending jobs by required black to minimise PK/MK swaps.
    QVector<jcolour::queue::PrintJob> jobs(pendingJobs_.begin(), pendingJobs_.end());
    QVector<jcolour::queue::PrintJob> optimized = groupByInk(jobs);
    pendingJobs_.clear();
    for (const auto& job : optimized) pendingJobs_.enqueue(job);
}

int PrintQueueWidget::countPkMkSwitches() const {
    QVector<jcolour::queue::PrintJob> jobs(pendingJobs_.begin(), pendingJobs_.end());
    return countSwitches(jobs);
}

void PrintQueueWidget::processNextJob() {
    if (!isProcessing_ || pendingJobs_.isEmpty()) {
        if (pendingJobs_.isEmpty()) {
            emit allJobsCompleted();
        }
        return;
    }

    auto job = pendingJobs_.dequeue();
    emit jobStarted(job.id);

    // Simulate processing
    QTimer::singleShot(1000, this, [this, job]() {
        completedJobs_.enqueue(job);
        emit jobCompleted(job.id);
        processNextJob();
    });
}

void PrintQueueWidget::onJobSelected(int row) {
    // Selection handling
}

void PrintQueueWidget::onRemoveJob() {
    int row = jobTable_->currentRow();
    if (row >= 0 && row < pendingJobs_.size()) {
        removeJob(pendingJobs_[row].id);
    }
}

void PrintQueueWidget::onMoveUp() {
    int row = jobTable_->currentRow();
    if (row > 0) {
        auto it = pendingJobs_.begin() + row;
        auto prevIt = it - 1;
        std::swap(*it, *prevIt);
        updateQueueDisplay();
    }
}

void PrintQueueWidget::onMoveDown() {
    int row = jobTable_->currentRow();
    if (row < pendingJobs_.size() - 1) {
        auto it = pendingJobs_.begin() + row;
        auto nextIt = it + 1;
        std::swap(*it, *nextIt);
        updateQueueDisplay();
    }
}

void PrintQueueWidget::onRetryFailed() {
    // Retry failed jobs
}

void PrintQueueWidget::onClearCompleted() {
    completedJobs_.clear();
    updateQueueDisplay();
}

// BatchQueueOptimizer implementation
BatchQueueOptimizer::BatchQueueOptimizer(QWidget* parent)
    : QWidget(parent)
    , originalSwitches_(0)
    , optimizedSwitches_(0)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Queue Optimizer - groups jobs to minimize ink switches"));
}

BatchQueueOptimizer::~BatchQueueOptimizer() = default;

void BatchQueueOptimizer::setJobs(const QVector<jcolour::queue::PrintJob>& jobs) {
    originalJobs_ = jobs;
    originalSwitches_ = calculateSwitches(jobs);
    calculateSavings();
}

QVector<jcolour::queue::PrintJob> BatchQueueOptimizer::getOptimizedOrder() const {
    return optimizedJobs_;
}

void BatchQueueOptimizer::groupByBlackInk() {
    optimizedJobs_ = groupByInk(originalJobs_);
    optimizedSwitches_ = calculateSwitches(optimizedJobs_);
}

int BatchQueueOptimizer::calculateSwitches(const QVector<jcolour::queue::PrintJob>& jobs) const {
    return countSwitches(jobs);
}

void BatchQueueOptimizer::calculateSavings() {
    groupByBlackInk();
    // Display savings to user
}

// EditionTrackingDialog implementation
EditionTrackingDialog::EditionTrackingDialog(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Edition Tracking");
    setMinimumSize(800, 600);

    auto* layout = new QVBoxLayout(this);

    auto* toolbar = new QHBoxLayout;
    auto* newEdBtn = new QPushButton("New Edition...");
    auto* recordBtn = new QPushButton("Record Print...");
    toolbar->addWidget(newEdBtn);
    toolbar->addWidget(recordBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    loadFromStorage();
}

EditionTrackingDialog::~EditionTrackingDialog() = default;

void EditionTrackingDialog::createEdition(const QString& title, int editionSize) {
    Edition ed;
    ed.id = QUuid::createUuid().toString();
    ed.title = title;
    ed.editionSize = editionSize;
    ed.currentNumber = 0;
    ed.createdAt = QDateTime::currentDateTime();
    editions_.append(ed);
    saveToStorage();
    emit editionCreated(ed.id);
}

void EditionTrackingDialog::recordPrint(const QString& editionId, int number, const PrintRecord& record) {
    for (auto& ed : editions_) {
        if (ed.id == editionId) {
            ed.prints.append(record);
            ed.currentNumber = qMax(ed.currentNumber, number);
            saveToStorage();
            emit printRecorded(editionId, number);
            break;
        }
    }
}

QVector<EditionTrackingDialog::PrintRecord> EditionTrackingDialog::getEditionHistory(const QString& editionId) const {
    for (const auto& ed : editions_) {
        if (ed.id == editionId) {
            return ed.prints;
        }
    }
    return {};
}

void EditionTrackingDialog::loadFromStorage() {
    QFile file("editions.json");
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray arr = doc.array();
        for (const auto& val : arr) {
            // Parse editions from JSON
            Q_UNUSED(val);
        }
    }
}

void EditionTrackingDialog::saveToStorage() {
    QJsonArray arr;
    for (const auto& ed : editions_) {
        QJsonObject obj;
        obj["id"] = ed.id;
        obj["title"] = ed.title;
        obj["editionSize"] = ed.editionSize;
        // ... serialize other fields
        arr.append(obj);
    }
    QFile file("editions.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
    }
}

// InkCostEstimator implementation
InkCostEstimator::InkCostEstimator(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Ink Cost Estimator"));
}

InkCostEstimator::~InkCostEstimator() = default;

InkCostEstimator::InkUsage InkCostEstimator::estimateForJob(const QString& imagePath, int copies) {
    InkUsage usage{};
    double coverage = calculateCoverage(imagePath);
    double baseInk = coverage * 0.1; // ml per print

    usage.pk_ml = baseInk * 0.4 * copies;
    usage.mk_ml = baseInk * 0.4 * copies;
    usage.lc_ml = baseInk * 0.1 * copies;
    usage.c_ml = baseInk * 0.15 * copies;
    usage.lm_ml = baseInk * 0.08 * copies;
    usage.m_ml = baseInk * 0.12 * copies;
    usage.y_ml = baseInk * 0.1 * copies;
    usage.or_ml = baseInk * 0.05 * copies;
    usage.lk_ml = baseInk * 0.2 * copies;
    usage.llk_ml = baseInk * 0.15 * copies;
    usage.v_ml = baseInk * 0.03 * copies;
    usage.gr_ml = 0;

    usage.total_ml = usage.pk_ml + usage.mk_ml + usage.lc_ml + usage.c_ml +
                    usage.lm_ml + usage.m_ml + usage.y_ml + usage.or_ml +
                    usage.lk_ml + usage.llk_ml + usage.v_ml + usage.gr_ml;

    usage.total_cost = usage.pk_ml * PK_COST_PER_ML +
                      usage.mk_ml * MK_COST_PER_ML +
                      usage.total_ml * COLOR_COST_PER_ML;

    return usage;
}

InkCostEstimator::InkUsage InkCostEstimator::estimateForQueue(const QVector<jcolour::queue::PrintJob>& jobs) {
    InkUsage total{};
    for (const auto& job : jobs) {
        auto usage = estimateForJob(job.imagePath, job.copies);
        total.pk_ml += usage.pk_ml;
        total.mk_ml += usage.mk_ml;
        total.c_ml += usage.c_ml;
        total.total_ml += usage.total_ml;
        total.total_cost += usage.total_cost;
    }
    return total;
}

double InkCostEstimator::calculateCoverage(const QString& imagePath) const {
    // Estimate ink coverage based on image analysis
    // This would actually analyze the image for ink coverage
    return 0.25; // 25% coverage estimate
}
