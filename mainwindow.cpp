#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <pthread.h>






//#define INBUFF 16384
//#define OUTBUFF 32768
//мьютекс для защиты потока воспроизведения
pthread_mutex_t MainWindow::playerMutex = PTHREAD_MUTEX_INITIALIZER;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(slotTimerAlarm()));
    sound_subsystem_init();
    scene = new QGraphicsScene();
    deltaBar = 0;
    widthBar = 0;
    gX=0;
    ui->graphicsView->setScene(scene);

}

MainWindow::~MainWindow()
{
    sound_subsystem_dispose();
    delete ui;
}

void MainWindow::on_actionOpen_triggered()
{
    QMessageBox myMsgBox;
    QString _fileName = QFileDialog::getOpenFileName(this, tr("OPEN FILE"), getenv("HOME"), tr("All (*.*)"));
    //myMsgBox.setText(_fileName);
    //myMsgBox.exec();
    ui->labelFN->setText(_fileName);
    read_audio_file(_fileName);

}


//функция обертка
//необходима для запуска функции воспроизведния
//в отдельном потоке
//свячзанно с тем, что потоковая функция должна быть
//описанна как static, но в этом случае не работают
//динамически подгружаемые mpg123 и ao
void* MainWindow::run(void* p)
{
    MainWindow* pl = reinterpret_cast<MainWindow*>(p);
    pl->play_in_thread();

    pthread_exit(0);
}

void MainWindow::play_in_thread()
{
    if (ao_dev !=NULL)
    {
       //здесь происходит управление воспроизведением звука
       //доработать алгоритм управления звковым потоком
       //добавить четыре состояния
       //ИГРА, СТОП, ВПЕРЕД и НАЗАД
       //добавить возможность вопспроизведения с любой точки

        /* decode and play */
      timer->start(100);//запускаем таймер, для того что бы взаимодействовать с progressbar
      while (mpg123_read(m, sndbuffer, INBUFF, &done) == MPG123_OK)
      {
          //printf("*");
          tickFlag++;
          ui->labelTicks->setText(QString("Ticks :  %1").arg(tickFlag));

          ao_play(ao_dev, (char*) &sndbuffer, done);

      }
      //timer->stop();
    }
    //if (!tickFlag) tickFlag = 1;
   //return ;
}

void MainWindow::on_pushButton_clicked()
{
    //start audio here
    //QString cText = "";

    tickFlag = 0;
    ui->progressBar->setValue(tickFlag);
    pthread_create(&playMusic,NULL,MainWindow::run,this);
    //timer->start(100);

   //возможная причина "подвисания"
   // pthread_join(playMusic, NULL);

}

void MainWindow::on_pushButton_2_clicked()
{
    //stop audio here
}

//открываем звуковой файл и производим декодировку
int MainWindow::read_audio_file(QString flName)
{
    unsigned char inbuff[INBUFF];
    int handle, bytes_read, gbytes;

    if((handle = open(flName.toStdString().c_str() ,O_RDONLY))== -1)
    {
          ui->labelMsg->setText(tr("Ошибка при открытии файла.\n"));
          return 0;
    }

    //тестовое чтение данных их звукового файла
    //к вопросизведениб звука никакого отношения не имеет
    gbytes = 0;
    if((bytes_read = read(handle,inbuff,INBUFF))==-1)
    {
       ui->labelMsg->setText(tr("Ошибка чтения.\n"));
      return 0;
    }
    gbytes += bytes_read;


    if (m) {
       mpg123_open(m, flName.toStdString().c_str());
       mpg123_getformat(m, &rate, &channels, &encoding);
       printf("rate = %d, channels = %ld encoding = %ld\n", rate, channels, encoding);
       gbytes =  mpg123_length(m);
       buffer = (unsigned char*) malloc(INBUFF * sizeof(unsigned char));
       widthBar = 12;
       deltaBar = 13;

       gbytes = (int) (gbytes / 4097);
       ui->labelMsg->setText(QString("Bytes read %1").arg(gbytes));
       ui->progressBar->setMaximum(gbytes);
       ui->soundSlider->setMaximum(gbytes);
       //ui->progressBar->setValue(0);
       /* set the output format and open the output device */
       ao_format.bits = mpg123_encsize(encoding) * BITS;
       ao_format.rate = rate;
       ao_format.channels = channels;
       ao_format.byte_format = AO_FMT_NATIVE;
       ao_format.matrix = 0;
       ao_dev = ao_open_live(ao_driver, &ao_format, NULL);
    }


    return 1;
}

