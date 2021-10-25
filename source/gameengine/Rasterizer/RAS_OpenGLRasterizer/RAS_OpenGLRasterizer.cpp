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

/** \file gameengine/Rasterizer/RAS_OpenGLRasterizer/RAS_OpenGLRasterizer.cpp
 *  \ingroup bgerastogl
 */

 
#include <math.h>
#include <stdlib.h>
 
#include "RAS_OpenGLRasterizer.h"

#include "GPU_glew.h"

#include "RAS_ICanvas.h"
#include "RAS_Rect.h"
#include "RAS_TexVert.h"
#include "RAS_MeshObject.h"
#include "RAS_Polygon.h"
#include "RAS_ILightObject.h"
#include "MT_CmMatrix4x4.h"

#include "RAS_OpenGLLight.h"
#include "RAS_OpenGLOffScreen.h"
#include "RAS_OpenGLSync.h"

#include "RAS_StorageVA.h"
#include "RAS_StorageVBO.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"
#include "GPU_shader.h"

extern "C"{
	#include "BLF_api.h"
	#include "BKE_DerivedMesh.h"
}


// XXX Clean these up <<<
#include "EXP_Value.h"
#include "KX_Scene.h"
#include "KX_RayCast.h"
#include "KX_GameObject.h"
// >>>

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

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

RAS_OpenGLRasterizer::RAS_OpenGLRasterizer(RAS_ICanvas* canvas, RAS_STORAGE_TYPE storage)
	:RAS_IRasterizer(canvas),
	m_2DCanvas(canvas),
	m_fogenabled(false),
	m_time(0.0f),
	m_campos(0.0f, 0.0f, 0.0f),
	m_camortho(false),
	m_camnegscale(false),
	m_stereomode(RAS_STEREO_NOSTEREO),
	m_curreye(RAS_STEREO_LEFTEYE),
	m_eyeseparation(0.0f),
	m_focallength(0.0f),
	m_setfocallength(false),
	m_noOfScanlines(32),
	m_motionblur(0),
	m_motionblurvalue(-1.0f),
	m_usingoverrideshader(false),
	m_clientobject(NULL),
	m_auxilaryClientInfo(NULL),
	m_drawingmode(KX_TEXTURED),
	m_texco_num(0),
	m_attrib_num(0),
	//m_last_alphablend(GPU_BLEND_SOLID),
	m_last_frontface(true),
	m_materialCachingInfo(0),
	m_storage_type(storage)
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

	m_prevafvalue = GPU_get_anisotropic();

	if (m_storage_type == RAS_VBO /*|| m_storage_type == RAS_AUTO_STORAGE && GLEW_ARB_vertex_buffer_object*/) {
		m_storage = new RAS_StorageVBO(&m_texco_num, m_texco, &m_attrib_num, m_attrib, m_attrib_layer);
	}
	else if ((m_storage_type == RAS_VA) || (m_storage_type == RAS_AUTO_STORAGE)) {
		m_storage = new RAS_StorageVA(&m_texco_num, m_texco, &m_attrib_num, m_attrib, m_attrib_layer);
	}
	else {
		printf("Unknown rasterizer storage type, falling back to vertex arrays\n");
		m_storage = new RAS_StorageVA(&m_texco_num, m_texco, &m_attrib_num, m_attrib, m_attrib_layer);
	}

	glGetIntegerv(GL_MAX_LIGHTS, (GLint *) &m_numgllights);
	if (m_numgllights < 8)
		m_numgllights = 8;
}



RAS_OpenGLRasterizer::~RAS_OpenGLRasterizer()
{
	// Restore the previous AF value
	GPU_set_anisotropic(m_prevafvalue);

	if (m_storage)
		delete m_storage;
}

bool RAS_OpenGLRasterizer::Init()
{
	bool storage_init;
	GPU_state_init();


	m_ambr = 0.0f;
	m_ambg = 0.0f;
	m_ambb = 0.0f;

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	//m_last_alphablend = GPU_BLEND_SOLID;
	GPU_set_material_alpha_blend(GPU_BLEND_SOLID);

	glFrontFace(GL_CCW);
	m_last_frontface = true;

	m_redback = 0.4375f;
	m_greenback = 0.4375f;
	m_blueback = 0.4375f;
	m_alphaback = 0.0f;

	glClearColor(m_redback,m_greenback,m_blueback,m_alphaback);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	glShadeModel(GL_SMOOTH);

	storage_init = m_storage->Init();

	return true && storage_init;
}


void RAS_OpenGLRasterizer::SetAmbientColor(float color[3])
{
	m_ambr = color[0];
	m_ambg = color[1];
	m_ambb = color[2];
}

void RAS_OpenGLRasterizer::SetAmbient(float factor)
{
	float ambient[] = {m_ambr * factor, m_ambg * factor, m_ambb * factor, 1.0f};
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
}

void RAS_OpenGLRasterizer::SetBackColor(float color[3])
{
	m_redback = color[0];
	m_greenback = color[1];
	m_blueback = color[2];
	m_alphaback = 0.0f;
}

void RAS_OpenGLRasterizer::SetFog(short type, float start, float dist, float intensity, float color[3])
{
	float params[4] = {color[0], color[1], color[2], 1.0f};
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_DENSITY, intensity / 10.0f);
	glFogf(GL_FOG_START, start);
	glFogf(GL_FOG_END, start + dist);
	glFogfv(GL_FOG_COLOR, params);
}

void RAS_OpenGLRasterizer::EnableFog(bool enable)
{
	m_fogenabled = enable;
}

void RAS_OpenGLRasterizer::DisplayFog()
{
	if ((m_drawingmode >= KX_SOLID) && m_fogenabled) {
		glEnable(GL_FOG);
	}
	else {
		glDisable(GL_FOG);
	}
}

bool RAS_OpenGLRasterizer::SetMaterial(const RAS_IPolyMaterial& mat)
{
	return mat.Activate(this, m_materialCachingInfo);
}



void RAS_OpenGLRasterizer::Exit()
{
	m_storage->Exit();

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0f);
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

