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
 * The Original Code is Copyright (C) 2015, Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RAS_OPENGLOFFSCREEN__
#define __RAS_OPENGLOFFSCREEN__

#include "RAS_IOffScreen.h"
#include "GPU_extensions.h"

class RAS_ICanvas;

class RAS_OpenGLOffScreen : public RAS_IOffScreen
{
	RAS_ICanvas *m_canvas;
	// these are GL objects
	unsigned int m_depthrb;
	unsigned int m_colorrb;
	unsigned int m_depthtx;
	unsigned int m_colortx;
	unsigned int m_fbo;
	unsigned int m_blitfbo;
	unsigned int m_blitrbo;
	unsigned int m_blittex;
	RAS_OFS_RENDER_TARGET m_target;
	bool m_bound;


public:
	RAS_OpenGLOffScreen(RAS_ICanvas *canvas);
	~RAS_OpenGLOffScreen();

	bool Create(int width, int height, int samples, RAS_OFS_RENDER_TARGET target);
	void Destroy();
	void Bind(RAS_OFS_BIND_MODE mode);
	void Blit();
	void Unbind();
	void MipMap();
};

#endif  /* __RAS_OPENGLOFFSCREEN__ */

