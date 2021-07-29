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

/** \file gpudirect/dvpapi.h
 *  \ingroup gpudirect
 */

#ifndef __DVPAPI_H__
#define __DVPAPI_H__

#ifdef WIN32

#include <stdlib.h>
#include <stdint.h>

#include "GL/glew.h"

#if defined(__GNUC__) && __GNUC__>=4
# define DVPAPI extern __attribute__ ((visibility("default")))
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
# define DVPAPI extern __global
#else
# define DVPAPI extern
#endif

#define DVPAPIENTRY
#define DVP_MAJOR_VERSION  1
#define DVP_MINOR_VERSION  63

typedef uint64_t DVPBufferHandle;
typedef uint64_t DVPSyncObjectHandle;

typedef enum {
	DVP_STATUS_OK                        =  0, 
	DVP_STATUS_INVALID_PARAMETER         =  1,
	DVP_STATUS_UNSUPPORTED               =  2,
	DVP_STATUS_END_ENUMERATION           =  3,
	DVP_STATUS_INVALID_DEVICE            =  4,
	DVP_STATUS_OUT_OF_MEMORY             =  5,
	DVP_STATUS_INVALID_OPERATION         =  6,
	DVP_STATUS_TIMEOUT                   =  7,
	DVP_STATUS_INVALID_CONTEXT           =  8,
	DVP_STATUS_INVALID_RESOURCE_TYPE     =  9,
	DVP_STATUS_INVALID_FORMAT_OR_TYPE    =  10,
	DVP_STATUS_DEVICE_UNINITIALIZED      =  11,
	DVP_STATUS_UNSIGNALED                =  12,
	DVP_STATUS_SYNC_ERROR                =  13,
	DVP_STATUS_SYNC_STILL_BOUND          =  14,
	DVP_STATUS_ERROR                     = -1, 
} DVPStatus;

// Pixel component formats stored in the system memory buffer
// analogous to those defined in the OpenGL API, except for 
// DVP_BUFFER and the DVP_CUDA_* types. DVP_BUFFER provides 
// an unspecified format type to allow for general interpretation
// of the bytes at a later stage (in GPU shader). Note that not 
// all paths will achieve optimal speeds due to lack of HW support 
// for the transformation. The CUDA types are to be used when
// copying to/from a system memory buffer from-to a CUDA array, as the 
// CUDA array implies a memory layout that matches the array.
typedef enum {
	DVP_BUFFER,                   // Buffer treated as a raw buffer 
	                              // and copied directly into GPU buffer
	                              // without any interpretation of the
	                              // stored bytes.
	DVP_DEPTH_COMPONENT,
	DVP_RGBA,
	DVP_BGRA,
	DVP_RED,
	DVP_GREEN,
	DVP_BLUE,
	DVP_ALPHA,
	DVP_RGB,
	DVP_BGR,
	DVP_LUMINANCE,
	DVP_LUMINANCE_ALPHA,
	DVP_CUDA_1_CHANNEL,
	DVP_CUDA_2_CHANNELS,
	DVP_CUDA_4_CHANNELS,
	DVP_RGBA_INTEGER,
	DVP_BGRA_INTEGER,
	DVP_RED_INTEGER,
	DVP_GREEN_INTEGER,
	DVP_BLUE_INTEGER,
	DVP_ALPHA_INTEGER,
	DVP_RGB_INTEGER,
	DVP_BGR_INTEGER,
	DVP_LUMINANCE_INTEGER,
	DVP_LUMINANCE_ALPHA_INTEGER,
} DVPBufferFormats;

// Possible pixel component storage types for system memory buffers
typedef enum {
	DVP_UNSIGNED_BYTE,
	DVP_BYTE,
	DVP_UNSIGNED_SHORT,
	DVP_SHORT,
	DVP_UNSIGNED_INT,
	DVP_INT,
	DVP_FLOAT,
	DVP_HALF_FLOAT,
	DVP_UNSIGNED_BYTE_3_3_2,
	DVP_UNSIGNED_BYTE_2_3_3_REV,
	DVP_UNSIGNED_SHORT_5_6_5,
	DVP_UNSIGNED_SHORT_5_6_5_REV,
	DVP_UNSIGNED_SHORT_4_4_4_4,
	DVP_UNSIGNED_SHORT_4_4_4_4_REV,
	DVP_UNSIGNED_SHORT_5_5_5_1,
	DVP_UNSIGNED_SHORT_1_5_5_5_REV,
	DVP_UNSIGNED_INT_8_8_8_8,
	DVP_UNSIGNED_INT_8_8_8_8_REV,
	DVP_UNSIGNED_INT_10_10_10_2,
	DVP_UNSIGNED_INT_2_10_10_10_REV,
} DVPBufferTypes;

