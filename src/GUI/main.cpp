#include "MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("NeuralMic");
    app.setApplicationVersion("0.0.1");

    MainWindow window;
    window.show();

    return app.exec();
}
