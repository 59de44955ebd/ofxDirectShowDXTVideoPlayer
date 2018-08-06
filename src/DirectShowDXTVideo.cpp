// DirectShowDXTVideo - contains a simple directshow video player implementation
// Based on code written by Theodore Watson, Jan 2014

#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>
#include <stdint.h>

#include "uids.h"
#include <streams.h>
#include "DirectShowDXTVideo.h"

#define SAFE_RELEASE(X) { if (X) X->Release(); X = NULL; }
#define CHECK_SUCCESS(X) { if (!X) {tearDown();return false;} }

static int comRefCount = 0;

static void retainCom() {
	if (comRefCount == 0) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	}
	comRefCount++;
}

static void releaseCom() {
	comRefCount--;
	if (comRefCount == 0) {
		CoUninitialize();
	}
}

DirectShowDXTVideo::DirectShowDXTVideo() {
	retainCom();
	clearValues();
	InitializeCriticalSection(&critSection);
}

DirectShowDXTVideo::~DirectShowDXTVideo() {
	stop();
	tearDown();
	releaseCom();
	DeleteCriticalSection(&critSection);
}

void DirectShowDXTVideo::tearDown() {

	//release interfaces
	if (pControlInterface) pControlInterface->Stop();
	SAFE_RELEASE(pControlInterface);
	SAFE_RELEASE(pEventInterface);
	SAFE_RELEASE(pSeekInterface);
	SAFE_RELEASE(pAudioInterface);
	SAFE_RELEASE(pPositionInterface);
	SAFE_RELEASE(pSourceFilterInterface);

	SAFE_RELEASE(pGraphManager); // removes filters on the fly

	// release filters
	if (pRawSampleGrabberFilter) pRawSampleGrabberFilter->SetCallback(NULL, 0);
	SAFE_RELEASE(pRawSampleGrabberFilter);
	SAFE_RELEASE(pLavSplitterSourceFilter);
	SAFE_RELEASE(pHapDecoderFilter);
	SAFE_RELEASE(pNullRendererFilter);
	SAFE_RELEASE(pAudioRendererFilter);

	if (rawBuffer) {
		delete[] rawBuffer;
		rawBuffer = NULL;
	}
	clearValues();
}

void DirectShowDXTVideo::clearValues() {
	hr = 0;
	rawBuffer = NULL;
	timeFormat = TIME_FORMAT_MEDIA_TIME;
	timeNow = 0;
	lPositionInSecs = 0;
	lDurationInNanoSecs = 0;
	lTotalDuration = 0;
	rtNew = 0;
	lPosition = 0;
	lvolume = -1000;
	evCode = 0;
	width = height = 0;
	videoSize = 0;
	bVideoOpened = false;
	bLoop = true;
	bPaused = false;
	bPlaying = false;
	bEndReached = false;
	bNewPixels = false;
	bFrameNew = false;
	curMovieFrame = -1;
	frameCount = -1;
	lastBufferSize = 0;
	movieRate = 1.0;
	averageTimePerFrame = 1.0 / 30.0;
}

STDMETHODIMP DirectShowDXTVideo::QueryInterface(REFIID riid, void **ppvObject) {
	*ppvObject = static_cast<ISampleGrabberCB*>(this);
	return S_OK;
}

STDMETHODIMP DirectShowDXTVideo::SampleCB(long Time, IMediaSample *pSample) {

	if (!rawBuffer) return E_OUTOFMEMORY;

	BYTE * ptrBuffer = NULL;
	HRESULT hr = pSample->GetPointer(&ptrBuffer);
	if (FAILED(hr)) return E_FAIL;

	long latestBufferLength = pSample->GetActualDataLength();

	EnterCriticalSection(&critSection);

	memcpy(rawBuffer, ptrBuffer, latestBufferLength);

	bNewPixels = true;

	//this is just so we know if there is a new frame
	frameCount++;

	LeaveCriticalSection(&critSection);

	return S_OK;
}

