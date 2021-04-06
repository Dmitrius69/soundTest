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
    //ui->graphicsView->setScene(scene);
    sChart = new SoundChart(parent, ui->graphicsView);
    sChart->setLenDataSeries(512);
    sChart->setDelta(8);
    sChart->setWidth(3);


}

MainWindow::~MainWindow()
{
    playerSt = QUIT;
    pthread_join(playMusic, NULL);
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
//свя+занно с тем, что потоковая функция должна быть
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
    int read_res = -1 ;
    if (ao_dev !=NULL)
    {
       //здесь происходит управление воспроизведением звука
       //доработать алгоритм управления звковым потоком
       //добавить четыре состояния
       //ИГРА, СТОП, ВПЕРЕД и НАЗАД
       //добавить возможность вопспроизведения с любой точки

        /* decode and play */
        printf("Starting thread\n");
        int ret_from_thread = 0;

        while (!ret_from_thread)
        {

           //printf("Main cycle\n");
           switch (playerSt)
           {
           case PLAY:
                read_res = mpg123_read(m, sndbuffer, INBUFF, &done);
                if (read_res == MPG123_OK)
                {

                    tickFlag++;
                    ui->labelTicks->setText(QString("Ticks :  %1").arg(tickFlag));

                    ao_play(ao_dev, (char*) &sndbuffer, done);

                    //printf("drawSample sndbuffer\n");
                    //drawSndSample();
                    //printf("was drawing!\n");


                }
                if (read_res == MPG123_DONE)  playerSt = QUIT;


                break;
           case STOP:
               //ЕСЛИ нажали стоп, то восроизведение начне м с начала
               tickFlag = 0;
                setPosition(0);
                break;
           case PAUSE:
                break;
           case QUIT:
                //завепршаем поток проигрыванияж
                ret_from_thread = 1;
               break;
           default:
               usleep(10000);
               break;
          }
        }

      //timer->start(100);//запускаем таймер, для того что бы взаимодействовать с progressbar
      //while (mpg123_read(m, sndbuffer, INBUFF, &done) == MPG123_OK)
      //{

      //    tickFlag++;
      //    ui->labelTicks->setText(QString("Ticks :  %1").arg(tickFlag));

      //     ao_play(ao_dev, (char*) &sndbuffer, done);

      //}
      //timer->stop();
    }
    //if (!tickFlag) tickFlag = 1;
   //return ;
    //clearScene();//очищащем память  сцены
    printf("Stop thread tiks=%d\n", tickFlag);

}

void MainWindow::clearScene()
{
    scene->clear();
}


//кнопка PLAY
void MainWindow::on_pushButton_clicked()
{
    playerSt = PLAY;
    timer->start(50);

}

//кнопка STOP
void MainWindow::on_pushButton_2_clicked()
{
    //stop audio here
    playerSt = STOP;
    timer->stop();
}


//
void MainWindow::setMedia(QString path)
{
    //SetPlayerState(STOP);

    if(ao_dev)
    {
        ao_close(ao_dev);
        ao_dev = NULL;
    }

    mpg123_close(m);

    /* open the file and get the decoding format */
    mpg123_open(m, path.toLocal8Bit());
    mpg123_getformat(m, &rate, &channels, &encoding);

    /* set the output format and open the output device */
    ao_format.bits        = mpg123_encsize(encoding) * BITS;
    ao_format.rate        = rate;
    ao_format.channels    = channels;
    ao_format.byte_format = AO_FMT_NATIVE;
    ao_format.matrix      = 0;

    ao_dev = ao_open_live(ao_driver, &ao_format, NULL);
}

void MainWindow::setPosition(qint64 position)
{
    // Still, libmpg123 will be confused by the junk data at the beginning.
    // A hack to fix the seeking behaviour is to insert
    // mpg123_seek_frame(mh, 10, SEEK_SET);
    // before doing any position work (matching -k 10 in your initial mpg123 command).
    mpg123_seek_frame(m, 10, SEEK_SET);

    //qDebug()<<"setPosition pos_ms="<<position;

    double pos_sec=(double)position/1000;   // get position in miliseconds and calculate in seconds
    int frame=mpg123_timeframe(m,pos_sec); // calculate frame number for position in seconds
    int seek_res=mpg123_seek_frame(m, frame,SEEK_SET);
    if(seek_res<0)
    {
        //qDebug()<<"Seek error: code="<<seek_res;
    }
}

void MainWindow::setVolume(int volume)
{
    volume01=(double)volume/100;
    int res=mpg123_volume(m, volume01);
    if(res!=MPG123_OK)
    {
        //qDebug()<<"Can't set volume to value "<<volume;
    }
}


//=======================================


