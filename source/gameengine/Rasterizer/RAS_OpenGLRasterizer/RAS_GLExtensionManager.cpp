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
	/* Bit array of available extensions */
static unsigned int enabled_extensions[(bgl::NUM_EXTENSIONS + 8*sizeof(unsigned int) - 1)/(8*sizeof(unsigned int))];
static std::vector<STR_String> extensions;
static int m_debug;
	
static void LinkExtensions();

static void EnableExtension(bgl::ExtensionName name)
{
	unsigned int num = (unsigned int) name;
	if (num < bgl::NUM_EXTENSIONS)
		enabled_extensions[num/(8*sizeof(unsigned int))] |= (1<<(num%(8*sizeof(unsigned int))));
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
	if (num >= NUM_EXTENSIONS)
		return false;
	
	return (enabled_extensions[num/(8*sizeof(unsigned int))] & (1<<(num%(8*sizeof(unsigned int))))) != 0;
}

bool QueryVersion(int major, int minor)
{
	static int gl_major = 0;
	static int gl_minor = 0;
	
	if (gl_major == 0)
	{
		STR_String gl_version = STR_String((const char *) glGetString(GL_VERSION));
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

	doDebugMessages = false;
}

