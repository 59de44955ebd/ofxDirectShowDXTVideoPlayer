#include "DSRawSampleGrabber.h"

#include <ddraw.h>
#include <objbase.h>
#include <commctrl.h>
#include <initguid.h>

/////////////////////// instantiation //////////////////////////

DSRawSampleGrabber::DSRawSampleGrabber(IUnknown * pOuter, HRESULT * phr, BOOL ModifiesData)
	: CVideoTransformFilter(FILTERNAME, (IUnknown*)pOuter, CLSID_RawSampleGrabber) {
	callback = NULL;
}

DSRawSampleGrabber::~DSRawSampleGrabber() {
	callback = NULL;
}

CUnknown *WINAPI DSRawSampleGrabber::CreateInstance(LPUNKNOWN punk, HRESULT *phr) {
	HRESULT hr;
	if (!phr) phr = &hr;
	DSRawSampleGrabber *pNewObject = new DSRawSampleGrabber(punk, phr, FALSE);
	if (pNewObject == NULL) *phr = E_OUTOFMEMORY;
	return pNewObject;
}

/////////////////////// IUnknown //////////////////////////
HRESULT DSRawSampleGrabber::NonDelegatingQueryInterface(const IID &riid, void **ppv) {
	return CVideoTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}

/////////////////////// CTransInPlaceFilter //////////////////////////

HRESULT DSRawSampleGrabber::CheckInputType(const CMediaType *pmt)
{

	// Does this have a VIDEOINFOHEADER format block
	const GUID *pFormatType = pmt->FormatType();
	if (*pFormatType != FORMAT_VideoInfo) {
		NOTE("Format GUID not a VIDEOINFOHEADER");
		return E_INVALIDARG;
	}
	ASSERT(pmt->Format());

	// Check the format looks reasonably ok
	ULONG Length = pmt->FormatLength();
	if (Length < SIZE_VIDEOHEADER) {
		NOTE("Format smaller than a VIDEOHEADER");
		return E_INVALIDARG;
	}

	// Check if the media major type is MEDIATYPE_Video
	const GUID *pMajorType = pmt->Type();
	if (*pMajorType != MEDIATYPE_Video) {
		NOTE("Major type not MEDIATYPE_Video");
		return E_INVALIDARG;
	}

	// Check if the media subtype is a supported DXT type
	const GUID *pSubType = pmt->Subtype();
	if (*pSubType == MEDIASUBTYPE_DXT1 || *pSubType == MEDIASUBTYPE_DXT5 || *pSubType == MEDIASUBTYPE_DXTY) {
		return S_OK;
	}

	return E_INVALIDARG;
}

HRESULT DSRawSampleGrabber::Transform(IMediaSample * pIn, IMediaSample *pOut){
	if (!pCallback) return S_OK;

	REFERENCE_TIME sampleTime;
	REFERENCE_TIME totalDuration;
	HRESULT hr = pIn->GetTime(&sampleTime, &totalDuration);

	// send the input buffer thru
	pCallback->SampleCB(long(sampleTime), pIn);

	return S_OK;
}

// since we connect to NullRenderer, this is irrelevant for us
HRESULT DSRawSampleGrabber::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut){
	return S_OK;
}

// since we connect to NullRenderer, this is irrelevant for us
HRESULT DSRawSampleGrabber::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
	if (m_pInput->IsConnected() == FALSE) return E_UNEXPECTED;

	ASSERT(pAlloc);
	ASSERT(pProperties);

	pProperties->cBuffers = 1; // we don't care
	pProperties->cbBuffer = 1; // we don't care

	ALLOCATOR_PROPERTIES Actual;
	HRESULT hr = pAlloc->SetProperties(pProperties, &Actual);
	if (FAILED(hr)) return hr;

	ASSERT(Actual.cBuffers >= 1);
	if (pProperties->cBuffers > Actual.cBuffers ||
		pProperties->cbBuffer > Actual.cbBuffer) {
		return E_FAIL;
	}

	return S_OK;
}

HRESULT DSRawSampleGrabber::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	// Is the input pin connected
	if (m_pInput->IsConnected() == FALSE) {
		return E_UNEXPECTED;
	}

	// This should never happen
	if (iPosition < 0) return E_INVALIDARG;

	// Do we have more items to offer
	if (iPosition > 0) return VFW_S_NO_MORE_ITEMS;

	*pMediaType = m_pInput->CurrentMediaType();

	return S_OK;
}

HRESULT STDMETHODCALLTYPE DSRawSampleGrabber::SetCallback(ISampleGrabberCB *pCallback, long WhichMethodToCallback)
{
	this->pCallback = pCallback;
	return S_OK;
}
