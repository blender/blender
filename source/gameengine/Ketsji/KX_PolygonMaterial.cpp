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

KX_PolygonMaterial::KX_PolygonMaterial(const STR_String &texname,
											   Material *material,
											   int tile,
											   int tilexrep,
											   int tileyrep,
											   int mode,
											   int transp,
											   bool alpha,
											   bool zsort,
											   int lightlayer,
											   struct MTFace* tface,
											   unsigned int* mcol,
											   PyTypeObject *T)
		: PyObjectPlus(T),
		  RAS_IPolyMaterial(texname,
							STR_String(material?material->id.name:""),
							tile,
							tilexrep,
							tileyrep,
							mode,
							transp,
							alpha,
							zsort,
							lightlayer),
		m_tface(tface),
		m_mcol(mcol),
		m_material(material),
		m_pymaterial(0),
		m_pass(0)
{
}

KX_PolygonMaterial::~KX_PolygonMaterial()
{
	if (m_pymaterial)
	{
		Py_DECREF(m_pymaterial);
	}
}

bool KX_PolygonMaterial::Activate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const 
{
	bool dopass = false;
	if (m_pymaterial)
	{
		PyObject *pyRasty = PyCObject_FromVoidPtr((void*)rasty, NULL);	/* new reference */
		PyObject *pyCachingInfo = PyCObject_FromVoidPtr((void*) &cachingInfo, NULL); /* new reference */
		
		PyObject *ret = PyObject_CallMethod(m_pymaterial, "activate", "(NNO)", pyRasty, pyCachingInfo, (PyObject*) this);
		if (ret)
		{
			bool value = PyInt_AsLong(ret);
			Py_DECREF(ret);
			dopass = value;
		}
		else
		{
			PyErr_Print();
		}
	}
	else
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
			GPU_set_tpage(NULL);

		cachingInfo = GetCachingInfo();

		if ((m_drawingmode & 4)&& (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED))
		{
			Image *ima = (Image*)m_tface->tpage;
			GPU_update_image_time(ima, rasty->GetTime());
			GPU_set_tpage(m_tface);
		}
		else
			GPU_set_tpage(NULL);
		
		if(m_drawingmode & RAS_IRasterizer::KX_TWOSIDE)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if ((m_drawingmode & RAS_IRasterizer::KX_LINES) ||
		    (rasty->GetDrawingMode() <= RAS_IRasterizer::KX_WIREFRAME))
			rasty->SetLines(true);
		else
			rasty->SetLines(false);
	}

	rasty->SetSpecularity(m_specular[0],m_specular[1],m_specular[2],m_specularity);
	rasty->SetShinyness(m_shininess);
	rasty->SetDiffuse(m_diffuse[0], m_diffuse[1],m_diffuse[2], 1.0);
	if (m_material)
		rasty->SetPolygonOffset(-m_material->zoffs, 0.0);
}

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


PyTypeObject KX_PolygonMaterial::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_PolygonMaterial",
		sizeof(KX_PolygonMaterial),
		0,
		PyDestructor,
		0,
		__getattr,
		__setattr,
		0, //&MyPyCompare,
		__repr,
		0 //&cvalue_as_number,
};

PyParentObject KX_PolygonMaterial::Parents[] = {
	&PyObjectPlus::Type,
	&KX_PolygonMaterial::Type,
	NULL
};

PyObject* KX_PolygonMaterial::_getattr(const STR_String& attr)
{
	if (attr == "texture")
		return PyString_FromString(m_texturename.ReadPtr());
	if (attr == "material")
		return PyString_FromString(m_materialname.ReadPtr());
		
	if (attr == "tface")
		return PyCObject_FromVoidPtr(m_tface, NULL);
		
	if (attr == "gl_texture")
	{
		Image *ima = m_tface->tpage;
		int bind = 0;
		if (ima)
			bind = ima->bindcode;
		
		return PyInt_FromLong(bind);
	}
	
	if (attr == "tile")
		return PyInt_FromLong(m_tile);
	if (attr == "tilexrep")
		return PyInt_FromLong(m_tilexrep);
	if (attr == "tileyrep")
		return PyInt_FromLong(m_tileyrep);
	
	if (attr == "drawingmode")
		return PyInt_FromLong(m_drawingmode);
	if (attr == "transparent")
		return PyInt_FromLong(m_alpha);
	if (attr == "zsort")
		return PyInt_FromLong(m_zsort);
	if (attr == "lightlayer")
		return PyInt_FromLong(m_lightlayer);
	if (attr == "triangle")
		// deprecated, triangle/quads shouldn't have been a material property
		return 0;
		
	if (attr == "diffuse")
		return PyObjectFrom(m_diffuse);
	if (attr == "shininess")
		return PyFloat_FromDouble(m_shininess);
	if (attr == "specular")
		return PyObjectFrom(m_specular);
	if (attr == "specularity")
		return PyFloat_FromDouble(m_specularity);
	
	_getattr_up(PyObjectPlus);
}