bool RAS_OpenGLRasterizer::BeginFrame(double time)
{
	m_time = time;

	// Blender camera routine destroys the settings
	if (m_drawingmode < KX_SOLID)
	{
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
	}
	else
	{
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	//m_last_alphablend = GPU_BLEND_SOLID;
	GPU_set_material_alpha_blend(GPU_BLEND_SOLID);

	glFrontFace(GL_CCW);
	m_last_frontface = true;

	glShadeModel(GL_SMOOTH);

	glEnable(GL_MULTISAMPLE_ARB);

	m_2DCanvas->BeginFrame();

	// Render Tools
	m_clientobject = NULL;
	m_lastlightlayer = -1;
	m_lastauxinfo = NULL;
	m_lastlighting = true; /* force disable in DisableOpenGLLights() */
	DisableOpenGLLights();
	
	return true;
}



void RAS_OpenGLRasterizer::SetDrawingMode(int drawingmode)
{
	m_drawingmode = drawingmode;

	if (m_drawingmode == KX_WIREFRAME)
		glDisable(GL_CULL_FACE);

	m_storage->SetDrawingMode(drawingmode);
}

int RAS_OpenGLRasterizer::GetDrawingMode()
{
	return m_drawingmode;
}


void RAS_OpenGLRasterizer::SetDepthMask(DepthMask depthmask)
{
	glDepthMask(depthmask == KX_DEPTHMASK_DISABLED ? GL_FALSE : GL_TRUE);
}


void RAS_OpenGLRasterizer::ClearColorBuffer()
{
	m_2DCanvas->ClearColor(m_redback,m_greenback,m_blueback,m_alphaback);
	m_2DCanvas->ClearBuffer(RAS_ICanvas::COLOR_BUFFER);
}


void RAS_OpenGLRasterizer::ClearDepthBuffer()
{
	m_2DCanvas->ClearBuffer(RAS_ICanvas::DEPTH_BUFFER);
}


void RAS_OpenGLRasterizer::ClearCachingInfo(void)
{
	m_materialCachingInfo = 0;
}

void RAS_OpenGLRasterizer::FlushDebugShapes(SCA_IScene *scene)
{
	std::vector<OglDebugShape> &debugShapes = m_debugShapes[scene];
	if (debugShapes.empty())
		return;

	// DrawDebugLines
	GLboolean light, tex;

	light= glIsEnabled(GL_LIGHTING);
	tex= glIsEnabled(GL_TEXTURE_2D);

	if (light) glDisable(GL_LIGHTING);
	if (tex) glDisable(GL_TEXTURE_2D);

	// draw lines
	glBegin(GL_LINES);
	for (unsigned int i = 0; i < debugShapes.size(); i++) {
		if (debugShapes[i].m_type != OglDebugShape::LINE)
			continue;
		glColor4f(debugShapes[i].m_color[0], debugShapes[i].m_color[1], debugShapes[i].m_color[2], 1.0f);
		const MT_Scalar *fromPtr = &debugShapes[i].m_pos.x();
		const MT_Scalar *toPtr= &debugShapes[i].m_param.x();
		glVertex3fv(fromPtr);
		glVertex3fv(toPtr);
	}
	glEnd();

	// draw circles
	for (unsigned int i = 0; i < debugShapes.size(); i++) {
		if (debugShapes[i].m_type != OglDebugShape::CIRCLE)
			continue;
		glBegin(GL_LINE_LOOP);
		glColor4f(debugShapes[i].m_color[0], debugShapes[i].m_color[1], debugShapes[i].m_color[2], 1.0f);

		static const MT_Vector3 worldUp(0.0f, 0.0f, 1.0f);
		MT_Vector3 norm = debugShapes[i].m_param;
		MT_Matrix3x3 tr;
		if (norm.fuzzyZero() || norm == worldUp)
		{
			tr.setIdentity();
		}
		else
		{
			MT_Vector3 xaxis, yaxis;
			xaxis = MT_cross(norm, worldUp);
			yaxis = MT_cross(xaxis, norm);
			tr.setValue(xaxis.x(), xaxis.y(), xaxis.z(),
				yaxis.x(), yaxis.y(), yaxis.z(),
				norm.x(), norm.y(), norm.z());
		}
		MT_Scalar rad = debugShapes[i].m_param2.x();
		int n = (int)debugShapes[i].m_param2.y();
		for (int j = 0; j<n; j++)
		{
			MT_Scalar theta = j*(float)M_PI*2/n;
			MT_Vector3 pos(cosf(theta) * rad, sinf(theta) * rad, 0.0f);
			pos = pos*tr;
			pos += debugShapes[i].m_pos;
			const MT_Scalar* posPtr = &pos.x();
			glVertex3fv(posPtr);
		}
		glEnd();
	}

	if (light) glEnable(GL_LIGHTING);
	if (tex) glEnable(GL_TEXTURE_2D);

	debugShapes.clear();
}

void RAS_OpenGLRasterizer::EndFrame()
{
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glDisable(GL_MULTISAMPLE_ARB);

	m_2DCanvas->EndFrame();
}

void RAS_OpenGLRasterizer::SetRenderArea()
{
	RAS_Rect area;
	// only above/below stereo method needs viewport adjustment
	switch (m_stereomode)
	{
		case RAS_STEREO_ABOVEBELOW:
			switch (m_curreye) {
				case RAS_STEREO_LEFTEYE:
					// upper half of window
					area.SetLeft(0);
					area.SetBottom(m_2DCanvas->GetHeight() -
						int(m_2DCanvas->GetHeight() - m_noOfScanlines) / 2);
	
					area.SetRight(int(m_2DCanvas->GetWidth()));
					area.SetTop(int(m_2DCanvas->GetHeight()));
					m_2DCanvas->SetDisplayArea(&area);
					break;
				case RAS_STEREO_RIGHTEYE:
					// lower half of window
					area.SetLeft(0);
					area.SetBottom(0);
					area.SetRight(int(m_2DCanvas->GetWidth()));
					area.SetTop(int(m_2DCanvas->GetHeight() - m_noOfScanlines) / 2);
					m_2DCanvas->SetDisplayArea(&area);
					break;
			}
			break;
		case RAS_STEREO_3DTVTOPBOTTOM:
			switch (m_curreye) {
				case RAS_STEREO_LEFTEYE:
					// upper half of window
					area.SetLeft(0);
					area.SetBottom(m_2DCanvas->GetHeight() -
						m_2DCanvas->GetHeight() / 2);
	
					area.SetRight(m_2DCanvas->GetWidth());
					area.SetTop(m_2DCanvas->GetHeight());
					m_2DCanvas->SetDisplayArea(&area);
					break;
				case RAS_STEREO_RIGHTEYE:
					// lower half of window
					area.SetLeft(0);
					area.SetBottom(0);
					area.SetRight(m_2DCanvas->GetWidth());
					area.SetTop(m_2DCanvas->GetHeight() / 2);
					m_2DCanvas->SetDisplayArea(&area);
					break;
			}
			break;
		case RAS_STEREO_SIDEBYSIDE:
			switch (m_curreye)
			{
				case RAS_STEREO_LEFTEYE:
					// Left half of window
					area.SetLeft(0);
					area.SetBottom(0);
					area.SetRight(m_2DCanvas->GetWidth()/2);
					area.SetTop(m_2DCanvas->GetHeight());
					m_2DCanvas->SetDisplayArea(&area);
					break;
				case RAS_STEREO_RIGHTEYE:
					// Right half of window
					area.SetLeft(m_2DCanvas->GetWidth()/2);
					area.SetBottom(0);
					area.SetRight(m_2DCanvas->GetWidth());
					area.SetTop(m_2DCanvas->GetHeight());
					m_2DCanvas->SetDisplayArea(&area);
					break;
			}
			break;
		default:
			// every available pixel
			area.SetLeft(0);
			area.SetBottom(0);
			area.SetRight(int(m_2DCanvas->GetWidth()));
			area.SetTop(int(m_2DCanvas->GetHeight()));
			m_2DCanvas->SetDisplayArea(&area);
			break;
	}
}
	
void RAS_OpenGLRasterizer::SetStereoMode(const StereoMode stereomode)
{
	m_stereomode = stereomode;
}

RAS_IRasterizer::StereoMode RAS_OpenGLRasterizer::GetStereoMode()
{
	return m_stereomode;
}

bool RAS_OpenGLRasterizer::Stereo()
{
	if (m_stereomode > RAS_STEREO_NOSTEREO) // > 0
		return true;
	else
		return false;
}

bool RAS_OpenGLRasterizer::InterlacedStereo()
{
	return m_stereomode == RAS_STEREO_VINTERLACE || m_stereomode == RAS_STEREO_INTERLACED;
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
			if (m_curreye == RAS_STEREO_LEFTEYE) {
				glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
			}
			else {
				//glAccum(GL_LOAD, 1.0f);
				glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_FALSE);
				ClearDepthBuffer();
			}
			break;
		case RAS_STEREO_VINTERLACE:
		{
			// OpenGL stippling is deprecated, it is no longer possible to affect all shaders
			// this way, offscreen rendering and then compositing may be the better solution
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

RAS_IOffScreen *RAS_OpenGLRasterizer::CreateOffScreen(int width, int height, int samples, int target)
{
	RAS_IOffScreen *ofs;

	ofs = new RAS_OpenGLOffScreen(m_2DCanvas);

	if (!ofs->Create(width, height, samples, (RAS_IOffScreen::RAS_OFS_RENDER_TARGET)target)) {
		delete ofs;
		return NULL;
	}
	return ofs;
}

RAS_ISync *RAS_OpenGLRasterizer::CreateSync(int type)
{
	RAS_ISync *sync;

	sync = new RAS_OpenGLSync();

	if (!sync->Create((RAS_ISync::RAS_SYNC_TYPE)type)) {
		delete sync;
		return NULL;
	}
	return sync;
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
									class RAS_IPolyMaterial* polymat)
{ 
	bool obcolor = ms.m_bObjectColor;
	MT_Vector4& rgba = ms.m_RGBAcolor;
	RAS_MeshSlot::iterator it;

	const STR_String& mytext = ((CValue*)m_clientobject)->GetPropertyText("Text");

	// handle object color
	if (obcolor) {
		glDisableClientState(GL_COLOR_ARRAY);
		glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
	}
	else
		glEnableClientState(GL_COLOR_ARRAY);

	for (ms.begin(it); !ms.end(it); ms.next(it)) {
		RAS_TexVert *vertex;
		size_t i, j, numvert;
		
		numvert = it.array->m_type;

		if (it.array->m_type == RAS_DisplayArray::LINE) {
			// line drawing, no text
			glBegin(GL_LINES);

			for (i=0; i<it.totindex; i+=2)
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
			for (i=0; i<it.totindex; i+=numvert)
			{
				float  v[4][3];
				const float  *v_ptr[4] = {NULL};
				const float *uv_ptr[4] = {NULL};
				int glattrib, unit;

				for (j=0; j<numvert; j++) {
					vertex = &it.vertex[it.index[i+j]];

					v[j][0] = vertex->getXYZ()[0];
					v[j][1] = vertex->getXYZ()[1];
					v[j][2] = vertex->getXYZ()[2];
					v_ptr[j] = v[j];

					uv_ptr[j] = vertex->getUV(0);
				}

				// find the right opengl attribute
				glattrib = -1;
				if (GLEW_ARB_vertex_program)
					for (unit=0; unit<m_attrib_num; unit++)
						if (m_attrib[unit] == RAS_TEXCO_UV)
							glattrib = unit;

				GPU_render_text(
				        polymat->GetMTexPoly(), polymat->GetDrawingMode(), mytext, mytext.Length(), polymat->GetMCol(),
				        v_ptr, uv_ptr, glattrib);

				ClearCachingInfo();
			}
		}
	}

	glDisableClientState(GL_COLOR_ARRAY);
}

void RAS_OpenGLRasterizer::SetTexCoordNum(int num)
{
	m_texco_num = num;
	if (m_texco_num > RAS_MAX_TEXCO)
		m_texco_num = RAS_MAX_TEXCO;
}

void RAS_OpenGLRasterizer::SetAttribNum(int num)
{
	m_attrib_num = num;
	if (m_attrib_num > RAS_MAX_ATTRIB)
		m_attrib_num = RAS_MAX_ATTRIB;
}

void RAS_OpenGLRasterizer::SetTexCoord(TexCoGen coords, int unit)
{
	// this changes from material to material
	if (unit < RAS_MAX_TEXCO)
		m_texco[unit] = coords;
}

void RAS_OpenGLRasterizer::SetAttrib(TexCoGen coords, int unit, int layer)
{
	// this changes from material to material
	if (unit < RAS_MAX_ATTRIB) {
		m_attrib[unit] = coords;
		m_attrib_layer[unit] = layer;
	}
}

void RAS_OpenGLRasterizer::IndexPrimitives(RAS_MeshSlot& ms)
{
	if (ms.m_pDerivedMesh)
		DrawDerivedMesh(ms);
	else
		m_storage->IndexPrimitives(ms);
}

// Code for hooking into Blender's mesh drawing for derived meshes.
// If/when we use more of Blender's drawing code, we may be able to
// clean this up
static bool current_wireframe;
static RAS_MaterialBucket *current_bucket;
static RAS_IPolyMaterial *current_polymat;
static RAS_MeshSlot *current_ms;
static RAS_MeshObject *current_mesh;
static int current_blmat_nr;
static GPUVertexAttribs current_gpu_attribs;
static Image *current_image;
static int CheckMaterialDM(int matnr, void *attribs)
{
	// only draw the current material
	if (matnr != current_blmat_nr)
		return 0;
	GPUVertexAttribs *gattribs = (GPUVertexAttribs *)attribs;
	if (gattribs)
		memcpy(gattribs, &current_gpu_attribs, sizeof(GPUVertexAttribs));
	return 1;
}

static DMDrawOption CheckTexDM(MTexPoly *mtexpoly, const bool has_mcol, int matnr)
{

	// index is the original face index, retrieve the polygon
	if (matnr == current_blmat_nr &&
		(mtexpoly == NULL || mtexpoly->tpage == current_image)) {
		// must handle color.
		if (current_wireframe)
			return DM_DRAW_OPTION_NO_MCOL;
		if (current_ms->m_bObjectColor) {
			MT_Vector4& rgba = current_ms->m_RGBAcolor;
			glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			// don't use mcol
			return DM_DRAW_OPTION_NO_MCOL;
		}
		if (!has_mcol) {
			// we have to set the color from the material
			unsigned char rgba[4];
			current_polymat->GetMaterialRGBAColor(rgba);
			glColor4ubv((const GLubyte *)rgba);
			return DM_DRAW_OPTION_NORMAL;
		}
		return DM_DRAW_OPTION_NORMAL;
	}
	return DM_DRAW_OPTION_SKIP;
}

void RAS_OpenGLRasterizer::DrawDerivedMesh(class RAS_MeshSlot &ms)
{
	// mesh data is in derived mesh
	current_bucket = ms.m_bucket;
	current_polymat = current_bucket->GetPolyMaterial();
	current_ms = &ms;
	current_mesh = ms.m_mesh;
	current_wireframe = m_drawingmode <= RAS_IRasterizer::KX_WIREFRAME;
	// MCol *mcol = (MCol*)ms.m_pDerivedMesh->getFaceDataArray(ms.m_pDerivedMesh, CD_MCOL); /* UNUSED */

	// handle two-side
	if (current_polymat->GetDrawingMode() & RAS_IRasterizer::KX_BACKCULL)
		this->SetCullFace(true);
	else
		this->SetCullFace(false);

	if (current_polymat->GetFlag() & RAS_BLENDERGLSL) {
		// GetMaterialIndex return the original mface material index,
		// increment by 1 to match what derived mesh is doing
		current_blmat_nr = current_polymat->GetMaterialIndex()+1;
		// For GLSL we need to retrieve the GPU material attribute
		Material* blmat = current_polymat->GetBlenderMaterial();
		Scene* blscene = current_polymat->GetBlenderScene();
		if (!current_wireframe && blscene && blmat)
			GPU_material_vertex_attributes(GPU_material_from_blender(blscene, blmat, false), &current_gpu_attribs);
		else
			memset(&current_gpu_attribs, 0, sizeof(current_gpu_attribs));
		// DM draw can mess up blending mode, restore at the end
		int current_blend_mode = GPU_get_material_alpha_blend();
		ms.m_pDerivedMesh->drawFacesGLSL(ms.m_pDerivedMesh, CheckMaterialDM);
		GPU_set_material_alpha_blend(current_blend_mode);
	} else {
		//ms.m_pDerivedMesh->drawMappedFacesTex(ms.m_pDerivedMesh, CheckTexfaceDM, mcol);
		current_blmat_nr = current_polymat->GetMaterialIndex();
		current_image = current_polymat->GetBlenderImage();
		ms.m_pDerivedMesh->drawFacesTex(ms.m_pDerivedMesh, CheckTexDM, NULL, NULL, DM_DRAW_USE_ACTIVE_UV);
	}
}

void RAS_OpenGLRasterizer::SetProjectionMatrix(MT_CmMatrix4x4 &mat)
{
	glMatrixMode(GL_PROJECTION);
	float* matrix = &mat(0, 0);
	glLoadMatrixf(matrix);

	m_camortho = (mat(3, 3) != 0.0f);
}

void RAS_OpenGLRasterizer::SetProjectionMatrix(const MT_Matrix4x4 & mat)
{
	glMatrixMode(GL_PROJECTION);
	float matrix[16];
	/* Get into argument. Looks a bit dodgy, but it's ok. */
	mat.getValue(matrix);
	glLoadMatrixf(matrix);

	m_camortho = (mat[3][3] != 0.0f);
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
) {
	MT_Matrix4x4 result;
	float mat[16];

	// correction for stereo
	if (Stereo())
	{
			float near_div_focallength;
			float offset;

			// if Rasterizer.setFocalLength is not called we use the camera focallength
			if (!m_setfocallength)
				// if focallength is null we use a value known to be reasonable
				m_focallength = (focallength == 0.f) ? m_eyeseparation * 30.0f
					: focallength;

			near_div_focallength = frustnear / m_focallength;
			offset = 0.5f * m_eyeseparation * near_div_focallength;
			switch (m_curreye) {
				case RAS_STEREO_LEFTEYE:
						left += offset;
						right += offset;
						break;
				case RAS_STEREO_RIGHTEYE:
						left -= offset;
						right -= offset;
						break;
			}
			// leave bottom and top untouched
			if (m_stereomode == RAS_STEREO_3DTVTOPBOTTOM) {
				// restore the vertical frustum because the 3DTV will
				// expand the top and bottom part to the full size of the screen
				bottom *= 2.0f;
				top *= 2.0f;
			}
	}
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(left, right, bottom, top, frustnear, frustfar);
		
	glGetFloatv(GL_PROJECTION_MATRIX, mat);
	result.setValue(mat);

	return result;
}

MT_Matrix4x4 RAS_OpenGLRasterizer::GetOrthoMatrix(
	float left,
	float right,
	float bottom,
	float top,
	float frustnear,
	float frustfar
) {
	MT_Matrix4x4 result;
	float mat[16];

	// stereo is meaningless for orthographic, disable it
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(left, right, bottom, top, frustnear, frustfar);
		
	glGetFloatv(GL_PROJECTION_MATRIX, mat);
	result.setValue(mat);

	return result;
}


// next arguments probably contain redundant info, for later...
void RAS_OpenGLRasterizer::SetViewMatrix(const MT_Matrix4x4 &mat, 
										 const MT_Matrix3x3 & camOrientMat3x3,
										 const MT_Point3 & pos,
										 const MT_Vector3 &scale,
										 bool perspective)
{
	m_viewmatrix = mat;

	// correction for stereo
	if (Stereo() && perspective)
	{
		MT_Vector3 unitViewDir(0.0f, -1.0f, 0.0f);  // minus y direction, Blender convention
		MT_Vector3 unitViewupVec(0.0f, 0.0f, 1.0f);
		MT_Vector3 viewDir, viewupVec;
		MT_Vector3 eyeline;

		// actual viewDir
		viewDir = camOrientMat3x3 * unitViewDir;  // this is the moto convention, vector on right hand side
		// actual viewup vec
		viewupVec = camOrientMat3x3 * unitViewupVec;

		// vector between eyes
		eyeline = viewDir.cross(viewupVec);

		switch (m_curreye) {
			case RAS_STEREO_LEFTEYE:
				{
				// translate to left by half the eye distance
				MT_Transform transform;
				transform.setIdentity();
				transform.translate(-(eyeline * m_eyeseparation / 2.0f));
				m_viewmatrix *= transform;
				}
				break;
			case RAS_STEREO_RIGHTEYE:
				{
				// translate to right by half the eye distance
				MT_Transform transform;
				transform.setIdentity();
				transform.translate(eyeline * m_eyeseparation / 2.0f);
				m_viewmatrix *= transform;
				}
				break;
		}
	}

	bool negX = (scale[0] < 0.0f);
	bool negY = (scale[1] < 0.0f);
	bool negZ = (scale[2] < 0.0f);
	if (negX || negY || negZ) {
		m_viewmatrix.tscale((negX)?-1.0f:1.0f, (negY)?-1.0f:1.0f, (negZ)?-1.0f:1.0f, 1.0);
	}
	m_viewinvmatrix = m_viewmatrix;
	m_viewinvmatrix.invert();

	// note: getValue gives back column major as needed by OpenGL
	MT_Scalar glviewmat[16];
	m_viewmatrix.getValue(glviewmat);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glviewmat);
	m_campos = pos;
	m_camnegscale = negX ^ negY ^ negZ;
}


const MT_Point3& RAS_OpenGLRasterizer::GetCameraPosition()
{
	return m_campos;
}

bool RAS_OpenGLRasterizer::GetCameraOrtho()
{
	return m_camortho;
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
	/* don't just set m_motionblur to 1, but check if it is 0 so
	 * we don't reset a motion blur that is already enabled */
	if (m_motionblur == 0)
		m_motionblur = 1;
	m_motionblurvalue = motionblurvalue;
}

void RAS_OpenGLRasterizer::DisableMotionBlur()
{
	m_motionblur = 0;
	m_motionblurvalue = -1.0f;
}

void RAS_OpenGLRasterizer::SetAlphaBlend(int alphablend)
{
	/* Variance shadow maps don't handle alpha well, best to not allow it for now  */
	if (m_drawingmode == KX_SHADOW && m_usingoverrideshader)
		GPU_set_material_alpha_blend(GPU_BLEND_SOLID);
	else
		GPU_set_material_alpha_blend(alphablend);
/*
	if (alphablend == m_last_alphablend)
		return;

	if (alphablend == GPU_BLEND_SOLID) {
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else if (alphablend == GPU_BLEND_ADD) {
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
	}
	else if (alphablend == GPU_BLEND_ALPHA) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.0f);
	}
	else if (alphablend == GPU_BLEND_CLIP) {
		glDisable(GL_BLEND); 
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.5f);
	}

	m_last_alphablend = alphablend;
*/
}

void RAS_OpenGLRasterizer::SetFrontFace(bool ccw)
{
	if (m_camnegscale)
		ccw = !ccw;

	if (m_last_frontface == ccw)
		return;

	if (ccw)
		glFrontFace(GL_CCW);
	else
		glFrontFace(GL_CW);
	
	m_last_frontface = ccw;
}

void RAS_OpenGLRasterizer::SetAnisotropicFiltering(short level)
{
	GPU_set_anisotropic((float)level);
}

short RAS_OpenGLRasterizer::GetAnisotropicFiltering()
{
	return (short)GPU_get_anisotropic();
}

void RAS_OpenGLRasterizer::SetMipmapping(MipmapOption val)
{
	if (val == RAS_IRasterizer::RAS_MIPMAP_LINEAR)
	{
		GPU_set_linear_mipmap(1);
		GPU_set_mipmap(1);
	}
	else if (val == RAS_IRasterizer::RAS_MIPMAP_NEAREST)
	{
		GPU_set_linear_mipmap(0);
		GPU_set_mipmap(1);
	}
	else
	{
		GPU_set_linear_mipmap(0);
		GPU_set_mipmap(0);
	}
}

RAS_IRasterizer::MipmapOption RAS_OpenGLRasterizer::GetMipmapping()
{
	if (GPU_get_mipmap()) {
		if (GPU_get_linear_mipmap()) {
			return RAS_IRasterizer::RAS_MIPMAP_LINEAR;
		}
		else {
			return RAS_IRasterizer::RAS_MIPMAP_NEAREST;
		}
	}
	else {
		return RAS_IRasterizer::RAS_MIPMAP_NONE;
	}
}

void RAS_OpenGLRasterizer::SetUsingOverrideShader(bool val)
{
	m_usingoverrideshader = val;
}

bool RAS_OpenGLRasterizer::GetUsingOverrideShader()
{
	return m_usingoverrideshader;
}

/**
 * Render Tools
 */

/* ProcessLighting performs lighting on objects. the layer is a bitfield that
 * contains layer information. There are 20 'official' layers in blender. A
 * light is applied on an object only when they are in the same layer. OpenGL
 * has a maximum of 8 lights (simultaneous), so 20 * 8 lights are possible in
 * a scene. */

void RAS_OpenGLRasterizer::ProcessLighting(bool uselights, const MT_Transform& viewmat)
{
	bool enable = false;
	int layer= -1;

	/* find the layer */
	if (uselights) {
		if (m_clientobject)
			layer = static_cast<KX_GameObject*>(m_clientobject)->GetLayer();
	}

	/* avoid state switching */
	if (m_lastlightlayer == layer && m_lastauxinfo == m_auxilaryClientInfo)
		return;

	m_lastlightlayer = layer;
	m_lastauxinfo = m_auxilaryClientInfo;

	/* enable/disable lights as needed */
	if (layer >= 0) {
		//enable = ApplyLights(layer, viewmat);
		// taken from blender source, incompatibility between Blender Object / GameObject
		KX_Scene* kxscene = (KX_Scene*)m_auxilaryClientInfo;
		float glviewmat[16];
		unsigned int count;
		std::vector<RAS_OpenGLLight*>::iterator lit = m_lights.begin();

		for (count=0; count<m_numgllights; count++)
			glDisable((GLenum)(GL_LIGHT0+count));

		viewmat.getValue(glviewmat);

		glPushMatrix();
		glLoadMatrixf(glviewmat);
		for (lit = m_lights.begin(), count = 0; !(lit==m_lights.end()) && count < m_numgllights; ++lit)
		{
			RAS_OpenGLLight* light = (*lit);

			if (light->ApplyFixedFunctionLighting(kxscene, layer, count))
				count++;
		}
		glPopMatrix();

		enable = count > 0;
	}

	if (enable)
		EnableOpenGLLights();
	else
		DisableOpenGLLights();
}

void RAS_OpenGLRasterizer::EnableOpenGLLights()
{
	if (m_lastlighting == true)
		return;

	glEnable(GL_LIGHTING);
	glEnable(GL_COLOR_MATERIAL);

	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, (GetCameraOrtho())? GL_FALSE: GL_TRUE);
	if (GLEW_EXT_separate_specular_color || GLEW_VERSION_1_2)
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL, GL_SEPARATE_SPECULAR_COLOR);

	m_lastlighting = true;
}

