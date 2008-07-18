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
 
#include <math.h>
#include <stdlib.h>
 
#include "RAS_OpenGLRasterizer.h"

#include "GL/glew.h"

#include "RAS_Rect.h"
#include "RAS_TexVert.h"
#include "MT_CmMatrix4x4.h"
#include "RAS_IRenderTools.h" // rendering text

/**
 *  32x32 bit masks for vinterlace stereo mode
 */
static GLuint left_eye_vinterlace_mask[32];
static GLuint right_eye_vinterlace_mask[32];

/**
 *  32x32 bit masks for hinterlace stereo mode.
 *  Left eye = &hinterlace_mask[0]
 *  Right eye = &hinterlace_mask[1]
 */
static GLuint hinterlace_mask[33];

RAS_OpenGLRasterizer::RAS_OpenGLRasterizer(RAS_ICanvas* canvas)
	:RAS_IRasterizer(canvas),
	m_2DCanvas(canvas),
	m_fogenabled(false),
	m_time(0.0),
	m_stereomode(RAS_STEREO_NOSTEREO),
	m_curreye(RAS_STEREO_LEFTEYE),
	m_eyeseparation(0.0),
	m_seteyesep(false),
	m_focallength(0.0),
	m_setfocallength(false),
	m_noOfScanlines(32),
	m_motionblur(0),
	m_motionblurvalue(-1.0),
	m_texco_num(0),
	m_attrib_num(0),
	m_materialCachingInfo(0)
{
	m_viewmatrix.Identity();
	
	for (int i = 0; i < 32; i++)
	{
		left_eye_vinterlace_mask[i] = 0x55555555;
		right_eye_vinterlace_mask[i] = 0xAAAAAAAA;
		hinterlace_mask[i] = (i&1)*0xFFFFFFFF;
	}
	hinterlace_mask[32] = 0;
}



RAS_OpenGLRasterizer::~RAS_OpenGLRasterizer()
{
}



static void Myinit_gl_stuff(void)	
{
	float mat_specular[] = { 0.5, 0.5, 0.5, 1.0 };
	float mat_shininess[] = { 35.0 };
/*  	float one= 1.0; */
	int a, x, y;
	GLubyte pat[32*32];
	const GLubyte *patc= pat;
		
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);


#if defined(__FreeBSD) || defined(__linux__)
	glDisable(GL_DITHER);	/* op sgi/sun hardware && 12 bits */
#endif
	
	/* no local viewer, looks ugly in ortho mode */
	/* glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, &one); */
	
	glDepthFunc(GL_LEQUAL);
	/* scaling matrices */
	glEnable(GL_NORMALIZE);

	glShadeModel(GL_FLAT);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_LIGHTING);
	glDisable(GL_LOGIC_OP);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	glPixelTransferi(GL_MAP_COLOR, GL_FALSE);
	glPixelTransferi(GL_RED_SCALE, 1);
	glPixelTransferi(GL_RED_BIAS, 0);
	glPixelTransferi(GL_GREEN_SCALE, 1);
	glPixelTransferi(GL_GREEN_BIAS, 0);
	glPixelTransferi(GL_BLUE_SCALE, 1);
	glPixelTransferi(GL_BLUE_BIAS, 0);
	glPixelTransferi(GL_ALPHA_SCALE, 1);
	glPixelTransferi(GL_ALPHA_BIAS, 0);

	a = 0;
	for(x=0; x<32; x++)
	{
		for(y=0; y<4; y++)
		{
			if( (x) & 1) pat[a++]= 0x88;
			else pat[a++]= 0x22;
		}
	}
	
	glPolygonStipple(patc);
	
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
}



bool RAS_OpenGLRasterizer::Init()
{

	Myinit_gl_stuff();

	m_redback = 0.4375;
	m_greenback = 0.4375;
	m_blueback = 0.4375;
	m_alphaback = 0.0;
	
	m_ambr = 0.0f;
	m_ambg = 0.0f;
	m_ambb = 0.0f;

	glClearColor(m_redback,m_greenback,m_blueback,m_alphaback);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glShadeModel(GL_SMOOTH);

	return true;
}


