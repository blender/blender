/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef __KX_VERTEXARRAYOPENGLRASTERIZER
#define __KX_VERTEXARRAYOPENGLRASTERIZER

#include "RAS_OpenGLRasterizer.h"

class RAS_VAOpenGLRasterizer : public RAS_OpenGLRasterizer
{
public:
	RAS_VAOpenGLRasterizer(RAS_ICanvas* canvas);
	virtual ~RAS_VAOpenGLRasterizer();

	virtual bool	Init();
	virtual void	Exit();

	virtual bool	Stereo();
	virtual void	SetDrawingMode(int drawingmode);

	virtual void	IndexPrimitives( const vecVertexArray& vertexarrays,
		const vecIndexArrays & indexarrays,
		int mode,
		class RAS_IPolyMaterial* polymat,
		class RAS_IRenderTools* rendertools,
		bool useObjectColor,
		const MT_Vector4& rgbacolor);


	virtual void	EnableTextures(bool enable);

};

#endif //__KX_VERTEXARRAYOPENGLRASTERIZER

