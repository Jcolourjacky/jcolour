#pragma once

#include <QWidget>
#include <QString>
#include <QDialog>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QListWidget>
#include <QLabel>
#include <QGroupBox>
#include <QProcess>

namespace jcolour {

class ProfileDropZone : public QWidget {
    Q_OBJECT
public:
    explicit ProfileDropZone(QWidget* parent = nullptr);
    ~ProfileDropZone() override;

    void setAcceptedExtensions(const QStringList& extensions);

signals:
    void fileDropped(const QString& path);
    void filesDropped(const QStringList& paths);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QStringList acceptedExtensions_;
    bool isDragOver_ = false;
    void paintEvent(QPaintEvent* event) override;
};

class ProfileImportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProfileImportDialog(QWidget* parent = nullptr);
    ~ProfileImportDialog() override;

    void setImportedPath(const QString& path);
    QString importedPath() const { return importedPath_; }

    struct ValidationResult {
        bool isValid = false;
        QString profileName;
        QString colorSpace;
        QString mediaType;
        QString manufacturer;
        QString errorMessage;
        QString warningMessage;
    };

    ValidationResult getValidationResult() const { return validation_; }

private slots:
    void onValidate();
    void onImport();
    void onCancel();

private:
    QString importedPath_;
    ValidationResult validation_;

    QLabel* previewLabel_;
    QLabel* infoLabel_;
    QPushButton* validateBtn_;
    QPushButton* importBtn_;
};

class ProfileLibraryBrowser : public QWidget {
    Q_OBJECT
public:
    explicit ProfileLibraryBrowser(QWidget* parent = nullptr);
    ~ProfileLibraryBrowser() override;

    void setDisplayMode(bool showBuiltIn, bool showThirdParty, bool showCustom);
    void refresh();

    QString selectedProfile() const { return selectedProfile_; }

signals:
    void profileSelected(const QString& path);
    void profileImported(const QString& path);
    void profileDeleted(const QString& path);

private slots:
    void onAddThirdParty();
    void onImportDroppedFiles();
    void onDeleteSelected();
    void onShowInFinder();

private:
    void loadProfiles();
    void updateProfileList();

    QStringList builtInProfiles_;
    QStringList thirdPartyProfiles_;
    QStringList customProfiles_;
    QString selectedProfile_;

    QListWidget* profileList_;
    QLabel* detailLabel_;
    bool showBuiltIn_ = true;
    bool showThirdParty_ = true;
    bool showCustom_ = true;
};

} // namespace jcolour