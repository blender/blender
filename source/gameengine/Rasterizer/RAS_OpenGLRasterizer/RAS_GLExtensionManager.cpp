/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
/*
  The extension manager's job is to link at runtime OpenGL extension 
  functions.

  Since the various platform have different methods of finding a fn
  pointer, this file attempts to encapsulate all that, so it gets a
  little messy.  Hopefully we can 
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#  include <windows.h>

#  include <GL/gl.h>

#elif defined(__APPLE__)
#  include <Carbon/Carbon.h>

#  include <OpenGL/gl.h>

#else /* UNIX */
#  include <GL/gl.h>
#  include <GL/glx.h>

#  include <dlfcn.h>
#endif

#include <vector>
#include <iostream>
#include <algorithm>

#include "STR_String.h"

#include "RAS_GLExtensionManager.h"

/* -----------------------------------------------------------------------------

   Platform specific functions section.
   
   Required Functions:
   static void bglInitEntryPoints (void)                -- Loads the GL library
   static void bglDeallocEntryPoints (void)             -- Frees the GL library
   static void *bglGetProcAddress(const GLubyte* entry) -- Finds the address of
       the GL function entry

*/
#ifdef __APPLE__
/* http://developer.apple.com/qa/qa2001/qa1188.html */
CFBundleRef gBundleRefOpenGL = NULL;

// -------------------------

static OSStatus bglInitEntryPoints (void)
{
    OSStatus err = noErr;
    const Str255 frameworkName = "\pOpenGL.framework";
    FSRefParam fileRefParam;
    FSRef fileRef;
    CFURLRef bundleURLOpenGL;

    memset(&fileRefParam, 0, sizeof(fileRefParam));
    memset(&fileRef, 0, sizeof(fileRef));

    fileRefParam.ioNamePtr  = frameworkName;
    fileRefParam.newRef = &fileRef;

    // Frameworks directory/folder
    err = FindFolder (kSystemDomain, kFrameworksFolderType, false,
                      &fileRefParam.ioVRefNum, &fileRefParam.ioDirID);
    if (noErr != err) {
        DebugStr ("\pCould not find frameworks folder");
        return err;
    }
    err = PBMakeFSRefSync (&fileRefParam); // make FSRef for folder
    if (noErr != err) {
        DebugStr ("\pCould make FSref to frameworks folder");
        return err;
    }
    // create URL to folder
    bundleURLOpenGL = CFURLCreateFromFSRef (kCFAllocatorDefault,
                                            &fileRef);
    if (!bundleURLOpenGL) {
        DebugStr ("\pCould create OpenGL Framework bundle URL");
        return paramErr;
    }
    // create ref to GL's bundle
    gBundleRefOpenGL = CFBundleCreate (kCFAllocatorDefault,
                                       bundleURLOpenGL);
    if (!gBundleRefOpenGL) {
        DebugStr ("\pCould not create OpenGL Framework bundle");
        return paramErr;
    }
    CFRelease (bundleURLOpenGL); // release created bundle
    // if the code was successfully loaded, look for our function.
    if (!CFBundleLoadExecutable (gBundleRefOpenGL)) {
        DebugStr ("\pCould not load MachO executable");
        return paramErr;
    }
    return err;
}

// -------------------------

static void bglDeallocEntryPoints (void)
{
    if (gBundleRefOpenGL != NULL) {
        // unload the bundle's code.
        CFBundleUnloadExecutable (gBundleRefOpenGL);
        CFRelease (gBundleRefOpenGL);
        gBundleRefOpenGL = NULL;
    }
}

// -------------------------

static void * bglGetProcAddress (const GLubyte * pszProc)
{
    if (!gBundleRefOpenGL)
    	return NULL;

    return CFBundleGetFunctionPointerForName (gBundleRefOpenGL,
                CFStringCreateWithCStringNoCopy (NULL,
                     (const char *) pszProc, CFStringGetSystemEncoding (), NULL));
}
#elif defined(GLX_ARB_get_proc_address)
/* Not all glx.h define PFNGLXGETPROCADDRESSARBPROC !
   We define our own if needed.                         */
#ifdef HAVE_PFNGLXGETPROCADDRESSARBPROC
#define PFNBGLXGETPROCADDRESSARBPROC PFNGLXGETPROCADDRESSARBPROC
#else
typedef void (*(*PFNBGLXGETPROCADDRESSARBPROC)(const GLubyte *procname))();
#endif

void *_getProcAddress(const GLubyte *procName) { return NULL; }
PFNBGLXGETPROCADDRESSARBPROC bglGetProcAddress;

static void bglInitEntryPoints (void)
{
	Display *dpy = glXGetCurrentDisplay();
	std::vector<STR_String> Xextensions = STR_String(glXQueryExtensionsString(dpy, DefaultScreen(dpy))).Explode(' ');
	if (std::find(Xextensions.begin(), Xextensions.end(), "GLX_ARB_get_proc_address") != Xextensions.end()) 
	{
		void *libGL = dlopen("libGL.so", RTLD_LAZY);
		if (libGL)
		{
			bglGetProcAddress = (PFNBGLXGETPROCADDRESSARBPROC) (dlsym(libGL, "glXGetProcAddressARB"));
			dlclose(libGL);
			if (!bglGetProcAddress)
				bglGetProcAddress = (PFNBGLXGETPROCADDRESSARBPROC) _getProcAddress;
		}
	}
}

static void bglDeallocEntryPoints (void) {}

#elif defined(WIN32)
static void bglInitEntryPoints (void) {}
static void bglDeallocEntryPoints (void) {}

#define bglGetProcAddress(entry) wglGetProcAddress((LPCSTR) entry)

#else /* Unknown Platform - disable extensions */
static void bglInitEntryPoints (void) {}
static void bglDeallocEntryPoints (void) {}

static void *bglGetProcAddress(const GLubyte* entry)
{
	/* No Extensions! */
	return NULL;
}

#endif /* End Platform Specific */

/* -----------------------------------------------------------------------------

   GL Extension Manager.
*/

RAS_GLExtensionManager::RAS_GLExtensionManager(int debug) :
	m_debug(debug)
{
	bglInitEntryPoints (); //init bundle
	EnableExtension(_BGL_TEST);
	LinkExtensions();
}

RAS_GLExtensionManager::~RAS_GLExtensionManager()
{
	bglDeallocEntryPoints();
}

bool RAS_GLExtensionManager::QueryExtension(STR_String extension_name)
{
	return std::find(extensions.begin(), extensions.end(), extension_name) != extensions.end();
}

bool RAS_GLExtensionManager::QueryExtension(RAS_GLExtensionManager::ExtensionName name)
{
	unsigned int num = (unsigned int) name;
	if (num >= NUM_EXTENSIONS)
		return false;
	
	return (enabled_extensions[num/(8*sizeof(unsigned int))] & (1<<(num%(8*sizeof(unsigned int))))) != 0;
}

bool RAS_GLExtensionManager::QueryVersion(int major, int minor)
{
	STR_String gl_version = STR_String((const char *) glGetString(GL_VERSION));
	int i = gl_version.Find('.');
	STR_String gl_major = gl_version.Left(i);
	STR_String gl_minor = gl_version.Mid(i+1, gl_version.FindOneOf(". ", i+1) - i - 1);
	
	if (m_debug)
	{
		static bool doQueryVersion = true;
		if (doQueryVersion)
		{
			doQueryVersion = false;
			std::cout << "GL_VERSION: " << gl_major << "." << gl_minor << " (" << gl_version << ")" << std::endl;
		}
	}
	
	if (gl_major.ToInt() >= major && gl_minor.ToInt() >= minor)
		return true;
		
	return false;
}


void RAS_GLExtensionManager::EnableExtension(RAS_GLExtensionManager::ExtensionName name)
{
	unsigned int num = (unsigned int) name;
	if (num < NUM_EXTENSIONS)
		enabled_extensions[num/(8*sizeof(unsigned int))] |= (1<<(num%(8*sizeof(unsigned int))));
}

/*******************************************************************************
1. Extension function entry points go here

Need to #ifdef (compile time test for extension)
Add null functions if appropriate

Some extensions have been incorporated into the core GL, eg Multitexture was 
added in GL v1.1.  If Blender calls one of these functions before they are 
linked, it will crash.  Even worse, if Blender *indirectly* calls one of these 
functions, (ie the GL implementation calls them itself) Blender will crash.

We fix this by adding them to the RAS_GL namespace - the functions are now 
private to the gameengine.  Code can transparently use extensions by adding:

using namespace RAS_GL;

to their source.  Cunning like a weasel.

/******************************************************************************/

