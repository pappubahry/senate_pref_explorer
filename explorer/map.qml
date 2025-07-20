import QtQuick 6.9
import QtQuick.Window 6.9
import QtLocation 6.9
import QtPositioning 6.9
import QtQml 6.9
import QtQml.Models 6.9
import Division_boundaries 1.0
import Booths 1.0

Item
{
  visible: true
  anchors.fill: parent
  signal exited_map()

  Plugin
  {
    id: mapPlugin
    name: "osm"
  }

  // I spent a long time going back and forth with o4-mini-high on how to
  // define a PluginParameter conditional on the value of some property.
  // This Component-Loader and pushing onto the parameters array was the first
  // successful approach after a dozen or more failures.
  Component
  {
    id: hostParamComponent
    PluginParameter
    {
      name:  "osm.mapping.custom.host"
      value: tileServer
    }
  }

  Loader
  {
    active: !useDefaultServer
    sourceComponent: hostParamComponent
    onLoaded: mapPlugin.parameters.push(item)
  }

  Map
  {
    id: map
    objectName: "map"

    // The properties are used to zoom to a location from C++:
    property real propLatitude : -30.
    property real propLongitude: 133.
    property real propZoomLevel: 4.

    anchors.fill: parent
    plugin: mapPlugin
    onSupportedMapTypesChanged:
    {
      activeMapType = supportedMapTypes[map.supportedMapTypes.length - 1]
    }

    Component.onCompleted:
    {
      map.center    = QtPositioning.coordinate(propLatitude, propLongitude)
      map.zoomLevel = propZoomLevel
    }

    onPropLatitudeChanged:  map.center = QtPositioning.coordinate(propLatitude, map.center.longitude)
    onPropLongitudeChanged: map.center = QtPositioning.coordinate(map.center.latitude, propLongitude)
    onPropZoomLevelChanged: map.zoomLevel = propZoomLevel

    WheelHandler
    {
      // In the good old days of Qt 5.12, the map panned and zoomed without
      // needing any special code to define it.

      id: wheel
      // workaround for QTBUG-87646 / QTBUG-112394 / QTBUG-112432:
      // Magic Mouse pretends to be a trackpad but doesn't work with PinchHandler
      // and we don't yet distinguish mice and trackpads on Wayland either
      acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"
                       ? PointerDevice.Mouse | PointerDevice.TouchPad
                       : PointerDevice.Mouse
      rotationScale: 1/480
      property real minZoom: 4.
      property real maxZoom: 18.

      onWheel: function(event)
      {
        var mousePoint = Qt.point(event.x, event.y)
        var mouseCoords = map.toCoordinate(mousePoint)

        var newZoom = map.zoomLevel + event.angleDelta.y * rotationScale
        map.zoomLevel = Math.min(maxZoom, Math.max(minZoom, newZoom))

        // Map has zoomed around its centre; calculate new lon/lat under the mouse
        var tempMouseCoords = map.toCoordinate(mousePoint)

        // Shift the centre so that the coords under the mouse are what they were
        // to begin with
        map.center.longitude += mouseCoords.longitude - tempMouseCoords.longitude
        map.center.latitude  += mouseCoords.latitude  - tempMouseCoords.latitude

        event.accepted = true
      }
    }

    DragHandler
    {
      id: drag
      target: null
      onTranslationChanged: (delta) => map.pan(-delta.x, -delta.y)
    }


    MapItemView
    {
      model: divisionsModel;

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
      model: boothsModel

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
  }

  Text
  {
    id: copyrightElement
    anchors
    {
      left: parent.left; bottom: parent.bottom
      leftMargin: 0; bottomMargin: 0
    }
    visible: !useDefaultServer
    textFormat: Text.RichText
    text: copyrightText
    font.pixelSize: 12
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

    onClicked: function(mouse)
    {
      const tx = mouse.x - copyrightElement.x;
      const ty = mouse.y - copyrightElement.y;
      const href = copyrightElement.linkAt(tx, ty);
      if (href)
      {
        Qt.openUrlExternally(href);
      }
    }
  }
}
