/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015, Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/VideoTexture/Texture.cpp
 *  \ingroup bgevideotex
 */

#ifdef WITH_GAMEENGINE_DECKLINK

// implementation

// FFmpeg defines its own version of stdint.h on Windows.
// Decklink needs FFmpeg, so it uses its version of stdint.h
// this is necessary for INT64_C macro
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
// this is necessary for UINTPTR_MAX (used by atomic-ops)
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "atomic_ops.h"

#include "EXP_PyObjectPlus.h"
#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h"
#include "DeckLink.h"

#include <memory.h>

// macro for exception handling and logging
#define CATCH_EXCP catch (Exception & exp) \
{ exp.report(); return NULL; }

static struct
{
	const char *name;
	BMDDisplayMode mode;
} sModeStringTab[] = {
	{ "NTSC", bmdModeNTSC },
	{ "NTSC2398", bmdModeNTSC2398 },
	{ "PAL", bmdModePAL },
	{ "NTSCp", bmdModeNTSCp },
	{ "PALp", bmdModePALp },

	/* HD 1080 Modes */

	{ "HD1080p2398", bmdModeHD1080p2398 },
	{ "HD1080p24", bmdModeHD1080p24 },
	{ "HD1080p25", bmdModeHD1080p25 },
	{ "HD1080p2997", bmdModeHD1080p2997 },
	{ "HD1080p30", bmdModeHD1080p30 },
	{ "HD1080i50", bmdModeHD1080i50 },
	{ "HD1080i5994", bmdModeHD1080i5994 },
	{ "HD1080i6000", bmdModeHD1080i6000 },
	{ "HD1080p50", bmdModeHD1080p50 },
	{ "HD1080p5994", bmdModeHD1080p5994 },
	{ "HD1080p6000", bmdModeHD1080p6000 },

	/* HD 720 Modes */

	{ "HD720p50", bmdModeHD720p50 },
	{ "HD720p5994", bmdModeHD720p5994 },
	{ "HD720p60", bmdModeHD720p60 },

	/* 2k Modes */

	{ "2k2398", bmdMode2k2398 },
	{ "2k24", bmdMode2k24 },
	{ "2k25", bmdMode2k25 },

	/* DCI Modes (output only) */

	{ "2kDCI2398", bmdMode2kDCI2398 },
	{ "2kDCI24", bmdMode2kDCI24 },
	{ "2kDCI25", bmdMode2kDCI25 },

	/* 4k Modes */

	{ "4K2160p2398", bmdMode4K2160p2398 },
	{ "4K2160p24", bmdMode4K2160p24 },
	{ "4K2160p25", bmdMode4K2160p25 },
	{ "4K2160p2997", bmdMode4K2160p2997 },
	{ "4K2160p30", bmdMode4K2160p30 },
	{ "4K2160p50", bmdMode4K2160p50 },
	{ "4K2160p5994", bmdMode4K2160p5994 },
	{ "4K2160p60", bmdMode4K2160p60 },
	// sentinel
	{ NULL }
};

static struct
{
	const char *name;
	BMDPixelFormat format;
} sFormatStringTab[] = {
	{ "8BitYUV", bmdFormat8BitYUV },
	{ "10BitYUV", bmdFormat10BitYUV },
	{ "8BitARGB", bmdFormat8BitARGB },
	{ "8BitBGRA", bmdFormat8BitBGRA },
	{ "10BitRGB", bmdFormat10BitRGB },
	{ "12BitRGB", bmdFormat12BitRGB },
	{ "12BitRGBLE", bmdFormat12BitRGBLE },
	{ "10BitRGBXLE", bmdFormat10BitRGBXLE },
	{ "10BitRGBX", bmdFormat10BitRGBX },
	// sentinel
	{ NULL }
};

ExceptionID DeckLinkBadDisplayMode, DeckLinkBadPixelFormat;
ExpDesc DeckLinkBadDisplayModeDesc(DeckLinkBadDisplayMode, "Invalid or unsupported display mode");
ExpDesc DeckLinkBadPixelFormatDesc(DeckLinkBadPixelFormat, "Invalid or unsupported pixel format");

