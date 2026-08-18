#include "printer_manager.h"
#include "translations.h"

#include "pjl.h"  // jcolour::pjl::EpsonPrinter

#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSettings>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>
#include <QDir>
#include <QFile>

namespace jcolour {

namespace {

const QStringList kModels = {
    "Epson SureColor P9500",
    "Epson SureColor P7500",
    "Epson SureColor P900",
    "Epson SureColor P700",
};

const QStringList kInkModes = {
    "Photo Black",
    "Matte Black",
    "Multi-channel B&W",
};

QString inkLabelToKey(const QString& label) {
    if (label.startsWith("Matte"))   return "MatteBlack";
    if (label.startsWith("Multi"))   return "MultiChannel";
    return "PhotoBlack";
}
QString inkKeyToLabel(const QString& key) {
    if (key == "MatteBlack")   return "Matte Black";
    if (key == "MultiChannel") return "Multi-channel B&W";
    return "Photo Black";
}

} // anon

// 12 通道墨量条（PJL 双向状态；-1 = 未知通道不画）
class InkLevelsBar : public QWidget {
public:
    explicit InkLevelsBar(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(64);
        setVisible(false);
    }
    void setLevels(const int levels[12]) {
        for (int i = 0; i < 12; ++i) levels_[i] = levels[i];
        setVisible(true);
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        // P9500 12 通道惯用顺序与代表色
        static const struct { const char* n; const char* c; } ch[12] = {
            {"PK", "#1a1a1a"}, {"MK", "#3a3a3a"}, {"C", "#0090d0"}, {"M", "#d0308c"},
            {"Y", "#d8b800"}, {"LC", "#70c4e8"}, {"LM", "#e890c4"}, {"GY", "#8a8a8a"},
            {"LGY", "#b8b8b8"}, {"V", "#7a4fa0"}, {"OR", "#e07820"}, {"GR", "#3e9e5b"},
        };
        QPainter g(this);
        const double bw = width() / 12.0;
        QFont f = g.font(); f.setPointSizeF(8); g.setFont(f);
        for (int i = 0; i < 12; ++i) {
            const QRectF cell(i * bw + 2, 0, bw - 4, height() - 14.0);
            g.setPen(QPen(QColor("#DDD9CF"), 1));
            g.setBrush(QColor("#F1EFE8"));
            g.drawRect(cell);
            if (levels_[i] >= 0) {
                const double h = cell.height() * qBound(0, levels_[i], 100) / 100.0;
                g.setPen(Qt::NoPen);
                g.setBrush(QColor(ch[i].c));
                g.drawRect(QRectF(cell.x(), cell.bottom() - h, cell.width(), h));
                if (levels_[i] <= 10) {                 // 低墨警示
                    g.setPen(QPen(QColor("#E71655"), 1.5));
                    g.setBrush(Qt::NoBrush);
                    g.drawRect(cell);
                }
            }
            g.setPen(QColor("#7A776D"));
            g.drawText(QRectF(i * bw, height() - 13.0, bw, 12), Qt::AlignCenter,
                       QString::fromLatin1(ch[i].n));
        }
    }
private:
    int levels_[12] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
};

PrinterManagerWidget::PrinterManagerWidget(QWidget* parent) : QWidget(parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(12);

    // ---- Left: list + buttons ---------------------------------------
    auto* leftCol = new QVBoxLayout;
    leftCol->setSpacing(6);
    list_ = new QListWidget;
    list_->setMinimumWidth(220);
    leftCol->addWidget(list_, 1);

    auto* btnRow = new QHBoxLayout;
    addBtn_     = new QPushButton("+");
    removeBtn_  = new QPushButton(QString::fromUtf8("\xE2\x88\x92")); // −
    discoverBtn_ = new QPushButton(TR("Discover"));
    addBtn_->setFixedWidth(28);
    removeBtn_->setFixedWidth(28);
    btnRow->addWidget(addBtn_);
    btnRow->addWidget(removeBtn_);
    btnRow->addWidget(discoverBtn_);
    btnRow->addStretch();
    leftCol->addLayout(btnRow);
    root->addLayout(leftCol);

    // ---- Right: settings form --------------------------------------
    auto* formCol = new QVBoxLayout;
    formCol->setSpacing(8);

    auto* form = new QFormLayout;
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(6);

    nameEdit_   = new QLineEdit;
    modelCombo_ = new QComboBox; modelCombo_->addItems(kModels);
    serialEdit_ = new QLineEdit; serialEdit_->setPlaceholderText("X9PK004821");
    // 端口类型（ErgoSoft 四种）：RAW 直连 / LPR / USB / 打印到文件
    connCombo_  = new QComboBox;
    connCombo_->addItems({QStringLiteral("TCP/IP (RAW 9100)"), QStringLiteral("LPR"),
                          QStringLiteral("USB"), QStringLiteral("打印到文件")});
    hostEdit_   = new QLineEdit; hostEdit_->setPlaceholderText("192.168.0.100");
    portSpin_   = new QSpinBox;  portSpin_->setRange(1, 65535); portSpin_->setValue(9100);
    queueEdit_  = new QLineEdit; queueEdit_->setPlaceholderText(QStringLiteral("PASSTHRU（LPR 队列名）"));
    dirEdit_    = new QLineEdit; dirEdit_->setPlaceholderText(QStringLiteral("~/Prints（输出目录）"));
    inkCombo_   = new QComboBox; inkCombo_->addItems(kInkModes);
    dpiCombo_   = new QComboBox; dpiCombo_->addItems({"720", "1440", "2880"});
    defaultChk_ = new QCheckBox(TR("Set as default printer"));

    form->addRow(TR("Name"),                nameEdit_);
    form->addRow(TR("Model"),               modelCombo_);
    form->addRow(TR("Serial No."),          serialEdit_);
    form->addRow(TR("Connection"),          connCombo_);
    form->addRow(TR("Host / IP"),           hostEdit_);
    form->addRow(TR("Port"),                portSpin_);
    form->addRow(QStringLiteral("LPR 队列"), queueEdit_);
    form->addRow(QStringLiteral("输出目录"),  dirEdit_);
    form->addRow(TR("Default ink mode"),    inkCombo_);
    form->addRow(TR("Resolution (dpi)"),    dpiCombo_);
    form->addRow(QString(),                 defaultChk_);
    formCol->addLayout(form);

    auto* actionRow = new QHBoxLayout;
    testBtn_   = new QPushButton(TR("Test connection"));
    statusBtn_ = new QPushButton(QStringLiteral("查询状态"));
    statusBtn_->setToolTip(QStringLiteral("PJL 双向查询：就绪/介质/12 通道墨量（仅 RAW 直连）"));
    actionRow->addWidget(testBtn_);
    actionRow->addWidget(statusBtn_);
    actionRow->addStretch();
    formCol->addLayout(actionRow);

    statusLbl_ = new QLabel(TR("No printer selected."));
    statusLbl_->setStyleSheet("color:#666; padding:4px 0;");
    statusLbl_->setWordWrap(true);
    formCol->addWidget(statusLbl_);

    inkBar_ = new InkLevelsBar;
    formCol->addWidget(inkBar_);

    formCol->addStretch();
    root->addLayout(formCol, 1);

    // ---- Wiring -----------------------------------------------------
    connect(list_, &QListWidget::currentRowChanged, this, [this](int) {
        onListSelectionChanged();
    });
    connect(addBtn_,      &QPushButton::clicked, this, &PrinterManagerWidget::onAdd);
    connect(removeBtn_,   &QPushButton::clicked, this, &PrinterManagerWidget::onRemove);
    connect(discoverBtn_, &QPushButton::clicked, this, &PrinterManagerWidget::onDiscover);
    connect(testBtn_,     &QPushButton::clicked, this, &PrinterManagerWidget::onTestConnection);
    connect(statusBtn_,   &QPushButton::clicked, this, &PrinterManagerWidget::onQueryStatus);
    // 端口类型切换 → 行显隐 + LPR/RAW 默认端口
    connect(connCombo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (!blockFormSignals_) {
            if (idx == 0 && portSpin_->value() == 515)  portSpin_->setValue(9100);
            if (idx == 1 && portSpin_->value() == 9100) portSpin_->setValue(515);
        }
        updateConnRows();
    });
    connect(defaultChk_,  &QCheckBox::toggled,   this, [this](bool on) {
        if (blockFormSignals_) return;
        if (currentIdx_ < 0) return;
        if (on) {
            for (auto& p : printers_) p.isDefault = false;
            printers_[currentIdx_].isDefault = true;
            saveToSettings();
            rebuildList();
            list_->setCurrentRow(currentIdx_);
            emit defaultPrinterChanged(printers_[currentIdx_].name);
        } else {
            printers_[currentIdx_].isDefault = false;
            saveToSettings();
            rebuildList();
            list_->setCurrentRow(currentIdx_);
        }
    });

