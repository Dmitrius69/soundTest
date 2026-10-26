#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <atomic>

#include <pthread.h>
#include <unistd.h>

#include <mpg123.h>
#include <ao/ao.h>

#include "spectrumanalyzer.h"

#define SUCCESS 1
#define ERROR   0
#define INBUFF  16384

typedef enum { STOP, PLAY, PAUSE, QUIT } PlayerState;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    int  read_audio_file(QString flName);
    void playFile(const QString &fileName);
    int  sound_subsystem_init();
    int  sound_subsystem_dispose();
    void play_in_thread();
    void setPosition(qint64 frame);   // позиция в PCM-кадрах
    void setVolume(int volume);

    // Данные, которыми обмениваются GUI и поток воспроизведения.
    mpg123_handle   *m;
    ao_device       *ao_dev;
    int              ao_driver;
    size_t           done;
    int              err;
    ao_sample_format ao_format;
    int              channels;
    int              encoding;
    long             rate;
    unsigned char    sndbuffer[INBUFF];

private slots:
    void on_actionOpen_triggered();
    void on_pushButton_clicked();      // Play
    void on_pushButton_2_clicked();    // Stop
    void on_pushButton_3_clicked();    // -5 c
    void on_pushButton_4_clicked();    // +5 c
    void on_toolButton_2_clicked();    // Pause / Resume
    void on_soundSlider_valueChanged(int value);
    void on_soundSlider_sliderReleased();
    void on_verticalSlider_valueChanged(int value);
    void slotTimerAlarm();

private:
    static void *run(void *p);
    void jumpBy(qint64 frames);
    void seekTo(qint64 frame);
    void setChartsPaused(bool paused);
    bool eventFilter(QObject *watched, QEvent *event);

    pthread_t playMusic;
    bool      threadRunning;

    QTimer            *timer;
    Ui::MainWindow    *ui;
    SpectrumAnalyzer  *analyzer;

    double volume01;

    std::atomic<PlayerState> playerSt;
    std::atomic<qint64>      framesPlayed;
    std::atomic<qint64>      totalFrames;
    std::atomic<qint64>      seekRequest; // < 0 — нет запроса
    bool                     updatingSlider;
};

#endif // MAINWINDOW_H
