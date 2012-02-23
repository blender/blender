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

/** \file RAS_2DFilterManager.h
 *  \ingroup bgerast
 */

#ifndef __RAS_I2DFILTER
#define __RAS_I2DFILTER

#include "RAS_ICanvas.h"
#define MAX_RENDER_PASS	100

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class RAS_2DFilterManager
{
private:
	unsigned int	CreateShaderProgram(const char* shadersource);
	unsigned int	CreateShaderProgram(int filtermode);
	void		AnalyseShader(int passindex, std::vector<STR_String>& propNames);
	void			StartShaderProgram(int passindex);
	void			EndShaderProgram();
	void			PrintShaderErrors(unsigned int shader, const char *task, const char *code);

	void SetupTextures(bool depth, bool luminance);
	void FreeTextures();

	void UpdateOffsetMatrix(RAS_ICanvas* canvas);
	void UpdateCanvasTextureCoord(unsigned int * viewport);
 
	float			canvascoord[4];
	float			textureoffsets[18];
	float			view[4];
	/* texname[0] contains render to texture, texname[1] contains depth texture,  texname[2] contains luminance texture*/
	unsigned int	texname[3]; 
	int				texturewidth;
	int				textureheight;
	int				canvaswidth;
	int				canvasheight;
	int				numberoffilters;
	/* bit 0: enable/disable depth texture
	 * bit 1: enable/disable luminance texture*/
	short			texflag[MAX_RENDER_PASS];

	bool			isshadersupported;
	bool			errorprinted;
	bool			need_tex_update;

	unsigned int	m_filters[MAX_RENDER_PASS];
	short		m_enabled[MAX_RENDER_PASS];

	// stores object properties to send to shaders in each pass
	std::vector<STR_String>	m_properties[MAX_RENDER_PASS];
	void* m_gameObjects[MAX_RENDER_PASS];
public:
	enum RAS_2DFILTER_MODE {
		RAS_2DFILTER_ENABLED = -2,
		RAS_2DFILTER_DISABLED = -1,
		RAS_2DFILTER_NOFILTER = 0,
		RAS_2DFILTER_MOTIONBLUR,
		RAS_2DFILTER_BLUR,
		RAS_2DFILTER_SHARPEN,
		RAS_2DFILTER_DILATION,
		RAS_2DFILTER_EROSION,
		RAS_2DFILTER_LAPLACIAN,
		RAS_2DFILTER_SOBEL,
		RAS_2DFILTER_PREWITT,
		RAS_2DFILTER_GRAYSCALE,
		RAS_2DFILTER_SEPIA,
		RAS_2DFILTER_INVERT,
		RAS_2DFILTER_CUSTOMFILTER,
		RAS_2DFILTER_NUMBER_OF_FILTERS
	};

	RAS_2DFilterManager();

	~RAS_2DFilterManager();

	void RenderFilters(RAS_ICanvas* canvas);

	void EnableFilter(std::vector<STR_String>& propNames, void* gameObj, RAS_2DFILTER_MODE mode, int pass, STR_String& text);
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:RAS_2DFilterManager"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};
#endif
