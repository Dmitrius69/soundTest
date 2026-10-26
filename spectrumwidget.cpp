#include "spectrumwidget.h"

#include <QFontMetrics>
#include <QHideEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QShowEvent>
#include <QTimer>
#include <QtGlobal>

#include <cmath>

namespace {

const double kFreqMin = 20.0;
const double kFreqMax = 20000.0;

inline float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

inline qreal frequencyToX(double hz, const QRectF &plot)
{
    if (hz < kFreqMin) hz = kFreqMin;
    if (hz > kFreqMax) hz = kFreqMax;
    const double t = std::log(hz / kFreqMin) / std::log(kFreqMax / kFreqMin);
    return plot.left() + t * plot.width();
}

QString axisLabel(double hz)
{
    if (hz >= 1000.0)
        return QString("%1k").arg(hz / 1000.0, 0, 'g', 3);
    return QString("%1").arg(hz, 0, 'g', 3);
}

QString formatHz(float hz)
{
    if (hz <= 0.0f)
        return QString::fromUtf8("—");
    if (hz >= 1000.0f)
        return QString::fromUtf8("%1 кГц")
                .arg(double(hz) / 1000.0, 0, 'f', hz >= 10000.0f ? 1 : 2);
    return QString::fromUtf8("%1 Гц").arg(int(hz + 0.5f));
}

int textWidth(const QFontMetrics &metrics, const QString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return metrics.horizontalAdvance(text);
#else
    return metrics.width(text);
#endif
}

QColor mixColor(const QColor &a, const QColor &b, float t)
{
    const float k = clamp01(t);
    return QColor(int(a.red()   + (b.red()   - a.red())   * k),
                  int(a.green() + (b.green() - a.green()) * k),
                  int(a.blue()  + (b.blue()  - a.blue())  * k));
}

// Catmull-Rom -> кубические Безье: гладкая кривая без изломов.
QPainterPath smoothPath(const QVector<QPointF> &points)
{
    QPainterPath path;
    if (points.isEmpty())
        return path;

    path.moveTo(points.first());
    if (points.size() == 1)
        return path;

    for (int i = 0; i < points.size() - 1; ++i)
    {
        const QPointF p0 = (i > 0) ? points.at(i - 1) : points.at(i);
        const QPointF p1 = points.at(i);
        const QPointF p2 = points.at(i + 1);
        const QPointF p3 = (i + 2 < points.size()) ? points.at(i + 2) : p2;

        const QPointF c1 = p1 + (p2 - p0) / 6.0;
        const QPointF c2 = p2 - (p3 - p1) / 6.0;
        path.cubicTo(c1, c2, p2);
    }
    return path;
}

} // namespace

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent),
      analyzer_(0),
      mode_(Response),
      title_(QString::fromUtf8("АЧХ")),
      infoRight_(QString::fromUtf8("—")),
      timer_(new QTimer(this)),
      lastTick_(0),
      lastSequence_(0),
      silentMs_(2000),
      phase_(0.0f),
      paused_(false),
      colorLow_(0, 229, 255),
      colorHigh_(168, 120, 255)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumHeight(110);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    display_.fill(0.0f, SpectrumAnalyzer::ResponseCount);
    peaks_.fill(0.0f, SpectrumAnalyzer::ResponseCount);
    peakHold_.fill(0, SpectrumAnalyzer::ResponseCount);

    clock_.start();
    connect(timer_, SIGNAL(timeout()), this, SLOT(animate()));
    timer_->start(16);
}

SpectrumWidget::~SpectrumWidget()
{
}

void SpectrumWidget::setAnalyzer(SpectrumAnalyzer *analyzer)
{
    analyzer_ = analyzer;
    lastSequence_ = 0;
    silentMs_ = 2000;
}

void SpectrumWidget::setMode(Mode mode)
{
    if (mode_ == mode)
        return;

    mode_ = mode;
    const int count = (mode_ == Bars) ? SpectrumAnalyzer::BandCount
                                      : SpectrumAnalyzer::ResponseCount;

    display_.fill(0.0f, count);
    peaks_.fill(0.0f, count);
    peakHold_.fill(0, count);

    if (mode_ == Bars)
    {
        title_ = QString::fromUtf8("СПЕКТР — распределение мощности по частотам");
        colorLow_ = QColor(0, 229, 255);
        colorHigh_ = QColor(255, 63, 164);
    }
    else
    {
        title_ = QString::fromUtf8("АЧХ — усреднённая амплитудно-частотная характеристика");
        colorLow_ = QColor(0, 229, 255);
        colorHigh_ = QColor(168, 120, 255);
    }

    infoRight_ = QString::fromUtf8("—");
    update();
}

