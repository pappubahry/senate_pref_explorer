import QtQuick 2.0
import QtQuick.Window 2.0
import QtLocation 5.11
import QtPositioning 5.11
import QtQml 2.12
import Division_boundaries 1.0
import Booths 1.0


Item
{
  visible: true
  width: map_size
  height: map_size
  signal exited_map()
  
  
  Map
  {
    id: map
    objectName: "map"
    width: map_size
    height: map_size
    
    property var latitude : -30
    property var longitude: 133
    property var zoom_level: 4
    
    anchors.fill: parent
    plugin: Plugin {name: "osm"}
    center: QtPositioning.coordinate(latitude, longitude)
    zoomLevel: zoom_level
    
    
    MapItemView
    {
      model: divisions_model;
      
      delegate: MapPolygon
      {
        path: model.coordinates;
        border.color: "black";
        border.width: 2;
        color: Qt.rgba(model.red, model.green, model.blue, model.opacity);
      }
    }
    
    
    MapItemView
    {
      model: booths_model
      
      delegate: MapQuickItem
      {
        coordinate: model.coordinates;
        anchorPoint.x: booth_txt.width/2;
        anchorPoint.y: booth_txt.height/2;
        
        sourceItem: Text
        {
          id: booth_txt;
          text: model.text;
          visible: model.text_visible;
          font.pointSize: 14;
          style: Text.Outline;
          styleColor: "black";
          color: Qt.rgba(model.red, model.green, model.blue, 1.);
        }
      }
    }
    
    // I've done all the point-in-polygon calculations in C++ because
    // they are sloooooooooow in QML.  But, perhaps because of a bug
    // in QQuickWidget, I don't have access to the leaveEvent(), so
    // I emit a signal from QML when the mouse leaves the map area.
    MouseArea
    {
      anchors.fill: parent;
      hoverEnabled: true;
      onExited: exited_map()
    }
  }
}
