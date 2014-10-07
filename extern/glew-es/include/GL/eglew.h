/*
** The OpenGL Extension Wrangler Library
** Copyright (C) 2002-2008, Milan Ikits <milan ikits[]ieee org>
** Copyright (C) 2002-2008, Marcelo E. Magallon <mmagallo[]debian org>
** Copyright (C) 2002, Lev Povalahev
** All rights reserved.
** 
** Redistribution and use in source and binary forms, with or without 
** modification, are permitted provided that the following conditions are met:
** 
** * Redistributions of source code must retain the above copyright notice, 
**   this list of conditions and the following disclaimer.
** * Redistributions in binary form must reproduce the above copyright notice, 
**   this list of conditions and the following disclaimer in the documentation 
**   and/or other materials provided with the distribution.
** * The name of the author may be used to endorse or promote products 
**   derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
** THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
** Copyright (c) 2007 The Khronos Group Inc.
** 
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
** 
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
** 
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.0 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
**
** http://oss.sgi.com/projects/FreeB
**
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
**
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2004 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
**
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
*/

/*
** Copyright (c) 2007-2009 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

/* Platform-specific types and definitions for egl.h
 * $Revision: 12306 $ on $Date: 2010-08-25 09:51:28 -0700 (Wed, 25 Aug 2010) $
 *
 * Adopters may modify khrplatform.h and this file to suit their platform.
 * You are encouraged to submit all modifications to the Khronos group so that
 * they can be included in future versions of this file.  Please submit changes
 * by sending them to the public Khronos Bugzilla (http://khronos.org/bugzilla)
 * by filing a bug against product "EGL" component "Registry".
 */

/* Copyright © 2011 Linaro Limited
 */

#ifndef __eglew_h__
#define __eglew_h__
#define __EGLEW_H__


#if defined(__egl_h_) || defined (__EGL_H_)
#error egl.h included before glew.h
#endif
#if defined(__eglext_h_) || defined (__EGLEXT_H_)
#error eglext.h included before glew.h
#endif
#if defined(__eglplatform_h_) || defined (__EGLPLATFORM_H_)
#error eglplatform.h included before glew.h
#endif
#if defined(GLES_EGLTYPES_H) || defined (gles_egltypes_h)
#error egltypes.h.h included before glew.h
#endif

#define __egl_h_
#define __EGL_H_
#define __eglext_h_
#define __EGLEXT_H_
#define GLES_EGLTYPES_H
#define gles_egltypes_h
#define __eglplatform_h_
#define __EGLPLATFORM_H_

/* TODO insert license */

/* Includes */
#include <GL/glew.h>

/* Macros used in EGL function prototype declarations.
 *
 * EGL functions should be prototyped as:
 *
 * EGLAPI return-type EGLAPIENTRY eglFunction(arguments);
 * typedef return-type (EXPAPIENTRYP PFNEGLFUNCTIONPROC) (arguments);
 *
 * KHRONOS_APICALL and KHRONOS_APIENTRY are defined in KHR/khrplatform.h
 */

#ifndef EGLAPI
#define EGLAPI KHRONOS_APICALL
#endif

#ifndef EGLAPIENTRY
#define EGLAPIENTRY  KHRONOS_APIENTRY
#endif
#define EGLAPIENTRYP EGLAPIENTRY*

/* The types NativeDisplayType, NativeWindowType, and NativePixmapType
 * are aliases of window-system-dependent types, such as X Display * or
 * Windows Device Context. They must be defined in platform-specific
 * code below. The EGL-prefixed versions of Native*Type are the same
 * types, renamed in EGL 1.3 so all types in the API start with "EGL".
 *
 * Khronos STRONGLY RECOMMENDS that you use the default definitions
 * provided below, since these changes affect both binary and source
 * portability of applications using EGL running on different EGL
 * implementations.
 */

#if defined(_WIN32) || defined(__VC32__) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__) /* Win32 and WinCE */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>

typedef HDC     EGLNativeDisplayType;
typedef HBITMAP EGLNativePixmapType;
typedef HWND    EGLNativeWindowType;

#elif defined(__WINSCW__) || defined(__SYMBIAN32__)  /* Symbian */

typedef int   EGLNativeDisplayType;
typedef void *EGLNativeWindowType;
typedef void *EGLNativePixmapType;

#elif defined(__unix__)

/* X11 (tentative)  */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef Display *EGLNativeDisplayType;
typedef Pixmap   EGLNativePixmapType;
typedef Window   EGLNativeWindowType;

#elif defined(__APPLE__)

typedef void *EGLNativeDisplayType;
typedef void *EGLNativePixmapType;
typedef void *EGLNativeWindowType;

#endif

/* EGL 1.2 types, renamed for consistency in EGL 1.3 */
typedef EGLNativeDisplayType NativeDisplayType;
typedef EGLNativePixmapType  NativePixmapType;
typedef EGLNativeWindowType  NativeWindowType;


/* Define EGLint. This must be a signed integral type large enough to contain
 * all legal attribute names and values passed into and out of EGL, whether
 * their type is boolean, bitmask, enumerant (symbolic constant), integer,
 * handle, or other.  While in general a 32-bit integer will suffice, if
 * handles are 64 bit types, then EGLint should be defined as a signed 64-bit
 * integer type.
 */
typedef khronos_int32_t EGLint;


/* Basic types */
typedef int EGLBoolean;

/* Internal types */
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;

/* Handle values */
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)-1)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_UNKNOWN   	((EGLint)-1)

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------- EGL_VERSION_1_1 ---------------------------- */

#ifndef EGL_VERSION_1_1
#define EGL_VERSION_1_0		1
#define EGL_VERSION_1_1 	1

/*
** Boolean
*/
#define EGL_FALSE		       0
#define EGL_TRUE		       1

/*
** Errors
*/
#define EGL_SUCCESS		       0x3000
#define EGL_NOT_INITIALIZED	       0x3001
#define EGL_BAD_ACCESS		       0x3002
#define EGL_BAD_ALLOC		       0x3003
#define EGL_BAD_ATTRIBUTE	       0x3004
#define EGL_BAD_CONFIG		       0x3005
#define EGL_BAD_CONTEXT		       0x3006
#define EGL_BAD_CURRENT_SURFACE        0x3007
#define EGL_BAD_DISPLAY		       0x3008
#define EGL_BAD_MATCH		       0x3009
#define EGL_BAD_NATIVE_PIXMAP	       0x300A
#define EGL_BAD_NATIVE_WINDOW	       0x300B
#define EGL_BAD_PARAMETER	       0x300C
#define EGL_BAD_SURFACE		       0x300D
#define EGL_CONTEXT_LOST	       0x300E
/* 0x300F - 0x301F reserved for additional errors. */

/*
** Config attributes
*/
#define EGL_BUFFER_SIZE		       0x3020
#define EGL_ALPHA_SIZE		       0x3021
#define EGL_BLUE_SIZE		       0x3022
#define EGL_GREEN_SIZE		       0x3023
#define EGL_RED_SIZE		       0x3024
#define EGL_DEPTH_SIZE		       0x3025
#define EGL_STENCIL_SIZE	       0x3026
#define EGL_CONFIG_CAVEAT	       0x3027
#define EGL_CONFIG_ID		       0x3028
#define EGL_LEVEL		       0x3029
#define EGL_MAX_PBUFFER_HEIGHT	       0x302A
#define EGL_MAX_PBUFFER_PIXELS	       0x302B
#define EGL_MAX_PBUFFER_WIDTH	       0x302C
#define EGL_NATIVE_RENDERABLE	       0x302D
#define EGL_NATIVE_VISUAL_ID	       0x302E
#define EGL_NATIVE_VISUAL_TYPE	       0x302F
/*#define EGL_PRESERVED_RESOURCES	 0x3030*/
#define EGL_SAMPLES		       0x3031
#define EGL_SAMPLE_BUFFERS	       0x3032
#define EGL_SURFACE_TYPE	       0x3033
#define EGL_TRANSPARENT_TYPE	       0x3034
#define EGL_TRANSPARENT_BLUE_VALUE     0x3035
#define EGL_TRANSPARENT_GREEN_VALUE    0x3036
#define EGL_TRANSPARENT_RED_VALUE      0x3037
#define EGL_NONE		       0x3038	/* Also a config value */
#define EGL_BIND_TO_TEXTURE_RGB        0x3039
#define EGL_BIND_TO_TEXTURE_RGBA       0x303A
#define EGL_MIN_SWAP_INTERVAL	       0x303B
#define EGL_MAX_SWAP_INTERVAL	       0x303C

