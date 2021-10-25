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

/** \file gameengine/VideoTexture/VideoDeckLink.cpp
 *  \ingroup bgevideotex
 */

#ifdef WITH_GAMEENGINE_DECKLINK

// FFmpeg defines its own version of stdint.h on Windows.
// Decklink needs FFmpeg, so it uses its version of stdint.h
// this is necessary for INT64_C macro
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
// this is necessary for UINTPTR_MAX (used by atomic-ops)
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#ifdef __STDC_LIMIT_MACROS  /* else it may be unused */
#endif
#endif
#include <stdint.h>
#include <string.h>
#ifndef WIN32
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#endif

#include "atomic_ops.h"

#include "MEM_guardedalloc.h"
#include "PIL_time.h"
#include "VideoDeckLink.h"
#include "DeckLink.h"
#include "Exception.h"
#include "KX_KetsjiEngine.h"
#include "KX_PythonInit.h"

extern ExceptionID DeckLinkInternalError;
ExceptionID SourceVideoOnlyCapture, VideoDeckLinkBadFormat, VideoDeckLinkOpenCard, VideoDeckLinkDvpInternalError, VideoDeckLinkPinMemoryError;
ExpDesc SourceVideoOnlyCaptureDesc(SourceVideoOnlyCapture, "This video source only allows live capture");
ExpDesc VideoDeckLinkBadFormatDesc(VideoDeckLinkBadFormat, "Invalid or unsupported capture format, should be <mode>/<pixel>[/3D]");
ExpDesc VideoDeckLinkOpenCardDesc(VideoDeckLinkOpenCard, "Cannot open capture card, check if driver installed");
ExpDesc VideoDeckLinkDvpInternalErrorDesc(VideoDeckLinkDvpInternalError, "DVP API internal error, please report");
ExpDesc VideoDeckLinkPinMemoryErrorDesc(VideoDeckLinkPinMemoryError, "Error pinning memory");


#ifdef WIN32
////////////////////////////////////////////
// SynInfo
//
// Sets up a semaphore which is shared between the GPU and CPU and used to
// synchronise access to DVP buffers.
#define DVP_CHECK(cmd)	if ((cmd) != DVP_STATUS_OK) THRWEXCP(VideoDeckLinkDvpInternalError, S_OK)

struct SyncInfo
{
	SyncInfo(uint32_t semaphoreAllocSize, uint32_t semaphoreAddrAlignment)
	{
		mSemUnaligned = (uint32_t*)malloc(semaphoreAllocSize + semaphoreAddrAlignment - 1);

		// Apply alignment constraints
		uint64_t val = (uint64_t)mSemUnaligned;
		val += semaphoreAddrAlignment - 1;
		val &= ~((uint64_t)semaphoreAddrAlignment - 1);
		mSem = (uint32_t*)val;

		// Initialise
		mSem[0] = 0;
		mReleaseValue = 0;
		mAcquireValue = 0;

		// Setup DVP sync object and import it
		DVPSyncObjectDesc syncObjectDesc;
		syncObjectDesc.externalClientWaitFunc = NULL;
		syncObjectDesc.sem = (uint32_t*)mSem;

		DVP_CHECK(dvpImportSyncObject(&syncObjectDesc, &mDvpSync));

	}
	~SyncInfo()
	{
		dvpFreeSyncObject(mDvpSync);
		free((void*)mSemUnaligned);
	}

	volatile uint32_t*	mSem;
	volatile uint32_t*	mSemUnaligned;
	volatile uint32_t	mReleaseValue;
	volatile uint32_t	mAcquireValue;
	DVPSyncObjectHandle	mDvpSync;
};

////////////////////////////////////////////
// TextureTransferDvp: transfer with GPUDirect
////////////////////////////////////////////

class TextureTransferDvp : public TextureTransfer
{
public:
	TextureTransferDvp(DVPBufferHandle dvpTextureHandle, TextureDesc *pDesc, void *address, uint32_t allocatedSize)
	{
		DVPSysmemBufferDesc sysMemBuffersDesc;

		mExtSync = NULL;
		mGpuSync = NULL;
		mDvpSysMemHandle = 0;
		mDvpTextureHandle = 0;
		mTextureHeight = 0;
		mAllocatedSize = 0;
		mBuffer = NULL;

		if (!_PinBuffer(address, allocatedSize))
			THRWEXCP(VideoDeckLinkPinMemoryError, S_OK);
		mAllocatedSize = allocatedSize;
		mBuffer = address;

		try {
			if (!mBufferAddrAlignment) {
				DVP_CHECK(dvpGetRequiredConstantsGLCtx(&mBufferAddrAlignment, &mBufferGpuStrideAlignment,
					&mSemaphoreAddrAlignment, &mSemaphoreAllocSize,
					&mSemaphorePayloadOffset, &mSemaphorePayloadSize));
			}
			mExtSync = new SyncInfo(mSemaphoreAllocSize, mSemaphoreAddrAlignment);
			mGpuSync = new SyncInfo(mSemaphoreAllocSize, mSemaphoreAddrAlignment);
			sysMemBuffersDesc.width = pDesc->width;
			sysMemBuffersDesc.height = pDesc->height;
			sysMemBuffersDesc.stride = pDesc->stride;
			switch (pDesc->format) {
				case GL_RED_INTEGER:
					sysMemBuffersDesc.format = DVP_RED_INTEGER;
					break;
				default:
					sysMemBuffersDesc.format = DVP_BGRA;
					break;
			}
			switch (pDesc->type) {
				case GL_UNSIGNED_BYTE:
					sysMemBuffersDesc.type = DVP_UNSIGNED_BYTE;
					break;
				case GL_UNSIGNED_INT_2_10_10_10_REV:
					sysMemBuffersDesc.type = DVP_UNSIGNED_INT_2_10_10_10_REV;
					break;
				case GL_UNSIGNED_INT_8_8_8_8:
					sysMemBuffersDesc.type = DVP_UNSIGNED_INT_8_8_8_8;
					break;
				case GL_UNSIGNED_INT_10_10_10_2:
					sysMemBuffersDesc.type = DVP_UNSIGNED_INT_10_10_10_2;
					break;
				default:
					sysMemBuffersDesc.type = DVP_UNSIGNED_INT;
					break;
			}
			sysMemBuffersDesc.size = pDesc->width * pDesc->height * 4;
			sysMemBuffersDesc.bufAddr = mBuffer;
			DVP_CHECK(dvpCreateBuffer(&sysMemBuffersDesc, &mDvpSysMemHandle));
			DVP_CHECK(dvpBindToGLCtx(mDvpSysMemHandle));
			mDvpTextureHandle = dvpTextureHandle;
			mTextureHeight = pDesc->height;
		}
		catch (Exception &) {
			clean();
			throw;
		}
	}
	~TextureTransferDvp()
	{
		clean();
	}

