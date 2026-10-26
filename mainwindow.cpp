#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "spectrumwidget.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>

#include <cstring>

namespace {

QString formatTime(qint64 frames, long sampleRate)
{
    if (sampleRate <= 0 || frames < 0)
        return QString("00:00");

    const qint64 seconds = frames / sampleRate;
    const qint64 hours   = seconds / 3600;
    const qint64 minutes = (seconds / 60) % 60;
    const qint64 secs    = seconds % 60;

    if (hours > 0)
        return QString("%1:%2:%3")
                .arg(hours)
                .arg(minutes, 2, 10, QChar('0'))
                .arg(secs, 2, 10, QChar('0'));

    return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m(0),
      ao_dev(0),
      ao_driver(0),
      done(0),
      err(0),
      channels(0),
      encoding(0),
      rate(0),
      playMusic(),
      threadRunning(false),
      timer(0),
      ui(new Ui::MainWindow),
      analyzer(0),
      volume01(0.8),
      playerSt(PAUSE),
      framesPlayed(0),
      totalFrames(0),
      seekRequest(-1),
      updatingSlider(false)
{
    ui->setupUi(this);
    std::memset(&ao_format, 0, sizeof(ao_format));

    sound_subsystem_init();

    analyzer = new SpectrumAnalyzer();

    ui->globsonicView->setAnalyzer(analyzer);
    ui->globsonicView->setMode(SpectrumWidget::Response);
    ui->globsonicView->setTitle(
                QString::fromUtf8("АЧХ — усреднённая амплитудно-частотная характеристика"));

    ui->graphicsView->setAnalyzer(analyzer);
    ui->graphicsView->setMode(SpectrumWidget::Bars);
    ui->graphicsView->setTitle(
                QString::fromUtf8("СПЕКТР — распределение мощности по частотам"));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(slotTimerAlarm()));

    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    updatingSlider = true;
    ui->soundSlider->setRange(0, 100);
    ui->soundSlider->setValue(0);
    updatingSlider = false;
    // Клик в любом месте дорожки должен перематывать, а не прибавлять
    // pageStep: у слайдера со стилями зона попадания слишком тонкая.
    ui->soundSlider->installEventFilter(this);

    ui->verticalSlider->setRange(0, 100);
    ui->verticalSlider->setValue(80);
    setVolume(80);

    ui->labelFN->setText(QString::fromUtf8("Файл не выбран — Файл → Открыть…"));
    ui->labelMsg->setText(QString::fromUtf8("готов"));
    ui->labelTicks->setText(QString("00:00 / 00:00"));

    setWindowTitle(QString::fromUtf8("soundTest — спектральный анализ"));
}

MainWindow::~MainWindow()
{
    playerSt.store(QUIT);

    if (threadRunning)
    {
        pthread_join(playMusic, NULL);
        threadRunning = false;
    }

    // Сначала уничтожаем виджеты (останавливаются их таймеры),
    // затем анализатор, которым они пользовались.
    delete ui;
    ui = 0;

    delete analyzer;
    analyzer = 0;

    sound_subsystem_dispose();
}

int MainWindow::sound_subsystem_init()
{
    ao_initialize();
    ao_driver = ao_default_driver_id();

    mpg123_init();
    m = mpg123_new(NULL, &err);
    if (!m)
        return ERROR;

    return SUCCESS;
}

int MainWindow::sound_subsystem_dispose()
{
    if (ao_dev)
    {
        ao_close(ao_dev);
        ao_dev = 0;
    }

    if (m)
    {
        mpg123_close(m);
        mpg123_delete(m);
        m = 0;
    }

    mpg123_exit();
    ao_shutdown();
    return SUCCESS;
}

void *MainWindow::run(void *p)
{
    MainWindow *player = reinterpret_cast<MainWindow *>(p);
    player->play_in_thread();
    pthread_exit(0);
    return 0;
}

void MainWindow::play_in_thread()
{
    if (!ao_dev || !m)
        return;

    while (playerSt.load() != QUIT)
    {
        // Отложенный переход по таймлайну (выполняется в аудиопотоке,
        // чтобы не трогать mpg123_handle из GUI одновременно с чтением).
        const qint64 requestedFrame = seekRequest.exchange(-1);
        if (requestedFrame >= 0)
        {
            setPosition(requestedFrame);
            framesPlayed.store(requestedFrame);
        }

        const PlayerState state = playerSt.load();

        if (state == PAUSE)
        {
            usleep(5000);
            continue;
        }

        if (state == STOP)
        {
            setPosition(0);
            framesPlayed.store(0);
            playerSt.store(PAUSE);
            continue;
        }

        int readResult = mpg123_read(m, sndbuffer, INBUFF, &done);

        if (readResult == MPG123_OK || readResult == MPG123_NEW_FORMAT)
        {
            if (done > 0 && analyzer)
            {
                const int sampleSize = mpg123_encsize(encoding);
                const int channelsNow = (channels > 0) ? channels : 1;
                const int bytesPerFrame = (sampleSize > 0 ? sampleSize : 1) * channelsNow;
                const size_t frames = done / size_t(bytesPerFrame);

                if (sampleSize == 2)
                    analyzer->pushPcm16(sndbuffer, frames, channelsNow);
                else if (sampleSize == 1)
                    analyzer->pushPcm8(sndbuffer, frames, channelsNow);
            }

            if (done > 0)
                ao_play(ao_dev, reinterpret_cast<char *>(sndbuffer), done);

            const off_t position = mpg123_tell(m);
            if (position >= 0)
                framesPlayed.store(qint64(position));
        }
        else if (readResult == MPG123_DONE)
        {
            setPosition(0);
            framesPlayed.store(totalFrames.load());
            playerSt.store(PAUSE);
        }
        else
        {
            usleep(5000);
        }
    }
}

void MainWindow::setPosition(qint64 frame)
{
    if (!m || frame < 0)
        return;

    mpg123_seek(m, off_t(frame), SEEK_SET);
}

void MainWindow::setVolume(int volume)
{
    volume01 = qBound(0.0, double(volume) / 100.0, 1.0);

    if (m)
        mpg123_volume(m, volume01);
}

int MainWindow::read_audio_file(QString flName)
{
    if (flName.isEmpty() || !m)
        return 0;

    if (timer)
        timer->stop();

    // Останавливаем старый поток, иначе он продолжит читать mpg123_handle,
    // который мы сейчас переоткрываем.
    if (threadRunning)
    {
        playerSt.store(QUIT);
        pthread_join(playMusic, NULL);
        threadRunning = false;
    }

    if (ao_dev)
    {
        ao_close(ao_dev);
        ao_dev = 0;
    }

    mpg123_close(m);

    if (mpg123_open(m, flName.toLocal8Bit().constData()) != MPG123_OK)
    {
        ui->labelMsg->setText(QString::fromUtf8("Ошибка открытия: %1")
                              .arg(QString::fromLocal8Bit(mpg123_strerror(m))));
        return 0;
    }

    if (mpg123_getformat(m, &rate, &channels, &encoding) != MPG123_OK ||
        rate <= 0 || channels <= 0)
    {
        ui->labelMsg->setText(QString::fromUtf8("Не удалось определить формат файла"));
        return 0;
    }

    const int sampleSize = mpg123_encsize(encoding);
    if (sampleSize <= 0)
    {
        ui->labelMsg->setText(QString::fromUtf8("Неподдерживаемый формат PCM"));
        return 0;
    }

    std::memset(&ao_format, 0, sizeof(ao_format));
    ao_format.bits        = sampleSize * 8;
    ao_format.rate        = rate;
    ao_format.channels    = channels;
    ao_format.byte_format = AO_FMT_NATIVE;
    ao_format.matrix      = 0;

    ao_dev = ao_open_live(ao_driver, &ao_format, NULL);
    if (!ao_dev)
    {
        ui->labelMsg->setText(QString::fromUtf8("Не удалось открыть аудиоустройство (libao)"));
        return 0;
    }

    qint64 total = qint64(mpg123_length(m));
    if (total < 0)
        total = 0;

    totalFrames.store(total);
    framesPlayed.store(0);
    seekRequest.store(-1);

    if (analyzer)
    {
        analyzer->setSampleRate(rate);
        analyzer->reset();
    }

    setChartsPaused(false);

    const int maxValue = int(qMin<qint64>(total, 2000000000LL));
    ui->progressBar->setRange(0, qMax(1, maxValue));
    ui->progressBar->setValue(0);

    updatingSlider = true;
    ui->soundSlider->setRange(0, qMax(1, maxValue));
    ui->soundSlider->setPageStep(int(qMax<qint64>(1, qint64(rate) * 5)));
    ui->soundSlider->setSingleStep(int(qMax<qint64>(1, qint64(rate))));
    ui->soundSlider->setValue(0);
    updatingSlider = false;

    const QString fileName = QFileInfo(flName).fileName();
    const QString channelText = (channels == 1)
            ? QString::fromUtf8("моно")
            : ((channels == 2) ? QString::fromUtf8("стерео")
                               : QString::fromUtf8("%1 кан.").arg(channels));

    ui->labelFN->setText(fileName);
    ui->labelFN->setToolTip(flName);
    ui->labelMsg->setText(QString::fromUtf8("%1 · %2 кГц · %3 · %4 бит")
                          .arg(QFileInfo(flName).suffix().toUpper())
                          .arg(double(rate) / 1000.0, 0, 'f', 1)
                          .arg(channelText)
                          .arg(sampleSize * 8));
    ui->labelTicks->setText(QString("00:00 / %1").arg(formatTime(total, rate)));

    setWindowTitle(QString::fromUtf8("%1 — soundTest").arg(fileName));

    setPosition(0);
    setVolume(ui->verticalSlider->value());
    playerSt.store(PAUSE);

    if (pthread_create(&playMusic, NULL, MainWindow::run, this) == 0)
        threadRunning = true;
    else
        ui->labelMsg->setText(QString::fromUtf8("Не удалось создать поток воспроизведения"));

    return 1;
}

void MainWindow::playFile(const QString &fileName)
{
    if (read_audio_file(fileName))
    {
        playerSt.store(PLAY);
        if (timer && !timer->isActive())
            timer->start(80);
    }
}

void MainWindow::on_actionOpen_triggered()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this,
                QString::fromUtf8("Открыть аудиофайл"),
                QDir::homePath(),
                QString::fromUtf8("MP3 (*.mp3 *.mp2 *.mp1);;Все файлы (*.*)"));

    if (fileName.isEmpty())
        return;

    read_audio_file(fileName);
}