bool DirectShowDXTVideo::loadMovieManualGraph(string path) {

	//Release all the filters etc.
	tearDown();

	bool bSuccess = true;

	this->createFilterGraphManager(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->createLavSplitterSourceFilter(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->createHapDecoderFilter(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->createRawSampleGrabberFilter(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->createNullRendererFilter(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->querySeekInterface(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->queryPositionInterface(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->queryAudioInterface(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->queryControlInterface(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->queryEventInterface(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->queryFileSourceFilterInterface(bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->addFilter(this->pLavSplitterSourceFilter, L"LAVSplitterSource", bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->addFilter(this->pHapDecoderFilter, L"HapDecoder", bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->addFilter(this->pRawSampleGrabberFilter, L"RawSampleGrabber", bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->addFilter(this->pNullRendererFilter, L"NullRenderer", bSuccess);
	CHECK_SUCCESS(bSuccess);

	this->pSourceFilterInterfaceLoad(path, bSuccess);
	CHECK_SUCCESS(bSuccess);

	// pLavSplitterSourceFilter -> pHapDecoderFilter
	IPin * lavSplitterSourceOutput = this->getOutputPin(this->pLavSplitterSourceFilter, bSuccess);
	CHECK_SUCCESS(bSuccess);
	IPin * hapDecoderInput = this->getInputPin(this->pHapDecoderFilter, bSuccess);
	if (bSuccess) {
		this->connectPins(lavSplitterSourceOutput, hapDecoderInput, bSuccess);
		hapDecoderInput->Release();
	}
	lavSplitterSourceOutput->Release();
	CHECK_SUCCESS(bSuccess);

	// pHapDecoderFilter -> pRawSampleGrabberFilter
	IPin * hapDecoderOutput = this->getOutputPin(this->pHapDecoderFilter, bSuccess);
	CHECK_SUCCESS(bSuccess);
	IPin * rawSampleGrabberInput = this->getInputPin(this->pRawSampleGrabberFilter, bSuccess);
	if (bSuccess) {
		this->connectPins(hapDecoderOutput, rawSampleGrabberInput, bSuccess);
		rawSampleGrabberInput->Release();
	}
	hapDecoderOutput->Release();
	CHECK_SUCCESS(bSuccess);

	// pRawSampleGrabberFilter -> pNullRendererFilter
	// No need to connect nullRenderer. AM_RENDEREX_RENDERTOEXISTINGRENDERS requires that the input pin is not connected
	IPin * rawSampleGrabberOutput = this->getOutputPin(this->pRawSampleGrabberFilter, bSuccess);
	CHECK_SUCCESS(bSuccess);
	hr = this->pGraphManager->RenderEx(rawSampleGrabberOutput, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL);
	rawSampleGrabberOutput->Release();
	if (FAILED(hr)) {
		tearDown();
		return false;
	}

	// check if file also contains audio, if yes, render it with system defaults
	IPin * lavSplitterSourceAudioOutput = NULL;
	if (getContainsAudio(pLavSplitterSourceFilter, lavSplitterSourceAudioOutput)) {
		this->createAudioRendererFilter(bSuccess);
		if (bSuccess) this->addFilter(this->pAudioRendererFilter, L"SoundRenderer", bSuccess);
		if (bSuccess) {
			hr = this->pGraphManager->RenderEx(lavSplitterSourceAudioOutput, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL);
			if (FAILED(hr)) ofLogWarning("DirectShowDXTVideo") << "Failed to render audio pin";
		}
		lavSplitterSourceAudioOutput->Release();
	}

	this->getDimensionsAndFrameInfo(bSuccess);

	updatePlayState();

	bVideoOpened = true;

	return true;
}

void DirectShowDXTVideo::createFilterGraphManager(bool &success)
{
	HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&pGraphManager);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::querySeekInterface(bool &success)
{
	//Allow the ability to go to a specific frame
	HRESULT hr = this->pGraphManager->QueryInterface(IID_IMediaSeeking, (void**)&this->pSeekInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryPositionInterface(bool &success)
{
	//Allows the ability to set the rate and query whether forward and backward seeking is possible
	HRESULT hr = this->pGraphManager->QueryInterface(IID_IMediaPosition, (LPVOID *)&this->pPositionInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryAudioInterface(bool &success)
{
	//Audio settings interface
	HRESULT hr = this->pGraphManager->QueryInterface(IID_IBasicAudio, (void**)&this->pAudioInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryControlInterface(bool &success)
{
	// Control flow of data through the filter graph. I.e. run, pause, stop
	HRESULT hr = this->pGraphManager->QueryInterface(IID_IMediaControl, (void **)&this->pControlInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryEventInterface(bool &success)
{
	// Media events
	HRESULT hr = this->pGraphManager->QueryInterface(IID_IMediaEvent, (void **)&this->pEventInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryFileSourceFilterInterface(bool &success)
{
	HRESULT hr = pLavSplitterSourceFilter->QueryInterface(IID_IFileSourceFilter, (void**)&pSourceFilterInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::pSourceFilterInterfaceLoad(string path, bool &success)
{
	std::wstring filePathW = std::wstring(path.begin(), path.end());

	HRESULT hr = pSourceFilterInterface->Load(filePathW.c_str(), NULL);
	if (FAILED(hr)) {
		ofLogError("DirectShowDXTVideo") << "Failed to load file " << path;
		success = false;
	}
}

void DirectShowDXTVideo::createLavSplitterSourceFilter(bool &success) {
	HRESULT hr = CoCreateInstance(CLSID_LAVSplitterSource, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&this->pLavSplitterSourceFilter));
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::createHapDecoderFilter(bool &success) {
	HRESULT hr = CoCreateInstance(CLSID_HapDecoder, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&this->pHapDecoderFilter));
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::createRawSampleGrabberFilter(bool &success) {
	HRESULT hr = 0;
	this->pRawSampleGrabberFilter = (DSRawSampleGrabber*)DSRawSampleGrabber::CreateInstance(NULL, &hr);
	this->pRawSampleGrabberFilter->AddRef();
	success = SUCCEEDED(hr);
	this->pRawSampleGrabberFilter->SetCallback(this, 0);
}

void DirectShowDXTVideo::createNullRendererFilter(bool &success) {
	HRESULT hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&this->pNullRendererFilter));
	success = SUCCEEDED(hr);
}

void DirectShowDXTVideo::createAudioRendererFilter(bool &success)
{
	HRESULT hr = CoCreateInstance(CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&this->pAudioRendererFilter));
	success = SUCCEEDED(hr);
}

void DirectShowDXTVideo::addFilter(IBaseFilter * filter, LPCWSTR filterName, bool &success)
{
	HRESULT hr = this->pGraphManager->AddFilter(filter, filterName);
	success = SUCCEEDED(hr);
}

// returns first found output pin, if any
IPin * DirectShowDXTVideo::getOutputPin(IBaseFilter * filter, bool &success)
{
	IEnumPins * pEnumPins;
	IPin * pPin;
	ULONG fetched;
	PIN_INFO pinInfo;
	success = false;

	filter->EnumPins(&pEnumPins);
	pEnumPins->Reset();
	while (pEnumPins->Next(1, &pPin, &fetched) == S_OK) {
		pPin->QueryPinInfo(&pinInfo);
		if (pinInfo.dir == PINDIR_OUTPUT) {
			success = true;
			break;
		}
		pPin->Release();
	};

	pEnumPins->Release();
	return pPin;
}

// returns first found input pin, if any
IPin * DirectShowDXTVideo::getInputPin(IBaseFilter * filter, bool &success)
{
	IEnumPins * pEnumPins;
	IPin * pPin;
	ULONG fetched;
	PIN_INFO pinInfo;
	success = false;

	filter->EnumPins(&pEnumPins);
	pEnumPins->Reset();
	while (pEnumPins->Next(1, &pPin, &fetched) == S_OK) {
		pPin->QueryPinInfo(&pinInfo);
		if (pinInfo.dir == PINDIR_INPUT) {
			success = true;
			break;
		}
		pPin->Release();
	};

	pEnumPins->Release();
	return pPin;
}

void DirectShowDXTVideo::connectPins(IPin * pinOut, IPin * pinIn, bool &success)
{
	HRESULT hr = pGraphManager->ConnectDirect(pinOut, pinIn, NULL);
	success = SUCCEEDED(hr);
}

void DirectShowDXTVideo::getDimensionsAndFrameInfo(bool &success)
{
	HRESULT hr = this->pControlInterface->Run();

	// grab file dimensions and frame info

	AM_MEDIA_TYPE mt;
	ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));

	hr = pRawSampleGrabberFilter->GetMediaType(0, (CMediaType*)&mt);

	if (mt.pbFormat != NULL) {

		VIDEOINFOHEADER * infoheader = (VIDEOINFOHEADER*)mt.pbFormat;
		this->width = infoheader->bmiHeader.biWidth;
		this->height = infoheader->bmiHeader.biHeight;
		this->averageTimePerFrame = infoheader->AvgTimePerFrame / 10000000.0;
		this->videoSize = infoheader->bmiHeader.biSizeImage; // how many pixels to allocate

		BITMAPINFOHEADER * bitmapheader = &((VIDEOINFOHEADER*)mt.pbFormat)->bmiHeader;

		if (bitmapheader->biCompression == FOURCC_DXT1)
		{
			ofLogNotice("DirectShowDXTVideo") << "Texture format is DXT1";
			this->textureFormat = TextureFormat_RGB_DXT1;
		}
		else if (bitmapheader->biCompression == FOURCC_DXT5)
		{
			ofLogNotice("DirectShowDXTVideo") << "Texture format is DXT5";
			this->textureFormat = TextureFormat_RGBA_DXT5;
		}
		else if (bitmapheader->biCompression == FOURCC_DXTY)
		{
			ofLogNotice("DirectShowDXTVideo") << "Texture format is DXTY";
			this->textureFormat = TextureFormat_YCoCg_DXT5;
		}
		else
		{
			success = false;
		}
	}
	else
	{
		success = false;
	}

	if (width == 0 || height == 0) {
		success = false;
	}
	else {

		if (videoSize == 0) {
			ofLogWarning("DirectShowDXTVideo") << "Video frame size not encoded in file header";
			videoSize = width * height * (this->textureFormat == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT ? 4 : 3);
		}

		this->rawBuffer = new unsigned char[videoSize];
	}

	// Now pause the graph.
	hr = pControlInterface->Stop();
}

bool DirectShowDXTVideo::getContainsAudio(IBaseFilter * filter, IPin *& audioPin) {
	IEnumPins * pEnumPins;
	IPin * pPin;
	ULONG fetched;
	PIN_INFO pinInfo;
	HRESULT hr;

	filter->EnumPins(&pEnumPins);
	//pEnumPins->Reset();

	while (pEnumPins->Next(1, &pPin, &fetched) == S_OK) {
		pPin->QueryPinInfo(&pinInfo);
		pinInfo.pFilter->Release();
		if (pinInfo.dir == PINDIR_OUTPUT) {
			// check if splitter has audio output
			IEnumMediaTypes * pEnumTypes = NULL;
			AM_MEDIA_TYPE * pmt = NULL;
			BOOL bFound = false;
			hr = pPin->EnumMediaTypes(&pEnumTypes);
			while (pEnumTypes->Next(1, &pmt, NULL) == S_OK) {
				if (pmt->majortype == MEDIATYPE_Audio) {
					// we found an audio pin!
					audioPin = pPin;
					pEnumTypes->Release();
					pEnumPins->Release();
					return true;
				}
			}
			pEnumTypes->Release();
		}
	}

	pEnumPins->Release();

	return false;
}

void DirectShowDXTVideo::update() {
	if (bVideoOpened) {

		long eventCode = 0;
		LONG_PTR ptrParam1 = 0;
		LONG_PTR ptrParam2 = 0;
		long timeoutMs = 2000;

		if (curMovieFrame != frameCount) {
			bFrameNew = true;
		}
		else {
			bFrameNew = false;
		}
		curMovieFrame = frameCount;

		while (S_OK == pEventInterface->GetEvent(&eventCode, (LONG_PTR*)&ptrParam1, (LONG_PTR*)&ptrParam2, 0)) {
			if (eventCode == EC_COMPLETE) {
				if (bLoop) {
					setPosition(0.0);
					frameCount = 0;
				}
				else {
					bEndReached = true;
					stop();
					updatePlayState();
				}
			}

			pEventInterface->FreeEventParams(eventCode, ptrParam1, ptrParam2);
		}
	}
}

bool DirectShowDXTVideo::isLoaded() {
	return bVideoOpened;
}

//volume has to be log corrected/converted
void DirectShowDXTVideo::setVolume(float volPct) {
	if (isLoaded()) {
		if (volPct < 0) volPct = 0.0;
		if (volPct > 1) volPct = 1.0;

		long vol = log10(volPct) * 4000.0;
		pAudioInterface->put_Volume(vol);
	}
}

float DirectShowDXTVideo::getVolume() {
	float volPct = 0.0;
	if (isLoaded()) {
		long vol = 0;
		pAudioInterface->get_Volume(&vol);
		volPct = powf(10, (float)vol / 4000.0);
	}
	return volPct;
}

double DirectShowDXTVideo::getDurationInSeconds() {
	if (this->timeFormat != TIME_FORMAT_MEDIA_TIME)
	{
		pSeekInterface->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
		this->timeFormat = TIME_FORMAT_MEDIA_TIME;
	}
	if (isLoaded()) {
		long long lDurationInNanoSecs = 0;
		pSeekInterface->GetDuration(&lDurationInNanoSecs);
		double timeInSeconds = (double)lDurationInNanoSecs / 10000000.0;

		return timeInSeconds;
	}
	return 0.0;
}

double DirectShowDXTVideo::getCurrentTimeInSeconds() {
	if (this->timeFormat != TIME_FORMAT_MEDIA_TIME)
	{
		pSeekInterface->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
		this->timeFormat = TIME_FORMAT_MEDIA_TIME;
	}
	if (isLoaded()) {
		long long lCurrentTimeInNanoSecs = 0;
		pSeekInterface->GetCurrentPosition(&lCurrentTimeInNanoSecs);
		double timeInSeconds = (double)lCurrentTimeInNanoSecs / 10000000.0;

		return timeInSeconds;
	}
	return 0.0;
}

void DirectShowDXTVideo::setPosition(float pct) {
	if (this->timeFormat != TIME_FORMAT_MEDIA_TIME)
	{
		pSeekInterface->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
		this->timeFormat = TIME_FORMAT_MEDIA_TIME;
	}
	if (bVideoOpened) {
		if (pct < 0.0) pct = 0.0;
		if (pct > 1.0) pct = 1.0;

		long long lDurationInNanoSecs = 0;
		pSeekInterface->GetDuration(&lDurationInNanoSecs);

		rtNew = ((float)lDurationInNanoSecs * pct);
		hr = pSeekInterface->SetPositions(&rtNew, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
	}
}

float DirectShowDXTVideo::getPosition() {
	if (this->timeFormat != TIME_FORMAT_MEDIA_TIME)
	{
		pSeekInterface->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
		this->timeFormat = TIME_FORMAT_MEDIA_TIME;
	}
	if (bVideoOpened) {
		float timeDur = getDurationInSeconds();
		if (timeDur > 0.0) {
			return getCurrentTimeInSeconds() / timeDur;
		}
	}
	return 0.0;
}

void DirectShowDXTVideo::setSpeed(float speed) {
	if (bVideoOpened) {
		pPositionInterface->put_Rate(speed);
		pPositionInterface->get_Rate(&movieRate);
	}
}

double DirectShowDXTVideo::getSpeed() {
	return movieRate;
}

DXTTextureFormat DirectShowDXTVideo::getTextureFormat() {
	return textureFormat;
}

void DirectShowDXTVideo::play() {
	if (bVideoOpened) {
		pControlInterface->Run();
		bEndReached = false;
		updatePlayState();
	}
}

void DirectShowDXTVideo::stop() {
	if (bVideoOpened) {
		if (isPlaying()) {
			setPosition(0.0);
		}
		pControlInterface->Stop();
		updatePlayState();
	}
}

void DirectShowDXTVideo::setPaused(bool bPaused) {
	if (bVideoOpened) {
		if (bPaused) {
			pControlInterface->Pause();
		}
		else {
			pControlInterface->Run();
		}
		updatePlayState();
	}
}

void DirectShowDXTVideo::updatePlayState() {
	if (bVideoOpened) {
		FILTER_STATE fs;
		hr = pControlInterface->GetState(4000, (OAFilterState*)&fs);
		if (hr == S_OK) {
			if (fs == State_Running) {
				bPlaying = true;
				bPaused = false;
			}
			else if (fs == State_Paused) {
				bPlaying = false;
				bPaused = true;
			}
			else if (fs == State_Stopped) {
				bPlaying = false;
				bPaused = false;
			}
		}
	}
}

bool DirectShowDXTVideo::isPlaying() {
	updatePlayState();
	return bPlaying;
}

bool DirectShowDXTVideo::isPaused() {
	updatePlayState();
	return bPaused;
}

bool DirectShowDXTVideo::isLooping() {
	return bLoop;
}

void DirectShowDXTVideo::setLoop(bool loop) {
	bLoop = loop;
}

bool DirectShowDXTVideo::isMovieDone() {
	return bEndReached;
}

float DirectShowDXTVideo::getWidth() {
	return width;
}

float DirectShowDXTVideo::getHeight() {
	return height;
}

bool DirectShowDXTVideo::isFrameNew() {
	return bFrameNew;
}

void DirectShowDXTVideo::nextFrame() {
	//we have to do it like this as the frame based approach is not very accurate
	if (bVideoOpened && (isPlaying() || isPaused())) {
		int curFrame = getCurrentFrame();
		int totalFrames = getTotalFrames();
		setFrame(min(curFrame + 1, totalFrames - 1));
	}
}

void DirectShowDXTVideo::previousFrame() {
	//we have to do it like this as the frame based approach is not very accurate
	if (bVideoOpened && (isPlaying() || isPaused())) {
		int curFrame = getCurrentFrame();
		setFrame(max(0, curFrame - 1));
	}
}

void DirectShowDXTVideo::setFrame(int frame) {
	if (this->timeFormat != TIME_FORMAT_FRAME)
	{
		pSeekInterface->SetTimeFormat(&TIME_FORMAT_FRAME);
		this->timeFormat = TIME_FORMAT_FRAME;
	}
	if (bVideoOpened) {
		LONGLONG frameNumber = frame;
		hr = pSeekInterface->SetPositions(&frameNumber, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
	}
}

int DirectShowDXTVideo::getCurrentFrame() {
	if (this->timeFormat != TIME_FORMAT_FRAME)
	{
		pSeekInterface->SetTimeFormat(&TIME_FORMAT_FRAME);
		this->timeFormat = TIME_FORMAT_FRAME;
	}
	LONGLONG currentFrame = 0;
	if (bVideoOpened) {
		pSeekInterface->GetCurrentPosition(&currentFrame);
	}
	return currentFrame;
}

int DirectShowDXTVideo::getTotalFrames() {
	if (this->timeFormat != TIME_FORMAT_FRAME)
	{
		pSeekInterface->SetTimeFormat(&TIME_FORMAT_FRAME);
		this->timeFormat = TIME_FORMAT_FRAME;
	}
	LONGLONG frames = 0;
	if (isLoaded()) {
		pSeekInterface->GetDuration(&frames);
	}
	return frames;
}

int DirectShowDXTVideo::getBufferSize() {
	return lastBufferSize;
}

void DirectShowDXTVideo::getPixels(unsigned char * dstBuffer) {
	if (bVideoOpened && bNewPixels) {
		EnterCriticalSection(&critSection);
		memcpy(dstBuffer, rawBuffer, videoSize);
		bNewPixels = false;
		LeaveCriticalSection(&critSection);
	}
}