void SpectrumWidget::setTitle(const QString &title)
{
    title_ = title;
    update();
}

void SpectrumWidget::setPaused(bool paused)
{
    if (paused_ == paused)
        return;

    paused_ = paused;
    update();
}

void SpectrumWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!timer_->isActive())
        timer_->start(16);
}

void SpectrumWidget::hideEvent(QHideEvent *event)
{
    timer_->stop();
    QWidget::hideEvent(event);
}

void SpectrumWidget::animate()
{
    if (paused_)
        return;

    const qint64 now = clock_.elapsed();
    float dt = float(now - lastTick_) / 16.0f;
    lastTick_ = now;
    if (dt < 0.25f) dt = 0.25f;
    if (dt > 4.0f)  dt = 4.0f;

    phase_ += 0.0045f * dt;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;

    bool fresh = false;

    if (analyzer_)
    {
        const SpectrumAnalyzer::Frame &frame = analyzer_->frame();
        fresh = (frame.seq != lastSequence_);

        if (fresh)
        {
            lastSequence_ = frame.seq;
            silentMs_ = 0;

            if (mode_ == Bars)
                infoRight_ = QString::fromUtf8("Пик: %1   ·   %2 дБFS")
                        .arg(formatHz(frame.peakHz))
                        .arg(double(frame.peakDb), 0, 'f', 1);
            else
                infoRight_ = QString::fromUtf8("RMS %1 дБFS   ·   пик %2")
                        .arg(double(frame.rmsDb), 0, 'f', 1)
                        .arg(formatHz(frame.peakHz));
        }
        else
        {
            silentMs_ += qint64(dt * 16.0f);
            if (silentMs_ > 900)
                infoRight_ = QString::fromUtf8("нет сигнала");
        }

        const QVector<float> &source = (mode_ == Bars) ? frame.bands : frame.response;
        const int count = qMin(source.size(), display_.size());
        // Между пачками декодирования (libmpg123 отдаёт буфер примерно
        // раз в 90 мс) данные не меняются. Не сбрасываем столбики в ноль,
        // иначе график «пульсирует» с частотой буферов. Спад начинается
        // только после 250 мс тишины.
        const bool active = (silentMs_ < 250);

        for (int i = 0; i < count; ++i)
        {
            const float target = (fresh || active) ? source.at(i) : 0.0f;
            float value = display_[i];

            if (target > value)
                value += (target - value) * 0.60f;       // быстрая атака
            else
                value += (target - value) * (active ? 0.18f : 0.055f); // плавный спад

            value = clamp01(value);
            display_[i] = value;

            if (mode_ == Bars)
            {
                if (value >= peaks_[i])
                {
                    peaks_[i] = value;
                    peakHold_[i] = 18;
                }
                else if (peakHold_[i] > 0)
                {
                    --peakHold_[i];
                }
                else
                {
                    peaks_[i] -= 0.020f * dt;
                    if (peaks_[i] < value) peaks_[i] = value;
                    if (peaks_[i] < 0.0f)  peaks_[i] = 0.0f;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < display_.size(); ++i)
            display_[i] *= 0.94f;
    }

    update();
}

QRectF SpectrumWidget::plotRect(const QRectF &bounds) const
{
    const qreal left   = 46.0;
    const qreal top    = 31.0;
    const qreal right  = 14.0;
    const qreal bottom = 24.0;

    return QRectF(bounds.left() + left,
                  bounds.top() + top,
                  qMax<qreal>(20.0, bounds.width() - left - right),
                  qMax<qreal>(20.0, bounds.height() - top - bottom));
}

QColor SpectrumWidget::levelColor(float value) const
{
    const QColor middle(96, 120, 255);
    if (value < 0.5f)
        return mixColor(colorLow_, middle, value / 0.5f);
    return mixColor(middle, colorHigh_, (value - 0.5f) / 0.5f);
}

void SpectrumWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QPainterPath shell;
    shell.addRoundedRect(bounds, 9.0, 9.0);
    painter.setClipPath(shell);

    renderBackground(painter, bounds);

    const QRectF plot = plotRect(bounds);
    renderGrid(painter, plot);

    painter.save();
    painter.setClipRect(plot.adjusted(-6.0, -2.0, 6.0, 2.0));
    if (mode_ == Bars)
        renderBars(painter, plot);
    else
        renderResponse(painter, plot);
    painter.restore();

    if (silentMs_ > 700 && !paused_)
        renderSilence(painter, plot);

    renderHeader(painter, bounds);

    painter.setClipping(false);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(48, 64, 84, 230), 1.0));
    painter.drawRoundedRect(bounds, 9.0, 9.0);
}

