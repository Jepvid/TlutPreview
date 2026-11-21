#include "main_window.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

#include <filesystem>

#include "colorize.h"

namespace tlutpreview {

namespace {

std::filesystem::path toPath(const QString& text) {
    return std::filesystem::path(text.toStdString());
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle(tr("TlutPreview"));
    resize(640, 0);

    auto* layout = new QVBoxLayout(this);

    auto* intro = new QLabel(
        tr("Makes a copy of an extracted texture pack folder (one with a manifest.json) with palettes (TLUTs) "
           "applied to CI4/CI8 textures and real transparency on glow/spark/dust I8 textures."));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel(tr("Texture pack")), 0, 0);
    sourceEdit_ = new QLineEdit();
    sourceEdit_->setPlaceholderText(tr("Folder with manifest.json"));
    grid->addWidget(sourceEdit_, 0, 1);
    auto* browseSource = new QPushButton(tr("Browse..."));
    grid->addWidget(browseSource, 0, 2);

    grid->addWidget(new QLabel(tr("Output")), 1, 0);
    outputEdit_ = new QLineEdit();
    outputEdit_->setPlaceholderText(tr("New or empty folder"));
    grid->addWidget(outputEdit_, 1, 1);
    auto* browseOutput = new QPushButton(tr("Browse..."));
    grid->addWidget(browseOutput, 1, 2);
    layout->addLayout(grid);

    auto* options = new QHBoxLayout();
    applyTlutCheckbox_ = new QCheckBox(tr("Apply TLUT"));
    applyTlutCheckbox_->setChecked(true);
    applyTlutCheckbox_->setToolTip(tr("Colors CI4/CI8 textures with their real palette."));
    options->addWidget(applyTlutCheckbox_);
    applyIaCheckbox_ = new QCheckBox(tr("Apply IA Transparency"));
    applyIaCheckbox_->setChecked(true);
    applyIaCheckbox_->setToolTip(tr("Shows real transparency for glow/spark/dust I8 textures."));
    options->addWidget(applyIaCheckbox_);
    options->addStretch(1);
    runButton_ = new QPushButton(tr("Create Copy"));
    options->addWidget(runButton_);
    layout->addLayout(options);

    progressBar_ = new QProgressBar();
    progressBar_->setVisible(false);
    layout->addWidget(progressBar_);

    statusLabel_ = new QLabel();
    statusLabel_->setWordWrap(true);
    statusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(statusLabel_);

    connect(browseSource, &QPushButton::clicked, this, &MainWindow::onBrowseSource);
    connect(browseOutput, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
    connect(sourceEdit_, &QLineEdit::textChanged, this, &MainWindow::onSourceEdited);
    connect(outputEdit_, &QLineEdit::textChanged, this, &MainWindow::updateRunEnabled);
    connect(applyTlutCheckbox_, &QCheckBox::toggled, this, &MainWindow::updateRunEnabled);
    connect(applyIaCheckbox_, &QCheckBox::toggled, this, &MainWindow::updateRunEnabled);
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::onRun);

    updateRunEnabled();
}

void MainWindow::onBrowseSource() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Texture Pack Folder"), sourceEdit_->text());
    if (!dir.isEmpty()) {
        sourceEdit_->setText(QDir::toNativeSeparators(dir));
    }
}

void MainWindow::onBrowseOutput() {
    QString start = outputEdit_->text().isEmpty() ? sourceEdit_->text() : QFileInfo(outputEdit_->text()).path();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), start);
    if (!dir.isEmpty()) {
        outputEdit_->setText(QDir::toNativeSeparators(dir));
    }
}

// Fills the output with "<pack> (TLUT)" next to the pack, unless the user
// has typed their own output path.
void MainWindow::onSourceEdited(const QString& text) {
    if (outputEdit_->text().isEmpty() || outputEdit_->text() == suggestedOutput_) {
        QFileInfo info(QDir::cleanPath(QDir::fromNativeSeparators(text)));
        suggestedOutput_ = text.isEmpty() ? QString()
                                          : QDir::toNativeSeparators(info.path() + "/" + info.fileName() + " (TLUT)");
        outputEdit_->setText(suggestedOutput_);
    }
    updateRunEnabled();
}

void MainWindow::updateRunEnabled() {
    bool hasManifest = QFileInfo::exists(QDir(sourceEdit_->text()).filePath(QStringLiteral("manifest.json")));
    bool anyOption = applyTlutCheckbox_->isChecked() || applyIaCheckbox_->isChecked();
    runButton_->setEnabled(hasManifest && anyOption && !outputEdit_->text().isEmpty() && !progressBar_->isVisible());
    if (!sourceEdit_->text().isEmpty() && !hasManifest && !progressBar_->isVisible()) {
        statusLabel_->setText(tr("No manifest.json in that folder. Pick a folder made by BitDeck's "
                                 "\"Extract textures from otr/o2r\"."));
    } else if (statusLabel_->text().startsWith(tr("No manifest.json"))) {
        statusLabel_->clear();
    }
}

void MainWindow::setBusy(bool busy) {
    progressBar_->setVisible(busy);
    sourceEdit_->setEnabled(!busy);
    outputEdit_->setEnabled(!busy);
    applyTlutCheckbox_->setEnabled(!busy);
    applyIaCheckbox_->setEnabled(!busy);
    updateRunEnabled();
}

void MainWindow::onRun() {
    std::filesystem::path source = toPath(sourceEdit_->text());
    std::filesystem::path output = toPath(outputEdit_->text());
    ColorizeOptions options;
    options.applyTlut = applyTlutCheckbox_->isChecked();
    options.applyIaTransparency = applyIaCheckbox_->isChecked();

    progressBar_->setRange(0, 0);
    statusLabel_->setText(tr("Copying..."));
    setBusy(true);

    QThread* worker = QThread::create([this, source, output, options] {
        auto reportProgress = [this](int done, int total) {
            QMetaObject::invokeMethod(this, [this, done, total] {
                progressBar_->setRange(0, total);
                progressBar_->setValue(done);
                statusLabel_->setText(tr("Applying... %1 / %2").arg(done).arg(total));
            });
        };
        QString message;
        try {
            ColorizeResult result = colorizeTexturePack(source, output, options, reportProgress);
            message = tr("Done. Copied %1 file(s) to %2.").arg(result.filesCopied).arg(QString::fromStdString(output.string()));
            if (options.applyTlut) {
                message += tr("\nTLUT applied to %1 CI texture(s); %2 had no TLUT found and stay grayscale.")
                               .arg(result.tlutApplied)
                               .arg(result.tlutUnmatched);
            }
            if (options.applyIaTransparency) {
                message += tr("\nIA transparency applied to %1 I8 texture(s).").arg(result.iaApplied);
            }
        } catch (const std::exception& e) {
            message = tr("Failed: %1").arg(QString::fromStdString(e.what()));
        }
        QMetaObject::invokeMethod(this, [this, message] {
            setBusy(false);
            statusLabel_->setText(message);
        });
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

} // namespace tlutpreview
