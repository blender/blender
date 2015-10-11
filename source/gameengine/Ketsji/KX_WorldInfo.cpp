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

/** \file gameengine/Ketsji/KX_WorldInfo.cpp
 *  \ingroup ketsji
 */


#include "KX_WorldInfo.h"
#include "KX_PythonInit.h"
#include "KX_PyMath.h"
#include "RAS_IRasterizer.h"
#include "GPU_material.h"

/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

/* This list includes only data type definitions */
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BLI_math.h"

#include "BKE_global.h"
#include "BKE_scene.h"
/* end of blender include block */


KX_WorldInfo::KX_WorldInfo(Scene *blenderscene, World *blenderworld)
{
	if (blenderworld) {
		m_name = blenderworld->id.name + 2;
		m_do_color_management = BKE_scene_check_color_management_enabled(blenderscene);
		m_hasworld = true;
		m_hasmist = ((blenderworld->mode) & WO_MIST ? true : false);
		m_misttype = blenderworld->mistype;
		m_miststart = blenderworld->miststa;
		m_mistdistance = blenderworld->mistdist;
		m_mistintensity = blenderworld->misi;
		setMistColor(blenderworld->horr, blenderworld->horg, blenderworld->horb);
		setBackColor(blenderworld->horr, blenderworld->horg, blenderworld->horb);
		setAmbientColor(blenderworld->ambr, blenderworld->ambg, blenderworld->ambb);
	}
	else {
		m_hasworld = false;
	}
}

KX_WorldInfo::~KX_WorldInfo()
{
}

const STR_String& KX_WorldInfo::GetName()
{
	return m_name;
}

bool KX_WorldInfo::hasWorld()
{
	return m_hasworld;
}

void KX_WorldInfo::setBackColor(float r, float g, float b)
{
	m_backgroundcolor[0] = r;
	m_backgroundcolor[1] = g;
	m_backgroundcolor[2] = b;

	if (m_do_color_management) {
		linearrgb_to_srgb_v3_v3(m_con_backgroundcolor, m_backgroundcolor);
	}
	else {
		copy_v3_v3(m_con_backgroundcolor, m_backgroundcolor);
	}
}

const float *KX_WorldInfo::getBackColorConverted() const
{
	return m_con_backgroundcolor;
}

void KX_WorldInfo::setMistType(short type)
{
	m_misttype = type;
}

void KX_WorldInfo::setUseMist(bool enable)
{
	m_hasmist = enable;
}

void KX_WorldInfo::setMistStart(float d)
{
	m_miststart = d;
}

void KX_WorldInfo::setMistDistance(float d)
{
	m_mistdistance = d;
}

void KX_WorldInfo::setMistIntensity(float intensity)
{
	m_mistintensity = intensity;
}
void KX_WorldInfo::setMistColor(float r, float g, float b)
{
	m_mistcolor[0] = r;
	m_mistcolor[1] = g;
	m_mistcolor[2] = b;

	if (m_do_color_management) {
		linearrgb_to_srgb_v3_v3(m_con_mistcolor, m_mistcolor);
	}
	else {
		copy_v3_v3(m_con_mistcolor, m_mistcolor);
	}
}

void KX_WorldInfo::setAmbientColor(float r, float g, float b)
{
	m_ambientcolor[0] = r;
	m_ambientcolor[1] = g;
	m_ambientcolor[2] = b;

	if (m_do_color_management) {
		linearrgb_to_srgb_v3_v3(m_con_ambientcolor, m_ambientcolor);
	}
	else {
		copy_v3_v3(m_con_ambientcolor, m_ambientcolor);
	}
}

void KX_WorldInfo::UpdateBackGround()
{
	if (m_hasworld) {
		RAS_IRasterizer *m_rasterizer = KX_GetActiveEngine()->GetRasterizer();

		if (m_rasterizer->GetDrawingMode() >= RAS_IRasterizer::KX_SOLID) {
			m_rasterizer->SetBackColor(m_con_backgroundcolor);
			GPU_horizon_update_color(m_backgroundcolor);
		}
	}
}

void KX_WorldInfo::UpdateWorldSettings()
{
	if (m_hasworld) {
		RAS_IRasterizer *m_rasterizer = KX_GetActiveEngine()->GetRasterizer();

		if (m_rasterizer->GetDrawingMode() >= RAS_IRasterizer::KX_SOLID) {
			m_rasterizer->SetAmbientColor(m_con_ambientcolor);
			GPU_ambient_update_color(m_ambientcolor);

			if (m_hasmist) {
				m_rasterizer->SetFog(m_misttype, m_miststart, m_mistdistance, m_mistintensity, m_con_mistcolor);
				GPU_mist_update_values(m_misttype, m_miststart, m_mistdistance, m_mistintensity, m_mistcolor);
				m_rasterizer->EnableFog(true);
				GPU_mist_update_enable(true);
			}
			else {
				m_rasterizer->EnableFog(false);
				GPU_mist_update_enable(false);
			}
		}
	}
}