HRESULT decklink_ReadDisplayMode(const char *format, size_t len, BMDDisplayMode *displayMode)
{
	int i;

	if (len == 0)
		len = strlen(format);
	for (i = 0; sModeStringTab[i].name != NULL; i++) {
		if (strlen(sModeStringTab[i].name) == len &&
			!strncmp(sModeStringTab[i].name, format, len))
		{
			*displayMode = sModeStringTab[i].mode;
			return S_OK;
		}
	}
	if (len != 4)
		THRWEXCP(DeckLinkBadDisplayMode, S_OK);
	// assume the user entered directly the mode value as a 4 char string
	*displayMode = (BMDDisplayMode)((((uint32_t)format[0]) << 24) + (((uint32_t)format[1]) << 16) + (((uint32_t)format[2]) << 8) + ((uint32_t)format[3]));
	return S_OK;
}

HRESULT decklink_ReadPixelFormat(const char *format, size_t len, BMDPixelFormat *pixelFormat)
{
	int i;

	if (!len)
		len = strlen(format);
	for (i = 0; sFormatStringTab[i].name != NULL; i++) {
		if (strlen(sFormatStringTab[i].name) == len &&
			!strncmp(sFormatStringTab[i].name, format, len))
		{
			*pixelFormat = sFormatStringTab[i].format;
			return S_OK;
		}
	}
	if (len != 4)
		THRWEXCP(DeckLinkBadPixelFormat, S_OK);
	// assume the user entered directly the mode value as a 4 char string
	*pixelFormat = (BMDPixelFormat)((((uint32_t)format[0]) << 24) + (((uint32_t)format[1]) << 16) + (((uint32_t)format[2]) << 8) + ((uint32_t)format[3]));
	return S_OK;
}