void RAS_OpenGLRasterizer::DisableOpenGLLights()
{
	if (m_lastlighting == false)
		return;

	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);

	m_lastlighting = false;
}

RAS_ILightObject *RAS_OpenGLRasterizer::CreateLight()
{
	return new RAS_OpenGLLight(this);
}

void RAS_OpenGLRasterizer::AddLight(RAS_ILightObject* lightobject)
{
	RAS_OpenGLLight* gllight = dynamic_cast<RAS_OpenGLLight*>(lightobject);
	assert(gllight);
	m_lights.push_back(gllight);
}

void RAS_OpenGLRasterizer::RemoveLight(RAS_ILightObject* lightobject)
{
	RAS_OpenGLLight* gllight = dynamic_cast<RAS_OpenGLLight*>(lightobject);
	assert(gllight);

	std::vector<RAS_OpenGLLight*>::iterator lit =
		std::find(m_lights.begin(),m_lights.end(),gllight);

	if (!(lit==m_lights.end()))
		m_lights.erase(lit);
}

bool RAS_OpenGLRasterizer::RayHit(struct KX_ClientObjectInfo *client, KX_RayCast *result, float *oglmatrix)
{
	if (result->m_hitMesh) {

		RAS_Polygon* poly = result->m_hitMesh->GetPolygon(result->m_hitPolygon);
		if (!poly->IsVisible())
			return false;

		MT_Vector3 resultnormal(result->m_hitNormal);
		MT_Vector3 left(oglmatrix[0],oglmatrix[1],oglmatrix[2]);
		MT_Vector3 dir = -(left.cross(resultnormal)).safe_normalized();
		left = (dir.cross(resultnormal)).safe_normalized();
		// for the up vector, we take the 'resultnormal' returned by the physics

		float maat[16] = {left[0],         left[1],         left[2],         0,
			               dir[0],          dir[1],          dir[2],          0,
		                  resultnormal[0], resultnormal[1], resultnormal[2], 0,
		                  0,               0,               0,               1};

		glTranslatef(oglmatrix[12],oglmatrix[13],oglmatrix[14]);
		//glMultMatrixd(oglmatrix);
		glMultMatrixf(maat);
		return true;
	}
	else {
		return false;
	}
}

