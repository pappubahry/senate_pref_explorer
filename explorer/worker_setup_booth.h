#ifndef WORKER_SETUP_BOOTH_H
#define WORKER_SETUP_BOOTH_H

#include <QObject>
#include "booth_model.h"

class Worker_setup_booth : public QObject
{
  Q_OBJECT
public:
  explicit Worker_setup_booth(QVector<Booth> &booths_input,
                              QVector<Booth_item> &booths_model,
                              QVector<long> &booth_ids,
                              long threshold,
                              bool text_type,
                              bool prepoll);
  ~Worker_setup_booth();
  
signals:
  void finished_setup();
  
public slots:
  void start_setup();
  
private:
  QVector<Booth> &_booths_input;
  QVector<Booth_item> &_booths_model;
  QVector<long> &_booth_ids;
  long _threshold;
  bool _text_type;
  bool _prepoll;
};

#endif // WORKER_SETUP_BOOTH_H