class DeckLink3DFrameWrapper : public IDeckLinkVideoFrame, IDeckLinkVideoFrame3DExtensions
{
public:
	// IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, LPVOID *ppv)
	{
		if (!memcmp(&iid, &IID_IDeckLinkVideoFrame3DExtensions, sizeof(iid))) {
			if (mpRightEye) {
				*ppv = (IDeckLinkVideoFrame3DExtensions*)this;
				return S_OK;
			}
		}
		return E_NOTIMPL;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef(void) { return 1U;  }
	virtual ULONG STDMETHODCALLTYPE Release(void) { return 1U; }
	// IDeckLinkVideoFrame
	virtual long STDMETHODCALLTYPE GetWidth(void) {	return mpLeftEye->GetWidth(); }
	virtual long STDMETHODCALLTYPE GetHeight(void) { return mpLeftEye->GetHeight(); }
	virtual long STDMETHODCALLTYPE GetRowBytes(void) { return mpLeftEye->GetRowBytes(); }
	virtual BMDPixelFormat STDMETHODCALLTYPE GetPixelFormat(void) { return mpLeftEye->GetPixelFormat(); }
	virtual BMDFrameFlags STDMETHODCALLTYPE GetFlags(void) { return mpLeftEye->GetFlags(); }
	virtual HRESULT STDMETHODCALLTYPE GetBytes(void **buffer) { return mpLeftEye->GetBytes(buffer); }
	virtual HRESULT STDMETHODCALLTYPE GetTimecode(BMDTimecodeFormat format,IDeckLinkTimecode **timecode)
		{ return mpLeftEye->GetTimecode(format, timecode); }
	virtual HRESULT STDMETHODCALLTYPE GetAncillaryData(IDeckLinkVideoFrameAncillary **ancillary) 
		{ return mpLeftEye->GetAncillaryData(ancillary); }
	// IDeckLinkVideoFrame3DExtensions
	virtual BMDVideo3DPackingFormat STDMETHODCALLTYPE Get3DPackingFormat(void)
	{
		return bmdVideo3DPackingLeftOnly;
	}
	virtual HRESULT STDMETHODCALLTYPE GetFrameForRightEye(
		/* [out] */ IDeckLinkVideoFrame **rightEyeFrame)
	{
		mpRightEye->AddRef();
		*rightEyeFrame = mpRightEye;
		return S_OK;
	}
	// Constructor
	DeckLink3DFrameWrapper(IDeckLinkVideoFrame *leftEye, IDeckLinkVideoFrame *rightEye)
	{
		mpLeftEye = leftEye;
		mpRightEye = rightEye;
	}
	// no need for a destructor, it's just a wrapper
private:
	IDeckLinkVideoFrame *mpLeftEye;
	IDeckLinkVideoFrame *mpRightEye;
};

static void decklink_Reset(DeckLink *self)
{
	self->m_lastClock = 0.0;
	self->mDLOutput = NULL;
	self->mUse3D = false;
	self->mDisplayMode = bmdModeUnknown;
	self->mKeyingSupported = false;
	self->mHDKeyingSupported = false;
	self->mSize[0] = 0;
	self->mSize[1] = 0;
	self->mFrameSize = 0;
	self->mLeftFrame = NULL;
	self->mRightFrame = NULL;
	self->mKeyer = NULL;
	self->mUseKeying = false;
	self->mKeyingLevel = 255;
	self->mUseExtend = false;
}

#ifdef __BIG_ENDIAN__
#define CONV_PIXEL(i)	((((i)>>16)&0xFF00)+(((i)&0xFF00)<<16)+((i)&0xFF00FF))
#else
#define CONV_PIXEL(i)	((((i)&0xFF)<<16)+(((i)>>16)&0xFF)+((i)&0xFF00FF00))
#endif

// adapt the pixel format and picture size from VideoTexture (RGBA) to DeckLink (BGRA)
static void decklink_ConvImage(uint32_t *dest, const short *destSize, const uint32_t *source, const short *srcSize, bool extend)
{
	short w, h, x, y;
	const uint32_t *s;
	uint32_t *d, p;
	bool sameSize = (destSize[0] == srcSize[0] && destSize[1] == srcSize[1]);

	if (sameSize || !extend) {
		// here we convert pixel by pixel
		w = (destSize[0] < srcSize[0]) ? destSize[0] : srcSize[0];
		h = (destSize[1] < srcSize[1]) ? destSize[1] : srcSize[1];
		for (y = 0; y < h; ++y) {
			s = source + y*srcSize[0];
			d = dest + y*destSize[0];
			for (x = 0; x < w; ++x, ++s, ++d) {
				*d = CONV_PIXEL(*s);
			}
		}
	}
	else {
		// here we scale
		// interpolation accumulator
		int accHeight = srcSize[1] >> 1;
		d = dest;
		s = source;
		// process image rows
		for (y = 0; y < srcSize[1]; ++y) {
			// increase height accum
			accHeight += destSize[1];
			// if pixel row has to be drawn
			if (accHeight >= srcSize[1]) {
				// decrease accum
				accHeight -= srcSize[1];
				// width accum
				int accWidth = srcSize[0] >> 1;
				// process row
				for (x = 0; x < srcSize[0]; ++x, ++s) {
					// increase width accum
					accWidth += destSize[0];
					// convert pixel
					p = CONV_PIXEL(*s);
					// if pixel has to be drown one or more times
					while (accWidth >= srcSize[0]) {
						// decrease accum
						accWidth -= srcSize[0];
						*d++ = p;
					}
				}
				// if there should be more identical lines
				while (accHeight >= srcSize[1]) {
					accHeight -= srcSize[1];
					// copy previous line
					memcpy(d, d - destSize[0], 4 * destSize[0]);
					d += destSize[0];
				}
			}
			else {
				// if we skip a source line
				s += srcSize[0];
			}
		}
	}
}

// DeckLink object allocation
static PyObject *DeckLink_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	// allocate object
	DeckLink * self = reinterpret_cast<DeckLink*>(type->tp_alloc(type, 0));
	// initialize object structure
	decklink_Reset(self);
	// m_leftEye is a python object, it's handled by python
	self->m_leftEye = NULL;
	self->m_rightEye = NULL;
	// return allocated object
	return reinterpret_cast<PyObject*>(self);
}


// forward declaration
PyObject *DeckLink_close(DeckLink *self);
int DeckLink_setSource(DeckLink *self, PyObject *value, void *closure);


