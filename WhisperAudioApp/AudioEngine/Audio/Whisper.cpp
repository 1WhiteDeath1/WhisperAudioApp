#include "Whisper.h"
#include <fstream>
#include <iostream>
#include <cstring>
using namespace std;

Whisper::Whisper() {
	header = WavHeader();//all zeros so theres no random junk in the header
	data = nullptr;
	numOfS = 0;
}
Whisper::Whisper(const char* fName) {
	header = WavHeader();
	data = nullptr;
	numOfS = 0;

	ifstream File(fName, ios::binary);
	if (!File.is_open()) {
		cout << "couldnt open file: " << fName << endl;
		return;
	}
	//getting the file size first so we never try to read past the end
	File.seekg(0, ios::end);
	long long fileSize = File.tellg();
	File.seekg(0, ios::beg);

	//a wav file starts with RIFF, then a size, then WAVE
	char riffId[4];
	int riffSize = 0;
	char waveId[4];
	File.read(riffId, 4);
	File.read(reinterpret_cast<char*>(&riffSize), 4);
	File.read(waveId, 4);
	if (!File || memcmp(riffId, "RIFF", 4) != 0 || memcmp(waveId, "WAVE", 4) != 0) {
		cout << "not a valid wav file: " << fName << endl;
		return;
	}

	//going through the chunks one by one, anything we dont need (LIST etc) just gets skipped
	bool foundFmt = false;
	bool foundData = false;
	int dataSize = 0;
	while (!foundData) {
		char chunkId[4];
		int chunkSize = 0;
		File.read(chunkId, 4);
		File.read(reinterpret_cast<char*>(&chunkSize), 4);
		if (!File || chunkSize < 0) break;//ran out of file before finding the data

		if (memcmp(chunkId, "fmt ", 4) == 0) {
			if (chunkSize < 16) break;
			//audioFormat till bitsPerSample are 16 bytes in a row in the header
			File.read(reinterpret_cast<char*>(&header.audioFormat), 16);
			//fmt chunk can be bigger than 16 (extra bytes) so skipping those
			File.seekg((chunkSize - 16) + (chunkSize % 2), ios::cur);
			foundFmt = true;
		}
		else if (memcmp(chunkId, "data", 4) == 0) {
			if (!foundFmt) break;//data before fmt so we dont know how to read it
			dataSize = chunkSize;
			foundData = true;
		}
		else {
			//chunks always end on an even byte so odd sizes have 1 padding byte
			File.seekg(chunkSize + (chunkSize % 2), ios::cur);
		}
	}
	if (!foundFmt || !foundData) {
		cout << "couldnt find the format/data of the wav file" << endl;
		return;
	}
	if (header.audioFormat != 1 || header.numChannels < 1 || header.sampleRate <= 0 ||
		(header.bitsPerSample != 16 && header.bitsPerSample != 8)) {
		cout << "only 8 or 16 bit PCM wav files are supported" << endl;
		return;
	}
	//some files say the data is bigger than what is really in the file so cutting it down
	long long remaining = fileSize - (long long)File.tellg();
	if (dataSize > remaining) dataSize = (int)remaining;
	if (dataSize < 0) dataSize = 0;

	//filling the ids in ourselves so save() always writes a clean 44 byte header
	memcpy(header.chunkId, "RIFF", 4);
	memcpy(header.format, "WAVE", 4);
	memcpy(header.subchunk1Id, "fmt ", 4);
	memcpy(header.subchunk2Id, "data", 4);
	header.subchunk1Size = 16;

	//reading remaining data
	if (header.bitsPerSample == 16) {
		numOfS = dataSize / 2;
		unsigned char* temp = new unsigned char[numOfS * 2]();
		File.read(reinterpret_cast<char*>(temp), numOfS * 2);
		data = new short[numOfS];//as 16 bits= 2bytes
		for (int i = 0; i < numOfS;i++) {
			unsigned char x = temp[2 * i];
			unsigned char y = temp[2 * i + 1];
			short d = (short)(x | (y << 8));
			data[i] = d;
		}
		delete[] temp;
	}
	else {
		//8 bit so first converting to 16
		numOfS = dataSize;
		unsigned char* temp = new unsigned char[numOfS]();
		File.read(reinterpret_cast<char*>(temp), numOfS);
		data = new short[numOfS];
		for (int i = 0; i < numOfS;i++) {
			data[i] = (short)((temp[i] - 128) * 256);
		}
		delete[] temp;
		//now changing header to be now 16 bit
		header.bitsPerSample = 16;
	}
	//dropping leftover samples that dont make a full frame (one sample from every channel)
	numOfS = numOfS - (numOfS % header.numChannels);
	header.byteRate = header.sampleRate * header.numChannels * 2;
	header.blockAlign = header.numChannels * 2;
	header.subchunk2Size = numOfS * 2;
	header.chunkSize = header.subchunk2Size + 36;
	File.close();
}
Whisper::~Whisper() {
	delete[] data;
}

