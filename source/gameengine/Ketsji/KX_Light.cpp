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

#ifdef WIN32

#pragma warning (disable : 4786)
#endif

#include "KX_Light.h"
#include "KX_Camera.h"
#include "RAS_IRasterizer.h"
#include "RAS_IRenderTools.h"

#include "KX_PyMath.h"

#ifdef BLENDER_GLSL
#include "GPU_material.h"
#endif
 
KX_LightObject::KX_LightObject(void* sgReplicationInfo,SG_Callbacks callbacks,
							   class RAS_IRenderTools* rendertools,
							   const RAS_LightObject&	lightobj,
							   struct GPULamp *gpulamp,
							   PyTypeObject* T
							   )
 :
	KX_GameObject(sgReplicationInfo,callbacks,T),
		m_rendertools(rendertools)
{
	m_lightobj = lightobj;
	m_lightobj.m_worldmatrix = GetOpenGLMatrixPtr();
	m_rendertools->AddLight(&m_lightobj);
	m_gpulamp = gpulamp;
};


KX_LightObject::~KX_LightObject()
{
	m_rendertools->RemoveLight(&m_lightobj);
}


CValue*		KX_LightObject::GetReplica()
{

	KX_LightObject* replica = new KX_LightObject(*this);
	
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	ProcessReplica(replica);
	
	replica->m_lightobj.m_worldmatrix = replica->GetOpenGLMatrixPtr();
	m_rendertools->AddLight(&replica->m_lightobj);
	return replica;
}

void KX_LightObject::Update()
{
#ifdef BLENDER_GLSL
	if(m_gpulamp) {
		float obmat[4][4];
		double *dobmat = GetOpenGLMatrixPtr()->getPointer();

		for(int i=0; i<4; i++)
			for(int j=0; j<4; j++, dobmat++)
				obmat[i][j] = (float)*dobmat;

		GPU_lamp_update(m_gpulamp, obmat);
	}
#endif
}

bool KX_LightObject::HasShadowBuffer()
{
#ifdef BLENDER_GLSL
	return (m_gpulamp && GPU_lamp_has_shadow_buffer(m_gpulamp));
#else
	return false;
#endif
}

int KX_LightObject::GetShadowLayer()
{
#ifdef BLENDER_GLSL
	if(m_gpulamp)
		return GPU_lamp_shadow_layer(m_gpulamp);
	else
#endif
		return 0;
}

void KX_LightObject::BindShadowBuffer(RAS_IRasterizer *ras, KX_Camera *cam, MT_Transform& camtrans)
{
#ifdef BLENDER_GLSL
	float viewmat[4][4], winmat[4][4];
	int winsize;

	/* bind framebuffer */
	GPU_lamp_shadow_buffer_bind(m_gpulamp, viewmat, &winsize, winmat);

	/* setup camera transformation */
	MT_Matrix4x4 modelviewmat((float*)viewmat);
	MT_Matrix4x4 projectionmat((float*)winmat);

	MT_Transform trans = MT_Transform((float*)viewmat);
	camtrans.invert(trans);

	cam->SetModelviewMatrix(modelviewmat);
	cam->SetProjectionMatrix(projectionmat);
	
	cam->NodeSetLocalPosition(camtrans.getOrigin());
	cam->NodeSetLocalOrientation(camtrans.getBasis());
	cam->NodeUpdateGS(0,true);

	/* setup rasterizer transformations */
	ras->SetProjectionMatrix(projectionmat);
	ras->SetViewMatrix(modelviewmat, cam->NodeGetWorldPosition(),
		cam->GetCameraLocation(), cam->GetCameraOrientation());
#endif
}

void KX_LightObject::UnbindShadowBuffer(RAS_IRasterizer *ras)
{
#ifdef BLENDER_GLSL
	GPU_lamp_shadow_buffer_unbind(m_gpulamp);
#endif
}