/*
** Config values
*/
#define EGL_DONT_CARE		       ((EGLint) -1)

#define EGL_SLOW_CONFIG		       0x3050	/* EGL_CONFIG_CAVEAT value */
#define EGL_NON_CONFORMANT_CONFIG      0x3051	/* " */
#define EGL_TRANSPARENT_RGB	       0x3052	/* EGL_TRANSPARENT_TYPE value */
#define EGL_NO_TEXTURE		       0x305C	/* EGL_TEXTURE_FORMAT/TARGET value */
#define EGL_TEXTURE_RGB		       0x305D	/* EGL_TEXTURE_FORMAT value */
#define EGL_TEXTURE_RGBA	       0x305E	/* " */
#define EGL_TEXTURE_2D		       0x305F	/* EGL_TEXTURE_TARGET value */

/*
** Config attribute mask bits
*/
#define EGL_PBUFFER_BIT		       0x01	/* EGL_SURFACE_TYPE mask bit */
#define EGL_PIXMAP_BIT		       0x02	/* " */
#define EGL_WINDOW_BIT		       0x04	/* " */

/*
** String names
*/
#define EGL_VENDOR		       0x3053	/* eglQueryString target */
#define EGL_VERSION		       0x3054	/* " */
#define EGL_EXTENSIONS		       0x3055	/* " */

/*
** Surface attributes
*/
#define EGL_HEIGHT		       0x3056
#define EGL_WIDTH		       0x3057
#define EGL_LARGEST_PBUFFER	       0x3058
#define EGL_TEXTURE_FORMAT	       0x3080	/* For pbuffers bound as textures */
#define EGL_TEXTURE_TARGET	       0x3081	/* " */
#define EGL_MIPMAP_TEXTURE	       0x3082	/* " */
#define EGL_MIPMAP_LEVEL	       0x3083	/* " */

/*
** BindTexImage / ReleaseTexImage buffer target
*/
#define EGL_BACK_BUFFER		       0x3084

/*
** Current surfaces
*/
#define EGL_DRAW		       0x3059
#define EGL_READ		       0x305A

/*
** Engines
*/
#define EGL_CORE_NATIVE_ENGINE	       0x305B

/* 0x305C-0x3FFFF reserved for future use */

/*
** Functions
*/

EGLAPI EGLint EGLAPIENTRY eglGetError (void);

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay (NativeDisplayType display);
EGLAPI EGLBoolean EGLAPIENTRY eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLAPI EGLBoolean EGLAPIENTRY eglTerminate (EGLDisplay dpy);
EGLAPI const char * EGLAPIENTRY eglQueryString (EGLDisplay dpy, EGLint name);
EGLAPI void (* EGLAPIENTRY eglGetProcAddress (const char *procname))();

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs (EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);

EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface (EGLDisplay dpy, EGLConfig config, NativeWindowType window, const EGLint *attrib_list);
EGLAPI EGLSurface EGLAPIENTRY eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig config, NativePixmapType pixmap, const EGLint *attrib_list);
EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface (EGLDisplay dpy, EGLSurface surface);
EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);

/* EGL 1.1 render-to-texture APIs */
EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);

/* EGL 1.1 swap control API */
EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval);

EGLAPI EGLContext EGLAPIENTRY eglCreateContext (EGLDisplay dpy, EGLConfig config, EGLContext share_list, const EGLint *attrib_list);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext (EGLDisplay dpy, EGLContext ctx);
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext (void);
EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface (EGLint readdraw);
EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay (void);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);

EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL (void);
EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative (EGLint engine);
EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers (EGLDisplay dpy, EGLSurface draw);
EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers (EGLDisplay dpy, EGLSurface surface, NativePixmapType target);

#define EGLEW_VERSION_1_1 EGLEW_GET_VAR(__EGLEW_VERSION_1_1)

#endif /* EGL_VERSION_1_1 */

/* ---------------------------- EGL_VERSION_1_2 ---------------------------- */

#if !defined(EGL_VERSION_1_2) 
#define EGL_VERSION_1_2 1

#define EGL_OPENGL_ES_BIT 0x01
#define EGL_OPENVG_BIT 0x02
#define EGL_PRESERVED_RESOURCES 0x3030
#define EGL_LUMINANCE_SIZE 0x303D
#define EGL_ALPHA_MASK_SIZE 0x303E
#define EGL_COLOR_BUFFER_TYPE 0x303F
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_SINGLE_BUFFER 0x3085
#define EGL_RENDER_BUFFER 0x3086
#define EGL_COLORSPACE 0x3087
#define EGL_ALPHA_FORMAT 0x3088
#define EGL_COLORSPACE_sRGB 0x3089
#define EGL_COLORSPACE_LINEAR 0x308A
#define EGL_ALPHA_FORMAT_NONPRE 0x308B
#define EGL_ALPHA_FORMAT_PRE 0x308C
#define EGL_CLIENT_APIS 0x308D
#define EGL_RGB_BUFFER 0x308E
#define EGL_LUMINANCE_BUFFER 0x308F
#define EGL_HORIZONTAL_RESOLUTION 0x3090
#define EGL_VERTICAL_RESOLUTION 0x3091
#define EGL_PIXEL_ASPECT_RATIO 0x3092
#define EGL_SWAP_BEHAVIOR 0x3093
#define EGL_BUFFER_PRESERVED 0x3094
#define EGL_BUFFER_DESTROYED 0x3095
#define EGL_OPENVG_IMAGE 0x3096
#define EGL_CONTEXT_CLIENT_TYPE 0x3097
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_OPENVG_API 0x30A1
#define EGL_DISPLAY_SCALING 10000

typedef unsigned int EGLenum;
typedef void *EGLClientBuffer;

typedef EGLBoolean (EGLAPIENTRY * PFNEGLBINDAPIPROC) (EGLenum);
typedef EGLSurface (EGLAPIENTRY * PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC) (EGLDisplay, EGLenum, EGLClientBuffer, EGLConfig, const EGLint *);
typedef EGLenum (EGLAPIENTRY * PFNEGLQUERYAPIPROC) (void);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLRELEASETHREADPROC) (void);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLWAITCLIENTPROC) (void);

#define eglBindAPI EGLEW_GET_FUN(__eglewBindAPI)
#define eglCreatePbufferFromClientBuffer EGLEW_GET_FUN(__eglewCreatePbufferFromClientBuffer)
#define eglQueryAPI EGLEW_GET_FUN(__eglewQueryAPI)
#define eglReleaseThread EGLEW_GET_FUN(__eglewReleaseThread)
#define eglWaitClient EGLEW_GET_FUN(__eglewWaitClient)

#define EGLEW_VERSION_1_2 EGLEW_GET_VAR(__EGLEW_VERSION_1_2)

#endif /* !EGL_VERSION_1_2 */

/* ---------------------------- EGL_VERSION_1_3 ---------------------------- */

#if !defined(EGL_VERSION_1_3) 
#define EGL_VERSION_1_3 1

#define EGL_OPENGL_ES2_BIT 0x04
#define EGL_MATCH_NATIVE_PIXMAP 0x3041
#define EGL_CONTEXT_CLIENT_VERSION 0x3098

#define EGLEW_VERSION_1_3 EGLEW_GET_VAR(__EGLEW_VERSION_1_3)

#endif /* !EGL_VERSION_1_3 */

/* ---------------------------- EGL_VERSION_1_4 ---------------------------- */

#if !defined(EGL_VERSION_1_4) 
#define EGL_VERSION_1_4 1

#define EGL_OPENGL_BIT 0x0008
#define EGL_VG_COLORSPACE_LINEAR_BIT 0x0020
#define EGL_VG_ALPHA_FORMAT_PRE_BIT 0x0040
#define EGL_MULTISAMPLE_RESOLVE_BOX_BIT 0x0200
#define EGL_SWAP_BEHAVIOR_PRESERVED_BIT 0x0400
#define EGL_VG_COLORSPACE 0x3087
#define EGL_VG_ALPHA_FORMAT 0x3088
#define EGL_VG_COLORSPACE_sRGB 0x3089
#define EGL_VG_COLORSPACE_LINEAR 0x308A
#define EGL_VG_ALPHA_FORMAT_NONPRE 0x308B
#define EGL_VG_ALPHA_FORMAT_PRE 0x308C
#define EGL_MULTISAMPLE_RESOLVE 0x3099
#define EGL_MULTISAMPLE_RESOLVE_DEFAULT 0x309A
#define EGL_MULTISAMPLE_RESOLVE_BOX 0x309B
#define EGL_OPENGL_API 0x30A2