    // Form-edit autosave
    auto autosave = [this] {
        if (blockFormSignals_) return;
        if (currentIdx_ < 0) return;
        readFormIntoCurrent();
        saveToSettings();
        // Refresh list label without losing selection
        const int keep = currentIdx_;
        rebuildList();
        list_->setCurrentRow(keep);
    };
    connect(nameEdit_,   &QLineEdit::editingFinished,  this, autosave);
    connect(serialEdit_, &QLineEdit::editingFinished,  this, autosave);
    connect(hostEdit_,  &QLineEdit::editingFinished,  this, autosave);
    connect(portSpin_,  qOverload<int>(&QSpinBox::valueChanged),    this, autosave);
    connect(modelCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, autosave);
    connect(connCombo_,  qOverload<int>(&QComboBox::currentIndexChanged), this, [this, autosave](int idx) {
        hostEdit_->setEnabled(idx == 0); // TCP/IP only
        portSpin_->setEnabled(idx == 0);
        autosave();
    });
    connect(inkCombo_,   qOverload<int>(&QComboBox::currentIndexChanged), this, autosave);
    connect(dpiCombo_,   qOverload<int>(&QComboBox::currentIndexChanged), this, autosave);

    loadFromSettings();
    if (printers_.isEmpty()) {
        // Seed with one default entry so the form isn't empty on first run.
        PrinterEntry seed;
        seed.name = "Epson P9500 (Studio)";
        seed.model = kModels.first();
        seed.connection = "tcp";
        seed.host = "192.168.0.100";
        seed.defaultInkMode = "PhotoBlack";
        seed.defaultDpi = 1440;
        seed.isDefault = true;
        printers_.append(seed);
        saveToSettings();
    }
    rebuildList();
    if (!printers_.isEmpty()) list_->setCurrentRow(0);
}

