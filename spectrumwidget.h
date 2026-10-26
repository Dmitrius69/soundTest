#ifndef SPECTRUMWIDGET_H
#define SPECTRUMWIDGET_H

#include <QColor>
#include <QElapsedTimer>
#include <QString>
#include <QVector>
#include <QWidget>

#include "spectrumanalyzer.h"

class QPainter;
class QTimer;

/*
 * Анимированный график спектра.
 *
 * Mode::Bars     — распределение мощности по частотам (столбики с пиками);
 * Mode::Response — усреднённая АЧХ (плавная кривая с заливкой).
 *
 * Оба режима рисуются вручную через QPainter: сглаживание, градиенты,
 * подсветка и плавная анимация атаки/спада. Данные берутся из
 * SpectrumAnalyzer, который наполняется потоком воспроизведения.
 */
class SpectrumWidget : public QWidget
{
    Q_OBJECT
public:
    enum Mode { Bars, Response };

    explicit SpectrumWidget(QWidget *parent = 0);
    ~SpectrumWidget();

    void setAnalyzer(SpectrumAnalyzer *analyzer);
    void setMode(Mode mode);
    Mode mode() const { return mode_; }
    void setTitle(const QString &title);

    // Пауза: график «замерзает» на последнем кадре вместо того, чтобы
    // плавно уходить в ноль и показывать «НЕТ СИГНАЛА».
    void setPaused(bool paused);
    bool isPaused() const { return paused_; }

protected:
    void paintEvent(QPaintEvent *event);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);

private slots:
    void animate();

private:
    QRectF  plotRect(const QRectF &bounds) const;
    void    renderBackground(QPainter &painter, const QRectF &bounds);
    void    renderGrid(QPainter &painter, const QRectF &plot);
    void    renderBars(QPainter &painter, const QRectF &plot);
    void    renderResponse(QPainter &painter, const QRectF &plot);
    void    renderHeader(QPainter &painter, const QRectF &bounds);
    void    renderSilence(QPainter &painter, const QRectF &plot);
    QColor  levelColor(float value) const;

    SpectrumAnalyzer *analyzer_;
    Mode              mode_;
    QString           title_;
    QString           infoRight_;

    QVector<float> display_;
    QVector<float> peaks_;
    QVector<int>   peakHold_;

    QTimer        *timer_;
    QElapsedTimer  clock_;
    qint64         lastTick_;
    quint64        lastSequence_;
    qint64         silentMs_;
    float          phase_;
    bool           paused_;

    QColor colorLow_;
    QColor colorHigh_;
};

#endif // SPECTRUMWIDGET_H
