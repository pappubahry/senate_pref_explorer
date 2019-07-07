#-------------------------------------------------
#
# Project created by QtCreator 2019-06-01T10:13:49
#
#-------------------------------------------------

QT       += core gui sql quickwidgets qml network positioning

RC_ICONS = pref6.ico

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = senate_pref_explorer
TEMPLATE = app


# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        freezetablewidget.cpp \
        main.cpp \
        main_widget.cpp \
        map_container.cpp \
        polygon_model.cpp \
        table_window.cpp \
        worker_setup_polygon.cpp \
        worker_sql_cross_table.cpp \
        worker_sql_main_table.cpp \
        worker_sql_npp_table.cpp

HEADERS += \
        freezetablewidget.h \
        main_widget.h \
        map_container.h \
        polygon_model.h \
        table_window.h \
        worker_setup_polygon.h \
        worker_sql_cross_table.h \
        worker_sql_main_table.h \
        worker_sql_npp_table.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
  map.qml

RESOURCES += \
  senate_pref_explorer.qrc