Whisper::Whisper(const Whisper& other) {
	header = other.header;
	numOfS = other.numOfS;
	if (numOfS > 0 && other.data != nullptr) {
		data = new short[numOfS];
		for (int i = 0; i < numOfS; i++)
			data[i] = other.data[i];
	}
	else {
		data = nullptr;
	}
}

void Whisper::peakNormalize() {
	short max = 0;
	for (int i = 0; i < numOfS;i++) {
		short abs = data[i] < 0 ? -data[i] : data[i];
		if (abs > max) {
			max = abs;
		}
	}
	if (max != 0) {
		double scale = 32767.0 / max;
		for (int i = 0; i < numOfS;i++) {
			double temp = data[i] * scale;
			temp = temp > 32767.0 ? 32767.0 : temp;
			temp = temp < -32767.0 ? -32767.0 : temp;
			data[i] = (short)temp;
		}
	}
}

Whisper& Whisper::operator*=(float factor) {
	for (int i = 0;i < numOfS;i++) {
		float newD = data[i] * factor;
		newD = newD > 32767 ? 32767 : newD;
		newD = newD < -32767 ? -32767 : newD;
		data[i] = (short)newD;
	}
	return *this;
}

void Whisper::reverse() {
	//samples are interleaved so whole frames get swapped, otherwise left and right would swap places too
	int channels = header.numChannels;
	if (channels < 1) channels = 1;
	int numFrames = numOfS / channels;
	for (int i = 0; i < numFrames / 2; i++) {
		for (int c = 0; c < channels; c++) {
			short temp = data[i * channels + c];
			data[i * channels + c] = data[(numFrames - 1 - i) * channels + c];
			data[(numFrames - 1 - i) * channels + c] = temp;
		}
	}
}

double Whisper::getDurationInSeconds() const {
	if (header.sampleRate == 0 || header.numChannels == 0) return 0.0;

	return (double)numOfS / (header.sampleRate * header.numChannels);
}

Whisper Whisper::hardSplice(double start, double end) const {
	int channels = header.numChannels;
	if (channels < 1) channels = 1;
	//working in frames first (one frame = one sample from every channel) so we never cut in the middle of a frame
	int startI = (int)(start * header.sampleRate) * channels;
	int endI = (int)(end * header.sampleRate) * channels;
	if (startI < 0) startI = 0;
	if (endI > numOfS) endI = numOfS;
	int length = endI - startI;


	Whisper buff;
	if (length <= 0) { return buff; }

	buff.header = header;
	buff.header.subchunk2Size = length * 2;
	buff.header.chunkSize = buff.header.subchunk2Size + 36;
	buff.numOfS = length;
	buff.data = new short[length];
	for (int i = 0; i < length; i++) {
		buff.data[i] = data[startI + i];
	}
	return buff;

}

