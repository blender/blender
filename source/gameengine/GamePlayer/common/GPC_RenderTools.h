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

#ifndef __GPC_RENDERTOOLS_H
#define __GPC_RENDERTOOLS_H

#if defined(WIN32) || defined(__APPLE__)
	#ifdef WIN32
		#include <windows.h>
		#include <GL/gl.h>
	#else // WIN32
		// __APPLE__ is defined
		#include <AGL/gl.h>
	#endif // WIN32
#else //defined(WIN32) || defined(__APPLE__)
	#include <GL/gl.h>
#endif //defined(WIN32) || defined(__APPLE__)


#include "RAS_IRenderTools.h"

#include "BMF_Api.h"

struct KX_ClientObjectInfo;


class GPC_RenderTools : public RAS_IRenderTools
{
public:
	GPC_RenderTools();
	virtual ~GPC_RenderTools();

	virtual void EndFrame(RAS_IRasterizer* rasty);
	virtual void BeginFrame(RAS_IRasterizer* rasty);

	void DisableOpenGLLights()
	{
		glDisable(GL_LIGHTING);
		glDisable(GL_COLOR_MATERIAL);
	}

	void EnableOpenGLLights();

	int ProcessLighting(int layer);

	void Perspective(int a, int width, int height, float mat[4][4], float viewmat[4][4])
	{
		if(a== 0)
		{
			glMatrixMode(GL_PROJECTION);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
		}
		else
		{
			if(a== 1)
			{
				glMatrixMode(GL_PROJECTION);
				glMatrixMode(GL_MODELVIEW);
			}
		}
	}

	/**
	 * @attention mode is ignored here
	 */
	virtual void RenderText2D(
					RAS_TEXT_RENDER_MODE mode,
					const char* text,
					int xco,
					int yco,
					int width,
					int height);

	/**
	 * Renders text into a (series of) polygon(s), using a texture font,
	 * Each character consists of one polygon (one quad or two triangles)
	 */
	virtual void RenderText(
					int mode,
					RAS_IPolyMaterial* polymat,
					float v1[3],
					float v2[3],
					float v3[3],
					float v4[3]);

	void Render(RAS_IRasterizer* rasty,double* oglmatrix,int objectdrawmode)
	{
		glPopMatrix();
		glPushMatrix();
		glMultMatrixd(oglmatrix);
	}

	void applyTransform(RAS_IRasterizer* rasty, double* oglmatrix, int objectdrawmode);

	virtual void PushMatrix()
	{
		glPushMatrix();
	}

	virtual void PopMatrix()
	{
		glPopMatrix();
	}

	virtual class RAS_IPolyMaterial* CreateBlenderPolyMaterial(
			const STR_String &texname,
			bool ba,
			const STR_String& matname,
			int tile,
			int tilexrep,int tileyrep,
			int mode,
			bool transparant,
			bool zsort,
			int lightlayer,
			bool bIsTriangle,
			void* clientobject,
			void* tface);

	int applyLights(int objectlayer);

	bool RayHit(KX_ClientObjectInfo* client, MT_Point3& hit_point, MT_Vector3& hit_normal, void * const data);

	virtual void MotionBlur(RAS_IRasterizer* rasterizer);

	virtual void Update2DFilter(RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode, int pass, STR_String& text);

	virtual	void Render2DFilters(RAS_ICanvas* canvas);

	virtual void SetClientObject(void* obj);

protected:
	/** 
	 * Copied from KX_BlenderGL.cpp in KX_blenderhook
	 */
	void BL_RenderText(
		int mode,
		const char* textstr,
		int textlen,
		struct MTFace* tface,
		unsigned int* col,
		float v1[3],float v2[3],float v3[3],float v4[3]);
	void BL_spack(unsigned int ucol)
	{
		char *cp = (char *)&ucol;		
		glColor3ub(cp[3], cp[2], cp[1]);
	}


	BMF_Font* m_font;
	static unsigned int m_numgllights;
};

#endif  // __GPC_RENDERTOOLS_H