void MainWindow::on_pushButton_clicked()
{
    if (!threadRunning)
        return;

    playerSt.store(PLAY);
    setChartsPaused(false);

    if (timer && !timer->isActive())
        timer->start(80);
}

void MainWindow::on_pushButton_2_clicked()
{
    if (!threadRunning)
        return;

    // Переход в начало выполнит аудиопоток, чтобы не трогать mpg123_handle
    // из GUI-потока одновременно с декодированием.
    playerSt.store(PAUSE);
    seekRequest.store(0);
    framesPlayed.store(0);
    setChartsPaused(false);

    if (timer)
        timer->stop();

    ui->progressBar->setValue(0);
    updatingSlider = true;
    ui->soundSlider->setValue(0);
    updatingSlider = false;
    ui->labelTicks->setText(QString("00:00 / %1").arg(formatTime(totalFrames.load(), rate)));
}

void MainWindow::on_pushButton_3_clicked()
{
    jumpBy(-qint64(double(rate) * 5.0));
}

void MainWindow::on_pushButton_4_clicked()
{
    jumpBy(qint64(double(rate) * 5.0));
}

void MainWindow::jumpBy(qint64 frames)
{
    if (!threadRunning || rate <= 0)
        return;

    seekTo(framesPlayed.load() + frames);
}

