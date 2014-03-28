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
 * Contributor(s): Mitchell Stokes
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "RAS_ILightObject.h"

class RAS_OpenGLRasterizer;
struct GPULamp;
struct Image;

class RAS_OpenGLLight : public RAS_ILightObject
{

	RAS_OpenGLRasterizer *m_rasterizer;

	GPULamp *GetGPULamp();
public:
	RAS_OpenGLLight(RAS_OpenGLRasterizer *ras);
	~RAS_OpenGLLight();

	bool ApplyFixedFunctionLighting(KX_Scene *kxscene, int oblayer, int slot);

	RAS_OpenGLLight* Clone() { return new RAS_OpenGLLight(*this); }

	bool HasShadowBuffer();
	int GetShadowLayer();
	void BindShadowBuffer(RAS_ICanvas *canvas, KX_Camera *cam, MT_Transform& camtrans);
	void UnbindShadowBuffer();
	Image *GetTextureImage(short texslot);
	void Update();
};
