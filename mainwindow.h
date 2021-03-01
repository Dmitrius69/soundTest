#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTime>
#include <QTimer>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include <mpg123.h>
#include <ao/ao.h>

#define SUCCESS 1
#define ERROR 0
#define BITS 8
#define INBUFF 16384
#define OUTBUFF 32768

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    int read_audio_file(QString flName);
    int sound_subsystem_init();
    int sound_subsystem_dispose();
    void play_in_thread();
    void drawBar(int idx, int bHeigth);
    void drawSndSample();

    mpg123_handle *m;     //указатель на mpg123 исходный файл
    unsigned char *buffer; //буффер, куда записываем семплы
    ao_device *ao_dev; //устройство вывода звука
    int ao_driver;
    size_t buffer_size;
    size_t done;
    int err;

    ao_sample_format ao_format;
    int channels, encoding;
    long rate;
    unsigned char sndbuffer[INBUFF];
    QString rawbuff;


private slots:
    void on_actionOpen_triggered();

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_progressBar_valueChanged(int value);

    void slotTimerAlarm();

private:
    static void* run(void* p);
    int tickFlag;
    pthread_t playMusic;
    QTimer *timer;
    Ui::MainWindow *ui;
    static pthread_mutex_t  playerMutex;

    QGraphicsScene      *scene;     // Объявляем сцену для отрисовки
    struct mPoint { int gX1 ;
              int gY1 ;
              int gW1 ;
              int gH1 ;
            }  ;

    mPoint sndBurs[INBUFF]; // здесь будем хранить бары построенные от семплов
    int widthBar; //ширина одного тика
    int deltaBar; //расстояние от начала бара n-1 до бара n
    int gX;

};

#endif // MAINWINDOW_H