// DeckLink object deallocation
static void DeckLink_dealloc(DeckLink *self)
{
	// release renderer
	Py_XDECREF(self->m_leftEye);
	// close decklink
	PyObject *ret = DeckLink_close(self);
	Py_DECREF(ret);
	// release object
	Py_TYPE((PyObject *)self)->tp_free((PyObject *)self);
}


ExceptionID AutoDetectionNotAvail, DeckLinkOpenCard, DeckLinkBadFormat, DeckLinkInternalError;
ExpDesc AutoDetectionNotAvailDesc(AutoDetectionNotAvail, "Auto detection not yet available");
ExpDesc DeckLinkOpenCardDesc(DeckLinkOpenCard, "Cannot open card for output");
ExpDesc DeckLinkBadFormatDesc(DeckLinkBadFormat, "Invalid or unsupported output format, use <mode>[/3D]");
ExpDesc DeckLinkInternalErrorDesc(DeckLinkInternalError, "DeckLink API internal error, please report");

// DeckLink object initialization
static int DeckLink_init(DeckLink *self, PyObject *args, PyObject *kwds)
{
	IDeckLinkIterator*				pIterator;
	IDeckLinkAttributes*			pAttributes;
	IDeckLinkDisplayModeIterator*	pDisplayModeIterator;
	IDeckLinkDisplayMode*			pDisplayMode;
	IDeckLink*						pDL;
	char*							p3D;
	BOOL							flag;
	size_t							len;
	int								i;
	uint32_t						displayFlags;
	BMDVideoOutputFlags				outputFlags;
	BMDDisplayModeSupport			support;
	uint32_t*						bytes;


	// material ID
	short cardIdx = 0;
	// texture ID
	char *format = NULL;

	static const char *kwlist[] = {"cardIdx", "format", NULL};
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|hs",
		const_cast<char**>(kwlist), &cardIdx, &format))
		return -1; 
	
	try {
		if (format == NULL) {
			THRWEXCP(AutoDetectionNotAvail, S_OK);
		}

		if ((p3D = strchr(format, '/')) != NULL && strcmp(p3D, "/3D"))
			THRWEXCP(DeckLinkBadFormat, S_OK);
		self->mUse3D = (p3D) ? true : false;
		// read the mode
		len = (p3D) ? (size_t)(p3D - format) : strlen(format);
		// throws if bad mode
		decklink_ReadDisplayMode(format, len, &self->mDisplayMode);

		pIterator = BMD_CreateDeckLinkIterator();
		pDL = NULL;
		if (pIterator) {
			i = 0;
			while (pIterator->Next(&pDL) == S_OK) {
				if (i == cardIdx) {
					break;
				}
				i++;
				pDL->Release();
				pDL = NULL;
			}
			pIterator->Release();
		}

		if (!pDL) {
			THRWEXCP(DeckLinkOpenCard, S_OK);
		}
		// detect the capabilities
		if (pDL->QueryInterface(IID_IDeckLinkAttributes, (void**)&pAttributes) == S_OK) {
			if (pAttributes->GetFlag(BMDDeckLinkSupportsInternalKeying, &flag) == S_OK && flag) {
				self->mKeyingSupported = true;
				if (pAttributes->GetFlag(BMDDeckLinkSupportsHDKeying, &flag) == S_OK && flag) {
					self->mHDKeyingSupported = true;
				}
			}
			pAttributes->Release();
		}

		if (pDL->QueryInterface(IID_IDeckLinkOutput, (void**)&self->mDLOutput) != S_OK) {
			self->mDLOutput = NULL;
		}
		if (self->mKeyingSupported) {
			pDL->QueryInterface(IID_IDeckLinkKeyer, (void **)&self->mKeyer);
		}
		// we don't need the device anymore, release to avoid leaking
		pDL->Release();

		if (!self->mDLOutput)
			THRWEXCP(DeckLinkOpenCard, S_OK);

		if (self->mDLOutput->GetDisplayModeIterator(&pDisplayModeIterator) != S_OK)
			THRWEXCP(DeckLinkInternalError, S_OK);

		displayFlags = (self->mUse3D) ? bmdDisplayModeSupports3D : 0;
		outputFlags = (self->mUse3D) ? bmdVideoOutputDualStream3D : bmdVideoOutputFlagDefault;
		pDisplayMode = NULL;
		i = 0;
		while (pDisplayModeIterator->Next(&pDisplayMode) == S_OK) {
			if (pDisplayMode->GetDisplayMode() == self->mDisplayMode
				&& (pDisplayMode->GetFlags() & displayFlags) == displayFlags) {
				if (self->mDLOutput->DoesSupportVideoMode(self->mDisplayMode, bmdFormat8BitBGRA, outputFlags, &support, NULL) != S_OK ||
				    support == bmdDisplayModeNotSupported)
				{
					printf("Warning: DeckLink card %d reports no BGRA support, proceed anyway\n", cardIdx);
				}
				break;
			}
			pDisplayMode->Release();
			pDisplayMode = NULL;
			i++;
		}
		pDisplayModeIterator->Release();

		if (!pDisplayMode)
			THRWEXCP(DeckLinkBadFormat, S_OK);
		self->mSize[0] = pDisplayMode->GetWidth();
		self->mSize[1] = pDisplayMode->GetHeight();
		self->mFrameSize = 4*self->mSize[0]*self->mSize[1];
		pDisplayMode->Release();
		if (self->mDLOutput->EnableVideoOutput(self->mDisplayMode, outputFlags) != S_OK)
			// this shouldn't fail
			THRWEXCP(DeckLinkOpenCard, S_OK);

		if (self->mDLOutput->CreateVideoFrame(self->mSize[0], self->mSize[1], self->mSize[0] * 4, bmdFormat8BitBGRA, bmdFrameFlagFlipVertical, &self->mLeftFrame) != S_OK)
			THRWEXCP(DeckLinkInternalError, S_OK);
		// clear alpha channel in the frame buffer
		self->mLeftFrame->GetBytes((void **)&bytes);
		memset(bytes, 0, self->mFrameSize);
		if (self->mUse3D) {
			if (self->mDLOutput->CreateVideoFrame(self->mSize[0], self->mSize[1], self->mSize[0] * 4, bmdFormat8BitBGRA, bmdFrameFlagFlipVertical, &self->mRightFrame) != S_OK)
				THRWEXCP(DeckLinkInternalError, S_OK);
			// clear alpha channel in the frame buffer
			self->mRightFrame->GetBytes((void **)&bytes);
			memset(bytes, 0, self->mFrameSize);
		}
	}
	catch (Exception & exp)
	{ 
		printf("DeckLink: exception when opening card %d: %s\n", cardIdx, exp.what());
		exp.report(); 
		// normally, the object should be deallocated
		return -1;
	}
	// initialization succeeded
	return 0;
}


