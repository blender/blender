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

/** \file gameengine/Rasterizer/RAS_OpenGLRasterizer/RAS_VAOpenGLRasterizer.cpp
 *  \ingroup bgerastogl
 */

#include "RAS_VAOpenGLRasterizer.h"
#include <stdlib.h>

#include "GL/glew.h"
#include "GPU_extensions.h"

#include "STR_String.h"
#include "RAS_TexVert.h"
#include "MT_CmMatrix4x4.h"
#include "RAS_IRenderTools.h" // rendering text
	
RAS_VAOpenGLRasterizer::RAS_VAOpenGLRasterizer(RAS_ICanvas* canvas, bool lock)
:	RAS_OpenGLRasterizer(canvas),
	m_Lock(lock && GLEW_EXT_compiled_vertex_array),
	m_last_texco_num(0),
	m_last_attrib_num(0)
{
}

RAS_VAOpenGLRasterizer::~RAS_VAOpenGLRasterizer()
{
}

bool RAS_VAOpenGLRasterizer::Init(void)
{
	
	bool result = RAS_OpenGLRasterizer::Init();
	
	if (result)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	return result;
}

void RAS_VAOpenGLRasterizer::SetDrawingMode(int drawingmode)
{
	m_drawingmode = drawingmode;

	switch (m_drawingmode)
	{
		case KX_BOUNDINGBOX:
		case KX_WIREFRAME:
			//glDisableClientState(GL_COLOR_ARRAY);
			//glDisable(GL_CULL_FACE);
			break;
		case KX_SOLID:
			//glDisableClientState(GL_COLOR_ARRAY);
			break;
		case KX_TEXTURED:
		case KX_SHADED:
		case KX_SHADOW:
			//glEnableClientState(GL_COLOR_ARRAY);
		default:
			break;
	}
}

void RAS_VAOpenGLRasterizer::Exit()
{
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	RAS_OpenGLRasterizer::Exit();
}

void RAS_VAOpenGLRasterizer::IndexPrimitives(RAS_MeshSlot& ms)
{
	static const GLsizei stride = sizeof(RAS_TexVert);
	bool wireframe = m_drawingmode <= KX_WIREFRAME;
	RAS_MeshSlot::iterator it;
	GLenum drawmode;

	if (ms.m_pDerivedMesh) {
		// cannot be handled here, pass to RAS_OpenGLRasterizer
		RAS_OpenGLRasterizer::IndexPrimitivesInternal(ms, false);
		return;
	}

	if (!wireframe)
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	// use glDrawElements to draw each vertexarray
	for (ms.begin(it); !ms.end(it); ms.next(it)) {
		if (it.totindex == 0)
			continue;

		// drawing mode
		if (it.array->m_type == RAS_DisplayArray::TRIANGLE)
			drawmode = GL_TRIANGLES;
		else if (it.array->m_type == RAS_DisplayArray::QUAD)
			drawmode = GL_QUADS;
		else
			drawmode = GL_LINES;

		// colors
		if (drawmode != GL_LINES && !wireframe) {
			if (ms.m_bObjectColor) {
				const MT_Vector4& rgba = ms.m_RGBAcolor;

				glDisableClientState(GL_COLOR_ARRAY);
				glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			}
			else {
				glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
				glEnableClientState(GL_COLOR_ARRAY);
			}
		}
		else
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

		glVertexPointer(3, GL_FLOAT, stride, it.vertex->getXYZ());
		glNormalPointer(GL_FLOAT, stride, it.vertex->getNormal());
		if (!wireframe) {
			glTexCoordPointer(2, GL_FLOAT, stride, it.vertex->getUV1());
			if (glIsEnabled(GL_COLOR_ARRAY))
				glColorPointer(4, GL_UNSIGNED_BYTE, stride, it.vertex->getRGBA());
		}

		// here the actual drawing takes places
		glDrawElements(drawmode, it.totindex, GL_UNSIGNED_SHORT, it.index);
	}

	if (!wireframe) {
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

void RAS_VAOpenGLRasterizer::IndexPrimitivesMulti(RAS_MeshSlot& ms)
{
	static const GLsizei stride = sizeof(RAS_TexVert);
	bool wireframe = m_drawingmode <= KX_WIREFRAME;
	RAS_MeshSlot::iterator it;
	GLenum drawmode;

	if (ms.m_pDerivedMesh) {
		// cannot be handled here, pass to RAS_OpenGLRasterizer
		RAS_OpenGLRasterizer::IndexPrimitivesInternal(ms, true);
		return;
	}

	if (!wireframe)
		EnableTextures(true);

	// use glDrawElements to draw each vertexarray
	for (ms.begin(it); !ms.end(it); ms.next(it)) {
		if (it.totindex == 0)
			continue;

		// drawing mode
		if (it.array->m_type == RAS_DisplayArray::TRIANGLE)
			drawmode = GL_TRIANGLES;
		else if (it.array->m_type == RAS_DisplayArray::QUAD)
			drawmode = GL_QUADS;
		else
			drawmode = GL_LINES;

		// colors
		if (drawmode != GL_LINES && !wireframe) {
			if (ms.m_bObjectColor) {
				const MT_Vector4& rgba = ms.m_RGBAcolor;

				glDisableClientState(GL_COLOR_ARRAY);
				glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			}
			else {
				glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
				glEnableClientState(GL_COLOR_ARRAY);
			}
		}
		else
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

		glVertexPointer(3, GL_FLOAT, stride, it.vertex->getXYZ());
		glNormalPointer(GL_FLOAT, stride, it.vertex->getNormal());
		if (!wireframe) {
			TexCoordPtr(it.vertex);
			if (glIsEnabled(GL_COLOR_ARRAY))
				glColorPointer(4, GL_UNSIGNED_BYTE, stride, it.vertex->getRGBA());
		}

		// here the actual drawing takes places
		glDrawElements(drawmode, it.totindex, GL_UNSIGNED_SHORT, it.index);
	}

	if (!wireframe) {
		glDisableClientState(GL_COLOR_ARRAY);
		EnableTextures(false);
	}
}

void RAS_VAOpenGLRasterizer::TexCoordPtr(const RAS_TexVert *tv)
{
	/* note: this function must closely match EnableTextures to enable/disable
	 * the right arrays, otherwise coordinate and attribute pointers from other
	 * materials can still be used and cause crashes */
	int unit;

	if (GLEW_ARB_multitexture)
	{
		for (unit=0; unit<m_texco_num; unit++)
		{
			glClientActiveTextureARB(GL_TEXTURE0_ARB+unit);
			if (tv->getFlag() & RAS_TexVert::SECOND_UV && (int)tv->getUnit() == unit) {
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(2, GL_FLOAT, sizeof(RAS_TexVert), tv->getUV2());
				continue;
			}
			switch(m_texco[unit])
			{
			case RAS_TEXCO_ORCO:
			case RAS_TEXCO_GLOB:
				glTexCoordPointer(3, GL_FLOAT, sizeof(RAS_TexVert),tv->getXYZ());
				break;
			case RAS_TEXCO_UV1:
				glTexCoordPointer(2, GL_FLOAT, sizeof(RAS_TexVert),tv->getUV1());
				break;
			case RAS_TEXCO_NORM:
				glTexCoordPointer(3, GL_FLOAT, sizeof(RAS_TexVert),tv->getNormal());
				break;
			case RAS_TEXTANGENT:
				glTexCoordPointer(4, GL_FLOAT, sizeof(RAS_TexVert),tv->getTangent());
				break;
			case RAS_TEXCO_UV2:
				glTexCoordPointer(2, GL_FLOAT, sizeof(RAS_TexVert),tv->getUV2());
				break;
			default:
				break;
			}
		}

		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}

	if (GLEW_ARB_vertex_program) {
		for (unit=0; unit<m_attrib_num; unit++) {
			switch(m_attrib[unit]) {
			case RAS_TEXCO_ORCO:
			case RAS_TEXCO_GLOB:
				glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getXYZ());
				break;
			case RAS_TEXCO_UV1:
				glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getUV1());
				break;
			case RAS_TEXCO_NORM:
				glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getNormal());
				break;
			case RAS_TEXTANGENT:
				glVertexAttribPointerARB(unit, 4, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getTangent());
				break;
			case RAS_TEXCO_UV2:
				glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getUV2());
				break;
			case RAS_TEXCO_VCOL:
				glVertexAttribPointerARB(unit, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(RAS_TexVert), tv->getRGBA());
				break;
			default:
				break;
			}
		}
	}
}

