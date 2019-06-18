#ifndef WORKER_SQL_CROSS_TABLE_H
#define WORKER_SQL_CROSS_TABLE_H

#include <QObject>
#include <main_widget.h>

class Worker_sql_cross_table : public QObject {
  Q_OBJECT
  
public:
  Worker_sql_cross_table(QString table_type,
                         int thread_num,
                         QString db_file,
                         QString q,
                         int num_rows,
                         QVector<int> args);
  ~Worker_sql_cross_table();

public slots:
  void do_query();
  
signals:
  void finished_query(const QVector<QVector<long>> &partial_table);
  void progress(double d);
  void error(QString err);
  
private:
  QString _table_type;
  int _thread_num;
  QString _db_file;
  QString _q;
  int _num_rows;
  QVector<int>_args;
};

#endif // WORKER_SQL_CROSS_TABLE_H