PyObject* KX_LightObject::_getattr(const STR_String& attr)
{
	if (attr == "layer")
		return PyInt_FromLong(m_lightobj.m_layer);
	
	if (attr == "energy")
		return PyFloat_FromDouble(m_lightobj.m_energy);
	
	if (attr == "distance")
		return PyFloat_FromDouble(m_lightobj.m_distance);
	
	if (attr == "colour" || attr == "color")
		return Py_BuildValue("[fff]", m_lightobj.m_red, m_lightobj.m_green, m_lightobj.m_blue);
		
	if (attr == "lin_attenuation")
		return PyFloat_FromDouble(m_lightobj.m_att1);
	
	if (attr == "quad_attenuation")
		return PyFloat_FromDouble(m_lightobj.m_att2);
	
	if (attr == "spotsize")
		return PyFloat_FromDouble(m_lightobj.m_spotsize);
	
	if (attr == "spotblend")
		return PyFloat_FromDouble(m_lightobj.m_spotblend);
		
	if (attr == "SPOT")
		return PyInt_FromLong(RAS_LightObject::LIGHT_SPOT);
		
	if (attr == "SUN")
		return PyInt_FromLong(RAS_LightObject::LIGHT_SUN);
	
	if (attr == "NORMAL")
		return PyInt_FromLong(RAS_LightObject::LIGHT_NORMAL);
	
	if (attr == "type")
		return PyInt_FromLong(m_lightobj.m_type);
		
	_getattr_up(KX_GameObject);
}

int       KX_LightObject::_setattr(const STR_String& attr, PyObject *pyvalue)
{
	if (attr == "SPOT" || attr == "SUN" || attr == "NORMAL")
	{
		PyErr_Format(PyExc_RuntimeError, "Attribute %s is read only.", attr.ReadPtr());
		return 1;
	}
	
	if (PyInt_Check(pyvalue))
	{
		int value = PyInt_AsLong(pyvalue);
		if (attr == "layer")
		{
			m_lightobj.m_layer = value;
			return 0;
		}
		
		if (attr == "type")
		{
			if (value >= RAS_LightObject::LIGHT_SPOT && value <= RAS_LightObject::LIGHT_NORMAL)
				m_lightobj.m_type = (RAS_LightObject::LightType) value;
			return 0;
		}
	}
	
	if (PyFloat_Check(pyvalue))
	{
		float value = PyFloat_AsDouble(pyvalue);
		if (attr == "energy")
		{
			m_lightobj.m_energy = value;
			return 0;
		}
	
		if (attr == "distance")
		{
			m_lightobj.m_distance = value;
			return 0;
		}
		
		if (attr == "lin_attenuation")
		{
			m_lightobj.m_att1 = value;
			return 0;
		}
		
		if (attr == "quad_attenuation")
		{
			m_lightobj.m_att2 = value;
			return 0;
		}
		
		if (attr == "spotsize")
		{
			m_lightobj.m_spotsize = value;
			return 0;
		}
		
		if (attr == "spotblend")
		{
			m_lightobj.m_spotblend = value;
			return 0;
		}
	}

	if (PySequence_Check(pyvalue))
	{
		if (attr == "colour" || attr == "color")
		{
			MT_Vector3 color;
			if (PyVecTo(pyvalue, color))
			{
				m_lightobj.m_red = color[0];
				m_lightobj.m_green = color[1];
				m_lightobj.m_blue = color[2];
				return 0;
			}
			return 1;
		}
	}
	
	return KX_GameObject::_setattr(attr, pyvalue);
}

PyMethodDef KX_LightObject::Methods[] = {
	{NULL,NULL} //Sentinel
};

char KX_LightObject::doc[] = "Module KX_LightObject\n\n"
"Constants:\n"
"\tSPOT\n"
"\tSUN\n"
"\tNORMAL\n"
"Attributes:\n"
"\ttype -> SPOT, SUN or NORMAL\n"
"\t\tThe type of light.\n"
"\tlayer -> integer bit field.\n"
"\t\tThe layers this light applies to.\n"
"\tenergy -> float.\n"
"\t\tThe brightness of the light.\n"
"\tdistance -> float.\n"
"\t\tThe effect radius of the light.\n"
"\tcolour -> list [r, g, b].\n"
"\tcolor  -> list [r, g, b].\n"
"\t\tThe color of the light.\n"
"\tlin_attenuation -> float.\n"
"\t\tThe attenuation factor for the light.\n"
"\tspotsize -> float.\n"
"\t\tThe size of the spot.\n"
"\tspotblend -> float.\n"
"\t\tThe blend? of the spot.\n";

PyTypeObject KX_LightObject::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_LightObject",
		sizeof(KX_LightObject),
		0,
		PyDestructor,
		0,
		__getattr,
		__setattr,
		0, //&MyPyCompare,
		__repr,
		0, //&cvalue_as_number,
		0,
		0,
		0,
		0, 0, 0, 0, 0, 0,
		doc
};

PyParentObject KX_LightObject::Parents[] = {
	&KX_LightObject::Type,
	&KX_GameObject::Type,
		&SCA_IObject::Type,
		&CValue::Type,
		NULL
};
