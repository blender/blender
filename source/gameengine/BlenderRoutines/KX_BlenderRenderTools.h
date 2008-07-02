/**
 * $Id$
 *
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
#ifndef __KX_BLENDERRENDERTOOLS
#define __KX_BLENDERRENDERTOOLS

#ifdef WIN32
// don't show stl-warnings
#pragma warning (disable:4786)
#endif

#include "RAS_IRenderTools.h"

struct KX_ClientObjectInfo;

/**
BlenderRenderTools are a set of tools to apply 2D/3D graphics effects, which are not
part of the (polygon) Rasterizer. 
Effects like 2D text, 3D (polygon) text, lighting.
*/

class KX_BlenderRenderTools  : public RAS_IRenderTools
{
	bool	m_lastblenderlights;
	void*	m_lastblenderobject;
	int		m_lastlayer;
	bool	m_lastlighting;
	static unsigned int m_numgllights;
	
	
public:
	
						KX_BlenderRenderTools();
	virtual				~KX_BlenderRenderTools();	

	virtual void		EndFrame(class RAS_IRasterizer* rasty);
	virtual void		BeginFrame(class RAS_IRasterizer* rasty);
	void				DisableOpenGLLights();
	void				EnableOpenGLLights();
	int					ProcessLighting(int layer);

	virtual void	    RenderText2D(RAS_TEXT_RENDER_MODE mode,
									 const char* text,
									 int xco,
									 int yco,
									 int width,
									 int height);
	virtual void		RenderText(int mode,
								   class RAS_IPolyMaterial* polymat,
								   float v1[3],
								   float v2[3],
								   float v3[3],
								   float v4[3]);
	void				applyTransform(class RAS_IRasterizer* rasty,
									   double* oglmatrix,
									   int objectdrawmode );
	int					applyLights(int objectlayer);
	virtual void		PushMatrix();
	virtual void		PopMatrix();

	virtual class RAS_IPolyMaterial* CreateBlenderPolyMaterial(const STR_String &texname,
									bool ba,
									const STR_String& matname,
									int tile,
									int tilexrep,
									int tileyrep,
									int mode,
									bool transparant,
									bool zsort,
									int lightlayer,
									bool bIsTriangle,
									void* clientobject,
									void* tface);
	
	bool RayHit(KX_ClientObjectInfo* client, MT_Point3& hit_point, MT_Vector3& hit_normal, void * const data);

	virtual void MotionBlur(RAS_IRasterizer* rasterizer);

	virtual void Update2DFilter(RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode, int pass, STR_String& text, short texture_flag);

	virtual	void Render2DFilters(RAS_ICanvas* canvas);

	virtual void SetClientObject(void* obj);

};

#endif //__KX_BLENDERRENDERTOOLS