typedef void (*__eglMustCastToProperFunctionPointerType)(void);

#define EGLEW_VERSION_1_4 EGLEW_GET_VAR(__EGLEW_VERSION_1_4)

#endif /* !EGL_VERSION_1_4 */

/* ---------------------------- EGL_VERSION_1_5 ---------------------------- */

#if !defined(EGL_VERSION_1_5) 
#define EGL_VERSION_1_5 1

typedef void *EGLSync;
typedef intptr_t EGLAttrib;
typedef khronos_utime_nanoseconds_t EGLTime;
#define EGL_CONTEXT_MAJOR_VERSION         0x3098
#define EGL_CONTEXT_MINOR_VERSION         0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK   0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY 0x31BD
#define EGL_NO_RESET_NOTIFICATION         0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET         0x31BF
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT 0x00000002
#define EGL_CONTEXT_OPENGL_DEBUG          0x31B0
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE 0x31B1
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS  0x31B2
#define EGL_OPENGL_ES3_BIT                0x00000040
#define EGL_CL_EVENT_HANDLE               0x309C
#define EGL_SYNC_CL_EVENT                 0x30FE
#define EGL_SYNC_CL_EVENT_COMPLETE        0x30FF
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE  0x30F0
#define EGL_SYNC_TYPE                     0x30F7
#define EGL_SYNC_STATUS                   0x30F1
#define EGL_SYNC_CONDITION                0x30F8
#define EGL_SIGNALED                      0x30F2
#define EGL_UNSIGNALED                    0x30F3
#define EGL_SYNC_FLUSH_COMMANDS_BIT       0x0001
#define EGL_FOREVER                       0xFFFFFFFFFFFFFFFFull
#define EGL_TIMEOUT_EXPIRED               0x30F5
#define EGL_CONDITION_SATISFIED           0x30F6
#define EGL_NO_SYNC                       ((EGLSync)0)
#define EGL_SYNC_FENCE                    0x30F9
#define EGL_GL_COLORSPACE                 0x309D
#define EGL_GL_COLORSPACE_SRGB            0x3089
#define EGL_GL_COLORSPACE_LINEAR          0x308A
#define EGL_GL_RENDERBUFFER               0x30B9
#define EGL_GL_TEXTURE_2D                 0x30B1
#define EGL_GL_TEXTURE_LEVEL              0x30BC
#define EGL_GL_TEXTURE_3D                 0x30B2
#define EGL_GL_TEXTURE_ZOFFSET            0x30BD
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x30B3
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x30B4
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x30B5
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x30B6
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x30B7
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x30B8

typedef EGLSync    (EGLAPIENTRY * PFNCREATESYNC                 ) (EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRY * PFNDESTROYSYNC                ) (EGLDisplay dpy, EGLSync sync);
typedef EGLint     (EGLAPIENTRY * PFNCLIENTWAITSYNC             ) (EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout);
typedef EGLBoolean (EGLAPIENTRY * PFNGETSYNCATTRIB              ) (EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value);
typedef EGLDisplay (EGLAPIENTRY * PFNGETPLATFORMDISPLAY         ) (EGLenum platform, void *native_display, const EGLAttrib *attrib_list);
typedef EGLSurface (EGLAPIENTRY * PFNCREATEPLATFORMWINDOWSURFACE) (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list);
typedef EGLSurface (EGLAPIENTRY * PFNCREATEPLATFORMPIXMAPSURFACE) (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list);
typedef EGLBoolean (EGLAPIENTRY * PFNWAITSYNC                   ) (EGLDisplay dpy, EGLSync sync, EGLint flags);

#define eglCreateSync                  EGLEW_GET_FUN(__eglewCreateSync                 )
#define eglDestroySync                 EGLEW_GET_FUN(__eglewDestroySync                )
#define eglClientWaitSync              EGLEW_GET_FUN(__eglewClientWaitSync             )
#define eglGetSyncAttrib               EGLEW_GET_FUN(__eglewGetSyncAttrib              )
#define eglGetPlatformDisplay          EGLEW_GET_FUN(__eglewGetPlatformDisplay         )
#define eglCreatePlatformWindowSurface EGLEW_GET_FUN(__eglewCreatePlatformWindowSurface)
#define eglCreatePlatformPixmapSurface EGLEW_GET_FUN(__eglewCreatePlatformPixmapSurface)
#define eglWaitSync                    EGLEW_GET_FUN(__eglewWaitSync                   )

#define EGLEW_VERSION_1_5 EGLEW_GET_VAR(__EGLEW_VERSION_1_5)

#endif /* !EGL_VERSION_1_5 */

/* ------------------------- EGL_ANDROID_blob_cache ------------------------ */

#if !defined(EGL_ANDROID_blob_cache) 
#define EGL_ANDROID_blob_cache 1

typedef khronos_ssize_t EGLsizeiANDROID; // XXX jwilkins: missing typedef
typedef EGLsizeiANDROID (*EGLGetBlobFuncANDROID) (const void* key, EGLsizeiANDROID keySize, void* value, EGLsizeiANDROID valueSize); // XXX jwilkins: missing typedef
typedef void (*EGLSetBlobFuncANDROID) (const void* key, EGLsizeiANDROID keySize, const void* value, EGLsizeiANDROID valueSize); // XXX jwilkins: missing typedef

typedef void (EGLAPIENTRY * PFNEGLSETBLOBCACHEFUNCSANDROIDPROC) (EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get);

#define eglSetBlobCacheFuncsANDROID EGLEW_GET_FUN(__eglewSetBlobCacheFuncsANDROID)

#define EGLEW_ANDROID_blob_cache EGLEW_GET_VAR(__EGLEW_ANDROID_blob_cache)

#endif /* !EGL_ANDROID_blob_cache */

/* --------------------- EGL_ANDROID_framebuffer_target -------------------- */

#if !defined(EGL_ANDROID_framebuffer_target) 
#define EGL_ANDROID_framebuffer_target 1

#define EGL_FRAMEBUFFER_TARGET_ANDROID 0x3147

#define EGLEW_ANDROID_framebuffer_target EGLEW_GET_VAR(__EGLEW_ANDROID_framebuffer_target)

#endif /* !EGL_ANDROID_framebuffer_target */

/* -------------------- EGL_ANDROID_image_native_buffer -------------------- */

#if !defined(EGL_ANDROID_image_native_buffer) 
#define EGL_ANDROID_image_native_buffer 1

#define EGL_NATIVE_BUFFER_ANDROID 0x3140

#define EGLEW_ANDROID_image_native_buffer EGLEW_GET_VAR(__EGLEW_ANDROID_image_native_buffer)

#endif /* !EGL_ANDROID_image_native_buffer */

/* --------------------- EGL_ANDROID_native_fence_sync --------------------- */

#if !defined(EGL_ANDROID_native_fence_sync) 
#define EGL_ANDROID_native_fence_sync 1

typedef void* EGLSyncKHR; // XXX jwilkins: missing typedef

#define EGL_SYNC_NATIVE_FENCE_ANDROID 0x3144
#define EGL_SYNC_NATIVE_FENCE_FD_ANDROID 0x3145
#define EGL_SYNC_NATIVE_FENCE_SIGNALED_ANDROID 0x3146

typedef EGLint (EGLAPIENTRY * PFNEGLDUPNATIVEFENCEFDANDROIDPROC) (EGLDisplay dpy, EGLSyncKHR);

#define eglDupNativeFenceFDANDROID EGLEW_GET_FUN(__eglewDupNativeFenceFDANDROID)

#define EGLEW_ANDROID_native_fence_sync EGLEW_GET_VAR(__EGLEW_ANDROID_native_fence_sync)

#endif /* !EGL_ANDROID_native_fence_sync */

/* ------------------------- EGL_ANDROID_recordable ------------------------ */

#if !defined(EGL_ANDROID_recordable) 
#define EGL_ANDROID_recordable 1

