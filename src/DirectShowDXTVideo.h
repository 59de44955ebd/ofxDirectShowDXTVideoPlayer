#pragma once

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <stdint.h>

#include "ofMain.h"
#include "DSShared.h"
#include "DXTShared.h"
#include "DSRawSampleGrabber.h"

class DirectShowDXTVideo : public ISampleGrabberCB {

public:

	DirectShowDXTVideo();
	~DirectShowDXTVideo();

	void tearDown();
	void clearValues();

	//------------------------------------------------
	STDMETHODIMP_(ULONG) AddRef() { return 1; }
	STDMETHODIMP_(ULONG) Release() { return 2; }
	//------------------------------------------------
	STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject);

	STDMETHODIMP SampleCB(long Time, IMediaSample *pSample);

	//This method is meant to have more overhead
	STDMETHODIMP BufferCB(double Time, BYTE *pBuffer, long BufferLen) {return E_NOTIMPL;}

	bool loadMovieManualGraph(string path);

	void createFilterGraphManager(bool &success);

	void querySeekInterface(bool &success);
	void queryPositionInterface(bool &success);
	void queryAudioInterface(bool &success);
	void queryControlInterface(bool &success);
	void queryEventInterface(bool &success);
	void queryFileSourceFilterInterface(bool &success);

	void fileSourceFilterInterfaceLoad(string path, bool &success);

	void createLavSplitterSourceFilter(bool &success);
	void createHapDecoderFilter(bool &success);
	void createRawSampleGrabberFilter(bool &success);
	void createNullRendererFilter(bool &success);
	void createAudioRendererFilter(bool &success);

	void addFilter(IBaseFilter * filter, LPCWSTR filterName, bool &success);
	bool hasPins(IBaseFilter * filter);
	IPin * getOutputPin(IBaseFilter * filter, bool &success);
	IPin * getInputPin(IBaseFilter * filter, bool &success);
	void connectPins(IPin * pinOut, IPin * pinIn, bool &success);
	void getDimensionsAndFrameInfo(bool &success);
	bool getContainsAudio(IBaseFilter * filter, IPin *& audioPin);
	void update();
	bool isLoaded();
	void setVolume(float volPct);
	float getVolume();
	double getDurationInSeconds();
	double getCurrentTimeInSeconds();
	void setPosition(float pct);
	float getPosition();
	void setSpeed(float speed);
	double getSpeed();
	DXTTextureFormat getTextureFormat();
	void play();
	void stop();
	void setPaused(bool bPaused);
	void updatePlayState();
	bool isPlaying();
	bool isPaused();
	bool isLooping();
	void setLoop(bool loop);
	bool isMovieDone();
	float getWidth();
	float getHeight();
	bool isFrameNew();
	void nextFrame();
	void previousFrame();
	void setFrame(int frame);
	int getCurrentFrame();
	int getTotalFrames();
	int getBufferSize();
	void getPixels(unsigned char * dstBuffer);

protected:

	HRESULT hr;							// COM return value

										// interfaces
	IFilterGraph2 * filterGraphManager;	// Filter Graph
	IMediaControl *controlInterface;	// Media Control interface
	IMediaEvent   *eventInterface;		// Media Event interface
	IMediaSeeking *seekInterface;		// Media Seeking interface
	IMediaPosition * positionInterface;
	IBasicAudio   *audioInterface;		// Audio Settings interface
	ISampleGrabber * m_pGrabber;
	IFileSourceFilter * fileSourceFilterInterface;

	// filters
	IBaseFilter * lavSplitterSourceFilter; // NEW
	IBaseFilter * hapDecoderFilter; // NEW
	IBaseFilter * nullRendererFilter;
	IBaseFilter * audioRendererFilter;

	DSRawSampleGrabber * rawSampleGrabberFilter;

	GUID timeFormat;
	REFERENCE_TIME timeNow;				// Used for FF & REW of movie, current time
	LONGLONG lPositionInSecs;		// Time in  seconds
	LONGLONG lDurationInNanoSecs;		// Duration in nanoseconds
	LONGLONG lTotalDuration;		// Total duration
	REFERENCE_TIME rtNew;				// Reference time of movie
	long lPosition;					// Desired position of movie used in FF & REW
	long lvolume;					// The volume level in 1/100ths dB Valid values range from -10,000 (silence) to 0 (full volume), 0 = 0 dB -10000 = -100 dB
	long evCode;					// event variable, used to in file to complete wait.

	long width, height;
	long videoSize;

	double averageTimePerFrame;

	bool bFrameNew;
	bool bNewPixels;
	bool bVideoOpened;
	bool bPlaying;
	bool bPaused;
	bool bLoop;
	bool bEndReached;
	double movieRate;
	int curMovieFrame;
	int frameCount;
	int lastBufferSize;

	CRITICAL_SECTION critSection;
	unsigned char * rawBuffer;

	DXTTextureFormat textureFormat;

};
