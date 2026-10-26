#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication a(argc, argv);

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window,          QColor(18, 22, 29));
    palette.setColor(QPalette::WindowText,      QColor(214, 226, 240));
    palette.setColor(QPalette::Base,            QColor(15, 19, 26));
    palette.setColor(QPalette::AlternateBase,   QColor(22, 28, 38));
    palette.setColor(QPalette::Text,            QColor(214, 226, 240));
    palette.setColor(QPalette::Button,          QColor(27, 38, 52));
    palette.setColor(QPalette::ButtonText,      QColor(214, 226, 240));
    palette.setColor(QPalette::Highlight,       QColor(0, 140, 220));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase,     QColor(20, 27, 36));
    palette.setColor(QPalette::ToolTipText,     QColor(214, 226, 240));
    a.setPalette(palette);

    a.setStyleSheet(
        "QWidget { background-color: #12161d; color: #d6e2f0; }"
        "QMainWindow, QMenuBar, QMenu, QStatusBar { background-color: #0e1218; }"
        "QLabel#labelFN { color: #e8f1fb; font-weight: bold; }"
        "QLabel#labelMsg { color: #7f93aa; }"
        "QLabel#labelTicks { color: #93a8bf; font-family: Consolas, 'DejaVu Sans Mono', monospace; }"
        "QPushButton, QToolButton {"
        "  background-color: #1a2533; border: 1px solid #2b3a4d;"
        "  border-radius: 5px; padding: 5px 12px; color: #d6e2f0; }"
        "QPushButton:hover, QToolButton:hover { background-color: #223146; border-color: #3c587c; }"
        "QPushButton:pressed, QToolButton:pressed { background-color: #2b4666; }"
        "QPushButton:disabled, QToolButton:disabled { color: #5b6b7f; }"
        "QSlider::groove:horizontal { height: 4px; background: #223041; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #1d9bd1; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #e8f1fb; width: 12px; margin: -5px 0;"
        "  border-radius: 6px; }"
        "QSlider::handle:horizontal:hover { background: #ffffff; }"
        "QProgressBar { background: #0e131a; border: 1px solid #223041;"
        "  border-radius: 3px; max-height: 6px; text-align: center; color: transparent; }"
        "QProgressBar::chunk { border-radius: 3px;"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "  stop:0 #00e5ff, stop:1 #a878ff); }"
        "QMenuBar::item:selected { background: #1d2a3a; }"
        "QMenu { background: #141b24; border: 1px solid #2a3748; }"
        "QMenu::item:selected { background: #20344a; }"
        "QToolTip { background: #141b24; color: #d6e2f0; border: 1px solid #2a3748; }");

    QFont applicationFont = a.font();
    applicationFont.setPointSize(9);
    a.setFont(applicationFont);

    MainWindow w;
    w.show();

    // Необязательный аргумент командной строки: сразу открыть и проиграть файл.
    const QStringList arguments = a.arguments();
    if (arguments.size() > 1)
    {
        const QString fileName = arguments.at(1);
        if (!fileName.startsWith(QLatin1Char('-')))
            w.playFile(fileName);
    }

    return a.exec();
}
