#include "profile_drag_drop.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QMimeData>
#include <QPainter>
#include <QStyle>

namespace jcolour {

// ============================================
// ProfileDropZone Implementation
// ============================================

ProfileDropZone::ProfileDropZone(QWidget* parent)
    : QWidget(parent)
    , isDragOver_(false)
{
    setAcceptDrops(true);
    setMinimumSize(200, 150);

    acceptedExtensions_ = {"icc", "iccg"};

    setStyleSheet("ProfileDropZone { "
                  "background-color: #e6e6e6; "
                  "border: 2px dashed #555; "
                  "border-radius: 8px; "
                  "} "
                  "ProfileDropZone[dragOver=true] { "
                  "background-color: #3a3a3a; "
                  "border-color: #0078d4; "
                  "}");
}

ProfileDropZone::~ProfileDropZone() = default;

void ProfileDropZone::setAcceptedExtensions(const QStringList& extensions) {
    acceptedExtensions_ = extensions;
}

void ProfileDropZone::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        QUrl url = event->mimeData()->urls().first();
        QString ext = QFileInfo(url.toLocalFile()).suffix().toLower();
        if (acceptedExtensions_.contains(ext)) {
            event->acceptProposedAction();
            isDragOver_ = true;
            setProperty("dragOver", true);
            update();
            return;
        }
    }
    event->ignore();
}

void ProfileDropZone::dragLeaveEvent(QDragLeaveEvent* event) {
    Q_UNUSED(event);
    isDragOver_ = false;
    setProperty("dragOver", false);
    update();
}

void ProfileDropZone::dropEvent(QDropEvent* event) {
    isDragOver_ = false;
    setProperty("dragOver", false);
    update();

    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        QString ext = QFileInfo(path).suffix().toLower();
        if (acceptedExtensions_.contains(ext)) {
            paths.append(path);
        }
    }

    if (paths.size() == 1) {
        emit fileDropped(paths.first());
    } else if (paths.size() > 1) {
        emit filesDropped(paths);
    }

    event->acceptProposedAction();
}

void ProfileDropZone::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), isDragOver_ ? QColor(58, 58, 58) : QColor(42, 42, 42));

    QPen pen(isDragOver_ ? QColor(0, 120, 212) : QColor(85, 85, 85));
    pen.setWidth(2);
    painter.setPen(pen);
    painter.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 8, 8);

    // Draw icon and text
    QIcon icon = style()->standardIcon(QStyle::SP_DirIcon);
    QSize iconSize(48, 48);
    painter.drawPixmap((width() - iconSize.width()) / 2, (height() - iconSize.height()) / 2 - 20,
                       icon.pixmap(iconSize));

    QString text = isDragOver_
        ? "Release to import"
        : "Drop ICC profile here\nor click Browse";
    painter.setPen(QColor(150, 150, 150));
    painter.drawText(rect().adjusted(0, height() / 2 + 30, 0, 0),
                     Qt::AlignHCenter | Qt::AlignTop, text);
}

// ============================================
// ProfileImportDialog Implementation
// ============================================