#define EGL_RECORDABLE_ANDROID 0x3142

#define EGLEW_ANDROID_recordable EGLEW_GET_VAR(__EGLEW_ANDROID_recordable)

#endif /* !EGL_ANDROID_recordable */

/* ---------------- EGL_ANGLE_d3d_share_handle_client_buffer --------------- */

#if !defined(EGL_ANGLE_d3d_share_handle_client_buffer) 
#define EGL_ANGLE_d3d_share_handle_client_buffer 1

#define EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE 0x3200

#define EGLEW_ANGLE_d3d_share_handle_client_buffer EGLEW_GET_VAR(__EGLEW_ANGLE_d3d_share_handle_client_buffer)

#endif /* !EGL_ANGLE_d3d_share_handle_client_buffer */

/* -------------------- EGL_ANGLE_query_surface_pointer -------------------- */

#if !defined(EGL_ANGLE_query_surface_pointer) 
#define EGL_ANGLE_query_surface_pointer 1

typedef EGLBoolean (EGLAPIENTRY * PFNEGLQUERYSURFACEPOINTERANGLEPROC) (EGLDisplay dpy, EGLSurface surface, EGLint attribute, void ** value);

#define eglQuerySurfacePointerANGLE EGLEW_GET_FUN(__eglewQuerySurfacePointerANGLE)

#define EGLEW_ANGLE_query_surface_pointer EGLEW_GET_VAR(__EGLEW_ANGLE_query_surface_pointer)

#endif /* !EGL_ANGLE_query_surface_pointer */

/* ------------- EGL_ANGLE_surface_d3d_texture_2d_share_handle ------------- */

#if !defined(EGL_ANGLE_surface_d3d_texture_2d_share_handle) 
#define EGL_ANGLE_surface_d3d_texture_2d_share_handle 1

#define EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE 0x3200

#define EGLEW_ANGLE_surface_d3d_texture_2d_share_handle EGLEW_GET_VAR(__EGLEW_ANGLE_surface_d3d_texture_2d_share_handle)

#endif /* !EGL_ANGLE_surface_d3d_texture_2d_share_handle */

/* --------------------------- EGL_EXT_buffer_age -------------------------- */

#if !defined(EGL_EXT_buffer_age) 
#define EGL_EXT_buffer_age 1

#define EGL_BUFFER_AGE_EXT 0x313D

#define EGLEW_EXT_buffer_age EGLEW_GET_VAR(__EGLEW_EXT_buffer_age)

#endif /* !EGL_EXT_buffer_age */

/* ------------------- EGL_EXT_create_context_robustness ------------------- */

#if !defined(EGL_EXT_create_context_robustness) 
#define EGL_EXT_create_context_robustness 1

#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT 0x30BF
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT 0x3138
#define EGL_NO_RESET_NOTIFICATION_EXT 0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_EXT 0x31BF

#define EGLEW_EXT_create_context_robustness EGLEW_GET_VAR(__EGLEW_EXT_create_context_robustness)

#endif /* !EGL_EXT_create_context_robustness */

/* ------------------------ EGL_EXT_multiview_window ----------------------- */

#if !defined(EGL_EXT_multiview_window) 
#define EGL_EXT_multiview_window 1

#define EGL_MULTIVIEW_VIEW_COUNT_EXT 0x3134

#define EGLEW_EXT_multiview_window EGLEW_GET_VAR(__EGLEW_EXT_multiview_window)

#endif /* !EGL_EXT_multiview_window */

/* -------------------------- EGL_HI_clientpixmap -------------------------- */

#if !defined(EGL_HI_clientpixmap) 
#define EGL_HI_clientpixmap 1

#define EGLEW_HI_clientpixmap EGLEW_GET_VAR(__EGLEW_HI_clientpixmap)

#endif /* !EGL_HI_clientpixmap */

/* -------------------------- EGL_HI_colorformats -------------------------- */

#if !defined(EGL_HI_colorformats) 
#define EGL_HI_colorformats 1

#define EGL_COLOR_FORMAT_HI 0x8F70
#define EGL_COLOR_RGB_HI 0x8F71
#define EGL_COLOR_RGBA_HI 0x8F72
#define EGL_COLOR_ARGB_HI 0x8F73

#define EGLEW_HI_colorformats EGLEW_GET_VAR(__EGLEW_HI_colorformats)

#endif /* !EGL_HI_colorformats */

/* ------------------------ EGL_IMG_context_priority ----------------------- */

#if !defined(EGL_IMG_context_priority) 
#define EGL_IMG_context_priority 1

#define EGL_CONTEXT_PRIORITY_LEVEL_IMG 0x3100
#define EGL_CONTEXT_PRIORITY_HIGH_IMG 0x3101
#define EGL_CONTEXT_PRIORITY_MEDIUM_IMG 0x3102
#define EGL_CONTEXT_PRIORITY_LOW_IMG 0x3103

#define EGLEW_IMG_context_priority EGLEW_GET_VAR(__EGLEW_IMG_context_priority)

#endif /* !EGL_IMG_context_priority */

/* ------------------------- EGL_KHR_config_attribs ------------------------ */

#if !defined(EGL_KHR_config_attribs) 
#define EGL_KHR_config_attribs 1

#define EGL_VG_COLORSPACE_LINEAR_BIT_KHR 0x0020
#define EGL_VG_ALPHA_FORMAT_PRE_BIT_KHR 0x0040
#define EGL_CONFORMANT_KHR 0x3042

#define EGLEW_KHR_config_attribs EGLEW_GET_VAR(__EGLEW_KHR_config_attribs)

#endif /* !EGL_KHR_config_attribs */

/* ------------------------- EGL_KHR_create_context ------------------------ */

#if !defined(EGL_KHR_create_context) 
#define EGL_KHR_create_context 1

#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR 0x00000001
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR 0x00000002
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR 0x00000004
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#define EGL_CONTEXT_MAJOR_VERSION_KHR 0x3098
#define EGL_CONTEXT_MINOR_VERSION_KHR 0x30FB
#define EGL_CONTEXT_FLAGS_KHR 0x30FC
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR 0x30FD
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR 0x31BD
#define EGL_NO_RESET_NOTIFICATION_KHR 0x31BE
#define EGL_LOSE_CONTEXT_ON_RESET_KHR 0x31BF

#define EGLEW_KHR_create_context EGLEW_GET_VAR(__EGLEW_KHR_create_context)

#endif /* !EGL_KHR_create_context */

/* --------------------------- EGL_KHR_fence_sync -------------------------- */

#if !defined(EGL_KHR_fence_sync) 
#define EGL_KHR_fence_sync 1

#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR 0x30F0
#define EGL_SYNC_CONDITION_KHR 0x30F8
#define EGL_SYNC_FENCE_KHR 0x30F9

#define EGLEW_KHR_fence_sync EGLEW_GET_VAR(__EGLEW_KHR_fence_sync)

#endif /* !EGL_KHR_fence_sync */

/* --------------------- EGL_KHR_gl_renderbuffer_image --------------------- */

#if !defined(EGL_KHR_gl_renderbuffer_image) 
#define EGL_KHR_gl_renderbuffer_image 1

#define EGLEW_KHR_gl_renderbuffer_image EGLEW_GET_VAR(__EGLEW_KHR_gl_renderbuffer_image)

#endif /* !EGL_KHR_gl_renderbuffer_image */

/* ---------------------- EGL_KHR_gl_texture_2D_image ---------------------- */

#if !defined(EGL_KHR_gl_texture_2D_image) 
#define EGL_KHR_gl_texture_2D_image 1

#define EGL_GL_TEXTURE_2D_KHR 0x30B1
#define EGL_GL_TEXTURE_3D_KHR 0x30B2
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR 0x30B3
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR 0x30B4
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR 0x30B5
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR 0x30B6
#define EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR 0x30B7
#define EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR 0x30B8
#define EGL_GL_RENDERBUFFER_KHR 0x30B9
#define EGL_GL_TEXTURE_LEVEL_KHR 0x30BC
#define EGL_GL_TEXTURE_ZOFFSET_KHR 0x30BD

#define EGLEW_KHR_gl_texture_2D_image EGLEW_GET_VAR(__EGLEW_KHR_gl_texture_2D_image)

#endif /* !EGL_KHR_gl_texture_2D_image */

/* ---------------------- EGL_KHR_gl_texture_3D_image ---------------------- */

