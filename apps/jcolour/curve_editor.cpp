#include "curve_editor.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QLabel>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>

const QStringList InkCurvesPanel::CHANNEL_NAMES = {
    "PK", "MK", "LC", "C", "LM", "M", "Y", "Or", "LK", "LLK", "V", "Gr"
};

InkCurveEditor::InkCurveEditor(QWidget* parent)
    : QWidget(parent)
    , activePointIndex_(-1)
    , currentInputValue_(0)
    , currentOutputValue_(0)
{
    setMinimumSize(400, 300);
    initControlPoints();
}

InkCurveEditor::~InkCurveEditor() = default;

void InkCurveEditor::initControlPoints() {
    controlPoints_.clear();
    for (int i = 0; i < POINT_COUNT; ++i) {
        int x = i * 10;
        controlPoints_.append(QPointF(x, x));
    }
}

QPointF InkCurveEditor::mapToWidget(const QPointF& p) const {
    if (curveArea_.isEmpty()) return QPointF();
    double wx = curveArea_.left() + (p.x() / 255.0) * curveArea_.width();
    double wy = curveArea_.bottom() - (p.y() / 255.0) * curveArea_.height();
    return QPointF(wx, wy);
}

QPointF InkCurveEditor::mapFromWidget(const QPointF& pos) const {
    if (curveArea_.isEmpty()) return QPointF();
    double x = ((pos.x() - curveArea_.left()) / curveArea_.width()) * 255.0;
    double y = ((curveArea_.bottom() - pos.y()) / curveArea_.height()) * 255.0;
    return QPointF(qBound(0.0, x, 255.0), qBound(0.0, y, 255.0));
}

void InkCurveEditor::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#e6e6e6"));

    if (curveArea_.isEmpty()) return;

    // Grid
    QPen gridPen(QColor(60, 60, 60));
    painter.setPen(gridPen);
    for (int i = 0; i <= 10; ++i) {
        double ratio = i / 10.0;
        int value = static_cast<int>(ratio * 255);

        QPointF start = mapToWidget(QPointF(value, 0));
        QPointF end = mapToWidget(QPointF(value, 255));
        painter.drawLine(start, end);

        start = mapToWidget(QPointF(0, value));
        end = mapToWidget(QPointF(255, value));
        painter.drawLine(start, end);
    }

    // Diagonal
    QPen diagPen(QColor(80, 80, 80));
    diagPen.setStyle(Qt::DashLine);
    painter.setPen(diagPen);
    painter.drawLine(mapToWidget(QPointF(0, 0)), mapToWidget(QPointF(255, 255)));

    // Curve
    if (controlPoints_.size() >= 2) {
        QPainterPath path;
        path.moveTo(mapToWidget(controlPoints_.first()));
        for (int i = 1; i < controlPoints_.size(); ++i) {
            path.lineTo(mapToWidget(controlPoints_[i]));
        }
        painter.setPen(QPen(QColor("#00d4ff"), 3));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    // Control points
    for (int i = 0; i < controlPoints_.size(); ++i) {
        QPointF pos = mapToWidget(controlPoints_[i]);
        if (i == activePointIndex_) {
            painter.setPen(QColor("#ffffff"));
            painter.setBrush(QColor("#00d4ff"));
            painter.drawEllipse(pos, 8, 8);
        } else {
            painter.setPen(QColor("#00d4ff"));
            painter.setBrush(QColor("#e6e6e6"));
            painter.drawEllipse(pos, 6, 6);
        }
    }

    // Value info
    if (currentInputValue_ > 0 || currentOutputValue_ > 0) {
        QFont font = painter.font();
        font.setPixelSize(14);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(QColor("#ffffff"));
        QString text = QString("%1% → %2%").arg(currentInputValue_).arg(currentOutputValue_);
        QRect textRect(width() - 120, 10, 110, 30);
        painter.fillRect(textRect.adjusted(-5, -3, 5, 3), QColor(0, 0, 0, 150));
        painter.drawText(textRect, Qt::AlignCenter, text);
    }
}

