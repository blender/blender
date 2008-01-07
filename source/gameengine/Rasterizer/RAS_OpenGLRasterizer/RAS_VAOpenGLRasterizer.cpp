/**
 * $Id$
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "RAS_VAOpenGLRasterizer.h"

#ifdef WIN32
#include <windows.h>
#endif // WIN32
#ifdef __APPLE__
#define GL_GLEXT_LEGACY 1
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "STR_String.h"
#include "RAS_TexVert.h"
#include "MT_CmMatrix4x4.h"
#include "RAS_IRenderTools.h" // rendering text

#include "RAS_GLExtensionManager.h"
	

using namespace bgl;

RAS_VAOpenGLRasterizer::RAS_VAOpenGLRasterizer(RAS_ICanvas* canvas, bool lock)
:	RAS_OpenGLRasterizer(canvas),
	m_Lock(lock && RAS_EXT_support._EXT_compiled_vertex_array)	
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
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

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
		{
		}
	case KX_WIREFRAME:
		{
			glDisable (GL_CULL_FACE);
			break;
		}
	case KX_TEXTURED:
		{
		}
	case KX_SHADED:
		{
			glEnableClientState(GL_COLOR_ARRAY);
		}
	case KX_SOLID:
		{
			break;
		}
	default:
		{
		}
	}
}



void RAS_VAOpenGLRasterizer::Exit()
{
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	RAS_OpenGLRasterizer::Exit();
}



void RAS_VAOpenGLRasterizer::IndexPrimitives( const vecVertexArray& vertexarrays,
							const vecIndexArrays & indexarrays,
							int mode,
							class RAS_IPolyMaterial* polymat,
							class RAS_IRenderTools* rendertools,
							bool useObjectColor,
							const MT_Vector4& rgbacolor,
							class KX_ListSlot** slot)
{
	static const GLsizei vtxstride = sizeof(RAS_TexVert);
	GLenum drawmode;
	switch (mode)
	{
	case 0:
		{
		drawmode = GL_TRIANGLES;
		break;
		}
	case 2:
		{
		drawmode = GL_QUADS;
		break;
		}
	case 1:	//lines
		{
		}
	default:
		{
		drawmode = GL_LINES;
		break;
		}
	}
	const RAS_TexVert* vertexarray;
	unsigned int numindices, vt;
	if (drawmode != GL_LINES)
	{
		if (useObjectColor)
		{
			glDisableClientState(GL_COLOR_ARRAY);
			glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);
		} else
		{
			glColor4d(0,0,0,1.0);
			glEnableClientState(GL_COLOR_ARRAY);
		}
	}
	else
	{
		glColor3d(0,0,0);
	}

	// use glDrawElements to draw each vertexarray
	for (vt=0;vt<vertexarrays.size();vt++)
	{
		vertexarray = &((*vertexarrays[vt]) [0]);
		const KX_IndexArray & indexarray = (*indexarrays[vt]);
		numindices = indexarray.size();

		if (!numindices)
			continue;
		
		glVertexPointer(3,GL_FLOAT,vtxstride,vertexarray->getLocalXYZ());
		glTexCoordPointer(2,GL_FLOAT,vtxstride,vertexarray->getUV1());
		glColorPointer(4,GL_UNSIGNED_BYTE,vtxstride,vertexarray->getRGBA());
		glNormalPointer(GL_FLOAT,vtxstride,vertexarray->getNormal());

		//if(m_Lock)
		//	local->Begin(vertexarrays[vt]->size());

		// here the actual drawing takes places
		glDrawElements(drawmode,numindices,GL_UNSIGNED_SHORT,&(indexarray[0]));

		//if(m_Lock)
		//	local->End();


	}
}


void RAS_VAOpenGLRasterizer::IndexPrimitivesMulti( const vecVertexArray& vertexarrays,
							const vecIndexArrays & indexarrays,
							int mode,
							class RAS_IPolyMaterial* polymat,
							class RAS_IRenderTools* rendertools,
							bool useObjectColor,
							const MT_Vector4& rgbacolor,
							class KX_ListSlot** slot)
{
	static const GLsizei vtxstride = sizeof(RAS_TexVert);
	GLenum drawmode;
	switch (mode)
	{
	case 0:
		{
		drawmode = GL_TRIANGLES;
		break;
		}
	case 2:
		{
		drawmode = GL_QUADS;
		break;
		}
	case 1:	//lines
		{
		}
	default:
		{
		drawmode = GL_LINES;
		break;
		}
	}
	const RAS_TexVert* vertexarray;
	unsigned int numindices, vt;
	const unsigned int enabled = polymat->GetEnabled();

	if (drawmode != GL_LINES)
	{
		if (useObjectColor)
		{
			glDisableClientState(GL_COLOR_ARRAY);
			glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);
		} else
		{
			glColor4d(0,0,0,1.0);
			glEnableClientState(GL_COLOR_ARRAY);
		}
	}
	else
	{
		glColor3d(0,0,0);
	}

	// use glDrawElements to draw each vertexarray
	for (vt=0;vt<vertexarrays.size();vt++)
	{
		vertexarray = &((*vertexarrays[vt]) [0]);
		const KX_IndexArray & indexarray = (*indexarrays[vt]);
		numindices = indexarray.size();

		if (!numindices)
			continue;
		
		glVertexPointer(3,GL_FLOAT,vtxstride,vertexarray->getLocalXYZ());
		TexCoordPtr(vertexarray, enabled);

		//glTexCoordPointer(2,GL_FLOAT,vtxstride,vertexarray->getUV1());
		glColorPointer(4,GL_UNSIGNED_BYTE,vtxstride,vertexarray->getRGBA());
		glNormalPointer(GL_FLOAT,vtxstride,vertexarray->getNormal());

		//if(m_Lock)
		//	local->Begin(vertexarrays[vt]->size());

		// here the actual drawing takes places
		glDrawElements(drawmode,numindices,GL_UNSIGNED_SHORT,&(indexarray[0]));
		
		//if(m_Lock)
		//	local->End();
	}
}

void RAS_VAOpenGLRasterizer::TexCoordPtr(const RAS_TexVert *tv, int enabled)
{
#ifdef GL_ARB_multitexture
	if(bgl::RAS_EXT_support._ARB_multitexture)
	{
		for(int unit=0; unit<enabled; unit++)
		{
			bgl::blClientActiveTextureARB(GL_TEXTURE0_ARB+unit);

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			if( tv->getFlag() & TV_2NDUV && tv->getUnit() == unit ) {
				glTexCoordPointer(2, GL_FLOAT, sizeof(RAS_TexVert), tv->getUV2());
				continue;
			}
			switch(m_texco[unit])
			{
			case RAS_TEXCO_DISABLE:
			case RAS_TEXCO_OBJECT:
			case RAS_TEXCO_GEN:
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				break;
			case RAS_TEXCO_ORCO:
			case RAS_TEXCO_GLOB:
				glTexCoordPointer(3, GL_FLOAT, sizeof(RAS_TexVert),tv->getLocalXYZ());
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
			}
		}
	}

#ifdef GL_ARB_vertex_program
	if(m_useTang && bgl::RAS_EXT_support._ARB_vertex_program)
		bgl::blVertexAttrib4fvARB(1/*tangent*/, tv->getTangent());
#endif

#endif
}


void RAS_VAOpenGLRasterizer::EnableTextures(bool enable)
{
	if (enable)
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	else
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

