/**
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

#ifndef __RAS_I2DFILTER
#define __RAS_I2DFILTER



#define MAX_RENDER_PASS	100

class RAS_2DFilterManager
{
private:
	unsigned int	CreateShaderProgram(char* shadersource);
	unsigned int	CreateShaderProgram(int filtermode);
	void			StartShaderProgram(int passindex);
	void			EndShaderProgram();

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

	unsigned int	m_filters[MAX_RENDER_PASS];
	short			m_enabled[MAX_RENDER_PASS];

	RAS_2DFilterManager();

	~RAS_2DFilterManager();

	void SetupTexture();

	void UpdateOffsetMatrix(int width, int height);

	void RenderFilters(RAS_ICanvas* canvas);

	void EnableFilter(RAS_2DFILTER_MODE mode, int pass, STR_String& text, short tflag);
};
#endif
