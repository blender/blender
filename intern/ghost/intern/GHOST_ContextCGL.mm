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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_ContextCGL.mm
 *  \ingroup GHOST
 *
 * Definition of GHOST_ContextCGL class.
 */

#include "GHOST_ContextCGL.h"

#include <Cocoa/Cocoa.h>

#ifdef GHOST_MULTITHREADED_OPENGL
#include <OpenGL/OpenGL.h>
#endif

#include <vector>
#include <cassert>


NSOpenGLContext *GHOST_ContextCGL::s_sharedOpenGLContext = nil;
int              GHOST_ContextCGL::s_sharedCount         = 0;


GHOST_ContextCGL::GHOST_ContextCGL(
        bool stereoVisual,
        GHOST_TUns16  numOfAASamples,
        NSWindow *window,
        NSOpenGLView *openGLView,
        int contextProfileMask,
        int contextMajorVersion,
        int contextMinorVersion,
        int contextFlags,
        int contextResetNotificationStrategy)
    : GHOST_Context(stereoVisual, numOfAASamples),
      m_openGLView(openGLView),
      m_openGLContext(nil),
      m_debug(contextFlags)
{
	assert(openGLView != nil);
}


GHOST_ContextCGL::~GHOST_ContextCGL()
{
	if (m_openGLContext != nil) {
		if (m_openGLContext == [NSOpenGLContext currentContext]) {
			[NSOpenGLContext clearCurrentContext];
			[m_openGLView clearGLContext];
		}

		if (m_openGLContext != s_sharedOpenGLContext || s_sharedCount == 1) {
			assert(s_sharedCount > 0);

			s_sharedCount--;

			if (s_sharedCount == 0)
				s_sharedOpenGLContext = nil;

			[m_openGLContext release];
		}
	}
}


GHOST_TSuccess GHOST_ContextCGL::swapBuffers()
{
	if (m_openGLContext != nil) {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		[m_openGLContext flushBuffer];
		[pool drain];
		return GHOST_kSuccess;
	}
	else {
		return GHOST_kFailure;
	}
}


GHOST_TSuccess GHOST_ContextCGL::setSwapInterval(int interval)
{
	if (m_openGLContext != nil) {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		[m_openGLContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];
		[pool drain];
		return GHOST_kSuccess;
	}
	else {
		return GHOST_kFailure;
	}
}


GHOST_TSuccess GHOST_ContextCGL::getSwapInterval(int &intervalOut)
{
	if (m_openGLContext != nil) {
		GLint interval;

		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

		[m_openGLContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];

		[pool drain];

		intervalOut = static_cast<int>(interval);

		return GHOST_kSuccess;
	}
	else {
		return GHOST_kFailure;
	}
}


GHOST_TSuccess GHOST_ContextCGL::activateDrawingContext()
{
	if (m_openGLContext != nil) {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		[m_openGLContext makeCurrentContext];

		activateGLEW();

		[pool drain];
		return GHOST_kSuccess;
	}
	else {
		return GHOST_kFailure;
	}
}


GHOST_TSuccess GHOST_ContextCGL::updateDrawingContext()
{
	if (m_openGLContext != nil) {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		[m_openGLContext update];
		[pool drain];
		return GHOST_kSuccess;
	}
	else {
		return GHOST_kFailure;
	}
}


static void makeAttribList(
        std::vector<NSOpenGLPixelFormatAttribute>& attribs,
        bool stereoVisual,
        int numOfAASamples,
        bool needAlpha,
        bool needStencil,
        bool softwareGL)
{
	attribs.clear();

	// Pixel Format Attributes for the windowed NSOpenGLContext
	attribs.push_back(NSOpenGLPFADoubleBuffer);

	if (softwareGL) {
		attribs.push_back(NSOpenGLPFARendererID);
		attribs.push_back(kCGLRendererGenericFloatID);
	}
	else {
		attribs.push_back(NSOpenGLPFAAccelerated);
		attribs.push_back(NSOpenGLPFANoRecovery);
	}

	/* Removed to allow 10.4 builds, and 2 GPUs rendering is not used anyway */
	//attribs.push_back(NSOpenGLPFAAllowOfflineRenderers);

	attribs.push_back(NSOpenGLPFADepthSize);
	attribs.push_back((NSOpenGLPixelFormatAttribute) 32);

	attribs.push_back(NSOpenGLPFAAccumSize);
	attribs.push_back((NSOpenGLPixelFormatAttribute) 32);

	if (stereoVisual)
		attribs.push_back(NSOpenGLPFAStereo);

	if (needAlpha) {
		attribs.push_back(NSOpenGLPFAAlphaSize);
		attribs.push_back((NSOpenGLPixelFormatAttribute) 8);
	}

	if (needStencil) {
		attribs.push_back(NSOpenGLPFAStencilSize);
		attribs.push_back((NSOpenGLPixelFormatAttribute) 8);
	}

	if (numOfAASamples > 0) {
		// Multisample anti-aliasing
		attribs.push_back(NSOpenGLPFAMultisample);

		attribs.push_back(NSOpenGLPFASampleBuffers);
		attribs.push_back((NSOpenGLPixelFormatAttribute) 1);

		attribs.push_back(NSOpenGLPFASamples);
		attribs.push_back((NSOpenGLPixelFormatAttribute) numOfAASamples);
	}

	attribs.push_back((NSOpenGLPixelFormatAttribute) 0);
}