#if !defined(EGL_KHR_gl_texture_3D_image) 
#define EGL_KHR_gl_texture_3D_image 1

#define EGLEW_KHR_gl_texture_3D_image EGLEW_GET_VAR(__EGLEW_KHR_gl_texture_3D_image)

#endif /* !EGL_KHR_gl_texture_3D_image */

/* -------------------- EGL_KHR_gl_texture_cubemap_image ------------------- */

#if !defined(EGL_KHR_gl_texture_cubemap_image) 
#define EGL_KHR_gl_texture_cubemap_image 1

#define EGLEW_KHR_gl_texture_cubemap_image EGLEW_GET_VAR(__EGLEW_KHR_gl_texture_cubemap_image)

#endif /* !EGL_KHR_gl_texture_cubemap_image */

/* ----------------------------- EGL_KHR_image ----------------------------- */

#if !defined(EGL_KHR_image) 
#define EGL_KHR_image 1

#define EGLEW_KHR_image EGLEW_GET_VAR(__EGLEW_KHR_image)

#endif /* !EGL_KHR_image */

/* --------------------------- EGL_KHR_image_base -------------------------- */

#if !defined(EGL_KHR_image_base) 
#define EGL_KHR_image_base 1

#define EGL_IMAGE_PRESERVED_KHR 0x30D2

typedef void *EGLImageKHR;

typedef EGLImageKHR (EGLAPIENTRY * PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint* attrib_list);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay dpy, EGLImageKHR image);

#define eglCreateImageKHR EGLEW_GET_FUN(__eglewCreateImageKHR)
#define eglDestroyImageKHR EGLEW_GET_FUN(__eglewDestroyImageKHR)

#define EGLEW_KHR_image_base EGLEW_GET_VAR(__EGLEW_KHR_image_base)

#endif /* !EGL_KHR_image_base */

/* -------------------------- EGL_KHR_image_pixmap ------------------------- */

#if !defined(EGL_KHR_image_pixmap) 
#define EGL_KHR_image_pixmap 1

#define EGL_NATIVE_PIXMAP_KHR 0x30B0

#define EGLEW_KHR_image_pixmap EGLEW_GET_VAR(__EGLEW_KHR_image_pixmap)

#endif /* !EGL_KHR_image_pixmap */

/* -------------------------- EGL_KHR_lock_surface ------------------------- */

#if !defined(EGL_KHR_lock_surface) 
#define EGL_KHR_lock_surface 1

#define EGL_READ_SURFACE_BIT_KHR 0x0001
#define EGL_WRITE_SURFACE_BIT_KHR 0x0002
#define EGL_LOCK_SURFACE_BIT_KHR 0x0080
#define EGL_OPTIMAL_FORMAT_BIT_KHR 0x0100
#define EGL_MATCH_FORMAT_KHR 0x3043
#define EGL_FORMAT_RGB_565_EXACT_KHR 0x30C0
#define EGL_FORMAT_RGB_565_KHR 0x30C1
#define EGL_FORMAT_RGBA_8888_EXACT_KHR 0x30C2
#define EGL_FORMAT_RGBA_8888_KHR 0x30C3
#define EGL_MAP_PRESERVE_PIXELS_KHR 0x30C4
#define EGL_LOCK_USAGE_HINT_KHR 0x30C5
#define EGL_BITMAP_POINTER_KHR 0x30C6
#define EGL_BITMAP_PITCH_KHR 0x30C7
#define EGL_BITMAP_ORIGIN_KHR 0x30C8
#define EGL_BITMAP_PIXEL_RED_OFFSET_KHR 0x30C9
#define EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR 0x30CA
#define EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR 0x30CB
#define EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR 0x30CC
#define EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR 0x30CD
#define EGL_LOWER_LEFT_KHR 0x30CE
#define EGL_UPPER_LEFT_KHR 0x30CF

typedef EGLBoolean (EGLAPIENTRY * PFNEGLLOCKSURFACEKHRPROC) (EGLDisplay display, EGLSurface surface, const EGLint* attrib_list);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLUNLOCKSURFACEKHRPROC) (EGLDisplay display, EGLSurface surface);

#define eglLockSurfaceKHR EGLEW_GET_FUN(__eglewLockSurfaceKHR)
#define eglUnlockSurfaceKHR EGLEW_GET_FUN(__eglewUnlockSurfaceKHR)

#define EGLEW_KHR_lock_surface EGLEW_GET_VAR(__EGLEW_KHR_lock_surface)

#endif /* !EGL_KHR_lock_surface */

/* ------------------------- EGL_KHR_lock_surface2 ------------------------- */

#if !defined(EGL_KHR_lock_surface2) 
#define EGL_KHR_lock_surface2 1

#define EGL_BITMAP_PIXEL_SIZE_KHR 0x3110

#define EGLEW_KHR_lock_surface2 EGLEW_GET_VAR(__EGLEW_KHR_lock_surface2)

#endif /* !EGL_KHR_lock_surface2 */

/* ------------------------- EGL_KHR_reusable_sync ------------------------- */

#if !defined(EGL_KHR_reusable_sync) 
#define EGL_KHR_reusable_sync 1

#define EGL_SYNC_FLUSH_COMMANDS_BIT_KHR 0x0001
#define EGL_SYNC_STATUS_KHR 0x30F1
#define EGL_SIGNALED_KHR 0x30F2
#define EGL_UNSIGNALED_KHR 0x30F3
#define EGL_TIMEOUT_EXPIRED_KHR 0x30F5
#define EGL_CONDITION_SATISFIED_KHR 0x30F6
#define EGL_SYNC_TYPE_KHR 0x30F7
#define EGL_SYNC_REUSABLE_KHR 0x30FA
#define EGL_FOREVER_KHR 0xFFFFFFFFFFFFFFFF

typedef khronos_utime_nanoseconds_t EGLTimeKHR;
typedef void* EGLSyncKHR;

typedef EGLint (EGLAPIENTRY * PFNEGLCLIENTWAITSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout);
typedef EGLSyncKHR (EGLAPIENTRY * PFNEGLCREATESYNCKHRPROC) (EGLDisplay dpy, EGLenum type, const EGLint* attrib_list);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLDESTROYSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLGETSYNCATTRIBKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint* value);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLSIGNALSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode);

#define eglClientWaitSyncKHR EGLEW_GET_FUN(__eglewClientWaitSyncKHR)
#define eglCreateSyncKHR EGLEW_GET_FUN(__eglewCreateSyncKHR)
#define eglDestroySyncKHR EGLEW_GET_FUN(__eglewDestroySyncKHR)
#define eglGetSyncAttribKHR EGLEW_GET_FUN(__eglewGetSyncAttribKHR)
#define eglSignalSyncKHR EGLEW_GET_FUN(__eglewSignalSyncKHR)

#define EGLEW_KHR_reusable_sync EGLEW_GET_VAR(__EGLEW_KHR_reusable_sync)

#endif /* !EGL_KHR_reusable_sync */

/* ----------------------------- EGL_KHR_stream ---------------------------- */

#if !defined(EGL_KHR_stream) 
#define EGL_KHR_stream 1

#define EGL_CONSUMER_LATENCY_USEC_KHR 0x3210
#define EGL_PRODUCER_FRAME_KHR 0x3212
#define EGL_CONSUMER_FRAME_KHR 0x3213
#define EGL_STREAM_STATE_KHR 0x3214
#define EGL_STREAM_STATE_CREATED_KHR 0x3215
#define EGL_STREAM_STATE_CONNECTING_KHR 0x3216
#define EGL_STREAM_STATE_EMPTY_KHR 0x3217
#define EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR 0x3218
#define EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR 0x3219
#define EGL_STREAM_STATE_DISCONNECTED_KHR 0x321A
#define EGL_BAD_STREAM_KHR 0x321B
#define EGL_BAD_STATE_KHR 0x321C

#define EGLEW_KHR_stream EGLEW_GET_VAR(__EGLEW_KHR_stream)

#endif /* !EGL_KHR_stream */

/* ------------------- EGL_KHR_stream_consumer_gltexture ------------------- */

#if !defined(EGL_KHR_stream_consumer_gltexture) 
#define EGL_KHR_stream_consumer_gltexture 1

#define EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR 0x321E

typedef void* EGLStreamKHR; // XXX jwilkins: missing typedef