void Whisper::getAudioSlice(short* outputBuffer, int startSample, int length) const {
	for (int i = 0; i < length; i++) {
		int index = startSample + i;
		if (index >= 0 && index < numOfS) {
			outputBuffer[i] = data[index];
		}
		else {
			outputBuffer[i] = 0;
		}
	}
}


Whisper Whisper::operator+(const Whisper& other) const {
	Whisper sum;
	sum.header = this->header;
	sum.numOfS = this->numOfS > other.numOfS ? this->numOfS : other.numOfS;
	sum.data = new short[sum.numOfS];

	for (int i = 0; i < sum.numOfS;i++) {
		short S = i < this->numOfS ? this->data[i] : 0;
		short oS = i < other.numOfS ? other.data[i] : 0;
		int SUM = S + oS;
		SUM = SUM > 32767 ? 32767 : SUM;
		SUM = SUM < -32767 ? -32767 : SUM;

		sum.data[i] = (short)SUM;
	}
	sum.header.subchunk2Size = sum.numOfS * 2;
	sum.header.chunkSize = sum.header.subchunk2Size + 36;
	return sum;
}

Whisper& Whisper::operator+=(const Whisper& other) {
	int length = numOfS + other.numOfS;
	short* newData = new short[length];
	for (int i = 0;i < numOfS;i++) {
		newData[i] = data[i];
	}
	for (int i = 0; i < other.numOfS;i++) {
		newData[i + numOfS] = other.data[i];
	}
	delete[] data;
	data = newData;
	numOfS = length;

	header.subchunk2Size = numOfS * 2;
	header.chunkSize = header.subchunk2Size + 36;
	return *this;
}
Whisper& Whisper::operator=(const Whisper& other) {
	if (this == &other) return *this;
	delete[] data;
	header = other.header;
	numOfS = other.numOfS;
	if (numOfS > 0) {
		data = new short[numOfS];
		for (int i = 0; i < numOfS; i++)
			data[i] = other.data[i];
	}
	else {
		data = nullptr;
	}
	return *this;
}
void Whisper::setData(short* samples, int num, const WavHeader& ogHeader) {
	numOfS = num;
	delete[] data;
	data = new short[numOfS];
	for (int i = 0; i < numOfS;i++) {
		data[i] = samples[i];
	}
	header = ogHeader;
	header.subchunk2Size = num * 2;
	header.chunkSize = header.subchunk2Size + 36;
}


Spectrum Whisper::decouple() const {
	int powerOf2 = 1;
	while (powerOf2 < numOfS) {//as size of power of 2 required so padding done remaining zero
		powerOf2 *= 2;
	}
	Complex* temp = new Complex[powerOf2];
	for (int i = 0; i < powerOf2;i++) {
		if (i < numOfS) {
			temp[i] = Complex(data[i], 0.0);
		}
		else {
			temp[i] = Complex(0, 0);//padding after copying the ogdata
		}
	}
	//reordering 
	Complex* tempTemp = new Complex[powerOf2];
	for (int i = 0; i < powerOf2;i++) {
		tempTemp[i] = temp[i];
	}
	reorder(temp, tempTemp, powerOf2);
	butterfly(temp, powerOf2, true);

	Spectrum result(temp, powerOf2, numOfS, header.sampleRate);
	delete[] temp;
	delete[] tempTemp;
	return result;
}

