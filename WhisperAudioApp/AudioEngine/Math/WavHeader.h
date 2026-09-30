#pragma once


#pragma pack(push, 1)
struct WavHeader {
	char chunkId[4];       // "RIFF" 
	int chunkSize;         // Total file size - 8 
	char format[4];        // "WAVE" 
	char subchunk1Id[4];   // "fmt " 
	int subchunk1Size;     // 16 for PCM 
	short audioFormat;     // 1 for PCM 
	short numChannels;     // 1 for Mono, 2 for Stereo 
	int sampleRate;        // e.g. 44100 
	int byteRate;          // sampleRate * numChannels * 2 
	short blockAlign;      // numChannels * 2 
	short bitsPerSample;   // 16 
	char subchunk2Id[4];   // "data" 
	int subchunk2Size;     // Size of audio data in bytes 
};
#pragma pack(pop) 

