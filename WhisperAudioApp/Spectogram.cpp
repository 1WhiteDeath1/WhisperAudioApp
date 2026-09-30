#include "Spectrogram.h"
#include "AudioEngine/DSP/FFT.h"
#include "AudioEngine/Audio/Whisper.h"
#include "AudioEngine/Math/Complex.h"
#include <cmath>
#include <algorithm>

Spectrogram::Spectrogram(int numFrames, int fftSize, int hopSize, int sampleRate)
    : numFrames(numFrames), fftSize(fftSize), hopSize(hopSize), sampleRate(sampleRate) {
    frames = new Spectrum[numFrames];
}

Spectrogram::Spectrogram(const Spectrogram& other)
    : numFrames(other.numFrames), fftSize(other.fftSize),
    hopSize(other.hopSize), sampleRate(other.sampleRate) {
    frames = new Spectrum[numFrames];
    for (int i = 0; i < numFrames; i++)
        frames[i] = other.frames[i];
}

Spectrogram::~Spectrogram() {
    delete[] frames;
}

Spectrogram& Spectrogram::operator=(const Spectrogram& other) {
    if (this == &other) return *this;
    delete[] frames;
    numFrames = other.numFrames;
    fftSize = other.fftSize;
    hopSize = other.hopSize;
    sampleRate = other.sampleRate;
    frames = new Spectrum[numFrames];
    for (int i = 0; i < numFrames; i++)
        frames[i] = other.frames[i];
    return *this;
}

Whisper Spectrogram::synthesize(const WavHeader& header, int originalLength) const {
    if (numFrames <= 0) return Whisper();//nothing to put back together

    // Total output length
    int totalSamples = (numFrames - 1) * hopSize + fftSize;

    // Accumulation buffers
    double* outputBuf = new double[totalSamples]();   // () zero-initializes
    double* windowSum = new double[totalSamples]();

    // Hann window — periodic form, same as used in analysis
    double* hannWindow = new double[fftSize];
    for (int i = 0; i < fftSize; i++)
        hannWindow[i] = 0.5 * (1.0 - cos(2.0 * 3.14159265358979323846 * i / fftSize));

    for (int frame = 0; frame < numFrames; frame++) {
        // IFFT this frame back to time domain
        Whisper frameAudio = frames[frame].inverseFFT(header);
        short* frameSamples = frameAudio.getDataPointer();
        int frameLen = frameAudio.getNumOfS();

        int startSample = frame * hopSize;

        // Overlap-add with window squared (synthesis window = analysis window)
        for (int i = 0; i < fftSize && i < frameLen; i++) {
            double w = hannWindow[i];
            outputBuf[startSample + i] += frameSamples[i] * w;
            windowSum[startSample + i] += w * w;   // for normalization
        }
    }

    // the end is padded up to a full window, so cutting it back to the original length
    // (otherwise the audio would get a bit longer every time an effect is applied)
    int outLength = totalSamples;
    if (originalLength > 0 && originalLength < totalSamples) outLength = originalLength;

    // Normalize by window overlap sum, convert to short*
    short* finalSamples = new short[outLength];
    for (int i = 0; i < outLength; i++) {
        double normalized = (windowSum[i] > 1e-8) ? outputBuf[i] / windowSum[i] : 0.0;
        // Clamp to short range
        if (normalized > 32767.0) normalized = 32767.0;
        else if (normalized < -32768.0) normalized = -32768.0;
        finalSamples[i] = (short)normalized;
    }

    Whisper result;
    result.setData(finalSamples, outLength, header);

    delete[] outputBuf;
    delete[] windowSum;
    delete[] hannWindow;
    delete[] finalSamples;
    return result;
}