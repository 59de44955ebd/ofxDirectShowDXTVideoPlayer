#pragma once

#include "DSShared.h"
#include <assert.h>
#include <streams.h>
#include "uids.h"

#define FILTERNAME L"Raw Sample Grabber"

typedef HRESULT(CALLBACK *MANAGEDCALLBACKPROC)(double Time, IMediaSample *pSample);

// CTransInPlaceFilter
class DSRawSampleGrabber : public CVideoTransformFilter {

private:

	MANAGEDCALLBACKPROC callback;
	ISampleGrabberCB * pCallback;
	CMediaType * m_t;
	long m_Width;
	long m_Height;
	long m_SampleSize;
	long m_Stride;

public:

	DSRawSampleGrabber(IUnknown * pOuter, HRESULT * phr, BOOL ModifiesData);
	~DSRawSampleGrabber();

	static CUnknown *WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

	// IUnknown
	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);

	// virtual CVideoTransformFilter methos
	HRESULT Transform(IMediaSample * pIn, IMediaSample *pOut);
	HRESULT CheckInputType(const CMediaType * pmt);
	HRESULT CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut);
	HRESULT DecideBufferSize(IMemAllocator * pAlloc, ALLOCATOR_PROPERTIES * pProperties);
	HRESULT GetMediaType(int iPosition, CMediaType * pMediaType);

	// custom
	HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB *pCallback, long WhichMethodToCallback);

	STDMETHODIMP RegisterCallback(MANAGEDCALLBACKPROC mdelegate);
};