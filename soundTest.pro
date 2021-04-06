#-------------------------------------------------
#
# Project created by QtCreator 2021-02-17T15:41:06
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = soundTest
TEMPLATE = app

#CONFIG += -std=c++11  -std=gnu++11
#эта строка необходима, что бы компилятоп смог
#правильно обрабатывать библиотеку atomic!
QMAKE_CXXFLAGS += -std=c++11  -std=gnu++11

SOURCES += main.cpp\
        mainwindow.cpp \
    soundchart.cpp

HEADERS  += mainwindow.h \
    soundchart.h

FORMS    += mainwindow.ui

#добавили в проект звуковые библиотеки
unix {
  LIBS += -lao -lmpg123 -lm
}
