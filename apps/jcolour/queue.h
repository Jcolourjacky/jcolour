#pragma once

#include <QWidget>
#include <QVector>
#include <QQueue>
#include <QTableWidget>
#include <QDateTime>

namespace jcolour {
namespace queue {

// Physical black a job requires. Epson PRO printers hold PK and MK in the head
// but only one feeds at a time; a PK↔MK swap wastes ink and ~2 min of purge, so
// the queue batches like-ink jobs together. Either = colour/RGB jobs that print
// fine with whichever black is currently loaded (they don't force a swap).
enum class BlackInk { Either = 0, PK, MK };

struct PrintJob {
    QString id;
    QString imagePath;
    QString presetName;
    int copies;
    QString status; // pending, printing, completed, failed
    QDateTime submittedAt;
    QDateTime startedAt;
    QDateTime completedAt;
    QString errorMessage;
    BlackInk blackInk = BlackInk::Either;  // set from the job's paper/ink mode; Either if unknown
};

struct QueueStats {
    int totalJobs;
    int pendingJobs;
    int completedJobs;
    int failedJobs;
    double estimatedInkUsage; // ml
    double estimatedTime; // minutes
    int pkMkSwitches;
};

}
}

class PrintQueueWidget : public QWidget {
    Q_OBJECT
public:
    explicit PrintQueueWidget(QWidget* parent = nullptr);
    ~PrintQueueWidget() override;

    void addJob(const jcolour::queue::PrintJob& job);
    void removeJob(const QString& jobId);
    // 底部队列监控条（ErgoSoft PrintQueue 式）读取用的快照
    QVector<jcolour::queue::PrintJob> pendingSnapshot() const {
        QVector<jcolour::queue::PrintJob> v;
        for (const auto& j : pendingJobs_) v.push_back(j);
        return v;
    }
    QVector<jcolour::queue::PrintJob> completedSnapshot() const {
        QVector<jcolour::queue::PrintJob> v;
        for (const auto& j : completedJobs_) v.push_back(j);
        return v;
    }
    bool processing() const { return isProcessing_; }
    void reorderJobs();
    void startProcessing();
    void pauseProcessing();

    jcolour::queue::QueueStats getStats() const;

signals:
    void jobStarted(const QString& jobId);
    void jobCompleted(const QString& jobId);
    void jobFailed(const QString& jobId, const QString& error);
    void queueReordered();
    void allJobsCompleted();

private slots:
    void onJobSelected(int row);
    void onRemoveJob();
    void onMoveUp();
    void onMoveDown();
    void onRetryFailed();
    void onClearCompleted();

private:
    void updateQueueDisplay();
    void optimizeQueueOrder();
    int countPkMkSwitches() const;
    void processNextJob();

    QTableWidget* jobTable_;
    QQueue<jcolour::queue::PrintJob> pendingJobs_;
    QQueue<jcolour::queue::PrintJob> completedJobs_;
    bool isProcessing_;
};

class BatchQueueOptimizer : public QWidget {
    Q_OBJECT
public:
    explicit BatchQueueOptimizer(QWidget* parent = nullptr);
    ~BatchQueueOptimizer() override;

    void setJobs(const QVector<jcolour::queue::PrintJob>& jobs);
    QVector<jcolour::queue::PrintJob> getOptimizedOrder() const;

signals:
    void orderOptimized(const QVector<jcolour::queue::PrintJob>& optimized);

private:
    void groupByBlackInk();
    void calculateSavings();
    int calculateSwitches(const QVector<jcolour::queue::PrintJob>& jobs) const;

    QVector<jcolour::queue::PrintJob> originalJobs_;
    QVector<jcolour::queue::PrintJob> optimizedJobs_;
    int originalSwitches_;
    int optimizedSwitches_;
};

class EditionTrackingDialog : public QWidget {
    Q_OBJECT
public:
    explicit EditionTrackingDialog(QWidget* parent = nullptr);
    ~EditionTrackingDialog() override;

    struct PrintRecord {
        int number;
        QString buyer;
        QDateTime printedAt;
        QString presetUsed;
        QString printerUsed;
        QString notes;
    };

    struct Edition {
        QString id;
        QString imageHash;
        QString title;
        int editionSize;
        int currentNumber;
        QDateTime createdAt;
        QVector<PrintRecord> prints;
    };

    void createEdition(const QString& title, int editionSize);
    void recordPrint(const QString& editionId, int number, const PrintRecord& record);
    QVector<PrintRecord> getEditionHistory(const QString& editionId) const;

signals:
    void editionCreated(const QString& id);
    void printRecorded(const QString& editionId, int number);

private:
    void loadFromStorage();
    void saveToStorage();

    QVector<Edition> editions_;
};

class InkCostEstimator : public QWidget {
    Q_OBJECT
public:
    explicit InkCostEstimator(QWidget* parent = nullptr);
    ~InkCostEstimator() override;

    struct InkUsage {
        double pk_ml;
        double mk_ml;
        double lc_ml;
        double c_ml;
        double lm_ml;
        double m_ml;
        double y_ml;
        double or_ml;
        double lk_ml;
        double llk_ml;
        double v_ml;
        double gr_ml;
        double total_ml;
        double total_cost;
    };

    InkUsage estimateForJob(const QString& imagePath, int copies);
    InkUsage estimateForQueue(const QVector<jcolour::queue::PrintJob>& jobs);

signals:
    void lowInkWarning(const QString& inkType, double remaining);

private:
    double calculateCoverage(const QString& imagePath) const;
    double estimateInkForDensity(const QVector<double>& densityValues);

    // Ink prices (approximate)
    static constexpr double PK_COST_PER_ML = 0.15;
    static constexpr double MK_COST_PER_ML = 0.12;
    static constexpr double COLOR_COST_PER_ML = 0.08;
};
