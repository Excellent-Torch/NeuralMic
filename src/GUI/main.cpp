#include "MainWindow.h"
#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QFileInfo>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("NeuralMic");
    app.setApplicationVersion("0.0.1");

    // Global application icon fallback (used when no windows are shown)
    QString iconPath = QApplication::applicationDirPath() + "/../assets/images/icon.png";
    if (!QFileInfo::exists(iconPath)) {
        iconPath = "/usr/share/neuralmic/images/icon.png";
        if (!QFileInfo::exists(iconPath)) {
            iconPath = "/usr/local/share/neuralmic/images/icon.png";
        }
    }
    QPixmap appIcon(iconPath);
    if (!appIcon.isNull()) {
        app.setWindowIcon(QIcon(appIcon));
    }

    // Keep the application alive when the main window is hidden to tray
    app.setQuitOnLastWindowClosed(false);

    MainWindow window;
    window.show();

    return app.exec();
}
