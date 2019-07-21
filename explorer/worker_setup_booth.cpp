#include "worker_setup_booth.h"
#include <QRegularExpression>

Worker_setup_booth::Worker_setup_booth(QVector<Booth> &booths_input,
                                       QVector<Booth_item> &booths_model,
                                       QVector<long> &booth_ids,
                                       long threshold,
                                       bool text_type,
                                       bool prepoll)
  : _booths_input(booths_input),
    _booths_model(booths_model),
    _booth_ids(booth_ids)
{
  _threshold = threshold;
  _text_type = text_type;
  _prepoll = prepoll;
}

Worker_setup_booth::~Worker_setup_booth()
{
}

void Worker_setup_booth::start_setup()
{
  long num_booths = _booths_input.length();
  long ct = 0;
  
  for (long i = 0; i < num_booths; i++)
  {
    // Many of the polling places have zero values for lon/lat;
    // the following check assumes that we're in Australia.
    if (_booths_input.at(i).longitude > 0.1 && _booths_input.at(i).latitude < -0.1)
    {
      _booth_ids.append(ct);
      
      Booth_item item;
      
      QString booth_name = _booths_input.at(i).booth;
      booth_name.replace(QRegularExpression("^.*_"), "");
      
      item.booth_id = _booths_input.at(i).id;
      item.formal_votes = _booths_input.at(i).formal_votes;
      item.division_id = _booths_input.at(i).division_id;
      item.value = 0.;
      
      item.prepoll = booth_name.contains("PPVC") || booth_name.contains("PREPOLL", Qt::CaseInsensitive);
      item.above_threshold = item.formal_votes >= _threshold;
      item.in_active_division = true;
      
      item.division_name = _booths_input.at(i).division;
      item.booth_name = booth_name;
      item.coordinates = QGeoCoordinate(_booths_input.at(i).latitude, _booths_input.at(i).longitude);
      item.text = "0";
      item.red = 0.;
      item.green = 0.;
      item.blue = 0.;
      
      item.visible_text = (item.prepoll == _prepoll) && item.above_threshold && _text_type;
      
      _booths_model.append(item);
      
      ct++;
    }
    else
    {
      _booth_ids.append(-1);
    }
  }
  
  emit finished_setup();
}