void RAS_OpenGLRasterizer::applyTransform(float* oglmatrix,int objectdrawmode )
{
	/* FIXME:
	blender: intern/moto/include/MT_Vector3.inl:42: MT_Vector3 operator/(const
	MT_Vector3&, double): Assertion `!MT_fuzzyZero(s)' failed.

	Program received signal SIGABRT, Aborted.
	[Switching to Thread 16384 (LWP 1519)]
	0x40477571 in kill () from /lib/libc.so.6
	(gdb) bt
	#7  0x08334368 in MT_Vector3::normalized() const ()
	#8  0x0833e6ec in RAS_OpenGLRasterizer::applyTransform(RAS_IRasterizer*, double*, int) ()
	*/

	if (objectdrawmode & RAS_IPolyMaterial::BILLBOARD_SCREENALIGNED ||
		objectdrawmode & RAS_IPolyMaterial::BILLBOARD_AXISALIGNED)
	{
		// rotate the billboard/halo
		//page 360/361 3D Game Engine Design, David Eberly for a discussion
		// on screen aligned and axis aligned billboards
		// assumed is that the preprocessor transformed all billboard polygons
		// so that their normal points into the positive x direction (1.0f, 0.0f, 0.0f)
		// when new parenting for objects is done, this rotation
		// will be moved into the object

		MT_Point3 objpos (oglmatrix[12],oglmatrix[13],oglmatrix[14]);
		MT_Point3 campos = GetCameraPosition();
		MT_Vector3 dir = (campos - objpos).safe_normalized();
		MT_Vector3 up(0,0,1.0f);

		KX_GameObject* gameobj = (KX_GameObject*)m_clientobject;
		// get scaling of halo object
		MT_Vector3  size = gameobj->GetSGNode()->GetWorldScaling();

		bool screenaligned = (objectdrawmode & RAS_IPolyMaterial::BILLBOARD_SCREENALIGNED)!=0;//false; //either screen or axisaligned
		if (screenaligned)
		{
			up = (up - up.dot(dir) * dir).safe_normalized();
		} else
		{
			dir = (dir - up.dot(dir)*up).safe_normalized();
		}

		MT_Vector3 left = dir.normalized();
		dir = (up.cross(left)).normalized();

		// we have calculated the row vectors, now we keep
		// local scaling into account:

		left *= size[0];
		dir  *= size[1];
		up   *= size[2];

		float maat[16] = {left[0], left[1], left[2], 0,
		                   dir[0],  dir[1],  dir[2],  0,
		                   up[0],   up[1],   up[2],   0,
		                   0,       0,       0,       1};

		glTranslatef(objpos[0],objpos[1],objpos[2]);
		glMultMatrixf(maat);

	}
	else {
		if (objectdrawmode & RAS_IPolyMaterial::SHADOW)
		{
			// shadow must be cast to the ground, physics system needed here!
			MT_Point3 frompoint(oglmatrix[12],oglmatrix[13],oglmatrix[14]);
			KX_GameObject *gameobj = (KX_GameObject*)m_clientobject;
			MT_Vector3 direction = MT_Vector3(0,0,-1);

			direction.normalize();
			direction *= 100000;

			MT_Point3 topoint = frompoint + direction;

			KX_Scene* kxscene = (KX_Scene*) m_auxilaryClientInfo;
			PHY_IPhysicsEnvironment* physics_environment = kxscene->GetPhysicsEnvironment();
			PHY_IPhysicsController* physics_controller = gameobj->GetPhysicsController();

			KX_GameObject *parent = gameobj->GetParent();
			if (!physics_controller && parent)
				physics_controller = parent->GetPhysicsController();

			KX_RayCast::Callback<RAS_OpenGLRasterizer, float> callback(this, physics_controller, oglmatrix);
			if (!KX_RayCast::RayTest(physics_environment, frompoint, topoint, callback))
			{
				// couldn't find something to cast the shadow on...
				glMultMatrixf(oglmatrix);
			}
			else
			{ // we found the "ground", but the cast matrix doesn't take
			  // scaling in consideration, so we must apply the object scale
				MT_Vector3  size = gameobj->GetSGNode()->GetLocalScale();
				glScalef(size[0], size[1], size[2]);
			}
		} else
		{

			// 'normal' object
			glMultMatrixf(oglmatrix);
		}
	}
}

