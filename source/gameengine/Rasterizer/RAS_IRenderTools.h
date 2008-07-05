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
#ifndef __RAS_IRENDERTOOLS
#define __RAS_IRENDERTOOLS

#include "MT_Transform.h"
#include "RAS_IRasterizer.h"
#include "RAS_2DFilterManager.h"

#include <vector>
#include <algorithm>

class		RAS_IPolyMaterial;
struct		RAS_LightObject;


class RAS_IRenderTools
{

protected:
	float	m_viewmat[16];
	void*	m_clientobject;
	void*	m_auxilaryClientInfo;

	bool	m_modified;
	
	std::vector<struct	RAS_LightObject*> m_lights;
	
	RAS_2DFilterManager m_filtermanager;

public:
	enum RAS_TEXT_RENDER_MODE {
		RAS_TEXT_RENDER_NODEF = 0,
		RAS_TEXT_NORMAL,
		RAS_TEXT_PADDED,
		RAS_TEXT_MAX
	};
	enum RAS_LIGHT_MODE {
		RAS_LIGHT_NONE = -1,
		RAS_LIGHT_OBJECT_LAYER = 0
	};

	RAS_IRenderTools(
	) :
		m_clientobject(NULL),
		m_modified(true)
	{
	};

	virtual 
	~RAS_IRenderTools(
	) {};

	virtual 
		void	
	BeginFrame(
		RAS_IRasterizer* rasty
	)=0;

	virtual 
		void	
	EndFrame(
		RAS_IRasterizer* rasty
	)=0;

	// the following function was formerly called 'Render'
	// by it doesn't render anymore
	// It only sets the transform for the rasterizer
	// so must be renamed to 'applyTransform' or something

	virtual 
		void	
	applyTransform(
		class RAS_IRasterizer* rasty,
		double* oglmatrix,
		int drawingmode
	)=0;

	/**
	 * Renders 2D text string.
	 * @param mode      The type of text
	 * @param text		The string to render.
	 * @param xco		Position on the screen (origin in lower left corner).
	 * @param yco		Position on the screen (origin in lower left corner).
	 * @param width		Width of the canvas to draw to.
	 * @param height	Height of the canvas to draw to.
	 */
	virtual 
		void	
	RenderText2D(
		RAS_TEXT_RENDER_MODE mode,
		const char* text,
		int xco,
		int yco,
		int width,
		int height
	) = 0;

	// 3d text, mapped on polygon
	virtual 
		void	
	RenderText(
		int mode,
		RAS_IPolyMaterial* polymat,
		float v1[3],
		float v2[3],
		float v3[3],
		float v4[3]
	)=0;

	virtual 
		void	
	SetViewMat(
		const MT_Transform& trans
	);

	virtual 
		int		
	ProcessLighting(
		int layer
	)=0;

	virtual
		void	
	SetClientObject(
		void* obj
	);

		void	
	SetAuxilaryClientInfo(
		void* inf
	);

	virtual 
		void	
	PushMatrix(
	)=0;

	virtual 
		void	
	PopMatrix(
	)=0;

	virtual 
		void		
	AddLight(
		struct	RAS_LightObject* lightobject
	);

	virtual 
		void		
	RemoveLight(
		struct RAS_LightObject* lightobject
	);

	virtual
		void
	MotionBlur(RAS_IRasterizer* rasterizer)=0;

	virtual
		void
		Update2DFilter(RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode, int pass, STR_String& text, short textureflag)=0;

	virtual
		void
		Render2DFilters(RAS_ICanvas* canvas)=0;

	virtual 
		class RAS_IPolyMaterial*	
	CreateBlenderPolyMaterial(
		const STR_String &texname,
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
		void* tface
	)=0;
};

#endif //__RAS_IRENDERTOOLS



