/**
 * $Id$
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
#include "RAS_OpenGLRasterizer.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif // WIN32
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "RAS_Rect.h"
#include "RAS_TexVert.h"
#include "MT_CmMatrix4x4.h"
#include "RAS_IRenderTools.h" // rendering text


RAS_OpenGLRasterizer::RAS_OpenGLRasterizer(RAS_ICanvas* canvas)
	:RAS_IRasterizer(canvas),
	m_2DCanvas(canvas),
	m_fogenabled(false),
	m_noOfScanlines(32),
	m_materialCachingInfo(0)
{
	m_viewmatrix.Identity();
	m_stereomode = RAS_STEREO_NOSTEREO;
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
}



bool RAS_OpenGLRasterizer::Init()
{

	Myinit_gl_stuff();

	m_redback = 0.4375;
	m_greenback = 0.4375;
	m_blueback = 0.4375;
	m_alphaback = 0.0;
	
	// enable both vertexcolor AND lighting color
	glEnable(GL_COLOR_MATERIAL);
	
	glClearColor(m_redback,m_greenback,m_blueback,m_alphaback);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glShadeModel(GL_SMOOTH);

	return true;
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



void RAS_OpenGLRasterizer::SetMaterial(const RAS_IPolyMaterial& mat)
{
	if (mat.GetCachingInfo() != m_materialCachingInfo)
	{
		mat.Activate(this, m_materialCachingInfo);
	}
}



void RAS_OpenGLRasterizer::Exit()
{

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0); 
	glClearColor(m_redback, m_greenback, m_blueback, m_alphaback);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthMask (GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glBlendFunc(GL_ONE, GL_ZERO);

	glDisable(GL_LIGHTING);
	
	EndFrame();
}



bool RAS_OpenGLRasterizer::BeginFrame(int drawingmode, double time)
{
	m_time = time;
	m_drawingmode = drawingmode;
	
	m_2DCanvas->ClearColor(m_redback,m_greenback,m_blueback,m_alphaback);
	m_2DCanvas->ClearBuffer(RAS_ICanvas::COLOR_BUFFER);

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
		}
	case KX_SOLID:
		{
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



void RAS_OpenGLRasterizer::SetDepthMask(int depthmask)
{
	switch (depthmask)
	{
	case KX_DEPTHMASK_ENABLED:
		{
			glDepthMask(GL_TRUE);
			//glDisable ( GL_ALPHA_TEST );
			break;
		};
	case KX_DEPTHMASK_DISABLED:
		{
			glDepthMask(GL_FALSE);
			//glAlphaFunc ( GL_GREATER, 0.0 ) ;
			//glEnable ( GL_ALPHA_TEST ) ;
			break;
		};
	default:
		{
		//printf("someone made a mistake, RAS_OpenGLRasterizer::SetDepthMask(int depthmask)\n");
		exit(0);
		}
	}
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
	m_2DCanvas->EndFrame();
}	


void RAS_OpenGLRasterizer::SetRenderArea()
{
	// only above/below stereo method needs viewport adjustment
	if(m_stereomode == RAS_STEREO_ABOVEBELOW)
	{
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
	}
	else
	{
		// every available pixel
		m_2DCanvas->GetDisplayArea().SetLeft(0);
		m_2DCanvas->GetDisplayArea().SetBottom(0);
		m_2DCanvas->GetDisplayArea().SetRight(int(m_2DCanvas->GetWidth()));
		m_2DCanvas->GetDisplayArea().SetTop(int(m_2DCanvas->GetHeight()));
	}
}

	
void RAS_OpenGLRasterizer::SetStereoMode(const int stereomode)
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


void RAS_OpenGLRasterizer::SetEye(int eye)
{
	m_curreye = eye;
	if(m_stereomode == RAS_STEREO_QUADBUFFERED) {
		if(m_curreye == RAS_STEREO_LEFTEYE)
			glDrawBuffer(GL_BACK_LEFT);
		else
			glDrawBuffer(GL_BACK_RIGHT);
	}
}


void RAS_OpenGLRasterizer::SetEyeSeparation(float eyeseparation)
{
	m_eyeseparation = eyeseparation;
}


void RAS_OpenGLRasterizer::SetFocalLength(float focallength)
{
	m_focallength = focallength;
}


void RAS_OpenGLRasterizer::SwapBuffers()
{
	m_2DCanvas->SwapBuffers();
}



void RAS_OpenGLRasterizer::IndexPrimitives(const vecVertexArray & vertexarrays,
									const vecIndexArrays & indexarrays,
									int mode,
									class RAS_IPolyMaterial* polymat,
									class RAS_IRenderTools* rendertools,
									bool useObjectColor,
									const MT_Vector4& rgbacolor
									)
{ 
	GLenum drawmode;
	switch (mode)
	{
	case 0:
		drawmode = GL_TRIANGLES;
		break;
	case 1:
		drawmode = GL_LINES;
		break;
	case 2:
		drawmode = GL_QUADS;
		break;
	default:
		drawmode = GL_LINES;
		break;
	}
	
	const RAS_TexVert* vertexarray ;
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
		case 1:
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
		case 2:
			{
				glBegin(GL_QUADS);
				vindex=0;
				if (useObjectColor)
				{
					for (unsigned int i=0;i<numindices;i+=4)
					{

						glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);

						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
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

						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				glEnd();	
				break;
			}
		case 0:
			{
				glBegin(GL_TRIANGLES);
				vindex=0;
				if (useObjectColor)
				{
					for (unsigned int i=0;i<numindices;i+=3)
					{

						glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);

						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				else 
				{
					for (unsigned int i=0;i<numindices;i+=3)
					{

						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
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

void RAS_OpenGLRasterizer::IndexPrimitives_Ex(const vecVertexArray & vertexarrays,
									const vecIndexArrays & indexarrays,
									int mode,
									class RAS_IPolyMaterial* polymat,
									class RAS_IRenderTools* rendertools,
									bool useObjectColor,
									const MT_Vector4& rgbacolor
									)
{ 
	bool	recalc;
	GLenum drawmode;
	switch (mode)
	{
	case 0:
		drawmode = GL_TRIANGLES;
		break;
	case 1:
		drawmode = GL_LINES;
		break;
	case 2:
		drawmode = GL_QUADS;
		break;
	default:
		drawmode = GL_LINES;
		break;
	}
	
	const RAS_TexVert* vertexarray ;
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
		case 1:
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
		case 2:
			{
				glBegin(GL_QUADS);
				vindex=0;
				if (useObjectColor)
				{
					for (unsigned int i=0;i<numindices;i+=4)
					{
						MT_Point3 mv1, mv2, mv3, mv4, fnor;
						/* Calc a new face normal */

						if (vertexarray[(indexarray[vindex])].getFlag() & TV_CALCFACENORMAL)
							recalc= true;
						else
							recalc=false;

						if (recalc){
							mv1 = MT_Point3(vertexarray[(indexarray[vindex])].getLocalXYZ());
							mv2 = MT_Point3(vertexarray[(indexarray[vindex+1])].getLocalXYZ());
							mv3 = MT_Point3(vertexarray[(indexarray[vindex+2])].getLocalXYZ());
							mv4 = MT_Point3(vertexarray[(indexarray[vindex+2])].getLocalXYZ());
							
							fnor = (((mv2-mv1).cross(mv3-mv2))+((mv4-mv3).cross(mv1-mv4))).safe_normalized();

							glNormal3f(fnor[0], fnor[1], fnor[2]);
						}

						glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);

						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());						
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
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
						MT_Point3 mv1, mv2, mv3, mv4, fnor;
						/* Calc a new face normal */

						if (vertexarray[(indexarray[vindex])].getFlag() & TV_CALCFACENORMAL)
							recalc= true;
						else
							recalc=false;


						if (recalc){
							mv1 = MT_Point3(vertexarray[(indexarray[vindex])].getLocalXYZ());
							mv2 = MT_Point3(vertexarray[(indexarray[vindex+1])].getLocalXYZ());
							mv3 = MT_Point3(vertexarray[(indexarray[vindex+2])].getLocalXYZ());
							mv4 = MT_Point3(vertexarray[(indexarray[vindex+2])].getLocalXYZ());
							
							fnor = (((mv2-mv1).cross(mv3-mv2))+((mv4-mv3).cross(mv1-mv4))).safe_normalized();

							glNormal3f(fnor[0], fnor[1], fnor[2]);
						}

						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				glEnd();	
				break;
			}
		case 0:
			{
				glBegin(GL_TRIANGLES);
				vindex=0;
				if (useObjectColor)
				{
					for (unsigned int i=0;i<numindices;i+=3)
					{
						MT_Point3 mv1, mv2, mv3, fnor;
						/* Calc a new face normal */

						if (vertexarray[(indexarray[vindex])].getFlag() & TV_CALCFACENORMAL)
							recalc= true;
						else
							recalc=false;

						if (recalc){
							mv1 = MT_Point3(vertexarray[(indexarray[vindex])].getLocalXYZ());
							mv2 = MT_Point3(vertexarray[(indexarray[vindex+1])].getLocalXYZ());
							mv3 = MT_Point3(vertexarray[(indexarray[vindex+2])].getLocalXYZ());
							
							fnor = ((mv2-mv1).cross(mv3-mv2)).safe_normalized();
							glNormal3f(fnor[0], fnor[1], fnor[2]);
						}

						glColor4d(rgbacolor[0], rgbacolor[1], rgbacolor[2], rgbacolor[3]);

						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
					}
				}
				else 
				{
					for (unsigned int i=0;i<numindices;i+=3)
					{
						MT_Point3 mv1, mv2, mv3, fnor;
						/* Calc a new face normal */

						if (vertexarray[(indexarray[vindex])].getFlag() & TV_CALCFACENORMAL)
							recalc= true;
						else
							recalc=false;


						if (recalc){
							mv1 = MT_Point3(vertexarray[(indexarray[vindex])].getLocalXYZ());
							mv2 = MT_Point3(vertexarray[(indexarray[vindex+1])].getLocalXYZ());
							mv3 = MT_Point3(vertexarray[(indexarray[vindex+2])].getLocalXYZ());
							
							fnor = ((mv2-mv1).cross(mv3-mv2)).safe_normalized();
							glNormal3f(fnor[0], fnor[1], fnor[2]);
						}

						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
						glTexCoord2fv(vertexarray[(indexarray[vindex])].getUV1());
						glVertex3fv(vertexarray[(indexarray[vindex])].getLocalXYZ());
						vindex++;
						
						glColor4ubv((const GLubyte *)&(vertexarray[(indexarray[vindex])].getRGBA()));
						if (!recalc)
							glNormal3sv(vertexarray[(indexarray[vindex])].getNormal());
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
									int mode,
									class RAS_IPolyMaterial* polymat,
									class RAS_IRenderTools* rendertools,
									bool useObjectColor,
									const MT_Vector4& rgbacolor
									)
{ 
	GLenum drawmode;
	switch (mode)
	{
	case 0:
		drawmode = GL_TRIANGLES;
		break;
	case 1:
		drawmode = GL_LINES;
		break;
	case 2:
		drawmode = GL_QUADS;
		break;
	default:
		drawmode = GL_LINES;
		break;
	}
	
	const RAS_TexVert* vertexarray ;
	
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
		case 1:
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
		case 2:
			{
				vindex=0;
				for (unsigned int i=0;i<numindices;i+=4)
				{
					float v1[3],v2[3],v3[3],v4[3];
					
					char *cp= (char *)&(vertexarray[(indexarray[vindex])].getRGBA());
					v1[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v1[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v1[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					cp= (char *)&(vertexarray[(indexarray[vindex])].getRGBA());
					v2[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v2[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v2[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					cp= (char *)&(vertexarray[(indexarray[vindex])].getRGBA());
					v3[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v3[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v3[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					vindex++;
					
					cp= (char *)&(vertexarray[(indexarray[vindex])].getRGBA());
					v4[0] = vertexarray[(indexarray[vindex])].getLocalXYZ()[0];
					v4[1] = vertexarray[(indexarray[vindex])].getLocalXYZ()[1];
					v4[2] = vertexarray[(indexarray[vindex])].getLocalXYZ()[2];
					
					vindex++;
					
					rendertools->RenderText(polymat->GetDrawingMode(),polymat,v1,v2,v3,v4);
					ClearCachingInfo();
				}
				break;
			}
		case 0:
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



void RAS_OpenGLRasterizer::SetProjectionMatrix(MT_CmMatrix4x4 &mat)
{
	glMatrixMode(GL_PROJECTION);
	double* matrix = &mat(0,0);
	glLoadMatrixd(matrix);
}


void RAS_OpenGLRasterizer::SetProjectionMatrix(MT_Matrix4x4 & mat)
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
	bool perspective
){
	MT_Matrix4x4 result;
	double mat[16];

	// correction for stereo
	if(m_stereomode != RAS_STEREO_NOSTEREO)
	{
			float near_div_focallength;
			// next 2 params should be specified on command line and in Blender publisher
			m_focallength = 1.5 * right;  // derived from example
			m_eyeseparation = 0.18 * right;  // just a guess...

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
		const MT_Point3 &camLoc, const MT_Quaternion &camOrientQuat)
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

double RAS_OpenGLRasterizer::GetTime()
{
	return m_time;
}


