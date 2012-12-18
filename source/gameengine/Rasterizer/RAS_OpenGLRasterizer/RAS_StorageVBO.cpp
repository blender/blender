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

#include "RAS_StorageVBO.h"
#include "RAS_MeshObject.h"

#include "GL/glew.h"

VBO::VBO(RAS_DisplayArray *data, unsigned int indices)
{
	this->data = data;
	this->size = data->m_vertex.size();
	this->indices = indices;
	this->stride = 32*sizeof(GLfloat); // ATI cards really like 32byte aligned VBOs, so we add a little padding

	//	Determine drawmode
	if (data->m_type == data->QUAD)
		this->mode = GL_QUADS;
	else if (data->m_type == data->TRIANGLE)
		this->mode = GL_TRIANGLES;
	else
		this->mode = GL_LINE;

	// Generate Buffers
	glGenBuffersARB(1, &this->ibo);
	glGenBuffersARB(1, &this->vbo_id);

	// Fill the buffers with initial data
	UpdateIndices();
	UpdateData();

	// Establish offsets
	this->vertex_offset = 0;
	this->normal_offset = (void*)(3*sizeof(GLfloat));
	this->tangent_offset = (void*)(6*sizeof(GLfloat));
	this->color_offset = (void*)(10*sizeof(GLfloat));
	this->uv_offset = (void*)(11*sizeof(GLfloat));
}

VBO::~VBO()
{
	glDeleteBuffersARB(1, &this->ibo);
	glDeleteBuffersARB(1, &this->vbo_id);
}

void VBO::UpdateData()
{
	unsigned int i, j, k;
	
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->vbo_id);
	glBufferData(GL_ARRAY_BUFFER, this->stride*this->size, NULL, GL_STATIC_DRAW);

	// Map the buffer
	GLfloat *vbo_map = (GLfloat*)glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

	// Gather data
	for (i = 0, j = 0; i < data->m_vertex.size(); i++, j += this->stride/sizeof(GLfloat))
	{
		memcpy(&vbo_map[j], data->m_vertex[i].getXYZ(), sizeof(float)*3);
		memcpy(&vbo_map[j+3], data->m_vertex[i].getNormal(), sizeof(float)*3);
		memcpy(&vbo_map[j+6], data->m_vertex[i].getTangent(), sizeof(float)*4);
		memcpy(&vbo_map[j+10], data->m_vertex[i].getRGBA(), sizeof(char)*4);

		for (k = 0; k < RAS_TexVert::MAX_UNIT; k++)
			memcpy(&vbo_map[j+11+(k*2)], data->m_vertex[i].getUV(k), sizeof(float)*2);
	}
	
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
}

void VBO::UpdateIndices()
{
	int space = data->m_index.size() * sizeof(GLushort);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, this->ibo);

	// Upload Data to VBO
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, space, &data->m_index[0], GL_STATIC_DRAW);
}

void VBO::Draw(int texco_num, RAS_IRasterizer::TexCoGen* texco, int attrib_num, RAS_IRasterizer::TexCoGen* attrib, bool multi)
{
	int unit;
	
	// Bind buffers
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, this->ibo);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, this->vbo_id);

	// Vertexes
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, this->stride, this->vertex_offset);

	// Normals
	glEnableClientState(GL_NORMAL_ARRAY);
	glNormalPointer(GL_FLOAT, this->stride, this->normal_offset);

	// Colors
	glEnableClientState(GL_COLOR_ARRAY);
	glColorPointer(4, GL_UNSIGNED_BYTE, this->stride, this->color_offset);

	if (multi)
	{
		for (unit = 0; unit < texco_num; ++unit)
		{
			glClientActiveTexture(GL_TEXTURE0_ARB + unit);
			switch (texco[unit]) {
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glTexCoordPointer(3, GL_FLOAT, this->stride, this->vertex_offset);
					break;
				case RAS_IRasterizer::RAS_TEXCO_UV:
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glTexCoordPointer(2, GL_FLOAT, this->stride, (void*)((intptr_t)this->uv_offset+(sizeof(GLfloat)*2*unit)));
					break;
				case RAS_IRasterizer::RAS_TEXCO_NORM:
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glTexCoordPointer(3, GL_FLOAT, this->stride, this->normal_offset);
					break;
				case RAS_IRasterizer::RAS_TEXTANGENT:
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glTexCoordPointer(4, GL_FLOAT, this->stride, this->tangent_offset);
					break;
				default:
					break;
			}
		}
		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else //TexFace
	{
		glClientActiveTextureARB(GL_TEXTURE0_ARB);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, this->stride, this->uv_offset);
	}

	if (GLEW_ARB_vertex_program)
	{
		int uv = 0;
		for (unit = 0; unit < attrib_num; ++unit)
		{
			switch (attrib[unit]) {
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
					glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, this->stride, this->vertex_offset);
					glEnableVertexAttribArrayARB(unit);
					break;
				case RAS_IRasterizer::RAS_TEXCO_UV:
					glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, this->stride, (void*)((intptr_t)this->uv_offset+uv));
					uv += sizeof(GLfloat)*2;
					glEnableVertexAttribArrayARB(unit);
					break;
				case RAS_IRasterizer::RAS_TEXCO_NORM:
					glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, stride, this->normal_offset);
					glEnableVertexAttribArrayARB(unit);
					break;
				case RAS_IRasterizer::RAS_TEXTANGENT:
					glVertexAttribPointerARB(unit, 4, GL_FLOAT, GL_FALSE, this->stride, this->tangent_offset);
					glEnableVertexAttribArrayARB(unit);
					break;
				default:
					break;
			}
		}
	}
	
	glDrawElements(this->mode, this->indices, GL_UNSIGNED_SHORT, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (GLEW_ARB_vertex_program)
	{
		for (int i = 0; i < attrib_num; ++i)
			glDisableVertexAttribArrayARB(i);
	}

	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
 }

RAS_StorageVBO::RAS_StorageVBO(int *texco_num, RAS_IRasterizer::TexCoGen *texco, int *attrib_num, RAS_IRasterizer::TexCoGen *attrib):
	m_texco_num(texco_num),
	m_texco(texco),
	m_attrib_num(attrib_num),
	m_attrib(attrib)
{
}

RAS_StorageVBO::~RAS_StorageVBO()
{
}

bool RAS_StorageVBO::Init()
{
	return true;
}

void RAS_StorageVBO::Exit()
{
	m_vbo_lookup.clear();
}

void RAS_StorageVBO::IndexPrimitives(RAS_MeshSlot& ms)
{
	IndexPrimitivesInternal(ms, false);
}

void RAS_StorageVBO::IndexPrimitivesMulti(RAS_MeshSlot& ms)
{
	IndexPrimitivesInternal(ms, true);
}

void RAS_StorageVBO::IndexPrimitivesInternal(RAS_MeshSlot& ms, bool multi)
{
	RAS_MeshSlot::iterator it;
	VBO *vbo;
	
	for (ms.begin(it); !ms.end(it); ms.next(it))
	{
		vbo = m_vbo_lookup[it.array];

		if (vbo == 0)
			m_vbo_lookup[it.array] = vbo = new VBO(it.array, it.totindex);

		// Update the vbo
		if (ms.m_mesh->MeshModified())
		{
			vbo->UpdateData();
		}

		vbo->Draw(*m_texco_num, m_texco, *m_attrib_num, m_attrib, multi);
	}
}