// TODO(merwin): make this available to all platforms
static void getVersion(int *major, int *minor)
{
#if 1 // legacy GL
	sscanf((const char*)glGetString(GL_VERSION), "%d.%d", major, minor);
#else // 3.0+
	glGetIntegerv(GL_MAJOR_VERSION, major);
	glGetIntegerv(GL_MINOR_VERSION, minor);
#endif
}

GHOST_TSuccess GHOST_ContextCGL::initializeDrawingContext()
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	std::vector<NSOpenGLPixelFormatAttribute> attribs;
	attribs.reserve(40);

	NSOpenGLContext *prev_openGLContext = [m_openGLView openGLContext];

#ifdef GHOST_OPENGL_ALPHA
	static const bool needAlpha   = true;
#else
	static const bool needAlpha   = false;
#endif

#ifdef GHOST_OPENGL_STENCIL
	static const bool needStencil = true;
#else
	static const bool needStencil = false;
#endif

	static bool softwareGL = getenv("BLENDER_SOFTWAREGL"); // command-line argument would be better
	GLint major = 0, minor = 0;
	NSOpenGLPixelFormat *pixelFormat;
	// TODO: keep pixel format for subsequent windows/contexts instead of recreating each time

	makeAttribList(attribs, m_stereoVisual, m_numOfAASamples, needAlpha, needStencil, softwareGL);

	pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:&attribs[0]];

	// Fall back to no multisampling if Antialiasing init failed
	if (m_numOfAASamples > 0 && pixelFormat == nil) {
		// XXX jwilkins: Does CGL only succeed when it makes an exact match on the number of samples?
		// Does this need to explicitly try for a lesser match before giving up?
		// (Now that I think about it, does WGL really require the code that it has for finding a lesser match?)

		attribs.clear();
		makeAttribList(attribs, m_stereoVisual, 0, needAlpha, needStencil, softwareGL);
		pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:&attribs[0]];
	}

	if (pixelFormat == nil)
		goto error;

	if (m_numOfAASamples > 0) { //Set m_numOfAASamples to the actual value
		GLint actualSamples;
		[pixelFormat getValues:&actualSamples forAttribute:NSOpenGLPFASamples forVirtualScreen:0];

		if (m_numOfAASamples != (GHOST_TUns16)actualSamples) {
			fprintf(stderr,
			        "Warning! Unable to find a multisample pixel format that supports exactly %d samples. "
			        "Substituting one that uses %d samples.\n",
			        m_numOfAASamples, actualSamples);

			m_numOfAASamples = (GHOST_TUns16)actualSamples;
		}
	}

	m_openGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:s_sharedOpenGLContext];
	[pixelFormat release];

	[m_openGLContext makeCurrentContext];

	getVersion(&major, &minor);
	if (m_debug) {
		fprintf(stderr, "OpenGL version %d.%d%s\n", major, minor, softwareGL ? " (software)" : "");
		fprintf(stderr, "Renderer: %s\n", glGetString(GL_RENDERER));
	}

	if (major < 2 || (major == 2 && minor < 1)) {
		// fall back to software renderer if GL < 2.1
		fprintf(stderr, "OpenGL 2.1 is not supported on your hardware, falling back to software");
		softwareGL = true;

		// discard hardware GL context
		[NSOpenGLContext clearCurrentContext];
		[m_openGLContext release];

		// create software GL context
		makeAttribList(attribs, m_stereoVisual, m_numOfAASamples, needAlpha, needStencil, softwareGL);
		pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:&attribs[0]];
		m_openGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:s_sharedOpenGLContext];
		[pixelFormat release];

		[m_openGLContext makeCurrentContext];

		getVersion(&major, &minor);
		if (m_debug) {
			fprintf(stderr, "OpenGL version %d.%d%s\n", major, minor, softwareGL ? " (software)" : "");
			fprintf(stderr, "Renderer: %s\n", glGetString(GL_RENDERER));
		}
	}

#ifdef GHOST_MULTITHREADED_OPENGL
	//Switch openGL to multhreaded mode
	CGLContextObj cglCtx = (CGLContextObj)[tmpOpenGLContext CGLContextObj];
	if (CGLEnable(cglCtx, kCGLCEMPEngine) == kCGLNoError)
		if (m_debug)
			fprintf(stderr, "\nSwitched OpenGL to multithreaded mode\n");
#endif

#ifdef GHOST_WAIT_FOR_VSYNC
	{
		GLint swapInt = 1;
		/* wait for vsync, to avoid tearing artifacts */
		[m_openGLContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
	}
#endif

	initContextGLEW();

	[m_openGLView setOpenGLContext:m_openGLContext];
	[m_openGLContext setView:m_openGLView];

	if (s_sharedCount == 0)
		s_sharedOpenGLContext = m_openGLContext;
	
	s_sharedCount++;


	initClearGL();
	[m_openGLContext flushBuffer];

	[pool drain];

	return GHOST_kSuccess;

error:

	[m_openGLView setOpenGLContext:prev_openGLContext];
	[pixelFormat release];

	[pool drain];

	return GHOST_kFailure;
}


GHOST_TSuccess GHOST_ContextCGL::releaseNativeHandles()
{
	m_openGLContext = NULL;
	m_openGLView    = NULL;

	return GHOST_kSuccess;
}