void RAS_VAOpenGLRasterizer::EnableTextures(bool enable)
{
	TexCoGen *texco, *attrib;
	int unit, texco_num, attrib_num;

	/* we cache last texcoords and attribs to ensure we disable the ones that
	 * were actually last set */
	if (enable) {
		texco = m_texco;
		texco_num = m_texco_num;
		attrib = m_attrib;
		attrib_num = m_attrib_num;
		
		memcpy(m_last_texco, m_texco, sizeof(TexCoGen)*m_texco_num);
		m_last_texco_num = m_texco_num;
		memcpy(m_last_attrib, m_attrib, sizeof(TexCoGen)*m_attrib_num);
		m_last_attrib_num = m_attrib_num;
	}
	else {
		texco = m_last_texco;
		texco_num = m_last_texco_num;
		attrib = m_last_attrib;
		attrib_num = m_last_attrib_num;
	}

	if (GLEW_ARB_multitexture) {
		for (unit=0; unit<texco_num; unit++) {
			glClientActiveTextureARB(GL_TEXTURE0_ARB+unit);

			switch(texco[unit])
			{
			case RAS_TEXCO_ORCO:
			case RAS_TEXCO_GLOB:
			case RAS_TEXCO_UV1:
			case RAS_TEXCO_NORM:
			case RAS_TEXTANGENT:
			case RAS_TEXCO_UV2:
				if (enable) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				break;
			default:
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				break;
			}
		}

		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else {
		if (texco_num) {
			if (enable) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	if (GLEW_ARB_vertex_program) {
		for (unit=0; unit<attrib_num; unit++) {
			switch(attrib[unit]) {
			case RAS_TEXCO_ORCO:
			case RAS_TEXCO_GLOB:
			case RAS_TEXCO_UV1:
			case RAS_TEXCO_NORM:
			case RAS_TEXTANGENT:
			case RAS_TEXCO_UV2:
			case RAS_TEXCO_VCOL:
				if (enable) glEnableVertexAttribArrayARB(unit);
				else glDisableVertexAttribArrayARB(unit);
				break;
			default:
				glDisableVertexAttribArrayARB(unit);
				break;
			}
		}
	}

	if (!enable) {
		m_last_texco_num = 0;
		m_last_attrib_num = 0;
	}
}

