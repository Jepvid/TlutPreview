#pragma once

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;

namespace tlutpreview {

// One window: an extracted texture pack folder in, an output folder, the
// Apply TLUT / Apply IA Transparency toggles, and a Create Copy button that
// runs colorizeTexturePack() on a worker thread.
class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void onBrowseSource();
    void onBrowseOutput();
    void onSourceEdited(const QString& text);
    void onRun();
    void updateRunEnabled();
    void setBusy(bool busy);

    QLineEdit* sourceEdit_;
    QLineEdit* outputEdit_;
    QCheckBox* applyTlutCheckbox_;
    QCheckBox* applyIaCheckbox_;
    QPushButton* runButton_;
    QProgressBar* progressBar_;
    QLabel* statusLabel_;

    // Last output path filled in from the source folder; replaced again
    // while the output field still holds it.
    QString suggestedOutput_;
};

} // namespace tlutpreview
