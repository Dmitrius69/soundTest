#-------------------------------------------------
#
# soundTest — проигрыватель MP3 и анализатор спектра
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET   = soundTest
TEMPLATE = app

CONFIG  += c++11

# Для MinGW/GCC явно включаем C++11 (на случай старого qmake).
*-g++: QMAKE_CXXFLAGS += -std=gnu++11

# MSVC должен читать исходники как UTF-8 (в подписях графиков кириллица).
msvc: QMAKE_CXXFLAGS += /utf-8

SOURCES += main.cpp \
           mainwindow.cpp \
           spectrumanalyzer.cpp \
           spectrumwidget.cpp

HEADERS += mainwindow.h \
           spectrumanalyzer.h \
           spectrumwidget.h

FORMS   += mainwindow.ui

# Звуковые библиотеки
unix|win32-g++ {
    LIBS += -lao -lmpg123 -lm
}
