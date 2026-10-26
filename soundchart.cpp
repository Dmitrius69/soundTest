#include "soundchart.h"

SoundChart::SoundChart(QObject *parent, QGraphicsView *baseView) :
    QObject(parent),
    startCharting_(false),
    lenData(MAXCHART_LEN),
    widthBar(1),
    deltaBar(1)
{
    vSndChart = baseView;
    sndChartScene = new QGraphicsScene();
    sceneHeight = vSndChart->height();
    sceneWidth = vSndChart->width();
    vSndChart->setScene(sndChartScene);
    startX = 0; //будем смещать x c помощью глобальной переменной
    startY = sceneHeight; //высота наших графиков будет (graphicsView->height ) pix
    acc = 0.5; //множитель высоты
    chartBrush.setColor(Qt::blue);
    chartPen.setColor(Qt::blue);
    sceneBrush.setColor(Qt::black);
    scenePen.setColor(Qt::black);
    //bars = new QVector<QGraphicsRectItem*>;
    //drawScene();//отрисовываем задник
    createHoldbars();



}



SoundChart::~SoundChart()
{
    //удаляем экземпляр объекта
}

//создаем набор из пустых баров (0-вой высоты)
void SoundChart::createHoldbars()
{
    int y;
    y=startY/2;
    for(int i=0;i<lenData;i++)
    {
        QGraphicsRectItem * item = sndChartScene->addRect(startX+deltaBar*i, y, widthBar, 0, chartPen, chartBrush); // высота всех делений равна нулю
        //bars.push_back(item);
        bars.append(item);
    }

}

void SoundChart::appendBars(int len, unsigned char data[])
{
    int y;
    y=startY/2;
    int shift = bars.size();
    for (int i = shift; i<len+shift; i++)
   {
       QGraphicsRectItem * item = sndChartScene->addRect(startX+deltaBar*i, y, widthBar, 0, chartPen, chartBrush); // высота всех делений равна нулю
       //bars.push_back(item);
       bars.append(item);

   }
}


void SoundChart::drawScene()
{
    int gW, gH;
    //готовим сцену для отрисовки
    //printf("Start drawSndSample -- in\n");
    gW = sceneWidth;
    gH = sceneHeight;
    //QRectF rF(0, 0, gW, gH);
    sndChartScene->setSceneRect(0, 0, gW, gH); //устанавливаем размер нашей сцены, он должен совпадать с размером graphicsView
    //QPolygon backP(QRect(0,0,gW,gH), false) ;
    //printf("Start scene clearing\n");
    //sndChartScene->addPolygon(backP, scenePen, sceneBrush);

}

void SoundChart::drawBars()
{

    for(int idx=0;idx<lenData;idx++)
    {
        //bars[idx]->setRect(sndBurs[idx].gX1, sndBurs[idx].gY1, sndBurs[idx].gW1, sndBurs[idx].gH1, chartPen, chartBrush); // высота i-го деления задается из массива VALUE
        //QRect *r = bars.at(idx);
        foreach ( QGraphicsItem* itm , sndChartScene->items()) {
           //itm->setRect(sndBurs[idx].gX1, sndBurs[idx].gY1, sndBurs[idx].gW1, sndBurs[idx].gH1);
            QRect* r = reinterpret_cast<QRect*> (itm);
            r->setRect(sndBurs[idx].gX1, sndBurs[idx].gY1, sndBurs[idx].gW1, sndBurs[idx].gH1);
        }

    }

}


void SoundChart::update(unsigned char data[])
{
  for (int i=0; i<lenData; i++){
    bars.at(i)->setRect(startX+i*deltaBar, startY/2-(data[i]*acc)/2, widthBar,  (data[i]*acc));
  }
}

void SoundChart::setWidth(int wBar)
{
    widthBar = wBar;
}

void SoundChart::setDelta(int dBars)
{
    deltaBar = dBars;

}

void SoundChart::setLenDataSeries(int len)
{
    lenData = len;
}

void SoundChart::MakeDataSeries(int data[])
{
     int x,y;

     for(int idx=0;idx<lenData; idx++)
     {

         x = startX+idx*deltaBar;
         y = startY/2 - (data[idx]*acc)/2;

         sndBurs[idx].gX1= x;
         sndBurs[idx].gY1 = y;
         sndBurs[idx].gW1 = widthBar;
         sndBurs[idx].gH1 = startY/2+(data[idx]*acc)/2;


     }

}
