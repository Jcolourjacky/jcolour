#include "settings_dialog.h"
#include "translations.h"
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

namespace jcolour {

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(TR("Settings"));
    setMinimumSize(360, 180);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* lay = new QVBoxLayout(this);
    lay->setSpacing(12);

    // Language row
    auto* langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(TR("Language") + ":"));
    languageCombo_ = new QComboBox;
    languageCombo_->addItems({TR("English"), TR("简体中文")});
    languageCombo_->setCurrentIndex(
        Translator::instance().language() == Translator::Language::English ? 0 : 1);
    langRow->addWidget(languageCombo_);
    langRow->addStretch();
    lay->addLayout(langRow);

    lay->addSpacing(8);

    // Buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* okBtn = new QPushButton(TR("OK"));
    auto* cancelBtn = new QPushButton(TR("Cancel"));
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lay->addLayout(btnRow);

    connect(okBtn, &QPushButton::clicked, this, [this] {
        auto lang = languageCombo_->currentIndex() == 0
                        ? Translator::Language::English
                        : Translator::Language::Chinese;
        Translator::instance().setLanguage(lang);
        accept();
    });
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

SettingsDialog::~SettingsDialog() = default;

} // namespace jcolour