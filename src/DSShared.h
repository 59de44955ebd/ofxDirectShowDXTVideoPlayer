// DirectShow includes and helper methods

#pragma once

#include <DShow.h>

#pragma include_alias( "dxtrans.h", "qedit.h" )
#define __IDxtCompositor_INTERFACE_DEFINED__
#define __IDxtAlphaSetter_INTERFACE_DEFINED__
#define __IDxtJpeg_INTERFACE_DEFINED__
#define __IDxtKey_INTERFACE_DEFINED__

#include <uuids.h>
#include <Aviriff.h>
#include <Windows.h>

//for threading
#include <process.h>

// Due to a missing qedit.h in recent Platform SDKs, we've replicated the relevant contents here
MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85")
ISampleGrabberCB : public IUnknown
{
public:

	virtual HRESULT STDMETHODCALLTYPE SampleCB(
		long SampleTime,
		IMediaSample *pSample) = 0;

	virtual HRESULT STDMETHODCALLTYPE BufferCB(
		double SampleTime,
		BYTE *pBuffer,
		long BufferLen) = 0;

};

EXTERN_C const CLSID CLSID_NullRenderer;

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
	((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |   \
	((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define FOURCC_DXTY  (MAKEFOURCC('D','X','T','Y'))
