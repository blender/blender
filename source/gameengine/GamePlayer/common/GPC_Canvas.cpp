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

/** \file gameengine/GamePlayer/common/GPC_Canvas.cpp
 *  \ingroup player
 */


#include "RAS_IPolygonMaterial.h"
#include "GPC_Canvas.h"

#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_image.h"
#include "MEM_guardedalloc.h"


GPC_Canvas::GPC_Canvas(
	int width,
	int height
) : 
	m_width(width),
	m_height(height)
{
	// initialize area so that it's available for game logic on frame 1 (ImageViewport)
	m_displayarea.m_x1 = 0;
	m_displayarea.m_y1 = 0;
	m_displayarea.m_x2 = width;
	m_displayarea.m_y2 = height;
	m_frame = 1;

	glGetIntegerv(GL_VIEWPORT, (GLint*)m_viewport);
}


GPC_Canvas::~GPC_Canvas()
{
}


void GPC_Canvas::Resize(int width, int height)
{
	m_width = width;
	m_height = height;

	// initialize area so that it's available for game logic on frame 1 (ImageViewport)
	m_displayarea.m_x1 = 0;
	m_displayarea.m_y1 = 0;
	m_displayarea.m_x2 = width;
	m_displayarea.m_y2 = height;
}


void GPC_Canvas::ClearColor(float r, float g, float b, float a)
{
	::glClearColor(r,g,b,a);
}

void GPC_Canvas::SetViewPort(int x1, int y1, int x2, int y2)
{
		/*	x1 and y1 are the min pixel coordinate (e.g. 0)
			x2 and y2 are the max pixel coordinate
			the width,height is calculated including both pixels
			therefore: max - min + 1
		*/
		
		/* XXX, nasty, this needs to go somewhere else,
		 * but where... definitely need to clean up this
		 * whole canvas/rendertools mess.
		 */
	glEnable(GL_SCISSOR_TEST);
	
	m_viewport[0] = x1;
	m_viewport[1] = y1;
	m_viewport[2] = x2-x1 + 1;
	m_viewport[3] = y2-y1 + 1;

	glViewport(x1,y1,x2-x1 + 1,y2-y1 + 1);
	glScissor(x1,y1,x2-x1 + 1,y2-y1 + 1);
}

void GPC_Canvas::UpdateViewPort(int x1, int y1, int x2, int y2)
{
	m_viewport[0] = x1;
	m_viewport[1] = y1;
	m_viewport[2] = x2;
	m_viewport[3] = y2;
}

const int *GPC_Canvas::GetViewPort()
{
#ifdef DEBUG
	// If we're in a debug build, we might as well make sure our values don't differ
	// from what the gpu thinks we have. This could lead to nasty, hard to find bugs.
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	assert(viewport[0] == m_viewport[0]);
	assert(viewport[1] == m_viewport[1]);
	assert(viewport[2] == m_viewport[2]);
	assert(viewport[3] == m_viewport[3]);
#endif

	return m_viewport;
}

void GPC_Canvas::ClearBuffer(
	int type
) {

	int ogltype = 0;
	if (type & RAS_ICanvas::COLOR_BUFFER )
		ogltype |= GL_COLOR_BUFFER_BIT;
	if (type & RAS_ICanvas::DEPTH_BUFFER )
		ogltype |= GL_DEPTH_BUFFER_BIT;

	::glClear(ogltype);
}

	void
GPC_Canvas::
MakeScreenShot(
	const char* filename
) {
	// copy image data
	unsigned int dumpsx = GetWidth();
	unsigned int dumpsy = GetHeight();
	unsigned int *pixels = (unsigned int *)MEM_mallocN(sizeof(int) * dumpsx * dumpsy, "pixels");

	if (!pixels) {
		std::cout << "Cannot allocate pixels array" << std::endl;
		return;
	}

	glReadPixels(0, 0, dumpsx, dumpsy, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	// initialize image file format data
	ImageFormatData *im_format = (ImageFormatData *)MEM_mallocN(sizeof(ImageFormatData), "im_format");
	BKE_imformat_defaults(im_format);

	/* save_screenshot() frees dumprect and im_format */
	save_screenshot(filename, dumpsx, dumpsy, pixels, im_format);
}