void RAS_OpenGLRasterizer::SetAmbientColor(float red, float green, float blue)
{
	m_ambr = red;
	m_ambg = green;
	m_ambb = blue;
}


void RAS_OpenGLRasterizer::SetAlphaTest(bool enable)
{
	if (enable)
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.6f);
	}
	else glDisable(GL_ALPHA_TEST);
}



void RAS_OpenGLRasterizer::SetAmbient(float factor)
{
	float ambient[] = { m_ambr*factor, m_ambg*factor, m_ambb*factor, 1.0f };
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
}


void RAS_OpenGLRasterizer::SetBackColor(float red,
										float green,
										float blue,
										float alpha)
{
	m_redback = red;
	m_greenback = green;
	m_blueback = blue;
	m_alphaback = alpha;
}



void RAS_OpenGLRasterizer::SetFogColor(float r,
									   float g,
									   float b)
{
	m_fogr = r;
	m_fogg = g;
	m_fogb = b;
	m_fogenabled = true;
}



void RAS_OpenGLRasterizer::SetFogStart(float start)
{
	m_fogstart = start;
	m_fogenabled = true;
}



void RAS_OpenGLRasterizer::SetFogEnd(float fogend)
{
	m_fogdist = fogend;
	m_fogenabled = true;
}



void RAS_OpenGLRasterizer::SetFog(float start,
								  float dist,
								  float r,
								  float g,
								  float b)
{
	m_fogstart = start;
	m_fogdist = dist;
	m_fogr = r;
	m_fogg = g;
	m_fogb = b;
	m_fogenabled = true;
}



void RAS_OpenGLRasterizer::DisableFog()
{
	m_fogenabled = false;
}



void RAS_OpenGLRasterizer::DisplayFog()
{
	if ((m_drawingmode >= KX_SOLID) && m_fogenabled)
	{
		float params[5];
		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogf(GL_FOG_DENSITY, 0.1f);
		glFogf(GL_FOG_START, m_fogstart);
		glFogf(GL_FOG_END, m_fogstart + m_fogdist);
		params[0]= m_fogr;
		params[1]= m_fogg;
		params[2]= m_fogb;
		params[3]= 0.0;
		glFogfv(GL_FOG_COLOR, params); 
		glEnable(GL_FOG);
	} 
	else
	{
		glDisable(GL_FOG);
	}
}



bool RAS_OpenGLRasterizer::SetMaterial(const RAS_IPolyMaterial& mat)
{
	return mat.Activate(this, m_materialCachingInfo);
}



void RAS_OpenGLRasterizer::Exit()
{

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0); 
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(m_redback, m_greenback, m_blueback, m_alphaback);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthMask (GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glBlendFunc(GL_ONE, GL_ZERO);
	
	glDisable(GL_POLYGON_STIPPLE);
	
	glDisable(GL_LIGHTING);
	if (GLEW_EXT_separate_specular_color || GLEW_VERSION_1_2)
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SINGLE_COLOR);
	
	EndFrame();
}

bool RAS_OpenGLRasterizer::InterlacedStereo() const
{
	return m_stereomode == RAS_STEREO_VINTERLACE || m_stereomode == RAS_STEREO_INTERLACED;
}

bool RAS_OpenGLRasterizer::BeginFrame(int drawingmode, double time)
{
	m_time = time;
	m_drawingmode = drawingmode;
	
	if (!InterlacedStereo() || m_curreye == RAS_STEREO_LEFTEYE)
	{
		m_2DCanvas->ClearColor(m_redback,m_greenback,m_blueback,m_alphaback);
		m_2DCanvas->ClearBuffer(RAS_ICanvas::COLOR_BUFFER);
	}

	// Blender camera routine destroys the settings
	if (m_drawingmode < KX_SOLID)
	{
		glDisable (GL_CULL_FACE);
		glDisable (GL_DEPTH_TEST);
	}
	else
	{
		glEnable(GL_DEPTH_TEST);
		glEnable (GL_CULL_FACE);
	}

	glShadeModel(GL_SMOOTH);

	m_2DCanvas->BeginFrame();
	
	return true;
}



