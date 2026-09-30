#pragma once
#include "AudioEngine/Math/WavHeader.h"
#include "AudioEngine/Math/Complex.h"


class Spectrogram;
class Filter;
class Whisper;
void reorder(Complex* data, Complex* temp, int size);
void reorderBitReversal(Complex* data, int size);
void butterfly(Complex* data, int size, bool forward);

Complex* buildTwiddleTable(int size);  // caller owns the memory, must delete[]

// New overload of butterfly that accepts the table
void butterfly(Complex* data, int size, bool forward, const Complex* twiddleTable);



class Spectrum
{
private:
	Complex* frequencyBins;
	int numOfB;
	int ogSize;
	int sampleRate;
public:
	Spectrum(Complex* freqBins = nullptr, int binCount = 0, int ogSize = 0, int sampleRate = 0);
	Spectrum(const Spectrum& other);
	~Spectrum();

	int getNumOfB() {
		return numOfB;
	}
	int getSampleRate() {
		return sampleRate;
	}
	Complex* getComplexData() {
		return frequencyBins;
	}
	void filterOn(const Filter& filter);



	void performFFT(const float* timeDomainInput, int size);
	Whisper inverseFFT(const WavHeader& ogHeader) const;
	
	Spectrum& operator=(const Spectrum& other) {
		if (this == &other) return *this; // self-assignment guard

		delete[] frequencyBins; // free existing data

		numOfB = other.numOfB;
		ogSize = other.ogSize;
		sampleRate = other.sampleRate;

		if (other.frequencyBins == nullptr) {
			frequencyBins = nullptr;
		}
		else {
			frequencyBins = new Complex[numOfB];
			for (int i = 0; i < numOfB; i++)
				frequencyBins[i] = other.frequencyBins[i];
		}
		return *this;
	}

};

double* buildNoiseProfile(const Spectrogram& sg, int fftSize);
void spectralSubtract(Spectrum& frame, double* noiseProfile, int numBins, double strength);
void spectralZero(Spectrum& frame, int numBins, double strength);