ProfileImportDialog::ProfileImportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Import ICC Profile");
    setMinimumSize(450, 350);
    setStyleSheet("QDialog { background-color: #e6e6e6; } "
                  "QLabel { color: #ccc; } "
                  "QPushButton { padding: 6px 16px; }");

    auto* layout = new QVBoxLayout(this);

    // Drop zone
    auto* dropZone = new ProfileDropZone;
    dropZone->setMinimumHeight(120);
    layout->addWidget(dropZone);

    // Info display
    auto* infoGroup = new QGroupBox("Profile Information");
    auto* infoLayout = new QVBoxLayout(infoGroup);

    previewLabel_ = new QLabel("No profile selected");
    previewLabel_->setStyleSheet("background-color: #ffffff; padding: 12px; border-radius: 4px;");
    previewLabel_->setAlignment(Qt::AlignCenter);
    infoLayout->addWidget(previewLabel_);

    infoLabel_ = new QLabel("Color Space: --\nMedia Type: --\nManufacturer: --");
    infoLabel_->setStyleSheet("color: #999; font-size: 12px;");
    infoLayout->addWidget(infoLabel_);

    layout->addWidget(infoGroup);

    // Buttons
    auto* buttonLayout = new QHBoxLayout;

    auto* browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, this, [this, dropZone]() {
        QString path = QFileDialog::getOpenFileName(this, "Select ICC Profile",
            QString(), "ICC Files (*.icc *.iccg);;All Files (*)");
        if (!path.isEmpty()) {
            importedPath_ = path;
            onValidate();
        }
    });
    buttonLayout->addWidget(browseBtn);

    buttonLayout->addStretch();

    validateBtn_ = new QPushButton("Validate");
    connect(validateBtn_, &QPushButton::clicked, this, &ProfileImportDialog::onValidate);
    buttonLayout->addWidget(validateBtn_);

    auto* cancelBtn = new QPushButton("Cancel");
    connect(cancelBtn, &QPushButton::clicked, this, &ProfileImportDialog::onCancel);
    buttonLayout->addWidget(cancelBtn);

    importBtn_ = new QPushButton("Import");
    importBtn_->setEnabled(false);
    importBtn_->setStyleSheet("background-color: #0078d4; color: #1f1f1f;");
    connect(importBtn_, &QPushButton::clicked, this, &ProfileImportDialog::onImport);
    buttonLayout->addWidget(importBtn_);

    layout->addLayout(buttonLayout);

    // Connect drop zone
    connect(dropZone, &ProfileDropZone::fileDropped, this, [this](const QString& path) {
        importedPath_ = path;
        onValidate();
    });
}

ProfileImportDialog::~ProfileImportDialog() = default;

void ProfileImportDialog::setImportedPath(const QString& path) {
    importedPath_ = path;
}

void ProfileImportDialog::onValidate() {
    if (importedPath_.isEmpty()) return;

    QFileInfo info(importedPath_);
    previewLabel_->setText(info.fileName());
    previewLabel_->setStyleSheet("background-color: #ffffff; padding: 12px; border-radius: 4px; color: #1f1f1f;");

    // Simulate validation (in production, would use ProfileValidator from profile_library)
    validation_.isValid = true;
    validation_.profileName = info.fileName();
    validation_.colorSpace = "RGB";
    validation_.mediaType = "Photo Paper";
    validation_.manufacturer = "Third Party";

    infoLabel_->setText(QString("Color Space: %1\nMedia Type: %2\nManufacturer: %3")
        .arg(validation_.colorSpace)
        .arg(validation_.mediaType)
        .arg(validation_.manufacturer));

    importBtn_->setEnabled(true);
}

void ProfileImportDialog::onImport() {
    if (!validation_.isValid) {
        QMessageBox::warning(this, "Invalid Profile", "Please validate the profile first.");
        return;
    }

    // Copy to custom profiles directory
    QString destDir = QDir::homePath() + "/Library/Application Support/jcolour/profiles";
    QDir().mkpath(destDir);

    QString destPath = destDir + "/" + QFileInfo(importedPath_).fileName();
    QFile::copy(importedPath_, destPath);

    accept();
}

void ProfileImportDialog::onCancel() {
    reject();
}

// ============================================
// ProfileLibraryBrowser Implementation
// ============================================

ProfileLibraryBrowser::ProfileLibraryBrowser(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle("Profile Library");
    setMinimumSize(500, 400);

    auto* layout = new QVBoxLayout(this);

    // Toolbar
    auto* toolbar = new QHBoxLayout;

    auto* addBtn = new QPushButton("Add Third-Party Profile...");
    connect(addBtn, &QPushButton::clicked, this, &ProfileLibraryBrowser::onAddThirdParty);
    toolbar->addWidget(addBtn);

    toolbar->addStretch();

    auto* deleteBtn = new QPushButton("Delete");
    connect(deleteBtn, &QPushButton::clicked, this, &ProfileLibraryBrowser::onDeleteSelected);
    toolbar->addWidget(deleteBtn);

    auto* showInFinderBtn = new QPushButton("Show in Finder");
    connect(showInFinderBtn, &QPushButton::clicked, this, &ProfileLibraryBrowser::onShowInFinder);
    toolbar->addWidget(showInFinderBtn);

    layout->addLayout(toolbar);

    // Profile list
    profileList_ = new QListWidget;
    profileList_->setAlternatingRowColors(true);
    connect(profileList_, &QListWidget::itemSelectionChanged, this, [this]() {
        if (profileList_->currentItem()) {
            selectedProfile_ = profileList_->currentItem()->data(Qt::UserRole).toString();
        }
    });
    layout->addWidget(profileList_, 1);

    // Detail panel
    detailLabel_ = new QLabel("Select a profile to view details");
    detailLabel_->setStyleSheet("background-color: #ffffff; padding: 8px; border-radius: 4px;");
    layout->addWidget(detailLabel_);

    // Load profiles
    loadProfiles();
    updateProfileList();
}

