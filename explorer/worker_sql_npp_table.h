#ifndef WORKER_SQL_NPP_TABLE_H
#define WORKER_SQL_NPP_TABLE_H

#include <QObject>
#include <main_widget.h>

class Worker_sql_npp_table : public QObject {
  Q_OBJECT
  
public:
  Worker_sql_npp_table(int thread_num,
                       QString db_file,
                       QString q,
                       int num_groups,
                       int num_divisions,
                       QVector<int> clicked_n_parties);
  ~Worker_sql_npp_table();
  
public slots:
  void do_query();
  
signals:
  void finished_query(const QVector<QVector<Table_main_item>> &partial_table);
  void progress(double d); // *** Prob useless ***
  void error (QString err);
  
private:
  int _thread_num;
  QString _db_file;
  QString _q;
  int _num_groups;
  QVector<int> _clicked_n_parties;
  int _num_divisions;
};

#endif // WORKER_SQL_NPP_TABLE_H
