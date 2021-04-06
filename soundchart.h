#ifndef SOUNDCHART_H
#define SOUNDCHART_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QGraphicsRectItem>
#include <QVector>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QPen>

#include <atomic>
#include <chrono>

#define MAXCHART_LEN 16384

class SoundChart : public QObject
{
    Q_OBJECT
public:
    explicit SoundChart(QObject *parent = 0, QGraphicsView *baseView = 0); //конструктор
    ~SoundChart();                            //деструктор
    void MakeDataSeries(int data[]);
    void drawScene();
    void setWidth(int wBar);
    void setDelta(int dBars);
    void setLenDataSeries(int len);
    void createHoldbars();
    void drawBars();
    void update(unsigned char data[]);


signals:

public slots:

private:
    std::atomic<bool> startCharting_; //переменная для потоков
    struct mPoint {
              int gX1 ;
              int gY1 ;
              int gW1 ;
              int gH1 ;
            }  ;

    mPoint sndBurs[MAXCHART_LEN]; // здесь будем хранить бары построенные от семплов
    int widthBar; //ширина одного тика
    int deltaBar; //расстояние от начала бара n-1 до бара n
    int lenData;
    int startX;
    int startY;
    float acc;
    int sceneHeight;
    int sceneWidth;
    QBrush chartBrush;
    QPen chartPen;
    QBrush sceneBrush;
    QPen scenePen;
    QGraphicsScene *sndChartScene;
    QGraphicsView  *vSndChart;
    QVector<QGraphicsRectItem*> bars;

};

#endif // SOUNDCHART_H
