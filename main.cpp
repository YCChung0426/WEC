#include "widget.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

    Widget w;
    w.showFullScreen();
    return a.exec();
}
