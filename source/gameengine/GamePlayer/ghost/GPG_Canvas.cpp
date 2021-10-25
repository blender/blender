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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/GamePlayer/ghost/GPG_Canvas.cpp
 *  \ingroup player
 */


#include "GPG_Canvas.h"
#include <assert.h>
#include "GHOST_ISystem.h"

GPG_Canvas::GPG_Canvas(GHOST_IWindow* window)
: GPC_Canvas(0, 0), m_window(window)
{
	if (m_window)
	{
		GHOST_Rect bnds;
		m_window->getClientBounds(bnds);
		this->Resize(bnds.getWidth(), bnds.getHeight());
	}
}


GPG_Canvas::~GPG_Canvas(void)
{
}


void GPG_Canvas::Init()
{
	if (m_window)
	{
		m_window->setDrawingContextType(GHOST_kDrawingContextTypeOpenGL);
		assert(m_window->getDrawingContextType() == GHOST_kDrawingContextTypeOpenGL);
	}
}

void GPG_Canvas::SetMousePosition(int x, int y)
{
	GHOST_ISystem* system = GHOST_ISystem::getSystem();
	if (system && m_window)
	{
		GHOST_TInt32 gx = (GHOST_TInt32)x;
		GHOST_TInt32 gy = (GHOST_TInt32)y;
		GHOST_TInt32 cx;
		GHOST_TInt32 cy;
		m_window->clientToScreen(gx, gy, cx, cy);
		system->setCursorPosition(cx, cy);
	}
}


void GPG_Canvas::SetMouseState(RAS_MouseState mousestate)
{
	m_mousestate = mousestate;

	if (m_window)
	{
		switch (mousestate)
		{
		case MOUSE_INVISIBLE:
			m_window->setCursorVisibility(false);
			break;
		case MOUSE_WAIT:
			m_window->setCursorShape(GHOST_kStandardCursorWait);
			m_window->setCursorVisibility(true);
			break;
		case MOUSE_NORMAL:
			m_window->setCursorShape(GHOST_kStandardCursorDefault);
			m_window->setCursorVisibility(true);
			break;
		}
	}
}


void GPG_Canvas::SwapBuffers()
{
	if (m_window)
	{
		m_window->swapBuffers();
	}
}

void GPG_Canvas::SetSwapInterval(int interval)
{
	if (m_window)
		m_window->setSwapInterval(interval);
}

bool GPG_Canvas::GetSwapInterval(int& intervalOut)
{
	if (m_window)
		return (bool)m_window->getSwapInterval(intervalOut);

	return false;
}

void GPG_Canvas::GetDisplayDimensions(int &width, int &height)
 {
	unsigned int uiwidth;
	unsigned int uiheight;

	GHOST_ISystem *system = GHOST_ISystem::getSystem();
	system->getMainDisplayDimensions(uiwidth, uiheight);

	width = uiwidth;
	height = uiheight;
}

void GPG_Canvas::ResizeWindow(int width, int height)
{
	if (m_window->getState() == GHOST_kWindowStateFullScreen)
	{
		GHOST_ISystem* system = GHOST_ISystem::getSystem();
		GHOST_DisplaySetting setting;
		setting.xPixels = width;
		setting.yPixels = height;
		//XXX allow these to be changed or kept from previous state
		setting.bpp = 32;
		setting.frequency = 60;

		system->updateFullScreen(setting, &m_window);
	}

	m_window->setClientSize(width, height);

	Resize(width, height);
}

void GPG_Canvas::SetFullScreen(bool enable)
{
	if (enable)
		m_window->setState(GHOST_kWindowStateFullScreen);
	else
		m_window->setState(GHOST_kWindowStateNormal);
}

bool GPG_Canvas::GetFullScreen()
{
	return m_window->getState() == GHOST_kWindowStateFullScreen;
}

float GPG_Canvas::GetMouseNormalizedX(int x)
{
	return float(x)/this->GetWidth();
}

float GPG_Canvas::GetMouseNormalizedY(int y)
{
	return float(y)/this->GetHeight();
}
