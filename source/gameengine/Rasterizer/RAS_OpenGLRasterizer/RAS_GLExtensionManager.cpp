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
#include <bitset>

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
#if defined(BGL_NO_EXTENSIONS)
static void bglInitEntryPoints (void) {}
static void bglDeallocEntryPoints (void) {}

static void *bglGetProcAddress(const GLubyte* entry)
{
	/* No Extensions! */
	return NULL;
}
#elif defined(__APPLE__)
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
        DebugStr ((unsigned char *)"\pCould not find frameworks folder");
        return err;
    }
    err = PBMakeFSRefSync (&fileRefParam); // make FSRef for folder
    if (noErr != err) {
        DebugStr ((unsigned char *)"\pCould make FSref to frameworks folder");
        return err;
    }
    // create URL to folder
    bundleURLOpenGL = CFURLCreateFromFSRef (kCFAllocatorDefault,
                                            &fileRef);
    if (!bundleURLOpenGL) {
        DebugStr ((unsigned char *)"\pCould create OpenGL Framework bundle URL");
        return paramErr;
    }
    // create ref to GL's bundle
    gBundleRefOpenGL = CFBundleCreate (kCFAllocatorDefault,
                                       bundleURLOpenGL);
    if (!gBundleRefOpenGL) {
        DebugStr ((unsigned char *)"\pCould not create OpenGL Framework bundle");
        return paramErr;
    }
    CFRelease (bundleURLOpenGL); // release created bundle
    // if the code was successfully loaded, look for our function.
    if (!CFBundleLoadExecutable (gBundleRefOpenGL)) {
        DebugStr ((unsigned char *)"\pCould not load MachO executable");
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

/*unused*/
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
	/* Bit array of available extensions */
static std::bitset<bgl::NUM_EXTENSIONS> enabled_extensions;
static std::vector<STR_String> extensions;
static int m_debug;
	
static void LinkExtensions();

static void EnableExtension(bgl::ExtensionName name)
{
	unsigned int num = (unsigned int) name;
	if (num < bgl::NUM_EXTENSIONS)
		enabled_extensions.set(num);
}


static bool QueryExtension(STR_String extension_name)
{
	return std::find(extensions.begin(), extensions.end(), extension_name) != extensions.end();
}

namespace bgl
{

void InitExtensions(int debug)
{
	m_debug = debug;
	bglInitEntryPoints (); //init bundle
	EnableExtension(_BGL_TEST);
	LinkExtensions();
	bglDeallocEntryPoints();
}

bool QueryExtension(ExtensionName name)
{
	unsigned int num = (unsigned int) name;
	if (num < NUM_EXTENSIONS)
		return enabled_extensions[num];
		 
	return false;
}

bool QueryVersion(int major, int minor)
{
	static int gl_major = 0;
	static int gl_minor = 0;
	
	if (gl_major == 0)
	{
		const char *gl_version_str = (const char *) glGetString(GL_VERSION);
		if (!gl_version_str)
			return false;
		STR_String gl_version = STR_String(gl_version_str);
		int i = gl_version.Find('.');
		gl_major = gl_version.Left(i).ToInt();
		gl_minor = gl_version.Mid(i+1, gl_version.FindOneOf(". ", i+1) - i - 1).ToInt();
	
		static bool doQueryVersion = m_debug;
		if (doQueryVersion)
		{
			doQueryVersion = false;
			std::cout << "GL_VERSION: " << gl_major << "." << gl_minor << " (" << gl_version << ")" << std::endl;
		}
	}
	
	if (gl_major > major)
		return true;
	
	if (gl_major == major && gl_minor >= minor)
		return true;
		
	return false;
}


/*******************************************************************************
1. Extension function entry points go here

Need to #ifdef (compile time test for extension)
Add null functions if appropriate

Some extensions have been incorporated into the core GL, eg Multitexture was 
added in GL v1.1.  If Blender calls one of these functions before they are 
linked, it will crash.  Even worse, if Blender *indirectly* calls one of these 
functions, (ie the GL implementation calls them itself) Blender will crash.

We fix this by adding them to the bgl namespace - the functions are now 
private to the gameengine.  Code can transparently use extensions by adding:

using namespace bgl;

to their source.  Cunning like a weasel.

 ******************************************************************************/

#if defined(PFNGLPNTRIANGLESIATIPROC)
PFNGLPNTRIANGLESIATIPROC glPNTrianglesiATI;
PFNGLPNTRIANGLESFATIPROC glPNTrianglesfATI;
#endif

BL_EXTInfo RAS_EXT_support;


#ifdef GL_ARB_multitexture
int max_texture_units = 2;
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
PFNGLMULTITEXCOORD1DARBPROC glMultiTexCoord1dARB;
PFNGLMULTITEXCOORD1DVARBPROC glMultiTexCoord1dvARB;
PFNGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB;
PFNGLMULTITEXCOORD1FVARBPROC glMultiTexCoord1fvARB;
PFNGLMULTITEXCOORD1IARBPROC glMultiTexCoord1iARB;
PFNGLMULTITEXCOORD1IVARBPROC glMultiTexCoord1ivARB;
PFNGLMULTITEXCOORD1SARBPROC glMultiTexCoord1sARB;
PFNGLMULTITEXCOORD1SVARBPROC glMultiTexCoord1svARB;
PFNGLMULTITEXCOORD2DARBPROC glMultiTexCoord2dARB;
PFNGLMULTITEXCOORD2DVARBPROC glMultiTexCoord2dvARB;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;
PFNGLMULTITEXCOORD2IARBPROC glMultiTexCoord2iARB;
PFNGLMULTITEXCOORD2IVARBPROC glMultiTexCoord2ivARB;
PFNGLMULTITEXCOORD2SARBPROC glMultiTexCoord2sARB;
PFNGLMULTITEXCOORD2SVARBPROC glMultiTexCoord2svARB;
PFNGLMULTITEXCOORD3DARBPROC glMultiTexCoord3dARB;
PFNGLMULTITEXCOORD3DVARBPROC glMultiTexCoord3dvARB;
PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB;
PFNGLMULTITEXCOORD3FVARBPROC glMultiTexCoord3fvARB;
PFNGLMULTITEXCOORD3IARBPROC glMultiTexCoord3iARB;
PFNGLMULTITEXCOORD3IVARBPROC glMultiTexCoord3ivARB;
PFNGLMULTITEXCOORD3SARBPROC glMultiTexCoord3sARB;
PFNGLMULTITEXCOORD3SVARBPROC glMultiTexCoord3svARB;
PFNGLMULTITEXCOORD4DARBPROC glMultiTexCoord4dARB;
PFNGLMULTITEXCOORD4DVARBPROC glMultiTexCoord4dvARB;
PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB;
PFNGLMULTITEXCOORD4FVARBPROC glMultiTexCoord4fvARB;
PFNGLMULTITEXCOORD4IARBPROC glMultiTexCoord4iARB;
PFNGLMULTITEXCOORD4IVARBPROC glMultiTexCoord4ivARB;
PFNGLMULTITEXCOORD4SARBPROC glMultiTexCoord4sARB;
PFNGLMULTITEXCOORD4SVARBPROC glMultiTexCoord4svARB;
#endif

#ifdef GL_ARB_shader_objects
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

#ifdef GL_ARB_vertex_shader
PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB;
PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB;
PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB;
#endif

} // namespace bgl

using namespace bgl;
/*******************************************************************************
2. Query extension functions here

Need to #ifdef (compile time test for extension)
Use QueryExtension("GL_EXT_name") to test at runtime.
Use bglGetProcAddress to find entry point
Use EnableExtension(_GL_EXT_...) to allow Blender to use the extension.

 ******************************************************************************/
static void LinkExtensions()
{
	static bool doDebugMessages = m_debug;
	extensions = STR_String((const char *) glGetString(GL_EXTENSIONS)).Explode(' ');
	RAS_EXT_support = BL_EXTInfo();

#if defined(PFNGLPNTRIANGLESIATIPROC)
	if (QueryExtension("GL_ATI_pn_triangles"))
	{
		glPNTrianglesiATI = reinterpret_cast<PFNGLPNTRIANGLESIATIPROC>(bglGetProcAddress((const GLubyte *) "glPNTrianglesiATI"));
		glPNTrianglesfATI = reinterpret_cast<PFNGLPNTRIANGLESFATIPROC>(bglGetProcAddress((const GLubyte *) "glPNTrianglesfATI"));
		if (glPNTrianglesiATI && glPNTrianglesfATI) {
			EnableExtension(_GL_ATI_pn_triangles);
			if (doDebugMessages)
				std::cout << "Enabled GL_ATI_pn_triangles" << std::endl;
		} else {
			std::cout << "ERROR: GL_ATI_pn_triangles implementation is broken!" << std::endl;
		}
	}
#endif

#ifdef GL_ARB_texture_env_combine
	if (QueryExtension("GL_ARB_texture_env_combine"))
	{
		EnableExtension(_GL_ARB_texture_env_combine);
		RAS_EXT_support._ARB_texture_env_combine = 1;
		if (doDebugMessages)
		{
			std::cout << "Detected GL_ARB_texture_env_combine" << std::endl;
		}
	}
#endif

#ifdef GL_ARB_texture_cube_map
	if (QueryExtension("GL_ARB_texture_cube_map"))
	{
		EnableExtension(_GL_ARB_texture_cube_map);
		RAS_EXT_support._ARB_texture_cube_map = 1;
		if (doDebugMessages)
			std::cout << "Detected GL_ARB_texture_cube_map" << std::endl;
	}
#endif

#ifdef GL_ARB_multitexture
	if (QueryExtension("GL_ARB_multitexture"))
	{
		bgl::glActiveTextureARB = reinterpret_cast<PFNGLACTIVETEXTUREARBPROC>(bglGetProcAddress((const GLubyte *) "glActiveTextureARB"));
		bgl::glClientActiveTextureARB = reinterpret_cast<PFNGLCLIENTACTIVETEXTUREARBPROC>(bglGetProcAddress((const GLubyte *) "glClientActiveTextureARB"));
		bgl::glMultiTexCoord1dARB = reinterpret_cast<PFNGLMULTITEXCOORD1DARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1dARB"));
		bgl::glMultiTexCoord1dvARB = reinterpret_cast<PFNGLMULTITEXCOORD1DVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1dvARB"));
		bgl::glMultiTexCoord1fARB = reinterpret_cast<PFNGLMULTITEXCOORD1FARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1fARB"));
		bgl::glMultiTexCoord1fvARB = reinterpret_cast<PFNGLMULTITEXCOORD1FVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1fvARB"));
		bgl::glMultiTexCoord1iARB = reinterpret_cast<PFNGLMULTITEXCOORD1IARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1iARB"));
		bgl::glMultiTexCoord1ivARB = reinterpret_cast<PFNGLMULTITEXCOORD1IVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1ivARB"));
		bgl::glMultiTexCoord1sARB = reinterpret_cast<PFNGLMULTITEXCOORD1SARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1sARB"));
		bgl::glMultiTexCoord1svARB = reinterpret_cast<PFNGLMULTITEXCOORD1SVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1svARB"));
		bgl::glMultiTexCoord2dARB = reinterpret_cast<PFNGLMULTITEXCOORD2DARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2dARB"));
		bgl::glMultiTexCoord2dvARB = reinterpret_cast<PFNGLMULTITEXCOORD2DVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2dvARB"));
		bgl::glMultiTexCoord2fARB = reinterpret_cast<PFNGLMULTITEXCOORD2FARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2fARB"));
		bgl::glMultiTexCoord2fvARB = reinterpret_cast<PFNGLMULTITEXCOORD2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2fvARB"));
		bgl::glMultiTexCoord2iARB = reinterpret_cast<PFNGLMULTITEXCOORD2IARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2iARB"));
		bgl::glMultiTexCoord2ivARB = reinterpret_cast<PFNGLMULTITEXCOORD2IVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2ivARB"));
		bgl::glMultiTexCoord2sARB = reinterpret_cast<PFNGLMULTITEXCOORD2SARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2sARB"));
		bgl::glMultiTexCoord2svARB = reinterpret_cast<PFNGLMULTITEXCOORD2SVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2svARB"));
		bgl::glMultiTexCoord3dARB = reinterpret_cast<PFNGLMULTITEXCOORD3DARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3dARB"));
		bgl::glMultiTexCoord3dvARB = reinterpret_cast<PFNGLMULTITEXCOORD3DVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3dvARB"));
		bgl::glMultiTexCoord3fARB = reinterpret_cast<PFNGLMULTITEXCOORD3FARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3fARB"));
		bgl::glMultiTexCoord3fvARB = reinterpret_cast<PFNGLMULTITEXCOORD3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3fvARB"));
		bgl::glMultiTexCoord3iARB = reinterpret_cast<PFNGLMULTITEXCOORD3IARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3iARB"));
		bgl::glMultiTexCoord3ivARB = reinterpret_cast<PFNGLMULTITEXCOORD3IVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3ivARB"));
		bgl::glMultiTexCoord3sARB = reinterpret_cast<PFNGLMULTITEXCOORD3SARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3sARB"));
		bgl::glMultiTexCoord3svARB = reinterpret_cast<PFNGLMULTITEXCOORD3SVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3svARB"));
		bgl::glMultiTexCoord4dARB = reinterpret_cast<PFNGLMULTITEXCOORD4DARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4dARB"));
		bgl::glMultiTexCoord4dvARB = reinterpret_cast<PFNGLMULTITEXCOORD4DVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4dvARB"));
		bgl::glMultiTexCoord4fARB = reinterpret_cast<PFNGLMULTITEXCOORD4FARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4fARB"));
		bgl::glMultiTexCoord4fvARB = reinterpret_cast<PFNGLMULTITEXCOORD4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4fvARB"));
		bgl::glMultiTexCoord4iARB = reinterpret_cast<PFNGLMULTITEXCOORD4IARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4iARB"));
		bgl::glMultiTexCoord4ivARB = reinterpret_cast<PFNGLMULTITEXCOORD4IVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4ivARB"));
		bgl::glMultiTexCoord4sARB = reinterpret_cast<PFNGLMULTITEXCOORD4SARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4sARB"));
		bgl::glMultiTexCoord4svARB = reinterpret_cast<PFNGLMULTITEXCOORD4SVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4svARB"));
		if (bgl::glActiveTextureARB && bgl::glClientActiveTextureARB && bgl::glMultiTexCoord1dARB && bgl::glMultiTexCoord1dvARB && bgl::glMultiTexCoord1fARB && bgl::glMultiTexCoord1fvARB && bgl::glMultiTexCoord1iARB && bgl::glMultiTexCoord1ivARB && bgl::glMultiTexCoord1sARB && bgl::glMultiTexCoord1svARB && bgl::glMultiTexCoord2dARB && bgl::glMultiTexCoord2dvARB && bgl::glMultiTexCoord2fARB && bgl::glMultiTexCoord2fvARB && bgl::glMultiTexCoord2iARB && bgl::glMultiTexCoord2ivARB && bgl::glMultiTexCoord2sARB && bgl::glMultiTexCoord2svARB && bgl::glMultiTexCoord3dARB && bgl::glMultiTexCoord3dvARB && bgl::glMultiTexCoord3fARB && bgl::glMultiTexCoord3fvARB && bgl::glMultiTexCoord3iARB && bgl::glMultiTexCoord3ivARB && bgl::glMultiTexCoord3sARB && bgl::glMultiTexCoord3svARB && bgl::glMultiTexCoord4dARB && bgl::glMultiTexCoord4dvARB && bgl::glMultiTexCoord4fARB && bgl::glMultiTexCoord4fvARB && bgl::glMultiTexCoord4iARB && bgl::glMultiTexCoord4ivARB && bgl::glMultiTexCoord4sARB && bgl::glMultiTexCoord4svARB) {
			EnableExtension(_GL_ARB_multitexture);
			RAS_EXT_support._ARB_multitexture = 1;
			if (doDebugMessages)
				std::cout << "Enabled GL_ARB_multitexture" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_multitexture implementation is broken!" << std::endl;
		}
	}
#endif

#ifdef GL_ARB_shader_objects
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
			RAS_EXT_support._ARB_shader_objects =1;
			if (doDebugMessages)
				std::cout << "Enabled GL_ARB_shader_objects" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_shader_objects implementation is broken!" << std::endl;
		}
	}
