#pragma once

#include <QDialog>

class QComboBox;
class QPushButton;

namespace jcolour {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override;

private:
    QComboBox* languageCombo_;
};

} // namespace jcolour