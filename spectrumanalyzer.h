#ifndef SPECTRUMANALYZER_H
#define SPECTRUMANALYZER_H

#include <QVector>
#include <atomic>
#include <cstddef>
#include <vector>

/*
 * Потокобезопасный анализатор спектра.
 *
 * Поток воспроизведения вызывает pushPcm16()/pushPcm8() и складывает
 * моно-семплы в lock-free кольцевой буфер. GUI-поток вызывает frame(),
 * который при появлении новых данных считает БПФ (окно Ханна) и отдаёт
 * готовый кадр: полосы спектра, усреднённую АЧХ, пик и RMS.
 */
class SpectrumAnalyzer
{
public:
    enum Constants {
        FftSize       = 4096,   // размер БПФ
        BandCount     = 72,     // число полос эквалайзерного вида
        ResponseCount = 160,    // число точек кривой АЧХ
        RingSize      = 1 << 15 // размер кольцевого буфера (семплы)
    };

    struct Frame
    {
        Frame();

        QVector<float> bands;    // BandCount значений 0..1
        QVector<float> response; // ResponseCount значений 0..1 (АЧХ)
        float   peakHz;          // частота максимальной составляющей
        float   peakDb;          // её уровень в дБFS
        float   rmsDb;           // СКЗ уровня сигнала
        quint64 seq;             // номер кадра, меняется при новых данных
    };

    SpectrumAnalyzer();

    void setSampleRate(int rate);
    int  sampleRate() const;

    // Сброс накопленной АЧХ и буфера (перед открытием нового файла).
    void reset();

    // Вызывается из потока воспроизведения.
    void pushPcm16(const void *data, size_t frames, int channels);
    void pushPcm8(const void *data, size_t frames, int channels);

    // Вызывается из GUI-потока. Пересчитывает спектр только при новых данных.
    const Frame &frame();

    static float bandFrequency(int index);     // центр полосы, Гц
    static float responseFrequency(int index); // точка кривой АЧХ, Гц

private:
    void  pushMono(float value);
    void  analyze(size_t position);
    static float normalizedLevel(float amplitude);
    static float binAmplitude(const std::vector<float> &amplitudes, float bin);

    std::vector<float> ring_;
    std::atomic<size_t> writePosition_;
    size_t producerPosition_;
    std::atomic<bool> resetRequested_;
    std::atomic<int> sampleRate_;
    size_t lastAnalyzed_;

    std::vector<float> window_;
    std::vector<float> fftReal_;
    std::vector<float> fftImag_;
    std::vector<float> amplitudes_;

    QVector<float> responseAverage_;
    Frame frame_;
};

#endif // SPECTRUMANALYZER_H
