#pragma once

#include <QWidget>
#include <QLabel>

namespace jcolour {

// Small "•Connected" badge for the toolbar.
class ConnectionBadge : public QWidget {
    Q_OBJECT
public:
    enum State { Connected, Disconnected, Warning };
    explicit ConnectionBadge(const QString& label, QWidget* parent = nullptr);
    void setState(State s);
    void setSubtitle(const QString& s);

private:
    void refresh();
    QLabel* dot_;
    QLabel* labelLeft_;
    QLabel* subtitle_;
    State   state_ = Disconnected;
    QString labelText_;
};

// 12-channel ink level mini-bar group, drawn manually.
class InkLevelStrip : public QWidget {
    Q_OBJECT
public:
    explicit InkLevelStrip(QWidget* parent = nullptr);
    // levels: 0..1 per channel; expects 12 entries
    void setLevels(const QVector<double>& levels);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> levels_;
};

} // namespace jcolour
