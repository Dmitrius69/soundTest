#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog>
#include <pthread.h>






#define INBUFF 16384
#define OUTBUFF 32768

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(slotTimerAlarm()));
    sound_subsystem_init();
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
      unsigned char sndbuffer[INBUFF];

        /* decode and play */
      timer->start(100);//запускаем таймер, для того что бы взаимодействовать с progressbar
      while (mpg123_read(m, sndbuffer, INBUFF, &done) == MPG123_OK)
      {
          //printf("*");
          tickFlag++;
          //ui->progressBar->setValue(tickFlag++);
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

    //while (!tickFlag)
    //{
    //  cText = cText + "*";
    //  ui->consoleOut->setText(cText);
   // }

   //возможная причина "подвисания"
   // pthread_join(playMusic, NULL);

}

void MainWindow::on_pushButton_2_clicked()
{
    //stop audio here
}


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

       gbytes = (int) (gbytes / 4097);
       ui->labelMsg->setText(QString("Bytes read %1").arg(gbytes));
       ui->progressBar->setMaximum(gbytes);
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

void MainWindow::slotTimerAlarm()
{
   ui->progressBar->setValue(tickFlag);
   if (tickFlag >= ui->progressBar->maximum()) timer->stop();
}

void MainWindow::on_progressBar_valueChanged(int value)
{
    //
    //if (value > ui->progressBar->maximum()) ui->progressBar->setValue(0);
}
