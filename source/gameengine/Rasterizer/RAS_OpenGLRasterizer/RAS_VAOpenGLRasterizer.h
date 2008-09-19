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
#ifndef __KX_VERTEXARRAYOPENGLRASTERIZER
#define __KX_VERTEXARRAYOPENGLRASTERIZER

#include "RAS_OpenGLRasterizer.h"

class RAS_VAOpenGLRasterizer : public RAS_OpenGLRasterizer
{
	void TexCoordPtr(const RAS_TexVert *tv);
	bool m_Lock;

	TexCoGen		m_last_texco[RAS_MAX_TEXCO];
	TexCoGen		m_last_attrib[RAS_MAX_ATTRIB];
	int				m_last_texco_num;
	int				m_last_attrib_num;

public:
	RAS_VAOpenGLRasterizer(RAS_ICanvas* canvas, bool lock=false);
	virtual ~RAS_VAOpenGLRasterizer();

	virtual bool	Init();
	virtual void	Exit();

	virtual void	SetDrawingMode(int drawingmode);

	virtual void	IndexPrimitives( const vecVertexArray& vertexarrays,
		const vecIndexArrays & indexarrays,
		DrawMode mode,
		bool useObjectColor,
		const MT_Vector4& rgbacolor,
		class KX_ListSlot** slot);

	virtual void IndexPrimitivesMulti( 
		const vecVertexArray& vertexarrays,
		const vecIndexArrays & indexarrays,
		DrawMode mode,
		bool useObjectColor,
		const MT_Vector4& rgbacolor,
		class KX_ListSlot** slot);


	virtual void	EnableTextures(bool enable);
	//virtual bool	QueryArrays(){return true;}
	//virtual bool	QueryLists(){return m_Lock;}

};

#endif //__KX_VERTEXARRAYOPENGLRASTERIZER