// close added decklink
PyObject *DeckLink_close(DeckLink * self)
{
	if (self->mLeftFrame)
		self->mLeftFrame->Release();
	if (self->mRightFrame)
		self->mRightFrame->Release();
	if (self->mKeyer)
		self->mKeyer->Release();
	if (self->mDLOutput)
		self->mDLOutput->Release();
	decklink_Reset(self);
	Py_RETURN_NONE;
}


// refresh decklink key frame
static PyObject *DeckLink_refresh(DeckLink *self, PyObject *args)
{
	// get parameter - refresh source
	PyObject *param;
	double ts = -1.0;

	if (!PyArg_ParseTuple(args, "O|d:refresh", &param, &ts) || !PyBool_Check(param)) {
		// report error
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return NULL;
	}
	// some trick here: we are in the business of loading a key frame in decklink,
	// no use to do it if we are still in the same rendering frame.
	// We find this out by looking at the engine current clock time
	KX_KetsjiEngine* engine = KX_GetActiveEngine();
	if (engine->GetClockTime() != self->m_lastClock) 
	{
		self->m_lastClock = engine->GetClockTime();
		// set source refresh
		bool refreshSource = (param == Py_True);
		uint32_t *leftEye = NULL;
		uint32_t *rightEye = NULL;
		// try to process key frame from source
		try {
			// check if optimization is possible
			if (self->m_leftEye != NULL) {
				ImageBase *leftImage = self->m_leftEye->m_image;
				short * srcSize = leftImage->getSize();
				self->mLeftFrame->GetBytes((void **)&leftEye);
				if (srcSize[0] == self->mSize[0] && srcSize[1] == self->mSize[1])
				{
					// buffer has same size, can load directly
					if (!leftImage->loadImage(leftEye, self->mFrameSize, GL_BGRA, ts))
						leftEye = NULL;
				}
				else {
					// scaling is required, go the hard way
					unsigned int *src = leftImage->getImage(0, ts);
					if (src != NULL)
						decklink_ConvImage(leftEye, self->mSize, src, srcSize, self->mUseExtend);
					else
						leftEye = NULL;
				}
			}
			if (leftEye) {
				if (self->mUse3D && self->m_rightEye != NULL) {
					ImageBase *rightImage = self->m_rightEye->m_image;
					short * srcSize = rightImage->getSize();
					self->mRightFrame->GetBytes((void **)&rightEye);
					if (srcSize[0] == self->mSize[0] && srcSize[1] == self->mSize[1])
					{
						// buffer has same size, can load directly
						rightImage->loadImage(rightEye, self->mFrameSize, GL_BGRA, ts);
					}
					else {
						// scaling is required, go the hard way
						unsigned int *src = rightImage->getImage(0, ts);
						if (src != NULL)
							decklink_ConvImage(rightEye, self->mSize, src, srcSize, self->mUseExtend);
					}
				}
				if (self->mUse3D) {
					DeckLink3DFrameWrapper frame3D(
						(IDeckLinkVideoFrame*)self->mLeftFrame,
						(IDeckLinkVideoFrame*)self->mRightFrame);
					self->mDLOutput->DisplayVideoFrameSync(&frame3D);
				}
				else {
					self->mDLOutput->DisplayVideoFrameSync((IDeckLinkVideoFrame*)self->mLeftFrame);
				}
			}
			// refresh texture source, if required
			if (refreshSource) {
				if (self->m_leftEye)
					self->m_leftEye->m_image->refresh();
				if (self->m_rightEye)
					self->m_rightEye->m_image->refresh();
			}
		}
		CATCH_EXCP;
	}
	Py_RETURN_NONE;
}