Spectrum Whisper::decoupleWindow(int fftSize) const {
	int channels = header.numChannels;
	if (channels < 1) channels = 1;
	int numFrames = numOfS / channels;//one frame = one sample from every channel
	int startFrame = numFrames / 2 - fftSize / 2;

	Complex* temp = new Complex[fftSize];
	for (int i = 0; i < fftSize; i++) {
		int frameIndex = startFrame + i;
		if (frameIndex >= 0 && frameIndex < numFrames) {
			//averaging all the channels into one so the spectrum shows the whole mix
			double sum = 0.0;
			for (int c = 0; c < channels; c++) {
				sum += data[frameIndex * channels + c];
			}
			double hann = 0.5 * (1.0 - cos(2.0 * 3.14159265358979323846 * i / (fftSize - 1)));
			temp[i] = Complex((sum / channels) * hann, 0.0);
		}
		else {
			temp[i] = Complex(0, 0);
		}
	}

	Complex* tempTemp = new Complex[fftSize];
	for (int i = 0; i < fftSize; i++) tempTemp[i] = temp[i];

	reorder(temp, tempTemp, fftSize);
	butterfly(temp, fftSize, true);

	Spectrum result(temp, fftSize, numOfS, header.sampleRate);
	delete[] temp;
	delete[] tempTemp;
	return result;
}
Spectrogram Whisper::decoupleSTFT(int fftSize, int hopSize) const {
	int numFrames = (numOfS + hopSize - 1) / hopSize;

	Spectrogram result(numFrames, fftSize, hopSize, header.sampleRate);

	// Precompute ONCE — shared across all frames
	double* hannWindow = new double[fftSize];
	for (int i = 0; i < fftSize; i++)
		hannWindow[i] = 0.5 * (1.0 - cos(2.0 * 3.14159265358979323846 * i / fftSize));

	Complex* twiddleTable = buildTwiddleTable(fftSize);
	Complex* temp = new Complex[fftSize];

	for (int frame = 0; frame < numFrames; frame++) {
		int startSample = frame * hopSize;

		// Fill windowed frame
		for (int i = 0; i < fftSize; i++) {
			int idx = startSample + i;
			double s = (idx < numOfS) ? (double)data[idx] : 0.0;
			temp[i] = Complex(s * hannWindow[i], 0.0);
		}

		reorderBitReversal(temp, fftSize);                    // in-place, no alloc
		butterfly(temp, fftSize, true, twiddleTable);         // twiddle reused

		result.getFrame(frame) = Spectrum(temp, fftSize, fftSize, header.sampleRate);
	}

	delete[] hannWindow;
	delete[] twiddleTable;
	delete[] temp;
	return result;
}
void Whisper::save(const char* fileName) const {
	if (numOfS == 0 || data == nullptr) {
		cout << "No DATA ERRor" << endl;
		return;
	}

	ofstream f(fileName, ios::binary);

	f.write(reinterpret_cast<const char*>(&header), 44);

	for (int i = 0; i < numOfS;i++) {
		short temp = (short)data[i];
		f.write(reinterpret_cast<const char*>(&temp), 2);
	}
	f.close();
	cout << "saved: " << fileName << endl;
}

Whisper Whisper::extractChannel(int channel) const {
	Whisper mono;
	if (data == nullptr || header.numChannels < 1 || channel < 0 || channel >= header.numChannels) {
		return mono;
	}
	//samples are interleaved (L R L R...) so every numChannels-th sample belongs to the same channel
	int length = numOfS / header.numChannels;
	mono.header = header;
	mono.header.numChannels = 1;
	mono.header.byteRate = header.sampleRate * 2;
	mono.header.blockAlign = 2;
	mono.header.subchunk2Size = length * 2;
	mono.header.chunkSize = mono.header.subchunk2Size + 36;
	mono.numOfS = length;
	mono.data = new short[length];
	for (int i = 0; i < length; i++) {
		mono.data[i] = data[i * header.numChannels + channel];
	}
	return mono;
}

void Whisper::setChannel(int channel, const Whisper& mono) {
	if (data == nullptr || header.numChannels < 1 || channel < 0 || channel >= header.numChannels) {
		return;
	}
	int length = numOfS / header.numChannels;
	for (int i = 0; i < length; i++) {
		//if the new channel is shorter the rest just becomes silence
		data[i * header.numChannels + channel] = (i < mono.numOfS) ? mono.data[i] : 0;
	}
}