int MainWindow::sound_subsystem_init()
{
    //произвордим инициализацию библиотек работы со звуком
    ao_initialize();
    ao_driver = ao_default_driver_id();
    mpg123_init();
    m = mpg123_new(NULL, &err);



    return SUCCESS;
}

int MainWindow::sound_subsystem_dispose()
{

    if (buffer != NULL) free(buffer);
    ao_close(ao_dev);
    mpg123_close(m);
    mpg123_delete(m);
    mpg123_exit();
    ao_shutdown();

    return SUCCESS;
}

void MainWindow::drawSndSample()
{
    int x,y;
    int gW, gH;
    //готовим сцену для отрисовки
    gW = ui->graphicsView->width();
    gH = ui->graphicsView->height();
    QRectF rF(0, 0, gW, gH);
    scene->setSceneRect(0, 0, gW, gH); //устанавливаем размер нашей сцены, он должен совпадать с размером graphicsView
    QPolygon backP(QRect(0,0,gW,gH), false) ;
    QPen mPen(Qt::green);
    QBrush mBrush(Qt::green);
    QPen wPen(Qt::white);
    QBrush wBrush(Qt::white);
    scene->addPolygon(backP, wPen, wBrush);

    for (int i=0;i<INBUFF - 1;i++ ) drawBar(i, sndbuffer[i]);
}

void MainWindow::drawBar(int idx, int bHeight)
{

    //рисукм наш звук
    int x,y;
    int gW, gH;

    gW = ui->graphicsView->width();
    gH = ui->graphicsView->height();

    int x0 = 0; //будем смещать x c помощью глобальной переменной
    int y0 = 300; //высота наших графиков будет (graphicsView->height ) pix


    QRectF rF(0, 0, gW, gH);
    scene->setSceneRect(0, 0, gW, gH); //устанавливаем размер нашей сцены, он должен совпадать с размером graphicsView
    QPolygon backP(QRect(0,0,gW,gH), false) ;

    QPen mPen(Qt::green);
    QBrush mBrush(Qt::green);
    QPen wPen(Qt::blue);
    QBrush wBrush(Qt::white);
    x = x0+idx*deltaBar;
    y = y0/2 - bHeight/2;

    /*if (gX == 0)
    {
        sndBurs[idx].gX1= x; sndBurs[idx].gY1 = y; sndBurs[idx].gW1 = widthBar; sndBurs[idx].gH1 = bHeight;

    }
    else
    {
        QPolygon bar(QRect(sndBurs[idx].gX1,sndBurs[idx].gY1,sndBurs[idx].gW1,sndBurs[idx].gH1), false);
        scene->addPolygon(bar, wPen, wBrush);

    }

    */
    //scene->addPolygon(*backP, mPen, mBrush);

    QBrush *blueBrush = new QBrush(Qt::white);
    scene->setBackgroundBrush(*blueBrush);
    //scene->addPolygon(backP, wPen, wBrush);
    sndBurs[idx].gX1= x; sndBurs[idx].gY1 = y; sndBurs[idx].gW1 = widthBar; sndBurs[idx].gH1 = y0/2+bHeight/2;
    QPolygon bar(QRect(x,y,widthBar,bHeight), false);
    scene->addPolygon(bar, mPen, mBrush);
    if (idx == (INBUFF -1)) gX = 1; //первый запуск прошел



}

void MainWindow::slotTimerAlarm()
{
   pthread_mutex_lock(&playerMutex);

   ui->soundSlider->setValue(tickFlag);
   //rawbuff = QString("%1 %2 %3 %4 %5 %6 %7 %8 %9 %10").arg(sndbuffer[0]).arg(sndbuffer[1]).arg(sndbuffer[2]).arg(sndbuffer[3]).arg(sndbuffer[4]).arg(sndbuffer[5]).arg(sndbuffer[6]).arg(sndbuffer[7]).arg(sndbuffer[8]).arg(sndbuffer[9]);
   //ui->progressBar->setValue(tickFlag++);
   //ui->labelrawdata->setText(rawbuff);
   drawSndSample();
   ui->progressBar->setValue(tickFlag);

   pthread_mutex_unlock(&playerMutex);
   if (tickFlag >= ui->progressBar->maximum()) timer->stop();
}

void MainWindow::on_progressBar_valueChanged(int value)
{
    //
    //if (value > ui->progressBar->maximum()) ui->progressBar->setValue(0);
}
