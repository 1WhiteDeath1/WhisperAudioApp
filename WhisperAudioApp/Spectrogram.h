
#pragma once
#include "AudioEngine/DSP/FFT.h"
#include "AudioEngine/Math/WavHeader.h"

class Whisper;

class Spectrogram {
private:
    Spectrum* frames;
    int numFrames;
    int fftSize;
    int hopSize;
    int sampleRate;

public:
    Spectrogram(int numFrames, int fftSize, int hopSize, int sampleRate);
    Spectrogram(const Spectrogram& other);
    ~Spectrogram();
    Spectrogram& operator=(const Spectrogram& other);

    Spectrum& getFrame(int i) const { return frames[i]; }
    int getNumFrames() const { return numFrames; }
    int getFftSize()   const { return fftSize; }
    int getHopSize()   const { return hopSize; }
    int getSampleRate()const { return sampleRate; }

    Whisper synthesize(const WavHeader& header, int originalLength = 0) const;//originalLength cuts the extra padding off the end (0 = keep everything)
};