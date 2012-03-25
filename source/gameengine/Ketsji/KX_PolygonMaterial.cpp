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

/** \file gameengine/Ketsji/KX_PolygonMaterial.cpp
 *  \ingroup ketsji
 */


#include <stddef.h>

#include "KX_PolygonMaterial.h"

#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_image.h"

#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"

#include "IMB_imbuf_types.h"

#include "GPU_draw.h"

#include "MEM_guardedalloc.h"

#include "RAS_LightObject.h"
#include "RAS_MaterialBucket.h"

#include "KX_PyMath.h"

#define KX_POLYGONMATERIAL_CAPSULE_ID "KX_POLYGONMATERIAL_PTR"

KX_PolygonMaterial::KX_PolygonMaterial()
		: PyObjectPlus(),
		  RAS_IPolyMaterial(),

	m_tface(NULL),
	m_mcol(NULL),
	m_material(NULL),
#ifdef WITH_PYTHON
	m_pymaterial(NULL),
#endif
	m_pass(0)
{
}

void KX_PolygonMaterial::Initialize(
		const STR_String &texname,
		Material* ma,
		int materialindex,
		int tile,
		int tilexrep,
		int tileyrep,
		int alphablend,
		bool alpha,
		bool zsort,
		bool light,
		int lightlayer,
		struct MTFace* tface,
		unsigned int* mcol)
{
	RAS_IPolyMaterial::Initialize(
							texname,
							ma?ma->id.name:"",
							materialindex,
							tile,
							tilexrep,
							tileyrep,
							alphablend,
							alpha,
							zsort,
							light,
							(texname && texname != ""?true:false), /* if we have a texture we have image */
							ma?&ma->game:NULL);
	m_tface = tface;
	m_mcol = mcol;
	m_material = ma;
#ifdef WITH_PYTHON
	m_pymaterial = 0;
#endif
	m_pass = 0;
}

KX_PolygonMaterial::~KX_PolygonMaterial()
{
#ifdef WITH_PYTHON
	if (m_pymaterial)
	{
		Py_DECREF(m_pymaterial);
	}
#endif // WITH_PYTHON
}

Image *KX_PolygonMaterial::GetBlenderImage() const
{
	return (m_tface) ? m_tface->tpage : NULL;
}

bool KX_PolygonMaterial::Activate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const 
{
	bool dopass = false;

#ifdef WITH_PYTHON
	if (m_pymaterial)
	{
		PyObject *pyRasty = PyCapsule_New((void*)rasty, KX_POLYGONMATERIAL_CAPSULE_ID, NULL);	/* new reference */
		PyObject *pyCachingInfo = PyCapsule_New((void*) &cachingInfo, KX_POLYGONMATERIAL_CAPSULE_ID, NULL); /* new reference */
		PyObject *ret = PyObject_CallMethod(m_pymaterial, (char *)"activate", (char *)"(NNO)", pyRasty, pyCachingInfo, (PyObject*) this->m_proxy);
		if (ret)
		{
			bool value = PyLong_AsSsize_t(ret);
			Py_DECREF(ret);
			dopass = value;
		}
		else
		{
			PyErr_Print();
			PyErr_Clear();
			PySys_SetObject( (char *)"last_traceback", NULL);
		}
	}
	else
#endif // WITH_PYTHON
	{
		switch (m_pass++)
		{
			case 0:
				DefaultActivate(rasty, cachingInfo);
				dopass = true;
				break;
			default:
				m_pass = 0;
				dopass = false;
				break;
		}
	}
	
	return dopass;
}

void KX_PolygonMaterial::DefaultActivate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const 
{
	if (GetCachingInfo() != cachingInfo)
	{
		if (!cachingInfo)
			GPU_set_tpage(NULL, 0, 0);

		cachingInfo = GetCachingInfo();

		if ((m_drawingmode & RAS_IRasterizer::KX_TEX)&& (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED))
		{
			Image *ima = (Image*)m_tface->tpage;
			GPU_update_image_time(ima, rasty->GetTime());
			GPU_set_tpage(m_tface, 1, m_alphablend);
		}
		else
			GPU_set_tpage(NULL, 0, 0);
		
		if (m_drawingmode & RAS_IRasterizer::KX_BACKCULL)
			rasty->SetCullFace(true);
		else
			rasty->SetCullFace(false);

		if ((m_drawingmode & RAS_IRasterizer::KX_LINES) ||
		        (rasty->GetDrawingMode() <= RAS_IRasterizer::KX_WIREFRAME))
			rasty->SetLines(true);
		else
			rasty->SetLines(false);
		rasty->SetSpecularity(m_specular[0],m_specular[1],m_specular[2],m_specularity);
		rasty->SetShinyness(m_shininess);
		rasty->SetDiffuse(m_diffuse[0], m_diffuse[1],m_diffuse[2], 1.0);
		if (m_material)
			rasty->SetPolygonOffset(-m_material->zoffs, 0.0);
	}

	//rasty->SetSpecularity(m_specular[0],m_specular[1],m_specular[2],m_specularity);
	//rasty->SetShinyness(m_shininess);
	//rasty->SetDiffuse(m_diffuse[0], m_diffuse[1],m_diffuse[2], 1.0);
	//if (m_material)
	//	rasty->SetPolygonOffset(-m_material->zoffs, 0.0);
}