QString PrinterManagerWidget::defaultPrinterName() const {
    for (const auto& p : printers_) if (p.isDefault) return p.name;
    return printers_.isEmpty() ? QString() : printers_.first().name;
}

void PrinterManagerWidget::loadFromSettings() {
    QSettings s;
    int n = s.beginReadArray("printers");
    printers_.clear();
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        PrinterEntry p;
        p.name           = s.value("name").toString();
        p.model          = s.value("model").toString();
        p.serial         = s.value("serial").toString();
        p.connection     = s.value("connection", "tcp").toString();
        p.host           = s.value("host").toString();
        p.port           = s.value("port", 9100).toInt();
        p.lprQueue       = s.value("lprQueue").toString();
        p.outputDir      = s.value("outputDir").toString();
        p.defaultInkMode = s.value("inkMode", "PhotoBlack").toString();
        p.defaultDpi     = s.value("dpi", 1440).toInt();
        p.isDefault      = s.value("isDefault", false).toBool();
        printers_.append(p);
    }
    s.endArray();
}

void PrinterManagerWidget::saveToSettings() {
    QSettings s;
    s.beginWriteArray("printers");
    for (int i = 0; i < printers_.size(); ++i) {
        s.setArrayIndex(i);
        const auto& p = printers_[i];
        s.setValue("name",       p.name);
        s.setValue("model",      p.model);
        s.setValue("serial",     p.serial);
        s.setValue("connection", p.connection);
        s.setValue("host",       p.host);
        s.setValue("port",       p.port);
        s.setValue("lprQueue",   p.lprQueue);
        s.setValue("outputDir",  p.outputDir);
        s.setValue("inkMode",    p.defaultInkMode);
        s.setValue("dpi",        p.defaultDpi);
        s.setValue("isDefault",  p.isDefault);
    }
    s.endArray();
    emit printersChanged();
}