namespace RAS_GL {
/* Generated from mkglext.py */

/* GL_EXT_compiled_vertex_array */
#ifdef GL_EXT_compiled_vertex_array
static void APIENTRY _lockfunc(GLint first,GLsizei count) {};
static void APIENTRY _unlockfunc() {};
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT=_lockfunc;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT=_unlockfunc;
#endif

#if defined(GL_ARB_transpose_matrix)
PFNGLLOADTRANSPOSEMATRIXFARBPROC glLoadTransposeMatrixfARB;
PFNGLLOADTRANSPOSEMATRIXDARBPROC glLoadTransposeMatrixdARB;
PFNGLMULTTRANSPOSEMATRIXFARBPROC glMultTransposeMatrixfARB;
PFNGLMULTTRANSPOSEMATRIXDARBPROC glMultTransposeMatrixdARB;
#endif

#if defined(GL_ARB_multisample)
PFNGLSAMPLECOVERAGEARBPROC glSampleCoverageARB;
#endif

#if defined(GL_ARB_texture_env_add)
#endif

#if defined(GL_ARB_texture_cube_map)
#endif

#if defined(GL_ARB_texture_compression)
PFNGLCOMPRESSEDTEXIMAGE3DARBPROC glCompressedTexImage3DARB;
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2DARB;
PFNGLCOMPRESSEDTEXIMAGE1DARBPROC glCompressedTexImage1DARB;
PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC glCompressedTexSubImage3DARB;
PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC glCompressedTexSubImage2DARB;
PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC glCompressedTexSubImage1DARB;
PFNGLGETCOMPRESSEDTEXIMAGEARBPROC glGetCompressedTexImageARB;
#endif

#if defined(GL_ARB_texture_border_clamp)
#endif

#if defined(GL_ARB_vertex_blend)
PFNGLWEIGHTBVARBPROC glWeightbvARB;
PFNGLWEIGHTSVARBPROC glWeightsvARB;
PFNGLWEIGHTIVARBPROC glWeightivARB;
PFNGLWEIGHTFVARBPROC glWeightfvARB;
PFNGLWEIGHTDVARBPROC glWeightdvARB;
PFNGLWEIGHTUBVARBPROC glWeightubvARB;
PFNGLWEIGHTUSVARBPROC glWeightusvARB;
PFNGLWEIGHTUIVARBPROC glWeightuivARB;
PFNGLWEIGHTPOINTERARBPROC glWeightPointerARB;
PFNGLVERTEXBLENDARBPROC glVertexBlendARB;
#endif

#if defined(GL_ARB_matrix_palette)
PFNGLCURRENTPALETTEMATRIXARBPROC glCurrentPaletteMatrixARB;
PFNGLMATRIXINDEXUBVARBPROC glMatrixIndexubvARB;
PFNGLMATRIXINDEXUSVARBPROC glMatrixIndexusvARB;
PFNGLMATRIXINDEXUIVARBPROC glMatrixIndexuivARB;
PFNGLMATRIXINDEXPOINTERARBPROC glMatrixIndexPointerARB;
#endif

#if defined(GL_ARB_texture_env_combine)
#endif

#if defined(GL_ARB_texture_env_crossbar)
#endif

#if defined(GL_ARB_texture_env_dot3)
#endif

#if defined(GL_ARB_texture_mirrored_repeat)
#endif

#if defined(GL_ARB_depth_texture)
#endif

#if defined(GL_ARB_shadow)
#endif

#if defined(GL_ARB_shadow_ambient)
#endif

#if defined(GL_ARB_window_pos)
PFNGLWINDOWPOS2DARBPROC glWindowPos2dARB;
PFNGLWINDOWPOS2DVARBPROC glWindowPos2dvARB;
PFNGLWINDOWPOS2FARBPROC glWindowPos2fARB;
PFNGLWINDOWPOS2FVARBPROC glWindowPos2fvARB;
PFNGLWINDOWPOS2IARBPROC glWindowPos2iARB;
PFNGLWINDOWPOS2IVARBPROC glWindowPos2ivARB;
PFNGLWINDOWPOS2SARBPROC glWindowPos2sARB;
PFNGLWINDOWPOS2SVARBPROC glWindowPos2svARB;
PFNGLWINDOWPOS3DARBPROC glWindowPos3dARB;
PFNGLWINDOWPOS3DVARBPROC glWindowPos3dvARB;
PFNGLWINDOWPOS3FARBPROC glWindowPos3fARB;
PFNGLWINDOWPOS3FVARBPROC glWindowPos3fvARB;
PFNGLWINDOWPOS3IARBPROC glWindowPos3iARB;
PFNGLWINDOWPOS3IVARBPROC glWindowPos3ivARB;
PFNGLWINDOWPOS3SARBPROC glWindowPos3sARB;
PFNGLWINDOWPOS3SVARBPROC glWindowPos3svARB;
#endif

#if defined(GL_ARB_vertex_program)
PFNGLVERTEXATTRIB1DARBPROC glVertexAttrib1dARB;
PFNGLVERTEXATTRIB1DVARBPROC glVertexAttrib1dvARB;
PFNGLVERTEXATTRIB1FARBPROC glVertexAttrib1fARB;
PFNGLVERTEXATTRIB1FVARBPROC glVertexAttrib1fvARB;
PFNGLVERTEXATTRIB1SARBPROC glVertexAttrib1sARB;
PFNGLVERTEXATTRIB1SVARBPROC glVertexAttrib1svARB;
PFNGLVERTEXATTRIB2DARBPROC glVertexAttrib2dARB;
PFNGLVERTEXATTRIB2DVARBPROC glVertexAttrib2dvARB;
PFNGLVERTEXATTRIB2FARBPROC glVertexAttrib2fARB;
PFNGLVERTEXATTRIB2FVARBPROC glVertexAttrib2fvARB;
PFNGLVERTEXATTRIB2SARBPROC glVertexAttrib2sARB;
PFNGLVERTEXATTRIB2SVARBPROC glVertexAttrib2svARB;
PFNGLVERTEXATTRIB3DARBPROC glVertexAttrib3dARB;
PFNGLVERTEXATTRIB3DVARBPROC glVertexAttrib3dvARB;
PFNGLVERTEXATTRIB3FARBPROC glVertexAttrib3fARB;
PFNGLVERTEXATTRIB3FVARBPROC glVertexAttrib3fvARB;
PFNGLVERTEXATTRIB3SARBPROC glVertexAttrib3sARB;
PFNGLVERTEXATTRIB3SVARBPROC glVertexAttrib3svARB;
PFNGLVERTEXATTRIB4NBVARBPROC glVertexAttrib4NbvARB;
PFNGLVERTEXATTRIB4NIVARBPROC glVertexAttrib4NivARB;
PFNGLVERTEXATTRIB4NSVARBPROC glVertexAttrib4NsvARB;
PFNGLVERTEXATTRIB4NUBARBPROC glVertexAttrib4NubARB;
PFNGLVERTEXATTRIB4NUBVARBPROC glVertexAttrib4NubvARB;
PFNGLVERTEXATTRIB4NUIVARBPROC glVertexAttrib4NuivARB;
PFNGLVERTEXATTRIB4NUSVARBPROC glVertexAttrib4NusvARB;
PFNGLVERTEXATTRIB4BVARBPROC glVertexAttrib4bvARB;
PFNGLVERTEXATTRIB4DARBPROC glVertexAttrib4dARB;
PFNGLVERTEXATTRIB4DVARBPROC glVertexAttrib4dvARB;
PFNGLVERTEXATTRIB4FARBPROC glVertexAttrib4fARB;
PFNGLVERTEXATTRIB4FVARBPROC glVertexAttrib4fvARB;
PFNGLVERTEXATTRIB4IVARBPROC glVertexAttrib4ivARB;
PFNGLVERTEXATTRIB4SARBPROC glVertexAttrib4sARB;
PFNGLVERTEXATTRIB4SVARBPROC glVertexAttrib4svARB;
PFNGLVERTEXATTRIB4UBVARBPROC glVertexAttrib4ubvARB;
PFNGLVERTEXATTRIB4UIVARBPROC glVertexAttrib4uivARB;
PFNGLVERTEXATTRIB4USVARBPROC glVertexAttrib4usvARB;
PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
PFNGLBINDPROGRAMARBPROC glBindProgramARB;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
PFNGLPROGRAMENVPARAMETER4DARBPROC glProgramEnvParameter4dARB;
PFNGLPROGRAMENVPARAMETER4DVARBPROC glProgramEnvParameter4dvARB;
PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB;
PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC glProgramLocalParameter4dARB;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glProgramLocalParameter4dvARB;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC glGetProgramEnvParameterdvARB;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC glGetProgramEnvParameterfvARB;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glGetProgramLocalParameterdvARB;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glGetProgramLocalParameterfvARB;
PFNGLGETPROGRAMIVARBPROC glGetProgramivARB;
PFNGLGETPROGRAMSTRINGARBPROC glGetProgramStringARB;
PFNGLGETVERTEXATTRIBDVARBPROC glGetVertexAttribdvARB;
PFNGLGETVERTEXATTRIBFVARBPROC glGetVertexAttribfvARB;
PFNGLGETVERTEXATTRIBIVARBPROC glGetVertexAttribivARB;
PFNGLGETVERTEXATTRIBPOINTERVARBPROC glGetVertexAttribPointervARB;
PFNGLISPROGRAMARBPROC glIsProgramARB;
#endif

#if defined(GL_ARB_fragment_program)
#endif

#if defined(GL_ARB_vertex_buffer_object)
PFNGLBINDBUFFERARBPROC glBindBufferARB;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;
PFNGLGENBUFFERSARBPROC glGenBuffersARB;
PFNGLISBUFFERARBPROC glIsBufferARB;
PFNGLBUFFERDATAARBPROC glBufferDataARB;
PFNGLBUFFERSUBDATAARBPROC glBufferSubDataARB;
PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubDataARB;
PFNGLMAPBUFFERARBPROC glMapBufferARB;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB;
PFNGLGETBUFFERPARAMETERIVARBPROC glGetBufferParameterivARB;
PFNGLGETBUFFERPOINTERVARBPROC glGetBufferPointervARB;
#endif

#if defined(GL_ARB_occlusion_query)
PFNGLGENQUERIESARBPROC glGenQueriesARB;
PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB;
PFNGLISQUERYARBPROC glIsQueryARB;
PFNGLBEGINQUERYARBPROC glBeginQueryARB;
PFNGLENDQUERYARBPROC glEndQueryARB;
PFNGLGETQUERYIVARBPROC glGetQueryivARB;
PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB;
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB;
#endif

#if defined(GL_ARB_shader_objects)
PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
PFNGLGETHANDLEARBPROC glGetHandleARB;
PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB;
PFNGLUNIFORM1FARBPROC glUniform1fARB;
PFNGLUNIFORM2FARBPROC glUniform2fARB;
PFNGLUNIFORM3FARBPROC glUniform3fARB;
PFNGLUNIFORM4FARBPROC glUniform4fARB;
PFNGLUNIFORM1IARBPROC glUniform1iARB;
PFNGLUNIFORM2IARBPROC glUniform2iARB;
PFNGLUNIFORM3IARBPROC glUniform3iARB;
PFNGLUNIFORM4IARBPROC glUniform4iARB;
PFNGLUNIFORM1FVARBPROC glUniform1fvARB;
PFNGLUNIFORM2FVARBPROC glUniform2fvARB;
PFNGLUNIFORM3FVARBPROC glUniform3fvARB;
PFNGLUNIFORM4FVARBPROC glUniform4fvARB;
PFNGLUNIFORM1IVARBPROC glUniform1ivARB;
PFNGLUNIFORM2IVARBPROC glUniform2ivARB;
PFNGLUNIFORM3IVARBPROC glUniform3ivARB;
PFNGLUNIFORM4IVARBPROC glUniform4ivARB;
PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB;
PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB;
PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB;
PFNGLGETOBJECTPARAMETERFVARBPROC glGetObjectParameterfvARB;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
PFNGLGETATTACHEDOBJECTSARBPROC glGetAttachedObjectsARB;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB;
PFNGLGETUNIFORMFVARBPROC glGetUniformfvARB;
PFNGLGETUNIFORMIVARBPROC glGetUniformivARB;
PFNGLGETSHADERSOURCEARBPROC glGetShaderSourceARB;
#endif

#if defined(GL_ARB_vertex_shader)
PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB;
PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB;
PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB;
#endif

#if defined(GL_ARB_fragment_shader)
#endif

#if defined(GL_ARB_shading_language_100)
#endif

#if defined(GL_ARB_texture_non_power_of_two)
#endif

#if defined(GL_ARB_point_sprite)
#endif

#if defined(GL_ARB_fragment_program_shadow)
#endif

#if defined(GL_EXT_abgr)
#endif

#if defined(GL_EXT_texture3D)
PFNGLTEXIMAGE3DEXTPROC glTexImage3DEXT;
PFNGLTEXSUBIMAGE3DEXTPROC glTexSubImage3DEXT;
#endif

#if defined(GL_SGIS_texture_filter4)
PFNGLGETTEXFILTERFUNCSGISPROC glGetTexFilterFuncSGIS;
PFNGLTEXFILTERFUNCSGISPROC glTexFilterFuncSGIS;
#endif

#if defined(GL_EXT_histogram)
PFNGLGETHISTOGRAMEXTPROC glGetHistogramEXT;
PFNGLGETHISTOGRAMPARAMETERFVEXTPROC glGetHistogramParameterfvEXT;
PFNGLGETHISTOGRAMPARAMETERIVEXTPROC glGetHistogramParameterivEXT;
PFNGLGETMINMAXEXTPROC glGetMinmaxEXT;
PFNGLGETMINMAXPARAMETERFVEXTPROC glGetMinmaxParameterfvEXT;
PFNGLGETMINMAXPARAMETERIVEXTPROC glGetMinmaxParameterivEXT;
PFNGLHISTOGRAMEXTPROC glHistogramEXT;
PFNGLMINMAXEXTPROC glMinmaxEXT;
PFNGLRESETHISTOGRAMEXTPROC glResetHistogramEXT;
PFNGLRESETMINMAXEXTPROC glResetMinmaxEXT;
#endif

#if defined(GL_EXT_convolution)
PFNGLCONVOLUTIONFILTER1DEXTPROC glConvolutionFilter1DEXT;
PFNGLCONVOLUTIONFILTER2DEXTPROC glConvolutionFilter2DEXT;
PFNGLCONVOLUTIONPARAMETERFEXTPROC glConvolutionParameterfEXT;
PFNGLCONVOLUTIONPARAMETERFVEXTPROC glConvolutionParameterfvEXT;
PFNGLCONVOLUTIONPARAMETERIEXTPROC glConvolutionParameteriEXT;
PFNGLCONVOLUTIONPARAMETERIVEXTPROC glConvolutionParameterivEXT;
PFNGLCOPYCONVOLUTIONFILTER1DEXTPROC glCopyConvolutionFilter1DEXT;
PFNGLCOPYCONVOLUTIONFILTER2DEXTPROC glCopyConvolutionFilter2DEXT;
PFNGLGETCONVOLUTIONFILTEREXTPROC glGetConvolutionFilterEXT;
PFNGLGETCONVOLUTIONPARAMETERFVEXTPROC glGetConvolutionParameterfvEXT;
PFNGLGETCONVOLUTIONPARAMETERIVEXTPROC glGetConvolutionParameterivEXT;
PFNGLGETSEPARABLEFILTEREXTPROC glGetSeparableFilterEXT;
PFNGLSEPARABLEFILTER2DEXTPROC glSeparableFilter2DEXT;
#endif

#if defined(GL_SGI_color_table)
PFNGLCOLORTABLESGIPROC glColorTableSGI;
PFNGLCOLORTABLEPARAMETERFVSGIPROC glColorTableParameterfvSGI;
PFNGLCOLORTABLEPARAMETERIVSGIPROC glColorTableParameterivSGI;
PFNGLCOPYCOLORTABLESGIPROC glCopyColorTableSGI;
PFNGLGETCOLORTABLESGIPROC glGetColorTableSGI;
PFNGLGETCOLORTABLEPARAMETERFVSGIPROC glGetColorTableParameterfvSGI;
PFNGLGETCOLORTABLEPARAMETERIVSGIPROC glGetColorTableParameterivSGI;
#endif

#if defined(GL_SGIX_pixel_texture)
PFNGLPIXELTEXGENSGIXPROC glPixelTexGenSGIX;
#endif

#if defined(GL_SGIS_pixel_texture)
PFNGLPIXELTEXGENPARAMETERISGISPROC glPixelTexGenParameteriSGIS;
PFNGLPIXELTEXGENPARAMETERIVSGISPROC glPixelTexGenParameterivSGIS;
PFNGLPIXELTEXGENPARAMETERFSGISPROC glPixelTexGenParameterfSGIS;
PFNGLPIXELTEXGENPARAMETERFVSGISPROC glPixelTexGenParameterfvSGIS;
PFNGLGETPIXELTEXGENPARAMETERIVSGISPROC glGetPixelTexGenParameterivSGIS;
PFNGLGETPIXELTEXGENPARAMETERFVSGISPROC glGetPixelTexGenParameterfvSGIS;
#endif

#if defined(GL_SGIS_texture4D)
PFNGLTEXIMAGE4DSGISPROC glTexImage4DSGIS;
PFNGLTEXSUBIMAGE4DSGISPROC glTexSubImage4DSGIS;
#endif

#if defined(GL_SGI_texture_color_table)
#endif

#if defined(GL_EXT_cmyka)
#endif

#if defined(GL_SGIS_detail_texture)
PFNGLDETAILTEXFUNCSGISPROC glDetailTexFuncSGIS;
PFNGLGETDETAILTEXFUNCSGISPROC glGetDetailTexFuncSGIS;
#endif

#if defined(GL_SGIS_sharpen_texture)
PFNGLSHARPENTEXFUNCSGISPROC glSharpenTexFuncSGIS;
PFNGLGETSHARPENTEXFUNCSGISPROC glGetSharpenTexFuncSGIS;
#endif

#if defined(GL_EXT_packed_pixels)
#endif

#if defined(GL_SGIS_texture_lod)
#endif

#if defined(GL_SGIS_multisample)
PFNGLSAMPLEMASKSGISPROC glSampleMaskSGIS;
PFNGLSAMPLEPATTERNSGISPROC glSamplePatternSGIS;
#endif

#if defined(GL_EXT_rescale_normal)
#endif

#if defined(GL_EXT_misc_attribute)
#endif

#if defined(GL_SGIS_generate_mipmap)
#endif

#if defined(GL_SGIX_clipmap)
#endif

#if defined(GL_SGIX_shadow)
#endif

#if defined(GL_SGIS_texture_edge_clamp)
#endif

#if defined(GL_SGIS_texture_border_clamp)
#endif

#if defined(GL_EXT_blend_minmax)
PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;
#endif

#if defined(GL_EXT_blend_subtract)
#endif

#if defined(GL_EXT_blend_logic_op)
#endif

#if defined(GL_SGIX_interlace)
#endif

#if defined(GL_SGIX_sprite)
PFNGLSPRITEPARAMETERFSGIXPROC glSpriteParameterfSGIX;
PFNGLSPRITEPARAMETERFVSGIXPROC glSpriteParameterfvSGIX;
PFNGLSPRITEPARAMETERISGIXPROC glSpriteParameteriSGIX;
PFNGLSPRITEPARAMETERIVSGIXPROC glSpriteParameterivSGIX;
#endif

#if defined(GL_SGIX_texture_multi_buffer)
#endif

#if defined(GL_SGIX_instruments)
PFNGLGETINSTRUMENTSSGIXPROC glGetInstrumentsSGIX;
PFNGLINSTRUMENTSBUFFERSGIXPROC glInstrumentsBufferSGIX;
PFNGLPOLLINSTRUMENTSSGIXPROC glPollInstrumentsSGIX;
PFNGLREADINSTRUMENTSSGIXPROC glReadInstrumentsSGIX;
PFNGLSTARTINSTRUMENTSSGIXPROC glStartInstrumentsSGIX;
PFNGLSTOPINSTRUMENTSSGIXPROC glStopInstrumentsSGIX;
#endif

#if defined(GL_SGIX_texture_scale_bias)
#endif

#if defined(GL_SGIX_framezoom)
PFNGLFRAMEZOOMSGIXPROC glFrameZoomSGIX;
#endif

#if defined(GL_SGIX_tag_sample_buffer)
PFNGLTAGSAMPLEBUFFERSGIXPROC glTagSampleBufferSGIX;
#endif

#if defined(GL_SGIX_reference_plane)
PFNGLREFERENCEPLANESGIXPROC glReferencePlaneSGIX;
#endif

#if defined(GL_SGIX_flush_raster)
PFNGLFLUSHRASTERSGIXPROC glFlushRasterSGIX;
#endif

#if defined(GL_SGIX_depth_texture)
#endif

#if defined(GL_SGIS_fog_function)
PFNGLFOGFUNCSGISPROC glFogFuncSGIS;
PFNGLGETFOGFUNCSGISPROC glGetFogFuncSGIS;
#endif

#if defined(GL_SGIX_fog_offset)
#endif

#if defined(GL_HP_image_transform)
PFNGLIMAGETRANSFORMPARAMETERIHPPROC glImageTransformParameteriHP;
PFNGLIMAGETRANSFORMPARAMETERFHPPROC glImageTransformParameterfHP;
PFNGLIMAGETRANSFORMPARAMETERIVHPPROC glImageTransformParameterivHP;
PFNGLIMAGETRANSFORMPARAMETERFVHPPROC glImageTransformParameterfvHP;
PFNGLGETIMAGETRANSFORMPARAMETERIVHPPROC glGetImageTransformParameterivHP;
PFNGLGETIMAGETRANSFORMPARAMETERFVHPPROC glGetImageTransformParameterfvHP;
#endif

#if defined(GL_HP_convolution_border_modes)
#endif

#if defined(GL_SGIX_texture_add_env)
#endif

#if defined(GL_EXT_color_subtable)
PFNGLCOLORSUBTABLEEXTPROC glColorSubTableEXT;
PFNGLCOPYCOLORSUBTABLEEXTPROC glCopyColorSubTableEXT;
#endif

#if defined(GL_PGI_vertex_hints)
#endif

#if defined(GL_PGI_misc_hints)
PFNGLHINTPGIPROC glHintPGI;
#endif

#if defined(GL_EXT_paletted_texture)
PFNGLCOLORTABLEEXTPROC glColorTableEXT;
PFNGLGETCOLORTABLEEXTPROC glGetColorTableEXT;
PFNGLGETCOLORTABLEPARAMETERIVEXTPROC glGetColorTableParameterivEXT;
PFNGLGETCOLORTABLEPARAMETERFVEXTPROC glGetColorTableParameterfvEXT;
#endif

#if defined(GL_EXT_clip_volume_hint)
#endif

#if defined(GL_SGIX_list_priority)
PFNGLGETLISTPARAMETERFVSGIXPROC glGetListParameterfvSGIX;
PFNGLGETLISTPARAMETERIVSGIXPROC glGetListParameterivSGIX;
PFNGLLISTPARAMETERFSGIXPROC glListParameterfSGIX;
PFNGLLISTPARAMETERFVSGIXPROC glListParameterfvSGIX;
PFNGLLISTPARAMETERISGIXPROC glListParameteriSGIX;
PFNGLLISTPARAMETERIVSGIXPROC glListParameterivSGIX;
#endif

#if defined(GL_SGIX_ir_instrument1)
#endif

#if defined(GL_SGIX_texture_lod_bias)
#endif

#if defined(GL_SGIX_shadow_ambient)
#endif

#if defined(GL_EXT_index_texture)
#endif

#if defined(GL_EXT_index_material)
PFNGLINDEXMATERIALEXTPROC glIndexMaterialEXT;
#endif

#if defined(GL_EXT_index_func)
PFNGLINDEXFUNCEXTPROC glIndexFuncEXT;
#endif

#if defined(GL_EXT_index_array_formats)
#endif

#if defined(GL_EXT_cull_vertex)
PFNGLCULLPARAMETERDVEXTPROC glCullParameterdvEXT;
PFNGLCULLPARAMETERFVEXTPROC glCullParameterfvEXT;
#endif

#if defined(GL_SGIX_ycrcb)
#endif

#if defined(GL_IBM_rasterpos_clip)
#endif

#if defined(GL_HP_texture_lighting)
#endif

#if defined(GL_EXT_draw_range_elements)
PFNGLDRAWRANGEELEMENTSEXTPROC glDrawRangeElementsEXT;
#endif

#if defined(GL_WIN_phong_shading)
#endif

#if defined(GL_WIN_specular_fog)
#endif

#if defined(GL_EXT_light_texture)
PFNGLAPPLYTEXTUREEXTPROC glApplyTextureEXT;
PFNGLTEXTURELIGHTEXTPROC glTextureLightEXT;
PFNGLTEXTUREMATERIALEXTPROC glTextureMaterialEXT;
#endif

#if defined(GL_SGIX_blend_alpha_minmax)
#endif

#if defined(GL_EXT_bgra)
#endif

#if defined(GL_SGIX_async)
PFNGLASYNCMARKERSGIXPROC glAsyncMarkerSGIX;
PFNGLFINISHASYNCSGIXPROC glFinishAsyncSGIX;
PFNGLPOLLASYNCSGIXPROC glPollAsyncSGIX;
PFNGLGENASYNCMARKERSSGIXPROC glGenAsyncMarkersSGIX;
PFNGLDELETEASYNCMARKERSSGIXPROC glDeleteAsyncMarkersSGIX;
PFNGLISASYNCMARKERSGIXPROC glIsAsyncMarkerSGIX;
#endif

#if defined(GL_SGIX_async_pixel)
#endif

#if defined(GL_SGIX_async_histogram)
#endif

#if defined(GL_INTEL_parallel_arrays)
PFNGLVERTEXPOINTERVINTELPROC glVertexPointervINTEL;
PFNGLNORMALPOINTERVINTELPROC glNormalPointervINTEL;
PFNGLCOLORPOINTERVINTELPROC glColorPointervINTEL;
PFNGLTEXCOORDPOINTERVINTELPROC glTexCoordPointervINTEL;
#endif

#if defined(GL_HP_occlusion_test)
#endif

#if defined(GL_EXT_pixel_transform)
PFNGLPIXELTRANSFORMPARAMETERIEXTPROC glPixelTransformParameteriEXT;
PFNGLPIXELTRANSFORMPARAMETERFEXTPROC glPixelTransformParameterfEXT;
PFNGLPIXELTRANSFORMPARAMETERIVEXTPROC glPixelTransformParameterivEXT;
PFNGLPIXELTRANSFORMPARAMETERFVEXTPROC glPixelTransformParameterfvEXT;
#endif

#if defined(GL_EXT_pixel_transform_color_table)
#endif

#if defined(GL_EXT_shared_texture_palette)
#endif

#if defined(GL_EXT_separate_specular_color)
#endif

#if defined(GL_EXT_secondary_color)
PFNGLSECONDARYCOLOR3BEXTPROC glSecondaryColor3bEXT;
PFNGLSECONDARYCOLOR3BVEXTPROC glSecondaryColor3bvEXT;
PFNGLSECONDARYCOLOR3DEXTPROC glSecondaryColor3dEXT;
PFNGLSECONDARYCOLOR3DVEXTPROC glSecondaryColor3dvEXT;
PFNGLSECONDARYCOLOR3FEXTPROC glSecondaryColor3fEXT;
PFNGLSECONDARYCOLOR3FVEXTPROC glSecondaryColor3fvEXT;
PFNGLSECONDARYCOLOR3IEXTPROC glSecondaryColor3iEXT;
PFNGLSECONDARYCOLOR3IVEXTPROC glSecondaryColor3ivEXT;
PFNGLSECONDARYCOLOR3SEXTPROC glSecondaryColor3sEXT;
PFNGLSECONDARYCOLOR3SVEXTPROC glSecondaryColor3svEXT;
PFNGLSECONDARYCOLOR3UBEXTPROC glSecondaryColor3ubEXT;
PFNGLSECONDARYCOLOR3UBVEXTPROC glSecondaryColor3ubvEXT;
PFNGLSECONDARYCOLOR3UIEXTPROC glSecondaryColor3uiEXT;
PFNGLSECONDARYCOLOR3UIVEXTPROC glSecondaryColor3uivEXT;
PFNGLSECONDARYCOLOR3USEXTPROC glSecondaryColor3usEXT;
PFNGLSECONDARYCOLOR3USVEXTPROC glSecondaryColor3usvEXT;
PFNGLSECONDARYCOLORPOINTEREXTPROC glSecondaryColorPointerEXT;
#endif

#if defined(GL_EXT_texture_perturb_normal)
PFNGLTEXTURENORMALEXTPROC glTextureNormalEXT;
#endif

#if defined(GL_EXT_multi_draw_arrays)
PFNGLMULTIDRAWARRAYSEXTPROC glMultiDrawArraysEXT;
PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElementsEXT;
#endif

#if defined(GL_EXT_fog_coord)
PFNGLFOGCOORDFEXTPROC glFogCoordfEXT;
PFNGLFOGCOORDFVEXTPROC glFogCoordfvEXT;
PFNGLFOGCOORDDEXTPROC glFogCoorddEXT;
PFNGLFOGCOORDDVEXTPROC glFogCoorddvEXT;
PFNGLFOGCOORDPOINTEREXTPROC glFogCoordPointerEXT;
#endif

#if defined(GL_REND_screen_coordinates)
#endif

#if defined(GL_EXT_coordinate_frame)
PFNGLTANGENT3BEXTPROC glTangent3bEXT;
PFNGLTANGENT3BVEXTPROC glTangent3bvEXT;
PFNGLTANGENT3DEXTPROC glTangent3dEXT;
PFNGLTANGENT3DVEXTPROC glTangent3dvEXT;
PFNGLTANGENT3FEXTPROC glTangent3fEXT;
PFNGLTANGENT3FVEXTPROC glTangent3fvEXT;
PFNGLTANGENT3IEXTPROC glTangent3iEXT;
PFNGLTANGENT3IVEXTPROC glTangent3ivEXT;
PFNGLTANGENT3SEXTPROC glTangent3sEXT;
PFNGLTANGENT3SVEXTPROC glTangent3svEXT;
PFNGLBINORMAL3BEXTPROC glBinormal3bEXT;
PFNGLBINORMAL3BVEXTPROC glBinormal3bvEXT;
PFNGLBINORMAL3DEXTPROC glBinormal3dEXT;
PFNGLBINORMAL3DVEXTPROC glBinormal3dvEXT;
PFNGLBINORMAL3FEXTPROC glBinormal3fEXT;
PFNGLBINORMAL3FVEXTPROC glBinormal3fvEXT;
PFNGLBINORMAL3IEXTPROC glBinormal3iEXT;
PFNGLBINORMAL3IVEXTPROC glBinormal3ivEXT;
PFNGLBINORMAL3SEXTPROC glBinormal3sEXT;
PFNGLBINORMAL3SVEXTPROC glBinormal3svEXT;
PFNGLTANGENTPOINTEREXTPROC glTangentPointerEXT;
PFNGLBINORMALPOINTEREXTPROC glBinormalPointerEXT;
#endif

#if defined(GL_EXT_texture_env_combine)
#endif

#if defined(GL_APPLE_specular_vector)
#endif

#if defined(GL_APPLE_transform_hint)
#endif

#if defined(GL_SUNX_constant_data)
PFNGLFINISHTEXTURESUNXPROC glFinishTextureSUNX;
#endif

#if defined(GL_SUN_global_alpha)
PFNGLGLOBALALPHAFACTORBSUNPROC glGlobalAlphaFactorbSUN;
PFNGLGLOBALALPHAFACTORSSUNPROC glGlobalAlphaFactorsSUN;
PFNGLGLOBALALPHAFACTORISUNPROC glGlobalAlphaFactoriSUN;
PFNGLGLOBALALPHAFACTORFSUNPROC glGlobalAlphaFactorfSUN;
PFNGLGLOBALALPHAFACTORDSUNPROC glGlobalAlphaFactordSUN;
PFNGLGLOBALALPHAFACTORUBSUNPROC glGlobalAlphaFactorubSUN;
PFNGLGLOBALALPHAFACTORUSSUNPROC glGlobalAlphaFactorusSUN;
PFNGLGLOBALALPHAFACTORUISUNPROC glGlobalAlphaFactoruiSUN;
#endif

#if defined(GL_SUN_triangle_list)
PFNGLREPLACEMENTCODEUISUNPROC glReplacementCodeuiSUN;
PFNGLREPLACEMENTCODEUSSUNPROC glReplacementCodeusSUN;
PFNGLREPLACEMENTCODEUBSUNPROC glReplacementCodeubSUN;
PFNGLREPLACEMENTCODEUIVSUNPROC glReplacementCodeuivSUN;
PFNGLREPLACEMENTCODEUSVSUNPROC glReplacementCodeusvSUN;
PFNGLREPLACEMENTCODEUBVSUNPROC glReplacementCodeubvSUN;
PFNGLREPLACEMENTCODEPOINTERSUNPROC glReplacementCodePointerSUN;
#endif

#if defined(GL_SUN_vertex)
PFNGLCOLOR4UBVERTEX2FSUNPROC glColor4ubVertex2fSUN;
PFNGLCOLOR4UBVERTEX2FVSUNPROC glColor4ubVertex2fvSUN;
PFNGLCOLOR4UBVERTEX3FSUNPROC glColor4ubVertex3fSUN;
PFNGLCOLOR4UBVERTEX3FVSUNPROC glColor4ubVertex3fvSUN;
PFNGLCOLOR3FVERTEX3FSUNPROC glColor3fVertex3fSUN;
PFNGLCOLOR3FVERTEX3FVSUNPROC glColor3fVertex3fvSUN;
PFNGLNORMAL3FVERTEX3FSUNPROC glNormal3fVertex3fSUN;
PFNGLNORMAL3FVERTEX3FVSUNPROC glNormal3fVertex3fvSUN;
PFNGLCOLOR4FNORMAL3FVERTEX3FSUNPROC glColor4fNormal3fVertex3fSUN;
PFNGLCOLOR4FNORMAL3FVERTEX3FVSUNPROC glColor4fNormal3fVertex3fvSUN;
PFNGLTEXCOORD2FVERTEX3FSUNPROC glTexCoord2fVertex3fSUN;
PFNGLTEXCOORD2FVERTEX3FVSUNPROC glTexCoord2fVertex3fvSUN;
PFNGLTEXCOORD4FVERTEX4FSUNPROC glTexCoord4fVertex4fSUN;
PFNGLTEXCOORD4FVERTEX4FVSUNPROC glTexCoord4fVertex4fvSUN;
PFNGLTEXCOORD2FCOLOR4UBVERTEX3FSUNPROC glTexCoord2fColor4ubVertex3fSUN;
PFNGLTEXCOORD2FCOLOR4UBVERTEX3FVSUNPROC glTexCoord2fColor4ubVertex3fvSUN;
PFNGLTEXCOORD2FCOLOR3FVERTEX3FSUNPROC glTexCoord2fColor3fVertex3fSUN;
PFNGLTEXCOORD2FCOLOR3FVERTEX3FVSUNPROC glTexCoord2fColor3fVertex3fvSUN;
PFNGLTEXCOORD2FNORMAL3FVERTEX3FSUNPROC glTexCoord2fNormal3fVertex3fSUN;
PFNGLTEXCOORD2FNORMAL3FVERTEX3FVSUNPROC glTexCoord2fNormal3fVertex3fvSUN;
PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC glTexCoord2fColor4fNormal3fVertex3fSUN;
PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC glTexCoord2fColor4fNormal3fVertex3fvSUN;
PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FSUNPROC glTexCoord4fColor4fNormal3fVertex4fSUN;
PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FVSUNPROC glTexCoord4fColor4fNormal3fVertex4fvSUN;
PFNGLREPLACEMENTCODEUIVERTEX3FSUNPROC glReplacementCodeuiVertex3fSUN;
PFNGLREPLACEMENTCODEUIVERTEX3FVSUNPROC glReplacementCodeuiVertex3fvSUN;
PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FSUNPROC glReplacementCodeuiColor4ubVertex3fSUN;
PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FVSUNPROC glReplacementCodeuiColor4ubVertex3fvSUN;
PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FSUNPROC glReplacementCodeuiColor3fVertex3fSUN;
PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FVSUNPROC glReplacementCodeuiColor3fVertex3fvSUN;
PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FSUNPROC glReplacementCodeuiNormal3fVertex3fSUN;
PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FVSUNPROC glReplacementCodeuiNormal3fVertex3fvSUN;
PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FSUNPROC glReplacementCodeuiColor4fNormal3fVertex3fSUN;
PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FVSUNPROC glReplacementCodeuiColor4fNormal3fVertex3fvSUN;
PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FSUNPROC glReplacementCodeuiTexCoord2fVertex3fSUN;
PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FVSUNPROC glReplacementCodeuiTexCoord2fVertex3fvSUN;
PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FSUNPROC glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN;
PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FVSUNPROC glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN;
PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN;
PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN;
#endif

#if defined(GL_EXT_blend_func_separate)
PFNGLBLENDFUNCSEPARATEEXTPROC glBlendFuncSeparateEXT;
#endif

#if defined(GL_INGR_color_clamp)
#endif

#if defined(GL_INGR_interlace_read)
#endif

#if defined(GL_EXT_stencil_wrap)
#endif

#if defined(GL_EXT_422_pixels)
#endif

#if defined(GL_NV_texgen_reflection)
#endif

#if defined(GL_SUN_convolution_border_modes)
#endif

#if defined(GL_EXT_texture_env_add)
#endif

#if defined(GL_EXT_texture_lod_bias)
#endif

#if defined(GL_EXT_texture_filter_anisotropic)
#endif

#if defined(GL_EXT_vertex_weighting)
PFNGLVERTEXWEIGHTFEXTPROC glVertexWeightfEXT;
PFNGLVERTEXWEIGHTFVEXTPROC glVertexWeightfvEXT;
PFNGLVERTEXWEIGHTPOINTEREXTPROC glVertexWeightPointerEXT;
#endif

#if defined(GL_NV_light_max_exponent)
#endif

#if defined(GL_NV_vertex_array_range)
PFNGLFLUSHVERTEXARRAYRANGENVPROC glFlushVertexArrayRangeNV;
PFNGLVERTEXARRAYRANGENVPROC glVertexArrayRangeNV;
#endif

#if defined(GL_NV_register_combiners)
PFNGLCOMBINERPARAMETERFVNVPROC glCombinerParameterfvNV;
PFNGLCOMBINERPARAMETERFNVPROC glCombinerParameterfNV;
PFNGLCOMBINERPARAMETERIVNVPROC glCombinerParameterivNV;
PFNGLCOMBINERPARAMETERINVPROC glCombinerParameteriNV;
PFNGLCOMBINERINPUTNVPROC glCombinerInputNV;
PFNGLCOMBINEROUTPUTNVPROC glCombinerOutputNV;
PFNGLFINALCOMBINERINPUTNVPROC glFinalCombinerInputNV;
PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC glGetCombinerInputParameterfvNV;
PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC glGetCombinerInputParameterivNV;
PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC glGetCombinerOutputParameterfvNV;
PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC glGetCombinerOutputParameterivNV;
PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC glGetFinalCombinerInputParameterfvNV;
PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC glGetFinalCombinerInputParameterivNV;
#endif

#if defined(GL_NV_fog_distance)
#endif

#if defined(GL_NV_texgen_emboss)
#endif

#if defined(GL_NV_blend_square)
#endif

#if defined(GL_NV_texture_env_combine4)
#endif

#if defined(GL_MESA_resize_buffers)
PFNGLRESIZEBUFFERSMESAPROC glResizeBuffersMESA;
#endif

#if defined(GL_MESA_window_pos)
PFNGLWINDOWPOS2DMESAPROC glWindowPos2dMESA;
PFNGLWINDOWPOS2DVMESAPROC glWindowPos2dvMESA;
PFNGLWINDOWPOS2FMESAPROC glWindowPos2fMESA;
PFNGLWINDOWPOS2FVMESAPROC glWindowPos2fvMESA;
PFNGLWINDOWPOS2IMESAPROC glWindowPos2iMESA;
PFNGLWINDOWPOS2IVMESAPROC glWindowPos2ivMESA;
PFNGLWINDOWPOS2SMESAPROC glWindowPos2sMESA;
PFNGLWINDOWPOS2SVMESAPROC glWindowPos2svMESA;
PFNGLWINDOWPOS3DMESAPROC glWindowPos3dMESA;
PFNGLWINDOWPOS3DVMESAPROC glWindowPos3dvMESA;
PFNGLWINDOWPOS3FMESAPROC glWindowPos3fMESA;
PFNGLWINDOWPOS3FVMESAPROC glWindowPos3fvMESA;
PFNGLWINDOWPOS3IMESAPROC glWindowPos3iMESA;
PFNGLWINDOWPOS3IVMESAPROC glWindowPos3ivMESA;
PFNGLWINDOWPOS3SMESAPROC glWindowPos3sMESA;
PFNGLWINDOWPOS3SVMESAPROC glWindowPos3svMESA;
PFNGLWINDOWPOS4DMESAPROC glWindowPos4dMESA;
PFNGLWINDOWPOS4DVMESAPROC glWindowPos4dvMESA;
PFNGLWINDOWPOS4FMESAPROC glWindowPos4fMESA;
PFNGLWINDOWPOS4FVMESAPROC glWindowPos4fvMESA;
PFNGLWINDOWPOS4IMESAPROC glWindowPos4iMESA;
PFNGLWINDOWPOS4IVMESAPROC glWindowPos4ivMESA;
PFNGLWINDOWPOS4SMESAPROC glWindowPos4sMESA;
PFNGLWINDOWPOS4SVMESAPROC glWindowPos4svMESA;
#endif

#if defined(GL_IBM_cull_vertex)
#endif

#if defined(GL_IBM_multimode_draw_arrays)
PFNGLMULTIMODEDRAWARRAYSIBMPROC glMultiModeDrawArraysIBM;
PFNGLMULTIMODEDRAWELEMENTSIBMPROC glMultiModeDrawElementsIBM;
#endif

#if defined(GL_IBM_vertex_array_lists)
PFNGLCOLORPOINTERLISTIBMPROC glColorPointerListIBM;
PFNGLSECONDARYCOLORPOINTERLISTIBMPROC glSecondaryColorPointerListIBM;
PFNGLEDGEFLAGPOINTERLISTIBMPROC glEdgeFlagPointerListIBM;
PFNGLFOGCOORDPOINTERLISTIBMPROC glFogCoordPointerListIBM;
PFNGLINDEXPOINTERLISTIBMPROC glIndexPointerListIBM;
PFNGLNORMALPOINTERLISTIBMPROC glNormalPointerListIBM;
PFNGLTEXCOORDPOINTERLISTIBMPROC glTexCoordPointerListIBM;
PFNGLVERTEXPOINTERLISTIBMPROC glVertexPointerListIBM;
#endif

#if defined(GL_3DFX_texture_compression_FXT1)
#endif

#if defined(GL_3DFX_multisample)
#endif

#if defined(GL_3DFX_tbuffer)
PFNGLTBUFFERMASK3DFXPROC glTbufferMask3DFX;
#endif

#if defined(GL_SGIX_vertex_preclip)
#endif

#if defined(GL_SGIX_resample)
#endif

#if defined(GL_SGIS_texture_color_mask)
PFNGLTEXTURECOLORMASKSGISPROC glTextureColorMaskSGIS;
#endif

#if defined(GL_EXT_texture_env_dot3)
#endif

#if defined(GL_ATI_texture_mirror_once)
#endif

#if defined(GL_NV_fence)
PFNGLDELETEFENCESNVPROC glDeleteFencesNV;
PFNGLGENFENCESNVPROC glGenFencesNV;
PFNGLISFENCENVPROC glIsFenceNV;
PFNGLTESTFENCENVPROC glTestFenceNV;
PFNGLGETFENCEIVNVPROC glGetFenceivNV;
PFNGLFINISHFENCENVPROC glFinishFenceNV;
PFNGLSETFENCENVPROC glSetFenceNV;
#endif

#if defined(GL_NV_evaluators)
PFNGLMAPCONTROLPOINTSNVPROC glMapControlPointsNV;
PFNGLMAPPARAMETERIVNVPROC glMapParameterivNV;
PFNGLMAPPARAMETERFVNVPROC glMapParameterfvNV;
PFNGLGETMAPCONTROLPOINTSNVPROC glGetMapControlPointsNV;
PFNGLGETMAPPARAMETERIVNVPROC glGetMapParameterivNV;
PFNGLGETMAPPARAMETERFVNVPROC glGetMapParameterfvNV;
PFNGLGETMAPATTRIBPARAMETERIVNVPROC glGetMapAttribParameterivNV;
PFNGLGETMAPATTRIBPARAMETERFVNVPROC glGetMapAttribParameterfvNV;
PFNGLEVALMAPSNVPROC glEvalMapsNV;
#endif

#if defined(GL_NV_packed_depth_stencil)
#endif

#if defined(GL_NV_register_combiners2)
PFNGLCOMBINERSTAGEPARAMETERFVNVPROC glCombinerStageParameterfvNV;
PFNGLGETCOMBINERSTAGEPARAMETERFVNVPROC glGetCombinerStageParameterfvNV;
#endif

#if defined(GL_NV_texture_compression_vtc)
#endif

#if defined(GL_NV_texture_rectangle)
#endif

#if defined(GL_NV_texture_shader)
#endif

#if defined(GL_NV_texture_shader2)
#endif

#if defined(GL_NV_vertex_array_range2)
#endif

#if defined(GL_NV_vertex_program)
PFNGLAREPROGRAMSRESIDENTNVPROC glAreProgramsResidentNV;
PFNGLBINDPROGRAMNVPROC glBindProgramNV;
PFNGLDELETEPROGRAMSNVPROC glDeleteProgramsNV;
PFNGLEXECUTEPROGRAMNVPROC glExecuteProgramNV;
PFNGLGENPROGRAMSNVPROC glGenProgramsNV;
PFNGLGETPROGRAMPARAMETERDVNVPROC glGetProgramParameterdvNV;
PFNGLGETPROGRAMPARAMETERFVNVPROC glGetProgramParameterfvNV;
PFNGLGETPROGRAMIVNVPROC glGetProgramivNV;
PFNGLGETPROGRAMSTRINGNVPROC glGetProgramStringNV;
PFNGLGETTRACKMATRIXIVNVPROC glGetTrackMatrixivNV;
PFNGLGETVERTEXATTRIBDVNVPROC glGetVertexAttribdvNV;
PFNGLGETVERTEXATTRIBFVNVPROC glGetVertexAttribfvNV;
PFNGLGETVERTEXATTRIBIVNVPROC glGetVertexAttribivNV;
PFNGLGETVERTEXATTRIBPOINTERVNVPROC glGetVertexAttribPointervNV;
PFNGLISPROGRAMNVPROC glIsProgramNV;
PFNGLLOADPROGRAMNVPROC glLoadProgramNV;
PFNGLPROGRAMPARAMETER4DNVPROC glProgramParameter4dNV;
PFNGLPROGRAMPARAMETER4DVNVPROC glProgramParameter4dvNV;
PFNGLPROGRAMPARAMETER4FNVPROC glProgramParameter4fNV;
PFNGLPROGRAMPARAMETER4FVNVPROC glProgramParameter4fvNV;
PFNGLPROGRAMPARAMETERS4DVNVPROC glProgramParameters4dvNV;
PFNGLPROGRAMPARAMETERS4FVNVPROC glProgramParameters4fvNV;
PFNGLREQUESTRESIDENTPROGRAMSNVPROC glRequestResidentProgramsNV;
PFNGLTRACKMATRIXNVPROC glTrackMatrixNV;
PFNGLVERTEXATTRIBPOINTERNVPROC glVertexAttribPointerNV;
PFNGLVERTEXATTRIB1DNVPROC glVertexAttrib1dNV;
PFNGLVERTEXATTRIB1DVNVPROC glVertexAttrib1dvNV;
PFNGLVERTEXATTRIB1FNVPROC glVertexAttrib1fNV;
PFNGLVERTEXATTRIB1FVNVPROC glVertexAttrib1fvNV;
PFNGLVERTEXATTRIB1SNVPROC glVertexAttrib1sNV;
PFNGLVERTEXATTRIB1SVNVPROC glVertexAttrib1svNV;
PFNGLVERTEXATTRIB2DNVPROC glVertexAttrib2dNV;
PFNGLVERTEXATTRIB2DVNVPROC glVertexAttrib2dvNV;
PFNGLVERTEXATTRIB2FNVPROC glVertexAttrib2fNV;
PFNGLVERTEXATTRIB2FVNVPROC glVertexAttrib2fvNV;
PFNGLVERTEXATTRIB2SNVPROC glVertexAttrib2sNV;
PFNGLVERTEXATTRIB2SVNVPROC glVertexAttrib2svNV;
PFNGLVERTEXATTRIB3DNVPROC glVertexAttrib3dNV;
PFNGLVERTEXATTRIB3DVNVPROC glVertexAttrib3dvNV;
PFNGLVERTEXATTRIB3FNVPROC glVertexAttrib3fNV;
PFNGLVERTEXATTRIB3FVNVPROC glVertexAttrib3fvNV;
PFNGLVERTEXATTRIB3SNVPROC glVertexAttrib3sNV;
PFNGLVERTEXATTRIB3SVNVPROC glVertexAttrib3svNV;
PFNGLVERTEXATTRIB4DNVPROC glVertexAttrib4dNV;
PFNGLVERTEXATTRIB4DVNVPROC glVertexAttrib4dvNV;
PFNGLVERTEXATTRIB4FNVPROC glVertexAttrib4fNV;
PFNGLVERTEXATTRIB4FVNVPROC glVertexAttrib4fvNV;
PFNGLVERTEXATTRIB4SNVPROC glVertexAttrib4sNV;
PFNGLVERTEXATTRIB4SVNVPROC glVertexAttrib4svNV;
PFNGLVERTEXATTRIB4UBNVPROC glVertexAttrib4ubNV;
PFNGLVERTEXATTRIB4UBVNVPROC glVertexAttrib4ubvNV;
PFNGLVERTEXATTRIBS1DVNVPROC glVertexAttribs1dvNV;
PFNGLVERTEXATTRIBS1FVNVPROC glVertexAttribs1fvNV;
PFNGLVERTEXATTRIBS1SVNVPROC glVertexAttribs1svNV;
PFNGLVERTEXATTRIBS2DVNVPROC glVertexAttribs2dvNV;
PFNGLVERTEXATTRIBS2FVNVPROC glVertexAttribs2fvNV;
PFNGLVERTEXATTRIBS2SVNVPROC glVertexAttribs2svNV;
PFNGLVERTEXATTRIBS3DVNVPROC glVertexAttribs3dvNV;
PFNGLVERTEXATTRIBS3FVNVPROC glVertexAttribs3fvNV;
PFNGLVERTEXATTRIBS3SVNVPROC glVertexAttribs3svNV;
PFNGLVERTEXATTRIBS4DVNVPROC glVertexAttribs4dvNV;
PFNGLVERTEXATTRIBS4FVNVPROC glVertexAttribs4fvNV;
PFNGLVERTEXATTRIBS4SVNVPROC glVertexAttribs4svNV;
PFNGLVERTEXATTRIBS4UBVNVPROC glVertexAttribs4ubvNV;
#endif

#if defined(GL_SGIX_texture_coordinate_clamp)
#endif

#if defined(GL_OML_interlace)
#endif

#if defined(GL_OML_subsample)
#endif

#if defined(GL_OML_resample)
#endif

#if defined(GL_NV_copy_depth_to_color)
#endif

#if defined(GL_ATI_envmap_bumpmap)
PFNGLTEXBUMPPARAMETERIVATIPROC glTexBumpParameterivATI;
PFNGLTEXBUMPPARAMETERFVATIPROC glTexBumpParameterfvATI;
PFNGLGETTEXBUMPPARAMETERIVATIPROC glGetTexBumpParameterivATI;
PFNGLGETTEXBUMPPARAMETERFVATIPROC glGetTexBumpParameterfvATI;
#endif

#if defined(GL_ATI_fragment_shader)
PFNGLGENFRAGMENTSHADERSATIPROC glGenFragmentShadersATI;
PFNGLBINDFRAGMENTSHADERATIPROC glBindFragmentShaderATI;
PFNGLDELETEFRAGMENTSHADERATIPROC glDeleteFragmentShaderATI;
PFNGLBEGINFRAGMENTSHADERATIPROC glBeginFragmentShaderATI;
PFNGLENDFRAGMENTSHADERATIPROC glEndFragmentShaderATI;
PFNGLPASSTEXCOORDATIPROC glPassTexCoordATI;
PFNGLSAMPLEMAPATIPROC glSampleMapATI;
PFNGLCOLORFRAGMENTOP1ATIPROC glColorFragmentOp1ATI;
PFNGLCOLORFRAGMENTOP2ATIPROC glColorFragmentOp2ATI;
PFNGLCOLORFRAGMENTOP3ATIPROC glColorFragmentOp3ATI;
PFNGLALPHAFRAGMENTOP1ATIPROC glAlphaFragmentOp1ATI;
PFNGLALPHAFRAGMENTOP2ATIPROC glAlphaFragmentOp2ATI;
PFNGLALPHAFRAGMENTOP3ATIPROC glAlphaFragmentOp3ATI;
PFNGLSETFRAGMENTSHADERCONSTANTATIPROC glSetFragmentShaderConstantATI;
#endif

#if defined(GL_ATI_pn_triangles)
#endif

#if defined(GL_ATI_vertex_array_object) && 0
PFNGLNEWOBJECTBUFFERATIPROC glNewObjectBufferATI;
PFNGLISOBJECTBUFFERATIPROC glIsObjectBufferATI;
PFNGLUPDATEOBJECTBUFFERATIPROC glUpdateObjectBufferATI;
PFNGLGETOBJECTBUFFERFVATIPROC glGetObjectBufferfvATI;
PFNGLGETOBJECTBUFFERIVATIPROC glGetObjectBufferivATI;
/* glDeleteObjectBufferATI became glFreeObjectBufferATI in GL_ATI_vertex_array_object v1.1 */
PFNGLFREEOBJECTBUFFERATIPROC glFreeObjectBufferATI;
PFNGLARRAYOBJECTATIPROC glArrayObjectATI;
PFNGLGETARRAYOBJECTFVATIPROC glGetArrayObjectfvATI;
PFNGLGETARRAYOBJECTIVATIPROC glGetArrayObjectivATI;
PFNGLVARIANTARRAYOBJECTATIPROC glVariantArrayObjectATI;
PFNGLGETVARIANTARRAYOBJECTFVATIPROC glGetVariantArrayObjectfvATI;
PFNGLGETVARIANTARRAYOBJECTIVATIPROC glGetVariantArrayObjectivATI;
#endif

#if defined(GL_EXT_vertex_shader)
PFNGLBEGINVERTEXSHADEREXTPROC glBeginVertexShaderEXT;
PFNGLENDVERTEXSHADEREXTPROC glEndVertexShaderEXT;
PFNGLBINDVERTEXSHADEREXTPROC glBindVertexShaderEXT;
PFNGLGENVERTEXSHADERSEXTPROC glGenVertexShadersEXT;
PFNGLDELETEVERTEXSHADEREXTPROC glDeleteVertexShaderEXT;
PFNGLSHADEROP1EXTPROC glShaderOp1EXT;
PFNGLSHADEROP2EXTPROC glShaderOp2EXT;
PFNGLSHADEROP3EXTPROC glShaderOp3EXT;
PFNGLSWIZZLEEXTPROC glSwizzleEXT;
PFNGLWRITEMASKEXTPROC glWriteMaskEXT;
PFNGLINSERTCOMPONENTEXTPROC glInsertComponentEXT;
PFNGLEXTRACTCOMPONENTEXTPROC glExtractComponentEXT;
PFNGLGENSYMBOLSEXTPROC glGenSymbolsEXT;
PFNGLSETINVARIANTEXTPROC glSetInvariantEXT;
PFNGLSETLOCALCONSTANTEXTPROC glSetLocalConstantEXT;
PFNGLVARIANTBVEXTPROC glVariantbvEXT;
PFNGLVARIANTSVEXTPROC glVariantsvEXT;
PFNGLVARIANTIVEXTPROC glVariantivEXT;
PFNGLVARIANTFVEXTPROC glVariantfvEXT;
PFNGLVARIANTDVEXTPROC glVariantdvEXT;
PFNGLVARIANTUBVEXTPROC glVariantubvEXT;
PFNGLVARIANTUSVEXTPROC glVariantusvEXT;
PFNGLVARIANTUIVEXTPROC glVariantuivEXT;
PFNGLVARIANTPOINTEREXTPROC glVariantPointerEXT;
PFNGLENABLEVARIANTCLIENTSTATEEXTPROC glEnableVariantClientStateEXT;
PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC glDisableVariantClientStateEXT;
PFNGLBINDLIGHTPARAMETEREXTPROC glBindLightParameterEXT;
PFNGLBINDMATERIALPARAMETEREXTPROC glBindMaterialParameterEXT;
PFNGLBINDTEXGENPARAMETEREXTPROC glBindTexGenParameterEXT;
PFNGLBINDTEXTUREUNITPARAMETEREXTPROC glBindTextureUnitParameterEXT;
PFNGLBINDPARAMETEREXTPROC glBindParameterEXT;
PFNGLISVARIANTENABLEDEXTPROC glIsVariantEnabledEXT;
PFNGLGETVARIANTBOOLEANVEXTPROC glGetVariantBooleanvEXT;
PFNGLGETVARIANTINTEGERVEXTPROC glGetVariantIntegervEXT;
PFNGLGETVARIANTFLOATVEXTPROC glGetVariantFloatvEXT;
PFNGLGETVARIANTPOINTERVEXTPROC glGetVariantPointervEXT;
PFNGLGETINVARIANTBOOLEANVEXTPROC glGetInvariantBooleanvEXT;
PFNGLGETINVARIANTINTEGERVEXTPROC glGetInvariantIntegervEXT;
PFNGLGETINVARIANTFLOATVEXTPROC glGetInvariantFloatvEXT;
PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC glGetLocalConstantBooleanvEXT;
PFNGLGETLOCALCONSTANTINTEGERVEXTPROC glGetLocalConstantIntegervEXT;
PFNGLGETLOCALCONSTANTFLOATVEXTPROC glGetLocalConstantFloatvEXT;
#endif

#if defined(GL_ATI_vertex_streams)
PFNGLVERTEXSTREAM1SATIPROC glVertexStream1sATI;
PFNGLVERTEXSTREAM1SVATIPROC glVertexStream1svATI;
PFNGLVERTEXSTREAM1IATIPROC glVertexStream1iATI;
PFNGLVERTEXSTREAM1IVATIPROC glVertexStream1ivATI;
PFNGLVERTEXSTREAM1FATIPROC glVertexStream1fATI;
PFNGLVERTEXSTREAM1FVATIPROC glVertexStream1fvATI;
PFNGLVERTEXSTREAM1DATIPROC glVertexStream1dATI;
PFNGLVERTEXSTREAM1DVATIPROC glVertexStream1dvATI;
PFNGLVERTEXSTREAM2SATIPROC glVertexStream2sATI;
PFNGLVERTEXSTREAM2SVATIPROC glVertexStream2svATI;
PFNGLVERTEXSTREAM2IATIPROC glVertexStream2iATI;
PFNGLVERTEXSTREAM2IVATIPROC glVertexStream2ivATI;
PFNGLVERTEXSTREAM2FATIPROC glVertexStream2fATI;
PFNGLVERTEXSTREAM2FVATIPROC glVertexStream2fvATI;
PFNGLVERTEXSTREAM2DATIPROC glVertexStream2dATI;
PFNGLVERTEXSTREAM2DVATIPROC glVertexStream2dvATI;
PFNGLVERTEXSTREAM3SATIPROC glVertexStream3sATI;
PFNGLVERTEXSTREAM3SVATIPROC glVertexStream3svATI;
PFNGLVERTEXSTREAM3IATIPROC glVertexStream3iATI;
PFNGLVERTEXSTREAM3IVATIPROC glVertexStream3ivATI;
PFNGLVERTEXSTREAM3FATIPROC glVertexStream3fATI;
PFNGLVERTEXSTREAM3FVATIPROC glVertexStream3fvATI;
PFNGLVERTEXSTREAM3DATIPROC glVertexStream3dATI;
PFNGLVERTEXSTREAM3DVATIPROC glVertexStream3dvATI;
PFNGLVERTEXSTREAM4SATIPROC glVertexStream4sATI;
PFNGLVERTEXSTREAM4SVATIPROC glVertexStream4svATI;
PFNGLVERTEXSTREAM4IATIPROC glVertexStream4iATI;
PFNGLVERTEXSTREAM4IVATIPROC glVertexStream4ivATI;
PFNGLVERTEXSTREAM4FATIPROC glVertexStream4fATI;
PFNGLVERTEXSTREAM4FVATIPROC glVertexStream4fvATI;
PFNGLVERTEXSTREAM4DATIPROC glVertexStream4dATI;
PFNGLVERTEXSTREAM4DVATIPROC glVertexStream4dvATI;
PFNGLNORMALSTREAM3BATIPROC glNormalStream3bATI;
PFNGLNORMALSTREAM3BVATIPROC glNormalStream3bvATI;
PFNGLNORMALSTREAM3SATIPROC glNormalStream3sATI;
PFNGLNORMALSTREAM3SVATIPROC glNormalStream3svATI;
PFNGLNORMALSTREAM3IATIPROC glNormalStream3iATI;
PFNGLNORMALSTREAM3IVATIPROC glNormalStream3ivATI;
PFNGLNORMALSTREAM3FATIPROC glNormalStream3fATI;
PFNGLNORMALSTREAM3FVATIPROC glNormalStream3fvATI;
PFNGLNORMALSTREAM3DATIPROC glNormalStream3dATI;
PFNGLNORMALSTREAM3DVATIPROC glNormalStream3dvATI;
PFNGLCLIENTACTIVEVERTEXSTREAMATIPROC glClientActiveVertexStreamATI;
PFNGLVERTEXBLENDENVIATIPROC glVertexBlendEnviATI;
PFNGLVERTEXBLENDENVFATIPROC glVertexBlendEnvfATI;
#endif

#if defined(GL_ATI_element_array)
PFNGLELEMENTPOINTERATIPROC glElementPointerATI;
PFNGLDRAWELEMENTARRAYATIPROC glDrawElementArrayATI;
PFNGLDRAWRANGEELEMENTARRAYATIPROC glDrawRangeElementArrayATI;
#endif

#if defined(GL_SUN_mesh_array)
PFNGLDRAWMESHARRAYSSUNPROC glDrawMeshArraysSUN;
#endif

#if defined(GL_SUN_slice_accum)
#endif

#if defined(GL_NV_multisample_filter_hint)
#endif

#if defined(GL_NV_depth_clamp)
#endif

#if defined(GL_NV_occlusion_query)
PFNGLGENOCCLUSIONQUERIESNVPROC glGenOcclusionQueriesNV;
PFNGLDELETEOCCLUSIONQUERIESNVPROC glDeleteOcclusionQueriesNV;
PFNGLISOCCLUSIONQUERYNVPROC glIsOcclusionQueryNV;
PFNGLBEGINOCCLUSIONQUERYNVPROC glBeginOcclusionQueryNV;
PFNGLENDOCCLUSIONQUERYNVPROC glEndOcclusionQueryNV;
PFNGLGETOCCLUSIONQUERYIVNVPROC glGetOcclusionQueryivNV;
PFNGLGETOCCLUSIONQUERYUIVNVPROC glGetOcclusionQueryuivNV;
#endif

#if defined(GL_NV_point_sprite)
PFNGLPOINTPARAMETERINVPROC glPointParameteriNV;
PFNGLPOINTPARAMETERIVNVPROC glPointParameterivNV;
#endif

#if defined(GL_NV_texture_shader3)
#endif

#if defined(GL_NV_vertex_program1_1)
#endif

#if defined(GL_EXT_shadow_funcs)
#endif

#if defined(GL_EXT_stencil_two_side)
PFNGLACTIVESTENCILFACEEXTPROC glActiveStencilFaceEXT;
#endif

#if defined(GL_ATI_text_fragment_shader)
#endif

#if defined(GL_APPLE_client_storage)
#endif

#if defined(GL_APPLE_element_array)
PFNGLELEMENTPOINTERAPPLEPROC glElementPointerAPPLE;
PFNGLDRAWELEMENTARRAYAPPLEPROC glDrawElementArrayAPPLE;
PFNGLDRAWRANGEELEMENTARRAYAPPLEPROC glDrawRangeElementArrayAPPLE;
PFNGLMULTIDRAWELEMENTARRAYAPPLEPROC glMultiDrawElementArrayAPPLE;
PFNGLMULTIDRAWRANGEELEMENTARRAYAPPLEPROC glMultiDrawRangeElementArrayAPPLE;
#endif

#if defined(GL_APPLE_fence)
PFNGLGENFENCESAPPLEPROC glGenFencesAPPLE;
PFNGLDELETEFENCESAPPLEPROC glDeleteFencesAPPLE;
PFNGLSETFENCEAPPLEPROC glSetFenceAPPLE;
PFNGLISFENCEAPPLEPROC glIsFenceAPPLE;
PFNGLTESTFENCEAPPLEPROC glTestFenceAPPLE;
PFNGLFINISHFENCEAPPLEPROC glFinishFenceAPPLE;
PFNGLTESTOBJECTAPPLEPROC glTestObjectAPPLE;
PFNGLFINISHOBJECTAPPLEPROC glFinishObjectAPPLE;
#endif

#if defined(GL_APPLE_vertex_array_object)
PFNGLBINDVERTEXARRAYAPPLEPROC glBindVertexArrayAPPLE;
PFNGLDELETEVERTEXARRAYSAPPLEPROC glDeleteVertexArraysAPPLE;
PFNGLGENVERTEXARRAYSAPPLEPROC glGenVertexArraysAPPLE;
PFNGLISVERTEXARRAYAPPLEPROC glIsVertexArrayAPPLE;
#endif

#if defined(GL_APPLE_vertex_array_range)
PFNGLVERTEXARRAYRANGEAPPLEPROC glVertexArrayRangeAPPLE;
PFNGLFLUSHVERTEXARRAYRANGEAPPLEPROC glFlushVertexArrayRangeAPPLE;
PFNGLVERTEXARRAYPARAMETERIAPPLEPROC glVertexArrayParameteriAPPLE;
#endif

#if defined(GL_APPLE_ycbcr_422)
#endif

#if defined(GL_S3_s3tc)
#endif

#if defined(GL_ATI_draw_buffers)
PFNGLDRAWBUFFERSATIPROC glDrawBuffersATI;
#endif

#if defined(GL_ATI_texture_env_combine3)
#endif

#if defined(GL_ATI_texture_float)
#endif

#if defined(GL_NV_float_buffer)
#endif

#if defined(GL_NV_fragment_program)
PFNGLPROGRAMNAMEDPARAMETER4FNVPROC glProgramNamedParameter4fNV;
PFNGLPROGRAMNAMEDPARAMETER4DNVPROC glProgramNamedParameter4dNV;
PFNGLPROGRAMNAMEDPARAMETER4FVNVPROC glProgramNamedParameter4fvNV;
PFNGLPROGRAMNAMEDPARAMETER4DVNVPROC glProgramNamedParameter4dvNV;
PFNGLGETPROGRAMNAMEDPARAMETERFVNVPROC glGetProgramNamedParameterfvNV;
PFNGLGETPROGRAMNAMEDPARAMETERDVNVPROC glGetProgramNamedParameterdvNV;
#endif

#if defined(GL_NV_half_float)
PFNGLVERTEX2HNVPROC glVertex2hNV;
PFNGLVERTEX2HVNVPROC glVertex2hvNV;
PFNGLVERTEX3HNVPROC glVertex3hNV;
PFNGLVERTEX3HVNVPROC glVertex3hvNV;
PFNGLVERTEX4HNVPROC glVertex4hNV;
PFNGLVERTEX4HVNVPROC glVertex4hvNV;
PFNGLNORMAL3HNVPROC glNormal3hNV;
PFNGLNORMAL3HVNVPROC glNormal3hvNV;
PFNGLCOLOR3HNVPROC glColor3hNV;
PFNGLCOLOR3HVNVPROC glColor3hvNV;
PFNGLCOLOR4HNVPROC glColor4hNV;
PFNGLCOLOR4HVNVPROC glColor4hvNV;
PFNGLTEXCOORD1HNVPROC glTexCoord1hNV;
PFNGLTEXCOORD1HVNVPROC glTexCoord1hvNV;
PFNGLTEXCOORD2HNVPROC glTexCoord2hNV;
PFNGLTEXCOORD2HVNVPROC glTexCoord2hvNV;
PFNGLTEXCOORD3HNVPROC glTexCoord3hNV;
PFNGLTEXCOORD3HVNVPROC glTexCoord3hvNV;
PFNGLTEXCOORD4HNVPROC glTexCoord4hNV;
PFNGLTEXCOORD4HVNVPROC glTexCoord4hvNV;
PFNGLMULTITEXCOORD1HNVPROC glMultiTexCoord1hNV;
PFNGLMULTITEXCOORD1HVNVPROC glMultiTexCoord1hvNV;
PFNGLMULTITEXCOORD2HNVPROC glMultiTexCoord2hNV;
PFNGLMULTITEXCOORD2HVNVPROC glMultiTexCoord2hvNV;
PFNGLMULTITEXCOORD3HNVPROC glMultiTexCoord3hNV;
PFNGLMULTITEXCOORD3HVNVPROC glMultiTexCoord3hvNV;
PFNGLMULTITEXCOORD4HNVPROC glMultiTexCoord4hNV;
PFNGLMULTITEXCOORD4HVNVPROC glMultiTexCoord4hvNV;
PFNGLFOGCOORDHNVPROC glFogCoordhNV;
PFNGLFOGCOORDHVNVPROC glFogCoordhvNV;
PFNGLSECONDARYCOLOR3HNVPROC glSecondaryColor3hNV;
PFNGLSECONDARYCOLOR3HVNVPROC glSecondaryColor3hvNV;
PFNGLVERTEXWEIGHTHNVPROC glVertexWeighthNV;
PFNGLVERTEXWEIGHTHVNVPROC glVertexWeighthvNV;
PFNGLVERTEXATTRIB1HNVPROC glVertexAttrib1hNV;
PFNGLVERTEXATTRIB1HVNVPROC glVertexAttrib1hvNV;
PFNGLVERTEXATTRIB2HNVPROC glVertexAttrib2hNV;
PFNGLVERTEXATTRIB2HVNVPROC glVertexAttrib2hvNV;
PFNGLVERTEXATTRIB3HNVPROC glVertexAttrib3hNV;
PFNGLVERTEXATTRIB3HVNVPROC glVertexAttrib3hvNV;
PFNGLVERTEXATTRIB4HNVPROC glVertexAttrib4hNV;
PFNGLVERTEXATTRIB4HVNVPROC glVertexAttrib4hvNV;
PFNGLVERTEXATTRIBS1HVNVPROC glVertexAttribs1hvNV;
PFNGLVERTEXATTRIBS2HVNVPROC glVertexAttribs2hvNV;
PFNGLVERTEXATTRIBS3HVNVPROC glVertexAttribs3hvNV;
PFNGLVERTEXATTRIBS4HVNVPROC glVertexAttribs4hvNV;
#endif

#if defined(GL_NV_pixel_data_range)
PFNGLPIXELDATARANGENVPROC glPixelDataRangeNV;
PFNGLFLUSHPIXELDATARANGENVPROC glFlushPixelDataRangeNV;
#endif

#if defined(GL_NV_primitive_restart)
PFNGLPRIMITIVERESTARTNVPROC glPrimitiveRestartNV;
PFNGLPRIMITIVERESTARTINDEXNVPROC glPrimitiveRestartIndexNV;
#endif

#if defined(GL_NV_texture_expand_normal)
#endif

#if defined(GL_NV_vertex_program2)
#endif

#if defined(GL_ATI_map_object_buffer)
PFNGLMAPOBJECTBUFFERATIPROC glMapObjectBufferATI;
PFNGLUNMAPOBJECTBUFFERATIPROC glUnmapObjectBufferATI;
#endif

#if defined(GL_ATI_separate_stencil)
PFNGLSTENCILOPSEPARATEATIPROC glStencilOpSeparateATI;
PFNGLSTENCILFUNCSEPARATEATIPROC glStencilFuncSeparateATI;
#endif

#if defined(GL_ATI_vertex_attrib_array_object)
PFNGLVERTEXATTRIBARRAYOBJECTATIPROC glVertexAttribArrayObjectATI;
PFNGLGETVERTEXATTRIBARRAYOBJECTFVATIPROC glGetVertexAttribArrayObjectfvATI;
PFNGLGETVERTEXATTRIBARRAYOBJECTIVATIPROC glGetVertexAttribArrayObjectivATI;
#endif

#if defined(GL_EXT_depth_bounds_test)
PFNGLDEPTHBOUNDSEXTPROC glDepthBoundsEXT;
#endif

#if defined(GL_EXT_texture_mirror_clamp)
#endif

#if defined(GL_EXT_blend_equation_separate)
PFNGLBLENDEQUATIONSEPARATEEXTPROC glBlendEquationSeparateEXT;
#endif

#if defined(GL_MESA_pack_invert)
#endif

#if defined(GL_MESA_ycbcr_texture)
#endif

/* End mkglext.py */
};

