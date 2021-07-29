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

/** \file gpudirect/dvpapi.c
 *  \ingroup gpudirect
 */

#ifdef WIN32

#include <stdlib.h>
#include "dvpapi.h"

extern "C" {
#include "BLI_dynlib.h"
}

#define KDVPAPI_Name "dvp.dll"

typedef DVPStatus (DVPAPIENTRY * PFNDVPINITGLCONTEXT) (uint32_t flags);
typedef DVPStatus (DVPAPIENTRY * PFNDVPCLOSEGLCONTEXT) (void);
typedef DVPStatus (DVPAPIENTRY * PFNDVPGETLIBRARYVERSION)(uint32_t *major, uint32_t *minor);

static uint32_t __dvpMajorVersion = 0;
static uint32_t __dvpMinorVersion = 0;
static PFNDVPGETLIBRARYVERSION __dvpGetLibrayVersion = NULL;
static PFNDVPINITGLCONTEXT __dvpInitGLContext = NULL;
static PFNDVPCLOSEGLCONTEXT __dvpCloseGLContext = NULL;
PFNDVPBEGIN __dvpBegin = NULL;
PFNDVPEND __dvpEnd = NULL;
PFNDVPCREATEBUFFER __dvpCreateBuffer = NULL;
PFNDVPDESTROYBUFFER __dvpDestroyBuffer = NULL;
PFNDVPFREEBUFFER __dvpFreeBuffer = NULL;
PFNDVPMEMCPYLINED __dvpMemcpyLined = NULL;
PFNDVPMEMCPY __dvpMemcpy = NULL;
PFNDVPIMPORTSYNCOBJECT __dvpImportSyncObject = NULL;
PFNDVPFREESYNCOBJECT __dvpFreeSyncObject = NULL;
PFNDVPMAPBUFFERENDAPI __dvpMapBufferEndAPI = NULL;
PFNDVPMAPBUFFERWAITDVP __dvpMapBufferWaitDVP = NULL;
PFNDVPMAPBUFFERENDDVP __dvpMapBufferEndDVP = NULL;
PFNDVPMAPBUFFERWAITAPI __dvpMapBufferWaitAPI = NULL;
PFNDVPBINDTOGLCTX __dvpBindToGLCtx = NULL;
PFNDVPGETREQUIREDCONSTANTSGLCTX __dvpGetRequiredConstantsGLCtx = NULL;
PFNDVPCREATEGPUTEXTUREGL __dvpCreateGPUTextureGL = NULL;
PFNDVPUNBINDFROMGLCTX __dvpUnbindFromGLCtx = NULL;

static DynamicLibrary *__dvpLibrary = NULL;

DVPStatus dvpGetLibrayVersion(uint32_t *major, uint32_t *minor)
{
	if (!__dvpLibrary)
		return DVP_STATUS_ERROR;
	*major = __dvpMajorVersion;
	*minor = __dvpMinorVersion;
	return DVP_STATUS_OK;
}