ProfileLibraryBrowser::~ProfileLibraryBrowser() = default;

void ProfileLibraryBrowser::setDisplayMode(bool showBuiltIn, bool showThirdParty, bool showCustom) {
    showBuiltIn_ = showBuiltIn;
    showThirdParty_ = showThirdParty;
    showCustom_ = showCustom;
    updateProfileList();
}

void ProfileLibraryBrowser::refresh() {
    loadProfiles();
    updateProfileList();
}

void ProfileLibraryBrowser::onAddThirdParty() {
    ProfileImportDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        if (!dialog.importedPath().isEmpty()) {
            emit profileImported(dialog.importedPath());
            refresh();
        }
    }
}

void ProfileLibraryBrowser::onImportDroppedFiles() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Select ICC Profiles",
        QString(), "ICC Files (*.icc *.iccg);;All Files (*)");

    for (const QString& path : files) {
        QString destDir = QDir::homePath() + "/Library/Application Support/jcolour/profiles";
        QDir().mkpath(destDir);
        QString destPath = destDir + "/" + QFileInfo(path).fileName();
        QFile::copy(path, destPath);
        emit profileImported(path);
    }

    refresh();
}

void ProfileLibraryBrowser::onDeleteSelected() {
    if (selectedProfile_.isEmpty()) return;

    int ret = QMessageBox::question(this, "Delete Profile",
        QString("Are you sure you want to remove %1?").arg(QFileInfo(selectedProfile_).fileName()),
        QMessageBox::Yes | QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        QFile::remove(selectedProfile_);
        emit profileDeleted(selectedProfile_);
        refresh();
    }
}

void ProfileLibraryBrowser::onShowInFinder() {
    QString path = selectedProfile_;
    if (!path.isEmpty()) {
        QStringList args;
        args << "-e" << "reveal" << path;
        QProcess::startDetached("open", args);
    }
}

void ProfileLibraryBrowser::loadProfiles() {
    builtInProfiles_.clear();
    thirdPartyProfiles_.clear();
    customProfiles_.clear();

    // Built-in profiles (from profile library)
    // In production, would use BuiltInProfileLibrary::instance().get_all_profiles()
    builtInProfiles_ << "Epson P9500 Photo Paper Glossy"
                     << "Epson P9500 Premium Luster"
                     << "Epson P9500 Archival Matte"
                     << "Hahnemühle Photo Rag 308"
                     << "Hahnemühle Baryta 325"
                     << "Canson Platine Fibre RAG";

    // Third-party profiles
    QString thirdPartyDir = "/Library/ColorSync/Profiles";
    if (QDir(thirdPartyDir).exists()) {
        QDir dir(thirdPartyDir);
        for (const QString& file : dir.entryList(QStringList() << "*.icc")) {
            thirdPartyProfiles_ << thirdPartyDir + "/" + file;
        }
    }

    // Custom profiles (user imported)
    QString customDir = QDir::homePath() + "/Library/Application Support/jcolour/profiles";
    if (QDir(customDir).exists()) {
        QDir dir(customDir);
        for (const QString& file : dir.entryList(QStringList() << "*.icc" << "*.iccg")) {
            customProfiles_ << customDir + "/" + file;
        }
    }
}

void ProfileLibraryBrowser::updateProfileList() {
    profileList_->clear();

    auto addItems = [this](const QStringList& profiles, const QString& category) {
        for (const QString& path : profiles) {
            auto* item = new QListWidgetItem(QFileInfo(path).fileName());
            item->setData(Qt::UserRole, path);
            item->setData(Qt::UserRole + 1, category);
            profileList_->addItem(item);
        }
    };

    if (showBuiltIn_) {
        addItems(builtInProfiles_, "Built-in");
    }
    if (showThirdParty_) {
        addItems(thirdPartyProfiles_, "Third-Party");
    }
    if (showCustom_) {
        addItems(customProfiles_, "Custom");
    }
}

} // namespace jcolour