	virtual void PerformTransfer()
	{
		// perform the transfer
		// tell DVP that the old texture buffer will no longer be used
		dvpMapBufferEndAPI(mDvpTextureHandle);
		// do we need this?
		mGpuSync->mReleaseValue++;
		dvpBegin();
		// Copy from system memory to GPU texture
		dvpMapBufferWaitDVP(mDvpTextureHandle);
		dvpMemcpyLined(mDvpSysMemHandle, mExtSync->mDvpSync, mExtSync->mAcquireValue, DVP_TIMEOUT_IGNORED,
			mDvpTextureHandle, mGpuSync->mDvpSync, mGpuSync->mReleaseValue, 0, mTextureHeight);
		dvpMapBufferEndDVP(mDvpTextureHandle);
		dvpEnd();
		dvpMapBufferWaitAPI(mDvpTextureHandle);
		// the transfer is now complete and the texture is ready for use
	}

private:
	static uint32_t			mBufferAddrAlignment;
	static uint32_t			mBufferGpuStrideAlignment;
	static uint32_t			mSemaphoreAddrAlignment;
	static uint32_t			mSemaphoreAllocSize;
	static uint32_t			mSemaphorePayloadOffset;
	static uint32_t			mSemaphorePayloadSize;

	void clean()
	{
		if (mDvpSysMemHandle) {
			dvpUnbindFromGLCtx(mDvpSysMemHandle);
			dvpDestroyBuffer(mDvpSysMemHandle);
		}
		if (mExtSync)
			delete mExtSync;
		if (mGpuSync)
			delete mGpuSync;
		if (mBuffer)
			_UnpinBuffer(mBuffer, mAllocatedSize);
	}
	SyncInfo*				mExtSync;
	SyncInfo*				mGpuSync;
	DVPBufferHandle			mDvpSysMemHandle;
	DVPBufferHandle			mDvpTextureHandle;
	uint32_t				mTextureHeight;
	uint32_t				mAllocatedSize;
	void*					mBuffer;
};

uint32_t	TextureTransferDvp::mBufferAddrAlignment;
uint32_t	TextureTransferDvp::mBufferGpuStrideAlignment;
uint32_t	TextureTransferDvp::mSemaphoreAddrAlignment;
uint32_t	TextureTransferDvp::mSemaphoreAllocSize;
uint32_t	TextureTransferDvp::mSemaphorePayloadOffset;
uint32_t	TextureTransferDvp::mSemaphorePayloadSize;

#endif

////////////////////////////////////////////
// TextureTransferOGL: transfer using standard OGL buffers
////////////////////////////////////////////

class TextureTransferOGL : public TextureTransfer
{
public:
	TextureTransferOGL(GLuint texId, TextureDesc *pDesc, void *address)
	{
		memcpy(&mDesc, pDesc, sizeof(mDesc));
		mTexId = texId;
		mBuffer = address;

		// as we cache transfer object, we will create one texture to hold the buffer
		glGenBuffers(1, &mUnpinnedTextureBuffer);
		// create a storage for it
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mUnpinnedTextureBuffer);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, pDesc->size, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
	~TextureTransferOGL()
	{
		glDeleteBuffers(1, &mUnpinnedTextureBuffer);
	}

	virtual void PerformTransfer()
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mUnpinnedTextureBuffer);
		glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, mDesc.size, mBuffer);
		glBindTexture(GL_TEXTURE_2D, mTexId);
		// NULL for last arg indicates use current GL_PIXEL_UNPACK_BUFFER target as texture data
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mDesc.width, mDesc.height, mDesc.format, mDesc.type, NULL);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
private:
	// intermediate texture to receive the buffer
	GLuint mUnpinnedTextureBuffer;
	// target texture to receive the image
	GLuint mTexId;
	// buffer
	void *mBuffer;
	// characteristic of the image
	TextureDesc mDesc;
};

////////////////////////////////////////////
// TextureTransferPMB: transfer using pinned memory buffer
////////////////////////////////////////////