void KX_PolygonMaterial::GetMaterialRGBAColor(unsigned char *rgba) const
{
	if (m_material) {
		*rgba++ = (unsigned char) (m_material->r*255.0);
		*rgba++ = (unsigned char) (m_material->g*255.0);
		*rgba++ = (unsigned char) (m_material->b*255.0);
		*rgba++ = (unsigned char) (m_material->alpha*255.0);
	} else
		RAS_IPolyMaterial::GetMaterialRGBAColor(rgba);
}

#ifdef WITH_PYTHON

//----------------------------------------------------------------------------
//Python


PyMethodDef KX_PolygonMaterial::Methods[] = {
	KX_PYMETHODTABLE(KX_PolygonMaterial, setCustomMaterial),
	KX_PYMETHODTABLE(KX_PolygonMaterial, updateTexture),
	KX_PYMETHODTABLE(KX_PolygonMaterial, setTexture),
	KX_PYMETHODTABLE(KX_PolygonMaterial, activate),
//	KX_PYMETHODTABLE(KX_PolygonMaterial, setPerPixelLights),
	
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_PolygonMaterial::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("texture",	KX_PolygonMaterial, pyattr_get_texture),
	KX_PYATTRIBUTE_RO_FUNCTION("material",	KX_PolygonMaterial, pyattr_get_material), /* should probably be .name ? */
	
	KX_PYATTRIBUTE_INT_RW("tile", INT_MIN, INT_MAX, true, KX_PolygonMaterial, m_tile),
	KX_PYATTRIBUTE_INT_RW("tilexrep", INT_MIN, INT_MAX, true, KX_PolygonMaterial, m_tilexrep),
	KX_PYATTRIBUTE_INT_RW("tileyrep", INT_MIN, INT_MAX, true, KX_PolygonMaterial, m_tileyrep),
	KX_PYATTRIBUTE_INT_RW("drawingmode", INT_MIN, INT_MAX, true, KX_PolygonMaterial, m_drawingmode),	
	//KX_PYATTRIBUTE_INT_RW("lightlayer", INT_MIN, INT_MAX, true, KX_PolygonMaterial, m_lightlayer),

	KX_PYATTRIBUTE_BOOL_RW("transparent", KX_PolygonMaterial, m_alpha),
	KX_PYATTRIBUTE_BOOL_RW("zsort", KX_PolygonMaterial, m_zsort),
	
	KX_PYATTRIBUTE_FLOAT_RW("shininess", 0.0f, 1000.0f, KX_PolygonMaterial, m_shininess),
	KX_PYATTRIBUTE_FLOAT_RW("specularity", 0.0f, 1000.0f, KX_PolygonMaterial, m_specularity),
	
	KX_PYATTRIBUTE_RW_FUNCTION("diffuse", KX_PolygonMaterial, pyattr_get_diffuse, pyattr_set_diffuse),
	KX_PYATTRIBUTE_RW_FUNCTION("specular",KX_PolygonMaterial, pyattr_get_specular, pyattr_set_specular),	
	
	KX_PYATTRIBUTE_RO_FUNCTION("tface",	KX_PolygonMaterial, pyattr_get_tface), /* How the heck is this even useful??? - Campbell */
	KX_PYATTRIBUTE_RO_FUNCTION("gl_texture", KX_PolygonMaterial, pyattr_get_gl_texture), /* could be called 'bindcode' */
	
	/* triangle used to be an attribute, removed for 2.49, nobody should be using it */
	{ NULL }	//Sentinel
};

PyTypeObject KX_PolygonMaterial::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_PolygonMaterial",
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

KX_PYMETHODDEF_DOC(KX_PolygonMaterial, setCustomMaterial, "setCustomMaterial(material)")
{
	PyObject *material;
	if (PyArg_ParseTuple(args, "O:setCustomMaterial", &material))
	{
		if (m_pymaterial) {
			Py_DECREF(m_pymaterial);
		}
		m_pymaterial = material;
		Py_INCREF(m_pymaterial);
		Py_RETURN_NONE;
	}
	
	return NULL;
}