int KX_PolygonMaterial::_setattr(const STR_String &attr, PyObject *pyvalue)
{
	if (PyFloat_Check(pyvalue))
	{
		float value = PyFloat_AsDouble(pyvalue);
		if (attr == "shininess")
		{
			m_shininess = value;
			return 0;
		}
		
		if (attr == "specularity")
		{
			m_specularity = value;
			return 0;
		}
	}
	
	if (PyInt_Check(pyvalue))
	{
		int value = PyInt_AsLong(pyvalue);
		if (attr == "tile")
		{
			m_tile = value;
			return 0;
		}
		
		if (attr == "tilexrep")
		{
			m_tilexrep = value;
			return 0;
		}
		
		if (attr == "tileyrep")
		{
			m_tileyrep = value;
			return 0;
		}
		
		if (attr == "drawingmode")
		{
			m_drawingmode = value;
			return 0;
		}
		
		if (attr == "transparent")
		{
			m_alpha = value;
			return 0;
		}
		
		if (attr == "zsort")
		{
			m_zsort = value;
			return 0;
		}
		
		if (attr == "lightlayer")
		{
			m_lightlayer = value;
			return 0;
		}
		
		// This probably won't work...
		if (attr == "triangle")
		{
			// deprecated, triangle/quads shouldn't have been a material property
			return 0;
		}
	}
	
	if (PySequence_Check(pyvalue))
	{
		if (PySequence_Size(pyvalue) == 3)
		{
			MT_Vector3 value;
			if (PyVecTo(pyvalue, value))
			{
				if (attr == "diffuse")
				{
					m_diffuse = value;
					return 0;
				}
				
				if (attr == "specular")
				{
					m_specular = value;
					return 0;
				}
			}
		}
	}

	return PyObjectPlus::_setattr(attr, pyvalue);
}

KX_PYMETHODDEF_DOC(KX_PolygonMaterial, setCustomMaterial, "setCustomMaterial(material)")
{
	PyObject *material;
	if (PyArg_ParseTuple(args, "O", &material))
	{
		if (m_pymaterial)
			Py_DECREF(m_pymaterial);

		m_pymaterial = material;
		Py_INCREF(m_pymaterial);
		Py_Return;
	}
	
	return NULL;
}

KX_PYMETHODDEF_DOC(KX_PolygonMaterial, updateTexture, "updateTexture(tface, rasty)")
{
	PyObject *pyrasty, *pytface;
	if (PyArg_ParseTuple(args, "O!O!", &PyCObject_Type, &pytface, &PyCObject_Type, &pyrasty))
	{
		MTFace *tface = (MTFace*) PyCObject_AsVoidPtr(pytface);
		RAS_IRasterizer *rasty = (RAS_IRasterizer*) PyCObject_AsVoidPtr(pyrasty);
		Image *ima = (Image*)tface->tpage;
		GPU_update_image_time(ima, rasty->GetTime());

		Py_Return;
	}
	
	return NULL;
}

KX_PYMETHODDEF_DOC(KX_PolygonMaterial, setTexture, "setTexture(tface)")
{
	PyObject *pytface;
	if (PyArg_ParseTuple(args, "O!", &PyCObject_Type, &pytface))
	{
		MTFace *tface = (MTFace*) PyCObject_AsVoidPtr(pytface);
		GPU_set_tpage(tface);
		Py_Return;
	}
	
	return NULL;
}

KX_PYMETHODDEF_DOC(KX_PolygonMaterial, activate, "activate(rasty, cachingInfo)")
{
	PyObject *pyrasty, *pyCachingInfo;
	if (PyArg_ParseTuple(args, "O!O!", &PyCObject_Type, &pyrasty, &PyCObject_Type, &pyCachingInfo))
	{
		RAS_IRasterizer *rasty = static_cast<RAS_IRasterizer*>(PyCObject_AsVoidPtr(pyrasty));
		TCachingInfo *cachingInfo = static_cast<TCachingInfo*>(PyCObject_AsVoidPtr(pyCachingInfo));
		if (rasty && cachingInfo)
		{
			DefaultActivate(rasty, *cachingInfo);
			Py_Return;
		}
	}
	
	return NULL;
}
