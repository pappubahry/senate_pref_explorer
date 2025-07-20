#ifndef CLICKABLE_LABEL_H
#define CLICKABLE_LABEL_H

#include <QLabel>

// https://wiki.qt.io/Clickable_QLabel CC BY-SA 2.5
class Clickable_label : public QLabel
{
  Q_OBJECT

public:
  explicit Clickable_label(QWidget* parent = nullptr)
    : QLabel(parent) {};
  ~Clickable_label() {};

signals:
  void clicked();

protected:
  void mouseReleaseEvent(QMouseEvent* event)
  {
    Q_UNUSED(event)
    emit clicked();
  }
};

#endif // CLICKABLE_LABEL_H