using namespace RAS_GL;

/*******************************************************************************
2. Query extension functions here

Need to #ifdef (compile time test for extension)
Use QueryExtension("GL_EXT_name") to test at runtime.
Use bglGetProcAddress to find entry point
Use EnableExtension(_GL_EXT_...) to allow Blender to use the extension.

 ******************************************************************************/
void RAS_GLExtensionManager::LinkExtensions()
{
	static bool doDebugMessages = true;
	extensions = STR_String((const char *) glGetString(GL_EXTENSIONS)).Explode(' ');

	/* Generated from mkglext.py */

#ifdef GL_EXT_compiled_vertex_array
	if (QueryExtension("GL_EXT_compiled_vertex_array"))
	{
		glUnlockArraysEXT = reinterpret_cast<PFNGLUNLOCKARRAYSEXTPROC>(bglGetProcAddress((const GLubyte *) "glUnlockArraysEXT"));
		glLockArraysEXT = reinterpret_cast<PFNGLLOCKARRAYSEXTPROC>(bglGetProcAddress((const GLubyte *) "glLockArraysEXT"));
		if (glUnlockArraysEXT && glLockArraysEXT)
		{
			EnableExtension(_GL_EXT_compiled_vertex_array);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_compiled_vertex_array" << std::endl;
		} else {
			glUnlockArraysEXT = _unlockfunc;
			glLockArraysEXT = _lockfunc;
			std::cout << "ERROR: GL_EXT_compiled_vertex_array implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ARB_multitexture"))
	{
		EnableExtension(_GL_ARB_multitexture);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_multitexture" << std::endl;
	}

#if defined(GL_ARB_transpose_matrix)
	if (QueryExtension("GL_ARB_transpose_matrix"))
	{
		glLoadTransposeMatrixfARB = reinterpret_cast<PFNGLLOADTRANSPOSEMATRIXFARBPROC>(bglGetProcAddress((const GLubyte *) "glLoadTransposeMatrixfARB"));
		glLoadTransposeMatrixdARB = reinterpret_cast<PFNGLLOADTRANSPOSEMATRIXDARBPROC>(bglGetProcAddress((const GLubyte *) "glLoadTransposeMatrixdARB"));
		glMultTransposeMatrixfARB = reinterpret_cast<PFNGLMULTTRANSPOSEMATRIXFARBPROC>(bglGetProcAddress((const GLubyte *) "glMultTransposeMatrixfARB"));
		glMultTransposeMatrixdARB = reinterpret_cast<PFNGLMULTTRANSPOSEMATRIXDARBPROC>(bglGetProcAddress((const GLubyte *) "glMultTransposeMatrixdARB"));
		if (glLoadTransposeMatrixfARB && glLoadTransposeMatrixdARB && glMultTransposeMatrixfARB && glMultTransposeMatrixdARB) {
			EnableExtension(_GL_ARB_transpose_matrix);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_transpose_matrix" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_transpose_matrix implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ARB_multisample)
	if (QueryExtension("GL_ARB_multisample"))
	{
		glSampleCoverageARB = reinterpret_cast<PFNGLSAMPLECOVERAGEARBPROC>(bglGetProcAddress((const GLubyte *) "glSampleCoverageARB"));
		if (glSampleCoverageARB) {
			EnableExtension(_GL_ARB_multisample);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_multisample" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_multisample implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ARB_texture_env_add"))
	{
		EnableExtension(_GL_ARB_texture_env_add);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_texture_env_add" << std::endl;
	}

	if (QueryExtension("GL_ARB_texture_cube_map"))
	{
		EnableExtension(_GL_ARB_texture_cube_map);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_texture_cube_map" << std::endl;
	}

#if defined(GL_ARB_texture_compression)
	if (QueryExtension("GL_ARB_texture_compression"))
	{
		glCompressedTexImage3DARB = reinterpret_cast<PFNGLCOMPRESSEDTEXIMAGE3DARBPROC>(bglGetProcAddress((const GLubyte *) "glCompressedTexImage3DARB"));
		glCompressedTexImage2DARB = reinterpret_cast<PFNGLCOMPRESSEDTEXIMAGE2DARBPROC>(bglGetProcAddress((const GLubyte *) "glCompressedTexImage2DARB"));
		glCompressedTexImage1DARB = reinterpret_cast<PFNGLCOMPRESSEDTEXIMAGE1DARBPROC>(bglGetProcAddress((const GLubyte *) "glCompressedTexImage1DARB"));
		glCompressedTexSubImage3DARB = reinterpret_cast<PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC>(bglGetProcAddress((const GLubyte *) "glCompressedTexSubImage3DARB"));
		glCompressedTexSubImage2DARB = reinterpret_cast<PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC>(bglGetProcAddress((const GLubyte *) "glCompressedTexSubImage2DARB"));
		glCompressedTexSubImage1DARB = reinterpret_cast<PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC>(bglGetProcAddress((const GLubyte *) "glCompressedTexSubImage1DARB"));
		glGetCompressedTexImageARB = reinterpret_cast<PFNGLGETCOMPRESSEDTEXIMAGEARBPROC>(bglGetProcAddress((const GLubyte *) "glGetCompressedTexImageARB"));
		if (glCompressedTexImage3DARB && glCompressedTexImage2DARB && glCompressedTexImage1DARB && glCompressedTexSubImage3DARB && glCompressedTexSubImage2DARB && glCompressedTexSubImage1DARB && glGetCompressedTexImageARB) {
			EnableExtension(_GL_ARB_texture_compression);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_texture_compression" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_texture_compression implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ARB_texture_border_clamp"))
	{
		EnableExtension(_GL_ARB_texture_border_clamp);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_texture_border_clamp" << std::endl;
	}

#if defined(GL_ARB_vertex_blend)
	if (QueryExtension("GL_ARB_vertex_blend"))
	{
		glWeightbvARB = reinterpret_cast<PFNGLWEIGHTBVARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightbvARB"));
		glWeightsvARB = reinterpret_cast<PFNGLWEIGHTSVARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightsvARB"));
		glWeightivARB = reinterpret_cast<PFNGLWEIGHTIVARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightivARB"));
		glWeightfvARB = reinterpret_cast<PFNGLWEIGHTFVARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightfvARB"));
		glWeightdvARB = reinterpret_cast<PFNGLWEIGHTDVARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightdvARB"));
		glWeightubvARB = reinterpret_cast<PFNGLWEIGHTUBVARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightubvARB"));
		glWeightusvARB = reinterpret_cast<PFNGLWEIGHTUSVARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightusvARB"));
		glWeightuivARB = reinterpret_cast<PFNGLWEIGHTUIVARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightuivARB"));
		glWeightPointerARB = reinterpret_cast<PFNGLWEIGHTPOINTERARBPROC>(bglGetProcAddress((const GLubyte *) "glWeightPointerARB"));
		glVertexBlendARB = reinterpret_cast<PFNGLVERTEXBLENDARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexBlendARB"));
		if (glWeightbvARB && glWeightsvARB && glWeightivARB && glWeightfvARB && glWeightdvARB && glWeightubvARB && glWeightusvARB && glWeightuivARB && glWeightPointerARB && glVertexBlendARB) {
			EnableExtension(_GL_ARB_vertex_blend);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_vertex_blend" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_vertex_blend implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ARB_matrix_palette)
	if (QueryExtension("GL_ARB_matrix_palette"))
	{
		glCurrentPaletteMatrixARB = reinterpret_cast<PFNGLCURRENTPALETTEMATRIXARBPROC>(bglGetProcAddress((const GLubyte *) "glCurrentPaletteMatrixARB"));
		glMatrixIndexubvARB = reinterpret_cast<PFNGLMATRIXINDEXUBVARBPROC>(bglGetProcAddress((const GLubyte *) "glMatrixIndexubvARB"));
		glMatrixIndexusvARB = reinterpret_cast<PFNGLMATRIXINDEXUSVARBPROC>(bglGetProcAddress((const GLubyte *) "glMatrixIndexusvARB"));
		glMatrixIndexuivARB = reinterpret_cast<PFNGLMATRIXINDEXUIVARBPROC>(bglGetProcAddress((const GLubyte *) "glMatrixIndexuivARB"));
		glMatrixIndexPointerARB = reinterpret_cast<PFNGLMATRIXINDEXPOINTERARBPROC>(bglGetProcAddress((const GLubyte *) "glMatrixIndexPointerARB"));
		if (glCurrentPaletteMatrixARB && glMatrixIndexubvARB && glMatrixIndexusvARB && glMatrixIndexuivARB && glMatrixIndexPointerARB) {
			EnableExtension(_GL_ARB_matrix_palette);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_matrix_palette" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_matrix_palette implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ARB_texture_env_combine"))
	{
		EnableExtension(_GL_ARB_texture_env_combine);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_texture_env_combine" << std::endl;
	}

	if (QueryExtension("GL_ARB_texture_env_crossbar"))
	{
		EnableExtension(_GL_ARB_texture_env_crossbar);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_texture_env_crossbar" << std::endl;
	}

	if (QueryExtension("GL_ARB_texture_env_dot3"))
	{
		EnableExtension(_GL_ARB_texture_env_dot3);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_texture_env_dot3" << std::endl;
	}

	if (QueryExtension("GL_ARB_texture_mirrored_repeat"))
	{
		EnableExtension(_GL_ARB_texture_mirrored_repeat);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_texture_mirrored_repeat" << std::endl;
	}

	if (QueryExtension("GL_ARB_depth_texture"))
	{
		EnableExtension(_GL_ARB_depth_texture);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_depth_texture" << std::endl;
	}

	if (QueryExtension("GL_ARB_shadow"))
	{
		EnableExtension(_GL_ARB_shadow);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_shadow" << std::endl;
	}

	if (QueryExtension("GL_ARB_shadow_ambient"))
	{
		EnableExtension(_GL_ARB_shadow_ambient);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_shadow_ambient" << std::endl;
	}

#if defined(GL_ARB_window_pos)
	if (QueryExtension("GL_ARB_window_pos"))
	{
		glWindowPos2dARB = reinterpret_cast<PFNGLWINDOWPOS2DARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2dARB"));
		glWindowPos2dvARB = reinterpret_cast<PFNGLWINDOWPOS2DVARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2dvARB"));
		glWindowPos2fARB = reinterpret_cast<PFNGLWINDOWPOS2FARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2fARB"));
		glWindowPos2fvARB = reinterpret_cast<PFNGLWINDOWPOS2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2fvARB"));
		glWindowPos2iARB = reinterpret_cast<PFNGLWINDOWPOS2IARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2iARB"));
		glWindowPos2ivARB = reinterpret_cast<PFNGLWINDOWPOS2IVARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2ivARB"));
		glWindowPos2sARB = reinterpret_cast<PFNGLWINDOWPOS2SARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2sARB"));
		glWindowPos2svARB = reinterpret_cast<PFNGLWINDOWPOS2SVARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2svARB"));
		glWindowPos3dARB = reinterpret_cast<PFNGLWINDOWPOS3DARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3dARB"));
		glWindowPos3dvARB = reinterpret_cast<PFNGLWINDOWPOS3DVARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3dvARB"));
		glWindowPos3fARB = reinterpret_cast<PFNGLWINDOWPOS3FARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3fARB"));
		glWindowPos3fvARB = reinterpret_cast<PFNGLWINDOWPOS3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3fvARB"));
		glWindowPos3iARB = reinterpret_cast<PFNGLWINDOWPOS3IARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3iARB"));
		glWindowPos3ivARB = reinterpret_cast<PFNGLWINDOWPOS3IVARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3ivARB"));
		glWindowPos3sARB = reinterpret_cast<PFNGLWINDOWPOS3SARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3sARB"));
		glWindowPos3svARB = reinterpret_cast<PFNGLWINDOWPOS3SVARBPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3svARB"));
		if (glWindowPos2dARB && glWindowPos2dvARB && glWindowPos2fARB && glWindowPos2fvARB && glWindowPos2iARB && glWindowPos2ivARB && glWindowPos2sARB && glWindowPos2svARB && glWindowPos3dARB && glWindowPos3dvARB && glWindowPos3fARB && glWindowPos3fvARB && glWindowPos3iARB && glWindowPos3ivARB && glWindowPos3sARB && glWindowPos3svARB) {
			EnableExtension(_GL_ARB_window_pos);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_window_pos" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_window_pos implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ARB_vertex_program)
	if (QueryExtension("GL_ARB_vertex_program"))
	{
		glVertexAttrib1dARB = reinterpret_cast<PFNGLVERTEXATTRIB1DARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1dARB"));
		glVertexAttrib1dvARB = reinterpret_cast<PFNGLVERTEXATTRIB1DVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1dvARB"));
		glVertexAttrib1fARB = reinterpret_cast<PFNGLVERTEXATTRIB1FARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1fARB"));
		glVertexAttrib1fvARB = reinterpret_cast<PFNGLVERTEXATTRIB1FVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1fvARB"));
		glVertexAttrib1sARB = reinterpret_cast<PFNGLVERTEXATTRIB1SARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1sARB"));
		glVertexAttrib1svARB = reinterpret_cast<PFNGLVERTEXATTRIB1SVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1svARB"));
		glVertexAttrib2dARB = reinterpret_cast<PFNGLVERTEXATTRIB2DARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2dARB"));
		glVertexAttrib2dvARB = reinterpret_cast<PFNGLVERTEXATTRIB2DVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2dvARB"));
		glVertexAttrib2fARB = reinterpret_cast<PFNGLVERTEXATTRIB2FARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2fARB"));
		glVertexAttrib2fvARB = reinterpret_cast<PFNGLVERTEXATTRIB2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2fvARB"));
		glVertexAttrib2sARB = reinterpret_cast<PFNGLVERTEXATTRIB2SARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2sARB"));
		glVertexAttrib2svARB = reinterpret_cast<PFNGLVERTEXATTRIB2SVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2svARB"));
		glVertexAttrib3dARB = reinterpret_cast<PFNGLVERTEXATTRIB3DARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3dARB"));
		glVertexAttrib3dvARB = reinterpret_cast<PFNGLVERTEXATTRIB3DVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3dvARB"));
		glVertexAttrib3fARB = reinterpret_cast<PFNGLVERTEXATTRIB3FARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3fARB"));
		glVertexAttrib3fvARB = reinterpret_cast<PFNGLVERTEXATTRIB3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3fvARB"));
		glVertexAttrib3sARB = reinterpret_cast<PFNGLVERTEXATTRIB3SARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3sARB"));
		glVertexAttrib3svARB = reinterpret_cast<PFNGLVERTEXATTRIB3SVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3svARB"));
		glVertexAttrib4NbvARB = reinterpret_cast<PFNGLVERTEXATTRIB4NBVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4NbvARB"));
		glVertexAttrib4NivARB = reinterpret_cast<PFNGLVERTEXATTRIB4NIVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4NivARB"));
		glVertexAttrib4NsvARB = reinterpret_cast<PFNGLVERTEXATTRIB4NSVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4NsvARB"));
		glVertexAttrib4NubARB = reinterpret_cast<PFNGLVERTEXATTRIB4NUBARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4NubARB"));
		glVertexAttrib4NubvARB = reinterpret_cast<PFNGLVERTEXATTRIB4NUBVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4NubvARB"));
		glVertexAttrib4NuivARB = reinterpret_cast<PFNGLVERTEXATTRIB4NUIVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4NuivARB"));
		glVertexAttrib4NusvARB = reinterpret_cast<PFNGLVERTEXATTRIB4NUSVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4NusvARB"));
		glVertexAttrib4bvARB = reinterpret_cast<PFNGLVERTEXATTRIB4BVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4bvARB"));
		glVertexAttrib4dARB = reinterpret_cast<PFNGLVERTEXATTRIB4DARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4dARB"));
		glVertexAttrib4dvARB = reinterpret_cast<PFNGLVERTEXATTRIB4DVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4dvARB"));
		glVertexAttrib4fARB = reinterpret_cast<PFNGLVERTEXATTRIB4FARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4fARB"));
		glVertexAttrib4fvARB = reinterpret_cast<PFNGLVERTEXATTRIB4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4fvARB"));
		glVertexAttrib4ivARB = reinterpret_cast<PFNGLVERTEXATTRIB4IVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4ivARB"));
		glVertexAttrib4sARB = reinterpret_cast<PFNGLVERTEXATTRIB4SARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4sARB"));
		glVertexAttrib4svARB = reinterpret_cast<PFNGLVERTEXATTRIB4SVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4svARB"));
		glVertexAttrib4ubvARB = reinterpret_cast<PFNGLVERTEXATTRIB4UBVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4ubvARB"));
		glVertexAttrib4uivARB = reinterpret_cast<PFNGLVERTEXATTRIB4UIVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4uivARB"));
		glVertexAttrib4usvARB = reinterpret_cast<PFNGLVERTEXATTRIB4USVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4usvARB"));
		glVertexAttribPointerARB = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribPointerARB"));
		glEnableVertexAttribArrayARB = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYARBPROC>(bglGetProcAddress((const GLubyte *) "glEnableVertexAttribArrayARB"));
		glDisableVertexAttribArrayARB = reinterpret_cast<PFNGLDISABLEVERTEXATTRIBARRAYARBPROC>(bglGetProcAddress((const GLubyte *) "glDisableVertexAttribArrayARB"));
		glProgramStringARB = reinterpret_cast<PFNGLPROGRAMSTRINGARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramStringARB"));
		glBindProgramARB = reinterpret_cast<PFNGLBINDPROGRAMARBPROC>(bglGetProcAddress((const GLubyte *) "glBindProgramARB"));
		glDeleteProgramsARB = reinterpret_cast<PFNGLDELETEPROGRAMSARBPROC>(bglGetProcAddress((const GLubyte *) "glDeleteProgramsARB"));
		glGenProgramsARB = reinterpret_cast<PFNGLGENPROGRAMSARBPROC>(bglGetProcAddress((const GLubyte *) "glGenProgramsARB"));
		glProgramEnvParameter4dARB = reinterpret_cast<PFNGLPROGRAMENVPARAMETER4DARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramEnvParameter4dARB"));
		glProgramEnvParameter4dvARB = reinterpret_cast<PFNGLPROGRAMENVPARAMETER4DVARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramEnvParameter4dvARB"));
		glProgramEnvParameter4fARB = reinterpret_cast<PFNGLPROGRAMENVPARAMETER4FARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramEnvParameter4fARB"));
		glProgramEnvParameter4fvARB = reinterpret_cast<PFNGLPROGRAMENVPARAMETER4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramEnvParameter4fvARB"));
		glProgramLocalParameter4dARB = reinterpret_cast<PFNGLPROGRAMLOCALPARAMETER4DARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramLocalParameter4dARB"));
		glProgramLocalParameter4dvARB = reinterpret_cast<PFNGLPROGRAMLOCALPARAMETER4DVARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramLocalParameter4dvARB"));
		glProgramLocalParameter4fARB = reinterpret_cast<PFNGLPROGRAMLOCALPARAMETER4FARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramLocalParameter4fARB"));
		glProgramLocalParameter4fvARB = reinterpret_cast<PFNGLPROGRAMLOCALPARAMETER4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glProgramLocalParameter4fvARB"));
		glGetProgramEnvParameterdvARB = reinterpret_cast<PFNGLGETPROGRAMENVPARAMETERDVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramEnvParameterdvARB"));
		glGetProgramEnvParameterfvARB = reinterpret_cast<PFNGLGETPROGRAMENVPARAMETERFVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramEnvParameterfvARB"));
		glGetProgramLocalParameterdvARB = reinterpret_cast<PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramLocalParameterdvARB"));
		glGetProgramLocalParameterfvARB = reinterpret_cast<PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramLocalParameterfvARB"));
		glGetProgramivARB = reinterpret_cast<PFNGLGETPROGRAMIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramivARB"));
		glGetProgramStringARB = reinterpret_cast<PFNGLGETPROGRAMSTRINGARBPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramStringARB"));
		glGetVertexAttribdvARB = reinterpret_cast<PFNGLGETVERTEXATTRIBDVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribdvARB"));
		glGetVertexAttribfvARB = reinterpret_cast<PFNGLGETVERTEXATTRIBFVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribfvARB"));
		glGetVertexAttribivARB = reinterpret_cast<PFNGLGETVERTEXATTRIBIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribivARB"));
		glGetVertexAttribPointervARB = reinterpret_cast<PFNGLGETVERTEXATTRIBPOINTERVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribPointervARB"));
		glIsProgramARB = reinterpret_cast<PFNGLISPROGRAMARBPROC>(bglGetProcAddress((const GLubyte *) "glIsProgramARB"));
		if (glVertexAttrib1dARB && glVertexAttrib1dvARB && glVertexAttrib1fARB && glVertexAttrib1fvARB && glVertexAttrib1sARB && glVertexAttrib1svARB && glVertexAttrib2dARB && glVertexAttrib2dvARB && glVertexAttrib2fARB && glVertexAttrib2fvARB && glVertexAttrib2sARB && glVertexAttrib2svARB && glVertexAttrib3dARB && glVertexAttrib3dvARB && glVertexAttrib3fARB && glVertexAttrib3fvARB && glVertexAttrib3sARB && glVertexAttrib3svARB && glVertexAttrib4NbvARB && glVertexAttrib4NivARB && glVertexAttrib4NsvARB && glVertexAttrib4NubARB && glVertexAttrib4NubvARB && glVertexAttrib4NuivARB && glVertexAttrib4NusvARB && glVertexAttrib4bvARB && glVertexAttrib4dARB && glVertexAttrib4dvARB && glVertexAttrib4fARB && glVertexAttrib4fvARB && glVertexAttrib4ivARB && glVertexAttrib4sARB && glVertexAttrib4svARB && glVertexAttrib4ubvARB && glVertexAttrib4uivARB && glVertexAttrib4usvARB && glVertexAttribPointerARB && glEnableVertexAttribArrayARB && glDisableVertexAttribArrayARB && glProgramStringARB && glBindProgramARB && glDeleteProgramsARB && glGenProgramsARB && glProgramEnvParameter4dARB && glProgramEnvParameter4dvARB && glProgramEnvParameter4fARB && glProgramEnvParameter4fvARB && glProgramLocalParameter4dARB && glProgramLocalParameter4dvARB && glProgramLocalParameter4fARB && glProgramLocalParameter4fvARB && glGetProgramEnvParameterdvARB && glGetProgramEnvParameterfvARB && glGetProgramLocalParameterdvARB && glGetProgramLocalParameterfvARB && glGetProgramivARB && glGetProgramStringARB && glGetVertexAttribdvARB && glGetVertexAttribfvARB && glGetVertexAttribivARB && glGetVertexAttribPointervARB && glIsProgramARB) {
			EnableExtension(_GL_ARB_vertex_program);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_vertex_program" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_vertex_program implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ARB_fragment_program"))
	{
		EnableExtension(_GL_ARB_fragment_program);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_fragment_program" << std::endl;
	}

#if defined(GL_ARB_vertex_buffer_object)
	if (QueryExtension("GL_ARB_vertex_buffer_object"))
	{
		glBindBufferARB = reinterpret_cast<PFNGLBINDBUFFERARBPROC>(bglGetProcAddress((const GLubyte *) "glBindBufferARB"));
		glDeleteBuffersARB = reinterpret_cast<PFNGLDELETEBUFFERSARBPROC>(bglGetProcAddress((const GLubyte *) "glDeleteBuffersARB"));
		glGenBuffersARB = reinterpret_cast<PFNGLGENBUFFERSARBPROC>(bglGetProcAddress((const GLubyte *) "glGenBuffersARB"));
		glIsBufferARB = reinterpret_cast<PFNGLISBUFFERARBPROC>(bglGetProcAddress((const GLubyte *) "glIsBufferARB"));
		glBufferDataARB = reinterpret_cast<PFNGLBUFFERDATAARBPROC>(bglGetProcAddress((const GLubyte *) "glBufferDataARB"));
		glBufferSubDataARB = reinterpret_cast<PFNGLBUFFERSUBDATAARBPROC>(bglGetProcAddress((const GLubyte *) "glBufferSubDataARB"));
		glGetBufferSubDataARB = reinterpret_cast<PFNGLGETBUFFERSUBDATAARBPROC>(bglGetProcAddress((const GLubyte *) "glGetBufferSubDataARB"));
		glMapBufferARB = reinterpret_cast<PFNGLMAPBUFFERARBPROC>(bglGetProcAddress((const GLubyte *) "glMapBufferARB"));
		glUnmapBufferARB = reinterpret_cast<PFNGLUNMAPBUFFERARBPROC>(bglGetProcAddress((const GLubyte *) "glUnmapBufferARB"));
		glGetBufferParameterivARB = reinterpret_cast<PFNGLGETBUFFERPARAMETERIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetBufferParameterivARB"));
		glGetBufferPointervARB = reinterpret_cast<PFNGLGETBUFFERPOINTERVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetBufferPointervARB"));
		if (glBindBufferARB && glDeleteBuffersARB && glGenBuffersARB && glIsBufferARB && glBufferDataARB && glBufferSubDataARB && glGetBufferSubDataARB && glMapBufferARB && glUnmapBufferARB && glGetBufferParameterivARB && glGetBufferPointervARB) {
			EnableExtension(_GL_ARB_vertex_buffer_object);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_vertex_buffer_object" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_vertex_buffer_object implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ARB_occlusion_query)
	if (QueryExtension("GL_ARB_occlusion_query"))
	{
		glGenQueriesARB = reinterpret_cast<PFNGLGENQUERIESARBPROC>(bglGetProcAddress((const GLubyte *) "glGenQueriesARB"));
		glDeleteQueriesARB = reinterpret_cast<PFNGLDELETEQUERIESARBPROC>(bglGetProcAddress((const GLubyte *) "glDeleteQueriesARB"));
		glIsQueryARB = reinterpret_cast<PFNGLISQUERYARBPROC>(bglGetProcAddress((const GLubyte *) "glIsQueryARB"));
		glBeginQueryARB = reinterpret_cast<PFNGLBEGINQUERYARBPROC>(bglGetProcAddress((const GLubyte *) "glBeginQueryARB"));
		glEndQueryARB = reinterpret_cast<PFNGLENDQUERYARBPROC>(bglGetProcAddress((const GLubyte *) "glEndQueryARB"));
		glGetQueryivARB = reinterpret_cast<PFNGLGETQUERYIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetQueryivARB"));
		glGetQueryObjectivARB = reinterpret_cast<PFNGLGETQUERYOBJECTIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetQueryObjectivARB"));
		glGetQueryObjectuivARB = reinterpret_cast<PFNGLGETQUERYOBJECTUIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetQueryObjectuivARB"));
		if (glGenQueriesARB && glDeleteQueriesARB && glIsQueryARB && glBeginQueryARB && glEndQueryARB && glGetQueryivARB && glGetQueryObjectivARB && glGetQueryObjectuivARB) {
			EnableExtension(_GL_ARB_occlusion_query);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_occlusion_query" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_occlusion_query implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ARB_shader_objects)
	if (QueryExtension("GL_ARB_shader_objects"))
	{
		glDeleteObjectARB = reinterpret_cast<PFNGLDELETEOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glDeleteObjectARB"));
		glGetHandleARB = reinterpret_cast<PFNGLGETHANDLEARBPROC>(bglGetProcAddress((const GLubyte *) "glGetHandleARB"));
		glDetachObjectARB = reinterpret_cast<PFNGLDETACHOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glDetachObjectARB"));
		glCreateShaderObjectARB = reinterpret_cast<PFNGLCREATESHADEROBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glCreateShaderObjectARB"));
		glShaderSourceARB = reinterpret_cast<PFNGLSHADERSOURCEARBPROC>(bglGetProcAddress((const GLubyte *) "glShaderSourceARB"));
		glCompileShaderARB = reinterpret_cast<PFNGLCOMPILESHADERARBPROC>(bglGetProcAddress((const GLubyte *) "glCompileShaderARB"));
		glCreateProgramObjectARB = reinterpret_cast<PFNGLCREATEPROGRAMOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glCreateProgramObjectARB"));
		glAttachObjectARB = reinterpret_cast<PFNGLATTACHOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glAttachObjectARB"));
		glLinkProgramARB = reinterpret_cast<PFNGLLINKPROGRAMARBPROC>(bglGetProcAddress((const GLubyte *) "glLinkProgramARB"));
		glUseProgramObjectARB = reinterpret_cast<PFNGLUSEPROGRAMOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glUseProgramObjectARB"));
		glValidateProgramARB = reinterpret_cast<PFNGLVALIDATEPROGRAMARBPROC>(bglGetProcAddress((const GLubyte *) "glValidateProgramARB"));
		glUniform1fARB = reinterpret_cast<PFNGLUNIFORM1FARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform1fARB"));
		glUniform2fARB = reinterpret_cast<PFNGLUNIFORM2FARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform2fARB"));
		glUniform3fARB = reinterpret_cast<PFNGLUNIFORM3FARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform3fARB"));
		glUniform4fARB = reinterpret_cast<PFNGLUNIFORM4FARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform4fARB"));
		glUniform1iARB = reinterpret_cast<PFNGLUNIFORM1IARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform1iARB"));
		glUniform2iARB = reinterpret_cast<PFNGLUNIFORM2IARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform2iARB"));
		glUniform3iARB = reinterpret_cast<PFNGLUNIFORM3IARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform3iARB"));
		glUniform4iARB = reinterpret_cast<PFNGLUNIFORM4IARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform4iARB"));
		glUniform1fvARB = reinterpret_cast<PFNGLUNIFORM1FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform1fvARB"));
		glUniform2fvARB = reinterpret_cast<PFNGLUNIFORM2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform2fvARB"));
		glUniform3fvARB = reinterpret_cast<PFNGLUNIFORM3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform3fvARB"));
		glUniform4fvARB = reinterpret_cast<PFNGLUNIFORM4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform4fvARB"));
		glUniform1ivARB = reinterpret_cast<PFNGLUNIFORM1IVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform1ivARB"));
		glUniform2ivARB = reinterpret_cast<PFNGLUNIFORM2IVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform2ivARB"));
		glUniform3ivARB = reinterpret_cast<PFNGLUNIFORM3IVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform3ivARB"));
		glUniform4ivARB = reinterpret_cast<PFNGLUNIFORM4IVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform4ivARB"));
		glUniformMatrix2fvARB = reinterpret_cast<PFNGLUNIFORMMATRIX2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniformMatrix2fvARB"));
		glUniformMatrix3fvARB = reinterpret_cast<PFNGLUNIFORMMATRIX3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniformMatrix3fvARB"));
		glUniformMatrix4fvARB = reinterpret_cast<PFNGLUNIFORMMATRIX4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniformMatrix4fvARB"));
		glGetObjectParameterfvARB = reinterpret_cast<PFNGLGETOBJECTPARAMETERFVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetObjectParameterfvARB"));
		glGetObjectParameterivARB = reinterpret_cast<PFNGLGETOBJECTPARAMETERIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetObjectParameterivARB"));
		glGetInfoLogARB = reinterpret_cast<PFNGLGETINFOLOGARBPROC>(bglGetProcAddress((const GLubyte *) "glGetInfoLogARB"));
		glGetAttachedObjectsARB = reinterpret_cast<PFNGLGETATTACHEDOBJECTSARBPROC>(bglGetProcAddress((const GLubyte *) "glGetAttachedObjectsARB"));
		glGetUniformLocationARB = reinterpret_cast<PFNGLGETUNIFORMLOCATIONARBPROC>(bglGetProcAddress((const GLubyte *) "glGetUniformLocationARB"));
		glGetActiveUniformARB = reinterpret_cast<PFNGLGETACTIVEUNIFORMARBPROC>(bglGetProcAddress((const GLubyte *) "glGetActiveUniformARB"));
		glGetUniformfvARB = reinterpret_cast<PFNGLGETUNIFORMFVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetUniformfvARB"));
		glGetUniformivARB = reinterpret_cast<PFNGLGETUNIFORMIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetUniformivARB"));
		glGetShaderSourceARB = reinterpret_cast<PFNGLGETSHADERSOURCEARBPROC>(bglGetProcAddress((const GLubyte *) "glGetShaderSourceARB"));
		if (glDeleteObjectARB && glGetHandleARB && glDetachObjectARB && glCreateShaderObjectARB && glShaderSourceARB && glCompileShaderARB && glCreateProgramObjectARB && glAttachObjectARB && glLinkProgramARB && glUseProgramObjectARB && glValidateProgramARB && glUniform1fARB && glUniform2fARB && glUniform3fARB && glUniform4fARB && glUniform1iARB && glUniform2iARB && glUniform3iARB && glUniform4iARB && glUniform1fvARB && glUniform2fvARB && glUniform3fvARB && glUniform4fvARB && glUniform1ivARB && glUniform2ivARB && glUniform3ivARB && glUniform4ivARB && glUniformMatrix2fvARB && glUniformMatrix3fvARB && glUniformMatrix4fvARB && glGetObjectParameterfvARB && glGetObjectParameterivARB && glGetInfoLogARB && glGetAttachedObjectsARB && glGetUniformLocationARB && glGetActiveUniformARB && glGetUniformfvARB && glGetUniformivARB && glGetShaderSourceARB) {
			EnableExtension(_GL_ARB_shader_objects);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_shader_objects" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_shader_objects implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ARB_vertex_shader)
	if (QueryExtension("GL_ARB_vertex_shader"))
	{
		glBindAttribLocationARB = reinterpret_cast<PFNGLBINDATTRIBLOCATIONARBPROC>(bglGetProcAddress((const GLubyte *) "glBindAttribLocationARB"));
		glGetActiveAttribARB = reinterpret_cast<PFNGLGETACTIVEATTRIBARBPROC>(bglGetProcAddress((const GLubyte *) "glGetActiveAttribARB"));
		glGetAttribLocationARB = reinterpret_cast<PFNGLGETATTRIBLOCATIONARBPROC>(bglGetProcAddress((const GLubyte *) "glGetAttribLocationARB"));
		if (glBindAttribLocationARB && glGetActiveAttribARB && glGetAttribLocationARB) {
			EnableExtension(_GL_ARB_vertex_shader);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ARB_vertex_shader" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_vertex_shader implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ARB_fragment_shader"))
	{
		EnableExtension(_GL_ARB_fragment_shader);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_fragment_shader" << std::endl;
	}

	if (QueryExtension("GL_ARB_shading_language_100"))
	{
		EnableExtension(_GL_ARB_shading_language_100);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_shading_language_100" << std::endl;
	}

	if (QueryExtension("GL_ARB_texture_non_power_of_two"))
	{
		EnableExtension(_GL_ARB_texture_non_power_of_two);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_texture_non_power_of_two" << std::endl;
	}

	if (QueryExtension("GL_ARB_point_sprite"))
	{
		EnableExtension(_GL_ARB_point_sprite);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_point_sprite" << std::endl;
	}

	if (QueryExtension("GL_ARB_fragment_program_shadow"))
	{
		EnableExtension(_GL_ARB_fragment_program_shadow);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ARB_fragment_program_shadow" << std::endl;
	}

	if (QueryExtension("GL_EXT_abgr"))
	{
		EnableExtension(_GL_EXT_abgr);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_abgr" << std::endl;
	}

#if defined(GL_EXT_texture3D)
	if (QueryExtension("GL_EXT_texture3D"))
	{
		glTexImage3DEXT = reinterpret_cast<PFNGLTEXIMAGE3DEXTPROC>(bglGetProcAddress((const GLubyte *) "glTexImage3DEXT"));
		glTexSubImage3DEXT = reinterpret_cast<PFNGLTEXSUBIMAGE3DEXTPROC>(bglGetProcAddress((const GLubyte *) "glTexSubImage3DEXT"));
		if (glTexImage3DEXT && glTexSubImage3DEXT) {
			EnableExtension(_GL_EXT_texture3D);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_texture3D" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_texture3D implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGIS_texture_filter4)
	if (QueryExtension("GL_SGIS_texture_filter4"))
	{
		glGetTexFilterFuncSGIS = reinterpret_cast<PFNGLGETTEXFILTERFUNCSGISPROC>(bglGetProcAddress((const GLubyte *) "glGetTexFilterFuncSGIS"));
		glTexFilterFuncSGIS = reinterpret_cast<PFNGLTEXFILTERFUNCSGISPROC>(bglGetProcAddress((const GLubyte *) "glTexFilterFuncSGIS"));
		if (glGetTexFilterFuncSGIS && glTexFilterFuncSGIS) {
			EnableExtension(_GL_SGIS_texture_filter4);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIS_texture_filter4" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIS_texture_filter4 implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_histogram)
	if (QueryExtension("GL_EXT_histogram"))
	{
		glGetHistogramEXT = reinterpret_cast<PFNGLGETHISTOGRAMEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetHistogramEXT"));
		glGetHistogramParameterfvEXT = reinterpret_cast<PFNGLGETHISTOGRAMPARAMETERFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetHistogramParameterfvEXT"));
		glGetHistogramParameterivEXT = reinterpret_cast<PFNGLGETHISTOGRAMPARAMETERIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetHistogramParameterivEXT"));
		glGetMinmaxEXT = reinterpret_cast<PFNGLGETMINMAXEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetMinmaxEXT"));
		glGetMinmaxParameterfvEXT = reinterpret_cast<PFNGLGETMINMAXPARAMETERFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetMinmaxParameterfvEXT"));
		glGetMinmaxParameterivEXT = reinterpret_cast<PFNGLGETMINMAXPARAMETERIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetMinmaxParameterivEXT"));
		glHistogramEXT = reinterpret_cast<PFNGLHISTOGRAMEXTPROC>(bglGetProcAddress((const GLubyte *) "glHistogramEXT"));
		glMinmaxEXT = reinterpret_cast<PFNGLMINMAXEXTPROC>(bglGetProcAddress((const GLubyte *) "glMinmaxEXT"));
		glResetHistogramEXT = reinterpret_cast<PFNGLRESETHISTOGRAMEXTPROC>(bglGetProcAddress((const GLubyte *) "glResetHistogramEXT"));
		glResetMinmaxEXT = reinterpret_cast<PFNGLRESETMINMAXEXTPROC>(bglGetProcAddress((const GLubyte *) "glResetMinmaxEXT"));
		if (glGetHistogramEXT && glGetHistogramParameterfvEXT && glGetHistogramParameterivEXT && glGetMinmaxEXT && glGetMinmaxParameterfvEXT && glGetMinmaxParameterivEXT && glHistogramEXT && glMinmaxEXT && glResetHistogramEXT && glResetMinmaxEXT) {
			EnableExtension(_GL_EXT_histogram);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_histogram" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_histogram implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_convolution)
	if (QueryExtension("GL_EXT_convolution"))
	{
		glConvolutionFilter1DEXT = reinterpret_cast<PFNGLCONVOLUTIONFILTER1DEXTPROC>(bglGetProcAddress((const GLubyte *) "glConvolutionFilter1DEXT"));
		glConvolutionFilter2DEXT = reinterpret_cast<PFNGLCONVOLUTIONFILTER2DEXTPROC>(bglGetProcAddress((const GLubyte *) "glConvolutionFilter2DEXT"));
		glConvolutionParameterfEXT = reinterpret_cast<PFNGLCONVOLUTIONPARAMETERFEXTPROC>(bglGetProcAddress((const GLubyte *) "glConvolutionParameterfEXT"));
		glConvolutionParameterfvEXT = reinterpret_cast<PFNGLCONVOLUTIONPARAMETERFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glConvolutionParameterfvEXT"));
		glConvolutionParameteriEXT = reinterpret_cast<PFNGLCONVOLUTIONPARAMETERIEXTPROC>(bglGetProcAddress((const GLubyte *) "glConvolutionParameteriEXT"));
		glConvolutionParameterivEXT = reinterpret_cast<PFNGLCONVOLUTIONPARAMETERIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glConvolutionParameterivEXT"));
		glCopyConvolutionFilter1DEXT = reinterpret_cast<PFNGLCOPYCONVOLUTIONFILTER1DEXTPROC>(bglGetProcAddress((const GLubyte *) "glCopyConvolutionFilter1DEXT"));
		glCopyConvolutionFilter2DEXT = reinterpret_cast<PFNGLCOPYCONVOLUTIONFILTER2DEXTPROC>(bglGetProcAddress((const GLubyte *) "glCopyConvolutionFilter2DEXT"));
		glGetConvolutionFilterEXT = reinterpret_cast<PFNGLGETCONVOLUTIONFILTEREXTPROC>(bglGetProcAddress((const GLubyte *) "glGetConvolutionFilterEXT"));
		glGetConvolutionParameterfvEXT = reinterpret_cast<PFNGLGETCONVOLUTIONPARAMETERFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetConvolutionParameterfvEXT"));
		glGetConvolutionParameterivEXT = reinterpret_cast<PFNGLGETCONVOLUTIONPARAMETERIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetConvolutionParameterivEXT"));
		glGetSeparableFilterEXT = reinterpret_cast<PFNGLGETSEPARABLEFILTEREXTPROC>(bglGetProcAddress((const GLubyte *) "glGetSeparableFilterEXT"));
		glSeparableFilter2DEXT = reinterpret_cast<PFNGLSEPARABLEFILTER2DEXTPROC>(bglGetProcAddress((const GLubyte *) "glSeparableFilter2DEXT"));
		if (glConvolutionFilter1DEXT && glConvolutionFilter2DEXT && glConvolutionParameterfEXT && glConvolutionParameterfvEXT && glConvolutionParameteriEXT && glConvolutionParameterivEXT && glCopyConvolutionFilter1DEXT && glCopyConvolutionFilter2DEXT && glGetConvolutionFilterEXT && glGetConvolutionParameterfvEXT && glGetConvolutionParameterivEXT && glGetSeparableFilterEXT && glSeparableFilter2DEXT) {
			EnableExtension(_GL_EXT_convolution);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_convolution" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_convolution implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGI_color_table)
	if (QueryExtension("GL_SGI_color_table"))
	{
		glColorTableSGI = reinterpret_cast<PFNGLCOLORTABLESGIPROC>(bglGetProcAddress((const GLubyte *) "glColorTableSGI"));
		glColorTableParameterfvSGI = reinterpret_cast<PFNGLCOLORTABLEPARAMETERFVSGIPROC>(bglGetProcAddress((const GLubyte *) "glColorTableParameterfvSGI"));
		glColorTableParameterivSGI = reinterpret_cast<PFNGLCOLORTABLEPARAMETERIVSGIPROC>(bglGetProcAddress((const GLubyte *) "glColorTableParameterivSGI"));
		glCopyColorTableSGI = reinterpret_cast<PFNGLCOPYCOLORTABLESGIPROC>(bglGetProcAddress((const GLubyte *) "glCopyColorTableSGI"));
		glGetColorTableSGI = reinterpret_cast<PFNGLGETCOLORTABLESGIPROC>(bglGetProcAddress((const GLubyte *) "glGetColorTableSGI"));
		glGetColorTableParameterfvSGI = reinterpret_cast<PFNGLGETCOLORTABLEPARAMETERFVSGIPROC>(bglGetProcAddress((const GLubyte *) "glGetColorTableParameterfvSGI"));
		glGetColorTableParameterivSGI = reinterpret_cast<PFNGLGETCOLORTABLEPARAMETERIVSGIPROC>(bglGetProcAddress((const GLubyte *) "glGetColorTableParameterivSGI"));
		if (glColorTableSGI && glColorTableParameterfvSGI && glColorTableParameterivSGI && glCopyColorTableSGI && glGetColorTableSGI && glGetColorTableParameterfvSGI && glGetColorTableParameterivSGI) {
			EnableExtension(_GL_SGI_color_table);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGI_color_table" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGI_color_table implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGIX_pixel_texture)
	if (QueryExtension("GL_SGIX_pixel_texture"))
	{
		glPixelTexGenSGIX = reinterpret_cast<PFNGLPIXELTEXGENSGIXPROC>(bglGetProcAddress((const GLubyte *) "glPixelTexGenSGIX"));
		if (glPixelTexGenSGIX) {
			EnableExtension(_GL_SGIX_pixel_texture);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_pixel_texture" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_pixel_texture implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGIS_pixel_texture)
	if (QueryExtension("GL_SGIS_pixel_texture"))
	{
		glPixelTexGenParameteriSGIS = reinterpret_cast<PFNGLPIXELTEXGENPARAMETERISGISPROC>(bglGetProcAddress((const GLubyte *) "glPixelTexGenParameteriSGIS"));
		glPixelTexGenParameterivSGIS = reinterpret_cast<PFNGLPIXELTEXGENPARAMETERIVSGISPROC>(bglGetProcAddress((const GLubyte *) "glPixelTexGenParameterivSGIS"));
		glPixelTexGenParameterfSGIS = reinterpret_cast<PFNGLPIXELTEXGENPARAMETERFSGISPROC>(bglGetProcAddress((const GLubyte *) "glPixelTexGenParameterfSGIS"));
		glPixelTexGenParameterfvSGIS = reinterpret_cast<PFNGLPIXELTEXGENPARAMETERFVSGISPROC>(bglGetProcAddress((const GLubyte *) "glPixelTexGenParameterfvSGIS"));
		glGetPixelTexGenParameterivSGIS = reinterpret_cast<PFNGLGETPIXELTEXGENPARAMETERIVSGISPROC>(bglGetProcAddress((const GLubyte *) "glGetPixelTexGenParameterivSGIS"));
		glGetPixelTexGenParameterfvSGIS = reinterpret_cast<PFNGLGETPIXELTEXGENPARAMETERFVSGISPROC>(bglGetProcAddress((const GLubyte *) "glGetPixelTexGenParameterfvSGIS"));
		if (glPixelTexGenParameteriSGIS && glPixelTexGenParameterivSGIS && glPixelTexGenParameterfSGIS && glPixelTexGenParameterfvSGIS && glGetPixelTexGenParameterivSGIS && glGetPixelTexGenParameterfvSGIS) {
			EnableExtension(_GL_SGIS_pixel_texture);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIS_pixel_texture" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIS_pixel_texture implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGIS_texture4D)
	if (QueryExtension("GL_SGIS_texture4D"))
	{
		glTexImage4DSGIS = reinterpret_cast<PFNGLTEXIMAGE4DSGISPROC>(bglGetProcAddress((const GLubyte *) "glTexImage4DSGIS"));
		glTexSubImage4DSGIS = reinterpret_cast<PFNGLTEXSUBIMAGE4DSGISPROC>(bglGetProcAddress((const GLubyte *) "glTexSubImage4DSGIS"));
		if (glTexImage4DSGIS && glTexSubImage4DSGIS) {
			EnableExtension(_GL_SGIS_texture4D);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIS_texture4D" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIS_texture4D implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGI_texture_color_table"))
	{
		EnableExtension(_GL_SGI_texture_color_table);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGI_texture_color_table" << std::endl;
	}

	if (QueryExtension("GL_EXT_cmyka"))
	{
		EnableExtension(_GL_EXT_cmyka);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_cmyka" << std::endl;
	}

#if defined(GL_SGIS_detail_texture)
	if (QueryExtension("GL_SGIS_detail_texture"))
	{
		glDetailTexFuncSGIS = reinterpret_cast<PFNGLDETAILTEXFUNCSGISPROC>(bglGetProcAddress((const GLubyte *) "glDetailTexFuncSGIS"));
		glGetDetailTexFuncSGIS = reinterpret_cast<PFNGLGETDETAILTEXFUNCSGISPROC>(bglGetProcAddress((const GLubyte *) "glGetDetailTexFuncSGIS"));
		if (glDetailTexFuncSGIS && glGetDetailTexFuncSGIS) {
			EnableExtension(_GL_SGIS_detail_texture);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIS_detail_texture" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIS_detail_texture implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGIS_sharpen_texture)
	if (QueryExtension("GL_SGIS_sharpen_texture"))
	{
		glSharpenTexFuncSGIS = reinterpret_cast<PFNGLSHARPENTEXFUNCSGISPROC>(bglGetProcAddress((const GLubyte *) "glSharpenTexFuncSGIS"));
		glGetSharpenTexFuncSGIS = reinterpret_cast<PFNGLGETSHARPENTEXFUNCSGISPROC>(bglGetProcAddress((const GLubyte *) "glGetSharpenTexFuncSGIS"));
		if (glSharpenTexFuncSGIS && glGetSharpenTexFuncSGIS) {
			EnableExtension(_GL_SGIS_sharpen_texture);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIS_sharpen_texture" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIS_sharpen_texture implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_packed_pixels"))
	{
		EnableExtension(_GL_EXT_packed_pixels);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_packed_pixels" << std::endl;
	}

	if (QueryExtension("GL_SGIS_texture_lod"))
	{
		EnableExtension(_GL_SGIS_texture_lod);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIS_texture_lod" << std::endl;
	}

#if defined(GL_SGIS_multisample)
	if (QueryExtension("GL_SGIS_multisample"))
	{
		glSampleMaskSGIS = reinterpret_cast<PFNGLSAMPLEMASKSGISPROC>(bglGetProcAddress((const GLubyte *) "glSampleMaskSGIS"));
		glSamplePatternSGIS = reinterpret_cast<PFNGLSAMPLEPATTERNSGISPROC>(bglGetProcAddress((const GLubyte *) "glSamplePatternSGIS"));
		if (glSampleMaskSGIS && glSamplePatternSGIS) {
			EnableExtension(_GL_SGIS_multisample);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIS_multisample" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIS_multisample implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_rescale_normal"))
	{
		EnableExtension(_GL_EXT_rescale_normal);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_rescale_normal" << std::endl;
	}

	if (QueryExtension("GL_EXT_misc_attribute"))
	{
		EnableExtension(_GL_EXT_misc_attribute);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_misc_attribute" << std::endl;
	}

	if (QueryExtension("GL_SGIS_generate_mipmap"))
	{
		EnableExtension(_GL_SGIS_generate_mipmap);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIS_generate_mipmap" << std::endl;
	}

	if (QueryExtension("GL_SGIX_clipmap"))
	{
		EnableExtension(_GL_SGIX_clipmap);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_clipmap" << std::endl;
	}

	if (QueryExtension("GL_SGIX_shadow"))
	{
		EnableExtension(_GL_SGIX_shadow);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_shadow" << std::endl;
	}

	if (QueryExtension("GL_SGIS_texture_edge_clamp"))
	{
		EnableExtension(_GL_SGIS_texture_edge_clamp);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIS_texture_edge_clamp" << std::endl;
	}

	if (QueryExtension("GL_SGIS_texture_border_clamp"))
	{
		EnableExtension(_GL_SGIS_texture_border_clamp);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIS_texture_border_clamp" << std::endl;
	}

#if defined(GL_EXT_blend_minmax)
	if (QueryExtension("GL_EXT_blend_minmax"))
	{
		glBlendEquationEXT = reinterpret_cast<PFNGLBLENDEQUATIONEXTPROC>(bglGetProcAddress((const GLubyte *) "glBlendEquationEXT"));
		if (glBlendEquationEXT) {
			EnableExtension(_GL_EXT_blend_minmax);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_blend_minmax" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_blend_minmax implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_blend_subtract"))
	{
		EnableExtension(_GL_EXT_blend_subtract);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_blend_subtract" << std::endl;
	}

	if (QueryExtension("GL_EXT_blend_logic_op"))
	{
		EnableExtension(_GL_EXT_blend_logic_op);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_blend_logic_op" << std::endl;
	}

	if (QueryExtension("GL_SGIX_interlace"))
	{
		EnableExtension(_GL_SGIX_interlace);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_interlace" << std::endl;
	}

#if defined(GL_SGIX_sprite)
	if (QueryExtension("GL_SGIX_sprite"))
	{
		glSpriteParameterfSGIX = reinterpret_cast<PFNGLSPRITEPARAMETERFSGIXPROC>(bglGetProcAddress((const GLubyte *) "glSpriteParameterfSGIX"));
		glSpriteParameterfvSGIX = reinterpret_cast<PFNGLSPRITEPARAMETERFVSGIXPROC>(bglGetProcAddress((const GLubyte *) "glSpriteParameterfvSGIX"));
		glSpriteParameteriSGIX = reinterpret_cast<PFNGLSPRITEPARAMETERISGIXPROC>(bglGetProcAddress((const GLubyte *) "glSpriteParameteriSGIX"));
		glSpriteParameterivSGIX = reinterpret_cast<PFNGLSPRITEPARAMETERIVSGIXPROC>(bglGetProcAddress((const GLubyte *) "glSpriteParameterivSGIX"));
		if (glSpriteParameterfSGIX && glSpriteParameterfvSGIX && glSpriteParameteriSGIX && glSpriteParameterivSGIX) {
			EnableExtension(_GL_SGIX_sprite);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_sprite" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_sprite implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_texture_multi_buffer"))
	{
		EnableExtension(_GL_SGIX_texture_multi_buffer);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_texture_multi_buffer" << std::endl;
	}

#if defined(GL_SGIX_instruments)
	if (QueryExtension("GL_SGIX_instruments"))
	{
		glGetInstrumentsSGIX = reinterpret_cast<PFNGLGETINSTRUMENTSSGIXPROC>(bglGetProcAddress((const GLubyte *) "glGetInstrumentsSGIX"));
		glInstrumentsBufferSGIX = reinterpret_cast<PFNGLINSTRUMENTSBUFFERSGIXPROC>(bglGetProcAddress((const GLubyte *) "glInstrumentsBufferSGIX"));
		glPollInstrumentsSGIX = reinterpret_cast<PFNGLPOLLINSTRUMENTSSGIXPROC>(bglGetProcAddress((const GLubyte *) "glPollInstrumentsSGIX"));
		glReadInstrumentsSGIX = reinterpret_cast<PFNGLREADINSTRUMENTSSGIXPROC>(bglGetProcAddress((const GLubyte *) "glReadInstrumentsSGIX"));
		glStartInstrumentsSGIX = reinterpret_cast<PFNGLSTARTINSTRUMENTSSGIXPROC>(bglGetProcAddress((const GLubyte *) "glStartInstrumentsSGIX"));
		glStopInstrumentsSGIX = reinterpret_cast<PFNGLSTOPINSTRUMENTSSGIXPROC>(bglGetProcAddress((const GLubyte *) "glStopInstrumentsSGIX"));
		if (glGetInstrumentsSGIX && glInstrumentsBufferSGIX && glPollInstrumentsSGIX && glReadInstrumentsSGIX && glStartInstrumentsSGIX && glStopInstrumentsSGIX) {
			EnableExtension(_GL_SGIX_instruments);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_instruments" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_instruments implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_texture_scale_bias"))
	{
		EnableExtension(_GL_SGIX_texture_scale_bias);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_texture_scale_bias" << std::endl;
	}

#if defined(GL_SGIX_framezoom)
	if (QueryExtension("GL_SGIX_framezoom"))
	{
		glFrameZoomSGIX = reinterpret_cast<PFNGLFRAMEZOOMSGIXPROC>(bglGetProcAddress((const GLubyte *) "glFrameZoomSGIX"));
		if (glFrameZoomSGIX) {
			EnableExtension(_GL_SGIX_framezoom);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_framezoom" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_framezoom implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGIX_tag_sample_buffer)
	if (QueryExtension("GL_SGIX_tag_sample_buffer"))
	{
		glTagSampleBufferSGIX = reinterpret_cast<PFNGLTAGSAMPLEBUFFERSGIXPROC>(bglGetProcAddress((const GLubyte *) "glTagSampleBufferSGIX"));
		if (glTagSampleBufferSGIX) {
			EnableExtension(_GL_SGIX_tag_sample_buffer);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_tag_sample_buffer" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_tag_sample_buffer implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGIX_reference_plane)
	if (QueryExtension("GL_SGIX_reference_plane"))
	{
		glReferencePlaneSGIX = reinterpret_cast<PFNGLREFERENCEPLANESGIXPROC>(bglGetProcAddress((const GLubyte *) "glReferencePlaneSGIX"));
		if (glReferencePlaneSGIX) {
			EnableExtension(_GL_SGIX_reference_plane);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_reference_plane" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_reference_plane implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SGIX_flush_raster)
	if (QueryExtension("GL_SGIX_flush_raster"))
	{
		glFlushRasterSGIX = reinterpret_cast<PFNGLFLUSHRASTERSGIXPROC>(bglGetProcAddress((const GLubyte *) "glFlushRasterSGIX"));
		if (glFlushRasterSGIX) {
			EnableExtension(_GL_SGIX_flush_raster);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_flush_raster" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_flush_raster implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_depth_texture"))
	{
		EnableExtension(_GL_SGIX_depth_texture);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_depth_texture" << std::endl;
	}

#if defined(GL_SGIS_fog_function)
	if (QueryExtension("GL_SGIS_fog_function"))
	{
		glFogFuncSGIS = reinterpret_cast<PFNGLFOGFUNCSGISPROC>(bglGetProcAddress((const GLubyte *) "glFogFuncSGIS"));
		glGetFogFuncSGIS = reinterpret_cast<PFNGLGETFOGFUNCSGISPROC>(bglGetProcAddress((const GLubyte *) "glGetFogFuncSGIS"));
		if (glFogFuncSGIS && glGetFogFuncSGIS) {
			EnableExtension(_GL_SGIS_fog_function);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIS_fog_function" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIS_fog_function implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_fog_offset"))
	{
		EnableExtension(_GL_SGIX_fog_offset);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_fog_offset" << std::endl;
	}

#if defined(GL_HP_image_transform)
	if (QueryExtension("GL_HP_image_transform"))
	{
		glImageTransformParameteriHP = reinterpret_cast<PFNGLIMAGETRANSFORMPARAMETERIHPPROC>(bglGetProcAddress((const GLubyte *) "glImageTransformParameteriHP"));
		glImageTransformParameterfHP = reinterpret_cast<PFNGLIMAGETRANSFORMPARAMETERFHPPROC>(bglGetProcAddress((const GLubyte *) "glImageTransformParameterfHP"));
		glImageTransformParameterivHP = reinterpret_cast<PFNGLIMAGETRANSFORMPARAMETERIVHPPROC>(bglGetProcAddress((const GLubyte *) "glImageTransformParameterivHP"));
		glImageTransformParameterfvHP = reinterpret_cast<PFNGLIMAGETRANSFORMPARAMETERFVHPPROC>(bglGetProcAddress((const GLubyte *) "glImageTransformParameterfvHP"));
		glGetImageTransformParameterivHP = reinterpret_cast<PFNGLGETIMAGETRANSFORMPARAMETERIVHPPROC>(bglGetProcAddress((const GLubyte *) "glGetImageTransformParameterivHP"));
		glGetImageTransformParameterfvHP = reinterpret_cast<PFNGLGETIMAGETRANSFORMPARAMETERFVHPPROC>(bglGetProcAddress((const GLubyte *) "glGetImageTransformParameterfvHP"));
		if (glImageTransformParameteriHP && glImageTransformParameterfHP && glImageTransformParameterivHP && glImageTransformParameterfvHP && glGetImageTransformParameterivHP && glGetImageTransformParameterfvHP) {
			EnableExtension(_GL_HP_image_transform);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_HP_image_transform" << std::endl;
		} else {
			std::cout << "ERROR: GL_HP_image_transform implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_HP_convolution_border_modes"))
	{
		EnableExtension(_GL_HP_convolution_border_modes);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_HP_convolution_border_modes" << std::endl;
	}

	if (QueryExtension("GL_SGIX_texture_add_env"))
	{
		EnableExtension(_GL_SGIX_texture_add_env);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_texture_add_env" << std::endl;
	}

#if defined(GL_EXT_color_subtable)
	if (QueryExtension("GL_EXT_color_subtable"))
	{
		glColorSubTableEXT = reinterpret_cast<PFNGLCOLORSUBTABLEEXTPROC>(bglGetProcAddress((const GLubyte *) "glColorSubTableEXT"));
		glCopyColorSubTableEXT = reinterpret_cast<PFNGLCOPYCOLORSUBTABLEEXTPROC>(bglGetProcAddress((const GLubyte *) "glCopyColorSubTableEXT"));
		if (glColorSubTableEXT && glCopyColorSubTableEXT) {
			EnableExtension(_GL_EXT_color_subtable);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_color_subtable" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_color_subtable implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_PGI_vertex_hints"))
	{
		EnableExtension(_GL_PGI_vertex_hints);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_PGI_vertex_hints" << std::endl;
	}

#if defined(GL_PGI_misc_hints)
	if (QueryExtension("GL_PGI_misc_hints"))
	{
		glHintPGI = reinterpret_cast<PFNGLHINTPGIPROC>(bglGetProcAddress((const GLubyte *) "glHintPGI"));
		if (glHintPGI) {
			EnableExtension(_GL_PGI_misc_hints);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_PGI_misc_hints" << std::endl;
		} else {
			std::cout << "ERROR: GL_PGI_misc_hints implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_paletted_texture)
	if (QueryExtension("GL_EXT_paletted_texture"))
	{
		glColorTableEXT = reinterpret_cast<PFNGLCOLORTABLEEXTPROC>(bglGetProcAddress((const GLubyte *) "glColorTableEXT"));
		glGetColorTableEXT = reinterpret_cast<PFNGLGETCOLORTABLEEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetColorTableEXT"));
		glGetColorTableParameterivEXT = reinterpret_cast<PFNGLGETCOLORTABLEPARAMETERIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetColorTableParameterivEXT"));
		glGetColorTableParameterfvEXT = reinterpret_cast<PFNGLGETCOLORTABLEPARAMETERFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetColorTableParameterfvEXT"));
		if (glColorTableEXT && glGetColorTableEXT && glGetColorTableParameterivEXT && glGetColorTableParameterfvEXT) {
			EnableExtension(_GL_EXT_paletted_texture);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_paletted_texture" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_paletted_texture implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_clip_volume_hint"))
	{
		EnableExtension(_GL_EXT_clip_volume_hint);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_clip_volume_hint" << std::endl;
	}

#if defined(GL_SGIX_list_priority)
	if (QueryExtension("GL_SGIX_list_priority"))
	{
		glGetListParameterfvSGIX = reinterpret_cast<PFNGLGETLISTPARAMETERFVSGIXPROC>(bglGetProcAddress((const GLubyte *) "glGetListParameterfvSGIX"));
		glGetListParameterivSGIX = reinterpret_cast<PFNGLGETLISTPARAMETERIVSGIXPROC>(bglGetProcAddress((const GLubyte *) "glGetListParameterivSGIX"));
		glListParameterfSGIX = reinterpret_cast<PFNGLLISTPARAMETERFSGIXPROC>(bglGetProcAddress((const GLubyte *) "glListParameterfSGIX"));
		glListParameterfvSGIX = reinterpret_cast<PFNGLLISTPARAMETERFVSGIXPROC>(bglGetProcAddress((const GLubyte *) "glListParameterfvSGIX"));
		glListParameteriSGIX = reinterpret_cast<PFNGLLISTPARAMETERISGIXPROC>(bglGetProcAddress((const GLubyte *) "glListParameteriSGIX"));
		glListParameterivSGIX = reinterpret_cast<PFNGLLISTPARAMETERIVSGIXPROC>(bglGetProcAddress((const GLubyte *) "glListParameterivSGIX"));
		if (glGetListParameterfvSGIX && glGetListParameterivSGIX && glListParameterfSGIX && glListParameterfvSGIX && glListParameteriSGIX && glListParameterivSGIX) {
			EnableExtension(_GL_SGIX_list_priority);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_list_priority" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_list_priority implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_ir_instrument1"))
	{
		EnableExtension(_GL_SGIX_ir_instrument1);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_ir_instrument1" << std::endl;
	}

	if (QueryExtension("GL_SGIX_texture_lod_bias"))
	{
		EnableExtension(_GL_SGIX_texture_lod_bias);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_texture_lod_bias" << std::endl;
	}

	if (QueryExtension("GL_SGIX_shadow_ambient"))
	{
		EnableExtension(_GL_SGIX_shadow_ambient);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_shadow_ambient" << std::endl;
	}

	if (QueryExtension("GL_EXT_index_texture"))
	{
		EnableExtension(_GL_EXT_index_texture);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_index_texture" << std::endl;
	}

#if defined(GL_EXT_index_material)
	if (QueryExtension("GL_EXT_index_material"))
	{
		glIndexMaterialEXT = reinterpret_cast<PFNGLINDEXMATERIALEXTPROC>(bglGetProcAddress((const GLubyte *) "glIndexMaterialEXT"));
		if (glIndexMaterialEXT) {
			EnableExtension(_GL_EXT_index_material);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_index_material" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_index_material implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_index_func)
	if (QueryExtension("GL_EXT_index_func"))
	{
		glIndexFuncEXT = reinterpret_cast<PFNGLINDEXFUNCEXTPROC>(bglGetProcAddress((const GLubyte *) "glIndexFuncEXT"));
		if (glIndexFuncEXT) {
			EnableExtension(_GL_EXT_index_func);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_index_func" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_index_func implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_index_array_formats"))
	{
		EnableExtension(_GL_EXT_index_array_formats);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_index_array_formats" << std::endl;
	}

#if defined(GL_EXT_cull_vertex)
	if (QueryExtension("GL_EXT_cull_vertex"))
	{
		glCullParameterdvEXT = reinterpret_cast<PFNGLCULLPARAMETERDVEXTPROC>(bglGetProcAddress((const GLubyte *) "glCullParameterdvEXT"));
		glCullParameterfvEXT = reinterpret_cast<PFNGLCULLPARAMETERFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glCullParameterfvEXT"));
		if (glCullParameterdvEXT && glCullParameterfvEXT) {
			EnableExtension(_GL_EXT_cull_vertex);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_cull_vertex" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_cull_vertex implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_ycrcb"))
	{
		EnableExtension(_GL_SGIX_ycrcb);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_ycrcb" << std::endl;
	}

	if (QueryExtension("GL_IBM_rasterpos_clip"))
	{
		EnableExtension(_GL_IBM_rasterpos_clip);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_IBM_rasterpos_clip" << std::endl;
	}

	if (QueryExtension("GL_HP_texture_lighting"))
	{
		EnableExtension(_GL_HP_texture_lighting);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_HP_texture_lighting" << std::endl;
	}

#if defined(GL_EXT_draw_range_elements)
	if (QueryExtension("GL_EXT_draw_range_elements"))
	{
		glDrawRangeElementsEXT = reinterpret_cast<PFNGLDRAWRANGEELEMENTSEXTPROC>(bglGetProcAddress((const GLubyte *) "glDrawRangeElementsEXT"));
		if (glDrawRangeElementsEXT) {
			EnableExtension(_GL_EXT_draw_range_elements);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_draw_range_elements" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_draw_range_elements implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_WIN_phong_shading"))
	{
		EnableExtension(_GL_WIN_phong_shading);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_WIN_phong_shading" << std::endl;
	}

	if (QueryExtension("GL_WIN_specular_fog"))
	{
		EnableExtension(_GL_WIN_specular_fog);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_WIN_specular_fog" << std::endl;
	}

#if defined(GL_EXT_light_texture)
	if (QueryExtension("GL_EXT_light_texture"))
	{
		glApplyTextureEXT = reinterpret_cast<PFNGLAPPLYTEXTUREEXTPROC>(bglGetProcAddress((const GLubyte *) "glApplyTextureEXT"));
		glTextureLightEXT = reinterpret_cast<PFNGLTEXTURELIGHTEXTPROC>(bglGetProcAddress((const GLubyte *) "glTextureLightEXT"));
		glTextureMaterialEXT = reinterpret_cast<PFNGLTEXTUREMATERIALEXTPROC>(bglGetProcAddress((const GLubyte *) "glTextureMaterialEXT"));
		if (glApplyTextureEXT && glTextureLightEXT && glTextureMaterialEXT) {
			EnableExtension(_GL_EXT_light_texture);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_light_texture" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_light_texture implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_blend_alpha_minmax"))
	{
		EnableExtension(_GL_SGIX_blend_alpha_minmax);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_blend_alpha_minmax" << std::endl;
	}

	if (QueryExtension("GL_EXT_bgra"))
	{
		EnableExtension(_GL_EXT_bgra);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_bgra" << std::endl;
	}

#if defined(GL_SGIX_async)
	if (QueryExtension("GL_SGIX_async"))
	{
		glAsyncMarkerSGIX = reinterpret_cast<PFNGLASYNCMARKERSGIXPROC>(bglGetProcAddress((const GLubyte *) "glAsyncMarkerSGIX"));
		glFinishAsyncSGIX = reinterpret_cast<PFNGLFINISHASYNCSGIXPROC>(bglGetProcAddress((const GLubyte *) "glFinishAsyncSGIX"));
		glPollAsyncSGIX = reinterpret_cast<PFNGLPOLLASYNCSGIXPROC>(bglGetProcAddress((const GLubyte *) "glPollAsyncSGIX"));
		glGenAsyncMarkersSGIX = reinterpret_cast<PFNGLGENASYNCMARKERSSGIXPROC>(bglGetProcAddress((const GLubyte *) "glGenAsyncMarkersSGIX"));
		glDeleteAsyncMarkersSGIX = reinterpret_cast<PFNGLDELETEASYNCMARKERSSGIXPROC>(bglGetProcAddress((const GLubyte *) "glDeleteAsyncMarkersSGIX"));
		glIsAsyncMarkerSGIX = reinterpret_cast<PFNGLISASYNCMARKERSGIXPROC>(bglGetProcAddress((const GLubyte *) "glIsAsyncMarkerSGIX"));
		if (glAsyncMarkerSGIX && glFinishAsyncSGIX && glPollAsyncSGIX && glGenAsyncMarkersSGIX && glDeleteAsyncMarkersSGIX && glIsAsyncMarkerSGIX) {
			EnableExtension(_GL_SGIX_async);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIX_async" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIX_async implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_async_pixel"))
	{
		EnableExtension(_GL_SGIX_async_pixel);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_async_pixel" << std::endl;
	}

	if (QueryExtension("GL_SGIX_async_histogram"))
	{
		EnableExtension(_GL_SGIX_async_histogram);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_async_histogram" << std::endl;
	}

#if defined(GL_INTEL_parallel_arrays)
	if (QueryExtension("GL_INTEL_parallel_arrays"))
	{
		glVertexPointervINTEL = reinterpret_cast<PFNGLVERTEXPOINTERVINTELPROC>(bglGetProcAddress((const GLubyte *) "glVertexPointervINTEL"));
		glNormalPointervINTEL = reinterpret_cast<PFNGLNORMALPOINTERVINTELPROC>(bglGetProcAddress((const GLubyte *) "glNormalPointervINTEL"));
		glColorPointervINTEL = reinterpret_cast<PFNGLCOLORPOINTERVINTELPROC>(bglGetProcAddress((const GLubyte *) "glColorPointervINTEL"));
		glTexCoordPointervINTEL = reinterpret_cast<PFNGLTEXCOORDPOINTERVINTELPROC>(bglGetProcAddress((const GLubyte *) "glTexCoordPointervINTEL"));
		if (glVertexPointervINTEL && glNormalPointervINTEL && glColorPointervINTEL && glTexCoordPointervINTEL) {
			EnableExtension(_GL_INTEL_parallel_arrays);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_INTEL_parallel_arrays" << std::endl;
		} else {
			std::cout << "ERROR: GL_INTEL_parallel_arrays implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_HP_occlusion_test"))
	{
		EnableExtension(_GL_HP_occlusion_test);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_HP_occlusion_test" << std::endl;
	}

#if defined(GL_EXT_pixel_transform)
	if (QueryExtension("GL_EXT_pixel_transform"))
	{
		glPixelTransformParameteriEXT = reinterpret_cast<PFNGLPIXELTRANSFORMPARAMETERIEXTPROC>(bglGetProcAddress((const GLubyte *) "glPixelTransformParameteriEXT"));
		glPixelTransformParameterfEXT = reinterpret_cast<PFNGLPIXELTRANSFORMPARAMETERFEXTPROC>(bglGetProcAddress((const GLubyte *) "glPixelTransformParameterfEXT"));
		glPixelTransformParameterivEXT = reinterpret_cast<PFNGLPIXELTRANSFORMPARAMETERIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glPixelTransformParameterivEXT"));
		glPixelTransformParameterfvEXT = reinterpret_cast<PFNGLPIXELTRANSFORMPARAMETERFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glPixelTransformParameterfvEXT"));
		if (glPixelTransformParameteriEXT && glPixelTransformParameterfEXT && glPixelTransformParameterivEXT && glPixelTransformParameterfvEXT) {
			EnableExtension(_GL_EXT_pixel_transform);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_pixel_transform" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_pixel_transform implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_pixel_transform_color_table"))
	{
		EnableExtension(_GL_EXT_pixel_transform_color_table);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_pixel_transform_color_table" << std::endl;
	}

	if (QueryExtension("GL_EXT_shared_texture_palette"))
	{
		EnableExtension(_GL_EXT_shared_texture_palette);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_shared_texture_palette" << std::endl;
	}

	if (QueryExtension("GL_EXT_separate_specular_color"))
	{
		EnableExtension(_GL_EXT_separate_specular_color);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_separate_specular_color" << std::endl;
	}

#if defined(GL_EXT_secondary_color)
	if (QueryExtension("GL_EXT_secondary_color"))
	{
		glSecondaryColor3bEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3BEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3bEXT"));
		glSecondaryColor3bvEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3BVEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3bvEXT"));
		glSecondaryColor3dEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3DEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3dEXT"));
		glSecondaryColor3dvEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3DVEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3dvEXT"));
		glSecondaryColor3fEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3FEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3fEXT"));
		glSecondaryColor3fvEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3FVEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3fvEXT"));
		glSecondaryColor3iEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3IEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3iEXT"));
		glSecondaryColor3ivEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3IVEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3ivEXT"));
		glSecondaryColor3sEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3SEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3sEXT"));
		glSecondaryColor3svEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3SVEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3svEXT"));
		glSecondaryColor3ubEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3UBEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3ubEXT"));
		glSecondaryColor3ubvEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3UBVEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3ubvEXT"));
		glSecondaryColor3uiEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3UIEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3uiEXT"));
		glSecondaryColor3uivEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3UIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3uivEXT"));
		glSecondaryColor3usEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3USEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3usEXT"));
		glSecondaryColor3usvEXT = reinterpret_cast<PFNGLSECONDARYCOLOR3USVEXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3usvEXT"));
		glSecondaryColorPointerEXT = reinterpret_cast<PFNGLSECONDARYCOLORPOINTEREXTPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColorPointerEXT"));
		if (glSecondaryColor3bEXT && glSecondaryColor3bvEXT && glSecondaryColor3dEXT && glSecondaryColor3dvEXT && glSecondaryColor3fEXT && glSecondaryColor3fvEXT && glSecondaryColor3iEXT && glSecondaryColor3ivEXT && glSecondaryColor3sEXT && glSecondaryColor3svEXT && glSecondaryColor3ubEXT && glSecondaryColor3ubvEXT && glSecondaryColor3uiEXT && glSecondaryColor3uivEXT && glSecondaryColor3usEXT && glSecondaryColor3usvEXT && glSecondaryColorPointerEXT) {
			EnableExtension(_GL_EXT_secondary_color);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_secondary_color" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_secondary_color implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_texture_perturb_normal)
	if (QueryExtension("GL_EXT_texture_perturb_normal"))
	{
		glTextureNormalEXT = reinterpret_cast<PFNGLTEXTURENORMALEXTPROC>(bglGetProcAddress((const GLubyte *) "glTextureNormalEXT"));
		if (glTextureNormalEXT) {
			EnableExtension(_GL_EXT_texture_perturb_normal);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_texture_perturb_normal" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_texture_perturb_normal implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_multi_draw_arrays)
	if (QueryExtension("GL_EXT_multi_draw_arrays"))
	{
		glMultiDrawArraysEXT = reinterpret_cast<PFNGLMULTIDRAWARRAYSEXTPROC>(bglGetProcAddress((const GLubyte *) "glMultiDrawArraysEXT"));
		glMultiDrawElementsEXT = reinterpret_cast<PFNGLMULTIDRAWELEMENTSEXTPROC>(bglGetProcAddress((const GLubyte *) "glMultiDrawElementsEXT"));
		if (glMultiDrawArraysEXT && glMultiDrawElementsEXT) {
			EnableExtension(_GL_EXT_multi_draw_arrays);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_multi_draw_arrays" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_multi_draw_arrays implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_fog_coord)
	if (QueryExtension("GL_EXT_fog_coord"))
	{
		glFogCoordfEXT = reinterpret_cast<PFNGLFOGCOORDFEXTPROC>(bglGetProcAddress((const GLubyte *) "glFogCoordfEXT"));
		glFogCoordfvEXT = reinterpret_cast<PFNGLFOGCOORDFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glFogCoordfvEXT"));
		glFogCoorddEXT = reinterpret_cast<PFNGLFOGCOORDDEXTPROC>(bglGetProcAddress((const GLubyte *) "glFogCoorddEXT"));
		glFogCoorddvEXT = reinterpret_cast<PFNGLFOGCOORDDVEXTPROC>(bglGetProcAddress((const GLubyte *) "glFogCoorddvEXT"));
		glFogCoordPointerEXT = reinterpret_cast<PFNGLFOGCOORDPOINTEREXTPROC>(bglGetProcAddress((const GLubyte *) "glFogCoordPointerEXT"));
		if (glFogCoordfEXT && glFogCoordfvEXT && glFogCoorddEXT && glFogCoorddvEXT && glFogCoordPointerEXT) {
			EnableExtension(_GL_EXT_fog_coord);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_fog_coord" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_fog_coord implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_REND_screen_coordinates"))
	{
		EnableExtension(_GL_REND_screen_coordinates);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_REND_screen_coordinates" << std::endl;
	}

#if defined(GL_EXT_coordinate_frame)
	if (QueryExtension("GL_EXT_coordinate_frame"))
	{
		glTangent3bEXT = reinterpret_cast<PFNGLTANGENT3BEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3bEXT"));
		glTangent3bvEXT = reinterpret_cast<PFNGLTANGENT3BVEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3bvEXT"));
		glTangent3dEXT = reinterpret_cast<PFNGLTANGENT3DEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3dEXT"));
		glTangent3dvEXT = reinterpret_cast<PFNGLTANGENT3DVEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3dvEXT"));
		glTangent3fEXT = reinterpret_cast<PFNGLTANGENT3FEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3fEXT"));
		glTangent3fvEXT = reinterpret_cast<PFNGLTANGENT3FVEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3fvEXT"));
		glTangent3iEXT = reinterpret_cast<PFNGLTANGENT3IEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3iEXT"));
		glTangent3ivEXT = reinterpret_cast<PFNGLTANGENT3IVEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3ivEXT"));
		glTangent3sEXT = reinterpret_cast<PFNGLTANGENT3SEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3sEXT"));
		glTangent3svEXT = reinterpret_cast<PFNGLTANGENT3SVEXTPROC>(bglGetProcAddress((const GLubyte *) "glTangent3svEXT"));
		glBinormal3bEXT = reinterpret_cast<PFNGLBINORMAL3BEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3bEXT"));
		glBinormal3bvEXT = reinterpret_cast<PFNGLBINORMAL3BVEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3bvEXT"));
		glBinormal3dEXT = reinterpret_cast<PFNGLBINORMAL3DEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3dEXT"));
		glBinormal3dvEXT = reinterpret_cast<PFNGLBINORMAL3DVEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3dvEXT"));
		glBinormal3fEXT = reinterpret_cast<PFNGLBINORMAL3FEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3fEXT"));
		glBinormal3fvEXT = reinterpret_cast<PFNGLBINORMAL3FVEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3fvEXT"));
		glBinormal3iEXT = reinterpret_cast<PFNGLBINORMAL3IEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3iEXT"));
		glBinormal3ivEXT = reinterpret_cast<PFNGLBINORMAL3IVEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3ivEXT"));
		glBinormal3sEXT = reinterpret_cast<PFNGLBINORMAL3SEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3sEXT"));
		glBinormal3svEXT = reinterpret_cast<PFNGLBINORMAL3SVEXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormal3svEXT"));
		glTangentPointerEXT = reinterpret_cast<PFNGLTANGENTPOINTEREXTPROC>(bglGetProcAddress((const GLubyte *) "glTangentPointerEXT"));
		glBinormalPointerEXT = reinterpret_cast<PFNGLBINORMALPOINTEREXTPROC>(bglGetProcAddress((const GLubyte *) "glBinormalPointerEXT"));
		if (glTangent3bEXT && glTangent3bvEXT && glTangent3dEXT && glTangent3dvEXT && glTangent3fEXT && glTangent3fvEXT && glTangent3iEXT && glTangent3ivEXT && glTangent3sEXT && glTangent3svEXT && glBinormal3bEXT && glBinormal3bvEXT && glBinormal3dEXT && glBinormal3dvEXT && glBinormal3fEXT && glBinormal3fvEXT && glBinormal3iEXT && glBinormal3ivEXT && glBinormal3sEXT && glBinormal3svEXT && glTangentPointerEXT && glBinormalPointerEXT) {
			EnableExtension(_GL_EXT_coordinate_frame);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_coordinate_frame" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_coordinate_frame implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_texture_env_combine"))
	{
		EnableExtension(_GL_EXT_texture_env_combine);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_texture_env_combine" << std::endl;
	}

	if (QueryExtension("GL_APPLE_specular_vector"))
	{
		EnableExtension(_GL_APPLE_specular_vector);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_APPLE_specular_vector" << std::endl;
	}

	if (QueryExtension("GL_APPLE_transform_hint"))
	{
		EnableExtension(_GL_APPLE_transform_hint);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_APPLE_transform_hint" << std::endl;
	}

#if defined(GL_SUNX_constant_data)
	if (QueryExtension("GL_SUNX_constant_data"))
	{
		glFinishTextureSUNX = reinterpret_cast<PFNGLFINISHTEXTURESUNXPROC>(bglGetProcAddress((const GLubyte *) "glFinishTextureSUNX"));
		if (glFinishTextureSUNX) {
			EnableExtension(_GL_SUNX_constant_data);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SUNX_constant_data" << std::endl;
		} else {
			std::cout << "ERROR: GL_SUNX_constant_data implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SUN_global_alpha)
	if (QueryExtension("GL_SUN_global_alpha"))
	{
		glGlobalAlphaFactorbSUN = reinterpret_cast<PFNGLGLOBALALPHAFACTORBSUNPROC>(bglGetProcAddress((const GLubyte *) "glGlobalAlphaFactorbSUN"));
		glGlobalAlphaFactorsSUN = reinterpret_cast<PFNGLGLOBALALPHAFACTORSSUNPROC>(bglGetProcAddress((const GLubyte *) "glGlobalAlphaFactorsSUN"));
		glGlobalAlphaFactoriSUN = reinterpret_cast<PFNGLGLOBALALPHAFACTORISUNPROC>(bglGetProcAddress((const GLubyte *) "glGlobalAlphaFactoriSUN"));
		glGlobalAlphaFactorfSUN = reinterpret_cast<PFNGLGLOBALALPHAFACTORFSUNPROC>(bglGetProcAddress((const GLubyte *) "glGlobalAlphaFactorfSUN"));
		glGlobalAlphaFactordSUN = reinterpret_cast<PFNGLGLOBALALPHAFACTORDSUNPROC>(bglGetProcAddress((const GLubyte *) "glGlobalAlphaFactordSUN"));
		glGlobalAlphaFactorubSUN = reinterpret_cast<PFNGLGLOBALALPHAFACTORUBSUNPROC>(bglGetProcAddress((const GLubyte *) "glGlobalAlphaFactorubSUN"));
		glGlobalAlphaFactorusSUN = reinterpret_cast<PFNGLGLOBALALPHAFACTORUSSUNPROC>(bglGetProcAddress((const GLubyte *) "glGlobalAlphaFactorusSUN"));
		glGlobalAlphaFactoruiSUN = reinterpret_cast<PFNGLGLOBALALPHAFACTORUISUNPROC>(bglGetProcAddress((const GLubyte *) "glGlobalAlphaFactoruiSUN"));
		if (glGlobalAlphaFactorbSUN && glGlobalAlphaFactorsSUN && glGlobalAlphaFactoriSUN && glGlobalAlphaFactorfSUN && glGlobalAlphaFactordSUN && glGlobalAlphaFactorubSUN && glGlobalAlphaFactorusSUN && glGlobalAlphaFactoruiSUN) {
			EnableExtension(_GL_SUN_global_alpha);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SUN_global_alpha" << std::endl;
		} else {
			std::cout << "ERROR: GL_SUN_global_alpha implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SUN_triangle_list)
	if (QueryExtension("GL_SUN_triangle_list"))
	{
		glReplacementCodeuiSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUISUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiSUN"));
		glReplacementCodeusSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUSSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeusSUN"));
		glReplacementCodeubSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUBSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeubSUN"));
		glReplacementCodeuivSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUIVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuivSUN"));
		glReplacementCodeusvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUSVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeusvSUN"));
		glReplacementCodeubvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUBVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeubvSUN"));
		glReplacementCodePointerSUN = reinterpret_cast<PFNGLREPLACEMENTCODEPOINTERSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodePointerSUN"));
		if (glReplacementCodeuiSUN && glReplacementCodeusSUN && glReplacementCodeubSUN && glReplacementCodeuivSUN && glReplacementCodeusvSUN && glReplacementCodeubvSUN && glReplacementCodePointerSUN) {
			EnableExtension(_GL_SUN_triangle_list);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SUN_triangle_list" << std::endl;
		} else {
			std::cout << "ERROR: GL_SUN_triangle_list implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SUN_vertex)
	if (QueryExtension("GL_SUN_vertex"))
	{
		glColor4ubVertex2fSUN = reinterpret_cast<PFNGLCOLOR4UBVERTEX2FSUNPROC>(bglGetProcAddress((const GLubyte *) "glColor4ubVertex2fSUN"));
		glColor4ubVertex2fvSUN = reinterpret_cast<PFNGLCOLOR4UBVERTEX2FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glColor4ubVertex2fvSUN"));
		glColor4ubVertex3fSUN = reinterpret_cast<PFNGLCOLOR4UBVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glColor4ubVertex3fSUN"));
		glColor4ubVertex3fvSUN = reinterpret_cast<PFNGLCOLOR4UBVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glColor4ubVertex3fvSUN"));
		glColor3fVertex3fSUN = reinterpret_cast<PFNGLCOLOR3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glColor3fVertex3fSUN"));
		glColor3fVertex3fvSUN = reinterpret_cast<PFNGLCOLOR3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glColor3fVertex3fvSUN"));
		glNormal3fVertex3fSUN = reinterpret_cast<PFNGLNORMAL3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glNormal3fVertex3fSUN"));
		glNormal3fVertex3fvSUN = reinterpret_cast<PFNGLNORMAL3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glNormal3fVertex3fvSUN"));
		glColor4fNormal3fVertex3fSUN = reinterpret_cast<PFNGLCOLOR4FNORMAL3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glColor4fNormal3fVertex3fSUN"));
		glColor4fNormal3fVertex3fvSUN = reinterpret_cast<PFNGLCOLOR4FNORMAL3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glColor4fNormal3fVertex3fvSUN"));
		glTexCoord2fVertex3fSUN = reinterpret_cast<PFNGLTEXCOORD2FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fVertex3fSUN"));
		glTexCoord2fVertex3fvSUN = reinterpret_cast<PFNGLTEXCOORD2FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fVertex3fvSUN"));
		glTexCoord4fVertex4fSUN = reinterpret_cast<PFNGLTEXCOORD4FVERTEX4FSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord4fVertex4fSUN"));
		glTexCoord4fVertex4fvSUN = reinterpret_cast<PFNGLTEXCOORD4FVERTEX4FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord4fVertex4fvSUN"));
		glTexCoord2fColor4ubVertex3fSUN = reinterpret_cast<PFNGLTEXCOORD2FCOLOR4UBVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fColor4ubVertex3fSUN"));
		glTexCoord2fColor4ubVertex3fvSUN = reinterpret_cast<PFNGLTEXCOORD2FCOLOR4UBVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fColor4ubVertex3fvSUN"));
		glTexCoord2fColor3fVertex3fSUN = reinterpret_cast<PFNGLTEXCOORD2FCOLOR3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fColor3fVertex3fSUN"));
		glTexCoord2fColor3fVertex3fvSUN = reinterpret_cast<PFNGLTEXCOORD2FCOLOR3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fColor3fVertex3fvSUN"));
		glTexCoord2fNormal3fVertex3fSUN = reinterpret_cast<PFNGLTEXCOORD2FNORMAL3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fNormal3fVertex3fSUN"));
		glTexCoord2fNormal3fVertex3fvSUN = reinterpret_cast<PFNGLTEXCOORD2FNORMAL3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fNormal3fVertex3fvSUN"));
		glTexCoord2fColor4fNormal3fVertex3fSUN = reinterpret_cast<PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fColor4fNormal3fVertex3fSUN"));
		glTexCoord2fColor4fNormal3fVertex3fvSUN = reinterpret_cast<PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2fColor4fNormal3fVertex3fvSUN"));
		glTexCoord4fColor4fNormal3fVertex4fSUN = reinterpret_cast<PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord4fColor4fNormal3fVertex4fSUN"));
		glTexCoord4fColor4fNormal3fVertex4fvSUN = reinterpret_cast<PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord4fColor4fNormal3fVertex4fvSUN"));
		glReplacementCodeuiVertex3fSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUIVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiVertex3fSUN"));
		glReplacementCodeuiVertex3fvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUIVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiVertex3fvSUN"));
		glReplacementCodeuiColor4ubVertex3fSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiColor4ubVertex3fSUN"));
		glReplacementCodeuiColor4ubVertex3fvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiColor4ubVertex3fvSUN"));
		glReplacementCodeuiColor3fVertex3fSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiColor3fVertex3fSUN"));
		glReplacementCodeuiColor3fVertex3fvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiColor3fVertex3fvSUN"));
		glReplacementCodeuiNormal3fVertex3fSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiNormal3fVertex3fSUN"));
		glReplacementCodeuiNormal3fVertex3fvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiNormal3fVertex3fvSUN"));
		glReplacementCodeuiColor4fNormal3fVertex3fSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiColor4fNormal3fVertex3fSUN"));
		glReplacementCodeuiColor4fNormal3fVertex3fvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiColor4fNormal3fVertex3fvSUN"));
		glReplacementCodeuiTexCoord2fVertex3fSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiTexCoord2fVertex3fSUN"));
		glReplacementCodeuiTexCoord2fVertex3fvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiTexCoord2fVertex3fvSUN"));
		glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN"));
		glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN"));
		glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN"));
		glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN = reinterpret_cast<PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC>(bglGetProcAddress((const GLubyte *) "glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN"));
		if (glColor4ubVertex2fSUN && glColor4ubVertex2fvSUN && glColor4ubVertex3fSUN && glColor4ubVertex3fvSUN && glColor3fVertex3fSUN && glColor3fVertex3fvSUN && glNormal3fVertex3fSUN && glNormal3fVertex3fvSUN && glColor4fNormal3fVertex3fSUN && glColor4fNormal3fVertex3fvSUN && glTexCoord2fVertex3fSUN && glTexCoord2fVertex3fvSUN && glTexCoord4fVertex4fSUN && glTexCoord4fVertex4fvSUN && glTexCoord2fColor4ubVertex3fSUN && glTexCoord2fColor4ubVertex3fvSUN && glTexCoord2fColor3fVertex3fSUN && glTexCoord2fColor3fVertex3fvSUN && glTexCoord2fNormal3fVertex3fSUN && glTexCoord2fNormal3fVertex3fvSUN && glTexCoord2fColor4fNormal3fVertex3fSUN && glTexCoord2fColor4fNormal3fVertex3fvSUN && glTexCoord4fColor4fNormal3fVertex4fSUN && glTexCoord4fColor4fNormal3fVertex4fvSUN && glReplacementCodeuiVertex3fSUN && glReplacementCodeuiVertex3fvSUN && glReplacementCodeuiColor4ubVertex3fSUN && glReplacementCodeuiColor4ubVertex3fvSUN && glReplacementCodeuiColor3fVertex3fSUN && glReplacementCodeuiColor3fVertex3fvSUN && glReplacementCodeuiNormal3fVertex3fSUN && glReplacementCodeuiNormal3fVertex3fvSUN && glReplacementCodeuiColor4fNormal3fVertex3fSUN && glReplacementCodeuiColor4fNormal3fVertex3fvSUN && glReplacementCodeuiTexCoord2fVertex3fSUN && glReplacementCodeuiTexCoord2fVertex3fvSUN && glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN && glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN && glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN && glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN) {
			EnableExtension(_GL_SUN_vertex);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SUN_vertex" << std::endl;
		} else {
			std::cout << "ERROR: GL_SUN_vertex implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_blend_func_separate)
	if (QueryExtension("GL_EXT_blend_func_separate"))
	{
		glBlendFuncSeparateEXT = reinterpret_cast<PFNGLBLENDFUNCSEPARATEEXTPROC>(bglGetProcAddress((const GLubyte *) "glBlendFuncSeparateEXT"));
		if (glBlendFuncSeparateEXT) {
			EnableExtension(_GL_EXT_blend_func_separate);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_blend_func_separate" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_blend_func_separate implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_INGR_color_clamp"))
	{
		EnableExtension(_GL_INGR_color_clamp);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_INGR_color_clamp" << std::endl;
	}

	if (QueryExtension("GL_INGR_interlace_read"))
	{
		EnableExtension(_GL_INGR_interlace_read);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_INGR_interlace_read" << std::endl;
	}

	if (QueryExtension("GL_EXT_stencil_wrap"))
	{
		EnableExtension(_GL_EXT_stencil_wrap);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_stencil_wrap" << std::endl;
	}

	if (QueryExtension("GL_EXT_422_pixels"))
	{
		EnableExtension(_GL_EXT_422_pixels);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_422_pixels" << std::endl;
	}

	if (QueryExtension("GL_NV_texgen_reflection"))
	{
		EnableExtension(_GL_NV_texgen_reflection);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texgen_reflection" << std::endl;
	}

	if (QueryExtension("GL_SUN_convolution_border_modes"))
	{
		EnableExtension(_GL_SUN_convolution_border_modes);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SUN_convolution_border_modes" << std::endl;
	}

	if (QueryExtension("GL_EXT_texture_env_add"))
	{
		EnableExtension(_GL_EXT_texture_env_add);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_texture_env_add" << std::endl;
	}

	if (QueryExtension("GL_EXT_texture_lod_bias"))
	{
		EnableExtension(_GL_EXT_texture_lod_bias);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_texture_lod_bias" << std::endl;
	}

	if (QueryExtension("GL_EXT_texture_filter_anisotropic"))
	{
		EnableExtension(_GL_EXT_texture_filter_anisotropic);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_texture_filter_anisotropic" << std::endl;
	}

#if defined(GL_EXT_vertex_weighting)
	if (QueryExtension("GL_EXT_vertex_weighting"))
	{
		glVertexWeightfEXT = reinterpret_cast<PFNGLVERTEXWEIGHTFEXTPROC>(bglGetProcAddress((const GLubyte *) "glVertexWeightfEXT"));
		glVertexWeightfvEXT = reinterpret_cast<PFNGLVERTEXWEIGHTFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVertexWeightfvEXT"));
		glVertexWeightPointerEXT = reinterpret_cast<PFNGLVERTEXWEIGHTPOINTEREXTPROC>(bglGetProcAddress((const GLubyte *) "glVertexWeightPointerEXT"));
		if (glVertexWeightfEXT && glVertexWeightfvEXT && glVertexWeightPointerEXT) {
			EnableExtension(_GL_EXT_vertex_weighting);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_vertex_weighting" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_vertex_weighting implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_NV_light_max_exponent"))
	{
		EnableExtension(_GL_NV_light_max_exponent);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_light_max_exponent" << std::endl;
	}

#if defined(GL_NV_vertex_array_range)
	if (QueryExtension("GL_NV_vertex_array_range"))
	{
		glFlushVertexArrayRangeNV = reinterpret_cast<PFNGLFLUSHVERTEXARRAYRANGENVPROC>(bglGetProcAddress((const GLubyte *) "glFlushVertexArrayRangeNV"));
		glVertexArrayRangeNV = reinterpret_cast<PFNGLVERTEXARRAYRANGENVPROC>(bglGetProcAddress((const GLubyte *) "glVertexArrayRangeNV"));
		if (glFlushVertexArrayRangeNV && glVertexArrayRangeNV) {
			EnableExtension(_GL_NV_vertex_array_range);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_vertex_array_range" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_vertex_array_range implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_NV_register_combiners)
	if (QueryExtension("GL_NV_register_combiners"))
	{
		glCombinerParameterfvNV = reinterpret_cast<PFNGLCOMBINERPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glCombinerParameterfvNV"));
		glCombinerParameterfNV = reinterpret_cast<PFNGLCOMBINERPARAMETERFNVPROC>(bglGetProcAddress((const GLubyte *) "glCombinerParameterfNV"));
		glCombinerParameterivNV = reinterpret_cast<PFNGLCOMBINERPARAMETERIVNVPROC>(bglGetProcAddress((const GLubyte *) "glCombinerParameterivNV"));
		glCombinerParameteriNV = reinterpret_cast<PFNGLCOMBINERPARAMETERINVPROC>(bglGetProcAddress((const GLubyte *) "glCombinerParameteriNV"));
		glCombinerInputNV = reinterpret_cast<PFNGLCOMBINERINPUTNVPROC>(bglGetProcAddress((const GLubyte *) "glCombinerInputNV"));
		glCombinerOutputNV = reinterpret_cast<PFNGLCOMBINEROUTPUTNVPROC>(bglGetProcAddress((const GLubyte *) "glCombinerOutputNV"));
		glFinalCombinerInputNV = reinterpret_cast<PFNGLFINALCOMBINERINPUTNVPROC>(bglGetProcAddress((const GLubyte *) "glFinalCombinerInputNV"));
		glGetCombinerInputParameterfvNV = reinterpret_cast<PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetCombinerInputParameterfvNV"));
		glGetCombinerInputParameterivNV = reinterpret_cast<PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetCombinerInputParameterivNV"));
		glGetCombinerOutputParameterfvNV = reinterpret_cast<PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetCombinerOutputParameterfvNV"));
		glGetCombinerOutputParameterivNV = reinterpret_cast<PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetCombinerOutputParameterivNV"));
		glGetFinalCombinerInputParameterfvNV = reinterpret_cast<PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetFinalCombinerInputParameterfvNV"));
		glGetFinalCombinerInputParameterivNV = reinterpret_cast<PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetFinalCombinerInputParameterivNV"));
		if (glCombinerParameterfvNV && glCombinerParameterfNV && glCombinerParameterivNV && glCombinerParameteriNV && glCombinerInputNV && glCombinerOutputNV && glFinalCombinerInputNV && glGetCombinerInputParameterfvNV && glGetCombinerInputParameterivNV && glGetCombinerOutputParameterfvNV && glGetCombinerOutputParameterivNV && glGetFinalCombinerInputParameterfvNV && glGetFinalCombinerInputParameterivNV) {
			EnableExtension(_GL_NV_register_combiners);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_register_combiners" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_register_combiners implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_NV_fog_distance"))
	{
		EnableExtension(_GL_NV_fog_distance);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_fog_distance" << std::endl;
	}

	if (QueryExtension("GL_NV_texgen_emboss"))
	{
		EnableExtension(_GL_NV_texgen_emboss);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texgen_emboss" << std::endl;
	}

	if (QueryExtension("GL_NV_blend_square"))
	{
		EnableExtension(_GL_NV_blend_square);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_blend_square" << std::endl;
	}

	if (QueryExtension("GL_NV_texture_env_combine4"))
	{
		EnableExtension(_GL_NV_texture_env_combine4);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texture_env_combine4" << std::endl;
	}

#if defined(GL_MESA_resize_buffers)
	if (QueryExtension("GL_MESA_resize_buffers"))
	{
		glResizeBuffersMESA = reinterpret_cast<PFNGLRESIZEBUFFERSMESAPROC>(bglGetProcAddress((const GLubyte *) "glResizeBuffersMESA"));
		if (glResizeBuffersMESA) {
			EnableExtension(_GL_MESA_resize_buffers);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_MESA_resize_buffers" << std::endl;
		} else {
			std::cout << "ERROR: GL_MESA_resize_buffers implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_MESA_window_pos)
	if (QueryExtension("GL_MESA_window_pos"))
	{
		glWindowPos2dMESA = reinterpret_cast<PFNGLWINDOWPOS2DMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2dMESA"));
		glWindowPos2dvMESA = reinterpret_cast<PFNGLWINDOWPOS2DVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2dvMESA"));
		glWindowPos2fMESA = reinterpret_cast<PFNGLWINDOWPOS2FMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2fMESA"));
		glWindowPos2fvMESA = reinterpret_cast<PFNGLWINDOWPOS2FVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2fvMESA"));
		glWindowPos2iMESA = reinterpret_cast<PFNGLWINDOWPOS2IMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2iMESA"));
		glWindowPos2ivMESA = reinterpret_cast<PFNGLWINDOWPOS2IVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2ivMESA"));
		glWindowPos2sMESA = reinterpret_cast<PFNGLWINDOWPOS2SMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2sMESA"));
		glWindowPos2svMESA = reinterpret_cast<PFNGLWINDOWPOS2SVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos2svMESA"));
		glWindowPos3dMESA = reinterpret_cast<PFNGLWINDOWPOS3DMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3dMESA"));
		glWindowPos3dvMESA = reinterpret_cast<PFNGLWINDOWPOS3DVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3dvMESA"));
		glWindowPos3fMESA = reinterpret_cast<PFNGLWINDOWPOS3FMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3fMESA"));
		glWindowPos3fvMESA = reinterpret_cast<PFNGLWINDOWPOS3FVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3fvMESA"));
		glWindowPos3iMESA = reinterpret_cast<PFNGLWINDOWPOS3IMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3iMESA"));
		glWindowPos3ivMESA = reinterpret_cast<PFNGLWINDOWPOS3IVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3ivMESA"));
		glWindowPos3sMESA = reinterpret_cast<PFNGLWINDOWPOS3SMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3sMESA"));
		glWindowPos3svMESA = reinterpret_cast<PFNGLWINDOWPOS3SVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos3svMESA"));
		glWindowPos4dMESA = reinterpret_cast<PFNGLWINDOWPOS4DMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos4dMESA"));
		glWindowPos4dvMESA = reinterpret_cast<PFNGLWINDOWPOS4DVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos4dvMESA"));
		glWindowPos4fMESA = reinterpret_cast<PFNGLWINDOWPOS4FMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos4fMESA"));
		glWindowPos4fvMESA = reinterpret_cast<PFNGLWINDOWPOS4FVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos4fvMESA"));
		glWindowPos4iMESA = reinterpret_cast<PFNGLWINDOWPOS4IMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos4iMESA"));
		glWindowPos4ivMESA = reinterpret_cast<PFNGLWINDOWPOS4IVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos4ivMESA"));
		glWindowPos4sMESA = reinterpret_cast<PFNGLWINDOWPOS4SMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos4sMESA"));
		glWindowPos4svMESA = reinterpret_cast<PFNGLWINDOWPOS4SVMESAPROC>(bglGetProcAddress((const GLubyte *) "glWindowPos4svMESA"));
		if (glWindowPos2dMESA && glWindowPos2dvMESA && glWindowPos2fMESA && glWindowPos2fvMESA && glWindowPos2iMESA && glWindowPos2ivMESA && glWindowPos2sMESA && glWindowPos2svMESA && glWindowPos3dMESA && glWindowPos3dvMESA && glWindowPos3fMESA && glWindowPos3fvMESA && glWindowPos3iMESA && glWindowPos3ivMESA && glWindowPos3sMESA && glWindowPos3svMESA && glWindowPos4dMESA && glWindowPos4dvMESA && glWindowPos4fMESA && glWindowPos4fvMESA && glWindowPos4iMESA && glWindowPos4ivMESA && glWindowPos4sMESA && glWindowPos4svMESA) {
			EnableExtension(_GL_MESA_window_pos);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_MESA_window_pos" << std::endl;
		} else {
			std::cout << "ERROR: GL_MESA_window_pos implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_IBM_cull_vertex"))
	{
		EnableExtension(_GL_IBM_cull_vertex);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_IBM_cull_vertex" << std::endl;
	}

#if defined(GL_IBM_multimode_draw_arrays)
	if (QueryExtension("GL_IBM_multimode_draw_arrays"))
	{
		glMultiModeDrawArraysIBM = reinterpret_cast<PFNGLMULTIMODEDRAWARRAYSIBMPROC>(bglGetProcAddress((const GLubyte *) "glMultiModeDrawArraysIBM"));
		glMultiModeDrawElementsIBM = reinterpret_cast<PFNGLMULTIMODEDRAWELEMENTSIBMPROC>(bglGetProcAddress((const GLubyte *) "glMultiModeDrawElementsIBM"));
		if (glMultiModeDrawArraysIBM && glMultiModeDrawElementsIBM) {
			EnableExtension(_GL_IBM_multimode_draw_arrays);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_IBM_multimode_draw_arrays" << std::endl;
		} else {
			std::cout << "ERROR: GL_IBM_multimode_draw_arrays implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_IBM_vertex_array_lists)
	if (QueryExtension("GL_IBM_vertex_array_lists"))
	{
		glColorPointerListIBM = reinterpret_cast<PFNGLCOLORPOINTERLISTIBMPROC>(bglGetProcAddress((const GLubyte *) "glColorPointerListIBM"));
		glSecondaryColorPointerListIBM = reinterpret_cast<PFNGLSECONDARYCOLORPOINTERLISTIBMPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColorPointerListIBM"));
		glEdgeFlagPointerListIBM = reinterpret_cast<PFNGLEDGEFLAGPOINTERLISTIBMPROC>(bglGetProcAddress((const GLubyte *) "glEdgeFlagPointerListIBM"));
		glFogCoordPointerListIBM = reinterpret_cast<PFNGLFOGCOORDPOINTERLISTIBMPROC>(bglGetProcAddress((const GLubyte *) "glFogCoordPointerListIBM"));
		glIndexPointerListIBM = reinterpret_cast<PFNGLINDEXPOINTERLISTIBMPROC>(bglGetProcAddress((const GLubyte *) "glIndexPointerListIBM"));
		glNormalPointerListIBM = reinterpret_cast<PFNGLNORMALPOINTERLISTIBMPROC>(bglGetProcAddress((const GLubyte *) "glNormalPointerListIBM"));
		glTexCoordPointerListIBM = reinterpret_cast<PFNGLTEXCOORDPOINTERLISTIBMPROC>(bglGetProcAddress((const GLubyte *) "glTexCoordPointerListIBM"));
		glVertexPointerListIBM = reinterpret_cast<PFNGLVERTEXPOINTERLISTIBMPROC>(bglGetProcAddress((const GLubyte *) "glVertexPointerListIBM"));
		if (glColorPointerListIBM && glSecondaryColorPointerListIBM && glEdgeFlagPointerListIBM && glFogCoordPointerListIBM && glIndexPointerListIBM && glNormalPointerListIBM && glTexCoordPointerListIBM && glVertexPointerListIBM) {
			EnableExtension(_GL_IBM_vertex_array_lists);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_IBM_vertex_array_lists" << std::endl;
		} else {
			std::cout << "ERROR: GL_IBM_vertex_array_lists implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_3DFX_texture_compression_FXT1"))
	{
		EnableExtension(_GL_3DFX_texture_compression_FXT1);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_3DFX_texture_compression_FXT1" << std::endl;
	}

	if (QueryExtension("GL_3DFX_multisample"))
	{
		EnableExtension(_GL_3DFX_multisample);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_3DFX_multisample" << std::endl;
	}

#if defined(GL_3DFX_tbuffer)
	if (QueryExtension("GL_3DFX_tbuffer"))
	{
		glTbufferMask3DFX = reinterpret_cast<PFNGLTBUFFERMASK3DFXPROC>(bglGetProcAddress((const GLubyte *) "glTbufferMask3DFX"));
		if (glTbufferMask3DFX) {
			EnableExtension(_GL_3DFX_tbuffer);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_3DFX_tbuffer" << std::endl;
		} else {
			std::cout << "ERROR: GL_3DFX_tbuffer implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_vertex_preclip"))
	{
		EnableExtension(_GL_SGIX_vertex_preclip);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_vertex_preclip" << std::endl;
	}

	if (QueryExtension("GL_SGIX_resample"))
	{
		EnableExtension(_GL_SGIX_resample);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_resample" << std::endl;
	}

#if defined(GL_SGIS_texture_color_mask)
	if (QueryExtension("GL_SGIS_texture_color_mask"))
	{
		glTextureColorMaskSGIS = reinterpret_cast<PFNGLTEXTURECOLORMASKSGISPROC>(bglGetProcAddress((const GLubyte *) "glTextureColorMaskSGIS"));
		if (glTextureColorMaskSGIS) {
			EnableExtension(_GL_SGIS_texture_color_mask);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SGIS_texture_color_mask" << std::endl;
		} else {
			std::cout << "ERROR: GL_SGIS_texture_color_mask implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_texture_env_dot3"))
	{
		EnableExtension(_GL_EXT_texture_env_dot3);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_texture_env_dot3" << std::endl;
	}

	if (QueryExtension("GL_ATI_texture_mirror_once"))
	{
		EnableExtension(_GL_ATI_texture_mirror_once);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ATI_texture_mirror_once" << std::endl;
	}

#if defined(GL_NV_fence)
	if (QueryExtension("GL_NV_fence"))
	{
		glDeleteFencesNV = reinterpret_cast<PFNGLDELETEFENCESNVPROC>(bglGetProcAddress((const GLubyte *) "glDeleteFencesNV"));
		glGenFencesNV = reinterpret_cast<PFNGLGENFENCESNVPROC>(bglGetProcAddress((const GLubyte *) "glGenFencesNV"));
		glIsFenceNV = reinterpret_cast<PFNGLISFENCENVPROC>(bglGetProcAddress((const GLubyte *) "glIsFenceNV"));
		glTestFenceNV = reinterpret_cast<PFNGLTESTFENCENVPROC>(bglGetProcAddress((const GLubyte *) "glTestFenceNV"));
		glGetFenceivNV = reinterpret_cast<PFNGLGETFENCEIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetFenceivNV"));
		glFinishFenceNV = reinterpret_cast<PFNGLFINISHFENCENVPROC>(bglGetProcAddress((const GLubyte *) "glFinishFenceNV"));
		glSetFenceNV = reinterpret_cast<PFNGLSETFENCENVPROC>(bglGetProcAddress((const GLubyte *) "glSetFenceNV"));
		if (glDeleteFencesNV && glGenFencesNV && glIsFenceNV && glTestFenceNV && glGetFenceivNV && glFinishFenceNV && glSetFenceNV) {
			EnableExtension(_GL_NV_fence);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_fence" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_fence implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_NV_evaluators)
	if (QueryExtension("GL_NV_evaluators"))
	{
		glMapControlPointsNV = reinterpret_cast<PFNGLMAPCONTROLPOINTSNVPROC>(bglGetProcAddress((const GLubyte *) "glMapControlPointsNV"));
		glMapParameterivNV = reinterpret_cast<PFNGLMAPPARAMETERIVNVPROC>(bglGetProcAddress((const GLubyte *) "glMapParameterivNV"));
		glMapParameterfvNV = reinterpret_cast<PFNGLMAPPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glMapParameterfvNV"));
		glGetMapControlPointsNV = reinterpret_cast<PFNGLGETMAPCONTROLPOINTSNVPROC>(bglGetProcAddress((const GLubyte *) "glGetMapControlPointsNV"));
		glGetMapParameterivNV = reinterpret_cast<PFNGLGETMAPPARAMETERIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetMapParameterivNV"));
		glGetMapParameterfvNV = reinterpret_cast<PFNGLGETMAPPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetMapParameterfvNV"));
		glGetMapAttribParameterivNV = reinterpret_cast<PFNGLGETMAPATTRIBPARAMETERIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetMapAttribParameterivNV"));
		glGetMapAttribParameterfvNV = reinterpret_cast<PFNGLGETMAPATTRIBPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetMapAttribParameterfvNV"));
		glEvalMapsNV = reinterpret_cast<PFNGLEVALMAPSNVPROC>(bglGetProcAddress((const GLubyte *) "glEvalMapsNV"));
		if (glMapControlPointsNV && glMapParameterivNV && glMapParameterfvNV && glGetMapControlPointsNV && glGetMapParameterivNV && glGetMapParameterfvNV && glGetMapAttribParameterivNV && glGetMapAttribParameterfvNV && glEvalMapsNV) {
			EnableExtension(_GL_NV_evaluators);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_evaluators" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_evaluators implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_NV_packed_depth_stencil"))
	{
		EnableExtension(_GL_NV_packed_depth_stencil);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_packed_depth_stencil" << std::endl;
	}

#if defined(GL_NV_register_combiners2)
	if (QueryExtension("GL_NV_register_combiners2"))
	{
		glCombinerStageParameterfvNV = reinterpret_cast<PFNGLCOMBINERSTAGEPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glCombinerStageParameterfvNV"));
		glGetCombinerStageParameterfvNV = reinterpret_cast<PFNGLGETCOMBINERSTAGEPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetCombinerStageParameterfvNV"));
		if (glCombinerStageParameterfvNV && glGetCombinerStageParameterfvNV) {
			EnableExtension(_GL_NV_register_combiners2);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_register_combiners2" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_register_combiners2 implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_NV_texture_compression_vtc"))
	{
		EnableExtension(_GL_NV_texture_compression_vtc);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texture_compression_vtc" << std::endl;
	}

	if (QueryExtension("GL_NV_texture_rectangle"))
	{
		EnableExtension(_GL_NV_texture_rectangle);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texture_rectangle" << std::endl;
	}

	if (QueryExtension("GL_NV_texture_shader"))
	{
		EnableExtension(_GL_NV_texture_shader);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texture_shader" << std::endl;
	}

	if (QueryExtension("GL_NV_texture_shader2"))
	{
		EnableExtension(_GL_NV_texture_shader2);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texture_shader2" << std::endl;
	}

	if (QueryExtension("GL_NV_vertex_array_range2"))
	{
		EnableExtension(_GL_NV_vertex_array_range2);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_vertex_array_range2" << std::endl;
	}

#if defined(GL_NV_vertex_program)
	if (QueryExtension("GL_NV_vertex_program"))
	{
		glAreProgramsResidentNV = reinterpret_cast<PFNGLAREPROGRAMSRESIDENTNVPROC>(bglGetProcAddress((const GLubyte *) "glAreProgramsResidentNV"));
		glBindProgramNV = reinterpret_cast<PFNGLBINDPROGRAMNVPROC>(bglGetProcAddress((const GLubyte *) "glBindProgramNV"));
		glDeleteProgramsNV = reinterpret_cast<PFNGLDELETEPROGRAMSNVPROC>(bglGetProcAddress((const GLubyte *) "glDeleteProgramsNV"));
		glExecuteProgramNV = reinterpret_cast<PFNGLEXECUTEPROGRAMNVPROC>(bglGetProcAddress((const GLubyte *) "glExecuteProgramNV"));
		glGenProgramsNV = reinterpret_cast<PFNGLGENPROGRAMSNVPROC>(bglGetProcAddress((const GLubyte *) "glGenProgramsNV"));
		glGetProgramParameterdvNV = reinterpret_cast<PFNGLGETPROGRAMPARAMETERDVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramParameterdvNV"));
		glGetProgramParameterfvNV = reinterpret_cast<PFNGLGETPROGRAMPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramParameterfvNV"));
		glGetProgramivNV = reinterpret_cast<PFNGLGETPROGRAMIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramivNV"));
		glGetProgramStringNV = reinterpret_cast<PFNGLGETPROGRAMSTRINGNVPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramStringNV"));
		glGetTrackMatrixivNV = reinterpret_cast<PFNGLGETTRACKMATRIXIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetTrackMatrixivNV"));
		glGetVertexAttribdvNV = reinterpret_cast<PFNGLGETVERTEXATTRIBDVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribdvNV"));
		glGetVertexAttribfvNV = reinterpret_cast<PFNGLGETVERTEXATTRIBFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribfvNV"));
		glGetVertexAttribivNV = reinterpret_cast<PFNGLGETVERTEXATTRIBIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribivNV"));
		glGetVertexAttribPointervNV = reinterpret_cast<PFNGLGETVERTEXATTRIBPOINTERVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribPointervNV"));
		glIsProgramNV = reinterpret_cast<PFNGLISPROGRAMNVPROC>(bglGetProcAddress((const GLubyte *) "glIsProgramNV"));
		glLoadProgramNV = reinterpret_cast<PFNGLLOADPROGRAMNVPROC>(bglGetProcAddress((const GLubyte *) "glLoadProgramNV"));
		glProgramParameter4dNV = reinterpret_cast<PFNGLPROGRAMPARAMETER4DNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramParameter4dNV"));
		glProgramParameter4dvNV = reinterpret_cast<PFNGLPROGRAMPARAMETER4DVNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramParameter4dvNV"));
		glProgramParameter4fNV = reinterpret_cast<PFNGLPROGRAMPARAMETER4FNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramParameter4fNV"));
		glProgramParameter4fvNV = reinterpret_cast<PFNGLPROGRAMPARAMETER4FVNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramParameter4fvNV"));
		glProgramParameters4dvNV = reinterpret_cast<PFNGLPROGRAMPARAMETERS4DVNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramParameters4dvNV"));
		glProgramParameters4fvNV = reinterpret_cast<PFNGLPROGRAMPARAMETERS4FVNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramParameters4fvNV"));
		glRequestResidentProgramsNV = reinterpret_cast<PFNGLREQUESTRESIDENTPROGRAMSNVPROC>(bglGetProcAddress((const GLubyte *) "glRequestResidentProgramsNV"));
		glTrackMatrixNV = reinterpret_cast<PFNGLTRACKMATRIXNVPROC>(bglGetProcAddress((const GLubyte *) "glTrackMatrixNV"));
		glVertexAttribPointerNV = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribPointerNV"));
		glVertexAttrib1dNV = reinterpret_cast<PFNGLVERTEXATTRIB1DNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1dNV"));
		glVertexAttrib1dvNV = reinterpret_cast<PFNGLVERTEXATTRIB1DVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1dvNV"));
		glVertexAttrib1fNV = reinterpret_cast<PFNGLVERTEXATTRIB1FNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1fNV"));
		glVertexAttrib1fvNV = reinterpret_cast<PFNGLVERTEXATTRIB1FVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1fvNV"));
		glVertexAttrib1sNV = reinterpret_cast<PFNGLVERTEXATTRIB1SNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1sNV"));
		glVertexAttrib1svNV = reinterpret_cast<PFNGLVERTEXATTRIB1SVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1svNV"));
		glVertexAttrib2dNV = reinterpret_cast<PFNGLVERTEXATTRIB2DNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2dNV"));
		glVertexAttrib2dvNV = reinterpret_cast<PFNGLVERTEXATTRIB2DVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2dvNV"));
		glVertexAttrib2fNV = reinterpret_cast<PFNGLVERTEXATTRIB2FNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2fNV"));
		glVertexAttrib2fvNV = reinterpret_cast<PFNGLVERTEXATTRIB2FVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2fvNV"));
		glVertexAttrib2sNV = reinterpret_cast<PFNGLVERTEXATTRIB2SNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2sNV"));
		glVertexAttrib2svNV = reinterpret_cast<PFNGLVERTEXATTRIB2SVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2svNV"));
		glVertexAttrib3dNV = reinterpret_cast<PFNGLVERTEXATTRIB3DNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3dNV"));
		glVertexAttrib3dvNV = reinterpret_cast<PFNGLVERTEXATTRIB3DVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3dvNV"));
		glVertexAttrib3fNV = reinterpret_cast<PFNGLVERTEXATTRIB3FNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3fNV"));
		glVertexAttrib3fvNV = reinterpret_cast<PFNGLVERTEXATTRIB3FVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3fvNV"));
		glVertexAttrib3sNV = reinterpret_cast<PFNGLVERTEXATTRIB3SNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3sNV"));
		glVertexAttrib3svNV = reinterpret_cast<PFNGLVERTEXATTRIB3SVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3svNV"));
		glVertexAttrib4dNV = reinterpret_cast<PFNGLVERTEXATTRIB4DNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4dNV"));
		glVertexAttrib4dvNV = reinterpret_cast<PFNGLVERTEXATTRIB4DVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4dvNV"));
		glVertexAttrib4fNV = reinterpret_cast<PFNGLVERTEXATTRIB4FNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4fNV"));
		glVertexAttrib4fvNV = reinterpret_cast<PFNGLVERTEXATTRIB4FVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4fvNV"));
		glVertexAttrib4sNV = reinterpret_cast<PFNGLVERTEXATTRIB4SNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4sNV"));
		glVertexAttrib4svNV = reinterpret_cast<PFNGLVERTEXATTRIB4SVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4svNV"));
		glVertexAttrib4ubNV = reinterpret_cast<PFNGLVERTEXATTRIB4UBNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4ubNV"));
		glVertexAttrib4ubvNV = reinterpret_cast<PFNGLVERTEXATTRIB4UBVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4ubvNV"));
		glVertexAttribs1dvNV = reinterpret_cast<PFNGLVERTEXATTRIBS1DVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs1dvNV"));
		glVertexAttribs1fvNV = reinterpret_cast<PFNGLVERTEXATTRIBS1FVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs1fvNV"));
		glVertexAttribs1svNV = reinterpret_cast<PFNGLVERTEXATTRIBS1SVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs1svNV"));
		glVertexAttribs2dvNV = reinterpret_cast<PFNGLVERTEXATTRIBS2DVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs2dvNV"));
		glVertexAttribs2fvNV = reinterpret_cast<PFNGLVERTEXATTRIBS2FVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs2fvNV"));
		glVertexAttribs2svNV = reinterpret_cast<PFNGLVERTEXATTRIBS2SVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs2svNV"));
		glVertexAttribs3dvNV = reinterpret_cast<PFNGLVERTEXATTRIBS3DVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs3dvNV"));
		glVertexAttribs3fvNV = reinterpret_cast<PFNGLVERTEXATTRIBS3FVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs3fvNV"));
		glVertexAttribs3svNV = reinterpret_cast<PFNGLVERTEXATTRIBS3SVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs3svNV"));
		glVertexAttribs4dvNV = reinterpret_cast<PFNGLVERTEXATTRIBS4DVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs4dvNV"));
		glVertexAttribs4fvNV = reinterpret_cast<PFNGLVERTEXATTRIBS4FVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs4fvNV"));
		glVertexAttribs4svNV = reinterpret_cast<PFNGLVERTEXATTRIBS4SVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs4svNV"));
		glVertexAttribs4ubvNV = reinterpret_cast<PFNGLVERTEXATTRIBS4UBVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs4ubvNV"));
		if (glAreProgramsResidentNV && glBindProgramNV && glDeleteProgramsNV && glExecuteProgramNV && glGenProgramsNV && glGetProgramParameterdvNV && glGetProgramParameterfvNV && glGetProgramivNV && glGetProgramStringNV && glGetTrackMatrixivNV && glGetVertexAttribdvNV && glGetVertexAttribfvNV && glGetVertexAttribivNV && glGetVertexAttribPointervNV && glIsProgramNV && glLoadProgramNV && glProgramParameter4dNV && glProgramParameter4dvNV && glProgramParameter4fNV && glProgramParameter4fvNV && glProgramParameters4dvNV && glProgramParameters4fvNV && glRequestResidentProgramsNV && glTrackMatrixNV && glVertexAttribPointerNV && glVertexAttrib1dNV && glVertexAttrib1dvNV && glVertexAttrib1fNV && glVertexAttrib1fvNV && glVertexAttrib1sNV && glVertexAttrib1svNV && glVertexAttrib2dNV && glVertexAttrib2dvNV && glVertexAttrib2fNV && glVertexAttrib2fvNV && glVertexAttrib2sNV && glVertexAttrib2svNV && glVertexAttrib3dNV && glVertexAttrib3dvNV && glVertexAttrib3fNV && glVertexAttrib3fvNV && glVertexAttrib3sNV && glVertexAttrib3svNV && glVertexAttrib4dNV && glVertexAttrib4dvNV && glVertexAttrib4fNV && glVertexAttrib4fvNV && glVertexAttrib4sNV && glVertexAttrib4svNV && glVertexAttrib4ubNV && glVertexAttrib4ubvNV && glVertexAttribs1dvNV && glVertexAttribs1fvNV && glVertexAttribs1svNV && glVertexAttribs2dvNV && glVertexAttribs2fvNV && glVertexAttribs2svNV && glVertexAttribs3dvNV && glVertexAttribs3fvNV && glVertexAttribs3svNV && glVertexAttribs4dvNV && glVertexAttribs4fvNV && glVertexAttribs4svNV && glVertexAttribs4ubvNV) {
			EnableExtension(_GL_NV_vertex_program);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_vertex_program" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_vertex_program implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SGIX_texture_coordinate_clamp"))
	{
		EnableExtension(_GL_SGIX_texture_coordinate_clamp);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SGIX_texture_coordinate_clamp" << std::endl;
	}

	if (QueryExtension("GL_OML_interlace"))
	{
		EnableExtension(_GL_OML_interlace);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_OML_interlace" << std::endl;
	}

	if (QueryExtension("GL_OML_subsample"))
	{
		EnableExtension(_GL_OML_subsample);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_OML_subsample" << std::endl;
	}

	if (QueryExtension("GL_OML_resample"))
	{
		EnableExtension(_GL_OML_resample);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_OML_resample" << std::endl;
	}

	if (QueryExtension("GL_NV_copy_depth_to_color"))
	{
		EnableExtension(_GL_NV_copy_depth_to_color);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_copy_depth_to_color" << std::endl;
	}

#if defined(GL_ATI_envmap_bumpmap)
	if (QueryExtension("GL_ATI_envmap_bumpmap"))
	{
		glTexBumpParameterivATI = reinterpret_cast<PFNGLTEXBUMPPARAMETERIVATIPROC>(bglGetProcAddress((const GLubyte *) "glTexBumpParameterivATI"));
		glTexBumpParameterfvATI = reinterpret_cast<PFNGLTEXBUMPPARAMETERFVATIPROC>(bglGetProcAddress((const GLubyte *) "glTexBumpParameterfvATI"));
		glGetTexBumpParameterivATI = reinterpret_cast<PFNGLGETTEXBUMPPARAMETERIVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetTexBumpParameterivATI"));
		glGetTexBumpParameterfvATI = reinterpret_cast<PFNGLGETTEXBUMPPARAMETERFVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetTexBumpParameterfvATI"));
		if (glTexBumpParameterivATI && glTexBumpParameterfvATI && glGetTexBumpParameterivATI && glGetTexBumpParameterfvATI) {
			EnableExtension(_GL_ATI_envmap_bumpmap);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_envmap_bumpmap" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_envmap_bumpmap implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ATI_fragment_shader)
	if (QueryExtension("GL_ATI_fragment_shader"))
	{
		glGenFragmentShadersATI = reinterpret_cast<PFNGLGENFRAGMENTSHADERSATIPROC>(bglGetProcAddress((const GLubyte *) "glGenFragmentShadersATI"));
		glBindFragmentShaderATI = reinterpret_cast<PFNGLBINDFRAGMENTSHADERATIPROC>(bglGetProcAddress((const GLubyte *) "glBindFragmentShaderATI"));
		glDeleteFragmentShaderATI = reinterpret_cast<PFNGLDELETEFRAGMENTSHADERATIPROC>(bglGetProcAddress((const GLubyte *) "glDeleteFragmentShaderATI"));
		glBeginFragmentShaderATI = reinterpret_cast<PFNGLBEGINFRAGMENTSHADERATIPROC>(bglGetProcAddress((const GLubyte *) "glBeginFragmentShaderATI"));
		glEndFragmentShaderATI = reinterpret_cast<PFNGLENDFRAGMENTSHADERATIPROC>(bglGetProcAddress((const GLubyte *) "glEndFragmentShaderATI"));
		glPassTexCoordATI = reinterpret_cast<PFNGLPASSTEXCOORDATIPROC>(bglGetProcAddress((const GLubyte *) "glPassTexCoordATI"));
		glSampleMapATI = reinterpret_cast<PFNGLSAMPLEMAPATIPROC>(bglGetProcAddress((const GLubyte *) "glSampleMapATI"));
		glColorFragmentOp1ATI = reinterpret_cast<PFNGLCOLORFRAGMENTOP1ATIPROC>(bglGetProcAddress((const GLubyte *) "glColorFragmentOp1ATI"));
		glColorFragmentOp2ATI = reinterpret_cast<PFNGLCOLORFRAGMENTOP2ATIPROC>(bglGetProcAddress((const GLubyte *) "glColorFragmentOp2ATI"));
		glColorFragmentOp3ATI = reinterpret_cast<PFNGLCOLORFRAGMENTOP3ATIPROC>(bglGetProcAddress((const GLubyte *) "glColorFragmentOp3ATI"));
		glAlphaFragmentOp1ATI = reinterpret_cast<PFNGLALPHAFRAGMENTOP1ATIPROC>(bglGetProcAddress((const GLubyte *) "glAlphaFragmentOp1ATI"));
		glAlphaFragmentOp2ATI = reinterpret_cast<PFNGLALPHAFRAGMENTOP2ATIPROC>(bglGetProcAddress((const GLubyte *) "glAlphaFragmentOp2ATI"));
		glAlphaFragmentOp3ATI = reinterpret_cast<PFNGLALPHAFRAGMENTOP3ATIPROC>(bglGetProcAddress((const GLubyte *) "glAlphaFragmentOp3ATI"));
		glSetFragmentShaderConstantATI = reinterpret_cast<PFNGLSETFRAGMENTSHADERCONSTANTATIPROC>(bglGetProcAddress((const GLubyte *) "glSetFragmentShaderConstantATI"));
		if (glGenFragmentShadersATI && glBindFragmentShaderATI && glDeleteFragmentShaderATI && glBeginFragmentShaderATI && glEndFragmentShaderATI && glPassTexCoordATI && glSampleMapATI && glColorFragmentOp1ATI && glColorFragmentOp2ATI && glColorFragmentOp3ATI && glAlphaFragmentOp1ATI && glAlphaFragmentOp2ATI && glAlphaFragmentOp3ATI && glSetFragmentShaderConstantATI) {
			EnableExtension(_GL_ATI_fragment_shader);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_fragment_shader" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_fragment_shader implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ATI_pn_triangles"))
	{
		EnableExtension(_GL_ATI_pn_triangles);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ATI_pn_triangles" << std::endl;
	}

#if defined(GL_ATI_vertex_array_object) && 0
	if (QueryExtension("GL_ATI_vertex_array_object"))
	{
		glNewObjectBufferATI = reinterpret_cast<PFNGLNEWOBJECTBUFFERATIPROC>(bglGetProcAddress((const GLubyte *) "glNewObjectBufferATI"));
		glIsObjectBufferATI = reinterpret_cast<PFNGLISOBJECTBUFFERATIPROC>(bglGetProcAddress((const GLubyte *) "glIsObjectBufferATI"));
		glUpdateObjectBufferATI = reinterpret_cast<PFNGLUPDATEOBJECTBUFFERATIPROC>(bglGetProcAddress((const GLubyte *) "glUpdateObjectBufferATI"));
		glGetObjectBufferfvATI = reinterpret_cast<PFNGLGETOBJECTBUFFERFVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetObjectBufferfvATI"));
		glGetObjectBufferivATI = reinterpret_cast<PFNGLGETOBJECTBUFFERIVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetObjectBufferivATI"));
		glFreeObjectBufferATI = reinterpret_cast<PFNGLFREEOBJECTBUFFERATIPROC>(bglGetProcAddress((const GLubyte *) "glFreeObjectBufferATI"));
		glArrayObjectATI = reinterpret_cast<PFNGLARRAYOBJECTATIPROC>(bglGetProcAddress((const GLubyte *) "glArrayObjectATI"));
		glGetArrayObjectfvATI = reinterpret_cast<PFNGLGETARRAYOBJECTFVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetArrayObjectfvATI"));
		glGetArrayObjectivATI = reinterpret_cast<PFNGLGETARRAYOBJECTIVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetArrayObjectivATI"));
		glVariantArrayObjectATI = reinterpret_cast<PFNGLVARIANTARRAYOBJECTATIPROC>(bglGetProcAddress((const GLubyte *) "glVariantArrayObjectATI"));
		glGetVariantArrayObjectfvATI = reinterpret_cast<PFNGLGETVARIANTARRAYOBJECTFVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetVariantArrayObjectfvATI"));
		glGetVariantArrayObjectivATI = reinterpret_cast<PFNGLGETVARIANTARRAYOBJECTIVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetVariantArrayObjectivATI"));
		if (glNewObjectBufferATI && glIsObjectBufferATI && glUpdateObjectBufferATI && glGetObjectBufferfvATI && glGetObjectBufferivATI && glFreeObjectBufferATI && glArrayObjectATI && glGetArrayObjectfvATI && glGetArrayObjectivATI && glVariantArrayObjectATI && glGetVariantArrayObjectfvATI && glGetVariantArrayObjectivATI) {
			EnableExtension(_GL_ATI_vertex_array_object);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_vertex_array_object" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_vertex_array_object implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_vertex_shader)
	if (QueryExtension("GL_EXT_vertex_shader"))
	{
		glBeginVertexShaderEXT = reinterpret_cast<PFNGLBEGINVERTEXSHADEREXTPROC>(bglGetProcAddress((const GLubyte *) "glBeginVertexShaderEXT"));
		glEndVertexShaderEXT = reinterpret_cast<PFNGLENDVERTEXSHADEREXTPROC>(bglGetProcAddress((const GLubyte *) "glEndVertexShaderEXT"));
		glBindVertexShaderEXT = reinterpret_cast<PFNGLBINDVERTEXSHADEREXTPROC>(bglGetProcAddress((const GLubyte *) "glBindVertexShaderEXT"));
		glGenVertexShadersEXT = reinterpret_cast<PFNGLGENVERTEXSHADERSEXTPROC>(bglGetProcAddress((const GLubyte *) "glGenVertexShadersEXT"));
		glDeleteVertexShaderEXT = reinterpret_cast<PFNGLDELETEVERTEXSHADEREXTPROC>(bglGetProcAddress((const GLubyte *) "glDeleteVertexShaderEXT"));
		glShaderOp1EXT = reinterpret_cast<PFNGLSHADEROP1EXTPROC>(bglGetProcAddress((const GLubyte *) "glShaderOp1EXT"));
		glShaderOp2EXT = reinterpret_cast<PFNGLSHADEROP2EXTPROC>(bglGetProcAddress((const GLubyte *) "glShaderOp2EXT"));
		glShaderOp3EXT = reinterpret_cast<PFNGLSHADEROP3EXTPROC>(bglGetProcAddress((const GLubyte *) "glShaderOp3EXT"));
		glSwizzleEXT = reinterpret_cast<PFNGLSWIZZLEEXTPROC>(bglGetProcAddress((const GLubyte *) "glSwizzleEXT"));
		glWriteMaskEXT = reinterpret_cast<PFNGLWRITEMASKEXTPROC>(bglGetProcAddress((const GLubyte *) "glWriteMaskEXT"));
		glInsertComponentEXT = reinterpret_cast<PFNGLINSERTCOMPONENTEXTPROC>(bglGetProcAddress((const GLubyte *) "glInsertComponentEXT"));
		glExtractComponentEXT = reinterpret_cast<PFNGLEXTRACTCOMPONENTEXTPROC>(bglGetProcAddress((const GLubyte *) "glExtractComponentEXT"));
		glGenSymbolsEXT = reinterpret_cast<PFNGLGENSYMBOLSEXTPROC>(bglGetProcAddress((const GLubyte *) "glGenSymbolsEXT"));
		glSetInvariantEXT = reinterpret_cast<PFNGLSETINVARIANTEXTPROC>(bglGetProcAddress((const GLubyte *) "glSetInvariantEXT"));
		glSetLocalConstantEXT = reinterpret_cast<PFNGLSETLOCALCONSTANTEXTPROC>(bglGetProcAddress((const GLubyte *) "glSetLocalConstantEXT"));
		glVariantbvEXT = reinterpret_cast<PFNGLVARIANTBVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantbvEXT"));
		glVariantsvEXT = reinterpret_cast<PFNGLVARIANTSVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantsvEXT"));
		glVariantivEXT = reinterpret_cast<PFNGLVARIANTIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantivEXT"));
		glVariantfvEXT = reinterpret_cast<PFNGLVARIANTFVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantfvEXT"));
		glVariantdvEXT = reinterpret_cast<PFNGLVARIANTDVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantdvEXT"));
		glVariantubvEXT = reinterpret_cast<PFNGLVARIANTUBVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantubvEXT"));
		glVariantusvEXT = reinterpret_cast<PFNGLVARIANTUSVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantusvEXT"));
		glVariantuivEXT = reinterpret_cast<PFNGLVARIANTUIVEXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantuivEXT"));
		glVariantPointerEXT = reinterpret_cast<PFNGLVARIANTPOINTEREXTPROC>(bglGetProcAddress((const GLubyte *) "glVariantPointerEXT"));
		glEnableVariantClientStateEXT = reinterpret_cast<PFNGLENABLEVARIANTCLIENTSTATEEXTPROC>(bglGetProcAddress((const GLubyte *) "glEnableVariantClientStateEXT"));
		glDisableVariantClientStateEXT = reinterpret_cast<PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC>(bglGetProcAddress((const GLubyte *) "glDisableVariantClientStateEXT"));
		glBindLightParameterEXT = reinterpret_cast<PFNGLBINDLIGHTPARAMETEREXTPROC>(bglGetProcAddress((const GLubyte *) "glBindLightParameterEXT"));
		glBindMaterialParameterEXT = reinterpret_cast<PFNGLBINDMATERIALPARAMETEREXTPROC>(bglGetProcAddress((const GLubyte *) "glBindMaterialParameterEXT"));
		glBindTexGenParameterEXT = reinterpret_cast<PFNGLBINDTEXGENPARAMETEREXTPROC>(bglGetProcAddress((const GLubyte *) "glBindTexGenParameterEXT"));
		glBindTextureUnitParameterEXT = reinterpret_cast<PFNGLBINDTEXTUREUNITPARAMETEREXTPROC>(bglGetProcAddress((const GLubyte *) "glBindTextureUnitParameterEXT"));
		glBindParameterEXT = reinterpret_cast<PFNGLBINDPARAMETEREXTPROC>(bglGetProcAddress((const GLubyte *) "glBindParameterEXT"));
		glIsVariantEnabledEXT = reinterpret_cast<PFNGLISVARIANTENABLEDEXTPROC>(bglGetProcAddress((const GLubyte *) "glIsVariantEnabledEXT"));
		glGetVariantBooleanvEXT = reinterpret_cast<PFNGLGETVARIANTBOOLEANVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetVariantBooleanvEXT"));
		glGetVariantIntegervEXT = reinterpret_cast<PFNGLGETVARIANTINTEGERVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetVariantIntegervEXT"));
		glGetVariantFloatvEXT = reinterpret_cast<PFNGLGETVARIANTFLOATVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetVariantFloatvEXT"));
		glGetVariantPointervEXT = reinterpret_cast<PFNGLGETVARIANTPOINTERVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetVariantPointervEXT"));
		glGetInvariantBooleanvEXT = reinterpret_cast<PFNGLGETINVARIANTBOOLEANVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetInvariantBooleanvEXT"));
		glGetInvariantIntegervEXT = reinterpret_cast<PFNGLGETINVARIANTINTEGERVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetInvariantIntegervEXT"));
		glGetInvariantFloatvEXT = reinterpret_cast<PFNGLGETINVARIANTFLOATVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetInvariantFloatvEXT"));
		glGetLocalConstantBooleanvEXT = reinterpret_cast<PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetLocalConstantBooleanvEXT"));
		glGetLocalConstantIntegervEXT = reinterpret_cast<PFNGLGETLOCALCONSTANTINTEGERVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetLocalConstantIntegervEXT"));
		glGetLocalConstantFloatvEXT = reinterpret_cast<PFNGLGETLOCALCONSTANTFLOATVEXTPROC>(bglGetProcAddress((const GLubyte *) "glGetLocalConstantFloatvEXT"));
		if (glBeginVertexShaderEXT && glEndVertexShaderEXT && glBindVertexShaderEXT && glGenVertexShadersEXT && glDeleteVertexShaderEXT && glShaderOp1EXT && glShaderOp2EXT && glShaderOp3EXT && glSwizzleEXT && glWriteMaskEXT && glInsertComponentEXT && glExtractComponentEXT && glGenSymbolsEXT && glSetInvariantEXT && glSetLocalConstantEXT && glVariantbvEXT && glVariantsvEXT && glVariantivEXT && glVariantfvEXT && glVariantdvEXT && glVariantubvEXT && glVariantusvEXT && glVariantuivEXT && glVariantPointerEXT && glEnableVariantClientStateEXT && glDisableVariantClientStateEXT && glBindLightParameterEXT && glBindMaterialParameterEXT && glBindTexGenParameterEXT && glBindTextureUnitParameterEXT && glBindParameterEXT && glIsVariantEnabledEXT && glGetVariantBooleanvEXT && glGetVariantIntegervEXT && glGetVariantFloatvEXT && glGetVariantPointervEXT && glGetInvariantBooleanvEXT && glGetInvariantIntegervEXT && glGetInvariantFloatvEXT && glGetLocalConstantBooleanvEXT && glGetLocalConstantIntegervEXT && glGetLocalConstantFloatvEXT) {
			EnableExtension(_GL_EXT_vertex_shader);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_vertex_shader" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_vertex_shader implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ATI_vertex_streams)
	if (QueryExtension("GL_ATI_vertex_streams"))
	{
		glVertexStream1sATI = reinterpret_cast<PFNGLVERTEXSTREAM1SATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream1sATI"));
		glVertexStream1svATI = reinterpret_cast<PFNGLVERTEXSTREAM1SVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream1svATI"));
		glVertexStream1iATI = reinterpret_cast<PFNGLVERTEXSTREAM1IATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream1iATI"));
		glVertexStream1ivATI = reinterpret_cast<PFNGLVERTEXSTREAM1IVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream1ivATI"));
		glVertexStream1fATI = reinterpret_cast<PFNGLVERTEXSTREAM1FATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream1fATI"));
		glVertexStream1fvATI = reinterpret_cast<PFNGLVERTEXSTREAM1FVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream1fvATI"));
		glVertexStream1dATI = reinterpret_cast<PFNGLVERTEXSTREAM1DATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream1dATI"));
		glVertexStream1dvATI = reinterpret_cast<PFNGLVERTEXSTREAM1DVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream1dvATI"));
		glVertexStream2sATI = reinterpret_cast<PFNGLVERTEXSTREAM2SATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream2sATI"));
		glVertexStream2svATI = reinterpret_cast<PFNGLVERTEXSTREAM2SVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream2svATI"));
		glVertexStream2iATI = reinterpret_cast<PFNGLVERTEXSTREAM2IATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream2iATI"));
		glVertexStream2ivATI = reinterpret_cast<PFNGLVERTEXSTREAM2IVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream2ivATI"));
		glVertexStream2fATI = reinterpret_cast<PFNGLVERTEXSTREAM2FATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream2fATI"));
		glVertexStream2fvATI = reinterpret_cast<PFNGLVERTEXSTREAM2FVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream2fvATI"));
		glVertexStream2dATI = reinterpret_cast<PFNGLVERTEXSTREAM2DATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream2dATI"));
		glVertexStream2dvATI = reinterpret_cast<PFNGLVERTEXSTREAM2DVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream2dvATI"));
		glVertexStream3sATI = reinterpret_cast<PFNGLVERTEXSTREAM3SATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream3sATI"));
		glVertexStream3svATI = reinterpret_cast<PFNGLVERTEXSTREAM3SVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream3svATI"));
		glVertexStream3iATI = reinterpret_cast<PFNGLVERTEXSTREAM3IATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream3iATI"));
		glVertexStream3ivATI = reinterpret_cast<PFNGLVERTEXSTREAM3IVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream3ivATI"));
		glVertexStream3fATI = reinterpret_cast<PFNGLVERTEXSTREAM3FATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream3fATI"));
		glVertexStream3fvATI = reinterpret_cast<PFNGLVERTEXSTREAM3FVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream3fvATI"));
		glVertexStream3dATI = reinterpret_cast<PFNGLVERTEXSTREAM3DATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream3dATI"));
		glVertexStream3dvATI = reinterpret_cast<PFNGLVERTEXSTREAM3DVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream3dvATI"));
		glVertexStream4sATI = reinterpret_cast<PFNGLVERTEXSTREAM4SATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream4sATI"));
		glVertexStream4svATI = reinterpret_cast<PFNGLVERTEXSTREAM4SVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream4svATI"));
		glVertexStream4iATI = reinterpret_cast<PFNGLVERTEXSTREAM4IATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream4iATI"));
		glVertexStream4ivATI = reinterpret_cast<PFNGLVERTEXSTREAM4IVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream4ivATI"));
		glVertexStream4fATI = reinterpret_cast<PFNGLVERTEXSTREAM4FATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream4fATI"));
		glVertexStream4fvATI = reinterpret_cast<PFNGLVERTEXSTREAM4FVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream4fvATI"));
		glVertexStream4dATI = reinterpret_cast<PFNGLVERTEXSTREAM4DATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream4dATI"));
		glVertexStream4dvATI = reinterpret_cast<PFNGLVERTEXSTREAM4DVATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexStream4dvATI"));
		glNormalStream3bATI = reinterpret_cast<PFNGLNORMALSTREAM3BATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3bATI"));
		glNormalStream3bvATI = reinterpret_cast<PFNGLNORMALSTREAM3BVATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3bvATI"));
		glNormalStream3sATI = reinterpret_cast<PFNGLNORMALSTREAM3SATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3sATI"));
		glNormalStream3svATI = reinterpret_cast<PFNGLNORMALSTREAM3SVATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3svATI"));
		glNormalStream3iATI = reinterpret_cast<PFNGLNORMALSTREAM3IATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3iATI"));
		glNormalStream3ivATI = reinterpret_cast<PFNGLNORMALSTREAM3IVATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3ivATI"));
		glNormalStream3fATI = reinterpret_cast<PFNGLNORMALSTREAM3FATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3fATI"));
		glNormalStream3fvATI = reinterpret_cast<PFNGLNORMALSTREAM3FVATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3fvATI"));
		glNormalStream3dATI = reinterpret_cast<PFNGLNORMALSTREAM3DATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3dATI"));
		glNormalStream3dvATI = reinterpret_cast<PFNGLNORMALSTREAM3DVATIPROC>(bglGetProcAddress((const GLubyte *) "glNormalStream3dvATI"));
		glClientActiveVertexStreamATI = reinterpret_cast<PFNGLCLIENTACTIVEVERTEXSTREAMATIPROC>(bglGetProcAddress((const GLubyte *) "glClientActiveVertexStreamATI"));
		glVertexBlendEnviATI = reinterpret_cast<PFNGLVERTEXBLENDENVIATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexBlendEnviATI"));
		glVertexBlendEnvfATI = reinterpret_cast<PFNGLVERTEXBLENDENVFATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexBlendEnvfATI"));
		if (glVertexStream1sATI && glVertexStream1svATI && glVertexStream1iATI && glVertexStream1ivATI && glVertexStream1fATI && glVertexStream1fvATI && glVertexStream1dATI && glVertexStream1dvATI && glVertexStream2sATI && glVertexStream2svATI && glVertexStream2iATI && glVertexStream2ivATI && glVertexStream2fATI && glVertexStream2fvATI && glVertexStream2dATI && glVertexStream2dvATI && glVertexStream3sATI && glVertexStream3svATI && glVertexStream3iATI && glVertexStream3ivATI && glVertexStream3fATI && glVertexStream3fvATI && glVertexStream3dATI && glVertexStream3dvATI && glVertexStream4sATI && glVertexStream4svATI && glVertexStream4iATI && glVertexStream4ivATI && glVertexStream4fATI && glVertexStream4fvATI && glVertexStream4dATI && glVertexStream4dvATI && glNormalStream3bATI && glNormalStream3bvATI && glNormalStream3sATI && glNormalStream3svATI && glNormalStream3iATI && glNormalStream3ivATI && glNormalStream3fATI && glNormalStream3fvATI && glNormalStream3dATI && glNormalStream3dvATI && glClientActiveVertexStreamATI && glVertexBlendEnviATI && glVertexBlendEnvfATI) {
			EnableExtension(_GL_ATI_vertex_streams);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_vertex_streams" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_vertex_streams implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ATI_element_array)
	if (QueryExtension("GL_ATI_element_array"))
	{
		glElementPointerATI = reinterpret_cast<PFNGLELEMENTPOINTERATIPROC>(bglGetProcAddress((const GLubyte *) "glElementPointerATI"));
		glDrawElementArrayATI = reinterpret_cast<PFNGLDRAWELEMENTARRAYATIPROC>(bglGetProcAddress((const GLubyte *) "glDrawElementArrayATI"));
		glDrawRangeElementArrayATI = reinterpret_cast<PFNGLDRAWRANGEELEMENTARRAYATIPROC>(bglGetProcAddress((const GLubyte *) "glDrawRangeElementArrayATI"));
		if (glElementPointerATI && glDrawElementArrayATI && glDrawRangeElementArrayATI) {
			EnableExtension(_GL_ATI_element_array);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_element_array" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_element_array implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_SUN_mesh_array)
	if (QueryExtension("GL_SUN_mesh_array"))
	{
		glDrawMeshArraysSUN = reinterpret_cast<PFNGLDRAWMESHARRAYSSUNPROC>(bglGetProcAddress((const GLubyte *) "glDrawMeshArraysSUN"));
		if (glDrawMeshArraysSUN) {
			EnableExtension(_GL_SUN_mesh_array);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_SUN_mesh_array" << std::endl;
		} else {
			std::cout << "ERROR: GL_SUN_mesh_array implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_SUN_slice_accum"))
	{
		EnableExtension(_GL_SUN_slice_accum);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_SUN_slice_accum" << std::endl;
	}

	if (QueryExtension("GL_NV_multisample_filter_hint"))
	{
		EnableExtension(_GL_NV_multisample_filter_hint);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_multisample_filter_hint" << std::endl;
	}

	if (QueryExtension("GL_NV_depth_clamp"))
	{
		EnableExtension(_GL_NV_depth_clamp);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_depth_clamp" << std::endl;
	}

#if defined(GL_NV_occlusion_query)
	if (QueryExtension("GL_NV_occlusion_query"))
	{
		glGenOcclusionQueriesNV = reinterpret_cast<PFNGLGENOCCLUSIONQUERIESNVPROC>(bglGetProcAddress((const GLubyte *) "glGenOcclusionQueriesNV"));
		glDeleteOcclusionQueriesNV = reinterpret_cast<PFNGLDELETEOCCLUSIONQUERIESNVPROC>(bglGetProcAddress((const GLubyte *) "glDeleteOcclusionQueriesNV"));
		glIsOcclusionQueryNV = reinterpret_cast<PFNGLISOCCLUSIONQUERYNVPROC>(bglGetProcAddress((const GLubyte *) "glIsOcclusionQueryNV"));
		glBeginOcclusionQueryNV = reinterpret_cast<PFNGLBEGINOCCLUSIONQUERYNVPROC>(bglGetProcAddress((const GLubyte *) "glBeginOcclusionQueryNV"));
		glEndOcclusionQueryNV = reinterpret_cast<PFNGLENDOCCLUSIONQUERYNVPROC>(bglGetProcAddress((const GLubyte *) "glEndOcclusionQueryNV"));
		glGetOcclusionQueryivNV = reinterpret_cast<PFNGLGETOCCLUSIONQUERYIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetOcclusionQueryivNV"));
		glGetOcclusionQueryuivNV = reinterpret_cast<PFNGLGETOCCLUSIONQUERYUIVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetOcclusionQueryuivNV"));
		if (glGenOcclusionQueriesNV && glDeleteOcclusionQueriesNV && glIsOcclusionQueryNV && glBeginOcclusionQueryNV && glEndOcclusionQueryNV && glGetOcclusionQueryivNV && glGetOcclusionQueryuivNV) {
			EnableExtension(_GL_NV_occlusion_query);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_occlusion_query" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_occlusion_query implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_NV_point_sprite)
	if (QueryExtension("GL_NV_point_sprite"))
	{
		glPointParameteriNV = reinterpret_cast<PFNGLPOINTPARAMETERINVPROC>(bglGetProcAddress((const GLubyte *) "glPointParameteriNV"));
		glPointParameterivNV = reinterpret_cast<PFNGLPOINTPARAMETERIVNVPROC>(bglGetProcAddress((const GLubyte *) "glPointParameterivNV"));
		if (glPointParameteriNV && glPointParameterivNV) {
			EnableExtension(_GL_NV_point_sprite);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_point_sprite" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_point_sprite implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_NV_texture_shader3"))
	{
		EnableExtension(_GL_NV_texture_shader3);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texture_shader3" << std::endl;
	}

	if (QueryExtension("GL_NV_vertex_program1_1"))
	{
		EnableExtension(_GL_NV_vertex_program1_1);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_vertex_program1_1" << std::endl;
	}

	if (QueryExtension("GL_EXT_shadow_funcs"))
	{
		EnableExtension(_GL_EXT_shadow_funcs);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_shadow_funcs" << std::endl;
	}

#if defined(GL_EXT_stencil_two_side)
	if (QueryExtension("GL_EXT_stencil_two_side"))
	{
		glActiveStencilFaceEXT = reinterpret_cast<PFNGLACTIVESTENCILFACEEXTPROC>(bglGetProcAddress((const GLubyte *) "glActiveStencilFaceEXT"));
		if (glActiveStencilFaceEXT) {
			EnableExtension(_GL_EXT_stencil_two_side);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_stencil_two_side" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_stencil_two_side implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ATI_text_fragment_shader"))
	{
		EnableExtension(_GL_ATI_text_fragment_shader);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ATI_text_fragment_shader" << std::endl;
	}

	if (QueryExtension("GL_APPLE_client_storage"))
	{
		EnableExtension(_GL_APPLE_client_storage);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_APPLE_client_storage" << std::endl;
	}

#if defined(GL_APPLE_element_array)
	if (QueryExtension("GL_APPLE_element_array"))
	{
		glElementPointerAPPLE = reinterpret_cast<PFNGLELEMENTPOINTERAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glElementPointerAPPLE"));
		glDrawElementArrayAPPLE = reinterpret_cast<PFNGLDRAWELEMENTARRAYAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glDrawElementArrayAPPLE"));
		glDrawRangeElementArrayAPPLE = reinterpret_cast<PFNGLDRAWRANGEELEMENTARRAYAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glDrawRangeElementArrayAPPLE"));
		glMultiDrawElementArrayAPPLE = reinterpret_cast<PFNGLMULTIDRAWELEMENTARRAYAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glMultiDrawElementArrayAPPLE"));
		glMultiDrawRangeElementArrayAPPLE = reinterpret_cast<PFNGLMULTIDRAWRANGEELEMENTARRAYAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glMultiDrawRangeElementArrayAPPLE"));
		if (glElementPointerAPPLE && glDrawElementArrayAPPLE && glDrawRangeElementArrayAPPLE && glMultiDrawElementArrayAPPLE && glMultiDrawRangeElementArrayAPPLE) {
			EnableExtension(_GL_APPLE_element_array);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_APPLE_element_array" << std::endl;
		} else {
			std::cout << "ERROR: GL_APPLE_element_array implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_APPLE_fence)
	if (QueryExtension("GL_APPLE_fence"))
	{
		glGenFencesAPPLE = reinterpret_cast<PFNGLGENFENCESAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glGenFencesAPPLE"));
		glDeleteFencesAPPLE = reinterpret_cast<PFNGLDELETEFENCESAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glDeleteFencesAPPLE"));
		glSetFenceAPPLE = reinterpret_cast<PFNGLSETFENCEAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glSetFenceAPPLE"));
		glIsFenceAPPLE = reinterpret_cast<PFNGLISFENCEAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glIsFenceAPPLE"));
		glTestFenceAPPLE = reinterpret_cast<PFNGLTESTFENCEAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glTestFenceAPPLE"));
		glFinishFenceAPPLE = reinterpret_cast<PFNGLFINISHFENCEAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glFinishFenceAPPLE"));
		glTestObjectAPPLE = reinterpret_cast<PFNGLTESTOBJECTAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glTestObjectAPPLE"));
		glFinishObjectAPPLE = reinterpret_cast<PFNGLFINISHOBJECTAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glFinishObjectAPPLE"));
		if (glGenFencesAPPLE && glDeleteFencesAPPLE && glSetFenceAPPLE && glIsFenceAPPLE && glTestFenceAPPLE && glFinishFenceAPPLE && glTestObjectAPPLE && glFinishObjectAPPLE) {
			EnableExtension(_GL_APPLE_fence);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_APPLE_fence" << std::endl;
		} else {
			std::cout << "ERROR: GL_APPLE_fence implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_APPLE_vertex_array_object)
	if (QueryExtension("GL_APPLE_vertex_array_object"))
	{
		glBindVertexArrayAPPLE = reinterpret_cast<PFNGLBINDVERTEXARRAYAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glBindVertexArrayAPPLE"));
		glDeleteVertexArraysAPPLE = reinterpret_cast<PFNGLDELETEVERTEXARRAYSAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glDeleteVertexArraysAPPLE"));
		glGenVertexArraysAPPLE = reinterpret_cast<PFNGLGENVERTEXARRAYSAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glGenVertexArraysAPPLE"));
		glIsVertexArrayAPPLE = reinterpret_cast<PFNGLISVERTEXARRAYAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glIsVertexArrayAPPLE"));
		if (glBindVertexArrayAPPLE && glDeleteVertexArraysAPPLE && glGenVertexArraysAPPLE && glIsVertexArrayAPPLE) {
			EnableExtension(_GL_APPLE_vertex_array_object);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_APPLE_vertex_array_object" << std::endl;
		} else {
			std::cout << "ERROR: GL_APPLE_vertex_array_object implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_APPLE_vertex_array_range)
	if (QueryExtension("GL_APPLE_vertex_array_range"))
	{
		glVertexArrayRangeAPPLE = reinterpret_cast<PFNGLVERTEXARRAYRANGEAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glVertexArrayRangeAPPLE"));
		glFlushVertexArrayRangeAPPLE = reinterpret_cast<PFNGLFLUSHVERTEXARRAYRANGEAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glFlushVertexArrayRangeAPPLE"));
		glVertexArrayParameteriAPPLE = reinterpret_cast<PFNGLVERTEXARRAYPARAMETERIAPPLEPROC>(bglGetProcAddress((const GLubyte *) "glVertexArrayParameteriAPPLE"));
		if (glVertexArrayRangeAPPLE && glFlushVertexArrayRangeAPPLE && glVertexArrayParameteriAPPLE) {
			EnableExtension(_GL_APPLE_vertex_array_range);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_APPLE_vertex_array_range" << std::endl;
		} else {
			std::cout << "ERROR: GL_APPLE_vertex_array_range implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_APPLE_ycbcr_422"))
	{
		EnableExtension(_GL_APPLE_ycbcr_422);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_APPLE_ycbcr_422" << std::endl;
	}

	if (QueryExtension("GL_S3_s3tc"))
	{
		EnableExtension(_GL_S3_s3tc);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_S3_s3tc" << std::endl;
	}

#if defined(GL_ATI_draw_buffers)
	if (QueryExtension("GL_ATI_draw_buffers"))
	{
		glDrawBuffersATI = reinterpret_cast<PFNGLDRAWBUFFERSATIPROC>(bglGetProcAddress((const GLubyte *) "glDrawBuffersATI"));
		if (glDrawBuffersATI) {
			EnableExtension(_GL_ATI_draw_buffers);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_draw_buffers" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_draw_buffers implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_ATI_texture_env_combine3"))
	{
		EnableExtension(_GL_ATI_texture_env_combine3);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ATI_texture_env_combine3" << std::endl;
	}

	if (QueryExtension("GL_ATI_texture_float"))
	{
		EnableExtension(_GL_ATI_texture_float);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_ATI_texture_float" << std::endl;
	}

	if (QueryExtension("GL_NV_float_buffer"))
	{
		EnableExtension(_GL_NV_float_buffer);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_float_buffer" << std::endl;
	}

#if defined(GL_NV_fragment_program)
	if (QueryExtension("GL_NV_fragment_program"))
	{
		glProgramNamedParameter4fNV = reinterpret_cast<PFNGLPROGRAMNAMEDPARAMETER4FNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramNamedParameter4fNV"));
		glProgramNamedParameter4dNV = reinterpret_cast<PFNGLPROGRAMNAMEDPARAMETER4DNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramNamedParameter4dNV"));
		glProgramNamedParameter4fvNV = reinterpret_cast<PFNGLPROGRAMNAMEDPARAMETER4FVNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramNamedParameter4fvNV"));
		glProgramNamedParameter4dvNV = reinterpret_cast<PFNGLPROGRAMNAMEDPARAMETER4DVNVPROC>(bglGetProcAddress((const GLubyte *) "glProgramNamedParameter4dvNV"));
		glGetProgramNamedParameterfvNV = reinterpret_cast<PFNGLGETPROGRAMNAMEDPARAMETERFVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramNamedParameterfvNV"));
		glGetProgramNamedParameterdvNV = reinterpret_cast<PFNGLGETPROGRAMNAMEDPARAMETERDVNVPROC>(bglGetProcAddress((const GLubyte *) "glGetProgramNamedParameterdvNV"));
		if (glProgramNamedParameter4fNV && glProgramNamedParameter4dNV && glProgramNamedParameter4fvNV && glProgramNamedParameter4dvNV && glGetProgramNamedParameterfvNV && glGetProgramNamedParameterdvNV) {
			EnableExtension(_GL_NV_fragment_program);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_fragment_program" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_fragment_program implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_NV_half_float)
	if (QueryExtension("GL_NV_half_float"))
	{
		glVertex2hNV = reinterpret_cast<PFNGLVERTEX2HNVPROC>(bglGetProcAddress((const GLubyte *) "glVertex2hNV"));
		glVertex2hvNV = reinterpret_cast<PFNGLVERTEX2HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertex2hvNV"));
		glVertex3hNV = reinterpret_cast<PFNGLVERTEX3HNVPROC>(bglGetProcAddress((const GLubyte *) "glVertex3hNV"));
		glVertex3hvNV = reinterpret_cast<PFNGLVERTEX3HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertex3hvNV"));
		glVertex4hNV = reinterpret_cast<PFNGLVERTEX4HNVPROC>(bglGetProcAddress((const GLubyte *) "glVertex4hNV"));
		glVertex4hvNV = reinterpret_cast<PFNGLVERTEX4HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertex4hvNV"));
		glNormal3hNV = reinterpret_cast<PFNGLNORMAL3HNVPROC>(bglGetProcAddress((const GLubyte *) "glNormal3hNV"));
		glNormal3hvNV = reinterpret_cast<PFNGLNORMAL3HVNVPROC>(bglGetProcAddress((const GLubyte *) "glNormal3hvNV"));
		glColor3hNV = reinterpret_cast<PFNGLCOLOR3HNVPROC>(bglGetProcAddress((const GLubyte *) "glColor3hNV"));
		glColor3hvNV = reinterpret_cast<PFNGLCOLOR3HVNVPROC>(bglGetProcAddress((const GLubyte *) "glColor3hvNV"));
		glColor4hNV = reinterpret_cast<PFNGLCOLOR4HNVPROC>(bglGetProcAddress((const GLubyte *) "glColor4hNV"));
		glColor4hvNV = reinterpret_cast<PFNGLCOLOR4HVNVPROC>(bglGetProcAddress((const GLubyte *) "glColor4hvNV"));
		glTexCoord1hNV = reinterpret_cast<PFNGLTEXCOORD1HNVPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord1hNV"));
		glTexCoord1hvNV = reinterpret_cast<PFNGLTEXCOORD1HVNVPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord1hvNV"));
		glTexCoord2hNV = reinterpret_cast<PFNGLTEXCOORD2HNVPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2hNV"));
		glTexCoord2hvNV = reinterpret_cast<PFNGLTEXCOORD2HVNVPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord2hvNV"));
		glTexCoord3hNV = reinterpret_cast<PFNGLTEXCOORD3HNVPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord3hNV"));
		glTexCoord3hvNV = reinterpret_cast<PFNGLTEXCOORD3HVNVPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord3hvNV"));
		glTexCoord4hNV = reinterpret_cast<PFNGLTEXCOORD4HNVPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord4hNV"));
		glTexCoord4hvNV = reinterpret_cast<PFNGLTEXCOORD4HVNVPROC>(bglGetProcAddress((const GLubyte *) "glTexCoord4hvNV"));
		glMultiTexCoord1hNV = reinterpret_cast<PFNGLMULTITEXCOORD1HNVPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1hNV"));
		glMultiTexCoord1hvNV = reinterpret_cast<PFNGLMULTITEXCOORD1HVNVPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1hvNV"));
		glMultiTexCoord2hNV = reinterpret_cast<PFNGLMULTITEXCOORD2HNVPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2hNV"));
		glMultiTexCoord2hvNV = reinterpret_cast<PFNGLMULTITEXCOORD2HVNVPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2hvNV"));
		glMultiTexCoord3hNV = reinterpret_cast<PFNGLMULTITEXCOORD3HNVPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3hNV"));
		glMultiTexCoord3hvNV = reinterpret_cast<PFNGLMULTITEXCOORD3HVNVPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3hvNV"));
		glMultiTexCoord4hNV = reinterpret_cast<PFNGLMULTITEXCOORD4HNVPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4hNV"));
		glMultiTexCoord4hvNV = reinterpret_cast<PFNGLMULTITEXCOORD4HVNVPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4hvNV"));
		glFogCoordhNV = reinterpret_cast<PFNGLFOGCOORDHNVPROC>(bglGetProcAddress((const GLubyte *) "glFogCoordhNV"));
		glFogCoordhvNV = reinterpret_cast<PFNGLFOGCOORDHVNVPROC>(bglGetProcAddress((const GLubyte *) "glFogCoordhvNV"));
		glSecondaryColor3hNV = reinterpret_cast<PFNGLSECONDARYCOLOR3HNVPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3hNV"));
		glSecondaryColor3hvNV = reinterpret_cast<PFNGLSECONDARYCOLOR3HVNVPROC>(bglGetProcAddress((const GLubyte *) "glSecondaryColor3hvNV"));
		glVertexWeighthNV = reinterpret_cast<PFNGLVERTEXWEIGHTHNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexWeighthNV"));
		glVertexWeighthvNV = reinterpret_cast<PFNGLVERTEXWEIGHTHVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexWeighthvNV"));
		glVertexAttrib1hNV = reinterpret_cast<PFNGLVERTEXATTRIB1HNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1hNV"));
		glVertexAttrib1hvNV = reinterpret_cast<PFNGLVERTEXATTRIB1HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1hvNV"));
		glVertexAttrib2hNV = reinterpret_cast<PFNGLVERTEXATTRIB2HNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2hNV"));
		glVertexAttrib2hvNV = reinterpret_cast<PFNGLVERTEXATTRIB2HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2hvNV"));
		glVertexAttrib3hNV = reinterpret_cast<PFNGLVERTEXATTRIB3HNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3hNV"));
		glVertexAttrib3hvNV = reinterpret_cast<PFNGLVERTEXATTRIB3HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3hvNV"));
		glVertexAttrib4hNV = reinterpret_cast<PFNGLVERTEXATTRIB4HNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4hNV"));
		glVertexAttrib4hvNV = reinterpret_cast<PFNGLVERTEXATTRIB4HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4hvNV"));
		glVertexAttribs1hvNV = reinterpret_cast<PFNGLVERTEXATTRIBS1HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs1hvNV"));
		glVertexAttribs2hvNV = reinterpret_cast<PFNGLVERTEXATTRIBS2HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs2hvNV"));
		glVertexAttribs3hvNV = reinterpret_cast<PFNGLVERTEXATTRIBS3HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs3hvNV"));
		glVertexAttribs4hvNV = reinterpret_cast<PFNGLVERTEXATTRIBS4HVNVPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribs4hvNV"));
		if (glVertex2hNV && glVertex2hvNV && glVertex3hNV && glVertex3hvNV && glVertex4hNV && glVertex4hvNV && glNormal3hNV && glNormal3hvNV && glColor3hNV && glColor3hvNV && glColor4hNV && glColor4hvNV && glTexCoord1hNV && glTexCoord1hvNV && glTexCoord2hNV && glTexCoord2hvNV && glTexCoord3hNV && glTexCoord3hvNV && glTexCoord4hNV && glTexCoord4hvNV && glMultiTexCoord1hNV && glMultiTexCoord1hvNV && glMultiTexCoord2hNV && glMultiTexCoord2hvNV && glMultiTexCoord3hNV && glMultiTexCoord3hvNV && glMultiTexCoord4hNV && glMultiTexCoord4hvNV && glFogCoordhNV && glFogCoordhvNV && glSecondaryColor3hNV && glSecondaryColor3hvNV && glVertexWeighthNV && glVertexWeighthvNV && glVertexAttrib1hNV && glVertexAttrib1hvNV && glVertexAttrib2hNV && glVertexAttrib2hvNV && glVertexAttrib3hNV && glVertexAttrib3hvNV && glVertexAttrib4hNV && glVertexAttrib4hvNV && glVertexAttribs1hvNV && glVertexAttribs2hvNV && glVertexAttribs3hvNV && glVertexAttribs4hvNV) {
			EnableExtension(_GL_NV_half_float);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_half_float" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_half_float implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_NV_pixel_data_range)
	if (QueryExtension("GL_NV_pixel_data_range"))
	{
		glPixelDataRangeNV = reinterpret_cast<PFNGLPIXELDATARANGENVPROC>(bglGetProcAddress((const GLubyte *) "glPixelDataRangeNV"));
		glFlushPixelDataRangeNV = reinterpret_cast<PFNGLFLUSHPIXELDATARANGENVPROC>(bglGetProcAddress((const GLubyte *) "glFlushPixelDataRangeNV"));
		if (glPixelDataRangeNV && glFlushPixelDataRangeNV) {
			EnableExtension(_GL_NV_pixel_data_range);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_pixel_data_range" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_pixel_data_range implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_NV_primitive_restart)
	if (QueryExtension("GL_NV_primitive_restart"))
	{
		glPrimitiveRestartNV = reinterpret_cast<PFNGLPRIMITIVERESTARTNVPROC>(bglGetProcAddress((const GLubyte *) "glPrimitiveRestartNV"));
		glPrimitiveRestartIndexNV = reinterpret_cast<PFNGLPRIMITIVERESTARTINDEXNVPROC>(bglGetProcAddress((const GLubyte *) "glPrimitiveRestartIndexNV"));
		if (glPrimitiveRestartNV && glPrimitiveRestartIndexNV) {
			EnableExtension(_GL_NV_primitive_restart);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_NV_primitive_restart" << std::endl;
		} else {
			std::cout << "ERROR: GL_NV_primitive_restart implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_NV_texture_expand_normal"))
	{
		EnableExtension(_GL_NV_texture_expand_normal);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_texture_expand_normal" << std::endl;
	}

	if (QueryExtension("GL_NV_vertex_program2"))
	{
		EnableExtension(_GL_NV_vertex_program2);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_NV_vertex_program2" << std::endl;
	}

#if defined(GL_ATI_map_object_buffer)
	if (QueryExtension("GL_ATI_map_object_buffer"))
	{
		glMapObjectBufferATI = reinterpret_cast<PFNGLMAPOBJECTBUFFERATIPROC>(bglGetProcAddress((const GLubyte *) "glMapObjectBufferATI"));
		glUnmapObjectBufferATI = reinterpret_cast<PFNGLUNMAPOBJECTBUFFERATIPROC>(bglGetProcAddress((const GLubyte *) "glUnmapObjectBufferATI"));
		if (glMapObjectBufferATI && glUnmapObjectBufferATI) {
			EnableExtension(_GL_ATI_map_object_buffer);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_map_object_buffer" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_map_object_buffer implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ATI_separate_stencil)
	if (QueryExtension("GL_ATI_separate_stencil"))
	{
		glStencilOpSeparateATI = reinterpret_cast<PFNGLSTENCILOPSEPARATEATIPROC>(bglGetProcAddress((const GLubyte *) "glStencilOpSeparateATI"));
		glStencilFuncSeparateATI = reinterpret_cast<PFNGLSTENCILFUNCSEPARATEATIPROC>(bglGetProcAddress((const GLubyte *) "glStencilFuncSeparateATI"));
		if (glStencilOpSeparateATI && glStencilFuncSeparateATI) {
			EnableExtension(_GL_ATI_separate_stencil);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_separate_stencil" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_separate_stencil implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_ATI_vertex_attrib_array_object)
	if (QueryExtension("GL_ATI_vertex_attrib_array_object"))
	{
		glVertexAttribArrayObjectATI = reinterpret_cast<PFNGLVERTEXATTRIBARRAYOBJECTATIPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttribArrayObjectATI"));
		glGetVertexAttribArrayObjectfvATI = reinterpret_cast<PFNGLGETVERTEXATTRIBARRAYOBJECTFVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribArrayObjectfvATI"));
		glGetVertexAttribArrayObjectivATI = reinterpret_cast<PFNGLGETVERTEXATTRIBARRAYOBJECTIVATIPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribArrayObjectivATI"));
		if (glVertexAttribArrayObjectATI && glGetVertexAttribArrayObjectfvATI && glGetVertexAttribArrayObjectivATI) {
			EnableExtension(_GL_ATI_vertex_attrib_array_object);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_ATI_vertex_attrib_array_object" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_vertex_attrib_array_object implementation is broken!" << std::endl;
		}
	}
#endif

#if defined(GL_EXT_depth_bounds_test)
	if (QueryExtension("GL_EXT_depth_bounds_test"))
	{
		glDepthBoundsEXT = reinterpret_cast<PFNGLDEPTHBOUNDSEXTPROC>(bglGetProcAddress((const GLubyte *) "glDepthBoundsEXT"));
		if (glDepthBoundsEXT) {
			EnableExtension(_GL_EXT_depth_bounds_test);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_depth_bounds_test" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_depth_bounds_test implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_EXT_texture_mirror_clamp"))
	{
		EnableExtension(_GL_EXT_texture_mirror_clamp);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_EXT_texture_mirror_clamp" << std::endl;
	}

#if defined(GL_EXT_blend_equation_separate)
	if (QueryExtension("GL_EXT_blend_equation_separate"))
	{
		glBlendEquationSeparateEXT = reinterpret_cast<PFNGLBLENDEQUATIONSEPARATEEXTPROC>(bglGetProcAddress((const GLubyte *) "glBlendEquationSeparateEXT"));
		if (glBlendEquationSeparateEXT) {
			EnableExtension(_GL_EXT_blend_equation_separate);
			if (m_debug && doDebugMessages)
				std::cout << "Enabled GL_EXT_blend_equation_separate" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_blend_equation_separate implementation is broken!" << std::endl;
		}
	}
#endif

	if (QueryExtension("GL_MESA_pack_invert"))
	{
		EnableExtension(_GL_MESA_pack_invert);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_MESA_pack_invert" << std::endl;
	}

	if (QueryExtension("GL_MESA_ycbcr_texture"))
	{
		EnableExtension(_GL_MESA_ycbcr_texture);
		if (m_debug && doDebugMessages)
			std::cout << "Enabled GL_MESA_ycbcr_texture" << std::endl;
	}


	/* End mkglext.py */
	doDebugMessages = false;
}

