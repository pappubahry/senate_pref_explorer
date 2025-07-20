#ifndef WORKER_SQL_CUSTOM_TABLE_H
#define WORKER_SQL_CUSTOM_TABLE_H

#include <QObject>

struct Custom_operation;

class Worker_sql_custom_table : public QObject
{
    Q_OBJECT
public:
    Worker_sql_custom_table(int thread_num,
                            const QString& db_file,
                            const QString& q,
                            int num_groups,
                            int num_booths,
                            std::vector<int>& axis_numbers,
                            std::vector<int>& row_stack_indices,
                            std::vector<int>& col_stack_indices,
                            int max_loop_index,
                            std::vector<std::vector<int>>& aggregated_indices,
                            std::vector<Custom_operation>& filter_operations,
                            std::vector<Custom_operation>& row_operations,
                            std::vector<Custom_operation>& col_operations,
                            std::vector<Custom_operation>& cell_operations);
    ~Worker_sql_custom_table();

public slots:
    void do_query();
    void do_query_by_booth();

signals:
    void finished_query(int partial_total_base, const QVector<int>& partial_row_base, const QVector<QVector<int>>& partial_table);
    void finished_query_by_booth(const QVector<int>& partial_total_base,
                                 const QVector<QVector<int>>& partial_row_base,
                                 const QVector<QVector<QVector<int>>>& partial_table);
    void error(QString err);

private:
    int _thread_num;
    QString _db_file;
    QString _q;
    int _num_groups;
    int _num_booths;
    std::vector<int> _axis_numbers;
    std::vector<int> _row_stack_indices;
    std::vector<int> _col_stack_indices;
    int _max_loop_index;
    std::vector<std::vector<int>> _aggregated_indices;
    bool _have_aggregated;
    std::vector<Custom_operation> _filter_operations;
    std::vector<Custom_operation> _row_operations;
    std::vector<Custom_operation> _col_operations;
    std::vector<Custom_operation> _cell_operations;
};

#endif // WORKER_SQL_CUSTOM_TABLE_H