DVPStatus dvpInitGLContext(uint32_t flags)
{
	DVPStatus status;
	if (!__dvpLibrary) {
		__dvpLibrary = BLI_dynlib_open(KDVPAPI_Name);
		if (!__dvpLibrary) {
			return DVP_STATUS_ERROR;
		}
//		"?dvpInitGLContext@@YA?AW4DVPStatus@@I@Z";
		__dvpInitGLContext = (PFNDVPINITGLCONTEXT)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpInitGLContext@@YA?AW4DVPStatus@@I@Z");
		__dvpCloseGLContext = (PFNDVPCLOSEGLCONTEXT)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpCloseGLContext@@YA?AW4DVPStatus@@XZ");
		__dvpGetLibrayVersion = (PFNDVPGETLIBRARYVERSION)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpGetLibrayVersion@@YA?AW4DVPStatus@@PEAI0@Z");
		__dvpBegin = (PFNDVPBEGIN)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpBegin@@YA?AW4DVPStatus@@XZ");
		__dvpEnd = (PFNDVPEND)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpEnd@@YA?AW4DVPStatus@@XZ");
		__dvpCreateBuffer = (PFNDVPCREATEBUFFER)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpCreateBuffer@@YA?AW4DVPStatus@@PEAUDVPSysmemBufferDescRec@@PEA_K@Z");
		__dvpDestroyBuffer = (PFNDVPDESTROYBUFFER)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpDestroyBuffer@@YA?AW4DVPStatus@@_K@Z");
		__dvpFreeBuffer = (PFNDVPFREEBUFFER)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpFreeBuffer@@YA?AW4DVPStatus@@_K@Z");
		__dvpMemcpyLined = (PFNDVPMEMCPYLINED)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpMemcpyLined@@YA?AW4DVPStatus@@_K0I000III@Z");
		__dvpMemcpy = (PFNDVPMEMCPY)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpMemcpy2D@@YA?AW4DVPStatus@@_K0I000IIIII@Z");
		__dvpImportSyncObject = (PFNDVPIMPORTSYNCOBJECT)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpImportSyncObject@@YA?AW4DVPStatus@@PEAUDVPSyncObjectDescRec@@PEA_K@Z");
		__dvpFreeSyncObject = (PFNDVPFREESYNCOBJECT)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpFreeSyncObject@@YA?AW4DVPStatus@@_K@Z");
		__dvpMapBufferEndAPI = (PFNDVPMAPBUFFERENDAPI)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpMapBufferEndAPI@@YA?AW4DVPStatus@@_K@Z");
		__dvpMapBufferWaitDVP = (PFNDVPMAPBUFFERWAITDVP)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpMapBufferWaitDVP@@YA?AW4DVPStatus@@_K@Z");
		__dvpMapBufferEndDVP = (PFNDVPMAPBUFFERENDDVP)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpMapBufferEndDVP@@YA?AW4DVPStatus@@_K@Z");
		__dvpMapBufferWaitAPI = (PFNDVPMAPBUFFERWAITAPI)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpMapBufferWaitAPI@@YA?AW4DVPStatus@@_K@Z");
		__dvpBindToGLCtx = (PFNDVPBINDTOGLCTX)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpBindToGLCtx@@YA?AW4DVPStatus@@_K@Z");
		__dvpGetRequiredConstantsGLCtx = (PFNDVPGETREQUIREDCONSTANTSGLCTX)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpGetRequiredConstantsGLCtx@@YA?AW4DVPStatus@@PEAI00000@Z");
		__dvpCreateGPUTextureGL = (PFNDVPCREATEGPUTEXTUREGL)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpCreateGPUTextureGL@@YA?AW4DVPStatus@@IPEA_K@Z");
		__dvpUnbindFromGLCtx = (PFNDVPUNBINDFROMGLCTX)BLI_dynlib_find_symbol(__dvpLibrary, "?dvpUnbindFromGLCtx@@YA?AW4DVPStatus@@_K@Z");
		if (!__dvpInitGLContext ||
			!__dvpCloseGLContext ||
			!__dvpGetLibrayVersion ||
			!__dvpBegin ||
			!__dvpEnd ||
			!__dvpCreateBuffer ||
			!__dvpDestroyBuffer ||
			!__dvpFreeBuffer ||
			!__dvpMemcpyLined ||
			!__dvpMemcpy ||
			!__dvpImportSyncObject ||
			!__dvpFreeSyncObject ||
			!__dvpMapBufferEndAPI ||
			!__dvpMapBufferWaitDVP ||
			!__dvpMapBufferEndDVP ||
			!__dvpMapBufferWaitAPI ||
			!__dvpBindToGLCtx ||
			!__dvpGetRequiredConstantsGLCtx ||
			!__dvpCreateGPUTextureGL ||
			!__dvpUnbindFromGLCtx)
		{
			return DVP_STATUS_ERROR;
		}
		// check that the library version is what we want
		if ((status = __dvpGetLibrayVersion(&__dvpMajorVersion, &__dvpMinorVersion)) != DVP_STATUS_OK)
			return status;
		if (__dvpMajorVersion != DVP_MAJOR_VERSION || __dvpMinorVersion < DVP_MINOR_VERSION)
			return DVP_STATUS_ERROR;
	}
	return (!__dvpInitGLContext) ? DVP_STATUS_ERROR : __dvpInitGLContext(flags);
}

DVPStatus dvpCloseGLContext(void)
{
	return (!__dvpCloseGLContext) ? DVP_STATUS_ERROR : __dvpCloseGLContext();
}

#endif // WIN32
