#include "spectrumanalyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

const double kPi = 3.14159265358979323846;
const float  kFreqMin = 20.0f;
const float  kFreqMax = 20000.0f;

inline float clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

inline float logFrequency(float t)
{
    return kFreqMin * std::pow(kFreqMax / kFreqMin, t);
}

// Итеративное БПФ по основанию 2 (radix-2), на месте.
void fft(std::vector<float> &re, std::vector<float> &im)
{
    const int n = int(re.size());
    if (n < 2) return;

    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1)
    {
        const double angle = -2.0 * kPi / double(len);
        const float  wr = float(std::cos(angle));
        const float  wi = float(std::sin(angle));
        const int    half = len >> 1;

        for (int start = 0; start < n; start += len)
        {
            float cr = 1.0f;
            float ci = 0.0f;
            for (int k = 0; k < half; ++k)
            {
                const int   a = start + k;
                const int   b = a + half;
                const float vr = re[b] * cr - im[b] * ci;
                const float vi = re[b] * ci + im[b] * cr;
                const float ur = re[a];
                const float ui = im[a];

                re[a] = ur + vr;
                im[a] = ui + vi;
                re[b] = ur - vr;
                im[b] = ui - vi;

                const float nextCr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nextCr;
            }
        }
    }
}

} // namespace

SpectrumAnalyzer::Frame::Frame()
    : peakHz(0.0f),
      peakDb(-120.0f),
      rmsDb(-120.0f),
      seq(0)
{
}

SpectrumAnalyzer::SpectrumAnalyzer()
    : ring_(RingSize, 0.0f),
      writePosition_(0),
      producerPosition_(0),
      resetRequested_(false),
      sampleRate_(44100),
      lastAnalyzed_(0),
      window_(FftSize),
      fftReal_(FftSize),
      fftImag_(FftSize),
      amplitudes_(FftSize / 2 + 1)
{
    frame_.bands.fill(0.0f, BandCount);
    frame_.response.fill(0.0f, ResponseCount);
    responseAverage_.fill(0.0f, ResponseCount);

    for (int i = 0; i < FftSize; ++i)
        window_[i] = float(0.5 - 0.5 * std::cos(2.0 * kPi * i / double(FftSize - 1)));
}

void SpectrumAnalyzer::setSampleRate(int rate)
{
    if (rate < 4000) rate = 44100;
    sampleRate_.store(rate, std::memory_order_relaxed);
}

int SpectrumAnalyzer::sampleRate() const
{
    return sampleRate_.load(std::memory_order_relaxed);
}

void SpectrumAnalyzer::reset()
{
    // Вызывать при остановленном потоке воспроизведения.
    lastAnalyzed_ = writePosition_.load(std::memory_order_acquire);
    std::fill(ring_.begin(), ring_.end(), 0.0f);

    responseAverage_.fill(0.0f, ResponseCount);
    frame_.bands.fill(0.0f, BandCount);
    frame_.response.fill(0.0f, ResponseCount);
    frame_.peakHz = 0.0f;
    frame_.peakDb = -120.0f;
    frame_.rmsDb = -120.0f;
    ++frame_.seq;

    resetRequested_.store(true, std::memory_order_release);
}

void SpectrumAnalyzer::pushMono(float value)
{
    ring_[producerPosition_ & (RingSize - 1)] = value;
    ++producerPosition_;
}

void SpectrumAnalyzer::pushPcm16(const void *data, size_t frames, int channels)
{
    if (!data || frames == 0) return;
    if (resetRequested_.exchange(false, std::memory_order_acq_rel))
    {
        producerPosition_ = 0;
        writePosition_.store(0, std::memory_order_release);
    }

    if (channels < 1) channels = 1;
    const short *samples = static_cast<const short *>(data);
    const float  scale = 1.0f / (32768.0f * float(channels));

    for (size_t i = 0; i < frames; ++i)
    {
        int sum = 0;
        for (int c = 0; c < channels; ++c)
            sum += int(samples[i * size_t(channels) + size_t(c)]);
        pushMono(float(sum) * scale);
    }

    writePosition_.store(producerPosition_, std::memory_order_release);
}

void SpectrumAnalyzer::pushPcm8(const void *data, size_t frames, int channels)
{
    if (!data || frames == 0) return;
    if (resetRequested_.exchange(false, std::memory_order_acq_rel))
    {
        producerPosition_ = 0;
        writePosition_.store(0, std::memory_order_release);
    }

    if (channels < 1) channels = 1;
    const unsigned char *samples = static_cast<const unsigned char *>(data);
    const float scale = 1.0f / (128.0f * float(channels));

    for (size_t i = 0; i < frames; ++i)
    {
        int sum = 0;
        for (int c = 0; c < channels; ++c)
            sum += int(samples[i * size_t(channels) + size_t(c)]) - 128;
        pushMono(float(sum) * scale);
    }

    writePosition_.store(producerPosition_, std::memory_order_release);
}