// get source object
static PyObject *DeckLink_getSource(DeckLink *self, PyObject *value, void *closure)
{
	// if source exists
	if (self->m_leftEye != NULL) {
		Py_INCREF(self->m_leftEye);
		return reinterpret_cast<PyObject*>(self->m_leftEye);
	}
	// otherwise return None
	Py_RETURN_NONE;
}


// set source object
int DeckLink_setSource(DeckLink *self, PyObject *value, void *closure)
{
	// check new value
	if (value == NULL || !pyImageTypes.in(Py_TYPE(value))) {
		// report value error
		PyErr_SetString(PyExc_TypeError, "Invalid type of value");
		return -1;
	}
	// increase ref count for new value
	Py_INCREF(value);
	// release previous
	Py_XDECREF(self->m_leftEye);
	// set new value
	self->m_leftEye = reinterpret_cast<PyImage*>(value);
	// return success
	return 0;
}

// get source object
static PyObject *DeckLink_getRight(DeckLink *self, PyObject *value, void *closure)
{
	// if source exists
	if (self->m_rightEye != NULL)
	{
		Py_INCREF(self->m_rightEye);
		return reinterpret_cast<PyObject*>(self->m_rightEye);
	}
	// otherwise return None
	Py_RETURN_NONE;
}


// set source object
static int DeckLink_setRight(DeckLink *self, PyObject *value, void *closure)
{
	// check new value
	if (value == NULL || !pyImageTypes.in(Py_TYPE(value)))
	{
		// report value error
		PyErr_SetString(PyExc_TypeError, "Invalid type of value");
		return -1;
	}
	// increase ref count for new value
	Py_INCREF(value);
	// release previous
	Py_XDECREF(self->m_rightEye);
	// set new value
	self->m_rightEye = reinterpret_cast<PyImage*>(value);
	// return success
	return 0;
}