#ifdef WITH_PYTHON

/* -------------------------------------------------------------------------
 * Python functions
 * ------------------------------------------------------------------------- */
PyObject *KX_WorldInfo::py_repr(void)
{
	return PyUnicode_From_STR_String(GetName());
}

/* -------------------------------------------------------------------------
 * Python Integration Hooks
 * ------------------------------------------------------------------------- */
PyTypeObject KX_WorldInfo::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_WorldInfo",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_WorldInfo::Methods[] = {
	{NULL,NULL} /* Sentinel */
};

PyAttributeDef KX_WorldInfo::Attributes[] = {
	KX_PYATTRIBUTE_BOOL_RW("mistEnable", KX_WorldInfo, m_hasmist),
	KX_PYATTRIBUTE_FLOAT_RW("mistStart", 0.0f, 10000.0f, KX_WorldInfo, m_miststart),
	KX_PYATTRIBUTE_FLOAT_RW("mistDistance", 0.001f, 10000.0f, KX_WorldInfo, m_mistdistance),
	KX_PYATTRIBUTE_FLOAT_RW("mistIntensity", 0.0f, 1.0f, KX_WorldInfo, m_mistintensity),
	KX_PYATTRIBUTE_SHORT_RW("mistType", 0, 2, true, KX_WorldInfo, m_misttype),
	KX_PYATTRIBUTE_RO_FUNCTION("KX_MIST_QUADRATIC", KX_WorldInfo, pyattr_get_mist_typeconst),
	KX_PYATTRIBUTE_RO_FUNCTION("KX_MIST_LINEAR", KX_WorldInfo, pyattr_get_mist_typeconst),
	KX_PYATTRIBUTE_RO_FUNCTION("KX_MIST_INV_QUADRATIC", KX_WorldInfo, pyattr_get_mist_typeconst),
	KX_PYATTRIBUTE_RW_FUNCTION("mistColor", KX_WorldInfo, pyattr_get_mist_color, pyattr_set_mist_color),
	KX_PYATTRIBUTE_RW_FUNCTION("backgroundColor", KX_WorldInfo, pyattr_get_back_color, pyattr_set_back_color),
	KX_PYATTRIBUTE_RW_FUNCTION("ambientColor", KX_WorldInfo, pyattr_get_ambient_color, pyattr_set_ambient_color),
	{ NULL } /* Sentinel */
};

/* Attribute get/set functions */

#ifdef USE_MATHUTILS

/*----------------------mathutils callbacks ----------------------------*/

/* subtype */
#define MATHUTILS_COL_CB_MIST_COLOR 1
#define MATHUTILS_COL_CB_BACK_COLOR 2
#define MATHUTILS_COL_CB_AMBIENT_COLOR 3

static unsigned char mathutils_world_color_cb_index = -1; /* index for our callbacks */

static int mathutils_world_generic_check(BaseMathObject *bmo)
{
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>BGE_PROXY_REF(bmo->cb_user);
	if (self == NULL)
		return -1;
		
	return 0;
}

static int mathutils_world_color_get(BaseMathObject *bmo, int subtype)
{
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>BGE_PROXY_REF(bmo->cb_user);
	if (self == NULL)
		return -1;

	switch (subtype) {
		case MATHUTILS_COL_CB_MIST_COLOR:
			copy_v3_v3(bmo->data, self->m_mistcolor);
			break;
		case MATHUTILS_COL_CB_BACK_COLOR:
			copy_v3_v3(bmo->data, self->m_backgroundcolor);
			break;
		case MATHUTILS_COL_CB_AMBIENT_COLOR:
			copy_v3_v3(bmo->data, self->m_ambientcolor);
			break;
	default:
		return -1;
	}
	return 0;
}

static int mathutils_world_color_set(BaseMathObject *bmo, int subtype)
{
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>BGE_PROXY_REF(bmo->cb_user);

	if (self == NULL)
		return -1;

	switch (subtype) {
		case MATHUTILS_COL_CB_MIST_COLOR:
			self->setMistColor(bmo->data[0], bmo->data[1], bmo->data[2]);
			break;
		case MATHUTILS_COL_CB_BACK_COLOR:
			self->setBackColor(bmo->data[0], bmo->data[1], bmo->data[2]);
			break;
		case MATHUTILS_COL_CB_AMBIENT_COLOR:
			self->setAmbientColor(bmo->data[0], bmo->data[1], bmo->data[2]);
			break;
	default:
		return -1;
	}
	return 0;
}