void SpectrumWidget::renderBackground(QPainter &painter, const QRectF &bounds)
{
    QLinearGradient background(bounds.topLeft(), bounds.bottomLeft());
    background.setColorAt(0.0, QColor(12, 16, 22));
    background.setColorAt(0.55, QColor(15, 21, 29));
    background.setColorAt(1.0, QColor(18, 25, 35));
    painter.fillRect(bounds, QBrush(background));

    QRadialGradient glow(QPointF(bounds.right() - bounds.width() * 0.18,
                                 bounds.top() + bounds.height() * 0.05),
                         bounds.width() * 0.72);
    glow.setColorAt(0.0, QColor(0, 150, 255, 30));
    glow.setColorAt(0.45, QColor(70, 40, 170, 14));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(bounds, QBrush(glow));
}

void SpectrumWidget::renderGrid(QPainter &painter, const QRectF &plot)
{
    QFont small = font();
    small.setPointSizeF(7.5);
    small.setBold(false);

    // Горизонтальная сетка, дБFS.
    for (int db = 0; db >= -72; db -= 12)
    {
        const float level = (float(db) + 78.0f) / 78.0f;
        const qreal y = plot.bottom() - qreal(level) * plot.height();

        const int alpha = (db == 0) ? 150 : 72;
        painter.setPen(QPen(QColor(62, 82, 105, alpha), 1.0));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));

        painter.setFont(small);
        painter.setPen(QColor(112, 134, 160, 205));
        painter.drawText(QRectF(plot.left() - 42.0, y - 8.0, 34.0, 16.0),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(db));
    }

    painter.setFont(small);
    painter.setPen(QColor(92, 112, 136, 180));
    painter.drawText(QRectF(plot.left() - 42.0, plot.top() - 15.0, 34.0, 14.0),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString::fromUtf8("дБ"));

    // Вертикальная сетка, логарифмическая шкала частот.
    static const double ticks[] = { 20.0, 50.0, 100.0, 200.0, 500.0,
                                    1000.0, 2000.0, 5000.0, 10000.0, 20000.0 };

    const int tickCount = int(sizeof(ticks) / sizeof(ticks[0]));
    for (int i = 0; i < tickCount; ++i)
    {
        const double hz = ticks[i];
        const qreal x = frequencyToX(hz, plot);

        painter.setPen(QPen(QColor(52, 70, 92, 90), 1.0));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));

        painter.setFont(small);
        painter.setPen(QColor(112, 134, 160, 200));

        QRectF label(x - 26.0, plot.bottom() + 3.0, 52.0, 16.0);
        if (i == 0)
            label.moveLeft(plot.left());
        else if (i == tickCount - 1)
            label.moveRight(plot.right());

        painter.drawText(label, Qt::AlignHCenter | Qt::AlignTop, axisLabel(hz));
    }

    painter.setPen(QPen(QColor(76, 98, 124, 200), 1.0));
    painter.drawLine(QPointF(plot.left(), plot.bottom()),
                     QPointF(plot.right(), plot.bottom()));
}

