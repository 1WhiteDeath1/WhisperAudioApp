#include "FFT.h"
#include "AudioEngine/Math/Filter.h"
#include "AudioEngine/Audio/Whisper.h"
#include <iostream>
using namespace std;

void reorder(Complex* data, Complex* temp, int size) {//using recursion to recursively divide the data into even and odd set repeatedly whiele changing positions of them to e in pairs
	if (size <= 1) return;

	int newSize = size / 2;

	for (int i = 0; i < newSize;i++) {
		//sparating evens and odds in data
		temp[i] = data[2 * i];
		temp[i + newSize] = data[1 + (2 * i)];
	}
	//now reorder the data based on even and oddd
	for (int i = 0; i < size; i++) {
		data[i] = temp[i];
	}

	reorder(data, temp, newSize);
	reorder(data + newSize, temp + newSize, newSize);
}

void reorderBitReversal(Complex* data, int size) {
	int bits = 0;
	for (int n = size; n > 1; n >>= 1) bits++;

	for (int i = 0; i < size; i++) {
		int j = 0;
		for (int b = 0; b < bits; b++)
			if (i & (1 << b)) j |= (1 << (bits - 1 - b));

		if (i < j) {
			Complex tmp = data[i];
			data[i] = data[j];
			data[j] = tmp;
		}
	}
}
void butterfly(Complex* data, int size, bool forward) {
	//uses tuckey cooley algorithm, twiddle factors as rotations for combining smaller DFts
	int numOfShifts = 0;
	int tempSize = size;
	while (tempSize > 1) {
		numOfShifts += 1;
		tempSize /= 2;
	}

	for (int i = 1; i <= numOfShifts;i++) {
		int subLength = pow(2, i);
		int tempSize = subLength / 2;
		double angle = 2 * 3.14159265358979 / subLength;
		if (forward) angle = -angle;
		Complex rotation(cos(angle), sin(angle));

		for (int x = 0;x < size; x += subLength) {
			//initialize cmplx num
			Complex wave(1, 0);

			for (int j = 0;j < tempSize;j++) {
				Complex temp1 = data[x + j];
				Complex temp2 = data[x + j + tempSize] * wave;

				data[x + j] = temp1 + temp2;
				data[x + j + tempSize] = temp1 - temp2;
				wave = wave * rotation;
			}
		}
	}
}

Complex* buildTwiddleTable(int size) {
	Complex* table = new Complex[size / 2];
	for (int k = 0; k < size / 2; k++) {
		double angle = -2.0 * 3.14159265358979323846 * k / size;
		table[k] = Complex(cos(angle), sin(angle));
	}
	return table;
}

void butterfly(Complex* data, int size, bool forward, const Complex* twiddleTable) {
	int numStages = 0;
	for (int n = size; n > 1; n >>= 1) numStages++;

	for (int stage = 1; stage <= numStages; stage++) {
		int subLength = 1 << stage;
		int half = subLength >> 1;

		for (int x = 0; x < size; x += subLength) {
			for (int j = 0; j < half; j++) {
				int tableIdx = j * (size / subLength);
				Complex w = twiddleTable[tableIdx];
				if (!forward) w.setImaginary(-w.getImaginary());

				Complex t1 = data[x + j];
				Complex t2 = data[x + j + half] * w;

				data[x + j] = t1 + t2;
				data[x + j + half] = t1 - t2;
			}
		}
	}
}


Spectrum::Spectrum(Complex* freqBins, int binCount, int ogSize, int sampleRate) {
	this->numOfB = binCount;
	this->ogSize = ogSize;
	this->sampleRate = sampleRate;
	if (freqBins == nullptr) { this->frequencyBins = nullptr; }
	else {
		this->frequencyBins = new Complex[numOfB];
		for (int i = 0; i < numOfB; i++) {
			frequencyBins[i] = freqBins[i];
		}
	}
}