// System memory descriptor describing the size and storage formats
// of the buffer
typedef struct DVPSysmemBufferDescRec {
	uint32_t width;                     // Buffer Width
	uint32_t height;                    // Buffer Height
	uint32_t stride;                    // Stride
	uint32_t size;                      // Specifies the surface size if 
	                                    // format == DVP_BUFFER
	DVPBufferFormats format;            // see enum above
	DVPBufferTypes type;                // see enum above
	void *bufAddr;                      // Buffer memory address
} DVPSysmemBufferDesc;

// Flags specified at sync object creation:
// ----------------------------------------
// Tells the implementation to use events wherever
// possible instead of software spin loops. Note if HW
// wait operations are supported by the implementation
// then events will not be used in the dvpMemcpy*
// functions. In such a case, events may still be used
// in dvpSyncObjClientWait* functions.
#define DVP_SYNC_OBJECT_FLAGS_USE_EVENTS      0x00000001

typedef struct DVPSyncObjectDescRec {
	uint32_t *sem;               // Location to write semaphore value
	uint32_t  flags;             // See above DVP_SYNC_OBJECT_FLAGS_* bits
	DVPStatus (*externalClientWaitFunc) (DVPSyncObjectHandle sync, 
	                                     uint32_t value,
	                                     bool GEQ, // If true then the function should wait for the sync value to be 
	                                               // greater than or equal to the value parameter. Otherwise just a
	                                               // straight forward equality comparison should be performed.
	                                     uint64_t timeout);
	                                     // If non-null, externalClientWaitFunc allows the DVP library
	                                     // to call the application to wait for a sync object to be
	                                     // released. This allows the application to create events, 
	                                     // which can be triggered on device interrupts instead of
	                                     // using spin loops inside the DVP library. Upon succeeding
	                                     // the function must return DVP_STATUS_OK, non-zero for failure 
	                                     // and DVP_STATUS_TIMEOUT on timeout. The externalClientWaitFunc should
	                                     // not alter the current GL or CUDA context state
} DVPSyncObjectDesc;

// Time used when event timeouts should be ignored
#define DVP_TIMEOUT_IGNORED                   0xFFFFFFFFFFFFFFFFull

typedef DVPStatus (DVPAPIENTRY * PFNDVPBEGIN) (void);
typedef DVPStatus (DVPAPIENTRY * PFNDVPEND)   (void);
typedef DVPStatus (DVPAPIENTRY * PFNDVPCREATEBUFFER)(DVPSysmemBufferDesc *desc, DVPBufferHandle *hBuf);
typedef DVPStatus (DVPAPIENTRY * PFNDVPDESTROYBUFFER)(DVPBufferHandle  hBuf);
typedef DVPStatus (DVPAPIENTRY * PFNDVPFREEBUFFER)(DVPBufferHandle gpuBufferHandle);
typedef DVPStatus (DVPAPIENTRY * PFNDVPMEMCPYLINED)(DVPBufferHandle      srcBuffer,
						    DVPSyncObjectHandle  srcSync,
						    uint32_t             srcAcquireValue,
						    uint64_t             timeout,
						    DVPBufferHandle      dstBuffer,
						    DVPSyncObjectHandle  dstSync,
						    uint32_t             dstReleaseValue,
						    uint32_t             startingLine,
						    uint32_t             numberOfLines);
typedef DVPStatus (DVPAPIENTRY * PFNDVPMEMCPY)(DVPBufferHandle srcBuffer,
					       DVPSyncObjectHandle  srcSync,
					       uint32_t             srcAcquireValue,
					       uint64_t             timeout,
					       DVPBufferHandle      dstBuffer,
					       DVPSyncObjectHandle  dstSync,
					       uint32_t             dstReleaseValue,
					       uint32_t             srcOffset,
					       uint32_t             dstOffset,
					       uint32_t             count);
typedef DVPStatus (DVPAPIENTRY * PFNDVPIMPORTSYNCOBJECT)(DVPSyncObjectDesc *desc, 
							 DVPSyncObjectHandle *syncObject);