typedef EGLBoolean (EGLAPIENTRY * PFNEGLSTREAMCONSUMERACQUIREKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLSTREAMCONSUMERRELEASEKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);

#define eglStreamConsumerAcquireKHR EGLEW_GET_FUN(__eglewStreamConsumerAcquireKHR)
#define eglStreamConsumerGLTextureExternalKHR EGLEW_GET_FUN(__eglewStreamConsumerGLTextureExternalKHR)
#define eglStreamConsumerReleaseKHR EGLEW_GET_FUN(__eglewStreamConsumerReleaseKHR)

#define EGLEW_KHR_stream_consumer_gltexture EGLEW_GET_VAR(__EGLEW_KHR_stream_consumer_gltexture)

#endif /* !EGL_KHR_stream_consumer_gltexture */

/* -------------------- EGL_KHR_stream_cross_process_fd -------------------- */

#if !defined(EGL_KHR_stream_cross_process_fd) 
#define EGL_KHR_stream_cross_process_fd 1

typedef int EGLNativeFileDescriptorKHR; // XXX jwilkins: missing typedef

typedef EGLStreamKHR (EGLAPIENTRY * PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC) (EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor);
typedef EGLNativeFileDescriptorKHR (EGLAPIENTRY * PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC) (EGLDisplay dpy, EGLStreamKHR stream);

#define eglCreateStreamFromFileDescriptorKHR EGLEW_GET_FUN(__eglewCreateStreamFromFileDescriptorKHR)
#define eglGetStreamFileDescriptorKHR EGLEW_GET_FUN(__eglewGetStreamFileDescriptorKHR)

#define EGLEW_KHR_stream_cross_process_fd EGLEW_GET_VAR(__EGLEW_KHR_stream_cross_process_fd)

#endif /* !EGL_KHR_stream_cross_process_fd */

/* -------------------------- EGL_KHR_stream_fifo -------------------------- */

#if !defined(EGL_KHR_stream_fifo) 
#define EGL_KHR_stream_fifo 1

#define EGL_STREAM_FIFO_LENGTH_KHR 0x31FC
#define EGL_STREAM_TIME_NOW_KHR 0x31FD
#define EGL_STREAM_TIME_CONSUMER_KHR 0x31FE
#define EGL_STREAM_TIME_PRODUCER_KHR 0x31FF

#define EGLEW_KHR_stream_fifo EGLEW_GET_VAR(__EGLEW_KHR_stream_fifo)

#endif /* !EGL_KHR_stream_fifo */

/* ----------------- EGL_KHR_stream_producer_aldatalocator ----------------- */

#if !defined(EGL_KHR_stream_producer_aldatalocator) 
#define EGL_KHR_stream_producer_aldatalocator 1

#define EGLEW_KHR_stream_producer_aldatalocator EGLEW_GET_VAR(__EGLEW_KHR_stream_producer_aldatalocator)

#endif /* !EGL_KHR_stream_producer_aldatalocator */

/* ------------------- EGL_KHR_stream_producer_eglsurface ------------------ */

#if !defined(EGL_KHR_stream_producer_eglsurface) 
#define EGL_KHR_stream_producer_eglsurface 1

#define EGL_STREAM_BIT_KHR 0x0800

typedef EGLSurface (EGLAPIENTRY * PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC) (EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint* attrib_list);

#define eglCreateStreamProducerSurfaceKHR EGLEW_GET_FUN(__eglewCreateStreamProducerSurfaceKHR)

#define EGLEW_KHR_stream_producer_eglsurface EGLEW_GET_VAR(__EGLEW_KHR_stream_producer_eglsurface)

#endif /* !EGL_KHR_stream_producer_eglsurface */

/* ---------------------- EGL_KHR_surfaceless_context ---------------------- */

#if !defined(EGL_KHR_surfaceless_context) 
#define EGL_KHR_surfaceless_context 1

#define EGLEW_KHR_surfaceless_context EGLEW_GET_VAR(__EGLEW_KHR_surfaceless_context)

#endif /* !EGL_KHR_surfaceless_context */

/* ------------------------ EGL_KHR_vg_parent_image ------------------------ */

#if !defined(EGL_KHR_vg_parent_image) 
#define EGL_KHR_vg_parent_image 1

#define EGL_VG_PARENT_IMAGE_KHR 0x30BA

#define EGLEW_KHR_vg_parent_image EGLEW_GET_VAR(__EGLEW_KHR_vg_parent_image)

#endif /* !EGL_KHR_vg_parent_image */

/* --------------------------- EGL_KHR_wait_sync --------------------------- */

#if !defined(EGL_KHR_wait_sync) 
#define EGL_KHR_wait_sync 1

typedef void* EGLSyncKHR; // XXX jwilkins: missing typedef

typedef EGLint (EGLAPIENTRY * PFNEGLWAITSYNCKHRPROC) (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);

#define eglWaitSyncKHR EGLEW_GET_FUN(__eglewWaitSyncKHR)

#define EGLEW_KHR_wait_sync EGLEW_GET_VAR(__EGLEW_KHR_wait_sync)

#endif /* !EGL_KHR_wait_sync */

/* --------------------------- EGL_MESA_drm_image -------------------------- */

#if !defined(EGL_MESA_drm_image) 
#define EGL_MESA_drm_image 1

#define EGL_DRM_BUFFER_USE_SCANOUT_MESA 0x0001
#define EGL_DRM_BUFFER_USE_SHARE_MESA 0x0002
#define EGL_DRM_BUFFER_FORMAT_MESA 0x31D0
#define EGL_DRM_BUFFER_USE_MESA 0x31D1
#define EGL_DRM_BUFFER_FORMAT_ARGB32_MESA 0x31D2
#define EGL_DRM_BUFFER_MESA 0x31D3
#define EGL_DRM_BUFFER_STRIDE_MESA 0x31D4

typedef EGLImageKHR (EGLAPIENTRY * PFNEGLCREATEDRMIMAGEMESAPROC) (EGLDisplay dpy, const EGLint* attrib_list);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLEXPORTDRMIMAGEMESAPROC) (EGLDisplay dpy, EGLImageKHR image, EGLint* name, EGLint *handle, EGLint *stride);

#define eglCreateDRMImageMESA EGLEW_GET_FUN(__eglewCreateDRMImageMESA)
#define eglExportDRMImageMESA EGLEW_GET_FUN(__eglewExportDRMImageMESA)

#define EGLEW_MESA_drm_image EGLEW_GET_VAR(__EGLEW_MESA_drm_image)

#endif /* !EGL_MESA_drm_image */

/* ------------------------ EGL_NV_3dvision_surface ------------------------ */

#if !defined(EGL_NV_3dvision_surface) 
#define EGL_NV_3dvision_surface 1

#define EGL_AUTO_STEREO_NV 0x3136

#define EGLEW_NV_3dvision_surface EGLEW_GET_VAR(__EGLEW_NV_3dvision_surface)

#endif /* !EGL_NV_3dvision_surface */

/* ------------------------- EGL_NV_coverage_sample ------------------------ */

#if !defined(EGL_NV_coverage_sample) 
#define EGL_NV_coverage_sample 1

#define EGL_COVERAGE_BUFFERS_NV 0x30E0
#define EGL_COVERAGE_SAMPLES_NV 0x30E1

#define EGLEW_NV_coverage_sample EGLEW_GET_VAR(__EGLEW_NV_coverage_sample)

#endif /* !EGL_NV_coverage_sample */

/* --------------------- EGL_NV_coverage_sample_resolve -------------------- */

#if !defined(EGL_NV_coverage_sample_resolve) 
#define EGL_NV_coverage_sample_resolve 1

#define EGL_COVERAGE_SAMPLE_RESOLVE_NV 0x3131
#define EGL_COVERAGE_SAMPLE_RESOLVE_DEFAULT_NV 0x3132
#define EGL_COVERAGE_SAMPLE_RESOLVE_NONE_NV 0x3133

#define EGLEW_NV_coverage_sample_resolve EGLEW_GET_VAR(__EGLEW_NV_coverage_sample_resolve)

#endif /* !EGL_NV_coverage_sample_resolve */

/* ------------------------- EGL_NV_depth_nonlinear ------------------------ */

#if !defined(EGL_NV_depth_nonlinear) 
#define EGL_NV_depth_nonlinear 1