static int mathutils_world_color_get_index(BaseMathObject *bmo, int subtype, int index)
{
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>BGE_PROXY_REF(bmo->cb_user);

	if (self == NULL)
		return -1;

	switch (subtype) {
		case MATHUTILS_COL_CB_MIST_COLOR:
		{
			const float *color = self->m_mistcolor;
			bmo->data[index] = color[index];
		}
		break;
		case MATHUTILS_COL_CB_BACK_COLOR:
		{
			const float *color = self->m_backgroundcolor;
			bmo->data[index] = color[index];
		}
			break;
		case MATHUTILS_COL_CB_AMBIENT_COLOR:
		{
			const float *color = self->m_ambientcolor;
			bmo->data[index] = color[index];
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static int mathutils_world_color_set_index(BaseMathObject *bmo, int subtype, int index)
{
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>BGE_PROXY_REF(bmo->cb_user);

	if (self == NULL)
		return -1;

	float color[4];
	switch (subtype) {
		case MATHUTILS_COL_CB_MIST_COLOR:
			copy_v3_v3(color, self->m_mistcolor);
			color[index] = bmo->data[index];
			self->setMistColor(color[0], color[1], color[2]);
		break;
		case MATHUTILS_COL_CB_BACK_COLOR:
			copy_v3_v3(color, self->m_backgroundcolor);
			color[index] = bmo->data[index];
			self->setBackColor(color[0], color[1], color[2]);
		break;
		case MATHUTILS_COL_CB_AMBIENT_COLOR:
			copy_v3_v3(color, self->m_ambientcolor);
			color[index] = bmo->data[index];
			self->setAmbientColor(color[0], color[1], color[2]);
			break;
	default:
		return -1;
	}
	return 0;
}

static Mathutils_Callback mathutils_world_color_cb = {
	mathutils_world_generic_check,
	mathutils_world_color_get,
	mathutils_world_color_set,
	mathutils_world_color_get_index,
	mathutils_world_color_set_index
};

void KX_WorldInfo_Mathutils_Callback_Init()
{
	// register mathutils callbacks, ok to run more than once.
	mathutils_world_color_cb_index = Mathutils_RegisterCallback(&mathutils_world_color_cb);
}
#endif // USE_MATHUTILS


PyObject *KX_WorldInfo::pyattr_get_mist_typeconst(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	PyObject *retvalue;

	const char* type = attrdef->m_name;

	if (!strcmp(type, "KX_MIST_QUADRATIC")) {
		retvalue = PyLong_FromLong(KX_MIST_QUADRATIC);
	} 
	else if (!strcmp(type, "KX_MIST_LINEAR")) {
		retvalue = PyLong_FromLong(KX_MIST_LINEAR);
	} 
	else if (!strcmp(type, "KX_MIST_INV_QUADRATIC")) {
		retvalue = PyLong_FromLong(KX_MIST_INV_QUADRATIC);
	}
	else {
		/* should never happen */
		PyErr_SetString(PyExc_TypeError, "invalid mist type");
		retvalue = NULL;
	}

	return retvalue;
}

PyObject *KX_WorldInfo::pyattr_get_mist_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Color_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v),
	        mathutils_world_color_cb_index, MATHUTILS_COL_CB_MIST_COLOR);
#else
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>(self_v);
	return PyObjectFrom(MT_Vector3(self->m_mistcolor));
#endif
}

int KX_WorldInfo::pyattr_set_mist_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>(self_v);

	MT_Vector3 color;
	if (PyVecTo(value, color))
	{
		self->setMistColor(color[0], color[1], color[2]);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_WorldInfo::pyattr_get_back_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{

#ifdef USE_MATHUTILS
	return Color_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v),
	        mathutils_world_color_cb_index, MATHUTILS_COL_CB_BACK_COLOR);
#else
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>(self_v);
	return PyObjectFrom(MT_Vector3(self->m_backgroundcolor));
#endif
}

int KX_WorldInfo::pyattr_set_back_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>(self_v);

	MT_Vector3 color;
	if (PyVecTo(value, color))
	{
		self->setBackColor(color[0], color[1], color[2]);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

PyObject *KX_WorldInfo::pyattr_get_ambient_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Color_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v),
	        mathutils_world_color_cb_index, MATHUTILS_COL_CB_AMBIENT_COLOR);
#else
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>(self_v);
	return PyObjectFrom(MT_Vector3(self->m_ambientcolor));
#endif
}

int KX_WorldInfo::pyattr_set_ambient_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_WorldInfo *self = static_cast<KX_WorldInfo*>(self_v);

	MT_Vector3 color;
	if (PyVecTo(value, color))
	{
		self->setAmbientColor(color[0], color[1], color[2]);
		return PY_SET_ATTR_SUCCESS;
	}
	return PY_SET_ATTR_FAIL;
}

#endif /* WITH_PYTHON */