KX_PYMETHODDEF_DOC(KX_PolygonMaterial, updateTexture, "updateTexture(tface, rasty)")
{
	PyObject *pyrasty, *pytface;
	if (PyArg_ParseTuple(args, "O!O!:updateTexture", &PyCapsule_Type, &pytface, &PyCapsule_Type, &pyrasty))
	{
		MTFace *tface = (MTFace*) PyCapsule_GetPointer(pytface, KX_POLYGONMATERIAL_CAPSULE_ID);
		RAS_IRasterizer *rasty = (RAS_IRasterizer*) PyCapsule_GetPointer(pyrasty, KX_POLYGONMATERIAL_CAPSULE_ID);
		Image *ima = (Image*)tface->tpage;
		GPU_update_image_time(ima, rasty->GetTime());

		Py_RETURN_NONE;
	}
	
	return NULL;
}

KX_PYMETHODDEF_DOC(KX_PolygonMaterial, setTexture, "setTexture(tface)")
{
	PyObject *pytface;
	if (PyArg_ParseTuple(args, "O!:setTexture", &PyCapsule_Type, &pytface))
	{
		MTFace *tface = (MTFace*) PyCapsule_GetPointer(pytface, KX_POLYGONMATERIAL_CAPSULE_ID);
		GPU_set_tpage(tface, 1, m_alphablend);
		Py_RETURN_NONE;
	}
	
	return NULL;
}

KX_PYMETHODDEF_DOC(KX_PolygonMaterial, activate, "activate(rasty, cachingInfo)")
{
	PyObject *pyrasty, *pyCachingInfo;
	if (PyArg_ParseTuple(args, "O!O!:activate", &PyCapsule_Type, &pyrasty, &PyCapsule_Type, &pyCachingInfo))
	{
		RAS_IRasterizer *rasty = static_cast<RAS_IRasterizer*>(PyCapsule_GetPointer(pyrasty, KX_POLYGONMATERIAL_CAPSULE_ID));
		TCachingInfo *cachingInfo = static_cast<TCachingInfo*>(PyCapsule_GetPointer(pyCachingInfo, KX_POLYGONMATERIAL_CAPSULE_ID));
		if (rasty && cachingInfo)
		{
			DefaultActivate(rasty, *cachingInfo);
			Py_RETURN_NONE;
		}
	}
	
	return NULL;
}

PyObject* KX_PolygonMaterial::pyattr_get_texture(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolygonMaterial* self= static_cast<KX_PolygonMaterial*>(self_v);
	return PyUnicode_From_STR_String(self->m_texturename);
}

PyObject* KX_PolygonMaterial::pyattr_get_material(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolygonMaterial* self= static_cast<KX_PolygonMaterial*>(self_v);
	return PyUnicode_From_STR_String(self->m_materialname);
}

/* this does not seem useful */
PyObject* KX_PolygonMaterial::pyattr_get_tface(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolygonMaterial* self= static_cast<KX_PolygonMaterial*>(self_v);
	return PyCapsule_New(self->m_tface, KX_POLYGONMATERIAL_CAPSULE_ID, NULL);
}

PyObject* KX_PolygonMaterial::pyattr_get_gl_texture(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolygonMaterial* self= static_cast<KX_PolygonMaterial*>(self_v);
	int bindcode= 0;
	if (self->m_tface && self->m_tface->tpage)
		bindcode= self->m_tface->tpage->bindcode;
	
	return PyLong_FromSsize_t(bindcode);
}


PyObject* KX_PolygonMaterial::pyattr_get_diffuse(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolygonMaterial* self= static_cast<KX_PolygonMaterial*>(self_v);
	return PyObjectFrom(self->m_diffuse);
}

int KX_PolygonMaterial::pyattr_set_diffuse(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_PolygonMaterial* self= static_cast<KX_PolygonMaterial*>(self_v);
	MT_Vector3 vec;
	
	if (!PyVecTo(value, vec))
		return PY_SET_ATTR_FAIL;
	
	self->m_diffuse= vec;
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_PolygonMaterial::pyattr_get_specular(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_PolygonMaterial* self= static_cast<KX_PolygonMaterial*>(self_v);
	return PyObjectFrom(self->m_specular);
}

int KX_PolygonMaterial::pyattr_set_specular(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_PolygonMaterial* self= static_cast<KX_PolygonMaterial*>(self_v);
	MT_Vector3 vec;
	
	if (!PyVecTo(value, vec))
		return PY_SET_ATTR_FAIL;
	
	self->m_specular= vec;
	return PY_SET_ATTR_SUCCESS;
}

#endif // WITH_PYTHON