#define EGL_DEPTH_ENCODING_NONE_NV 0
#define EGL_DEPTH_ENCODING_NV 0x30E2
#define EGL_DEPTH_ENCODING_NONLINEAR_NV 0x30E3

#define EGLEW_NV_depth_nonlinear EGLEW_GET_VAR(__EGLEW_NV_depth_nonlinear)

#endif /* !EGL_NV_depth_nonlinear */

/* -------------------------- EGL_NV_native_query -------------------------- */

#if !defined(EGL_NV_native_query) 
#define EGL_NV_native_query 1

typedef EGLBoolean (EGLAPIENTRY * PFNEGLQUERYNATIVEDISPLAYNVPROC) (EGLDisplay dpy, EGLNativeDisplayType* display_id);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLQUERYNATIVEPIXMAPNVPROC) (EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType* pixmap);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLQUERYNATIVEWINDOWNVPROC) (EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType* window);

#define eglQueryNativeDisplayNV EGLEW_GET_FUN(__eglewQueryNativeDisplayNV)
#define eglQueryNativePixmapNV EGLEW_GET_FUN(__eglewQueryNativePixmapNV)
#define eglQueryNativeWindowNV EGLEW_GET_FUN(__eglewQueryNativeWindowNV)

#define EGLEW_NV_native_query EGLEW_GET_VAR(__EGLEW_NV_native_query)

#endif /* !EGL_NV_native_query */

/* ---------------------- EGL_NV_post_convert_rounding --------------------- */

#if !defined(EGL_NV_post_convert_rounding) 
#define EGL_NV_post_convert_rounding 1

#define EGLEW_NV_post_convert_rounding EGLEW_GET_VAR(__EGLEW_NV_post_convert_rounding)

#endif /* !EGL_NV_post_convert_rounding */

/* ------------------------- EGL_NV_post_sub_buffer ------------------------ */

#if !defined(EGL_NV_post_sub_buffer) 
#define EGL_NV_post_sub_buffer 1

#define EGL_POST_SUB_BUFFER_SUPPORTED_NV 0x30BE

typedef EGLBoolean (EGLAPIENTRY * PFNEGLPOSTSUBBUFFERNVPROC) (EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height);

#define eglPostSubBufferNV EGLEW_GET_FUN(__eglewPostSubBufferNV)

#define EGLEW_NV_post_sub_buffer EGLEW_GET_VAR(__EGLEW_NV_post_sub_buffer)

#endif /* !EGL_NV_post_sub_buffer */

/* ------------------------------ EGL_NV_sync ------------------------------ */

#if !defined(EGL_NV_sync) 
#define EGL_NV_sync 1

#define EGL_SYNC_FLUSH_COMMANDS_BIT_NV 0x0001
#define EGL_SYNC_PRIOR_COMMANDS_COMPLETE_NV 0x30E6
#define EGL_SYNC_STATUS_NV 0x30E7
#define EGL_SIGNALED_NV 0x30E8
#define EGL_UNSIGNALED_NV 0x30E9
#define EGL_ALREADY_SIGNALED_NV 0x30EA
#define EGL_TIMEOUT_EXPIRED_NV 0x30EB
#define EGL_CONDITION_SATISFIED_NV 0x30EC
#define EGL_SYNC_TYPE_NV 0x30ED
#define EGL_SYNC_CONDITION_NV 0x30EE
#define EGL_SYNC_FENCE_NV 0x30EF
#define EGL_FOREVER_NV 0xFFFFFFFFFFFFFFFF

typedef khronos_utime_nanoseconds_t EGLTimeNV;
typedef void* EGLSyncNV;

typedef EGLint (EGLAPIENTRY * PFNEGLCLIENTWAITSYNCNVPROC) (EGLSyncNV sync, EGLint flags, EGLTimeNV timeout);
typedef EGLSyncNV (EGLAPIENTRY * PFNEGLCREATEFENCESYNCNVPROC) (EGLDisplay dpy, EGLenum condition, const EGLint* attrib_list);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLDESTROYSYNCNVPROC) (EGLSyncNV sync);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLFENCENVPROC) (EGLSyncNV sync);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLGETSYNCATTRIBNVPROC) (EGLSyncNV sync, EGLint attribute, EGLint* value);
typedef EGLBoolean (EGLAPIENTRY * PFNEGLSIGNALSYNCNVPROC) (EGLSyncNV sync, EGLenum mode);

#define eglClientWaitSyncNV EGLEW_GET_FUN(__eglewClientWaitSyncNV)
#define eglCreateFenceSyncNV EGLEW_GET_FUN(__eglewCreateFenceSyncNV)
#define eglDestroySyncNV EGLEW_GET_FUN(__eglewDestroySyncNV)
#define eglFenceNV EGLEW_GET_FUN(__eglewFenceNV)
#define eglGetSyncAttribNV EGLEW_GET_FUN(__eglewGetSyncAttribNV)
#define eglSignalSyncNV EGLEW_GET_FUN(__eglewSignalSyncNV)

#define EGLEW_NV_sync EGLEW_GET_VAR(__EGLEW_NV_sync)

#endif /* !EGL_NV_sync */

/* --------------------------- EGL_NV_system_time -------------------------- */

#if !defined(EGL_NV_system_time) 
#define EGL_NV_system_time 1

typedef khronos_utime_nanoseconds_t EGLuint64NV;

typedef EGLuint64NV (EGLAPIENTRY * PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC) (void);
typedef EGLuint64NV (EGLAPIENTRY * PFNEGLGETSYSTEMTIMENVPROC) (void);

#define eglGetSystemTimeFrequencyNV EGLEW_GET_FUN(__eglewGetSystemTimeFrequencyNV)
#define eglGetSystemTimeNV EGLEW_GET_FUN(__eglewGetSystemTimeNV)

#define EGLEW_NV_system_time EGLEW_GET_VAR(__EGLEW_NV_system_time)

#endif /* !EGL_NV_system_time */

/* ------------------------------------------------------------------------- */

#if defined(GLEW_MX) && defined(_WIN32)
 #define EGLEW_FUN_EXPORT 
 #else 
 #define EGLEW_FUN_EXPORT GLEWAPI 
 #endif /* GLEW_MX */

#if defined(GLEW_MX)
 #define EGLEW_VAR_EXPORT
#else
#define EGLEW_VAR_EXPORT GLEWAPI 
 #endif /* GLEW_MX */