void InkCurveEditor::mousePressEvent(QMouseEvent* event) {
    activePointIndex_ = -1;
    QPointF pos = event->pos();

    for (int i = 1; i < controlPoints_.size() - 1; ++i) {
        QPointF cp = mapToWidget(controlPoints_[i]);
        if ((pos - cp).manhattanLength() < 15) {
            activePointIndex_ = i;
            currentInputValue_ = static_cast<int>(controlPoints_[i].x());
            currentOutputValue_ = static_cast<int>(controlPoints_[i].y());
            break;
        }
    }

    if (activePointIndex_ < 0) {
        QPointF curvePos = mapFromWidget(pos);
        int nearestIdx = qRound(curvePos.x() / 10.0);
        nearestIdx = qBound(1, nearestIdx, POINT_COUNT - 2);
        activePointIndex_ = nearestIdx;
        controlPoints_[activePointIndex_].setY(curvePos.y());
        currentInputValue_ = static_cast<int>(controlPoints_[activePointIndex_].x());
        currentOutputValue_ = static_cast<int>(controlPoints_[activePointIndex_].y());
        update();
    }
}

void InkCurveEditor::mouseMoveEvent(QMouseEvent* event) {
    if (activePointIndex_ < 0) return;
    QPointF curvePos = mapFromWidget(event->pos());
    int y = qBound(0, static_cast<int>(curvePos.y()), 255);
    controlPoints_[activePointIndex_].setY(y);
    currentOutputValue_ = y;
    emit curveChanged();
    update();
}

void InkCurveEditor::mouseReleaseEvent(QMouseEvent*) {
    activePointIndex_ = -1;
    update();
}

void InkCurveEditor::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    curveArea_ = QRect(MARGIN, MARGIN, width() - 2 * MARGIN, height() - 2 * MARGIN);
    update();
}

QVector<double> InkCurveEditor::getCurveValues(int resolution) const {
    QVector<double> values(resolution);
    for (int i = 0; i < resolution; ++i) {
        double x = i * 255.0 / (resolution - 1);
        for (int j = 0; j < controlPoints_.size() - 1; ++j) {
            double x0 = controlPoints_[j].x();
            double x1 = controlPoints_[j + 1].x();
            if (x >= x0 && x <= x1) {
                double y0 = controlPoints_[j].y();
                double y1 = controlPoints_[j + 1].y();
                double t = (x1 > x0) ? (x - x0) / (x1 - x0) : 0;
                values[i] = y0 + t * (y1 - y0);
                break;
            }
        }
    }
    return values;
}

void InkCurveEditor::setNeutralCurve() {
    for (int i = 0; i < controlPoints_.size(); ++i) {
        int x = i * 10;
        controlPoints_[i] = QPointF(x, x);
    }
    currentInputValue_ = 0;
    currentOutputValue_ = 0;
    update();
    emit curveChanged();
}

