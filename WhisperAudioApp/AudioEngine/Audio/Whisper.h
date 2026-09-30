#pragma once
#include "AudioEngine/Math/WavHeader.h"
#include "Spectrogram.h"
#include "AudioEngine/DSP/FFT.h"


class Whisper
{
private:
	WavHeader header;
	short* data;
	int numOfS;
public:
	Whisper();
	Whisper(const char* fileName);
	Whisper(const Whisper& other);
	~Whisper();
	int getNumOfS() { return numOfS; }
	WavHeader getHeader() {
		return header;
	}
	void peakNormalize();
	Whisper& operator *= (float scale);
	void reverse();
	double getDurationInSeconds() const;
	Whisper hardSplice(double start, double end) const;
	void getAudioSlice(short* buffer, int startSample, int numSamples) const;
	Whisper operator+(const Whisper& other) const;
	Whisper& operator+=(const Whisper& other);
	Whisper& operator=(const Whisper& other);

	Spectrum decouple() const;
	Spectrum decoupleWindow(int fftSize) const;

	Spectrogram decoupleSTFT(int fftSize = 2048, int hopSize = 512) const;
	void setData(short* samples, int num, const WavHeader& ogHeader);
	Whisper extractChannel(int channel) const;//pulls one channel out as its own mono Whisper
	void setChannel(int channel, const Whisper& mono);//puts a mono Whisper back into one channel
	void save(const char* fileName) const;

	short* getDataPointer() { return data; }
};