#endif

#ifdef GL_ARB_vertex_shader
	if (QueryExtension("GL_ARB_vertex_shader"))
	{
		glBindAttribLocationARB = reinterpret_cast<PFNGLBINDATTRIBLOCATIONARBPROC>(bglGetProcAddress((const GLubyte *) "glBindAttribLocationARB"));
		glGetActiveAttribARB = reinterpret_cast<PFNGLGETACTIVEATTRIBARBPROC>(bglGetProcAddress((const GLubyte *) "glGetActiveAttribARB"));
		glGetAttribLocationARB = reinterpret_cast<PFNGLGETATTRIBLOCATIONARBPROC>(bglGetProcAddress((const GLubyte *) "glGetAttribLocationARB"));
		if (glBindAttribLocationARB && glGetActiveAttribARB && glGetAttribLocationARB) {
			EnableExtension(_GL_ARB_vertex_shader);
			RAS_EXT_support._ARB_vertex_shader = 1;
			if (doDebugMessages)
				std::cout << "Enabled GL_ARB_vertex_shader" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_vertex_shader implementation is broken!" << std::endl;
		}
	}
#endif

#ifdef GL_ARB_fragment_shader
	if (QueryExtension("GL_ARB_fragment_shader"))
	{
		EnableExtension(_GL_ARB_fragment_shader);
		RAS_EXT_support._ARB_fragment_shader = 1;
		if (doDebugMessages)
			std::cout << "Detected GL_ARB_fragment_shader" << std::endl;
	}
#endif

	if (QueryExtension("GL_EXT_separate_specular_color"))
	{
			EnableExtension(_GL_EXT_separate_specular_color);
			if (doDebugMessages)
					std::cout << "Detected GL_EXT_separate_specular_color" << std::endl;
	}

	doDebugMessages = false;
}