// WorkflowManager - jcolour v1.0 has only 3 workflows
WorkflowManager::WorkflowManager(QWidget* parent)
    : QWidget(parent)
    , currentIndex_(-1)
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(16);

    // Left: workflow list with simple style
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(4);

    list_ = new QListWidget;
    list_->setMinimumWidth(280);
    list_->setSpacing(0);
    list_->setStyleSheet(R"(
        QListWidget {
            background: transparent;
            border: none;
            outline: none;
        }
        QListWidget::item {
            padding: 12px 16px;
            border: none;
        }
        QListWidget::item:selected {
            background: #3a3a3a;
            color: #fff;
        }
        QListWidget::item:hover:!selected {
            background: #e6e6e6;
        }
    )");
    leftCol->addWidget(list_, 1);

    mainLayout->addLayout(leftCol);

    // Right: paper type selector (same as screenshot)
    auto* rightCol = new QVBoxLayout;
    rightCol->setSpacing(12);

    auto* title = new QLabel(QCoreApplication::translate("jcolour", "Workflow"));
    title->setStyleSheet("color: #fff; font-size: 16px; font-weight: bold;");
    rightCol->addWidget(title);

    paperCombo_ = new QComboBox;
    paperCombo_->setStyleSheet(R"(
        QComboBox {
            background: #e6e6e6;
            color: #fff;
            border: 1px solid #444;
            border-radius: 6px;
            padding: 12px 16px;
            font-size: 14px;
        }
        QComboBox:hover {
            border-color: #555;
        }
        QComboBox::drop-down {
            border: none;
            width: 30px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 6px solid #888;
            margin-right: 10px;
        }
        QComboBox QAbstractItemView {
            background: #e6e6e6;
            color: #fff;
            border: 1px solid #444;
            selection-background: #3a3a3a;
            padding: 8px;
        }
    )");

    // jcolour v1.0 has exactly 3 workflows
    paperCombo_->addItems({
        QCoreApplication::translate("jcolour", "B&W Multi-Channel"),
        QCoreApplication::translate("jcolour", "B&W Single Black"),
        QCoreApplication::translate("jcolour", "Color Photo")
    });

    rightCol->addWidget(paperCombo_);
    rightCol->addStretch();

    mainLayout->addLayout(rightCol, 1);

    // Load saved selection or default
    QSettings s;
    int savedIdx = s.value("workflowIndex", 0).toInt();
    if (savedIdx >= 3) savedIdx = 0;

    // Connections
    connect(list_, &QListWidget::currentRowChanged, this, &WorkflowManager::onWorkflowSelected);
    connect(paperCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WorkflowManager::onPaperTypeChanged);

    // Build list items
    list_->addItem(QCoreApplication::translate("jcolour", "B&W Multi-Channel"));
    list_->addItem(QCoreApplication::translate("jcolour", "B&W Single Black"));
    list_->addItem(QCoreApplication::translate("jcolour", "Color Photo"));

    // Select saved or first
    list_->setCurrentRow(savedIdx);
    paperCombo_->setCurrentIndex(savedIdx);
}

WorkflowManager::~WorkflowManager() = default;

void WorkflowManager::loadWorkflows() {
    QSettings s;
    workflows_.clear();
    // Always have 3 workflows for jcolour v1.0
    QStringList names = {"B&W Multi-Channel", "B&W Single Black", "Color Photo"};
    for (int i = 0; i < 3; ++i) {
        Workflow w;
        w.name = names[i];
        w.iccProfile = s.value(QString("workflow%1_icc").arg(i), "Epson P9500").toString();
        w.resolution = s.value(QString("workflow%1_dpi").arg(i), 1440).toInt();
        w.paperType = s.value(QString("workflow%1_paper").arg(i), "").toString();
        w.inkMode = s.value(QString("workflow%1_ink").arg(i), "").toString();
        w.isDefault = (i == 0);
        workflows_.append(w);
    }
}

void WorkflowManager::updateForm(int row) {
    if (row >= 0 && row < workflows_.size()) {
        currentIndex_ = row;
        emit workflowChanged(row);
    }
}

void WorkflowManager::onWorkflowSelected(int row) {
    updateForm(row);
    paperCombo_->blockSignals(true);
    paperCombo_->setCurrentIndex(row);
    paperCombo_->blockSignals(false);
}

void WorkflowManager::onPaperTypeChanged(int index) {
    QSettings s;
    s.setValue("workflowIndex", index);
    updateForm(index);
    list_->blockSignals(true);
    list_->setCurrentRow(index);
    list_->blockSignals(false);
}

QList<WorkflowManager::Workflow> WorkflowManager::getAllWorkflows() const {
    return workflows_;
}

QString WorkflowManager::defaultWorkflow() const {
    if (workflows_.isEmpty()) return "B&W Multi-Channel";
    return workflows_.first().name;
}