float SpectrumAnalyzer::bandFrequency(int index)
{
    const float t = (float(index) + 0.5f) / float(BandCount);
    return logFrequency(t);
}

float SpectrumAnalyzer::responseFrequency(int index)
{
    const float t = float(index) / float(ResponseCount - 1);
    return logFrequency(t);
}

float SpectrumAnalyzer::normalizedLevel(float amplitude)
{
    if (amplitude <= 1.0e-7f) return 0.0f;
    const float db = 20.0f * std::log10(amplitude);
    // шкала -78..0 дБFS -> 0..1
    return clamp01((db + 78.0f) / 78.0f);
}

float SpectrumAnalyzer::binAmplitude(const std::vector<float> &amplitudes, float bin)
{
    const int last = int(amplitudes.size()) - 1;
    if (last < 1) return 0.0f;
    if (bin <= 1.0f) return amplitudes[1];
    if (bin >= float(last)) return amplitudes[last];

    const int   k = int(bin);
    const float f = bin - float(k);
    return amplitudes[k] * (1.0f - f) + amplitudes[k + 1] * f;
}

const SpectrumAnalyzer::Frame &SpectrumAnalyzer::frame()
{
    const size_t position = writePosition_.load(std::memory_order_acquire);
    if (position != lastAnalyzed_)
    {
        analyze(position);
        lastAnalyzed_ = position;
    }
    return frame_;
}

void SpectrumAnalyzer::analyze(size_t position)
{
    if (position < size_t(FftSize)) return;

    const size_t start = position - FftSize;
    double sumSquares = 0.0;

    for (int i = 0; i < FftSize; ++i)
    {
        const float sample = ring_[(start + size_t(i)) & (RingSize - 1)];
        sumSquares += double(sample) * double(sample);
        fftReal_[i] = sample * window_[i];
        fftImag_[i] = 0.0f;
    }

    const float rms = float(std::sqrt(sumSquares / double(FftSize)));

    fft(fftReal_, fftImag_);

    // Нормировка: амплитуда синусоиды 0 dBFS даёт 1.0
    // (множитель 2 компенсирует отрицательные частоты, 0.5 — когерентное
    // усиление окна Ханна).
    const float norm = 2.0f / (float(FftSize) * 0.5f);
    const int   half = FftSize / 2;

    for (int k = 0; k <= half; ++k)
    {
        const float re = fftReal_[k];
        const float im = fftImag_[k];
        amplitudes_[k] = std::sqrt(re * re + im * im) * norm;
    }
    amplitudes_[0] = 0.0f;

    const float sampleRate = float(sampleRate_.load(std::memory_order_relaxed));

    // Полосы (логарифмическая шкала 20 Гц .. 20 кГц)
    for (int b = 0; b < BandCount; ++b)
    {
        const float hz  = bandFrequency(b);
        const float bin = hz * float(FftSize) / sampleRate;
        frame_.bands[b] = normalizedLevel(binAmplitude(amplitudes_, bin));
    }

    // АЧХ: та же спектральная мощность, но с экспоненциальным усреднением
    // по времени. Быстрая атака, медленный спад — кривая не «дрожит».
    for (int i = 0; i < ResponseCount; ++i)
    {
        const float hz  = responseFrequency(i);
        const float bin = hz * float(FftSize) / sampleRate;
        const float level = normalizedLevel(binAmplitude(amplitudes_, bin));

        float average = responseAverage_[i];
        const float coeff = (level > average) ? 0.30f : 0.055f;
        average += (level - average) * coeff;
        responseAverage_[i] = average;
        frame_.response[i] = average;
    }

    // Пик спектра с параболической интерполяцией по трём бинам.
    int   peakBin = 1;
    float peakAmp = 0.0f;
    for (int k = 1; k < half; ++k)
    {
        if (amplitudes_[k] > peakAmp)
        {
            peakAmp = amplitudes_[k];
            peakBin = k;
        }
    }

    float peakHz = float(peakBin) * sampleRate / float(FftSize);
    if (peakBin > 1 && peakBin < half - 1)
    {
        const float a = amplitudes_[peakBin - 1];
        const float b = amplitudes_[peakBin];
        const float c = amplitudes_[peakBin + 1];
        const float denominator = a - 2.0f * b + c;

        if (std::fabs(denominator) > 1.0e-12f)
        {
            const float delta = 0.5f * (a - c) / denominator;
            if (std::fabs(delta) < 1.0f)
                peakHz = (float(peakBin) + delta) * sampleRate / float(FftSize);
        }
    }

    frame_.peakHz = peakHz;
    frame_.peakDb = 20.0f * std::log10(std::max(peakAmp, 1.0e-7f));
    frame_.rmsDb  = 20.0f * std::log10(std::max(rms, 1.0e-7f));
    ++frame_.seq;
}