typedef DVPStatus (DVPAPIENTRY * PFNDVPFREESYNCOBJECT)(DVPSyncObjectHandle syncObject);
typedef DVPStatus (DVPAPIENTRY * PFNDVPGETREQUIREDCONSTANTSGLCTX)(uint32_t *bufferAddrAlignment,
								  uint32_t *bufferGPUStrideAlignment,
								  uint32_t *semaphoreAddrAlignment,
								  uint32_t *semaphoreAllocSize,
								  uint32_t *semaphorePayloadOffset,
								  uint32_t *semaphorePayloadSize);
typedef DVPStatus (DVPAPIENTRY * PFNDVPBINDTOGLCTX)(DVPBufferHandle hBuf);
typedef DVPStatus (DVPAPIENTRY * PFNDVPUNBINDFROMGLCTX)(DVPBufferHandle hBuf);
typedef DVPStatus (DVPAPIENTRY * PFNDVPMAPBUFFERENDAPI)(DVPBufferHandle gpuBufferHandle);
typedef DVPStatus (DVPAPIENTRY * PFNDVPMAPBUFFERWAITDVP)(DVPBufferHandle gpuBufferHandle);
typedef DVPStatus (DVPAPIENTRY * PFNDVPMAPBUFFERENDDVP)(DVPBufferHandle gpuBufferHandle);
typedef DVPStatus (DVPAPIENTRY * PFNDVPMAPBUFFERWAITAPI)(DVPBufferHandle gpuBufferHandle);
typedef DVPStatus (DVPAPIENTRY * PFNDVPCREATEGPUTEXTUREGL)(GLuint texID, 
							   DVPBufferHandle *bufferHandle);

// Flags supplied to the dvpInit* functions:
//
// DVP_DEVICE_FLAGS_SHARE_APP_CONTEXT is only supported for OpenGL
// contexts and is the only supported flag for CUDA. It allows for 
// certain cases to be optimized by sharing the context 
// of the application for the DVP operations. This removes the
// need to do certain synchronizations. See issue 5 for parallel
// issues. When used, the app's GL context must be current for all calls 
// to the DVP library.
// the DVP library.
#define DVP_DEVICE_FLAGS_SHARE_APP_CONTEXT    0x000000001

//------------------------------------------------------------------------
// Function:      dvpInitGLContext
//
//                To be called before any DVP resources are allocated.
//                This call allows for specification of flags that may
//                change the way DVP operations are performed. See above
//                for the list of flags.
//
//                The OpenGL context must be current at time of call.
//
// Parameters:    flags[IN]  - Buffer description structure
// 
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER 
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
extern DVPStatus dvpInitGLContext(uint32_t flags);

//------------------------------------------------------------------------
// Function:      dvpCloseGLContext
//
//                Function to be called when app closes to allow freeing
//                of any DVP library allocated resources.
//
//                The OpenGL context must be current at time of call.
//
// Parameters:    none
// 
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER    
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
extern DVPStatus dvpCloseGLContext();

//------------------------------------------------------------------------
// Function:      dvpGetLibrayVersion
//
// Description:   Returns the current version of the library
//
// Parameters:    major[OUT]     - returned major version
//                minor[OUT]     - returned minor version
//
// Returns:       DVP_STATUS_OK
//------------------------------------------------------------------------
extern DVPStatus dvpGetLibrayVersion(uint32_t *major, uint32_t *minor);

//------------------------------------------------------------------------
// Function:      dvpBegin 
//
// Description:   dvpBegin must be called before any combination of DVP
//                function calls dvpMemCpy*, dvpMapBufferWaitDVP,
//                dvpSyncObjClientWait*, and dvpMapBufferEndDVP. After 
//                the last of these functions has been called is dvpEnd
//                must be called. This allows for more efficient batched 
//                DVP operations.
//
// Parameters:    none
// 
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
#define dvpBegin DVPAPI_GET_FUN(__dvpBegin)

//------------------------------------------------------------------------
// Function:      dvpEnd
//
// Description:   dvpEnd signals the end of a batch of DVP function calls
//                that began with dvpBegin
//
// Parameters:    none
// 
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
#define dvpEnd DVPAPI_GET_FUN(__dvpEnd)


//------------------------------------------------------------------------
// Function:      dvpCreateBuffer
//
// Description:   Create a DVP buffer using system memory, wrapping a user
//                passed pointer. The pointer must be aligned 
//                to values returned by dvpGetRequiredAlignments*
//
// Parameters:    desc[IN]  - Buffer description structure
//                hBuf[OUT] - DVP Buffer handle
// 
// Returns:       DVP_STATUS_OK                
//                DVP_STATUS_INVALID_PARAMETER 
//                DVP_STATUS_ERROR           
//------------------------------------------------------------------------
#define dvpCreateBuffer DVPAPI_GET_FUN(__dvpCreateBuffer)


