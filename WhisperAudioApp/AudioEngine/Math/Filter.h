#pragma once
class Filter
{
private:
	float* weights;
	int size;
public:
	Filter(int size = 0);
	Filter(const Filter& other);
	~Filter();
	Filter& operator=(const Filter& other);

	float* getWeights() const;
	int getSize() { return size; }


	static Filter lowPass(int numOfB, int sampleRate, float cuttOff);
	static Filter highPass(int numOfB, int sampleRate, float cuttOff);
	static Filter bandPass(int numOfB, int sampleRate, float lowerHertz, float upperHertz);
	static Filter tenBandEQ(int numofB, int sampleRate,float gains[10]);

};