#if defined (GLEW_MX) && defined(_WIN32)
struct EGLEWContextStruct
{
#endif /* GLEW_MX */

EGLEW_FUN_EXPORT PFNCREATESYNC                  __eglewCreateSync;
EGLEW_FUN_EXPORT PFNDESTROYSYNC                 __eglewDestroySync;
EGLEW_FUN_EXPORT PFNCLIENTWAITSYNC              __eglewClientWaitSync;
EGLEW_FUN_EXPORT PFNGETSYNCATTRIB               __eglewGetSyncAttrib;
EGLEW_FUN_EXPORT PFNGETPLATFORMDISPLAY          __eglewGetPlatformDisplay;
EGLEW_FUN_EXPORT PFNCREATEPLATFORMWINDOWSURFACE __eglewCreatePlatformWindowSurface;
EGLEW_FUN_EXPORT PFNCREATEPLATFORMPIXMAPSURFACE __eglewCreatePlatformPixmapSurface;
EGLEW_FUN_EXPORT PFNWAITSYNC                    __eglewWaitSync;

EGLEW_FUN_EXPORT PFNEGLBINDAPIPROC __eglewBindAPI;
EGLEW_FUN_EXPORT PFNEGLCREATEPBUFFERFROMCLIENTBUFFERPROC __eglewCreatePbufferFromClientBuffer;
EGLEW_FUN_EXPORT PFNEGLQUERYAPIPROC __eglewQueryAPI;
EGLEW_FUN_EXPORT PFNEGLRELEASETHREADPROC __eglewReleaseThread;
EGLEW_FUN_EXPORT PFNEGLWAITCLIENTPROC __eglewWaitClient;

EGLEW_FUN_EXPORT PFNEGLSETBLOBCACHEFUNCSANDROIDPROC __eglewSetBlobCacheFuncsANDROID;

EGLEW_FUN_EXPORT PFNEGLDUPNATIVEFENCEFDANDROIDPROC __eglewDupNativeFenceFDANDROID;

EGLEW_FUN_EXPORT PFNEGLQUERYSURFACEPOINTERANGLEPROC __eglewQuerySurfacePointerANGLE;

EGLEW_FUN_EXPORT PFNEGLCREATEIMAGEKHRPROC __eglewCreateImageKHR;
EGLEW_FUN_EXPORT PFNEGLDESTROYIMAGEKHRPROC __eglewDestroyImageKHR;

EGLEW_FUN_EXPORT PFNEGLLOCKSURFACEKHRPROC __eglewLockSurfaceKHR;
EGLEW_FUN_EXPORT PFNEGLUNLOCKSURFACEKHRPROC __eglewUnlockSurfaceKHR;

EGLEW_FUN_EXPORT PFNEGLCLIENTWAITSYNCKHRPROC __eglewClientWaitSyncKHR;
EGLEW_FUN_EXPORT PFNEGLCREATESYNCKHRPROC __eglewCreateSyncKHR;
EGLEW_FUN_EXPORT PFNEGLDESTROYSYNCKHRPROC __eglewDestroySyncKHR;
EGLEW_FUN_EXPORT PFNEGLGETSYNCATTRIBKHRPROC __eglewGetSyncAttribKHR;
EGLEW_FUN_EXPORT PFNEGLSIGNALSYNCKHRPROC __eglewSignalSyncKHR;

EGLEW_FUN_EXPORT PFNEGLSTREAMCONSUMERACQUIREKHRPROC __eglewStreamConsumerAcquireKHR;
EGLEW_FUN_EXPORT PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC __eglewStreamConsumerGLTextureExternalKHR;
EGLEW_FUN_EXPORT PFNEGLSTREAMCONSUMERRELEASEKHRPROC __eglewStreamConsumerReleaseKHR;

EGLEW_FUN_EXPORT PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC __eglewCreateStreamFromFileDescriptorKHR;
EGLEW_FUN_EXPORT PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC __eglewGetStreamFileDescriptorKHR;

EGLEW_FUN_EXPORT PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC __eglewCreateStreamProducerSurfaceKHR;

EGLEW_FUN_EXPORT PFNEGLWAITSYNCKHRPROC __eglewWaitSyncKHR;

EGLEW_FUN_EXPORT PFNEGLCREATEDRMIMAGEMESAPROC __eglewCreateDRMImageMESA;
EGLEW_FUN_EXPORT PFNEGLEXPORTDRMIMAGEMESAPROC __eglewExportDRMImageMESA;

EGLEW_FUN_EXPORT PFNEGLQUERYNATIVEDISPLAYNVPROC __eglewQueryNativeDisplayNV;
EGLEW_FUN_EXPORT PFNEGLQUERYNATIVEPIXMAPNVPROC __eglewQueryNativePixmapNV;
EGLEW_FUN_EXPORT PFNEGLQUERYNATIVEWINDOWNVPROC __eglewQueryNativeWindowNV;

EGLEW_FUN_EXPORT PFNEGLPOSTSUBBUFFERNVPROC __eglewPostSubBufferNV;

EGLEW_FUN_EXPORT PFNEGLCLIENTWAITSYNCNVPROC __eglewClientWaitSyncNV;
EGLEW_FUN_EXPORT PFNEGLCREATEFENCESYNCNVPROC __eglewCreateFenceSyncNV;
EGLEW_FUN_EXPORT PFNEGLDESTROYSYNCNVPROC __eglewDestroySyncNV;
EGLEW_FUN_EXPORT PFNEGLFENCENVPROC __eglewFenceNV;
EGLEW_FUN_EXPORT PFNEGLGETSYNCATTRIBNVPROC __eglewGetSyncAttribNV;
EGLEW_FUN_EXPORT PFNEGLSIGNALSYNCNVPROC __eglewSignalSyncNV;

EGLEW_FUN_EXPORT PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC __eglewGetSystemTimeFrequencyNV;
EGLEW_FUN_EXPORT PFNEGLGETSYSTEMTIMENVPROC __eglewGetSystemTimeNV;

#if defined(GLEW_MX) && !defined(_WIN32)
struct EGLEWContextStruct
{
#endif /* GLEW_MX */

EGLEW_VAR_EXPORT GLboolean __EGLEW_VERSION_1_1;
EGLEW_VAR_EXPORT GLboolean __EGLEW_VERSION_1_2;
EGLEW_VAR_EXPORT GLboolean __EGLEW_VERSION_1_3;
EGLEW_VAR_EXPORT GLboolean __EGLEW_VERSION_1_4;
EGLEW_VAR_EXPORT GLboolean __EGLEW_VERSION_1_5;
EGLEW_VAR_EXPORT GLboolean __EGLEW_ANDROID_blob_cache;
EGLEW_VAR_EXPORT GLboolean __EGLEW_ANDROID_framebuffer_target;
EGLEW_VAR_EXPORT GLboolean __EGLEW_ANDROID_image_native_buffer;
EGLEW_VAR_EXPORT GLboolean __EGLEW_ANDROID_native_fence_sync;
EGLEW_VAR_EXPORT GLboolean __EGLEW_ANDROID_recordable;
EGLEW_VAR_EXPORT GLboolean __EGLEW_ANGLE_d3d_share_handle_client_buffer;
EGLEW_VAR_EXPORT GLboolean __EGLEW_ANGLE_query_surface_pointer;
EGLEW_VAR_EXPORT GLboolean __EGLEW_ANGLE_surface_d3d_texture_2d_share_handle;
EGLEW_VAR_EXPORT GLboolean __EGLEW_EXT_buffer_age;
EGLEW_VAR_EXPORT GLboolean __EGLEW_EXT_create_context_robustness;
EGLEW_VAR_EXPORT GLboolean __EGLEW_EXT_multiview_window;
EGLEW_VAR_EXPORT GLboolean __EGLEW_HI_clientpixmap;
EGLEW_VAR_EXPORT GLboolean __EGLEW_HI_colorformats;
EGLEW_VAR_EXPORT GLboolean __EGLEW_IMG_context_priority;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_config_attribs;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_create_context;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_fence_sync;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_gl_renderbuffer_image;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_gl_texture_2D_image;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_gl_texture_3D_image;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_gl_texture_cubemap_image;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_image;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_image_base;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_image_pixmap;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_lock_surface;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_lock_surface2;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_reusable_sync;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_stream;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_stream_consumer_gltexture;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_stream_cross_process_fd;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_stream_fifo;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_stream_producer_aldatalocator;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_stream_producer_eglsurface;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_surfaceless_context;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_vg_parent_image;
EGLEW_VAR_EXPORT GLboolean __EGLEW_KHR_wait_sync;
EGLEW_VAR_EXPORT GLboolean __EGLEW_MESA_drm_image;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_3dvision_surface;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_coverage_sample;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_coverage_sample_resolve;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_depth_nonlinear;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_native_query;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_post_convert_rounding;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_post_sub_buffer;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_sync;
EGLEW_VAR_EXPORT GLboolean __EGLEW_NV_system_time;

#ifdef GLEW_MX
}; /* EGLEWContextStruct */
#endif /* GLEW_MX */

/* ------------------------------------------------------------------------ */

#ifdef GLEW_MX

typedef struct EGLEWContextStruct EGLEWContext;
extern GLenum eglewContextInit (EGLDisplay display, EGLEWContext* ctx);
extern GLboolean eglewContextIsSupported (const EGLEWContext* ctx, const char* name);

#define eglewInit(display) eglewContextInit(display, eglewGetContext())
#define eglewIsSupported(x) eglewContextIsSupported(eglewGetContext(), x)

#define EGLEW_GET_VAR(x) (*(const GLboolean*)&(eglewGetContext()->x))
#ifdef _WIN32
#  define EGLEW_GET_FUN(x) eglewGetContext()->x
#else
#  define EGLEW_GET_FUN(x) x
#endif

#else /* GLEW_MX */

#define EGLEW_GET_VAR(x) (*(const GLboolean*)&x)
#define EGLEW_GET_FUN(x) x

extern GLenum eglewContextInit(EGLDisplay display); // XXX jwilkins: context handling not really written yet?
#define eglewInit eglewContextInit // XXX jwilkins:  context handling not really written yet?

extern GLboolean eglewIsSupported (const char* name);

#endif /* GLEW_MX */

extern GLboolean eglewGetExtension (const char* name);

#ifdef __cplusplus
}
#endif

#undef GLEWAPI

#endif /* __eglew_h__ */