//------------------------------------------------------------------------
// Function:      dvpDestroyBuffer
//
// Description:   Destroy a previously created DVP buffer.
//
// Parameters:    hBuf[IN] - DVP Buffer handle
// 
// Returns:       DVP_STATUS_OK                
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR  
//------------------------------------------------------------------------
#define dvpDestroyBuffer DVPAPI_GET_FUN(__dvpDestroyBuffer)

//------------------------------------------------------------------------
// Function:      dvpFreeBuffer
//
// Description:   dvpFreeBuffer frees the DVP buffer reference
//
// Parameters:    gpuBufferHandle[IN] - DVP Buffer handle
// 
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
#define dvpFreeBuffer DVPAPI_GET_FUN(__dvpFreeBuffer)

//------------------------------------------------------------------------
// Function:      dvpMemcpyLined
//
// Description:   dvpMemcpyLined provides buffer copies between a
//                DVP sysmem buffer and a graphics API texture (as opposed to
//                a buffer type). Other buffer types (such
//                as graphics API buffers) return DVP_STATUS_INVALID_PARAMETER.
//
//                In addition, see "dvpMemcpy* general comments" above.
//
// Parameters:    srcBuffer[IN]        - src buffer handle
//                srcSync[IN]          - sync to acquire on before transfer
//                srcAcquireValue[IN]  - value to acquire on before transfer
//                timeout[IN]          - time out value in nanoseconds.
//                dstBuffer[IN]        - src buffer handle
//                dstSync[IN]          - sync to release on transfer completion
//                dstReleaseValue[IN]  - value to release on completion
//                startingLine[IN]     - starting line of buffer
//                numberOfLines[IN]    - number of lines to copy
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//
// GL state effected: The following GL state may be altered by this
//               function (not relevant if no GL source or destination
//               is used):
//                -GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS, 
//                 GL_PACK_ROW_LENGTH
//                -The buffer bound to GL_PIXEL_PACK_BUFFER
//                -The current bound framebuffer (GL_FRAMEBUFFER_EXT)
//                -GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS,
//                 GL_UNPACK_ROW_LENGTH
//                -The buffer bound to GL_PIXEL_UNPACK_BUFFER
//                -The texture bound to GL_TEXTURE_2D
//------------------------------------------------------------------------
#define dvpMemcpyLined DVPAPI_GET_FUN(__dvpMemcpyLined)


//------------------------------------------------------------------------
// Function:      dvpMemcpy
//
// Description:   dvpMemcpy provides buffer copies between a
//                DVP sysmem buffer and a graphics API pure buffer (as 
//                opposed to a texture type). Other buffer types (such
//                as graphics API textures) return 
//                DVP_STATUS_INVALID_PARAMETER.
//
//                The start address of the srcBuffer is given by srcOffset
//                and the dstBuffer start address is given by dstOffset.
//
//                In addition, see "dvpMemcpy* general comments" above.
//
// Parameters:    srcBuffer[IN]             - src buffer handle
//                srcSync[IN]               - sync to acquire on before transfer
//                srcAcquireValue[IN]       - value to acquire on before transfer
//                timeout[IN]               - time out value in nanoseconds.
//                dstBuffer[IN]             - src buffer handle
//                dstSync[IN]               - sync to release on completion
//                dstReleaseValue[IN]       - value to release on completion
//                uint32_t srcOffset[IN]    - byte offset of srcBuffer
//                uint32_t dstOffset[IN]    - byte offset of dstBuffer
//                uint32_t count[IN]        - number of bytes to copy
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//
// GL state effected: The following GL state may be altered by this
//               function (not relevant if no GL source or destination
//               is used):
//                 - The buffer bound to GL_COPY_WRITE_BUFFER
//                 - The buffer bound to GL_COPY_READ_BUFFER
// 
//------------------------------------------------------------------------
#define dvpMemcpy DVPAPI_GET_FUN(__dvpMemcpy)

//------------------------------------------------------------------------
// Function:      dvpImportSyncObject
//
// Description:   dvpImportSyncObject creates a DVPSyncObject from the 
//                DVPSyncObjectDesc. Note that a sync object is not 
//                supported for copy operations targeting different APIs.
//                This means, for example, it is illegal to call dvpMemCpy*
//                for source or target GL texture with sync object A and 
//                then later use that same sync object in dvpMemCpy* 
//                operation for a source or target CUDA buffer. The same
//                semaphore memory can still be used for two different sync
//                objects.
//
// Parameters:    desc[IN]        - data describing the sync object
//                syncObject[OUT] - handle to sync object
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
#define dvpImportSyncObject DVPAPI_GET_FUN(__dvpImportSyncObject)

