#ifndef WORKER_SQL_CROSS_TABLE_H
#define WORKER_SQL_CROSS_TABLE_H

#include <QObject>

class Worker_sql_cross_table : public QObject
{
  Q_OBJECT

public:
  Worker_sql_cross_table(const QString& table_type, int thread_num, const QString& db_file, const QString& q, int num_rows, QVector<int>& args);
  ~Worker_sql_cross_table();

public slots:
  void do_query();

signals:
  void finished_query(const QVector<QVector<int>>& partial_table);
  void error(QString err);

private:
  QString _table_type;
  int _thread_num;
  QString _db_file;
  QString _q;
  int _num_rows;
  QVector<int> _args;
};

#endif // WORKER_SQL_CROSS_TABLE_H
