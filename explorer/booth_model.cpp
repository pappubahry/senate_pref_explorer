#include "booth_model.h"
#include "viridis.h"

#include <QRegularExpression>

void Booth_model::setup_list(QVector<Booth> &booths, int booth_threshold)
{
  _map_not_ready = true;
  _booth_ids.clear();
  _booths.clear();
  
  const int num_booths = booths.length();
  int ct               = 0;

  for (int i = 0; i < num_booths; i++)
  {
    const Booth& booth = booths.at(i);
    // Many of the polling places have zero values for lon/lat;
    // the following check assumes that we're in Australia.
    if (booth.longitude > 0.1 && booth.latitude < -0.1)
    {
      _booth_ids.append(ct);

      Booth_item* item = new Booth_item(this);

      QString booth_name = booth.booth;
      static QRegularExpression pattern("^.*_");
      booth_name.replace(pattern, "");

      item->set_booth_id(booth.id);
      item->set_formal_votes(booth.formal_votes);
      item->set_division_id(booth.division_id);
      item->set_value(0.);
      item->set_prepoll(booth_name.contains("PPVC") || booth_name.contains("PREPOLL", Qt::CaseInsensitive));
      item->set_above_threshold(booth.formal_votes > booth_threshold);
      item->set_in_active_division(true);
      item->set_booth_name(booth_name);
      item->set_division_name(booth.division);
      item->set_coordinates(QGeoCoordinate(booth.latitude, booth.longitude));
      item->set_text("0");
      item->set_color(QColor(0, 0, 0));
      item->set_visible((item->is_prepoll() == _prepoll) && item->is_above_threshold() && _text_type);
      _booths.append(item);

      ct++;
    }
    else
    {
      _booth_ids.append(-1);
    }
  }

  emit boothsChanged();

  _map_not_ready = false;
}

void Booth_model::clear_values()
{
  if (_map_not_ready) { return; }
  const int num_booths = _booths.length();
  for (int i = 0; i < num_booths; i++)
  {
    set_value(_booths.at(i)->get_booth_id(), 0.);
  }

  set_colors();
}

void Booth_model::set_value(int booth_id, double value)
{
  if (_booth_ids.at(booth_id) < 0) { return; }
  if (booth_id >= _booth_ids.length() || _map_not_ready) { return; }
  
  const int i = _booth_ids.at(booth_id);
  
  _booths.at(i)->set_value(value);
  _booths.at(i)->set_text(QString::number(value, 'f', _decimals));
}

void Booth_model::set_colors()
{
  if (_map_not_ready || _idle) { return; }
  
  const int num_booths = _booths.length();
  for (int i = 0; i < num_booths; i++)
  {
    const double denom = _color_scale_max - _color_scale_min;
    int j = denom > 1e-5 ? qRound(255 * (_booths.at(i)->get_value() - _color_scale_min) / denom) : 0;
    
    j = qMax(j, 0);
    j = qMin(j, 255);

    const QVector<double>& color = Viridis::colors.at(j);
    _booths.at(i)->set_color(QColor::fromRgbF(color.at(0), color.at(1), color.at(2)));
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
  return _booths.at(i)->is_in_active_division() &&
         _booths.at(i)->is_prepoll() == _prepoll &&
         _booths.at(i)->is_above_threshold();
}

void Booth_model::recalculate_visible(int i)
{
  const bool visible = is_booth_visible(i) && _text_type;
  _booths.at(i)->set_visible(visible);
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
    _booths.at(i)->set_above_threshold(_booths.at(i)->get_formal_votes() >= n);
    recalculate_visible(i);
  }
}

void Booth_model::update_active_division(int division)
{
  for (int i = 0; i < _booths.length(); i++)
  {
    if (division >= 0)
    {
      _booths.at(i)->set_in_active_division(_booths.at(i)->get_division_id() == division);
    }
    else
    {
      _booths.at(i)->set_in_active_division(true);
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
    if (_booths.at(i)->visible())
    {
      const QGeoCoordinate coords = _booths.at(i)->coordinates();
      const double dx = (coords.longitude() - lon) / d_lon;
      const double dy = (coords.latitude()  - lat) / d_lat;
      
      if (dx*dx + dy*dy < r2)
      {
        if (mouseover_text.length() == max_items)
        {
          mouseover_text[max_items - 1] = "More...";
          break;
        }
        
        mouseover_text.append(QString("%1 %2: %3")
                              .arg(_booths.at(i)->get_division_name(), _booths.at(i)->get_booth_name(),
                                QString::number(_booths.at(i)->get_value(), 'f', _decimals_mouseover)));
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