Spectrum::Spectrum(const Spectrum& other) {
	numOfB = other.numOfB;
	ogSize = other.ogSize;
	sampleRate = other.sampleRate;
	frequencyBins = new Complex[numOfB];
	for (int i = 0; i < numOfB; i++)
		frequencyBins[i] = other.frequencyBins[i];
}
Spectrum::~Spectrum() {
	delete[] frequencyBins;
}
//applies filter by multiplying weights
void Spectrum::filterOn(const Filter& filter) {
	if (frequencyBins == nullptr || filter.getWeights() == nullptr) {
		return;
	}
	for (int i = 0; i < numOfB;i++) {
		double r = frequencyBins[i].getReal() * filter.getWeights()[i];
		double img = frequencyBins[i].getImaginary() * filter.getWeights()[i];
		frequencyBins[i].setReal(r);
		frequencyBins[i].setImaginary(img);
	}
}
//this function does the FFT using standard algorithm of cooley tuckey by using reorder & butterfly function
void FFT(Complex* temp, int n) {

	//reordering 
	Complex* tempTemp = new Complex[n];
	for (int i = 0; i < n;i++) {
		tempTemp[i] = temp[i];
	}
	reorder(temp, tempTemp, n);
	butterfly(temp, n, true);

	delete[] tempTemp;
}

void spectralSubtract(Spectrum& frame,double* noiseProfile,int numBins,double strength) {
	for (int i = 0; i < numBins;i++) {
		double Ogmag = frame.getComplexData()[i].magnitude();
		double phase = frame.getComplexData()[i].phase();
		double mag = Ogmag - noiseProfile[i] * strength;
		mag = max(double(mag), 0.01 * Ogmag); // prevent negative or zero magnitude
	
		float nReal = mag * cos(phase);
		float nImag = mag * sin(phase);
		frame.getComplexData()[i].setReal(nReal);
		frame.getComplexData()[i].setImaginary(nImag);
	}
}
void spectralZero(Spectrum& frame,int numBins, double strength) {
	int hightest = 0;
	for (int i = 0; i < numBins; i++) {
		double currmag = frame.getComplexData()[i].magnitude();
		double highestmag = frame.getComplexData()[hightest].magnitude();
		if (currmag > highestmag) {
			hightest = i;
		}
	}
	double threshold = frame.getComplexData()[hightest].magnitude() * strength;
	for (int i = 0; i < numBins;i++) {
		double mag = frame.getComplexData()[i].magnitude();
		if (mag < threshold) {
			frame.getComplexData()[i].setReal(0);
			frame.getComplexData()[i].setImaginary(0);
		}
	}
}

double* buildNoiseProfile( const Spectrogram& sg, int fftSize) {
	double* profile = new double[fftSize]();
	if (sg.getNumFrames() == 0) return profile;//nothing to average, all zeros so subtracting does nothing
	for (int i = 0; i < sg.getNumFrames();i++) {
		for (int j = 0; j < fftSize; j++) {
			profile[j] += sg.getFrame(i).getComplexData()[j].magnitude();
		}
	}
	for (int i = 0; i < fftSize;i++) {
		profile[i] /= sg.getNumFrames();
	}

	return profile;
}

Whisper Spectrum::inverseFFT(const WavHeader& originalHeader) const {
	if (frequencyBins == nullptr || numOfB == 0) {
		return Whisper();
	}
	//for inversing , we take conjugate and then do FFT and then conjigate and divide by n for getting original data
	Complex* temp = new Complex[numOfB];
	for (int i = 0; i < numOfB;i++) {
		temp[i] = frequencyBins[i];
		temp[i] = temp[i].conjugate();
	}
	FFT(temp, numOfB);
	for (int i = 0; i < numOfB;i++) {
		temp[i] = temp[i].conjugate();
		temp[i].setReal(temp[i].getReal() / (double)numOfB);
		temp[i].setImaginary(temp[i].getImaginary() / (double)numOfB);

	}
	short* samples = new short[ogSize];
	for (int i = 0; i < ogSize;i++) {
		double num = temp[i].getReal();
		//hard clipping
		num = num > 32767 ? 32767 : num;
		num = num < -32767 ? -32767 : num;
		samples[i] = (short)num;
	}
	delete[] temp;

	Whisper r;
	r.setData(samples, ogSize, originalHeader);
	delete[] samples;
	return r;


}