class TextureTransferPMD : public TextureTransfer
{
public:
	TextureTransferPMD(GLuint texId, TextureDesc *pDesc, void *address, uint32_t allocatedSize)
	{
		memcpy(&mDesc, pDesc, sizeof(mDesc));
		mTexId = texId;
		mBuffer = address;
		mAllocatedSize = allocatedSize;

		_PinBuffer(address, allocatedSize);

		// as we cache transfer object, we will create one texture to hold the buffer
		glGenBuffers(1, &mPinnedTextureBuffer);
		// create a storage for it
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, mPinnedTextureBuffer);
		glBufferData(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, pDesc->size, address, GL_STREAM_DRAW);
		glBindBuffer(GL_EXTERNAL_VIRTUAL_MEMORY_BUFFER_AMD, 0);
	}
	~TextureTransferPMD()
	{
		glDeleteBuffers(1, &mPinnedTextureBuffer);
        if (mBuffer)
            _UnpinBuffer(mBuffer, mAllocatedSize);
    }

	virtual void PerformTransfer()
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mPinnedTextureBuffer);
		glBindTexture(GL_TEXTURE_2D, mTexId);
		// NULL for last arg indicates use current GL_PIXEL_UNPACK_BUFFER target as texture data
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mDesc.width, mDesc.height, mDesc.format, mDesc.type, NULL);
		// wait for the trasnfer to complete
		GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 40 * 1000 * 1000);	// timeout in nanosec
		glDeleteSync(fence);
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}
private:
	// intermediate texture to receive the buffer
	GLuint mPinnedTextureBuffer;
	// target texture to receive the image
	GLuint mTexId;
	// buffer
	void *mBuffer;
    // the allocated size
    uint32_t mAllocatedSize;
    // characteristic of the image
	TextureDesc mDesc;
};

bool TextureTransfer::_PinBuffer(void *address, uint32_t size)
{
#ifdef WIN32
	return VirtualLock(address, size);
#elif defined(_POSIX_MEMLOCK_RANGE)
    return !mlock(address, size);
#endif
}

void TextureTransfer::_UnpinBuffer(void* address, uint32_t size)
{
#ifdef WIN32
	VirtualUnlock(address, size);
#elif defined(_POSIX_MEMLOCK_RANGE)
    munlock(address, size);
#endif
}



////////////////////////////////////////////
// PinnedMemoryAllocator
////////////////////////////////////////////


// static members
bool		PinnedMemoryAllocator::mGPUDirectInitialized = false;
bool		PinnedMemoryAllocator::mHasDvp = false;
bool		PinnedMemoryAllocator::mHasAMDPinnedMemory = false;
size_t		PinnedMemoryAllocator::mReservedProcessMemory = 0;

bool PinnedMemoryAllocator::ReserveMemory(size_t size)
{
#ifdef WIN32
	// Increase the process working set size to allow pinning of memory.
	if (size <= mReservedProcessMemory)
		return true;
	SIZE_T dwMin = 0, dwMax = 0;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());
	if (!hProcess)
		return false;

	// Retrieve the working set size of the process.
	if (!dwMin && !GetProcessWorkingSetSize(hProcess, &dwMin, &dwMax))
		return false;

	BOOL res = SetProcessWorkingSetSize(hProcess, (size - mReservedProcessMemory) + dwMin, (size - mReservedProcessMemory) + dwMax);
	if (!res)
		return false;
	mReservedProcessMemory = size;
	CloseHandle(hProcess);
	return true;
#else
	struct rlimit rlim;
	if (getrlimit(RLIMIT_MEMLOCK, &rlim) == 0) {
		if (rlim.rlim_cur < size) {
			if (rlim.rlim_max < size)
				rlim.rlim_max = size;
			rlim.rlim_cur = size;
			return !setrlimit(RLIMIT_MEMLOCK, &rlim);
		}
	}
	return false;
#endif
}

PinnedMemoryAllocator::PinnedMemoryAllocator(unsigned cacheSize, size_t memSize) :
mRefCount(1U),
#ifdef WIN32
mDvpCaptureTextureHandle(0),
#endif
mTexId(0),
mBufferCacheSize(cacheSize)
{
	pthread_mutex_init(&mMutex, NULL);
	// do it once
	if (!mGPUDirectInitialized) {
#ifdef WIN32
		// In windows, AMD_pinned_memory option is not available, 
		// we must use special DVP API only available for Quadro cards
		const char* renderer = (const char *)glGetString(GL_RENDERER);
		mHasDvp = (strstr(renderer, "Quadro") != NULL);

		if (mHasDvp) {
			// In case the DLL is not in place, don't fail, just fallback on OpenGL
			if (dvpInitGLContext(DVP_DEVICE_FLAGS_SHARE_APP_CONTEXT) != DVP_STATUS_OK) {
				printf("Warning: Could not initialize DVP context, fallback on OpenGL transfer.\nInstall dvp.dll to take advantage of nVidia GPUDirect.\n");
				mHasDvp = false;
			}
		}
#endif
		if (GLEW_AMD_pinned_memory)
			mHasAMDPinnedMemory = true;

		mGPUDirectInitialized = true;
	}
	if (mHasDvp || mHasAMDPinnedMemory) {
		ReserveMemory(memSize);
	}
}

