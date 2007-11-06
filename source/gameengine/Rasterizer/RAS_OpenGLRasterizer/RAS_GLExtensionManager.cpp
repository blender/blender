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
#  define GL_GLEXT_LEGACY 1
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
                      &fileRefParam.ioVRefNum, (SInt32*)&fileRefParam.ioDirID);
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


//weird bug related to combination of pthreads,libGL and dlopen
//cannot call dlclose in such environment, causes crashes
//so try to keep a global handle to libGL
void* libGL = 0;

static void bglInitEntryPoints (void)
{
	Display *dpy = glXGetCurrentDisplay();
	std::vector<STR_String> Xextensions = STR_String(glXQueryExtensionsString(dpy, DefaultScreen(dpy))).Explode(' ');
	if (std::find(Xextensions.begin(), Xextensions.end(), "GLX_ARB_get_proc_address") != Xextensions.end()) 
	{
		if (!libGL)
		{
			libGL = dlopen("libGL.so", RTLD_LAZY|RTLD_GLOBAL);
			if (libGL)
				bglGetProcAddress = (PFNBGLXGETPROCADDRESSARBPROC) (dlsym(libGL, "glXGetProcAddressARB"));
			else
				std::cout << "Error: " << dlerror() << std::endl;

			// dlclose(libGL);
			if (!bglGetProcAddress)
				bglGetProcAddress = (PFNBGLXGETPROCADDRESSARBPROC) _getProcAddress;
			
			// --
			if(!bglGetProcAddress)
				std::cout << "Error: unable to find _getProcAddress in libGL" << std::endl;
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
 PFNGLACTIVETEXTUREARBPROC blActiveTextureARB;
 PFNGLCLIENTACTIVETEXTUREARBPROC blClientActiveTextureARB;
 PFNGLMULTITEXCOORD1DARBPROC blMultiTexCoord1dARB;
 PFNGLMULTITEXCOORD1DVARBPROC blMultiTexCoord1dvARB;
 PFNGLMULTITEXCOORD1FARBPROC blMultiTexCoord1fARB;
 PFNGLMULTITEXCOORD1FVARBPROC blMultiTexCoord1fvARB;
 PFNGLMULTITEXCOORD1IARBPROC blMultiTexCoord1iARB;
 PFNGLMULTITEXCOORD1IVARBPROC blMultiTexCoord1ivARB;
 PFNGLMULTITEXCOORD1SARBPROC blMultiTexCoord1sARB;
 PFNGLMULTITEXCOORD1SVARBPROC blMultiTexCoord1svARB;
 PFNGLMULTITEXCOORD2DARBPROC blMultiTexCoord2dARB;
 PFNGLMULTITEXCOORD2DVARBPROC blMultiTexCoord2dvARB;
 PFNGLMULTITEXCOORD2FARBPROC blMultiTexCoord2fARB;
 PFNGLMULTITEXCOORD2FVARBPROC blMultiTexCoord2fvARB;
 PFNGLMULTITEXCOORD2IARBPROC blMultiTexCoord2iARB;
 PFNGLMULTITEXCOORD2IVARBPROC blMultiTexCoord2ivARB;
 PFNGLMULTITEXCOORD2SARBPROC blMultiTexCoord2sARB;
 PFNGLMULTITEXCOORD2SVARBPROC blMultiTexCoord2svARB;
 PFNGLMULTITEXCOORD3DARBPROC blMultiTexCoord3dARB;
 PFNGLMULTITEXCOORD3DVARBPROC blMultiTexCoord3dvARB;
 PFNGLMULTITEXCOORD3FARBPROC blMultiTexCoord3fARB;
 PFNGLMULTITEXCOORD3FVARBPROC blMultiTexCoord3fvARB;
 PFNGLMULTITEXCOORD3IARBPROC blMultiTexCoord3iARB;
 PFNGLMULTITEXCOORD3IVARBPROC blMultiTexCoord3ivARB;
 PFNGLMULTITEXCOORD3SARBPROC blMultiTexCoord3sARB;
 PFNGLMULTITEXCOORD3SVARBPROC blMultiTexCoord3svARB;
 PFNGLMULTITEXCOORD4DARBPROC blMultiTexCoord4dARB;
 PFNGLMULTITEXCOORD4DVARBPROC blMultiTexCoord4dvARB;
 PFNGLMULTITEXCOORD4FARBPROC blMultiTexCoord4fARB;
 PFNGLMULTITEXCOORD4FVARBPROC blMultiTexCoord4fvARB;
 PFNGLMULTITEXCOORD4IARBPROC blMultiTexCoord4iARB;
 PFNGLMULTITEXCOORD4IVARBPROC blMultiTexCoord4ivARB;
 PFNGLMULTITEXCOORD4SARBPROC blMultiTexCoord4sARB;
 PFNGLMULTITEXCOORD4SVARBPROC blMultiTexCoord4svARB;
#endif

#ifdef GL_ARB_shader_objects
 PFNGLDELETEOBJECTARBPROC blDeleteObjectARB;
 PFNGLGETHANDLEARBPROC blGetHandleARB;
 PFNGLDETACHOBJECTARBPROC blDetachObjectARB;
 PFNGLCREATESHADEROBJECTARBPROC blCreateShaderObjectARB;
 PFNGLSHADERSOURCEARBPROC blShaderSourceARB;
 PFNGLCOMPILESHADERARBPROC blCompileShaderARB;
 PFNGLCREATEPROGRAMOBJECTARBPROC blCreateProgramObjectARB;
 PFNGLATTACHOBJECTARBPROC blAttachObjectARB;
 PFNGLLINKPROGRAMARBPROC blLinkProgramARB;
 PFNGLUSEPROGRAMOBJECTARBPROC blUseProgramObjectARB;
 PFNGLVALIDATEPROGRAMARBPROC blValidateProgramARB;
 PFNGLUNIFORM1FARBPROC blUniform1fARB;
 PFNGLUNIFORM2FARBPROC blUniform2fARB;
 PFNGLUNIFORM3FARBPROC blUniform3fARB;
 PFNGLUNIFORM4FARBPROC blUniform4fARB;
 PFNGLUNIFORM1IARBPROC blUniform1iARB;
 PFNGLUNIFORM2IARBPROC blUniform2iARB;
 PFNGLUNIFORM3IARBPROC blUniform3iARB;
 PFNGLUNIFORM4IARBPROC blUniform4iARB;
 PFNGLUNIFORM1FVARBPROC blUniform1fvARB;
 PFNGLUNIFORM2FVARBPROC blUniform2fvARB;
 PFNGLUNIFORM3FVARBPROC blUniform3fvARB;
 PFNGLUNIFORM4FVARBPROC blUniform4fvARB;
 PFNGLUNIFORM1IVARBPROC blUniform1ivARB;
 PFNGLUNIFORM2IVARBPROC blUniform2ivARB;
 PFNGLUNIFORM3IVARBPROC blUniform3ivARB;
 PFNGLUNIFORM4IVARBPROC blUniform4ivARB;
 PFNGLUNIFORMMATRIX2FVARBPROC blUniformMatrix2fvARB;
 PFNGLUNIFORMMATRIX3FVARBPROC blUniformMatrix3fvARB;
 PFNGLUNIFORMMATRIX4FVARBPROC blUniformMatrix4fvARB;
 PFNGLGETOBJECTPARAMETERFVARBPROC blGetObjectParameterfvARB;
 PFNGLGETOBJECTPARAMETERIVARBPROC blGetObjectParameterivARB;
 PFNGLGETINFOLOGARBPROC blGetInfoLogARB;
 PFNGLGETATTACHEDOBJECTSARBPROC blGetAttachedObjectsARB;
 PFNGLGETUNIFORMLOCATIONARBPROC blGetUniformLocationARB;
 PFNGLGETACTIVEUNIFORMARBPROC blGetActiveUniformARB;
 PFNGLGETUNIFORMFVARBPROC blGetUniformfvARB;
 PFNGLGETUNIFORMIVARBPROC blGetUniformivARB;
 PFNGLGETSHADERSOURCEARBPROC blGetShaderSourceARB;
#endif

#ifdef GL_ARB_vertex_shader
PFNGLBINDATTRIBLOCATIONARBPROC blBindAttribLocationARB;
PFNGLGETACTIVEATTRIBARBPROC blGetActiveAttribARB;
PFNGLGETATTRIBLOCATIONARBPROC blGetAttribLocationARB;
#endif

#ifdef GL_ARB_vertex_program
 PFNGLVERTEXATTRIB1FARBPROC blVertexAttrib1fARB;
 PFNGLVERTEXATTRIB1FVARBPROC blVertexAttrib1fvARB;
 PFNGLVERTEXATTRIB2FARBPROC blVertexAttrib2fARB;
 PFNGLVERTEXATTRIB2FVARBPROC blVertexAttrib2fvARB;
 PFNGLVERTEXATTRIB3FARBPROC blVertexAttrib3fARB;
 PFNGLVERTEXATTRIB3FVARBPROC blVertexAttrib3fvARB;
 PFNGLVERTEXATTRIB4FARBPROC blVertexAttrib4fARB;
 PFNGLVERTEXATTRIB4FVARBPROC blVertexAttrib4fvARB;
 PFNGLGETPROGRAMSTRINGARBPROC blGetProgramStringARB;
 PFNGLGETVERTEXATTRIBDVARBPROC blGetVertexAttribdvARB;
 PFNGLGETVERTEXATTRIBFVARBPROC blGetVertexAttribfvARB;
 PFNGLGETVERTEXATTRIBIVARBPROC blGetVertexAttribivARB;
#endif

 /*
#ifdef GL_EXT_compiled_vertex_array
 PFNGLLOCKARRAYSEXTPROC blLockArraysEXT;
 PFNGLUNLOCKARRAYSEXTPROC blUnlockArraysEXT;
#endif
*/

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
		bgl::blActiveTextureARB = reinterpret_cast<PFNGLACTIVETEXTUREARBPROC>(bglGetProcAddress((const GLubyte *) "glActiveTextureARB"));
		bgl::blClientActiveTextureARB = reinterpret_cast<PFNGLCLIENTACTIVETEXTUREARBPROC>(bglGetProcAddress((const GLubyte *) "glClientActiveTextureARB"));
		bgl::blMultiTexCoord1dARB = reinterpret_cast<PFNGLMULTITEXCOORD1DARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1dARB"));
		bgl::blMultiTexCoord1dvARB = reinterpret_cast<PFNGLMULTITEXCOORD1DVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1dvARB"));
		bgl::blMultiTexCoord1fARB = reinterpret_cast<PFNGLMULTITEXCOORD1FARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1fARB"));
		bgl::blMultiTexCoord1fvARB = reinterpret_cast<PFNGLMULTITEXCOORD1FVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1fvARB"));
		bgl::blMultiTexCoord1iARB = reinterpret_cast<PFNGLMULTITEXCOORD1IARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1iARB"));
		bgl::blMultiTexCoord1ivARB = reinterpret_cast<PFNGLMULTITEXCOORD1IVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1ivARB"));
		bgl::blMultiTexCoord1sARB = reinterpret_cast<PFNGLMULTITEXCOORD1SARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1sARB"));
		bgl::blMultiTexCoord1svARB = reinterpret_cast<PFNGLMULTITEXCOORD1SVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord1svARB"));
		bgl::blMultiTexCoord2dARB = reinterpret_cast<PFNGLMULTITEXCOORD2DARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2dARB"));
		bgl::blMultiTexCoord2dvARB = reinterpret_cast<PFNGLMULTITEXCOORD2DVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2dvARB"));
		bgl::blMultiTexCoord2fARB = reinterpret_cast<PFNGLMULTITEXCOORD2FARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2fARB"));
		bgl::blMultiTexCoord2fvARB = reinterpret_cast<PFNGLMULTITEXCOORD2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2fvARB"));
		bgl::blMultiTexCoord2iARB = reinterpret_cast<PFNGLMULTITEXCOORD2IARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2iARB"));
		bgl::blMultiTexCoord2ivARB = reinterpret_cast<PFNGLMULTITEXCOORD2IVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2ivARB"));
		bgl::blMultiTexCoord2sARB = reinterpret_cast<PFNGLMULTITEXCOORD2SARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2sARB"));
		bgl::blMultiTexCoord2svARB = reinterpret_cast<PFNGLMULTITEXCOORD2SVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord2svARB"));
		bgl::blMultiTexCoord3dARB = reinterpret_cast<PFNGLMULTITEXCOORD3DARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3dARB"));
		bgl::blMultiTexCoord3dvARB = reinterpret_cast<PFNGLMULTITEXCOORD3DVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3dvARB"));
		bgl::blMultiTexCoord3fARB = reinterpret_cast<PFNGLMULTITEXCOORD3FARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3fARB"));
		bgl::blMultiTexCoord3fvARB = reinterpret_cast<PFNGLMULTITEXCOORD3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3fvARB"));
		bgl::blMultiTexCoord3iARB = reinterpret_cast<PFNGLMULTITEXCOORD3IARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3iARB"));
		bgl::blMultiTexCoord3ivARB = reinterpret_cast<PFNGLMULTITEXCOORD3IVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3ivARB"));
		bgl::blMultiTexCoord3sARB = reinterpret_cast<PFNGLMULTITEXCOORD3SARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3sARB"));
		bgl::blMultiTexCoord3svARB = reinterpret_cast<PFNGLMULTITEXCOORD3SVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord3svARB"));
		bgl::blMultiTexCoord4dARB = reinterpret_cast<PFNGLMULTITEXCOORD4DARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4dARB"));
		bgl::blMultiTexCoord4dvARB = reinterpret_cast<PFNGLMULTITEXCOORD4DVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4dvARB"));
		bgl::blMultiTexCoord4fARB = reinterpret_cast<PFNGLMULTITEXCOORD4FARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4fARB"));
		bgl::blMultiTexCoord4fvARB = reinterpret_cast<PFNGLMULTITEXCOORD4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4fvARB"));
		bgl::blMultiTexCoord4iARB = reinterpret_cast<PFNGLMULTITEXCOORD4IARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4iARB"));
		bgl::blMultiTexCoord4ivARB = reinterpret_cast<PFNGLMULTITEXCOORD4IVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4ivARB"));
		bgl::blMultiTexCoord4sARB = reinterpret_cast<PFNGLMULTITEXCOORD4SARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4sARB"));
		bgl::blMultiTexCoord4svARB = reinterpret_cast<PFNGLMULTITEXCOORD4SVARBPROC>(bglGetProcAddress((const GLubyte *) "glMultiTexCoord4svARB"));
		if (bgl::blActiveTextureARB && bgl::blClientActiveTextureARB && bgl::blMultiTexCoord1dARB && bgl::blMultiTexCoord1dvARB && bgl::blMultiTexCoord1fARB && bgl::blMultiTexCoord1fvARB && bgl::blMultiTexCoord1iARB && bgl::blMultiTexCoord1ivARB && bgl::blMultiTexCoord1sARB && bgl::blMultiTexCoord1svARB && bgl::blMultiTexCoord2dARB && bgl::blMultiTexCoord2dvARB && bgl::blMultiTexCoord2fARB && bgl::blMultiTexCoord2fvARB && bgl::blMultiTexCoord2iARB && bgl::blMultiTexCoord2ivARB && bgl::blMultiTexCoord2sARB && bgl::blMultiTexCoord2svARB && bgl::blMultiTexCoord3dARB && bgl::blMultiTexCoord3dvARB && bgl::blMultiTexCoord3fARB && bgl::blMultiTexCoord3fvARB && bgl::blMultiTexCoord3iARB && bgl::blMultiTexCoord3ivARB && bgl::blMultiTexCoord3sARB && bgl::blMultiTexCoord3svARB && bgl::blMultiTexCoord4dARB && bgl::blMultiTexCoord4dvARB && bgl::blMultiTexCoord4fARB && bgl::blMultiTexCoord4fvARB && bgl::blMultiTexCoord4iARB && bgl::blMultiTexCoord4ivARB && bgl::blMultiTexCoord4sARB && bgl::blMultiTexCoord4svARB) {
			EnableExtension(_GL_ARB_multitexture);
			RAS_EXT_support._ARB_multitexture = 1;
			if (doDebugMessages)
				std::cout << "Enabled GL_ARB_multitexture" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_multitexture implementation is broken!" << std::endl;
		}
	}
#endif

#if GL_ARB_shader_objects
	if (QueryExtension("GL_ARB_shader_objects"))
	{
		bgl::blDeleteObjectARB = reinterpret_cast<PFNGLDELETEOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glDeleteObjectARB"));
		bgl::blGetHandleARB = reinterpret_cast<PFNGLGETHANDLEARBPROC>(bglGetProcAddress((const GLubyte *) "glGetHandleARB"));
		bgl::blDetachObjectARB = reinterpret_cast<PFNGLDETACHOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glDetachObjectARB"));
		bgl::blCreateShaderObjectARB = reinterpret_cast<PFNGLCREATESHADEROBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glCreateShaderObjectARB"));
		bgl::blShaderSourceARB = reinterpret_cast<PFNGLSHADERSOURCEARBPROC>(bglGetProcAddress((const GLubyte *) "glShaderSourceARB"));
		bgl::blCompileShaderARB = reinterpret_cast<PFNGLCOMPILESHADERARBPROC>(bglGetProcAddress((const GLubyte *) "glCompileShaderARB"));
		bgl::blCreateProgramObjectARB = reinterpret_cast<PFNGLCREATEPROGRAMOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glCreateProgramObjectARB"));
		bgl::blAttachObjectARB = reinterpret_cast<PFNGLATTACHOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glAttachObjectARB"));
		bgl::blLinkProgramARB = reinterpret_cast<PFNGLLINKPROGRAMARBPROC>(bglGetProcAddress((const GLubyte *) "glLinkProgramARB"));
		bgl::blUseProgramObjectARB = reinterpret_cast<PFNGLUSEPROGRAMOBJECTARBPROC>(bglGetProcAddress((const GLubyte *) "glUseProgramObjectARB"));
		bgl::blValidateProgramARB = reinterpret_cast<PFNGLVALIDATEPROGRAMARBPROC>(bglGetProcAddress((const GLubyte *) "glValidateProgramARB"));
		bgl::blUniform1fARB = reinterpret_cast<PFNGLUNIFORM1FARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform1fARB"));
		bgl::blUniform2fARB = reinterpret_cast<PFNGLUNIFORM2FARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform2fARB"));
		bgl::blUniform3fARB = reinterpret_cast<PFNGLUNIFORM3FARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform3fARB"));
		bgl::blUniform4fARB = reinterpret_cast<PFNGLUNIFORM4FARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform4fARB"));
		bgl::blUniform1iARB = reinterpret_cast<PFNGLUNIFORM1IARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform1iARB"));
		bgl::blUniform2iARB = reinterpret_cast<PFNGLUNIFORM2IARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform2iARB"));
		bgl::blUniform3iARB = reinterpret_cast<PFNGLUNIFORM3IARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform3iARB"));
		bgl::blUniform4iARB = reinterpret_cast<PFNGLUNIFORM4IARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform4iARB"));
		bgl::blUniform1fvARB = reinterpret_cast<PFNGLUNIFORM1FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform1fvARB"));
		bgl::blUniform2fvARB = reinterpret_cast<PFNGLUNIFORM2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform2fvARB"));
		bgl::blUniform3fvARB = reinterpret_cast<PFNGLUNIFORM3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform3fvARB"));
		bgl::blUniform4fvARB = reinterpret_cast<PFNGLUNIFORM4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform4fvARB"));
		bgl::blUniform1ivARB = reinterpret_cast<PFNGLUNIFORM1IVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform1ivARB"));
		bgl::blUniform2ivARB = reinterpret_cast<PFNGLUNIFORM2IVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform2ivARB"));
		bgl::blUniform3ivARB = reinterpret_cast<PFNGLUNIFORM3IVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform3ivARB"));
		bgl::blUniform4ivARB = reinterpret_cast<PFNGLUNIFORM4IVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniform4ivARB"));
		bgl::blUniformMatrix2fvARB = reinterpret_cast<PFNGLUNIFORMMATRIX2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniformMatrix2fvARB"));
		bgl::blUniformMatrix3fvARB = reinterpret_cast<PFNGLUNIFORMMATRIX3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniformMatrix3fvARB"));
		bgl::blUniformMatrix4fvARB = reinterpret_cast<PFNGLUNIFORMMATRIX4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glUniformMatrix4fvARB"));
		bgl::blGetObjectParameterfvARB = reinterpret_cast<PFNGLGETOBJECTPARAMETERFVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetObjectParameterfvARB"));
		bgl::blGetObjectParameterivARB = reinterpret_cast<PFNGLGETOBJECTPARAMETERIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetObjectParameterivARB"));
		bgl::blGetInfoLogARB = reinterpret_cast<PFNGLGETINFOLOGARBPROC>(bglGetProcAddress((const GLubyte *) "glGetInfoLogARB"));
		bgl::blGetAttachedObjectsARB = reinterpret_cast<PFNGLGETATTACHEDOBJECTSARBPROC>(bglGetProcAddress((const GLubyte *) "glGetAttachedObjectsARB"));
		bgl::blGetUniformLocationARB = reinterpret_cast<PFNGLGETUNIFORMLOCATIONARBPROC>(bglGetProcAddress((const GLubyte *) "glGetUniformLocationARB"));
		bgl::blGetActiveUniformARB = reinterpret_cast<PFNGLGETACTIVEUNIFORMARBPROC>(bglGetProcAddress((const GLubyte *) "glGetActiveUniformARB"));
		bgl::blGetUniformfvARB = reinterpret_cast<PFNGLGETUNIFORMFVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetUniformfvARB"));
		bgl::blGetUniformivARB = reinterpret_cast<PFNGLGETUNIFORMIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetUniformivARB"));
		bgl::blGetShaderSourceARB = reinterpret_cast<PFNGLGETSHADERSOURCEARBPROC>(bglGetProcAddress((const GLubyte *) "glGetShaderSourceARB"));
		if (bgl::blDeleteObjectARB && bgl::blGetHandleARB && bgl::blDetachObjectARB && bgl::blCreateShaderObjectARB && bgl::blShaderSourceARB && bgl::blCompileShaderARB && bgl::blCreateProgramObjectARB && bgl::blAttachObjectARB && bgl::blLinkProgramARB && bgl::blUseProgramObjectARB && bgl::blValidateProgramARB && bgl::blUniform1fARB && bgl::blUniform2fARB && bgl::blUniform3fARB && bgl::blUniform4fARB && bgl::blUniform1iARB && bgl::blUniform2iARB && bgl::blUniform3iARB && bgl::blUniform4iARB && bgl::blUniform1fvARB && bgl::blUniform2fvARB && bgl::blUniform3fvARB && bgl::blUniform4fvARB && bgl::blUniform1ivARB && bgl::blUniform2ivARB && bgl::blUniform3ivARB && bgl::blUniform4ivARB && bgl::blUniformMatrix2fvARB && bgl::blUniformMatrix3fvARB && bgl::blUniformMatrix4fvARB && bgl::blGetObjectParameterfvARB && bgl::blGetObjectParameterivARB && bgl::blGetInfoLogARB && bgl::blGetAttachedObjectsARB && bgl::blGetUniformLocationARB && bgl::blGetActiveUniformARB && bgl::blGetUniformfvARB && bgl::blGetUniformivARB && bgl::blGetShaderSourceARB) {
			EnableExtension(_GL_ARB_shader_objects);
			RAS_EXT_support._ARB_shader_objects =1;
			if (doDebugMessages)
				std::cout << "Enabled GL_ARB_shader_objects" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_shader_objects implementation is broken!" << std::endl;
		}
	}
#endif

#if GL_ARB_vertex_shader
	if (QueryExtension("GL_ARB_vertex_shader"))
	{
		bgl::blBindAttribLocationARB = reinterpret_cast<PFNGLBINDATTRIBLOCATIONARBPROC>(bglGetProcAddress((const GLubyte *) "glBindAttribLocationARB"));
		bgl::blGetActiveAttribARB = reinterpret_cast<PFNGLGETACTIVEATTRIBARBPROC>(bglGetProcAddress((const GLubyte *) "glGetActiveAttribARB"));
		bgl::blGetAttribLocationARB = reinterpret_cast<PFNGLGETATTRIBLOCATIONARBPROC>(bglGetProcAddress((const GLubyte *) "glGetAttribLocationARB"));
		if (bgl::blBindAttribLocationARB && bgl::blGetActiveAttribARB && bgl::blGetAttribLocationARB) {
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

#if defined(GL_ARB_vertex_program)
	if (QueryExtension("GL_ARB_vertex_program"))
	{
		bgl::blVertexAttrib1fARB = reinterpret_cast<PFNGLVERTEXATTRIB1FARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1fARB"));
		bgl::blVertexAttrib1fvARB = reinterpret_cast<PFNGLVERTEXATTRIB1FVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib1fvARB"));
		bgl::blVertexAttrib2fARB = reinterpret_cast<PFNGLVERTEXATTRIB2FARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2fARB"));
		bgl::blVertexAttrib2fvARB = reinterpret_cast<PFNGLVERTEXATTRIB2FVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib2fvARB"));
		bgl::blVertexAttrib3fARB = reinterpret_cast<PFNGLVERTEXATTRIB3FARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3fARB"));
		bgl::blVertexAttrib3fvARB = reinterpret_cast<PFNGLVERTEXATTRIB3FVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib3fvARB"));
		bgl::blVertexAttrib4fARB = reinterpret_cast<PFNGLVERTEXATTRIB4FARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4fARB"));
		bgl::blVertexAttrib4fvARB = reinterpret_cast<PFNGLVERTEXATTRIB4FVARBPROC>(bglGetProcAddress((const GLubyte *) "glVertexAttrib4fvARB"));
		bgl::blGetVertexAttribdvARB = reinterpret_cast<PFNGLGETVERTEXATTRIBDVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribdvARB"));
		bgl::blGetVertexAttribfvARB = reinterpret_cast<PFNGLGETVERTEXATTRIBFVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribfvARB"));
		bgl::blGetVertexAttribivARB = reinterpret_cast<PFNGLGETVERTEXATTRIBIVARBPROC>(bglGetProcAddress((const GLubyte *) "glGetVertexAttribivARB"));
		if (bgl::blVertexAttrib1fARB && bgl::blVertexAttrib1fvARB && bgl::blVertexAttrib2fARB && bgl::blVertexAttrib2fvARB && bgl::blVertexAttrib3fARB && bgl::blVertexAttrib3fvARB && bgl::blGetVertexAttribdvARB) {
			EnableExtension(_GL_ARB_vertex_program);
			RAS_EXT_support._ARB_vertex_program = 1;
			if (doDebugMessages)
				std::cout << "Enabled GL_ARB_vertex_program" << std::endl;
		} else {
			std::cout << "ERROR: GL_ARB_vertex_program implementation is broken!" << std::endl;
		}
	}
#endif


#ifdef GL_ARB_depth_texture
	if (QueryExtension("GL_ARB_depth_texture"))
	{
		EnableExtension(_GL_ARB_depth_texture);
		RAS_EXT_support._ARB_depth_texture = 1;
		if (doDebugMessages)
		{
			std::cout << "Detected GL_ARB_depth_texture" << std::endl;
		}
	}
#endif
/*
#ifdef GL_EXT_compiled_vertex_array
	if (QueryExtension("GL_EXT_compiled_vertex_array"))
	{
		blLockArraysEXT = reinterpret_cast<PFNGLLOCKARRAYSEXTPROC>(bglGetProcAddress((const GLubyte *) "glLockArraysEXT"));
		blUnlockArraysEXT = reinterpret_cast<PFNGLUNLOCKARRAYSEXTPROC>(bglGetProcAddress((const GLubyte *) "glUnlockArraysEXT"));
		if (blLockArraysEXT && blUnlockArraysEXT) {
			EnableExtension(_GL_EXT_compiled_vertex_array);
			RAS_EXT_support._EXT_compiled_vertex_array = 1;
			if (doDebugMessages)
				std::cout << "Enabled GL_EXT_compiled_vertex_array" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_compiled_vertex_array implementation is broken!" << std::endl;
		}
	}
#endif
*/
	if (QueryExtension("GL_EXT_separate_specular_color"))
	{
			EnableExtension(_GL_EXT_separate_specular_color);
			if (doDebugMessages)
					std::cout << "Detected GL_EXT_separate_specular_color" << std::endl;
	}

	doDebugMessages = false;
}

