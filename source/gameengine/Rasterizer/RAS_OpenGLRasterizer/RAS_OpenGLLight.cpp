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
 * Contributor(s): Mitchell Stokes
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GL/glew.h"

#include "RAS_OpenGLLight.h"
#include "RAS_OpenGLRasterizer.h"
#include "RAS_ICanvas.h"

#include "MT_CmMatrix4x4.h"

#include "KX_Camera.h"
#include "KX_Light.h"
#include "KX_Scene.h"

#include "DNA_lamp_types.h"
#include "DNA_scene_types.h"

#include "GPU_material.h"

RAS_OpenGLLight::RAS_OpenGLLight(RAS_OpenGLRasterizer *ras)
    :m_rasterizer(ras)
{
}

RAS_OpenGLLight::~RAS_OpenGLLight()
{
	GPULamp *lamp;
	KX_LightObject* kxlight = (KX_LightObject*)m_light;
	Lamp *la = (Lamp*)kxlight->GetBlenderObject()->data;

	if ((lamp = GetGPULamp())) {
		float obmat[4][4] = {{0}};
		GPU_lamp_update(lamp, 0, 0, obmat);
		GPU_lamp_update_distance(lamp, la->dist, la->att1, la->att2);
		GPU_lamp_update_spot(lamp, la->spotsize, la->spotblend);
	}
}

bool RAS_OpenGLLight::ApplyFixedFunctionLighting(KX_Scene *kxscene, int oblayer, int slot)
{
	KX_Scene* lightscene = (KX_Scene*)m_scene;
	KX_LightObject* kxlight = (KX_LightObject*)m_light;
	float vec[4];
	int scenelayer = ~0;

	if (kxscene && kxscene->GetBlenderScene())
		scenelayer = kxscene->GetBlenderScene()->lay;

	/* only use lights in the same layer as the object */
	if (!(m_layer & oblayer))
		return false;
	/* only use lights in the same scene, and in a visible layer */
	if (kxscene != lightscene || !(m_layer & scenelayer))
		return false;

	// lights don't get their openGL matrix updated, do it now
	if (kxlight->GetSGNode()->IsDirty())
		kxlight->GetOpenGLMatrix();

	MT_CmMatrix4x4& worldmatrix= *kxlight->GetOpenGLMatrixPtr();

	vec[0] = worldmatrix(0,3);
	vec[1] = worldmatrix(1,3);
	vec[2] = worldmatrix(2,3);
	vec[3] = 1.0f;

	if (m_type==RAS_ILightObject::LIGHT_SUN) {

		vec[0] = worldmatrix(0,2);
		vec[1] = worldmatrix(1,2);
		vec[2] = worldmatrix(2,2);
		//vec[0] = base->object->obmat[2][0];
		//vec[1] = base->object->obmat[2][1];
		//vec[2] = base->object->obmat[2][2];
		vec[3] = 0.0;
		glLightfv((GLenum)(GL_LIGHT0+slot), GL_POSITION, vec);
	}
	else {
		//vec[3] = 1.0;
		glLightfv((GLenum)(GL_LIGHT0+slot), GL_POSITION, vec);
		glLightf((GLenum)(GL_LIGHT0+slot), GL_CONSTANT_ATTENUATION, 1.0);
		glLightf((GLenum)(GL_LIGHT0+slot), GL_LINEAR_ATTENUATION, m_att1/m_distance);
		// without this next line it looks backward compatible.
		//attennuation still is acceptable
		glLightf((GLenum)(GL_LIGHT0+slot), GL_QUADRATIC_ATTENUATION, m_att2/(m_distance*m_distance));

		if (m_type==RAS_ILightObject::LIGHT_SPOT) {
			vec[0] = -worldmatrix(0,2);
			vec[1] = -worldmatrix(1,2);
			vec[2] = -worldmatrix(2,2);
			//vec[0] = -base->object->obmat[2][0];
			//vec[1] = -base->object->obmat[2][1];
			//vec[2] = -base->object->obmat[2][2];
			glLightfv((GLenum)(GL_LIGHT0+slot), GL_SPOT_DIRECTION, vec);
			glLightf((GLenum)(GL_LIGHT0+slot), GL_SPOT_CUTOFF, m_spotsize / 2.0f);
			glLightf((GLenum)(GL_LIGHT0+slot), GL_SPOT_EXPONENT, 128.0f * m_spotblend);
		}
		else {
			glLightf((GLenum)(GL_LIGHT0+slot), GL_SPOT_CUTOFF, 180.0);
		}
	}

	if (m_nodiffuse) {
		vec[0] = vec[1] = vec[2] = vec[3] = 0.0;
	}
	else {
		vec[0] = m_energy*m_color[0];
		vec[1] = m_energy*m_color[1];
		vec[2] = m_energy*m_color[2];
		vec[3] = 1.0;
	}

	glLightfv((GLenum)(GL_LIGHT0+slot), GL_DIFFUSE, vec);
	if (m_nospecular)
	{
		vec[0] = vec[1] = vec[2] = vec[3] = 0.0;
	}
	else if (m_nodiffuse) {
		vec[0] = m_energy*m_color[0];
		vec[1] = m_energy*m_color[1];
		vec[2] = m_energy*m_color[2];
		vec[3] = 1.0;
	}

	glLightfv((GLenum)(GL_LIGHT0+slot), GL_SPECULAR, vec);
	glEnable((GLenum)(GL_LIGHT0+slot));

	return true;
}

