#ifndef WORKER_SQL_NPP_TABLE_H
#define WORKER_SQL_NPP_TABLE_H

#include <QObject>

class Worker_sql_npp_table : public QObject
{
  Q_OBJECT

public:
  Worker_sql_npp_table(int thread_num, const QString& db_file, const QString& q, int num_groups, int num_booths, QVector<int>& clicked_n_parties);
  ~Worker_sql_npp_table();

public slots:
  void do_query();

signals:
  void finished_query(const QVector<QVector<QVector<int>>>& partial_table);
  void error(QString err);

private:
  int _thread_num;
  QString _db_file;
  QString _q;
  int _num_groups;
  QVector<int> _clicked_n_parties;
  int _num_booths;
};

#endif // WORKER_SQL_NPP_TABLE_H
