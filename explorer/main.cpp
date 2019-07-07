#include "main_widget.h"
#include <QApplication>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
  QCoreApplication::addLibraryPath(".");
  QApplication a(argc, argv);
  Widget w;
  
  // Following line is to allow a spinbox to lose focus
  // when you click elsewhere in the window.
  w.setFocusPolicy(Qt::ClickFocus);
  
  w.setWindowTitle("Senate preference explorer");
  w.resize(1250, 700);
  w.setMinimumSize(QSize(500, 500));
  w.show();

  return a.exec();
}
