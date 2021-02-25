#-------------------------------------------------
#
# Project created by QtCreator 2021-02-17T15:41:06
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = soundTest
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.h

FORMS    += mainwindow.ui

#добавили в проект звуковые библиотеки
unix {
  LIBS += -lao -lmpg123 -lm
}
