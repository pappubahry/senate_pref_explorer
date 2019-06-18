#ifndef WORKER_SQL_MAIN_TABLE_H
#define WORKER_SQL_MAIN_TABLE_H

#include <QObject>
#include <main_widget.h>

class Worker_sql_main_table : public QObject {
  Q_OBJECT
  
public:
  Worker_sql_main_table(int thread_num,
                        QString db_file,
                        QString q,
                        bool wide_table,
                        bool base_count,
                        int num_groups,
                        int num_rows,
                        int num_divisions,
                        QVector<int> clicked_cells);
  ~Worker_sql_main_table();
  
public slots:
  void do_query();
  
signals:
  void finished_query(const QVector<Table_main_item> &partial_table);
  void progress(double d); // *** Maybe useless ***
  void error(QString err);
  
private:
  int _thread_num;
  QString _db_file;
  QString _q;
  bool _wide_table;
  bool _base_count;
  int _num_groups;
  int _num_rows;
  int _num_divisions;
  QVector<int> _clicked_cells;
};

#endif // WORKER_SQL_MAIN_TABLE_H