PinnedMemoryAllocator::~PinnedMemoryAllocator()
{
	void *address;
	// first clean the cache if not already done
	while (!mBufferCache.empty()) {
		address = mBufferCache.back();
		mBufferCache.pop_back();
		_ReleaseBuffer(address);
	}
	// clean preallocated buffers
	while (!mAllocatedSize.empty()) {
		address = mAllocatedSize.begin()->first;
		_ReleaseBuffer(address);
	}

#ifdef WIN32
	if (mDvpCaptureTextureHandle)
		dvpDestroyBuffer(mDvpCaptureTextureHandle);
#endif
}

void PinnedMemoryAllocator::TransferBuffer(void* address, TextureDesc* texDesc, GLuint texId)
{
	uint32_t allocatedSize = 0;
	TextureTransfer *pTransfer = NULL;

	Lock();
	if (mAllocatedSize.count(address) > 0)
		allocatedSize = mAllocatedSize[address];
	Unlock();
	if (!allocatedSize)
		// internal error!!
		return;
	if (mTexId != texId)
	{
		// first time we try to send data to the GPU, allocate a buffer for the texture
		glBindTexture(GL_TEXTURE_2D, texId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_2D, 0, texDesc->internalFormat, texDesc->width, texDesc->height, 0, texDesc->format, texDesc->type, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
		mTexId = texId;
	}
#ifdef WIN32
	if (mHasDvp)
	{
		if (!mDvpCaptureTextureHandle)
		{
			// bind DVP to the OGL texture
			DVP_CHECK(dvpCreateGPUTextureGL(texId, &mDvpCaptureTextureHandle));
		}
	}
#endif
	Lock();
	if (mPinnedBuffer.count(address) > 0)
	{
		pTransfer = mPinnedBuffer[address];
	}
	Unlock();
	if (!pTransfer)
	{
#ifdef WIN32
		if (mHasDvp)
			pTransfer = new TextureTransferDvp(mDvpCaptureTextureHandle, texDesc, address, allocatedSize);
		else
#endif
		if (mHasAMDPinnedMemory) {
			pTransfer = new TextureTransferPMD(texId, texDesc, address, allocatedSize);
		}
		else {
			pTransfer = new TextureTransferOGL(texId, texDesc, address);
		}
		if (pTransfer)
		{
			Lock();
			mPinnedBuffer[address] = pTransfer;
			Unlock();
		}
	}
	if (pTransfer)
		pTransfer->PerformTransfer();
}

// IUnknown methods
HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::QueryInterface(REFIID /*iid*/, LPVOID* /*ppv*/)
{
	return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE		PinnedMemoryAllocator::AddRef(void)
{
	return atomic_add_and_fetch_uint32(&mRefCount, 1U);
}

ULONG STDMETHODCALLTYPE		PinnedMemoryAllocator::Release(void)
{
	uint32_t newCount = atomic_sub_and_fetch_uint32(&mRefCount, 1U);
	if (newCount == 0)
		delete this;
	return (ULONG)newCount;
}

// IDeckLinkMemoryAllocator methods
HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::AllocateBuffer(dl_size_t bufferSize, void* *allocatedBuffer)
{
	Lock();
	if (mBufferCache.empty())
	{
		// Allocate memory on a page boundary
		// Note: aligned alloc exist in Blender but only for small alignment, use direct allocation then.
		// Note: the DeckLink API tries to allocate up to 65 buffer in advance, we will limit this to 3
		//       because we don't need any caching
		if (mAllocatedSize.size() >= mBufferCacheSize)
			*allocatedBuffer = NULL;
		else {
#ifdef WIN32
			*allocatedBuffer = VirtualAlloc(NULL, bufferSize, MEM_COMMIT | MEM_RESERVE | MEM_WRITE_WATCH, PAGE_READWRITE);
#else
			if (posix_memalign(allocatedBuffer, 4096, bufferSize) != 0)
				*allocatedBuffer = NULL;
#endif
			mAllocatedSize[*allocatedBuffer] = bufferSize;
		}
	}
	else {
		// Re-use most recently ReleaseBuffer'd address
		*allocatedBuffer = mBufferCache.back();
		mBufferCache.pop_back();
	}
	Unlock();
	return (*allocatedBuffer) ? S_OK : E_OUTOFMEMORY;
}

HRESULT STDMETHODCALLTYPE PinnedMemoryAllocator::ReleaseBuffer(void* buffer)
{
	HRESULT result = S_OK;
	Lock();
	if (mBufferCache.size() < mBufferCacheSize) {
		mBufferCache.push_back(buffer);
	}
	else {
		result = _ReleaseBuffer(buffer);
	}
	Unlock();
	return result;
}


HRESULT PinnedMemoryAllocator::_ReleaseBuffer(void* buffer)
{
	TextureTransfer *pTransfer;
	if (mAllocatedSize.count(buffer) == 0) {
		// Internal error!!
		return S_OK;
	}
	else {
		// No room left in cache, so un-pin (if it was pinned) and free this buffer
		if (mPinnedBuffer.count(buffer) > 0) {
			pTransfer = mPinnedBuffer[buffer];
			mPinnedBuffer.erase(buffer);
			delete pTransfer;
		}
#ifdef WIN32
		VirtualFree(buffer, 0, MEM_RELEASE);
#else
		free(buffer);
#endif
		mAllocatedSize.erase(buffer);
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::Commit()
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE	PinnedMemoryAllocator::Decommit()
{
	void *buffer;
	Lock();
	while (!mBufferCache.empty()) {
		// Cleanup any frames allocated and pinned in AllocateBuffer() but not freed in ReleaseBuffer()
		buffer = mBufferCache.back();
		mBufferCache.pop_back();
		_ReleaseBuffer(buffer);
	}
	Unlock();
	return S_OK;
}


////////////////////////////////////////////
// Capture Delegate Class
////////////////////////////////////////////

CaptureDelegate::CaptureDelegate(VideoDeckLink* pOwner) : mpOwner(pOwner)
{
}

HRESULT	CaptureDelegate::VideoInputFrameArrived(IDeckLinkVideoInputFrame* inputFrame, IDeckLinkAudioInputPacket* /*audioPacket*/)
{
	if (!inputFrame) {
		// It's possible to receive a NULL inputFrame, but a valid audioPacket. Ignore audio-only frame.
		return S_OK;
	}
	if ((inputFrame->GetFlags() & bmdFrameHasNoInputSource) == bmdFrameHasNoInputSource) {
		// let's not bother transferring frames if there is no source
		return S_OK;
	}
	mpOwner->VideoFrameArrived(inputFrame);
	return S_OK;
}

HRESULT	CaptureDelegate::VideoInputFormatChanged(BMDVideoInputFormatChangedEvents notificationEvents, IDeckLinkDisplayMode *newDisplayMode, BMDDetectedVideoInputFormatFlags detectedSignalFlags)
{
	return S_OK;
}




// macro for exception handling and logging
#define CATCH_EXCP catch (Exception & exp) \
{ exp.report(); m_status = SourceError; }

// class VideoDeckLink


// constructor
VideoDeckLink::VideoDeckLink (HRESULT * hRslt) : VideoBase(),
mDLInput(NULL),
mUse3D(false),
mFrameWidth(0),
mFrameHeight(0),
mpAllocator(NULL),
mpCaptureDelegate(NULL),
mpCacheFrame(NULL),
mClosing(false)
{
	mDisplayMode = (BMDDisplayMode)0;
	mPixelFormat = (BMDPixelFormat)0;
	pthread_mutex_init(&mCacheMutex, NULL);
}

// destructor
VideoDeckLink::~VideoDeckLink ()
{
	LockCache();
	mClosing = true;
	if (mpCacheFrame)
	{
		mpCacheFrame->Release();
		mpCacheFrame = NULL;
	}
	UnlockCache();
	if (mDLInput != NULL)
	{
		// Cleanup for Capture
		mDLInput->StopStreams();
		mDLInput->SetCallback(NULL);
		mDLInput->DisableVideoInput();
		mDLInput->DisableAudioInput();
		mDLInput->FlushStreams();
		if (mDLInput->Release() != 0) {
			printf("Reference count not NULL on DeckLink device when closing it, please report!\n");
		}
		mDLInput = NULL;
	}
	
	if (mpAllocator)
	{
		// if the device was properly cleared, this should be 0
		if (mpAllocator->Release() != 0) {
			printf("Reference count not NULL on Allocator when closing it, please report!\n");
		}
		mpAllocator = NULL;
	}
	if (mpCaptureDelegate)
	{
		delete mpCaptureDelegate;
		mpCaptureDelegate = NULL;
	}
}

void VideoDeckLink::refresh(void)
{
	m_avail = false;
}

// release components
bool VideoDeckLink::release()
{
	// release
	return true;
}

// open video file
void VideoDeckLink::openFile (char *filename)
{
	// only live capture on this device
	THRWEXCP(SourceVideoOnlyCapture, S_OK);
}


// open video capture device
void VideoDeckLink::openCam (char *format, short camIdx)
{
	IDeckLinkDisplayModeIterator*	pDLDisplayModeIterator;
	BMDDisplayModeSupport			modeSupport;
	IDeckLinkDisplayMode*			pDLDisplayMode;
	IDeckLinkIterator*				pIterator;
	BMDTimeValue					frameDuration;
	BMDTimeScale					frameTimescale;
	IDeckLink*						pDL;
	uint32_t displayFlags, inputFlags; 
	char *pPixel, *p3D, *pEnd, *pSize;
	size_t len;
	int i, modeIdx, cacheSize;

	// format is constructed as <displayMode>/<pixelFormat>[/3D][:<cacheSize>]
	// <displayMode> takes the form of BMDDisplayMode identifier minus the 'bmdMode' prefix.
	//               This implementation understands all the modes defined in SDK 10.3.1 but you can alternatively
	//               use the 4 characters internal representation of the mode (e.g. 'HD1080p24' == '24ps')
	// <pixelFormat> takes the form of BMDPixelFormat identifier minus the 'bmdFormat' prefix.
	//               This implementation understand all the formats defined in SDK 10.32.1 but you can alternatively
	//               use the 4 characters internal representation of the format (e.g. '10BitRGB' == 'r210')
	// Not all combinations of mode and pixel format are possible and it also depends on the card!
	// Use /3D postfix if you are capturing a 3D stream with frame packing
	// Example: To capture FullHD 1920x1080@24Hz with 3D packing and 4:4:4 10 bits RGB pixel format, use
	// "HD1080p24/10BitRGB/3D"  (same as "24ps/r210/3D")
	// (this will be the normal capture format for FullHD on the DeckLink 4k extreme)

	if ((pSize = strchr(format, ':')) != NULL) {
		cacheSize = strtol(pSize+1, &pEnd, 10);
	}
	else {
		cacheSize = 8;
		pSize = format + strlen(format);
	}
	if ((pPixel = strchr(format, '/')) == NULL ||
		((p3D = strchr(pPixel + 1, '/')) != NULL && strncmp(p3D, "/3D", pSize-p3D)))
		THRWEXCP(VideoDeckLinkBadFormat, S_OK);
	mUse3D = (p3D) ? true : false;
	// to simplify pixel format parsing
	if (!p3D)
		p3D = pSize;

	// read the mode
	len = (size_t)(pPixel - format);
	// accept integer display mode

	try {
		// throws if bad mode
		decklink_ReadDisplayMode(format, len, &mDisplayMode);
		// found a valid mode, remember that we do not look for an index
		modeIdx = -1;
	}
	catch (Exception &) {
		// accept also purely numerical mode as a mode index
		modeIdx = strtol(format, &pEnd, 10);
		if (pEnd != pPixel || modeIdx < 0)
			// not a pure number, give up
			throw;
	}

	// skip /
	pPixel++;
	len = (size_t)(p3D - pPixel);
	// throws if bad format
	decklink_ReadPixelFormat(pPixel, len, &mPixelFormat);

	// Caution: DeckLink API used from this point, make sure entity are released before throwing
	// open the card
	pIterator = BMD_CreateDeckLinkIterator();
	if (pIterator)  {
		i = 0;
		while (pIterator->Next(&pDL) == S_OK) {
			if (i == camIdx) {
				if (pDL->QueryInterface(IID_IDeckLinkInput, (void**)&mDLInput) != S_OK)
					mDLInput = NULL;
				pDL->Release();
				break;
			}
			i++;
			pDL->Release();
		}
		pIterator->Release();
	}
	if (!mDLInput)
		THRWEXCP(VideoDeckLinkOpenCard, S_OK);

	
	// check if display mode and pixel format are supported
	if (mDLInput->GetDisplayModeIterator(&pDLDisplayModeIterator) != S_OK)
		THRWEXCP(DeckLinkInternalError, S_OK);

	pDLDisplayMode = NULL;
	displayFlags = (mUse3D) ? bmdDisplayModeSupports3D : 0;
	inputFlags = (mUse3D) ? bmdVideoInputDualStream3D : bmdVideoInputFlagDefault;
	while (pDLDisplayModeIterator->Next(&pDLDisplayMode) == S_OK)
	{
		if (modeIdx == 0 || pDLDisplayMode->GetDisplayMode() == mDisplayMode) {
			// in case we get here because of modeIdx, make sure we have mDisplayMode set
			mDisplayMode = pDLDisplayMode->GetDisplayMode();
			if ((pDLDisplayMode->GetFlags() & displayFlags) == displayFlags &&
			    mDLInput->DoesSupportVideoMode(mDisplayMode, mPixelFormat, inputFlags, &modeSupport, NULL) == S_OK &&
			    modeSupport == bmdDisplayModeSupported)
			{
				break;
			}
		}
		pDLDisplayMode->Release();
		pDLDisplayMode = NULL;
		if (modeIdx-- == 0) {
			// reached the correct mode index but it does not meet the pixel format, give up
			break;
		}
	}
	pDLDisplayModeIterator->Release();

	if (pDLDisplayMode == NULL)
		THRWEXCP(VideoDeckLinkBadFormat, S_OK);

	mFrameWidth = pDLDisplayMode->GetWidth();
	mFrameHeight = pDLDisplayMode->GetHeight();
	mTextureDesc.height = (mUse3D) ? 2 * mFrameHeight : mFrameHeight;
	pDLDisplayMode->GetFrameRate(&frameDuration, &frameTimescale);
	pDLDisplayMode->Release();
	// for information, in case the application wants to know
	m_size[0] = mFrameWidth;
	m_size[1] = mTextureDesc.height;
	m_frameRate = (float)frameTimescale / (float)frameDuration;

	switch (mPixelFormat)
	{
	case bmdFormat8BitYUV:
		// 2 pixels per word
		mTextureDesc.stride = mFrameWidth * 2;
		mTextureDesc.width = mFrameWidth / 2;
		mTextureDesc.internalFormat = GL_RGBA;
		mTextureDesc.format = GL_BGRA;
		mTextureDesc.type = GL_UNSIGNED_BYTE;
		break;
	case bmdFormat10BitYUV:
		// 6 pixels in 4 words, rounded to 48 pixels
		mTextureDesc.stride = ((mFrameWidth + 47) / 48) * 128;
		mTextureDesc.width = mTextureDesc.stride/4;
		mTextureDesc.internalFormat = GL_RGB10_A2;
		mTextureDesc.format = GL_BGRA;
		mTextureDesc.type = GL_UNSIGNED_INT_2_10_10_10_REV;
		break;
	case bmdFormat8BitARGB:
		mTextureDesc.stride = mFrameWidth * 4;
		mTextureDesc.width = mFrameWidth;
		mTextureDesc.internalFormat = GL_RGBA;
		mTextureDesc.format = GL_BGRA;
		mTextureDesc.type = GL_UNSIGNED_INT_8_8_8_8;
		break;
	case bmdFormat8BitBGRA:
		mTextureDesc.stride = mFrameWidth * 4;
		mTextureDesc.width = mFrameWidth;
		mTextureDesc.internalFormat = GL_RGBA;
		mTextureDesc.format = GL_BGRA;
		mTextureDesc.type = GL_UNSIGNED_BYTE;
		break;
	case bmdFormat10BitRGBXLE:
		// 1 pixel per word, rounded to 64 pixels
		mTextureDesc.stride = ((mFrameWidth + 63) / 64) * 256;
		mTextureDesc.width = mTextureDesc.stride/4;
		mTextureDesc.internalFormat = GL_RGB10_A2;
		mTextureDesc.format = GL_RGBA;
		mTextureDesc.type = GL_UNSIGNED_INT_10_10_10_2;
		break;
	case bmdFormat10BitRGBX:
	case bmdFormat10BitRGB:
		// 1 pixel per word, rounded to 64 pixels
		mTextureDesc.stride = ((mFrameWidth + 63) / 64) * 256;
		mTextureDesc.width = mTextureDesc.stride/4;
		mTextureDesc.internalFormat = GL_R32UI;
		mTextureDesc.format = GL_RED_INTEGER;
		mTextureDesc.type = GL_UNSIGNED_INT;
		break;
	case bmdFormat12BitRGB:
	case bmdFormat12BitRGBLE:
		// 8 pixels in 9 word
		mTextureDesc.stride = (mFrameWidth * 36) / 8;
		mTextureDesc.width = mTextureDesc.stride/4;
		mTextureDesc.internalFormat = GL_R32UI;
		mTextureDesc.format = GL_RED_INTEGER;
		mTextureDesc.type = GL_UNSIGNED_INT;
		break;
	default:
		// for unknown pixel format, this will be resolved when a frame arrives
		mTextureDesc.format = GL_RED_INTEGER;
		mTextureDesc.type = GL_UNSIGNED_INT;
		break;
	}
	// reserve memory for cache frame + 1 to accomodate for pixel format that we don't know yet
	// note: we can't use stride as it is not yet known if the pixel format is unknown
	//       use instead the frame width as in worst case it's not much different (e.g. HD720/10BITYUV: 1296 pixels versus 1280)
	// note: some pixel format take more than 4 bytes take that into account (9/8 versus 1)
	mpAllocator = new PinnedMemoryAllocator(cacheSize, mFrameWidth*mTextureDesc.height * 4 * (1+cacheSize*9/8));

	if (mDLInput->SetVideoInputFrameMemoryAllocator(mpAllocator) != S_OK)
		THRWEXCP(DeckLinkInternalError, S_OK);

	mpCaptureDelegate = new CaptureDelegate(this);
	if (mDLInput->SetCallback(mpCaptureDelegate) != S_OK)
		THRWEXCP(DeckLinkInternalError, S_OK);

	if (mDLInput->EnableVideoInput(mDisplayMode, mPixelFormat, ((mUse3D) ? bmdVideoInputDualStream3D : bmdVideoInputFlagDefault)) != S_OK)
		// this shouldn't failed, we tested above
		THRWEXCP(DeckLinkInternalError, S_OK); 

	// just in case it is needed to capture from certain cards, we don't check error because we don't need audio
	mDLInput->EnableAudioInput(bmdAudioSampleRate48kHz, bmdAudioSampleType16bitInteger, 2);

	// open base class
	VideoBase::openCam(format, camIdx);

	// ready to capture, will start when application calls play()
}

// play video
bool VideoDeckLink::play (void)
{
	try
	{
		// if object is able to play
		if (VideoBase::play())
		{
			mDLInput->FlushStreams();
			return (mDLInput->StartStreams() == S_OK);
		}
	}
	CATCH_EXCP;
	return false;
}


// pause video
bool VideoDeckLink::pause (void)
{
	try
	{
		if (VideoBase::pause())
		{
			mDLInput->PauseStreams();
			return true;
		}
	}
	CATCH_EXCP;
	return false;
}

// stop video
bool VideoDeckLink::stop (void)
{
	try
	{
		VideoBase::stop();
		mDLInput->StopStreams();
		return true;
	}
	CATCH_EXCP;
	return false;
}


// set video range
void VideoDeckLink::setRange (double start, double stop)
{
}

// set framerate
void VideoDeckLink::setFrameRate (float rate)
{
}


// image calculation
// send cache frame directly to GPU
void VideoDeckLink::calcImage (unsigned int texId, double ts)
{
	IDeckLinkVideoInputFrame* pFrame;
	LockCache();
	pFrame = mpCacheFrame;
	mpCacheFrame = NULL;
	UnlockCache();
	if (pFrame) {
		// BUG: the dvpBindToGLCtx function fails the first time it is used, don't know why.
		// This causes an exception to be thrown.
		// This should be fixed but in the meantime we will catch the exception because
		// it is crucial that we release the frame to keep the reference count right on the DeckLink device
		try {
			uint32_t rowSize = pFrame->GetRowBytes();
			uint32_t textureSize = rowSize * pFrame->GetHeight();
			void* videoPixels = NULL;
			void* rightEyePixels = NULL;
			if (!mTextureDesc.stride) {
				// we could not compute the texture size earlier (unknown pixel size)
				// let's do it now
				mTextureDesc.stride = rowSize;
				mTextureDesc.width = mTextureDesc.stride / 4;
			}
			if (mTextureDesc.stride != rowSize) {
				// unexpected frame size, ignore
				// TBD: print a warning
			}
			else {
				pFrame->GetBytes(&videoPixels);
				if (mUse3D) {
					IDeckLinkVideoFrame3DExtensions *if3DExtensions = NULL;
					IDeckLinkVideoFrame *rightEyeFrame = NULL;
					if (pFrame->QueryInterface(IID_IDeckLinkVideoFrame3DExtensions, (void **)&if3DExtensions) == S_OK &&
						if3DExtensions->GetFrameForRightEye(&rightEyeFrame) == S_OK) {
						rightEyeFrame->GetBytes(&rightEyePixels);
						textureSize += ((uint64_t)rightEyePixels - (uint64_t)videoPixels);
					}
					if (rightEyeFrame)
						rightEyeFrame->Release();
					if (if3DExtensions)
						if3DExtensions->Release();
				}
				mTextureDesc.size = mTextureDesc.width * mTextureDesc.height * 4;
				if (mTextureDesc.size == textureSize) {
					// this means that both left and right frame are contiguous and that there is no padding
					// do the transfer
					mpAllocator->TransferBuffer(videoPixels, &mTextureDesc, texId);
				}
			}
		} 
		catch (Exception &) {
			pFrame->Release();
			throw;
		}
		// this will trigger PinnedMemoryAllocator::RealaseBuffer
		pFrame->Release();
	}
	// currently we don't pass the image to the application
	m_avail = false;
}

// A frame is available from the board
// Called from an internal thread, just pass the frame to the main thread
void VideoDeckLink::VideoFrameArrived(IDeckLinkVideoInputFrame* inputFrame)
{
	IDeckLinkVideoInputFrame* pOldFrame = NULL;
	LockCache();
	if (!mClosing)
	{
		pOldFrame = mpCacheFrame;
		mpCacheFrame = inputFrame;
		inputFrame->AddRef();
	}
	UnlockCache();
	// old frame no longer needed, just release it
	if (pOldFrame)
		pOldFrame->Release();
}

// python methods

// object initialization
static int VideoDeckLink_init(PyObject *pySelf, PyObject *args, PyObject *kwds)
{
	static const char *kwlist[] = { "format", "capture", NULL };
	PyImage *self = reinterpret_cast<PyImage*>(pySelf);
	// see openCam for a description of format
	char * format = NULL;
	// capture device number, i.e. DeckLink card number, default first one
	short capt = 0;

	if (!GLEW_VERSION_1_5) {
		PyErr_SetString(PyExc_RuntimeError, "VideoDeckLink requires at least OpenGL 1.5");
		return -1;
	}
	// get parameters
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|h",
		const_cast<char**>(kwlist), &format, &capt))
		return -1; 

	try {
		// create video object
		Video_init<VideoDeckLink>(self);

		// open video source, control comes back to VideoDeckLink::openCam
		Video_open(getVideo(self), format, capt);
	}
	catch (Exception & exp) {
		exp.report();
		return -1;
	}
	// initialization succeded
	return 0;
}

// methods structure
static PyMethodDef videoMethods[] =
{ // methods from VideoBase class
	{"play", (PyCFunction)Video_play, METH_NOARGS, "Play (restart) video"},
	{"pause", (PyCFunction)Video_pause, METH_NOARGS, "pause video"},
	{"stop", (PyCFunction)Video_stop, METH_NOARGS, "stop video (play will replay it from start)"},
	{"refresh", (PyCFunction)Video_refresh, METH_VARARGS, "Refresh video - get its status"},
	{NULL}
};
// attributes structure
static PyGetSetDef videoGetSets[] =
{ // methods from VideoBase class
	{(char*)"status", (getter)Video_getStatus, NULL, (char*)"video status", NULL},
	{(char*)"framerate", (getter)Video_getFrameRate, NULL, (char*)"frame rate", NULL},
	// attributes from ImageBase class
	{(char*)"valid", (getter)Image_valid, NULL, (char*)"bool to tell if an image is available", NULL},
	{(char*)"image", (getter)Image_getImage, NULL, (char*)"image data", NULL},
	{(char*)"size", (getter)Image_getSize, NULL, (char*)"image size", NULL},
	{(char*)"scale", (getter)Image_getScale, (setter)Image_setScale, (char*)"fast scale of image (near neighbor)", NULL},
	{(char*)"flip", (getter)Image_getFlip, (setter)Image_setFlip, (char*)"flip image vertically", NULL},
	{(char*)"filter", (getter)Image_getFilter, (setter)Image_setFilter, (char*)"pixel filter", NULL},
	{NULL}
};

// python type declaration
PyTypeObject VideoDeckLinkType =
{ 
	PyVarObject_HEAD_INIT(NULL, 0)
	"VideoTexture.VideoDeckLink",   /*tp_name*/
	sizeof(PyImage),          /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	(destructor)Image_dealloc, /*tp_dealloc*/
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
	"DeckLink video source",       /* tp_doc */
	0,		               /* tp_traverse */
	0,		               /* tp_clear */
	0,		               /* tp_richcompare */
	0,		               /* tp_weaklistoffset */
	0,		               /* tp_iter */
	0,		               /* tp_iternext */
	videoMethods,    /* tp_methods */
	0,                   /* tp_members */
	videoGetSets,          /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)VideoDeckLink_init,     /* tp_init */
	0,                         /* tp_alloc */
	Image_allocNew,           /* tp_new */
};



////////////////////////////////////////////
// DeckLink Capture Delegate Class
////////////////////////////////////////////

#endif		// WITH_GAMEENGINE_DECKLINK