static void DisableForText()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); /* needed for texture fonts otherwise they render as wireframe */

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);

	if (GLEW_ARB_multitexture) {
		for (int i=0; i<RAS_MAX_TEXCO; i++) {
			glActiveTextureARB(GL_TEXTURE0_ARB+i);

			if (GLEW_ARB_texture_cube_map) {
				glDisable(GL_TEXTURE_CUBE_MAP_ARB);
				glDisable(GL_TEXTURE_GEN_S);
				glDisable(GL_TEXTURE_GEN_T);
				glDisable(GL_TEXTURE_GEN_Q);
				glDisable(GL_TEXTURE_GEN_R);
			}
			glDisable(GL_TEXTURE_2D);
		}

		glActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else {
		if (GLEW_ARB_texture_cube_map)
			glDisable(GL_TEXTURE_CUBE_MAP_ARB);

		glDisable(GL_TEXTURE_2D);
	}
}

void RAS_OpenGLRasterizer::RenderBox2D(int xco,
			int yco,
			int width,
			int height,
			float percentage)
{
	/* This is a rather important line :( The gl-mode hasn't been left
	 * behind quite as neatly as we'd have wanted to. I don't know
	 * what cause it, though :/ .*/
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glOrtho(0, width, 0, height, -100, 100);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	yco = height - yco;
	int barsize = 50;

	/* draw in black first */
	glColor3ub(0, 0, 0);
	glBegin(GL_QUADS);
	glVertex2f(xco + 1 + 1 + barsize * percentage, yco - 1 + 10);
	glVertex2f(xco + 1, yco - 1 + 10);
	glVertex2f(xco + 1, yco - 1);
	glVertex2f(xco + 1 + 1 + barsize * percentage, yco - 1);
	glEnd();

	glColor3ub(255, 255, 255);
	glBegin(GL_QUADS);
	glVertex2f(xco + 1 + barsize * percentage, yco + 10);
	glVertex2f(xco, yco + 10);
	glVertex2f(xco, yco);
	glVertex2f(xco + 1 + barsize * percentage, yco);
	glEnd();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

void RAS_OpenGLRasterizer::RenderText3D(
        int fontid, const char *text, int size, int dpi,
        const float color[4], const float mat[16], float aspect)
{
	/* gl prepping */
	DisableForText();

	/* the actual drawing */
	glColor4fv(color);

	/* multiply the text matrix by the object matrix */
	BLF_enable(fontid, BLF_MATRIX|BLF_ASPECT);
	BLF_matrix(fontid, mat);

	/* aspect is the inverse scale that allows you to increase
	 * your resolution without sizing the final text size
	 * the bigger the size, the smaller the aspect */
	BLF_aspect(fontid, aspect, aspect, aspect);

	BLF_size(fontid, size, dpi);
	BLF_position(fontid, 0, 0, 0);
	BLF_draw(fontid, text, 65535);

	BLF_disable(fontid, BLF_MATRIX|BLF_ASPECT);
}

void RAS_OpenGLRasterizer::RenderText2D(
        RAS_TEXT_RENDER_MODE mode,
        const char* text,
        int xco, int yco,
        int width, int height)
{
	/* This is a rather important line :( The gl-mode hasn't been left
	 * behind quite as neatly as we'd have wanted to. I don't know
	 * what cause it, though :/ .*/
	DisableForText();
	glDisable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glOrtho(0, width, 0, height, -100, 100);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	if (mode == RAS_TEXT_PADDED) {
		/* draw in black first */
		glColor3ub(0, 0, 0);
		BLF_size(blf_mono_font, 11, 72);
		BLF_position(blf_mono_font, (float)xco+1, (float)(height-yco-1), 0.0f);
		BLF_draw(blf_mono_font, text, 65535); /* XXX, use real len */
	}

	/* the actual drawing */
	glColor3ub(255, 255, 255);
	BLF_size(blf_mono_font, 11, 72);
	BLF_position(blf_mono_font, (float)xco, (float)(height-yco), 0.0f);
	BLF_draw(blf_mono_font, text, 65535); /* XXX, use real len */

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

void RAS_OpenGLRasterizer::PushMatrix()
{
	glPushMatrix();
}

void RAS_OpenGLRasterizer::PopMatrix()
{
	glPopMatrix();
}

void RAS_OpenGLRasterizer::MotionBlur()
{
	int state = GetMotionBlurState();
	float motionblurvalue;
	if (state)
	{
		motionblurvalue = GetMotionBlurValue();
		if (state==1)
		{
			// bugfix:load color buffer into accum buffer for the first time(state=1)
			glAccum(GL_LOAD, 1.0f);
			SetMotionBlurState(2);
		}
		else if (motionblurvalue >= 0.0f && motionblurvalue <= 1.0f) {
			glAccum(GL_MULT, motionblurvalue);
			glAccum(GL_ACCUM, 1-motionblurvalue);
			glAccum(GL_RETURN, 1.0f);
			glFlush();
		}
	}
}

void RAS_OpenGLRasterizer::SetClientObject(void* obj)
{
	if (m_clientobject != obj)
	{
		bool ccw = (obj == NULL || !((KX_GameObject*)obj)->IsNegativeScaling());
		SetFrontFace(ccw);

		m_clientobject = obj;
	}
}

void RAS_OpenGLRasterizer::SetAuxilaryClientInfo(void* inf)
{
	m_auxilaryClientInfo = inf;
}

void RAS_OpenGLRasterizer::PrintHardwareInfo()
{
	#define pprint(x) std::cout << x << std::endl;

	pprint("GL_VENDOR: " << glGetString(GL_VENDOR));
	pprint("GL_RENDERER: " << glGetString(GL_RENDERER));
	pprint("GL_VERSION:  " << glGetString(GL_VERSION));
	bool support=0;
	pprint("Supported Extensions...");
	pprint(" GL_ARB_shader_objects supported?       "<< (GLEW_ARB_shader_objects?"yes.":"no."));

	support= GLEW_ARB_vertex_shader;
	pprint(" GL_ARB_vertex_shader supported?        "<< (support?"yes.":"no."));
	if (support) {
		pprint(" ----------Details----------");
		int max=0;
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, (GLint*)&max);
		pprint("  Max uniform components." << max);

		glGetIntegerv(GL_MAX_VARYING_FLOATS_ARB, (GLint*)&max);
		pprint("  Max varying floats." << max);

		glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB, (GLint*)&max);
		pprint("  Max vertex texture units." << max);

		glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB, (GLint*)&max);
		pprint("  Max combined texture units." << max);
		pprint("");
	}

	support=GLEW_ARB_fragment_shader;
	pprint(" GL_ARB_fragment_shader supported?      "<< (support?"yes.":"no."));
	if (support) {
		pprint(" ----------Details----------");
		int max=0;
		glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB, (GLint*)&max);
		pprint("  Max uniform components." << max);
		pprint("");
	}

	support = GLEW_ARB_texture_cube_map;
	pprint(" GL_ARB_texture_cube_map supported?     "<< (support?"yes.":"no."));
	if (support) {
		pprint(" ----------Details----------");
		int size=0;
		glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB, (GLint*)&size);
		pprint("  Max cubemap size." << size);
		pprint("");
	}

	support = GLEW_ARB_multitexture;
	pprint(" GL_ARB_multitexture supported?         "<< (support?"yes.":"no."));
	if (support) {
		pprint(" ----------Details----------");
		int units=0;
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint*)&units);
		pprint("  Max texture units available.  " << units);
		pprint("");
	}

	pprint(" GL_ARB_texture_env_combine supported?  "<< (GLEW_ARB_texture_env_combine?"yes.":"no."));

	pprint(" GL_ARB_texture_non_power_of_two supported  " << (GPU_full_non_power_of_two_support()?"yes.":"no."));
}