void RAS_OpenGLRasterizer::SetDrawingMode(int drawingmode)
{
	m_drawingmode = drawingmode;

	switch (m_drawingmode)
	{
	case KX_WIREFRAME:
		{
			glDisable (GL_CULL_FACE);
			break;
		}
	default:
		{
		}
	}
}



int RAS_OpenGLRasterizer::GetDrawingMode()
{
	return m_drawingmode;
}



void RAS_OpenGLRasterizer::SetDepthMask(DepthMask depthmask)
{
	glDepthMask(depthmask == KX_DEPTHMASK_DISABLED ? GL_FALSE : GL_TRUE);
}



void RAS_OpenGLRasterizer::ClearDepthBuffer()
{
	m_2DCanvas->ClearBuffer(RAS_ICanvas::DEPTH_BUFFER);
}


void RAS_OpenGLRasterizer::ClearCachingInfo(void)
{
	m_materialCachingInfo = 0;
}


void RAS_OpenGLRasterizer::EndFrame()
{
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);

	//DrawDebugLines
	glBegin(GL_LINES);
	for (unsigned int i=0;i<m_debugLines.size();i++)
	{
		

		glColor4f(m_debugLines[i].m_color[0],m_debugLines[i].m_color[1],m_debugLines[i].m_color[2],1.f);
		const MT_Scalar* fromPtr = &m_debugLines[i].m_from.x();
		const MT_Scalar* toPtr= &m_debugLines[i].m_to.x();

		glVertex3dv(fromPtr);
		glVertex3dv(toPtr);
	}
	glEnd();

	m_debugLines.clear();

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	m_2DCanvas->EndFrame();
}	

void RAS_OpenGLRasterizer::SetRenderArea()
{
	// only above/below stereo method needs viewport adjustment
	switch (m_stereomode)
	{
		case RAS_STEREO_ABOVEBELOW:
			switch(m_curreye)
			{
				case RAS_STEREO_LEFTEYE:
					// upper half of window
					m_2DCanvas->GetDisplayArea().SetLeft(0);
					m_2DCanvas->GetDisplayArea().SetBottom(m_2DCanvas->GetHeight() -
						int(m_2DCanvas->GetHeight() - m_noOfScanlines) / 2);
	
					m_2DCanvas->GetDisplayArea().SetRight(int(m_2DCanvas->GetWidth()));
					m_2DCanvas->GetDisplayArea().SetTop(int(m_2DCanvas->GetHeight()));
					break;
				case RAS_STEREO_RIGHTEYE:
					// lower half of window
					m_2DCanvas->GetDisplayArea().SetLeft(0);
					m_2DCanvas->GetDisplayArea().SetBottom(0);
					m_2DCanvas->GetDisplayArea().SetRight(int(m_2DCanvas->GetWidth()));
					m_2DCanvas->GetDisplayArea().SetTop(int(m_2DCanvas->GetHeight() - m_noOfScanlines) / 2);
					break;
			}
			break;
		case RAS_STEREO_SIDEBYSIDE:
			switch (m_curreye)
			{
				case RAS_STEREO_LEFTEYE:
					// Left half of window
					m_2DCanvas->GetDisplayArea().SetLeft(0);
					m_2DCanvas->GetDisplayArea().SetBottom(0);
					m_2DCanvas->GetDisplayArea().SetRight(m_2DCanvas->GetWidth()/2);
					m_2DCanvas->GetDisplayArea().SetTop(m_2DCanvas->GetHeight());
					break;
				case RAS_STEREO_RIGHTEYE:
					// Right half of window
					m_2DCanvas->GetDisplayArea().SetLeft(m_2DCanvas->GetWidth()/2);
					m_2DCanvas->GetDisplayArea().SetBottom(0);
					m_2DCanvas->GetDisplayArea().SetRight(m_2DCanvas->GetWidth());
					m_2DCanvas->GetDisplayArea().SetTop(m_2DCanvas->GetHeight());
					break;
			}
			break;
		default:
			// every available pixel
			m_2DCanvas->GetDisplayArea().SetLeft(0);
			m_2DCanvas->GetDisplayArea().SetBottom(0);
			m_2DCanvas->GetDisplayArea().SetRight(int(m_2DCanvas->GetWidth()));
			m_2DCanvas->GetDisplayArea().SetTop(int(m_2DCanvas->GetHeight()));
			break;
	}
}

	
void RAS_OpenGLRasterizer::SetStereoMode(const StereoMode stereomode)
{
	m_stereomode = stereomode;
}