void MainWindow::on_toolButton_2_clicked()
{
    if (!threadRunning)
        return;

    if (playerSt.load() == PLAY)
    {
        playerSt.store(PAUSE);
        setChartsPaused(true);
    }
    else
    {
        playerSt.store(PLAY);
        setChartsPaused(false);
        if (timer && !timer->isActive())
            timer->start(80);
    }
}

void MainWindow::setChartsPaused(bool paused)
{
    if (!ui)
        return;

    ui->globsonicView->setPaused(paused);
    ui->graphicsView->setPaused(paused);
}

void MainWindow::on_soundSlider_valueChanged(int value)
{
    // Программное обновление позиции и перетаскивание ползунка сюда не
    // относятся: первое отсекается флагом, второе обрабатывается в
    // sliderReleased, чтобы не перематывать файл на каждом пикселе.
    if (updatingSlider || !threadRunning || ui->soundSlider->isSliderDown())
        return;

    seekTo(qint64(value));
}

void MainWindow::on_soundSlider_sliderReleased()
{
    if (!threadRunning)
        return;

    seekTo(qint64(ui->soundSlider->value()));
}

void MainWindow::seekTo(qint64 frame)
{
    if (rate <= 0)
        return;

    const qint64 total = totalFrames.load();

    if (frame < 0)
        frame = 0;
    if (total > 0 && frame > total)
        frame = total;

    framesPlayed.store(frame);
    seekRequest.store(frame);

    ui->labelTicks->setText(QString("%1 / %2")
                            .arg(formatTime(frame, rate))
                            .arg(formatTime(total, rate)));

    if (timer && !timer->isActive())
        timer->start(80);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->soundSlider && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouse = static_cast<QMouseEvent *>(event);

        if (mouse->button() == Qt::LeftButton && threadRunning)
        {
            QStyleOptionSlider option;
            option.initFrom(ui->soundSlider);
            option.orientation  = ui->soundSlider->orientation();
            option.minimum      = ui->soundSlider->minimum();
            option.maximum      = ui->soundSlider->maximum();
            option.sliderPosition = ui->soundSlider->sliderPosition();
            option.sliderValue  = ui->soundSlider->value();
            option.singleStep   = ui->soundSlider->singleStep();
            option.pageStep     = ui->soundSlider->pageStep();
            option.upsideDown   = false;

            const QStyle::SubControl control =
                    ui->soundSlider->style()->hitTestComplexControl(
                        QStyle::CC_Slider, &option, mouse->pos(), ui->soundSlider);

            // По ползунку — обычное перетаскивание, по остальной дорожке —
            // мгновенный переход в точку клика.
            if (control != QStyle::SC_SliderHandle)
            {
                const int value = QStyle::sliderValueFromPosition(
                            option.minimum, option.maximum,
                            mouse->pos().x(), ui->soundSlider->width(),
                            option.upsideDown);

                seekTo(qint64(value));

                updatingSlider = true;
                ui->soundSlider->setValue(value);
                updatingSlider = false;

                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::on_verticalSlider_valueChanged(int value)
{
    setVolume(value);
}

void MainWindow::slotTimerAlarm()
{
    const qint64 total = totalFrames.load();
    qint64 position = framesPlayed.load();

    if (total <= 0)
        return;

    if (position < 0)
        position = 0;
    if (position > total)
        position = total;

    const int value = int(qMin<qint64>(position, 2000000000LL));

    ui->progressBar->setValue(value);

    if (!ui->soundSlider->isSliderDown())
    {
        updatingSlider = true;
        ui->soundSlider->setValue(value);
        updatingSlider = false;
    }

    ui->labelTicks->setText(QString("%1 / %2")
                            .arg(formatTime(position, rate))
                            .arg(formatTime(total, rate)));

    if (position >= total && playerSt.load() == PAUSE && timer)
        timer->stop();
}