static PyObject *DeckLink_getKeying(DeckLink *self, PyObject *value, void *closure)
{
	if (self->mUseKeying) Py_RETURN_TRUE;
	else Py_RETURN_FALSE;
}

static int DeckLink_setKeying(DeckLink *self, PyObject *value, void *closure)
{
	if (value == NULL || !PyBool_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return -1;
	}
	if (self->mKeyer != NULL)
	{
		if (value == Py_True)
		{
			if (self->mKeyer->Enable(false) != S_OK)
			{
				PyErr_SetString(PyExc_RuntimeError, "Error enabling keyer");
				return -1;
			}
			self->mUseKeying = true;
			self->mKeyer->SetLevel(self->mKeyingLevel);
		}
		else
		{
			self->mKeyer->Disable();
			self->mUseKeying = false;
		}
	}
	// success
	return 0;
}

static PyObject *DeckLink_getLevel(DeckLink *self, PyObject *value, void *closure)
{
	return Py_BuildValue("h", self->mKeyingLevel);
}

static int DeckLink_setLevel(DeckLink *self, PyObject *value, void *closure)
{
	long level;
	if (value == NULL || !PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "The value must be an integer from 0 to 255");
		return -1;
	}
	level = PyLong_AsLong(value);
	if (level > 255)
		level = 255;
	else if (level < 0)
		level = 0;
	self->mKeyingLevel = (uint8_t)level;
	if (self->mUseKeying) {
		if (self->mKeyer->SetLevel(self->mKeyingLevel) != S_OK) {
			PyErr_SetString(PyExc_RuntimeError, "Error changin level of keyer");
			return -1;
		}
	}
	// success
	return 0;
}

static PyObject *DeckLink_getExtend(DeckLink *self, PyObject *value, void *closure)
{
	if (self->mUseExtend) Py_RETURN_TRUE;
	else Py_RETURN_FALSE;
}

static int DeckLink_setExtend(DeckLink *self, PyObject *value, void *closure)
{
	if (value == NULL || !PyBool_Check(value))
	{
		PyErr_SetString(PyExc_TypeError, "The value must be a bool");
		return -1;
	}
	self->mUseExtend = (value == Py_True);
	return 0;
}

// class DeckLink methods
static PyMethodDef decklinkMethods[] =
{
	{ "close", (PyCFunction)DeckLink_close, METH_NOARGS, "Close dynamic decklink and restore original"},
	{ "refresh", (PyCFunction)DeckLink_refresh, METH_VARARGS, "Refresh decklink from source"},
	{NULL}  /* Sentinel */
};

// class DeckLink attributes
static PyGetSetDef decklinkGetSets[] =
{ 
	{ (char*)"source", (getter)DeckLink_getSource, (setter)DeckLink_setSource, (char*)"source of decklink (left eye)", NULL},
	{ (char*)"right", (getter)DeckLink_getRight, (setter)DeckLink_setRight, (char*)"source of decklink (right eye)", NULL },
	{ (char*)"keying", (getter)DeckLink_getKeying, (setter)DeckLink_setKeying, (char*)"whether keying is enabled (frame is alpha-composited with passthrough output)", NULL },
	{ (char*)"level", (getter)DeckLink_getLevel, (setter)DeckLink_setLevel, (char*)"change the level of keying (overall alpha level of key frame, 0 to 255)", NULL },
	{ (char*)"extend", (getter)DeckLink_getExtend, (setter)DeckLink_setExtend, (char*)"whether image should stretched to fit frame", NULL },
	{ NULL }
};


// class DeckLink declaration
PyTypeObject DeckLinkType =
{
	PyVarObject_HEAD_INIT(NULL, 0)
	"VideoTexture.DeckLink",   /*tp_name*/
	sizeof(DeckLink),           /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)DeckLink_dealloc,/*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	&imageBufferProcs,         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"DeckLink objects",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	decklinkMethods,      /* tp_methods */
	0,                   /* tp_members */
	decklinkGetSets,            /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)DeckLink_init,    /* tp_init */
	0,                         /* tp_alloc */
	DeckLink_new,               /* tp_new */
};

#endif	/* WITH_GAMEENGINE_DECKLINK */