bool RAS_OpenGLRasterizer::Stereo()
{
	if(m_stereomode == RAS_STEREO_NOSTEREO)
		return false;
	else
		return true;
}


void RAS_OpenGLRasterizer::SetEye(const StereoEye eye)
{
	m_curreye = eye;
	switch (m_stereomode)
	{
		case RAS_STEREO_QUADBUFFERED:
			glDrawBuffer(m_curreye == RAS_STEREO_LEFTEYE ? GL_BACK_LEFT : GL_BACK_RIGHT);
			break;
		case RAS_STEREO_ANAGLYPH:
			if (m_curreye == RAS_STEREO_LEFTEYE)
			{
				glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
			} else {
				//glAccum(GL_LOAD, 1.0);
				glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
				ClearDepthBuffer();
			}
			break;
		case RAS_STEREO_VINTERLACE:
		{
			glEnable(GL_POLYGON_STIPPLE);
			glPolygonStipple((const GLubyte*) ((m_curreye == RAS_STEREO_LEFTEYE) ? left_eye_vinterlace_mask : right_eye_vinterlace_mask));
			if (m_curreye == RAS_STEREO_RIGHTEYE)
				ClearDepthBuffer();
			break;
		}
		case RAS_STEREO_INTERLACED:
		{
			glEnable(GL_POLYGON_STIPPLE);
			glPolygonStipple((const GLubyte*) &hinterlace_mask[m_curreye == RAS_STEREO_LEFTEYE?0:1]);
			if (m_curreye == RAS_STEREO_RIGHTEYE)
				ClearDepthBuffer();
			break;
		}
		default:
			break;
	}
}

RAS_IRasterizer::StereoEye RAS_OpenGLRasterizer::GetEye()
{
	return m_curreye;
}


void RAS_OpenGLRasterizer::SetEyeSeparation(const float eyeseparation)
{
	m_eyeseparation = eyeseparation;
	m_seteyesep = true;
}

float RAS_OpenGLRasterizer::GetEyeSeparation()
{
	return m_eyeseparation;
}

void RAS_OpenGLRasterizer::SetFocalLength(const float focallength)
{
	m_focallength = focallength;
	m_setfocallength = true;
}

float RAS_OpenGLRasterizer::GetFocalLength()
{
	return m_focallength;
}


void RAS_OpenGLRasterizer::SwapBuffers()
{
	m_2DCanvas->SwapBuffers();
}



void RAS_OpenGLRasterizer::GetViewMatrix(MT_Matrix4x4 &mat) const
{
	float viewmat[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, viewmat);
	mat.setValue(viewmat);
}