// InkCurvesPanel implementation
InkCurvesPanel::InkCurvesPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    channelTabs_ = new QTabWidget;
    channelTabs_->setDocumentMode(true);
    mainLayout->addWidget(channelTabs_, 1);

    // Create curve editors for each channel
    static const QStringList channelNamesCn = {
        "PK (照片黑)", "MK (哑光黑)", "LC (淡青)", "C (青)",
        "LM (淡品)", "M (品)", "Y (黄)", "Or (橙)",
        "LK (淡黑)", "LLK (LLK)", "V (紫)", "Gr (灰)"
    };
    for (int i = 0; i < NUM_INK_CHANNELS; ++i) {
        InkCurveEditor* editor = new InkCurveEditor;
        curveEditors_.append(editor);
        channelTabs_->addTab(editor, channelNamesCn[i]);
    }

    // Color the tabs
    static const QStringList COLORS = {
        "#ffffff", "#4a4a4a", "#00ccff", "#0088ff",
        "#ff00cc", "#ff0088", "#ffee00", "#ff8800",
        "#888888", "#aaaaaa", "#8800ff", "#888888"
    };
    QTabBar* bar = channelTabs_->tabBar();
    for (int i = 0; i < bar->count() && i < COLORS.size(); ++i) {
        bar->setTabTextColor(i, QColor(COLORS[i]));
    }

    // Controls
    auto* controlFrame = new QFrame;
    controlFrame->setFrameShape(QFrame::StyledPanel);
    controlFrame->setStyleSheet("QFrame { background: #1e1e1e; border-radius: 6px; }");
    auto* controlLayout = new QGridLayout(controlFrame);
    controlLayout->setContentsMargins(12, 8, 12, 8);
    controlLayout->setSpacing(12);

    auto* inputLbl = new QLabel(QCoreApplication::translate("jcolour", "Input"));
    inputLbl->setStyleSheet("color: #888; font-size: 13px;");
    inputSpin_ = new QSpinBox;
    inputSpin_->setRange(0, 255);
    inputSpin_->setValue(0);

    auto* outputLbl = new QLabel(QCoreApplication::translate("jcolour", "Output"));
    outputLbl->setStyleSheet("color: #888; font-size: 13px;");
    outputSpin_ = new QSpinBox;
    outputSpin_->setRange(0, 255);
    outputSpin_->setValue(0);

    auto* lightLbl = new QLabel(QCoreApplication::translate("jcolour", "Brightness"));
    lightLbl->setStyleSheet("color: #888; font-size: 13px;");
    lightnessSlider_ = new QSlider(Qt::Horizontal);
    lightnessSlider_->setRange(-100, 100);
    lightnessSlider_->setValue(0);

    controlLayout->addWidget(inputLbl, 0, 0);
    controlLayout->addWidget(inputSpin_, 0, 1);
    controlLayout->addWidget(outputLbl, 0, 2);
    controlLayout->addWidget(outputSpin_, 0, 3);
    controlLayout->addWidget(lightLbl, 1, 0);
    controlLayout->addWidget(lightnessSlider_, 1, 1, 1, 2);

    auto* btnLayout = new QHBoxLayout;
    QPushButton* resetBtn = new QPushButton(QCoreApplication::translate("jcolour", "Reset"));
    QPushButton* testBtn = new QPushButton(QCoreApplication::translate("jcolour", "Test Print"));
    btnLayout->addStretch();
    btnLayout->addWidget(resetBtn);
    btnLayout->addWidget(testBtn);
    controlLayout->addLayout(btnLayout, 2, 0, 1, 4);

    mainLayout->addWidget(controlFrame);

    // Connect reset button
    connect(resetBtn, &QPushButton::clicked, [this]() {
        InkCurveEditor* editor = qobject_cast<InkCurveEditor*>(channelTabs_->widget(channelTabs_->currentIndex()));
        if (editor) editor->setNeutralCurve();
    });

    connect(channelTabs_, &QTabWidget::currentChanged, this, &InkCurvesPanel::onChannelSelected);
}

InkCurvesPanel::~InkCurvesPanel() = default;

void InkCurvesPanel::onChannelSelected(int index) {
    Q_UNUSED(index);
}