/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Simple test file for the bitmap font library using GHOST.
 * @author	Maarten Gribnau
 * @date	November 15, 2001
 */

#include <iostream>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define FALSE 0
#define TRUE 1

#if defined(WIN32) || defined(__APPLE__)
#ifdef WIN32
#include <windows.h>
#include <GL/gl.h>
#else // WIN32
// __APPLE__ is defined
#include <AGL/gl.h>
#endif // WIN32
#else // defined(WIN32) || defined(__APPLE__)
#include <GL/gl.h>
#endif // defined(WIN32) || defined(__APPLE__)


#include "STR_String.h"
#include "GHOST_Rect.h"

#include "GHOST_ISystem.h"
#include "GHOST_IEvent.h"
#include "GHOST_IEventConsumer.h"

#include "BMF_Api.h"

static class Application* fApp;
static GHOST_ISystem* fSystem = 0;


static void drawGL()
{
	GLint x = 10, y = 10;

	::glRasterPos2i(x, y);
	BMF_Font *font = BMF_GetFont(BMF_kHelvetica10);
	BMF_DrawString(font, "Helvetica 10 point");
	y += 14;
	::glRasterPos2i(x, y);
	font = BMF_GetFont(BMF_kHelvetica12);
	BMF_DrawString(font, "Helvetica 12 point");
	y += 16;
	::glRasterPos2i(x, y);
	font = BMF_GetFont(BMF_kHelveticaBold8);
	BMF_DrawString(font, "Helvetica Bold 8 point");
	y += 12;
	::glRasterPos2i(x, y);
	font = BMF_GetFont(BMF_kHelveticaBold10);
	BMF_DrawString(font, "Helvetica Bold 10 point");
	y += 14;
	::glRasterPos2i(x, y);
	font = BMF_GetFont(BMF_kHelveticaBold12);
	BMF_DrawString(font, "Helvetica Bold 12 point");
	y += 16;
	::glRasterPos2i(x, y);
	font = BMF_GetFont(BMF_kHelveticaBold14);
	BMF_DrawString(font, "Helvetica Bold 14 point");
	y += 18;
	::glRasterPos2i(x, y);
	font = BMF_GetFont(BMF_kScreen12);
	BMF_DrawString(font, "Screen 12 point");
	y += 16;
	::glRasterPos2i(x, y);
	font = BMF_GetFont(BMF_kScreen14);
	BMF_DrawString(font, "Screen 14 point");
	y += 18;
	::glRasterPos2i(x, y);
	font = BMF_GetFont(BMF_kScreen15);
	BMF_DrawString(font, "Screen 15 point");
}


static void setViewPortGL(GHOST_IWindow* window)
{
    window->activateDrawingContext();
    GHOST_Rect bnds;
	window->getClientBounds(bnds);

	::glViewport(0, 0, bnds.getWidth(), bnds.getHeight());

	::glMatrixMode(GL_PROJECTION);
	::glLoadIdentity();
	::glOrtho(0, bnds.getWidth(), 0, bnds.getHeight(), -10, 10);
	
	::glClearColor(.2f,0.0f,0.0f,0.0f);
	::glClear(GL_COLOR_BUFFER_BIT);
}



class Application : public GHOST_IEventConsumer {
public:
	Application(GHOST_ISystem* system);
	virtual	bool processEvent(GHOST_IEvent* event);

	GHOST_ISystem* m_system;
	GHOST_IWindow* m_mainWindow;
	bool m_exitRequested;
};


Application::Application(GHOST_ISystem* system)
	: m_system(system), m_mainWindow(0), m_exitRequested(false)
{
	fApp = this;

	// Create the main window
	STR_String title1 ("gears - main window");
	m_mainWindow = system->createWindow(title1, 10, 64, 320, 200, GHOST_kWindowStateNormal, GHOST_kDrawingContextTypeOpenGL,FALSE);
    if (!m_mainWindow) {
		std::cout << "could not create main window\n";
        exit(-1);
    }
}


bool Application::processEvent(GHOST_IEvent* event)
{
	bool handled = true;

	switch (event->getType()) {
	case GHOST_kEventWindowClose:
		{
		GHOST_IWindow* window2 = event->getWindow();
		if (window2 == m_mainWindow) {
			exit(0);
		} else {
			m_system->disposeWindow(window2);
		}
 		}
		break;

	case GHOST_kEventWindowActivate:
		handled = false;
		break;
	case GHOST_kEventWindowDeactivate:
		handled = false;
		break;
	case GHOST_kEventWindowUpdate:
		{
			GHOST_IWindow* window2 = event->getWindow();
			if (!m_system->validWindow(window2)) break;
			{
				setViewPortGL(window2);
				drawGL();
				window2->swapBuffers();
			}
		}
		break;
		
	default:
		handled = false;
		break;
	}
	return handled;
}


int main(int /*argc*/, char** /*argv*/)
{
	// Create the system
	GHOST_ISystem::createSystem();
	fSystem = GHOST_ISystem::getSystem();

	if (fSystem) {
		// Create an application object
		Application app (fSystem);

		// Add the application as event consumer
		fSystem->addEventConsumer(&app);

		// Enter main loop
		while (!app.m_exitRequested) {
			fSystem->processEvents(TRUE);
			fSystem->dispatchEvents();
		}
	}

	// Dispose the system
	GHOST_ISystem::disposeSystem();

	return 0;
}

