#include "Filter.h"

//filter class functions
Filter::Filter(int size) {
	if (size == 0) {
		this->size = 0;
		weights = nullptr;
	}
	else {
		this->size = size;
		weights = new float[size];
		for (int i = 0; i < size; i++) {
			weights[i] = 0;
		}
	}
}
Filter::Filter(const Filter& other) {
	size = other.size;
	weights = new float[size];
	for (int i = 0; i < size;i++) {
		weights[i] = other.weights[i];
	}
}
Filter::~Filter() {
	delete[] weights;
}
Filter& Filter::operator=(const Filter& other) {
	delete[] weights;
	size = other.size;
	if (size > 0) {
		weights = new float[size];
		for (int i = 0; i < size; i++) {
			weights[i] = other.weights[i];
		}
	}
	else {
		weights = nullptr;
	}
	return *this;
}
float* Filter::getWeights() const {
	return weights;
}

//this function calculates the index of cutoff frequency and based on that sets weights
Filter Filter::lowPass(int numOfB, int sampleRate, float cutOff) {
	Filter f(numOfB);
	long long temp = ((long long)cutOff * numOfB) / sampleRate;//mapping frequency to its respectiev bin index in frequencyBins array
	int cutOffIndex = (int)(temp);
	int middle = numOfB / 2;

	for (int i = 0; i < numOfB;i++) {
		//as negative frequency mirrored so taken acount
		if (i <= middle) {
			if (i <= cutOffIndex) {
				f.weights[i] = 1;
			}
			else {
				f.weights[i] = 0;
			}

		}
		else {
			int negative = numOfB - i;
			if (negative <= cutOffIndex) {
				f.weights[i] = 1;
			}
			else {
				f.weights[i] = 0;
			}

		}

	}
	return f;
}

Filter Filter::highPass(int numOfB, int sampleRate, float cutOff) {
	Filter f(numOfB);
	long long temp = ((long long)cutOff * numOfB) / sampleRate;
	int cutOffIndex = (int)(temp);
	int middle = numOfB / 2;

	for (int i = 0; i < numOfB;i++) {
		//as negative frequency mirrored so taken acount
		if (i <= middle) {
			if (i >= cutOffIndex) {
				f.weights[i] = 1;
			}
			else {
				f.weights[i] = 0;
			}

		}
		else {
			int negative = numOfB - i;
			if (negative >= cutOffIndex) {
				f.weights[i] = 1;
			}
			else {
				f.weights[i] = 0;
			}

		}

	}
	return f;
}
Filter Filter::bandPass(int numOfB, int sampleRate, float lowerH, float highH) {
	Filter f(numOfB);
	int lowerIndex = (int)(lowerH * numOfB / sampleRate);
	int highIndex = (int)(highH * numOfB / sampleRate);

	for (int i = 0; i < numOfB; i++) {
		if ((i >= lowerIndex && i <= highIndex) || (i >= (numOfB - highIndex) && i <= (numOfB - lowerIndex))) {
			f.weights[i] = 1;
		}
		else {
			f.weights[i] = 0;
		}
	}
	return f;
}

Filter Filter:: tenBandEQ(int numOfB, int sampleRate, float gains[10]) {
	Filter f(numOfB);
	int frequencies[10] = { 31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };


	for(int i=0;i<10;i++){
		float lowerFreq = (i == 0) ? 0 : 1.41421356f * frequencies[i - 1];
		float highFreq = (i == 9) ? sampleRate / 2 : 1.41421356f * frequencies[i];
		int lowerIndex = (int)(lowerFreq * numOfB / sampleRate);
		int highIndex = (int)(highFreq * numOfB / sampleRate);
		for (int j = 0; j < numOfB; j++) {
			if ((j >= lowerIndex && j <= highIndex) || (j >= (numOfB - highIndex) && j <= (numOfB - lowerIndex))) {
				f.weights[j] = gains[i];
			}
		}
	}
	

	return f;
}