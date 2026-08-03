#pragma once

#include <QObject>
#include <QMap>
#include <QString>

namespace jcolour {

class Translator : public QObject {
    Q_OBJECT
public:
    static Translator& instance();

    enum class Language { English, Chinese };
    Q_ENUM(Language)

    void setLanguage(Language lang);
    Language language() const { return currentLang_; }

    QString translate(const char* key) const;

signals:
    void languageChanged();

private:
    Translator(QObject* parent = nullptr);
    ~Translator() = default;
    Translator(const Translator&) = delete;
    Translator& operator=(const Translator&) = delete;

    Language currentLang_ = Language::English;
    QMap<QString, QStringList> translations_;
    void initTranslations();
};

#define TR(key) jcolour::Translator::instance().translate(key)

} // namespace jcolour