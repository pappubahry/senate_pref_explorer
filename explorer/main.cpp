#include "main_widget.h"
#include <QApplication>

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  Widget w;
  w.setWindowTitle("Senate preference explorer");
  //w.showMaximized();
  w.resize(800, 600);
  w.setMinimumSize(QSize(500, 500));
  w.show();

  return a.exec();
}