void SpectrumWidget::renderBars(QPainter &painter, const QRectF &plot)
{
    const int count = display_.size();
    if (count <= 0)
        return;

    // Лёгкое «свечение пола» под столбиками.
    QLinearGradient floorGlow(plot.left(), plot.bottom() - 46.0,
                              plot.left(), plot.bottom());
    floorGlow.setColorAt(0.0, QColor(0, 0, 0, 0));
    floorGlow.setColorAt(1.0, QColor(0, 180, 255, 18));
    painter.fillRect(QRectF(plot.left(), plot.bottom() - 46.0,
                           plot.width(), 46.0),
                     QBrush(floorGlow));

    const qreal bandWidth = plot.width() / qreal(count);
    const qreal gap = qMin<qreal>(3.0, bandWidth * 0.24);
    const qreal barWidth = qMax<qreal>(1.0, bandWidth - gap);

    int   loudest = -1;
    float loudestValue = 0.0f;

    for (int i = 0; i < count; ++i)
    {
        const float value = clamp01(display_.at(i));
        if (value > loudestValue)
        {
            loudestValue = value;
            loudest = i;
        }

        if (value < 0.002f)
            continue;

        const qreal x = plot.left() + qreal(i) * bandWidth + gap * 0.5;
        const qreal height = qMax<qreal>(2.0, qreal(value) * plot.height());
        const qreal y = plot.bottom() - height;

        const QColor color = levelColor(value);

        QLinearGradient gradient(QPointF(x, plot.bottom()), QPointF(x, y));
        QColor bottom = color;
        bottom.setAlpha(28);
        QColor middle = color;
        middle.setAlpha(185);
        gradient.setColorAt(0.0, bottom);
        gradient.setColorAt(0.60, middle);
        gradient.setColorAt(1.0, color.lighter(140));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(gradient));
        painter.drawRoundedRect(QRectF(x, y, barWidth, height), 2.0, 2.0);

        // Яркая «шапка» столбика.
        painter.setPen(QPen(QColor(255, 255, 255, 115), 1.0));
        painter.drawLine(QPointF(x + 0.5, y), QPointF(x + barWidth - 0.5, y));

        // Маркер удержания пика — короткий штрих над столбиком.
        const float peak = clamp01(peaks_.value(i));
        if (peak > value + 0.008f && peak > 0.004f)
        {
            const qreal peakY = plot.bottom() - qreal(peak) * plot.height();
            QColor peakColor = color.lighter(150);
            peakColor.setAlpha(175);
            painter.setPen(QPen(peakColor, 2.0, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(x + barWidth * 0.5 - 2.5, peakY),
                             QPointF(x + barWidth * 0.5 + 2.5, peakY));
        }
    }

    // Мягкая подсветка самого громкого столбика.
    if (loudest >= 0 && loudestValue > 0.05f)
    {
        const qreal cx = plot.left() + (qreal(loudest) + 0.5) * bandWidth;
        const qreal cy = plot.bottom() - qreal(loudestValue) * plot.height();

        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
        QRadialGradient glow(QPointF(cx, cy), 46.0);
        glow.setColorAt(0.0, QColor(255, 255, 255, 70));
        glow.setColorAt(0.35, QColor(0, 229, 255, 40));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(glow));
        painter.drawEllipse(QPointF(cx, cy), 46.0, 46.0);
        painter.restore();
    }
}

