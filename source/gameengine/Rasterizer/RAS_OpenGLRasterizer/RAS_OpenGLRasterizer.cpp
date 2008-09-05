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

#include "GPU_draw.h"
#include "GPU_material.h"

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
	m_last_blendmode(GPU_BLEND_SOLID),
	m_last_frontface(true),
	m_materialCachingInfo(0)
{
	m_viewmatrix.setIdentity();
	m_viewinvmatrix.setIdentity();
	
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

bool RAS_OpenGLRasterizer::Init()
{
	GPU_state_init();

	m_redback = 0.4375;
	m_greenback = 0.4375;
	m_blueback = 0.4375;
	m_alphaback = 0.0;
	
	m_ambr = 0.0f;
	m_ambg = 0.0f;
	m_ambb = 0.0f;

	SetBlendingMode(GPU_BLEND_SOLID);
	SetFrontFace(true);

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

	SetBlendingMode(GPU_BLEND_SOLID);
	SetFrontFace(true);

	glShadeModel(GL_SMOOTH);

	m_2DCanvas->BeginFrame();
	
	return true;
}



void RAS_OpenGLRasterizer::SetDrawingMode(int drawingmode)
{
	m_drawingmode = drawingmode;

	if(m_drawingmode == KX_WIREFRAME)
		glDisable(GL_CULL_FACE);
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



const MT_Matrix4x4& RAS_OpenGLRasterizer::GetViewMatrix() const
{
	return m_viewmatrix;
}

const MT_Matrix4x4& RAS_OpenGLRasterizer::GetViewInvMatrix() const
{
	return m_viewinvmatrix;
}

void RAS_OpenGLRasterizer::IndexPrimitives_3DText(RAS_MeshSlot& ms,
									class RAS_IPolyMaterial* polymat,
									class RAS_IRenderTools* rendertools)
{ 
	bool obcolor = ms.m_bObjectColor;
	MT_Vector4& rgba = ms.m_RGBAcolor;
	RAS_MeshSlot::iterator it;

	// handle object color
	if (obcolor) {
		glDisableClientState(GL_COLOR_ARRAY);
		glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
	}
	else
		glEnableClientState(GL_COLOR_ARRAY);

	for(ms.begin(it); !ms.end(it); ms.next(it)) {
		RAS_TexVert *vertex;
		size_t i, j, numvert;
		
		numvert = it.array->m_type;

		if(it.array->m_type == RAS_DisplayArray::LINE) {
			// line drawing, no text
			glBegin(GL_LINES);

			for(i=0; i<it.totindex; i+=2)
			{
				vertex = &it.vertex[it.index[i]];
				glVertex3fv(vertex->getXYZ());

				vertex = &it.vertex[it.index[i+1]];
				glVertex3fv(vertex->getXYZ());
			}

			glEnd();
		}
		else {
			// triangle and quad text drawing
			for(i=0; i<it.totindex; i+=numvert)
			{
				float v[4][3];
				int glattrib, unit;

				for(j=0; j<numvert; j++) {
					vertex = &it.vertex[it.index[i+j]];

					v[j][0] = vertex->getXYZ()[0];
					v[j][1] = vertex->getXYZ()[1];
					v[j][2] = vertex->getXYZ()[2];
				}

				// find the right opengl attribute
				glattrib = -1;
				if(GLEW_ARB_vertex_program)
					for(unit=0; unit<m_attrib_num; unit++)
						if(m_attrib[unit] == RAS_TEXCO_UV1)
							glattrib = unit;
				
				rendertools->RenderText(polymat->GetDrawingMode(), polymat,
					v[0], v[1], v[2], (numvert == 4)? v[3]: NULL, glattrib);

				ClearCachingInfo();
			}
		}
	}

	glDisableClientState(GL_COLOR_ARRAY);
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
			if(tv.getFlag() & RAS_TexVert::SECOND_UV && (int)tv.getUnit() == unit) {
				glMultiTexCoord2fvARB(GL_TEXTURE0_ARB+unit, tv.getUV2());
				continue;
			}
			switch(m_texco[unit]) {
			case RAS_TEXCO_ORCO:
			case RAS_TEXCO_GLOB:
				glMultiTexCoord3fvARB(GL_TEXTURE0_ARB+unit, tv.getXYZ());
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
				glVertexAttrib3fvARB(unit, tv.getXYZ());
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

void RAS_OpenGLRasterizer::IndexPrimitives(RAS_MeshSlot& ms)
{
	IndexPrimitivesInternal(ms, false);
}

void RAS_OpenGLRasterizer::IndexPrimitivesMulti(RAS_MeshSlot& ms)
{
	IndexPrimitivesInternal(ms, true);
}

void RAS_OpenGLRasterizer::IndexPrimitivesInternal(RAS_MeshSlot& ms, bool multi)
{ 
	bool obcolor = ms.m_bObjectColor;
	bool wireframe = m_drawingmode <= KX_WIREFRAME;
	MT_Vector4& rgba = ms.m_RGBAcolor;
	RAS_MeshSlot::iterator it;

	// iterate over display arrays, each containing an index + vertex array
	for(ms.begin(it); !ms.end(it); ms.next(it)) {
		RAS_TexVert *vertex;
		size_t i, j, numvert;
		
		numvert = it.array->m_type;

		if(it.array->m_type == RAS_DisplayArray::LINE) {
			// line drawing
			glBegin(GL_LINES);

			for(i=0; i<it.totindex; i+=2)
			{
				vertex = &it.vertex[it.index[i]];
				glVertex3fv(vertex->getXYZ());

				vertex = &it.vertex[it.index[i+1]];
				glVertex3fv(vertex->getXYZ());
			}

			glEnd();
		}
		else {
			// triangle and quad drawing
			if(it.array->m_type == RAS_DisplayArray::TRIANGLE)
				glBegin(GL_TRIANGLES);
			else
				glBegin(GL_QUADS);

			for(i=0; i<it.totindex; i+=numvert)
			{
				if(obcolor)
					glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);

				for(j=0; j<numvert; j++) {
					vertex = &it.vertex[it.index[i+j]];

					if(!wireframe) {
						if(!obcolor)
							glColor4ubv((const GLubyte *)(vertex->getRGBA()));

						glNormal3fv(vertex->getNormal());

						if(multi)
							TexCoord(*vertex);
						else
							glTexCoord2fv(vertex->getUV1());
					}

					glVertex3fv(vertex->getXYZ());
				}
			}

			glEnd();
		}
	}
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
	m_viewmatrix = mat;

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
				m_viewmatrix *= transform;
				}
				break;
			case RAS_STEREO_RIGHTEYE:
				{
				// translate to right by half the eye distance
				MT_Transform transform;
				transform.setIdentity();
				transform.translate(eyeline * m_eyeseparation / 2.0);
				m_viewmatrix *= transform;
				}
				break;
		}
	}

	m_viewinvmatrix = m_viewmatrix;
	m_viewinvmatrix.invert();

	// note: getValue gives back column major as needed by OpenGL
	MT_Scalar glviewmat[16];
	m_viewmatrix.getValue(glviewmat);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(glviewmat);
	m_campos = campos;
}


const MT_Point3& RAS_OpenGLRasterizer::GetCameraPosition()
{
	return m_campos;
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

void RAS_OpenGLRasterizer::SetBlendingMode(int blendmode)
{
	if(blendmode == m_last_blendmode)
		return;

	if(blendmode == GPU_BLEND_SOLID) {
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if(blendmode == GPU_BLEND_ADD) {
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
	}
	else if(blendmode == GPU_BLEND_ALPHA) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.0f);
	}
	else if(blendmode == GPU_BLEND_CLIP) {
		glDisable(GL_BLEND); 
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
	}

	m_last_blendmode = blendmode;
}

void RAS_OpenGLRasterizer::SetFrontFace(bool ccw)
{
	if(m_last_frontface == ccw)
		return;

	if(ccw)
		glFrontFace(GL_CCW);
	else
		glFrontFace(GL_CW);
	
	m_last_frontface = ccw;
}