void PrinterManagerWidget::rebuildList() {
    blockFormSignals_ = true;
    list_->clear();
    for (const auto& p : printers_) {
        QString label = p.isDefault ? (QString::fromUtf8("\xE2\x98\x85 ") + p.name) : p.name;
        if (!p.serial.isEmpty()) label += "  ·  SN " + p.serial;  // 同型號靠序列號分辨
        list_->addItem(label);
    }
    blockFormSignals_ = false;
}

void PrinterManagerWidget::onListSelectionChanged() {
    currentIdx_ = list_->currentRow();
    writeCurrentIntoForm();
}

void PrinterManagerWidget::writeCurrentIntoForm() {
    blockFormSignals_ = true;
    if (currentIdx_ < 0 || currentIdx_ >= printers_.size()) {
        nameEdit_->clear();
        modelCombo_->setCurrentIndex(0);
        serialEdit_->clear();
        connCombo_->setCurrentIndex(0);
        hostEdit_->clear();
        portSpin_->setValue(9100);
        inkCombo_->setCurrentIndex(0);
        dpiCombo_->setCurrentIndex(1);
        defaultChk_->setChecked(false);
        statusLbl_->setText(TR("No printer selected."));
        blockFormSignals_ = false;
        return;
    }
    const auto& p = printers_[currentIdx_];
    nameEdit_->setText(p.name);
    int mi = kModels.indexOf(p.model);
    modelCombo_->setCurrentIndex(mi >= 0 ? mi : 0);
    serialEdit_->setText(p.serial);
    connCombo_->setCurrentIndex(p.connection == "lpr" ? 1
                              : p.connection == "usb" ? 2
                              : p.connection == "file" ? 3 : 0);
    hostEdit_->setText(p.host);
    portSpin_->setValue(p.port);
    queueEdit_->setText(p.lprQueue);
    dirEdit_->setText(p.outputDir);
    inkCombo_->setCurrentText(inkKeyToLabel(p.defaultInkMode));
    dpiCombo_->setCurrentText(QString::number(p.defaultDpi));
    defaultChk_->setChecked(p.isDefault);
    updateConnRows();
    statusLbl_->setText(p.serial.isEmpty()
        ? QString("%1 · %2").arg(p.model, p.connection.toUpper())
        : QString("%1 · SN %2 · %3").arg(p.model, p.serial, p.connection.toUpper()));
    inkBar_->setVisible(false);
    blockFormSignals_ = false;
}

void PrinterManagerWidget::updateConnRows() {
    const int idx = connCombo_->currentIndex();      // 0 raw · 1 lpr · 2 usb · 3 file
    const bool net = idx == 0 || idx == 1;
    hostEdit_->setEnabled(net);
    portSpin_->setEnabled(net);
    queueEdit_->setEnabled(idx == 1);
    dirEdit_->setEnabled(idx == 3);
}

