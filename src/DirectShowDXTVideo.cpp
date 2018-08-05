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

	if (controlInterface) {
		controlInterface->Stop();
		controlInterface->Release();
	}
	if (eventInterface) {
		eventInterface->Release();
	}
	if (seekInterface) {
		seekInterface->Release();
	}
	if (audioInterface) {
		audioInterface->Release();
	}
	if (positionInterface) {
		positionInterface->Release();
	}

	if (fileSourceFilterInterface) {
		fileSourceFilterInterface->Release();
	}

	if (filterGraphManager) {
		this->filterGraphManager->RemoveFilter(this->lavSplitterSourceFilter);
		this->filterGraphManager->RemoveFilter(this->hapDecoderFilter);
		this->filterGraphManager->RemoveFilter(this->rawSampleGrabberFilter);
		this->filterGraphManager->RemoveFilter(this->nullRendererFilter);
		this->filterGraphManager->RemoveFilter(this->audioRendererFilter);
		filterGraphManager->Release();
	}

	if (lavSplitterSourceFilter) {
		lavSplitterSourceFilter->Release();
	}
	if (hapDecoderFilter) {
		hapDecoderFilter->Release();
	}
	if (nullRendererFilter) {
		nullRendererFilter->Release();
	}
	if (audioRendererFilter) {
		audioRendererFilter->Release();
	}
	if (rawSampleGrabberFilter) {
		rawSampleGrabberFilter->SetCallback(NULL, 0);
		rawSampleGrabberFilter->Release();
	}
	if (rawBuffer) {
		delete[] rawBuffer;
	}
	clearValues();
}

