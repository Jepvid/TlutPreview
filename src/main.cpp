#include <QApplication>

#include "main_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("TlutPreview"));

    tlutpreview::MainWindow window;
    window.show();
    return QApplication::exec();
}