void PrinterManagerWidget::readFormIntoCurrent() {
    if (currentIdx_ < 0 || currentIdx_ >= printers_.size()) return;
    auto& p = printers_[currentIdx_];
    p.name           = nameEdit_->text().trimmed();
    if (p.name.isEmpty()) p.name = "Untitled printer";
    p.model          = modelCombo_->currentText();
    p.serial         = serialEdit_->text().trimmed();
    p.connection     = connCombo_->currentIndex() == 1 ? "lpr"
                     : connCombo_->currentIndex() == 2 ? "usb"
                     : connCombo_->currentIndex() == 3 ? "file" : "tcp";
    p.host           = hostEdit_->text().trimmed();
    p.port           = portSpin_->value();
    p.lprQueue       = queueEdit_->text().trimmed();
    p.outputDir      = dirEdit_->text().trimmed();
    p.defaultInkMode = inkLabelToKey(inkCombo_->currentText());
    p.defaultDpi     = dpiCombo_->currentText().toInt();
}

void PrinterManagerWidget::onAdd() {
    PrinterEntry p;
    p.name = "New printer";
    p.model = kModels.first();
    p.connection = "tcp";
    p.port = 9100;
    p.defaultInkMode = "PhotoBlack";
    p.defaultDpi = 1440;
    p.isDefault = printers_.isEmpty();
    printers_.append(p);
    saveToSettings();
    rebuildList();
    list_->setCurrentRow(printers_.size() - 1);
}

void PrinterManagerWidget::onRemove() {
    if (currentIdx_ < 0 || currentIdx_ >= printers_.size()) return;
    const QString name = printers_[currentIdx_].name;
    if (QMessageBox::question(this, TR("Remove Printer"),
        QString("Remove printer '%1' ?").arg(name)) != QMessageBox::Yes) return;
    const bool wasDefault = printers_[currentIdx_].isDefault;
    printers_.remove(currentIdx_);
    if (wasDefault && !printers_.isEmpty()) printers_[0].isDefault = true;
    currentIdx_ = -1;
    saveToSettings();
    rebuildList();
    if (!printers_.isEmpty()) list_->setCurrentRow(0);
    else writeCurrentIntoForm();
}

void PrinterManagerWidget::onDiscover() {
    statusLbl_->setText(TR("Scanning network…"));
    statusLbl_->repaint();
    auto found = jcolour::pjl::EpsonPrinter::discover_printers();
    if (found.empty()) {
        statusLbl_->setText(TR("No printers discovered. Add manually with +."));
        return;
    }
    int added = 0;
    for (const auto& uri : found) {
        const QString u = QString::fromStdString(uri);
        // Avoid duplicates by host string match
        bool dup = false;
        for (const auto& p : printers_) if (u.contains(p.host) && !p.host.isEmpty()) { dup = true; break; }
        if (dup) continue;
        PrinterEntry p;
        p.name = u;
        p.model = kModels.first();
        p.connection = u.startsWith("usb") ? "usb" : "tcp";
        if (p.connection == "tcp") {
            // crude host extraction
            QString host = u;
            host.remove("tcp://").remove("socket://").remove("ipp://").remove("http://");
            int colon = host.indexOf(':');
            int slash = host.indexOf('/');
            if (slash > 0) host = host.left(slash);
            if (colon > 0) {
                p.port = host.mid(colon + 1).toInt();
                if (p.port == 0) p.port = 9100;
                host = host.left(colon);
            }
            p.host = host;
        }
        p.defaultInkMode = "PhotoBlack";
        p.defaultDpi = 1440;
        p.isDefault = printers_.isEmpty();
        printers_.append(p);
        ++added;
    }
    saveToSettings();
    rebuildList();
    statusLbl_->setText(QString(TR("Discovered %1 new printer(s).")).arg(added));
}