//------------------------------------------------------------------------
// Function:      dvpFreeSyncObject
//
// Description:   dvpFreeSyncObject waits for any outstanding releases on 
//                this sync object before freeing the resources allocated for
//                the specified sync object. The application must make sure
//                any outstanding acquire operations have already been
//                completed.
//
//                If OpenGL is being used and the app's GL context is being
//                shared (via the DVP_DEVICE_FLAGS_SHARE_APP_CONTEXT flag),
//                then dvpFreeSyncObject needs to be called while each context,
//                on which the sync object was used, is current. If 
//                DVP_DEVICE_FLAGS_SHARE_APP_CONTEXT is used and there are out
//                standing contexts from which this sync object must be free'd
//                then dvpFreeSyncObject will return DVP_STATUS_SYNC_STILL_BOUND.
//
// Parameters:    syncObject[IN] - handle to sync object to be free'd
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//                DVP_STATUS_SYNC_STILL_BOUND
//------------------------------------------------------------------------
#define dvpFreeSyncObject DVPAPI_GET_FUN(__dvpFreeSyncObject)


//------------------------------------------------------------------------
// Function:      dvpMapBufferEndAPI
//
// Description:   Tells DVP to setup a signal for this buffer in the
//                callers API context or device. The signal follows all
//                previous API operations up to this point and, thus,
//                allows subsequent DVP calls to know when then this buffer
//                is ready for use within the DVP library. This function
//                would be followed by a call to dvpMapBufferWaitDVP to
//                synchronize rendering in the API stream and the DVP 
//                stream.
//
//                If OpenGL or CUDA is used, the OpenGL/CUDA context
//                must be current at time of call.
//
//                The use of dvpMapBufferEndAPI is NOT recommended for
//                CUDA synchronisation, as it is more optimal to use a
//                applcation CUDA stream in conjunction with 
//                dvpMapBufferEndCUDAStream. This allows the driver to 
//                do optimisations, such as parllelise the copy operations
//                and compute.
//
//                This must be called outside the dvpBegin/dvpEnd pair. In
//                addition, this call is not thread safe and must be called
//                from or fenced against the rendering thread associated with
//                the context or device.
//
// Parameters:    gpuBufferHandle[IN] - buffer to track
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//                DVP_STATUS_UNSIGNALED     - returned if the API is 
//                     unable to place a signal in the API context queue
//------------------------------------------------------------------------
#define dvpMapBufferEndAPI DVPAPI_GET_FUN(__dvpMapBufferEndAPI)

//------------------------------------------------------------------------
// Function:      dvpMapBufferEndAPI
//
// Description:   Tells DVP to setup a signal for this buffer in the
//                callers API context or device. The signal follows all
//                previous API operations up to this point and, thus,
//                allows subsequent DVP calls to know when then this buffer
//                is ready for use within the DVP library. This function
//                would be followed by a call to dvpMapBufferWaitDVP to
//                synchronize rendering in the API stream and the DVP 
//                stream.
//
//                If OpenGL or CUDA is used, the OpenGL/CUDA context
//                must be current at time of call.
//
//                The use of dvpMapBufferEndAPI is NOT recommended for
//                CUDA synchronisation, as it is more optimal to use a
//                applcation CUDA stream in conjunction with 
//                dvpMapBufferEndCUDAStream. This allows the driver to 
//                do optimisations, such as parllelise the copy operations
//                and compute.
//
//                This must be called outside the dvpBegin/dvpEnd pair. In
//                addition, this call is not thread safe and must be called
//                from or fenced against the rendering thread associated with
//                the context or device.
//
// Parameters:    gpuBufferHandle[IN] - buffer to track
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//                DVP_STATUS_UNSIGNALED     - returned if the API is 
//                     unable to place a signal in the API context queue
//------------------------------------------------------------------------
#define dvpMapBufferEndAPI DVPAPI_GET_FUN(__dvpMapBufferEndAPI)

//------------------------------------------------------------------------
// Function:      dvpMapBufferWaitDVP
//
// Description:   Tells DVP to make the DVP stream wait for a previous 
//                signal triggered by a dvpMapBufferEndAPI call.
//
//                This must be called inside the dvpBegin/dvpEnd pair.
//
// Parameters:    gpuBufferHandle[IN] - buffer to track
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
#define dvpMapBufferWaitDVP DVPAPI_GET_FUN(__dvpMapBufferWaitDVP)

