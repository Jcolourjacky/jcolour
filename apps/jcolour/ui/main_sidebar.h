#pragma once

#include <QWidget>
#include <QButtonGroup>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QVector>

namespace jcolour {

class MainSidebar : public QWidget {
    Q_OBJECT
public:
    enum Page {
        PageJobs = 0,
        PageHistory,
        PageSettings,            // 設定（內含印表機設定 / 儀表板 / 墨水管理 / 量測設備）
        PageColorManagement,     // 印表機色彩管理
        PageCount
    };

    explicit MainSidebar(QWidget* parent = nullptr);

    void setActive(Page page);
    void setVersionText(const QString& text);

signals:
    void pageRequested(int page);

private:
    QToolButton* makeNavButton(const QString& text, const QString& glyph, Page page);
    QWidget*     makeLogoHeader();

    QButtonGroup*           group_   = nullptr;
    QVector<QPushButton*>   buttons_;
    QLabel*                 version_ = nullptr;
};

}  // namespace jcolour
