#include "booth_model.h"
#include "worker_setup_booth.h"

#include <QThread>
#include <QRegularExpression>

Booth_model::Booth_model(QObject *parent) : QAbstractListModel(parent)
{
}

int Booth_model::rowCount(const QModelIndex &parent) const
{
  if (parent.isValid()) { return 0; }
  return _booths.length();
}

QVariant Booth_model::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || _booths.length() == 0)
  {
    return QVariant();
  }
  
  const Booth_item item = _booths.at(index.row());
  
  if      (role == Value_role)          { return QVariant(item.value);                  }
  else if (role == Text_role)           { return QVariant(item.text);                   }
  else if (role == Red_role)            { return QVariant(item.red);                    }
  else if (role == Green_role)          { return QVariant(item.green);                  }
  else if (role == Blue_role)           { return QVariant(item.blue);                   }
  else if (role == Visible_text_role)   { return QVariant(item.visible_text);           }
  else if (role == Coordinates_role)    { return QVariant::fromValue(item.coordinates); }
  
  return QVariant();
}

bool Booth_model::setData(const QModelIndex &index, const QVariant &value, int role)
{
  if (_map_not_ready || _booths.length() <= index.row()) { return false; }
  
  const int i = index.row();
  
  if (role == Visible_text_role)
  {
    const bool v = value.toBool();
    if (_booths.at(i).visible_text != v)
    {
      _booths[i].visible_text = v;
      emit dataChanged(index, index, QVector<int>() << Visible_text_role);
      return true;
    }
  }

  return false;
}

Qt::ItemFlags Booth_model::flags(const QModelIndex &index) const
{
  if (!index.isValid()) { return Qt::NoItemFlags; }
  return Qt::ItemIsEditable;
}

QHash<int, QByteArray> Booth_model::roleNames() const
{
  QHash<int, QByteArray> names;
  names[Coordinates_role] = "coordinates";
  names[Value_role] = "value";
  names[Text_role] = "text";
  names[Red_role] = "red";
  names[Green_role] = "green";
  names[Blue_role] = "blue";
  names[Visible_text_role] = "text_visible";
  return names;
}

void Booth_model::setup_list(QVector<Booth> &booths, int booth_threshold)
{
  _map_not_ready = true;
  
  beginResetModel();
  _booths.clear();
  _booth_ids.clear();
  
  QThread *thread = new QThread;
  Worker_setup_booth *worker = new Worker_setup_booth(booths,
                                                      _booths,
                                                      _booth_ids,
                                                      booth_threshold,
                                                      _text_type,
                                                      _prepoll);
  
  
  worker->moveToThread(thread);
  connect(thread, &QThread::started,                   worker, &Worker_setup_booth::start_setup);
  connect(worker, &Worker_setup_booth::finished_setup, this,   &Booth_model::finalise_setup);
  connect(worker, &Worker_setup_booth::finished_setup, thread, &QThread::quit);
  connect(worker, &Worker_setup_booth::finished_setup, worker, &Worker_setup_booth::deleteLater);
  connect(thread, &QThread::finished,                  thread, &QThread::deleteLater);
  thread->start();
}

void Booth_model::finalise_setup()
{
  endResetModel();
  _map_not_ready = false;
}

void Booth_model::clear_values()
{
  if (_map_not_ready) { return; }
  const int num_booths = _booths.length();
  for (int i = 0; i < num_booths; i++)
  {
    set_value(_booths.at(i).booth_id, 0., false);
  }

  if (num_booths > 0)
  {
    const QModelIndex top    = index(0, 0);
    const QModelIndex bottom = index(num_booths - 1, 0);
    emit dataChanged(top, bottom, { Value_role, Text_role });
  }

  set_colors();
}

void Booth_model::set_value(int booth_id, double value, bool emit_signal)
{
  if (_booth_ids.at(booth_id) < 0) { return; }
  if (booth_id >= _booth_ids.length() || _map_not_ready) { return; }
  
  const int i = _booth_ids.at(booth_id);
  
  _booths[i].value = value;
  _booths[i].text = QString("%1").arg(value, 0, 'f', _decimals);
  if (emit_signal)
  {
    QModelIndex index = this->index(i);
    emit dataChanged(index, index, QVector<int>() << Value_role << Text_role);
  }
}