//------------------------------------------------------------------------
// Function:      dvpMapBufferEndDVP
//
// Description:   Tells DVP to setup a signal for this buffer after
//                DVP operations are complete. The signal allows 
//                the API to know when then this buffer is 
//                ready for use within a API stream. This function would
//                be followed by a call to dvpMapBufferWaitAPI to
//                synchronize copies in the DVP stream and the API 
//                rendering stream.
//
//                This must be called inside the dvpBegin/dvpEnd pair.
//
// Parameters:    gpuBufferHandle[IN] - buffer to track
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
#define dvpMapBufferEndDVP DVPAPI_GET_FUN(__dvpMapBufferEndDVP)

//------------------------------------------------------------------------
// Function:      dvpMapBufferWaitAPI
//
// Description:   Tells DVP to make the current API context or device to 
//                wait for a previous signal triggered by a 
//                dvpMapBufferEndDVP call.
//
//                The use of dvpMapBufferWaitCUDAStream is NOT recommended for
//                CUDA synchronisation, as it is more optimal to use a
//                applcation CUDA stream in conjunction with 
//                dvpMapBufferEndCUDAStream. This allows the driver to 
//                do optimisations, such as parllelise the copy operations
//                and compute.
//
//                If OpenGL or CUDA is used, the OpenGL/CUDA context
//                must be current at time of call.
//
//                This must be called outside the dvpBegin/dvpEnd pair. In
//                addition, this call is not thread safe and must be called
//                from or fenced against the rendering thread associated with
//                the context or device.
//
// Parameters:    gpuBufferHandle[IN] - buffer to track
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------
#define dvpMapBufferWaitAPI DVPAPI_GET_FUN(__dvpMapBufferWaitAPI)

//------------------------------------------------------------------------
// If the multiple GL contexts used in the application access the same
// sysmem buffers, then application must create those GL contexts with
// display list shared.
//------------------------------------------------------------------------
#define dvpBindToGLCtx DVPAPI_GET_FUN(__dvpBindToGLCtx)
#define dvpGetRequiredConstantsGLCtx DVPAPI_GET_FUN(__dvpGetRequiredConstantsGLCtx)
#define dvpCreateGPUTextureGL DVPAPI_GET_FUN(__dvpCreateGPUTextureGL)
#define dvpUnbindFromGLCtx DVPAPI_GET_FUN(__dvpUnbindFromGLCtx)


DVPAPI PFNDVPBEGIN __dvpBegin;
DVPAPI PFNDVPEND __dvpEnd;
DVPAPI PFNDVPCREATEBUFFER __dvpCreateBuffer;
DVPAPI PFNDVPDESTROYBUFFER __dvpDestroyBuffer;
DVPAPI PFNDVPFREEBUFFER __dvpFreeBuffer;
DVPAPI PFNDVPMEMCPYLINED __dvpMemcpyLined;
DVPAPI PFNDVPMEMCPY __dvpMemcpy;
DVPAPI PFNDVPIMPORTSYNCOBJECT __dvpImportSyncObject;
DVPAPI PFNDVPFREESYNCOBJECT __dvpFreeSyncObject;
DVPAPI PFNDVPMAPBUFFERENDAPI __dvpMapBufferEndAPI;
DVPAPI PFNDVPMAPBUFFERWAITDVP __dvpMapBufferWaitDVP;
DVPAPI PFNDVPMAPBUFFERENDDVP __dvpMapBufferEndDVP;
DVPAPI PFNDVPMAPBUFFERWAITAPI __dvpMapBufferWaitAPI;


//------------------------------------------------------------------------
// If the multiple GL contexts used in the application access the same
// sysmem buffers, then application must create those GL contexts with
// display list shared.
//------------------------------------------------------------------------
DVPAPI PFNDVPBINDTOGLCTX __dvpBindToGLCtx;
DVPAPI PFNDVPGETREQUIREDCONSTANTSGLCTX __dvpGetRequiredConstantsGLCtx;
DVPAPI PFNDVPCREATEGPUTEXTUREGL __dvpCreateGPUTextureGL;
DVPAPI PFNDVPUNBINDFROMGLCTX __dvpUnbindFromGLCtx;

#define DVPAPI_GET_FUN(x)	x

#endif	// WIN32

#endif	// __DVPAPI_H__