//открываем звуковой файл и производим декодировку
int MainWindow::read_audio_file(QString flName)
{
    unsigned char inbuff[INBUFF];
    int handle, bytes_read, gbytes;

    bytes_read = 0;
    Q_UNUSED(bytes_read);
    //flName.toStdString().c_str()
    if((handle = open(flName.toLocal8Bit() ,O_RDONLY))== -1)
    {
          ui->labelMsg->setText(tr("Can't open mp3 file.\n"));
          return 0;
    }

    //тестовое чтение данных их звукового файла
    //к вопросизведениб звука никакого отношения не имеет
    gbytes = 0;
    //if((bytes_read = read(handle,inbuff,INBUFF))==-1)
    //{
    //   ui->labelMsg->setText(tr("Read faile.\n"));
    //  return 0;
    //}
    //gbytes += bytes_read;

    if(ao_dev)
    {
        ao_close(ao_dev);
        ao_dev = NULL;
    }

    mpg123_close(m);


    if (m) {
       pthread_mutex_lock(&playerMutex);
       playerSt = QUIT;
       mpg123_open(m,flName.toLocal8Bit());
       //            flName.toStdString().c_str());
       mpg123_getformat(m, &rate, &channels, &encoding);
       //printf("rate = %d, channels = %ld encoding = %ld\n", rate, channels, encoding);
       gbytes =  mpg123_length(m);
       if (buffer != NULL)  free(buffer);
       buffer = (unsigned char*) malloc(UP_BUFFER_SIZE * sizeof(unsigned char));
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
       setPosition(0);
       playerSt = STOP;
       //printf("Start playerThread\n");
       //если слушали музыку до этого, выключаем поток и создаем его заново
       //if (playMusic != NULL) pthread_join(playMusic,NULL);
       tickFlag = 0;
       pthread_mutex_unlock(&playerMutex);
       pthread_create(&playMusic,NULL,MainWindow::run,this);
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

    int gW, gH;
    //готовим сцену для отрисовки
    //printf("Start drawSndSample -- in\n");
    gW = ui->graphicsView->width();
    gH = ui->graphicsView->height();
    QRectF rF(0, 0, gW, gH);
    scene->setSceneRect(0, 0, gW, gH); //устанавливаем размер нашей сцены, он должен совпадать с размером graphicsView
    QPolygon backP(QRect(0,0,gW,gH), false) ;
    //QPen mPen(Qt::green);
    //QBrush mBrush(Qt::green);
    QPen wPen(Qt::black);
    QBrush wBrush(Qt::black);
    //printf("Start scene clearing\n");
    scene->addPolygon(backP, wPen, wBrush);

    //printf("Scene was cleared\n");
    for (int i=0;i<INBUFF - 1;i++ ) drawBar(i, sndbuffer[i], 2, 1);
}

void MainWindow::drawBar(int idx, int bHeight, int delta, int wBar)
{

    //рисукм наш звук
    int x,y;
    float acc = 0.5;
    //printf("Start drawBar idx=%d bHeigth = %d\n", idx, bHeight);

    int x0 = 0; //будем смещать x c помощью глобальной переменной
    int y0 = ui->graphicsView->height(); //высота наших графиков будет (graphicsView->height ) pix


    //QPolygon backP(QRect(0,0,gW,gH), false) ;

    QPen mPen(Qt::blue);
    QBrush mBrush(Qt::blue);
    //QPen wPen(Qt::blue);
    //QBrush wBrush(Qt::white);
    x = x0+idx*delta;
    y = y0/2 - (bHeight*acc)/2;
    //printf("x=%d y=%d deltaBar=%d widthBar=%d\n", x, y, delta, wBar);
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

    //QBrush *blueBrush = new QBrush(Qt::white);
    //scene->setBackgroundBrush(*blueBrush);
    //scene->addPolygon(backP, wPen, wBrush);
    sndBurs[idx].gX1= x; sndBurs[idx].gY1 = y; sndBurs[idx].gW1 = wBar; sndBurs[idx].gH1 = y0/2+(bHeight*acc)/2;
    //printf("Creating QRECT\n");
    QPolygon bar(QRect(x,y,wBar,(int)bHeight*acc), false);
    //printf("Add poligon\n");
    scene->addPolygon(bar, mPen, mBrush);
    //printf("Poligon was added\n");
    //if (idx == (INBUFF -1)) gX = 1; //первый запуск прошел

}

void MainWindow::slotTimerAlarm()
{
   pthread_mutex_lock(&playerMutex);
   //printf("Timer tick start\n");
   ui->soundSlider->setValue(tickFlag);
   //printf("Tick = %d\n", tickFlag);
   //rawbuff = QString("%1 %2 %3 %4 %5 %6 %7 %8 %9 %10").arg(sndbuffer[0]).arg(sndbuffer[1]).arg(sndbuffer[2]).arg(sndbuffer[3]).arg(sndbuffer[4]).arg(sndbuffer[5]).arg(sndbuffer[6]).arg(sndbuffer[7]).arg(sndbuffer[8]).arg(sndbuffer[9]);
   //ui->progressBar->setValue(tickFlag++);
   //ui->labelrawdata->setText(rawbuff);

   //отрисовку будем делать в отдельном классе
   //drawSndSample();
   unsigned char temp[INBUFF];
   for (int i=0; i<INBUFF; i++) temp[i] = 0;
   int delta = INBUFF/512;
   int k=0;
   for (int i=0; i<512; i++)
   {
       temp[i]= sndbuffer[k];
       k+=delta;
   }
   sChart->update(temp);
   ui->progressBar->setValue(tickFlag);

   pthread_mutex_unlock(&playerMutex);
   if (tickFlag >= ui->progressBar->maximum()) timer->stop();
}

void MainWindow::on_progressBar_valueChanged(int value)
{
    //
    //if (value > ui->progressBar->maximum()) ui->progressBar->setValue(0);
}

void MainWindow::on_verticalSlider_valueChanged(int value)
{
    //изменяем громкость
    setVolume(value);
}

void MainWindow::on_toolButton_2_clicked()
{
    playerSt = PAUSE;
    timer->stop();
}
