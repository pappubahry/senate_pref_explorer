#ifndef WORKER_SQL_CUSTOM_EVERY_EXPR_H
#define WORKER_SQL_CUSTOM_EVERY_EXPR_H

#include <QObject>

struct Custom_operation;

class Worker_sql_custom_every_expr : public QObject
{
  Q_OBJECT
public:
  explicit Worker_sql_custom_every_expr(const QString& db_file,
                                        int axis,
                                        int thread_num,
                                        const QString& q,
                                        int num_groups,
                                        int max_loop_index,
                                        std::vector<std::vector<int>>& aggregated_indices,
                                        std::vector<Custom_operation>& filter_operations,
                                        std::vector<Custom_operation>& cell_operations);
  ~Worker_sql_custom_every_expr();

public slots:
  void do_query_operations();
  void do_query_pure_sql();

signals:
  void finished_query(const int axis, const QVector<int>& values);
  void error(QString err);

private:
  QString _db_file;
  int _axis;
  int _thread_num;
  QString _q;
  int _num_groups;
  int _max_loop_index;
  std::vector<std::vector<int>> _aggregated_indices;
  bool _have_aggregated;
  std::vector<Custom_operation> _filter_operations;
  std::vector<Custom_operation> _cell_operations;
};

#endif // WORKER_SQL_CUSTOM_EVERY_EXPR_H