void DirectShowDXTVideo::clearValues() {

	hr = 0;

	// interfaces
	filterGraphManager = NULL;
	controlInterface = NULL;
	eventInterface = NULL;
	seekInterface = NULL;
	audioInterface = NULL;
	m_pGrabber = NULL;
	positionInterface = NULL;

	// filters
	nullRendererFilter = NULL;
	lavSplitterSourceFilter = NULL;
	fileSourceFilterInterface = NULL;
	hapDecoderFilter = NULL;
	rawSampleGrabberFilter = NULL;
	audioRendererFilter = NULL;

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

//------------------------------------------------
STDMETHODIMP DirectShowDXTVideo::QueryInterface(REFIID riid, void **ppvObject) {
	*ppvObject = static_cast<ISampleGrabberCB*>(this);
	return S_OK;
}

//------------------------------------------------
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

	bool success = true;

	this->createFilterGraphManager(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->createLavSplitterSourceFilter(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->createHapDecoderFilter(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->createRawSampleGrabberFilter(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->createNullRendererFilter(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->querySeekInterface(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->queryPositionInterface(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->queryAudioInterface(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->queryControlInterface(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->queryEventInterface(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->queryFileSourceFilterInterface(success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->addFilter(this->lavSplitterSourceFilter, L"LAVSplitterSource", success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->addFilter(this->hapDecoderFilter, L"HapDecoder", success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->addFilter(this->rawSampleGrabberFilter, L"RawSampleGrabber", success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->addFilter(this->nullRendererFilter, L"NullRenderer", success);
	if (success == false) {
		tearDown();
		return false;
	}

	this->fileSourceFilterInterfaceLoad(path, success);
	if (success == false) {
		tearDown();
		return false;
	}

	// lavSplitterSourceFilter -> hapDecoderFilter
	IPin * lavSplitterSourceOutput = this->getOutputPin(this->lavSplitterSourceFilter, success);
	IPin * hapDecoderInput = this->getInputPin(this->hapDecoderFilter, success);
	this->connectPins(lavSplitterSourceOutput, hapDecoderInput, success);
	lavSplitterSourceOutput->Release();
	hapDecoderInput->Release();

	// hapDecoderFilter -> rawSampleGrabberFilter
	IPin * hapDecoderOutput = this->getOutputPin(this->hapDecoderFilter, success);
	IPin * rawSampleGrabberInput = this->getInputPin(this->rawSampleGrabberFilter, success);
	this->connectPins(hapDecoderOutput, rawSampleGrabberInput, success);
	hapDecoderOutput->Release();
	rawSampleGrabberInput->Release();

	// check if file contains audio
	IPin * lavSplitterSourceAudioOutput = NULL;
	if (getContainsAudio(lavSplitterSourceFilter, lavSplitterSourceAudioOutput)) {
		this->createAudioRendererFilter(success);
		this->addFilter(this->audioRendererFilter, L"SoundRenderer", success);

		IPin * audioInputPin = this->getInputPin(audioRendererFilter, success);

		this->connectPins(lavSplitterSourceAudioOutput, audioInputPin, success);

		lavSplitterSourceAudioOutput->Release();
		audioInputPin->Release();
	}

	IPin * rawSampleGrabberOutput = this->getOutputPin(this->rawSampleGrabberFilter, success);

	// No need to connect nullRenderer. AM_RENDEREX_RENDERTOEXISTINGRENDERS requires that the input pin is not connected
	hr = this->filterGraphManager->RenderEx(rawSampleGrabberOutput, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL);

	this->getDimensionsAndFrameInfo(success);

	updatePlayState();

	bVideoOpened = true;

	return true;
}

void DirectShowDXTVideo::createFilterGraphManager(bool &success)
{
	HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void **)&filterGraphManager);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::querySeekInterface(bool &success)
{
	//Allow the ability to go to a specific frame
	HRESULT hr = this->filterGraphManager->QueryInterface(IID_IMediaSeeking, (void**)&this->seekInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryPositionInterface(bool &success)
{
	//Allows the ability to set the rate and query whether forward and backward seeking is possible
	HRESULT hr = this->filterGraphManager->QueryInterface(IID_IMediaPosition, (LPVOID *)&this->positionInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryAudioInterface(bool &success)
{
	//Audio settings interface
	HRESULT hr = this->filterGraphManager->QueryInterface(IID_IBasicAudio, (void**)&this->audioInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryControlInterface(bool &success)
{
	// Control flow of data through the filter graph. I.e. run, pause, stop
	HRESULT hr = this->filterGraphManager->QueryInterface(IID_IMediaControl, (void **)&this->controlInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryEventInterface(bool &success)
{
	// Media events
	HRESULT hr = this->filterGraphManager->QueryInterface(IID_IMediaEvent, (void **)&this->eventInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::queryFileSourceFilterInterface(bool &success)
{
	HRESULT hr = lavSplitterSourceFilter->QueryInterface(IID_IFileSourceFilter, (void**)&fileSourceFilterInterface);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::fileSourceFilterInterfaceLoad(string path, bool &success)
{
	std::wstring filePathW = std::wstring(path.begin(), path.end());

	HRESULT hr = fileSourceFilterInterface->Load(filePathW.c_str(), NULL);
	if (FAILED(hr)) {
		ofLogError("DirectShowDXTVideo") << "Failed to load file " << path;
		success = false;
	}
}

void DirectShowDXTVideo::createLavSplitterSourceFilter(bool &success) {
	HRESULT hr = CoCreateInstance(CLSID_LAVSplitterSource, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&this->lavSplitterSourceFilter));
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::createHapDecoderFilter(bool &success) {
	HRESULT hr = CoCreateInstance(CLSID_HapDecoder, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&this->hapDecoderFilter));
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::createRawSampleGrabberFilter(bool &success) {
	HRESULT hr = 0;
	this->rawSampleGrabberFilter = (DSRawSampleGrabber*)DSRawSampleGrabber::CreateInstance(NULL, &hr);
	this->rawSampleGrabberFilter->AddRef();
	if (FAILED(hr)) {
		success = false;
	}

	this->rawSampleGrabberFilter->SetCallback(this, 0);
}

void DirectShowDXTVideo::createNullRendererFilter(bool &success) {
	HRESULT hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&this->nullRendererFilter));
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::createAudioRendererFilter(bool &success)
{
	HRESULT hr = CoCreateInstance(CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)(&this->audioRendererFilter));
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::addFilter(IBaseFilter * filter, LPCWSTR filterName, bool &success)
{
	HRESULT hr = this->filterGraphManager->AddFilter(filter, filterName);
	if (FAILED(hr)) {
		success = false;
	}
}

bool DirectShowDXTVideo::hasPins(IBaseFilter * filter)
{
	IEnumPins * enumPins;
	IPin * pin;

	filter->EnumPins(&enumPins);
	enumPins->Reset();
	int numPins = 0;
	while (enumPins->Next(1, &pin, 0) == S_OK) {
		numPins++;
	}

	if (numPins == 0) {
		return false;
	}

	enumPins->Release();
	return true;
}

IPin * DirectShowDXTVideo::getOutputPin(IBaseFilter * filter, bool &success)
{
	IEnumPins * enumPins;
	IPin * pin;
	ULONG fetched;
	PIN_INFO pinfo;

	filter->EnumPins(&enumPins);
	enumPins->Reset();
	enumPins->Next(1, &pin, &fetched);
	if (pin == NULL) {
		success = false;
	}
	else
	{
		pin->QueryPinInfo(&pinfo);
		if (pinfo.dir == PINDIR_INPUT) {
			pin->Release();
			enumPins->Next(1, &pin, &fetched);
		}

		if (pinfo.dir != PINDIR_OUTPUT) {
			success = false;
		}
	}
	enumPins->Release();

	return pin;
}

IPin * DirectShowDXTVideo::getInputPin(IBaseFilter * filter, bool &success)
{
	IEnumPins * enumPins;
	IPin * pin;
	ULONG fetched;
	PIN_INFO pinfo;
	filter->EnumPins(&enumPins);
	enumPins->Reset();
	enumPins->Next(1, &pin, &fetched);
	pin->QueryPinInfo(&pinfo);
	pinfo.pFilter->Release();
	if (pinfo.dir == PINDIR_OUTPUT) {
		pin->Release();
		enumPins->Next(1, &pin, &fetched);
	}
	if (pin == NULL) {
		success = false;
	}
	else
	{
		pin->QueryPinInfo(&pinfo);
		pinfo.pFilter->Release();
		if (pinfo.dir != PINDIR_INPUT)
		{
			success = false;
		}
	}
	enumPins->Release();

	return pin;
}

void DirectShowDXTVideo::connectPins(IPin * pinOut, IPin * pinIn, bool &success)
{
	HRESULT hr = filterGraphManager->ConnectDirect(pinOut, pinIn, NULL);
	if (FAILED(hr)) {
		success = false;
	}
}

void DirectShowDXTVideo::getDimensionsAndFrameInfo(bool &success)
{
	HRESULT hr = this->controlInterface->Run();

	// grab file dimensions and frame info

	AM_MEDIA_TYPE mt;
	ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));

	hr = rawSampleGrabberFilter->GetMediaType(0, (CMediaType*)&mt);

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
	hr = controlInterface->Stop();
}

bool DirectShowDXTVideo::getContainsAudio(IBaseFilter * filter, IPin *& audioPin) {

	bool bContainsAudio = false;

	IEnumPins * enumPins;
	IPin * pin;
	ULONG fetched;
	PIN_INFO pinfo;
	HRESULT hr;

	filter->EnumPins(&enumPins);
	enumPins->Reset();

	while (enumPins->Next(1, &pin, &fetched) == S_OK) {

		pin->QueryPinInfo(&pinfo);
		pinfo.pFilter->Release();

		if (pinfo.dir == PINDIR_OUTPUT) {

			// check if splitter has audio output
			IEnumMediaTypes * pEnum = NULL;
			AM_MEDIA_TYPE * pmt = NULL;
			BOOL bFound = false;
			hr = pin->EnumMediaTypes(&pEnum);
			while (pEnum->Next(1, &pmt, NULL) == S_OK) {
				if (pmt->majortype == MEDIATYPE_Audio) {
					audioPin = pin;
					bContainsAudio = true;
				}
			}
		}
	}

	enumPins->Release();

	return bContainsAudio;
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

		while (S_OK == eventInterface->GetEvent(&eventCode, (LONG_PTR*)&ptrParam1, (LONG_PTR*)&ptrParam2, 0)) {
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

			eventInterface->FreeEventParams(eventCode, ptrParam1, ptrParam2);
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
		audioInterface->put_Volume(vol);
	}
}

float DirectShowDXTVideo::getVolume() {
	float volPct = 0.0;
	if (isLoaded()) {
		long vol = 0;
		audioInterface->get_Volume(&vol);
		volPct = powf(10, (float)vol / 4000.0);
	}
	return volPct;
}

double DirectShowDXTVideo::getDurationInSeconds() {
	if (this->timeFormat != TIME_FORMAT_MEDIA_TIME)
	{
		seekInterface->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
		this->timeFormat = TIME_FORMAT_MEDIA_TIME;
	}
	if (isLoaded()) {
		long long lDurationInNanoSecs = 0;
		seekInterface->GetDuration(&lDurationInNanoSecs);
		double timeInSeconds = (double)lDurationInNanoSecs / 10000000.0;

		return timeInSeconds;
	}
	return 0.0;
}

double DirectShowDXTVideo::getCurrentTimeInSeconds() {
	if (this->timeFormat != TIME_FORMAT_MEDIA_TIME)
	{
		seekInterface->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
		this->timeFormat = TIME_FORMAT_MEDIA_TIME;
	}
	if (isLoaded()) {
		long long lCurrentTimeInNanoSecs = 0;
		seekInterface->GetCurrentPosition(&lCurrentTimeInNanoSecs);
		double timeInSeconds = (double)lCurrentTimeInNanoSecs / 10000000.0;

		return timeInSeconds;
	}
	return 0.0;
}

void DirectShowDXTVideo::setPosition(float pct) {
	if (this->timeFormat != TIME_FORMAT_MEDIA_TIME)
	{
		seekInterface->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
		this->timeFormat = TIME_FORMAT_MEDIA_TIME;
	}
	if (bVideoOpened) {
		if (pct < 0.0) pct = 0.0;
		if (pct > 1.0) pct = 1.0;

		long long lDurationInNanoSecs = 0;
		seekInterface->GetDuration(&lDurationInNanoSecs);

		rtNew = ((float)lDurationInNanoSecs * pct);
		hr = seekInterface->SetPositions(&rtNew, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
	}
}

float DirectShowDXTVideo::getPosition() {
	if (this->timeFormat != TIME_FORMAT_MEDIA_TIME)
	{
		seekInterface->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
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
		positionInterface->put_Rate(speed);
		positionInterface->get_Rate(&movieRate);
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
		controlInterface->Run();
		bEndReached = false;
		updatePlayState();
	}
}

void DirectShowDXTVideo::stop() {
	if (bVideoOpened) {
		if (isPlaying()) {
			setPosition(0.0);
		}
		controlInterface->Stop();
		updatePlayState();
	}
}

void DirectShowDXTVideo::setPaused(bool bPaused) {
	if (bVideoOpened) {
		if (bPaused) {
			controlInterface->Pause();
		}
		else {
			controlInterface->Run();
		}
		updatePlayState();
	}
}

void DirectShowDXTVideo::updatePlayState() {
	if (bVideoOpened) {
		FILTER_STATE fs;
		hr = controlInterface->GetState(4000, (OAFilterState*)&fs);
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
		seekInterface->SetTimeFormat(&TIME_FORMAT_FRAME);
		this->timeFormat = TIME_FORMAT_FRAME;
	}
	if (bVideoOpened) {
		LONGLONG frameNumber = frame;
		hr = seekInterface->SetPositions(&frameNumber, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning);
	}
}

int DirectShowDXTVideo::getCurrentFrame() {
	if (this->timeFormat != TIME_FORMAT_FRAME)
	{
		seekInterface->SetTimeFormat(&TIME_FORMAT_FRAME);
		this->timeFormat = TIME_FORMAT_FRAME;
	}
	LONGLONG currentFrame = 0;
	if (bVideoOpened) {
		seekInterface->GetCurrentPosition(&currentFrame);
	}
	return currentFrame;
}

int DirectShowDXTVideo::getTotalFrames() {
	if (this->timeFormat != TIME_FORMAT_FRAME)
	{
		seekInterface->SetTimeFormat(&TIME_FORMAT_FRAME);
		this->timeFormat = TIME_FORMAT_FRAME;
	}
	LONGLONG frames = 0;
	if (isLoaded()) {
		seekInterface->GetDuration(&frames);
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
