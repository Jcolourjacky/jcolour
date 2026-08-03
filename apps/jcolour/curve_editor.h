#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QCheckBox>
#include <QSlider>
#include <QVector>
#include <QPointF>
#include <QPainter>
#include <QStringList>

class InkCurveEditor : public QWidget {
    Q_OBJECT

public:
    explicit InkCurveEditor(QWidget* parent = nullptr);
    ~InkCurveEditor() override;

    QVector<double> getCurveValues(int resolution = 256) const;
    void setNeutralCurve();

signals:
    void curveChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void initControlPoints();
    QPointF mapToWidget(const QPointF& p) const;
    QPointF mapFromWidget(const QPointF& pos) const;

    QVector<QPointF> controlPoints_;
    int activePointIndex_;
    int currentInputValue_;
    int currentOutputValue_;
    QRect curveArea_;
    static constexpr int MARGIN = 40;
    static constexpr int POINT_COUNT = 26;
};

class WorkflowManager : public QWidget {
    Q_OBJECT

public:
    explicit WorkflowManager(QWidget* parent = nullptr);
    ~WorkflowManager() override;

    struct Workflow {
        QString name;
        QString iccProfile;
        int resolution;
        QString paperType;
        QString inkMode;
        bool isDefault;
    };

    QList<Workflow> getAllWorkflows() const;
    QString defaultWorkflow() const;
    int currentWorkflowIndex() const { return currentIndex_; }

signals:
    void workflowChanged(int index);

private slots:
    void onWorkflowSelected(int row);
    void onPaperTypeChanged(int index);

private:
    void loadWorkflows();
    void updateForm(int row);

    QList<Workflow> workflows_;
    int currentIndex_;
    QListWidget* list_;
    QComboBox* paperCombo_;
};

class InkCurvesPanel : public QWidget {
    Q_OBJECT

public:
    explicit InkCurvesPanel(QWidget* parent = nullptr);
    ~InkCurvesPanel() override;

    static constexpr int NUM_INK_CHANNELS = 12;
    static const QStringList CHANNEL_NAMES;

signals:
    void curvesChanged();

private slots:
    void onChannelSelected(int index);

private:
    QTabWidget* channelTabs_;
    QVector<InkCurveEditor*> curveEditors_;
    QSpinBox* inputSpin_;
    QSpinBox* outputSpin_;
    QSlider* lightnessSlider_;
};