void Booth_model::emit_all_data_changed_text()
{
  const int num_booths = _booths.length();
  if (num_booths > 0)
  {
    const QModelIndex top    = index(0, 0);
    const QModelIndex bottom = index(num_booths - 1, 0);
    emit dataChanged(top, bottom, { Value_role, Text_role });
  }
}

void Booth_model::set_colors()
{
  if (_map_not_ready || _idle) { return; }
  
  const int num_booths = _booths.length();
  for (int i = 0; i < num_booths; i++)
  {
    const double denom = _color_scale_max - _color_scale_min;
    int j = denom > 1e-5 ? qRound(255 * (_booths.at(i).value - _color_scale_min) / denom) : 0;
    
    j = qMax(j, 0);
    j = qMin(j, 255);
    
    _booths[i].red   = _viridis_scale.at(j).at(0);
    _booths[i].green = _viridis_scale.at(j).at(1);
    _booths[i].blue  = _viridis_scale.at(j).at(2);
  }

  if (num_booths > 0)
  {
    const QModelIndex top    = index(0,   0);
    const QModelIndex bottom = index(num_booths - 1, 0);
    emit dataChanged(top, bottom, { Red_role, Green_role, Blue_role });
  }
}

void Booth_model::set_decimals(int n)
{
  _decimals = n;
}

void Booth_model::set_decimals_mouseover(int n)
{
  _decimals_mouseover = n;
}

void Booth_model::set_visible(bool b)
{
  _text_type = b;
  if (!b)
  {
    _idle = true;
  }
  
  for (int i = 0; i < _booths.length(); i++)
  {
    recalculate_visible(i);
  }
}

bool Booth_model::is_booth_visible(int i)
{
  return _booths.at(i).in_active_division &&
         _booths.at(i).prepoll == _prepoll &&
         _booths.at(i).above_threshold;
}

void Booth_model::recalculate_visible(int i)
{
  const bool v        = is_booth_visible(i);
  const bool v_text   = v && _text_type;
  
  const QModelIndex index = this->index(i);
  setData(index, v_text, Visible_text_role);
}

void Booth_model::update_scale_min(double x)
{
  _color_scale_min = x;
  set_colors();
}

void Booth_model::update_scale_max(double x)
{
  _color_scale_max = x;
  set_colors();
}

void Booth_model::update_min_votes(int n)
{
  Q_UNUSED(n);
  for (int i = 0; i < _booths.length(); i++)
  {
    _booths[i].above_threshold = _booths.at(i).formal_votes >= n;
    recalculate_visible(i);
  }
}

void Booth_model::update_active_division(int division)
{
  for (int i = 0; i < _booths.length(); i++)
  {
    if (division >= 0)
    {
      _booths[i].in_active_division = _booths.at(i).division_id == division;
    }
    else
    {
      _booths[i].in_active_division = true;
    }
    recalculate_visible(i);
  }
}

void Booth_model::check_mouseover(double lon, double lat, double d_lon, double d_lat)
{
  const int r = 4; // pixel radius
  const int r2 = r*r;
  const int max_items = 2;
  
  QStringList mouseover_text;
  
  for (int i = 0; i < _booths.length(); i++)
  {
    if (_booths.at(i).visible_text)
    {
      const double dx = (_booths.at(i).coordinates.longitude() - lon) / d_lon;
      const double dy = (_booths.at(i).coordinates.latitude() - lat) / d_lat;
      
      if (dx*dx + dy*dy < r2)
      {
        if (mouseover_text.length() == max_items)
        {
          mouseover_text[max_items - 1] = "More...";
          break;
        }
        
        mouseover_text.append(QString("%1 %2: %3")
                              .arg(_booths.at(i).division_name, _booths.at(i).booth_name,
                                QString::number(_booths.at(i).value, 'f', _decimals_mouseover)));
      }
    }
  }
  
  QString text("");
  QString newline("");
  
  for (int i = 0; i < mouseover_text.length(); i++)
  {
    text = QString("%1%2%3").arg(text, newline, mouseover_text.at(i));
    
    newline = "<br>";
  }
  
  emit send_tooltip(text);
}

void Booth_model::update_prepoll_flag(bool show_prepoll)
{
  _prepoll = show_prepoll;
  
  for (int i = 0; i < _booths.length(); i++)
  {
    recalculate_visible(i);
  }
}

void Booth_model::set_idle(bool b)
{
  _idle = b;
}
