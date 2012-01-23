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

/** \file gameengine/BlenderRoutines/KX_BlenderCanvas.cpp
 *  \ingroup blroutines
 */


#include "KX_BlenderCanvas.h"
#include "DNA_screen_types.h"
#include <stdio.h>


KX_BlenderCanvas::KX_BlenderCanvas(struct wmWindow *win, RAS_Rect &rect, struct ARegion *ar) :
m_win(win),
m_frame_rect(rect)
{
	// area boundaries needed for mouse coordinates in Letterbox framing mode
	m_area_left = ar->winrct.xmin;
	m_area_top = ar->winrct.ymax;
}

KX_BlenderCanvas::~KX_BlenderCanvas()
{
}

void KX_BlenderCanvas::Init()
{
	glDepthFunc(GL_LEQUAL);
}	


void KX_BlenderCanvas::SwapBuffers()
{
	BL_SwapBuffers(m_win);
}

void KX_BlenderCanvas::ResizeWindow(int width, int height)
{
	// Not implemented for the embedded player
}

void KX_BlenderCanvas::BeginFrame()
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

}


void KX_BlenderCanvas::EndFrame()
{
		// this is needed, else blender distorts a lot
	glPopAttrib();
	glPushAttrib(GL_ALL_ATTRIB_BITS);
		
	glDisable(GL_FOG);
}



void KX_BlenderCanvas::ClearColor(float r,float g,float b,float a)
{
	glClearColor(r,g,b,a);
}



void KX_BlenderCanvas::ClearBuffer(int type)
{
	int ogltype = 0;

	if (type & RAS_ICanvas::COLOR_BUFFER )
		ogltype |= GL_COLOR_BUFFER_BIT;

	if (type & RAS_ICanvas::DEPTH_BUFFER )
		ogltype |= GL_DEPTH_BUFFER_BIT;
	glClear(ogltype);
}

int KX_BlenderCanvas::GetWidth(
) const {
	return m_frame_rect.GetWidth();
}

int KX_BlenderCanvas::GetHeight(
) const {
	return m_frame_rect.GetHeight();
}

int KX_BlenderCanvas::GetMouseX(int x)
{
	float left = GetWindowArea().GetLeft();
	return float(x - (left - m_area_left));
}

int KX_BlenderCanvas::GetMouseY(int y)
{
	float top = GetWindowArea().GetTop();
	return float(y - (m_area_top - top));
}

float KX_BlenderCanvas::GetMouseNormalizedX(int x)
{
	int can_x = GetMouseX(x);
	return float(can_x)/this->GetWidth();
}

float KX_BlenderCanvas::GetMouseNormalizedY(int y)
{
	int can_y = GetMouseY(y);
	return float(can_y)/this->GetHeight();
}

RAS_Rect &
KX_BlenderCanvas::
GetWindowArea(
){
	return m_area_rect;
}	

	void
KX_BlenderCanvas::
SetViewPort(
	int x1, int y1,
	int x2, int y2
){
	/*	x1 and y1 are the min pixel coordinate (e.g. 0)
		x2 and y2 are the max pixel coordinate
		the width,height is calculated including both pixels
		therefore: max - min + 1
	*/
	int vp_width = (x2 - x1) + 1;
	int vp_height = (y2 - y1) + 1;
	int minx = m_frame_rect.GetLeft();
	int miny = m_frame_rect.GetBottom();

	m_area_rect.SetLeft(minx + x1);
	m_area_rect.SetBottom(miny + y1);
	m_area_rect.SetRight(minx + x2);
	m_area_rect.SetTop(miny + y2);

	glViewport(minx + x1, miny + y1, vp_width, vp_height);
	glScissor(minx + x1, miny + y1, vp_width, vp_height);
}


void KX_BlenderCanvas::SetMouseState(RAS_MouseState mousestate)
{
	m_mousestate = mousestate;

	switch (mousestate)
	{
	case MOUSE_INVISIBLE:
		{
			BL_HideMouse(m_win);
			break;
		}
	case MOUSE_WAIT:
		{
			BL_WaitMouse(m_win);
			break;
		}
	case MOUSE_NORMAL:
		{
			BL_NormalMouse(m_win);
			break;
		}
	default:
		{
		}
	}
}



//	(0,0) is top left, (width,height) is bottom right
void KX_BlenderCanvas::SetMousePosition(int x,int y)
{
	int winX = m_frame_rect.GetLeft();
	int winY = m_frame_rect.GetBottom();
	int winH = m_frame_rect.GetHeight();
	
	BL_warp_pointer(m_win, winX + x, winY + (winH-y));
}



void KX_BlenderCanvas::MakeScreenShot(const char* filename)
{
	ScrArea area_dummy= {0};
	area_dummy.totrct.xmin = m_frame_rect.GetLeft();
	area_dummy.totrct.xmax = m_frame_rect.GetRight();
	area_dummy.totrct.ymin = m_frame_rect.GetBottom();
	area_dummy.totrct.ymax = m_frame_rect.GetTop();

	BL_MakeScreenShot(&area_dummy, filename);
}
