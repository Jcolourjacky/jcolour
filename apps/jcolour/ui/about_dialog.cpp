#include "about_dialog.h"

#include "../sbom.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>

namespace jcolour::ui {

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("关于 jcolour"));
    resize(820, 560);
    auto* v = new QVBoxLayout(this);

    sbom::BuildInfo info;
    info.appVersion = QApplication::applicationVersion();
    const auto comps = sbom::collect(true);

    auto* tabs = new QTabWidget;
    v->addWidget(tabs, 1);

    // 关于
    {
        auto* w = new QWidget;
        auto* l = new QVBoxLayout(w);
        auto* logo = new QLabel;
        const QPixmap px(QStringLiteral(":/logo.png"));
        if (!px.isNull()) logo->setPixmap(px.scaledToHeight(64, Qt::SmoothTransformation));
        l->addWidget(logo);
        auto* t = new QLabel(QStringLiteral(
            "<h2 style='margin:0'>jcolour %1</h2>"
            "<p>摄影师自打印工作站 · Epson SureColor P 系列 · macOS</p>"
            "<p>建档前先做每通道墨限与线性化、总墨量 TIL（SmartProfiler 式）；"
            "色域外映射可选视觉保真 / 色度最近；换纸 / 换墨批次后可只做再校准。</p>"
            "<p style='color:#7A776D'>Qt %2 · Little CMS %3 · 第三方组件见「软件物料清单」与「第三方声明」。</p>")
            .arg(info.appVersion, QString::fromLatin1(qVersion()),
                 comps.size() > 1 ? comps[1].version : QString()));
        t->setWordWrap(true);
        t->setTextFormat(Qt::RichText);
        l->addWidget(t);
        l->addStretch();
        tabs->addTab(w, QStringLiteral("关于"));
    }

    // SBOM
    {
        auto* w = new QWidget;
        auto* l = new QVBoxLayout(w);
        auto* hint = new QLabel(QStringLiteral(
            "软件物料清单（SBOM）：版本为运行时实际加载的库所报告；可导出 CycloneDX 1.5 JSON 供企业合规审查。"));
        hint->setWordWrap(true);
        hint->setStyleSheet("color:#7A776D;");
        l->addWidget(hint);
        auto* table = new QTableWidget(comps.size(), 5);
        table->setHorizontalHeaderLabels({QStringLiteral("组件"), QStringLiteral("版本"), QStringLiteral("许可证"),
                                          QStringLiteral("使用方式"), QStringLiteral("用途")});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->horizontalHeader()->setStretchLastSection(true);
        for (int i = 0; i < comps.size(); ++i) {
            const auto& c = comps[i];
            const QString ver = !c.version.isEmpty() ? c.version
                              : c.scope == QStringLiteral("excluded") ? QStringLiteral("随系统")
                                                                       : QStringLiteral("未安装");
            const QStringList cells{c.name, ver, c.license, c.linkage, c.purpose};
            for (int k = 0; k < cells.size(); ++k) {
                auto* it = new QTableWidgetItem(cells[k]);
                if (!c.note.isEmpty()) it->setToolTip(c.note);
                table->setItem(i, k, it);
            }
        }
        table->resizeColumnsToContents();
        l->addWidget(table, 1);
        auto* row = new QHBoxLayout;
        row->addStretch();
        auto* exp = new QPushButton(QStringLiteral("导出 SBOM（CycloneDX JSON）…"));
        row->addWidget(exp);
        l->addLayout(row);
        connect(exp, &QPushButton::clicked, this, [this, comps, info] {
            const QString path = QFileDialog::getSaveFileName(
                this, QStringLiteral("导出 SBOM"), QStringLiteral("jcolour-%1.cdx.json").arg(info.appVersion),
                QStringLiteral("CycloneDX JSON (*.json)"));
            if (path.isEmpty()) return;
            QFile f(path);
            if (f.open(QIODevice::WriteOnly))
                f.write(sbom::toCycloneDx(comps, info, QDateTime::currentDateTimeUtc(),
                                          QUuid::createUuid().toString(QUuid::WithoutBraces)));
        });
        tabs->addTab(w, QStringLiteral("软件物料清单"));
    }

    // 第三方声明
    {
        auto* w = new QWidget;
        auto* l = new QVBoxLayout(w);
        auto* text = new QPlainTextEdit(sbom::thirdPartyNotices(comps, info));
        text->setReadOnly(true);
        l->addWidget(text, 1);
        auto* row = new QHBoxLayout;
        row->addStretch();
        auto* exp = new QPushButton(QStringLiteral("导出第三方声明…"));
        row->addWidget(exp);
        l->addLayout(row);
        connect(exp, &QPushButton::clicked, this, [this, text] {
            const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出第三方声明"),
                                                              QStringLiteral("THIRD_PARTY_NOTICES.txt"));
            if (path.isEmpty()) return;
            QFile f(path);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) f.write(text->toPlainText().toUtf8());
        });
        tabs->addTab(w, QStringLiteral("第三方声明"));
    }

    auto* close = new QPushButton(QStringLiteral("关闭"));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    auto* row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(close);
    v->addLayout(row);
}

}  // namespace jcolour::ui