void PrinterManagerWidget::onTestConnection() {
    if (currentIdx_ < 0) return;
    readFormIntoCurrent();
    const auto& p = printers_[currentIdx_];

    // 打印到文件：测试 = 目录存在且可写
    if (p.connection == "file") {
        const QString dir = p.outputDir.isEmpty()
            ? QString() : QDir(p.outputDir).absolutePath();
        if (dir.isEmpty() || !QDir(dir).exists()) {
            statusLbl_->setText(QStringLiteral("输出目录不存在：%1").arg(p.outputDir));
            return;
        }
        QFile probe(dir + "/.jcolour-write-test");
        const bool writable = probe.open(QIODevice::WriteOnly);
        probe.remove();
        statusLbl_->setText(writable
            ? QStringLiteral("输出目录可写：%1").arg(dir)
            : QStringLiteral("输出目录不可写：%1").arg(dir));
        return;
    }

    QString uri;
    if (p.connection == "tcp" || p.connection == "lpr") {
        if (p.host.isEmpty()) {
            statusLbl_->setText(TR("Set a host/IP before testing."));
            return;
        }
        uri = QString("socket://%1:%2").arg(p.host).arg(p.port);
    } else {
        uri = "usb://" + p.model;
    }
    statusLbl_->setText(TR("Connecting…"));
    statusLbl_->repaint();
    jcolour::pjl::EpsonPrinter printer(uri.toStdString());
    bool ok = printer.connect();
    if (!ok) {
        statusLbl_->setText(QString(TR("Connection failed: %1")).arg(uri));
        return;
    }
    // LPR：仅验证端口可达（515 是 lpd 控制通道，不做 PJL 对话）
    if (p.connection == "lpr") {
        printer.disconnect();
        statusLbl_->setText(QStringLiteral("LPR 端口可达：%1:%2 · 队列 %3（发送经系统 lpd）")
            .arg(p.host).arg(p.port)
            .arg(p.lprQueue.isEmpty() ? QStringLiteral("默认") : p.lprQueue));
        return;
    }
    auto st = printer.query_status();
    printer.disconnect();
    QString msg = QString(TR("Connected to %1.")).arg(uri);
    if (st.is_ready) msg += " Ready.";
    if (!st.media_type.empty())
        msg += QString(" Media: %1.").arg(QString::fromStdString(st.media_type));
    statusLbl_->setText(msg);
}

void PrinterManagerWidget::onQueryStatus() {
    // 双向状态（ErgoSoft PrintClient 的状态面板）：就绪/错误/介质/12 通道墨量。
    // 仅 RAW 直连支持 PJL 查询；LPR/USB/文件按端口能力如实提示。
    if (currentIdx_ < 0) return;
    readFormIntoCurrent();
    const auto& p = printers_[currentIdx_];
    if (p.connection != "tcp") {
        statusLbl_->setText(QStringLiteral("状态查询需 RAW 直连（PJL 双向）；当前端口类型为 %1。")
                                .arg(connCombo_->currentText()));
        return;
    }
    if (p.host.isEmpty()) { statusLbl_->setText(TR("Set a host/IP before testing.")); return; }
    statusLbl_->setText(QStringLiteral("查询状态…"));
    statusLbl_->repaint();
    jcolour::pjl::EpsonPrinter printer(
        QString("socket://%1:%2").arg(p.host).arg(p.port).toStdString());
    if (!printer.connect()) {
        statusLbl_->setText(QStringLiteral("无法连接 %1:%2").arg(p.host).arg(p.port));
        return;
    }
    auto st = printer.query_status();
    printer.disconnect();
    QString msg = st.is_ready ? QStringLiteral("● 就绪") : QStringLiteral("● 未就绪");
    if (!st.error_message.empty())
        msg += QStringLiteral(" · 错误：%1").arg(QString::fromStdString(st.error_message));
    if (!st.media_type.empty())
        msg += QStringLiteral(" · 介质 %1").arg(QString::fromStdString(st.media_type));
    if (st.media_width > 0)
        msg += QStringLiteral(" · 幅宽 %1mm").arg(st.media_width);
    statusLbl_->setText(msg);
    inkBar_->setLevels(st.ink_levels);
}

void PrinterManagerWidget::onSetDefault() {
    if (currentIdx_ < 0) return;
    for (auto& p : printers_) p.isDefault = false;
    printers_[currentIdx_].isDefault = true;
    saveToSettings();
    rebuildList();
    list_->setCurrentRow(currentIdx_);
    emit defaultPrinterChanged(printers_[currentIdx_].name);
}

} // namespace jcolour