void RAS_OpenGLRasterizer::IndexPrimitives(const vecVertexArray & vertexarrays,
									const vecIndexArrays & indexarrays,
									DrawMode mode,
									bool useObjectColor,
									const MT_Vector4& rgbacolor,
									class KX_ListSlot** slot
									)
{ 
	const RAS_TexVert* vertexarray;
	unsigned int numindices, vt;

	for (vt=0;vt<vertexarrays.size();vt++)
	{
		vertexarray = &((*vertexarrays[vt]) [0]);
		const KX_IndexArray & indexarray = (*indexarrays[vt]);
		numindices = indexarray.size();
		
		if (!numindices)
			break;
		
		int vindex=0;
		switch (mode)
		{
		case KX_MODE_LINES:
			{
				glBegin(GL_LINES);
				vindex=0;
				for (unsigned int i=0;i<numindices;i+=2)
				{
					glVertex3fv(vertexarray[(indexarray[vindex++])].getLocalXYZ());
					glVertex3fv(vertexarray[(indexarray[vindex++])].getLocalXYZ());
				}
				glEnd();
			}
			break;
		case KX_MODE_QUADS:
			{
				glBegin(GL_QUADS);
				vindex=0;
				if (useObjectColor)
				{
					for (unsigned int i=0;i<numindices;i+=4)
					{

						glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);

						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				else
				{
					for (unsigned int i=0;i<numindices;i+=4)
					{
						// This looks curiously endian unsafe to me.
						// However it depends on the way the colors are packed into 
						// the m_rgba field of RAS_TexVert

						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				glEnd();	
				break;
			}
		case KX_MODE_TRIANGLES:
			{
				glBegin(GL_TRIANGLES);
				vindex=0;
				if (useObjectColor)
				{
					for (unsigned int i=0;i<numindices;i+=3)
					{

						glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);

						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				else 
				{
					for (unsigned int i=0;i<numindices;i+=3)
					{

						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				glEnd();	
				break;
			}
		default:
			{
			}
			
		} // switch
	} // for each vertexarray

}

void RAS_OpenGLRasterizer::IndexPrimitives_3DText(const vecVertexArray & vertexarrays,
									const vecIndexArrays & indexarrays,
									DrawMode mode,
									class RAS_IPolyMaterial* polymat,
									class RAS_IRenderTools* rendertools,
									bool useObjectColor,
									const MT_Vector4& rgbacolor
									)
{ 
	const RAS_TexVert* vertexarray;
	unsigned int numindices, vt;
	
	if (useObjectColor)
	{
		glDisableClientState(GL_COLOR_ARRAY);
		glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);
	}
	else
	{
		glEnableClientState(GL_COLOR_ARRAY);
	}
	
	for (vt=0;vt<vertexarrays.size();vt++)
	{
		vertexarray = &((*vertexarrays[vt]) [0]);
		const KX_IndexArray & indexarray = (*indexarrays[vt]);
		numindices = indexarray.size();
		
		if (!numindices)
			break;
		
		int vindex=0;
		switch (mode)
		{
		case KX_MODE_LINES:
			{
				glBegin(GL_LINES);
				vindex=0;
				for (unsigned int i=0;i<numindices;i+=2)
				{
					glVertex3fv(vertexarray[(indexarray[vindex++])].getLocalXYZ());
					glVertex3fv(vertexarray[(indexarray[vindex++])].getLocalXYZ());
				}
				glEnd();
			}
			break;
		case KX_MODE_QUADS:
			{
				vindex=0;
				for (unsigned int i=0;i<numindices;i+=4)
				{
					float v1[3],v2[3],v3[3],v4[3];
					
					v1[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v1[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v1[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					v2[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v2[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v2[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					v3[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v3[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v3[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					v4[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v4[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v4[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					
					vindex++;
					
					rendertools->RenderText(polymat->GetDrawingMode(),polymat,v1,v2,v3,v4);
					ClearCachingInfo();
				}
				break;
			}
		case KX_MODE_TRIANGLES:
			{
				glBegin(GL_TRIANGLES);
				vindex=0;
				for (unsigned int i=0;i<numindices;i+=3)
				{
					float v1[3],v2[3],v3[3];
					
					v1[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v1[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v1[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					v2[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v2[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v2[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					v3[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v3[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v3[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					rendertools->RenderText(polymat->GetDrawingMode(),polymat,v1,v2,v3,NULL);		
					ClearCachingInfo();
				}
				glEnd();	
				break;
			}
		default:
			{
			}
		}	//switch
	}	//for each vertexarray
}

void RAS_OpenGLRasterizer::SetTexCoordNum(int num)
{
	m_texco_num = num;
	if(m_texco_num > RAS_MAX_TEXCO)
		m_texco_num = RAS_MAX_TEXCO;
}

void RAS_OpenGLRasterizer::SetAttribNum(int num)
{
	m_attrib_num = num;
	if(m_attrib_num > RAS_MAX_ATTRIB)
		m_attrib_num = RAS_MAX_ATTRIB;
}

void RAS_OpenGLRasterizer::SetTexCoord(TexCoGen coords, int unit)
{
	// this changes from material to material
	if(unit < RAS_MAX_TEXCO)
		m_texco[unit] = coords;
}

void RAS_OpenGLRasterizer::SetAttrib(TexCoGen coords, int unit)
{
	// this changes from material to material
	if(unit < RAS_MAX_ATTRIB)
		m_attrib[unit] = coords;
}

void RAS_OpenGLRasterizer::TexCoord(const RAS_TexVert &tv)
{
	int unit;

	if(GLEW_ARB_multitexture) {
		for(unit=0; unit<m_texco_num; unit++) {
			if(tv.getFlag() & TV_2NDUV && (int)tv.getUnit() == unit) {
				glMultiTexCoord2fvARB(GL_TEXTURE0_ARB+unit, tv.getUV2());
				continue;
			}
			switch(m_texco[unit]) {
			case RAS_TEXCO_ORCO:
			case RAS_TEXCO_GLOB:
				glMultiTexCoord3fvARB(GL_TEXTURE0_ARB+unit, tv.getLocalXYZ());
				break;
			case RAS_TEXCO_UV1:
				glMultiTexCoord2fvARB(GL_TEXTURE0_ARB+unit, tv.getUV1());
				break;
			case RAS_TEXCO_NORM:
				glMultiTexCoord3fvARB(GL_TEXTURE0_ARB+unit, tv.getNormal());
				break;
			case RAS_TEXTANGENT:
				glMultiTexCoord4fvARB(GL_TEXTURE0_ARB+unit, tv.getTangent());
				break;
			case RAS_TEXCO_UV2:
				glMultiTexCoord2fvARB(GL_TEXTURE0_ARB+unit, tv.getUV2());
				break;
			default:
				break;
			}
		}
	}

	if(GLEW_ARB_vertex_program) {
		for(unit=0; unit<m_attrib_num; unit++) {
			switch(m_attrib[unit]) {
			case RAS_TEXCO_ORCO:
			case RAS_TEXCO_GLOB:
				glVertexAttrib3fvARB(unit, tv.getLocalXYZ());
				break;
			case RAS_TEXCO_UV1:
				glVertexAttrib2fvARB(unit, tv.getUV1());
				break;
			case RAS_TEXCO_NORM:
				glVertexAttrib3fvARB(unit, tv.getNormal());
				break;
			case RAS_TEXTANGENT:
				glVertexAttrib4fvARB(unit, tv.getTangent());
				break;
			case RAS_TEXCO_UV2:
				glVertexAttrib2fvARB(unit, tv.getUV2());
				break;
			case RAS_TEXCO_VCOL:
				glVertexAttrib4ubvARB(unit, tv.getRGBA());
				break;
			default:
				break;
			}
		}
	}

}
void RAS_OpenGLRasterizer::Tangent(	const RAS_TexVert& v1,
									const RAS_TexVert& v2,
									const RAS_TexVert& v3,
									const MT_Vector3 &no)
{
	// TODO: set for deformer... 
	MT_Vector3 x1(v1.getLocalXYZ()), x2(v2.getLocalXYZ()), x3(v3.getLocalXYZ());
	MT_Vector2 uv1(v1.getUV1()), uv2(v2.getUV1()), uv3(v3.getUV1());
	MT_Vector3 dx1(x2 - x1), dx2(x3 - x1);
	MT_Vector2 duv1(uv2 - uv1), duv2(uv3 - uv1);

	MT_Scalar r = 1.0 / (duv1.x() * duv2.y() - duv2.x() * duv1.y());
	duv1 *= r;
	duv2 *= r;
	MT_Vector3 sdir(duv2.y() * dx1 - duv1.y() * dx2);
	MT_Vector3 tdir(duv1.x() * dx2 - duv2.x() * dx1);

	// Gram-Schmidt orthogonalize
	MT_Vector3 t(sdir - no.cross(no.cross(sdir)));
	if (!MT_fuzzyZero(t)) t /= t.length();

	float tangent[4];
	t.getValue(tangent);
	// Calculate handedness
	tangent[3] = no.dot(sdir.cross(tdir)) < 0.0 ? -1.0 : 1.0;
}


void RAS_OpenGLRasterizer::IndexPrimitivesMulti(
		const vecVertexArray& vertexarrays,
		const vecIndexArrays & indexarrays,
		DrawMode mode,
		bool useObjectColor,
		const MT_Vector4& rgbacolor,
		class KX_ListSlot** slot
		)
{ 

	const RAS_TexVert* vertexarray;
	unsigned int numindices,vt;

	for (vt=0;vt<vertexarrays.size();vt++)
	{
		vertexarray = &((*vertexarrays[vt]) [0]);
		const KX_IndexArray & indexarray = (*indexarrays[vt]);
		numindices = indexarray.size();

		if (!numindices)
			break;

		int vindex=0;
		switch (mode)
		{
		case KX_MODE_LINES:
			{
				glBegin(GL_LINES);
				vindex=0;
				for (unsigned int i=0;i<numindices;i+=2)
				{
					glVertex3fv(vertexarray[(indexarray[vindex++])].getLocalXYZ());
					glVertex3fv(vertexarray[(indexarray[vindex++])].getLocalXYZ());
				}
				glEnd();
			}
			break;
		case KX_MODE_QUADS:
			{
				glBegin(GL_QUADS);
				vindex=0;
				if (useObjectColor)
				{
					for (unsigned int i=0;i<numindices;i+=4)
					{

						glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);

						//
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;

						//
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;

						//
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;

						//
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				else
				{
					for (unsigned int i=0;i<numindices;i+=4)
					{
						// This looks curiously endian unsafe to me.
						// However it depends on the way the colors are packed into 
						// the m_rgba field of RAS_TexVert
				
						//
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
				
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					
						//
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;

						//
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;

						//
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				glEnd();	
				break;
			}
		case KX_MODE_TRIANGLES:
			{
				glBegin(GL_TRIANGLES);
				vindex=0;
				if (useObjectColor)
				{
					for (unsigned int i=0;i<numindices;i+=3)
					{

						glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);
						//
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;

						//
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;

						//
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				else 
				{
					for (unsigned int i=0;i<numindices;i+=3)
					{
						//
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
				
						//
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;

						//
						glColor4ubv((const GLubyte *)(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3fv(vertexarray[(indexarray[vindex])].getNormal());
						TexCoord(vertexarray[(indexarray[vindex])]);
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				glEnd();	
				break;
			}
		default:
			{
			}
		} // switch
	} // for each vertexarray
}

void RAS_OpenGLRasterizer::SetProjectionMatrix(MT_CmMatrix4x4 &mat)
{
	glMatrixMode(GL_PROJECTION);
	double* matrix = &mat(0,0);
	glLoadMatrixd(matrix);
}


void RAS_OpenGLRasterizer::SetProjectionMatrix(const MT_Matrix4x4 & mat)
{
	glMatrixMode(GL_PROJECTION);
	double matrix[16];
	/* Get into argument. Looks a bit dodgy, but it's ok. */
	mat.getValue(matrix);
	/* Internally, MT_Matrix4x4 uses doubles (MT_Scalar). */
	glLoadMatrixd(matrix);	
}

MT_Matrix4x4 RAS_OpenGLRasterizer::GetFrustumMatrix(
	float left,
	float right,
	float bottom,
	float top,
	float frustnear,
	float frustfar,
	float focallength,
	bool
){
	MT_Matrix4x4 result;
	double mat[16];

	// correction for stereo
	if(m_stereomode != RAS_STEREO_NOSTEREO)
	{
			float near_div_focallength;
			// next 2 params should be specified on command line and in Blender publisher
			if (!m_setfocallength)
				m_focallength = (focallength == 0.f) ? 1.5 * right  // derived from example
					: focallength; 
			if (!m_seteyesep)
				m_eyeseparation = m_focallength/30;  // reasonable value...

			near_div_focallength = frustnear / m_focallength;
			switch(m_curreye)
			{
				case RAS_STEREO_LEFTEYE:
						left += 0.5 * m_eyeseparation * near_div_focallength;
						right += 0.5 * m_eyeseparation * near_div_focallength;
						break;
				case RAS_STEREO_RIGHTEYE:
						left -= 0.5 * m_eyeseparation * near_div_focallength;
						right -= 0.5 * m_eyeseparation * near_div_focallength;
						break;
			}
			// leave bottom, top, bottom and top untouched
	}
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(left, right, bottom, top, frustnear, frustfar);
		
	glGetDoublev(GL_PROJECTION_MATRIX, mat);
	result.setValue(mat);

	return result;
}


// next arguments probably contain redundant info, for later...
void RAS_OpenGLRasterizer::SetViewMatrix(const MT_Matrix4x4 &mat, const MT_Vector3& campos,
		const MT_Point3 &, const MT_Quaternion &camOrientQuat)
{
	MT_Matrix4x4 viewMat = mat;

	// correction for stereo
	if(m_stereomode != RAS_STEREO_NOSTEREO)
	{
		MT_Matrix3x3 camOrientMat3x3(camOrientQuat);
		MT_Vector3 unitViewDir(0.0, -1.0, 0.0);  // minus y direction, Blender convention
		MT_Vector3 unitViewupVec(0.0, 0.0, 1.0);
		MT_Vector3 viewDir, viewupVec;
		MT_Vector3 eyeline;

		// actual viewDir
		viewDir = camOrientMat3x3 * unitViewDir;  // this is the moto convention, vector on right hand side
		// actual viewup vec
		viewupVec = camOrientMat3x3 * unitViewupVec;

		// vector between eyes
		eyeline = viewDir.cross(viewupVec);

		switch(m_curreye)
		{
			case RAS_STEREO_LEFTEYE:
				{
				// translate to left by half the eye distance
				MT_Transform transform;
				transform.setIdentity();
				transform.translate(-(eyeline * m_eyeseparation / 2.0));
				viewMat *= transform;
				}
				break;
			case RAS_STEREO_RIGHTEYE:
				{
				// translate to right by half the eye distance
				MT_Transform transform;
				transform.setIdentity();
				transform.translate(eyeline * m_eyeseparation / 2.0);
				viewMat *= transform;
				}
				break;
		}
	}

	// convert row major matrix 'viewMat' to column major for OpenGL
	MT_Scalar cammat[16];
	viewMat.getValue(cammat);
	MT_CmMatrix4x4 viewCmmat = cammat;

	glMatrixMode(GL_MODELVIEW);
	m_viewmatrix = viewCmmat;
	glLoadMatrixd(&m_viewmatrix(0,0));
	m_campos = campos;
}


const MT_Point3& RAS_OpenGLRasterizer::GetCameraPosition()
{
	return m_campos;
}



void RAS_OpenGLRasterizer::LoadViewMatrix()
{
	glLoadMatrixd(&m_viewmatrix(0,0));
}



void RAS_OpenGLRasterizer::EnableTextures(bool enable)
{
}



void RAS_OpenGLRasterizer::SetCullFace(bool enable)
{
	if (enable)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
}

void RAS_OpenGLRasterizer::SetLines(bool enable)
{
	if (enable)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void RAS_OpenGLRasterizer::SetSpecularity(float specX,
										  float specY,
										  float specZ,
										  float specval)
{
	GLfloat mat_specular[] = {specX, specY, specZ, specval};
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
}



void RAS_OpenGLRasterizer::SetShinyness(float shiny)
{
	GLfloat mat_shininess[] = {	shiny };
	glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
}



void RAS_OpenGLRasterizer::SetDiffuse(float difX,float difY,float difZ,float diffuse)
{
	GLfloat mat_diffuse [] = {difX, difY,difZ, diffuse};
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
}

void RAS_OpenGLRasterizer::SetEmissive(float eX, float eY, float eZ, float e)
{
	GLfloat mat_emit [] = {eX,eY,eZ,e};
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, mat_emit);
}


double RAS_OpenGLRasterizer::GetTime()
{
	return m_time;
}

void RAS_OpenGLRasterizer::SetPolygonOffset(float mult, float add)
{
	glPolygonOffset(mult, add);
	GLint mode = GL_POLYGON_OFFSET_FILL;
	if (m_drawingmode < KX_SHADED)
		mode = GL_POLYGON_OFFSET_LINE;
	if (mult != 0.0f || add != 0.0f)
		glEnable(mode);
	else
		glDisable(mode);
}

void RAS_OpenGLRasterizer::EnableMotionBlur(float motionblurvalue)
{
	m_motionblur = 1;
	m_motionblurvalue = motionblurvalue;
}

void RAS_OpenGLRasterizer::DisableMotionBlur()
{
	m_motionblur = 0;
	m_motionblurvalue = -1.0;
}