GPULamp *RAS_OpenGLLight::GetGPULamp()
{
	KX_LightObject* kxlight = (KX_LightObject*)m_light;

	if (m_glsl)
		return GPU_lamp_from_blender(kxlight->GetScene()->GetBlenderScene(), kxlight->GetBlenderObject(), kxlight->GetBlenderGroupObject());
	else
		return NULL;
}


bool RAS_OpenGLLight::HasShadowBuffer()
{
	GPULamp *lamp;

	if ((lamp = GetGPULamp()))
		return GPU_lamp_has_shadow_buffer(lamp);
	else
		return false;
}

int RAS_OpenGLLight::GetShadowLayer()
{
	GPULamp *lamp;

	if ((lamp = GetGPULamp()))
		return GPU_lamp_shadow_layer(lamp);
	else
		return 0;
}

void RAS_OpenGLLight::BindShadowBuffer(RAS_ICanvas *canvas, KX_Camera *cam, MT_Transform& camtrans)
{
	GPULamp *lamp;
	float viewmat[4][4], winmat[4][4];
	int winsize;

	/* bind framebuffer */
	lamp = GetGPULamp();
	GPU_lamp_shadow_buffer_bind(lamp, viewmat, &winsize, winmat);

	if (GPU_lamp_shadow_buffer_type(lamp) == LA_SHADMAP_VARIANCE)
		m_rasterizer->SetUsingOverrideShader(true);

	/* GPU_lamp_shadow_buffer_bind() changes the viewport, so update the canvas */
	canvas->UpdateViewPort(0, 0, winsize, winsize);

	/* setup camera transformation */
	MT_Matrix4x4 modelviewmat((float*)viewmat);
	MT_Matrix4x4 projectionmat((float*)winmat);

	MT_Transform trans = MT_Transform((float*)viewmat);
	camtrans.invert(trans);

	cam->SetModelviewMatrix(modelviewmat);
	cam->SetProjectionMatrix(projectionmat);

	cam->NodeSetLocalPosition(camtrans.getOrigin());
	cam->NodeSetLocalOrientation(camtrans.getBasis());
	cam->NodeUpdateGS(0);

	/* setup rasterizer transformations */
	/* SetViewMatrix may use stereomode which we temporarily disable here */
	RAS_IRasterizer::StereoMode stereomode = m_rasterizer->GetStereoMode();
	m_rasterizer->SetStereoMode(RAS_IRasterizer::RAS_STEREO_NOSTEREO);
	m_rasterizer->SetProjectionMatrix(projectionmat);
	m_rasterizer->SetViewMatrix(modelviewmat, cam->NodeGetWorldOrientation(), cam->NodeGetWorldPosition(), cam->GetCameraData()->m_perspective);
	m_rasterizer->SetStereoMode(stereomode);
}

void RAS_OpenGLLight::UnbindShadowBuffer()
{
	GPULamp *lamp = GetGPULamp();
	GPU_lamp_shadow_buffer_unbind(lamp);

	if (GPU_lamp_shadow_buffer_type(lamp) == LA_SHADMAP_VARIANCE)
		m_rasterizer->SetUsingOverrideShader(false);
}

Image *RAS_OpenGLLight::GetTextureImage(short texslot)
{
	KX_LightObject* kxlight = (KX_LightObject*)m_light;
	Lamp *la = (Lamp*)kxlight->GetBlenderObject()->data;

	if (texslot >= MAX_MTEX || texslot < 0)
	{
		printf("KX_LightObject::GetTextureImage(): texslot exceeds slot bounds (0-%d)\n", MAX_MTEX-1);
		return NULL;
	}

	if (la->mtex[texslot])
		return la->mtex[texslot]->tex->ima;

	return NULL;
}

void RAS_OpenGLLight::Update()
{
	GPULamp *lamp;
	KX_LightObject* kxlight = (KX_LightObject*)m_light;

	if ((lamp = GetGPULamp()) != NULL && kxlight->GetSGNode()) {
		float obmat[4][4];
		// lights don't get their openGL matrix updated, do it now
		if (kxlight->GetSGNode()->IsDirty())
			kxlight->GetOpenGLMatrix();
		double *dobmat = kxlight->GetOpenGLMatrixPtr()->getPointer();

		for (int i=0; i<4; i++)
			for (int j=0; j<4; j++, dobmat++)
				obmat[i][j] = (float)*dobmat;

		GPU_lamp_update(lamp, m_layer, 0, obmat);
		GPU_lamp_update_colors(lamp, m_color[0], m_color[1],
			m_color[2], m_energy);
		GPU_lamp_update_distance(lamp, m_distance, m_att1, m_att2);
		GPU_lamp_update_spot(lamp, m_spotsize, m_spotblend);
	}
}


