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

#ifndef __KX_VERTEXBUFFEROBJECTSTORAGE
#define __KX_VERTEXBUFFEROBJECTSTORAGE

#include <map>
#include "glew-mx.h"

#include "RAS_IStorage.h"
#include "RAS_IRasterizer.h"

#include "RAS_OpenGLRasterizer.h"

class VBO
{
public:
	VBO(RAS_DisplayArray *data, unsigned int indices);
	~VBO();

	void	Draw(int texco_num, RAS_IRasterizer::TexCoGen* texco, int attrib_num, RAS_IRasterizer::TexCoGen* attrib, int *attrib_layer, bool multi);

	void	UpdateData();
	void	UpdateIndices();
private:
	RAS_DisplayArray*	data;
	GLuint			size;
	GLuint			stride;
	GLuint			indices;
	GLenum			mode;
	GLuint			ibo;
	GLuint			vbo_id;

	void*			vertex_offset;
	void*			normal_offset;
	void*			color_offset;
	void*			tangent_offset;
	void*			uv_offset;
};

class RAS_StorageVBO : public RAS_IStorage
{

public:
	RAS_StorageVBO(int *texco_num, RAS_IRasterizer::TexCoGen *texco, int *attrib_num, RAS_IRasterizer::TexCoGen *attrib, int *attrib_layer);
	virtual ~RAS_StorageVBO();

	virtual bool	Init();
	virtual void	Exit();

	virtual void	IndexPrimitives(RAS_MeshSlot& ms);
	virtual void	IndexPrimitivesMulti(RAS_MeshSlot& ms);

	virtual void	SetDrawingMode(int drawingmode){m_drawingmode=drawingmode;};

protected:
	int				m_drawingmode;

	int*			m_texco_num;
	int*			m_attrib_num;

	RAS_IRasterizer::TexCoGen*		m_texco;
	RAS_IRasterizer::TexCoGen*		m_attrib;
	int*			                m_attrib_layer;

	std::map<RAS_DisplayArray*, class VBO*>	m_vbo_lookup;

	virtual void			IndexPrimitivesInternal(RAS_MeshSlot& ms, bool multi);

#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:RAS_StorageVA"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__KX_VERTEXBUFFEROBJECTSTORAGE