void SpectrumWidget::renderResponse(QPainter &painter, const QRectF &plot)
{
    const int count = display_.size();
    if (count < 2)
        return;

    QVector<QPointF> points;
    points.reserve(count);

    for (int i = 0; i < count; ++i)
    {
        const qreal x = plot.left() + qreal(i) / qreal(count - 1) * plot.width();
        const qreal y = plot.bottom() - qreal(clamp01(display_.at(i))) * plot.height();
        points.append(QPointF(x, y));
    }

    const QPainterPath curve = smoothPath(points);

    float maximum = 0.0f;
    for (int i = 0; i < count; ++i)
        maximum = qMax(maximum, display_.at(i));
    if (maximum < 0.015f)
        return;

    // Заливка под кривой.
    QPainterPath area = curve;
    area.lineTo(plot.right(), plot.bottom());
    area.lineTo(plot.left(), plot.bottom());
    area.closeSubpath();

    QLinearGradient areaGradient(QPointF(0.0, plot.top()), QPointF(0.0, plot.bottom()));
    QColor top = colorLow_;
    top.setAlpha(95);
    QColor middle = colorHigh_;
    middle.setAlpha(26);
    areaGradient.setColorAt(0.0, top);
    areaGradient.setColorAt(0.55, middle);
    areaGradient.setColorAt(1.0, QColor(0, 0, 0, 0));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(areaGradient));
    painter.drawPath(area);

    // Свечение вокруг линии.
    painter.setBrush(Qt::NoBrush);
    QColor glowColor = colorLow_;
    glowColor.setAlpha(34);
    painter.setPen(QPen(glowColor, 9.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(curve);

    glowColor.setAlpha(78);
    painter.setPen(QPen(glowColor, 4.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(curve);

    // Основная линия с градиентом по частоте.
    QLinearGradient lineGradient(plot.left(), 0.0, plot.right(), 0.0);
    lineGradient.setColorAt(0.0, colorLow_);
    lineGradient.setColorAt(0.5, QColor(92, 180, 255));
    lineGradient.setColorAt(1.0, colorHigh_);

    painter.setPen(QPen(QBrush(lineGradient), 2.2,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(curve);

    // Бегущая «сканирующая» точка по кривой.
    if (silentMs_ < 900)
    {
        const qreal scanX = plot.left() + qreal(phase_) * plot.width();
        const QRectF scanRect(scanX - 32.0, plot.top(), 64.0, plot.height());
        QLinearGradient scan(scanRect.topLeft(), scanRect.topRight());
        scan.setColorAt(0.0, QColor(180, 230, 255, 0));
        scan.setColorAt(0.5, QColor(180, 230, 255, 28));
        scan.setColorAt(1.0, QColor(180, 230, 255, 0));
        painter.fillRect(scanRect, QBrush(scan));

        const float position = phase_ * float(count - 1);
        const int index = qBound(0, int(position), count - 1);
        const qreal y = plot.bottom() - qreal(clamp01(display_.at(index))) * plot.height();

        QRadialGradient dot(QPointF(scanX, y), 15.0);
        dot.setColorAt(0.0, QColor(255, 255, 255, 210));
        dot.setColorAt(0.35, QColor(0, 229, 255, 90));
        dot.setColorAt(1.0, QColor(0, 0, 0, 0));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(dot));
        painter.drawEllipse(QPointF(scanX, y), 15.0, 15.0);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(255, 255, 255, 225), 1.8));
        painter.drawEllipse(QPointF(scanX, y), 2.6, 2.6);
    }
}

void SpectrumWidget::renderHeader(QPainter &painter, const QRectF &bounds)
{
    QFont titleFont = font();
    titleFont.setPointSizeF(9.5);
    titleFont.setBold(true);

    QFontMetrics titleMetrics(titleFont);
    const qreal titleWidth = bounds.width() * 0.62;

    QRectF titleRect(bounds.left() + 18.0, bounds.top() + 5.0,
                     titleWidth, 21.0);

    painter.setFont(titleFont);
    painter.setPen(QColor(216, 230, 244));
    painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                     titleMetrics.elidedText(title_, Qt::ElideRight,
                                             int(titleRect.width())));

    // Акцентная черта перед заголовком.
    painter.setPen(Qt::NoPen);
    painter.setBrush(colorLow_);
    painter.drawRoundedRect(QRectF(bounds.left() + 9.0, bounds.top() + 10.0,
                                   3.0, 11.0), 1.5, 1.5);

    QFont infoFont = font();
    infoFont.setPointSizeF(8.5);
    infoFont.setBold(false);

    QFontMetrics infoMetrics(infoFont);
    const qreal infoWidth = bounds.width() * 0.34;

    QRectF infoRect(bounds.right() - 14.0 - infoWidth, bounds.top() + 5.0,
                    infoWidth, 21.0);

    painter.setFont(infoFont);
    painter.setPen(QColor(124, 148, 176));
    painter.drawText(infoRect, Qt::AlignRight | Qt::AlignVCenter,
                     infoMetrics.elidedText(infoRight_, Qt::ElideRight,
                                            int(infoRect.width())));
}

void SpectrumWidget::renderSilence(QPainter &painter, const QRectF &plot)
{
    QFont silentFont = font();
    silentFont.setPointSizeF(10.5);
    silentFont.setBold(true);
    silentFont.setLetterSpacing(QFont::AbsoluteSpacing, 3.0);

    const QString text = QString::fromUtf8("НЕТ СИГНАЛА");
    QFontMetrics metrics(silentFont);
    const int width = textWidth(metrics, text) + 44;
    const int height = 30;

    QRectF box(plot.center().x() - width / 2.0,
               plot.center().y() - height / 2.0,
               width, height);

    painter.setPen(QPen(QColor(110, 140, 175, 60), 1.0));
    painter.setBrush(QColor(13, 19, 28, 170));
    painter.drawRoundedRect(box, 8.0, 8.0);

    painter.setFont(silentFont);
    painter.setPen(QColor(130, 158, 188, 135));
    painter.drawText(box, Qt::AlignCenter, text);
}
