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

#include "KX_PolyProxy.h"
#include "KX_MeshProxy.h"
#include "RAS_MeshObject.h"
#include "KX_BlenderMaterial.h"
#include "KX_PolygonMaterial.h"

#include "KX_PyMath.h"

PyTypeObject KX_PolyProxy::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_PolyProxy",
	sizeof(KX_PolyProxy),
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
	0
};

PyParentObject KX_PolyProxy::Parents[] = {
	&KX_PolyProxy::Type,
	&SCA_IObject::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_PolyProxy::Methods[] = {
	KX_PYMETHODTABLE_NOARG(KX_PolyProxy,getMaterialIndex),
	KX_PYMETHODTABLE_NOARG(KX_PolyProxy,getNumVertex),
	KX_PYMETHODTABLE_NOARG(KX_PolyProxy,isVisible),
	KX_PYMETHODTABLE_NOARG(KX_PolyProxy,isCollider),
	KX_PYMETHODTABLE_NOARG(KX_PolyProxy,getMaterialName),
	KX_PYMETHODTABLE_NOARG(KX_PolyProxy,getTextureName),
	KX_PYMETHODTABLE(KX_PolyProxy,getVertexIndex),
	KX_PYMETHODTABLE_NOARG(KX_PolyProxy,getMesh),
	KX_PYMETHODTABLE_NOARG(KX_PolyProxy,getMaterial),
	{NULL,NULL} //Sentinel
};

PyObject*
KX_PolyProxy::_getattr(const STR_String& attr)
{
	if (attr == "matname")
	{
		return PyString_FromString(m_polygon->GetMaterial()->GetPolyMaterial()->GetMaterialName());
	}
	if (attr == "texture")
	{
		return PyString_FromString(m_polygon->GetMaterial()->GetPolyMaterial()->GetTextureName());
	}
	if (attr == "material")
	{
		RAS_IPolyMaterial *polymat = m_polygon->GetMaterial()->GetPolyMaterial();
		if(polymat->GetFlag() & RAS_BLENDERMAT)
		{
			KX_BlenderMaterial* mat = static_cast<KX_BlenderMaterial*>(polymat);
			Py_INCREF(mat);
			return mat;
		}
		else
		{
			KX_PolygonMaterial* mat = static_cast<KX_PolygonMaterial*>(polymat);
			Py_INCREF(mat);
			return mat;
		}
	}
	if (attr == "matid")
	{
		// we'll have to scan through the material bucket of the mes and compare with 
		// the one of the polygon
		RAS_MaterialBucket* polyBucket = m_polygon->GetMaterial();
		unsigned int matid;
		for (matid=0; matid<m_mesh->NumMaterials(); matid++)
		{
			RAS_MaterialBucket* meshBucket = m_mesh->GetMaterialBucket(matid);
			if (meshBucket == polyBucket)
				// found it
				break;
		}
		return PyInt_FromLong(matid);
	}
	if (attr == "v1")
	{
		return PyInt_FromLong(m_polygon->GetVertexIndexBase().m_indexarray[0]);
	}
	if (attr == "v2")
	{
		return PyInt_FromLong(m_polygon->GetVertexIndexBase().m_indexarray[1]);
	}
	if (attr == "v3")
	{
		return PyInt_FromLong(m_polygon->GetVertexIndexBase().m_indexarray[2]);
	}
	if (attr == "v4")
	{
		return PyInt_FromLong(((m_polygon->VertexCount()>3)?m_polygon->GetVertexIndexBase().m_indexarray[3]:0));
	}
	if (attr == "visible")
	{
		return PyInt_FromLong(m_polygon->IsVisible());
	}
	if (attr == "collide")
	{
		return PyInt_FromLong(m_polygon->IsCollider());
	}
	_getattr_up(SCA_IObject);
}

KX_PolyProxy::KX_PolyProxy(const RAS_MeshObject*mesh, RAS_Polygon* polygon)
:	m_mesh((RAS_MeshObject*)mesh),
	m_polygon(polygon)
{
}

KX_PolyProxy::~KX_PolyProxy()
{
}


// stuff for cvalue related things
CValue*		KX_PolyProxy::Calc(VALUE_OPERATOR, CValue *) { return NULL;}
CValue*		KX_PolyProxy::CalcFinal(VALUE_DATA_TYPE, VALUE_OPERATOR, CValue *) { return NULL;}	
STR_String	sPolyName="polygone";
const STR_String &	KX_PolyProxy::GetText() {return sPolyName;};
float		KX_PolyProxy::GetNumber() { return -1;}
STR_String	KX_PolyProxy::GetName() { return sPolyName;}
void		KX_PolyProxy::SetName(STR_String) { };
CValue*		KX_PolyProxy::GetReplica() { return NULL;}
void		KX_PolyProxy::ReplicaSetName(STR_String) {};


// stuff for python integration

KX_PYMETHODDEF_DOC_NOARG(KX_PolyProxy, getMaterialIndex, 
"getMaterialIndex() : return the material index of the polygon in the mesh\n")
{
	RAS_MaterialBucket* polyBucket = m_polygon->GetMaterial();
	unsigned int matid;
	for (matid=0; matid<m_mesh->NumMaterials(); matid++)
	{
		RAS_MaterialBucket* meshBucket = m_mesh->GetMaterialBucket(matid);
		if (meshBucket == polyBucket)
			// found it
			break;
	}
	return PyInt_FromLong(matid);
}

KX_PYMETHODDEF_DOC_NOARG(KX_PolyProxy, getNumVertex,
"getNumVertex() : returns the number of vertex of the polygon, 3 or 4\n")
{
	return PyInt_FromLong(m_polygon->VertexCount());
}

KX_PYMETHODDEF_DOC_NOARG(KX_PolyProxy, isVisible,
"isVisible() : returns whether the polygon is visible or not\n")
{
	return PyInt_FromLong(m_polygon->IsVisible());
}

KX_PYMETHODDEF_DOC_NOARG(KX_PolyProxy, isCollider,
"isCollider() : returns whether the polygon is receives collision or not\n")
{
	return PyInt_FromLong(m_polygon->IsCollider());
}

KX_PYMETHODDEF_DOC_NOARG(KX_PolyProxy, getMaterialName,
"getMaterialName() : returns the polygon material name, \"NoMaterial\" if no material\n")
{
	return PyString_FromString(m_polygon->GetMaterial()->GetPolyMaterial()->GetMaterialName());
}

KX_PYMETHODDEF_DOC_NOARG(KX_PolyProxy, getTextureName,
"getTexturelName() : returns the polygon texture name, \"NULL\" if no texture\n")
{
	return PyString_FromString(m_polygon->GetMaterial()->GetPolyMaterial()->GetTextureName());
}

KX_PYMETHODDEF_DOC(KX_PolyProxy, getVertexIndex,
"getVertexIndex(vertex) : returns the mesh vertex index of a polygon vertex\n"
"vertex: index of the vertex in the polygon: 0->3\n"
"return value can be used to retrieve the vertex details through mesh proxy\n"
"Note: getVertexIndex(3) on a triangle polygon returns 0\n")
{
	int index;
	if (!PyArg_ParseTuple(args,"i",&index))
	{
		return NULL;
	}
	if (index < 0 || index > 3)
	{
		PyErr_SetString(PyExc_AttributeError, "Valid range for index is 0-3");
		return NULL;
	}
	if (index < m_polygon->VertexCount())
	{
		return PyInt_FromLong(m_polygon->GetVertexIndexBase().m_indexarray[index]);
	}
	return PyInt_FromLong(0);
}

KX_PYMETHODDEF_DOC_NOARG(KX_PolyProxy, getMesh,
"getMesh() : returns a mesh proxy\n")
{
	KX_MeshProxy* meshproxy = new KX_MeshProxy((RAS_MeshObject*)m_mesh);
	return meshproxy;
}

KX_PYMETHODDEF_DOC_NOARG(KX_PolyProxy, getMaterial,
"getMaterial() : returns a material\n")
{
	RAS_IPolyMaterial *polymat = m_polygon->GetMaterial()->GetPolyMaterial();
	if(polymat->GetFlag() & RAS_BLENDERMAT)
	{
		KX_BlenderMaterial* mat = static_cast<KX_BlenderMaterial*>(polymat);
		Py_INCREF(mat);
		return mat;
	}
	else
	{
		KX_PolygonMaterial* mat = static_cast<KX_PolygonMaterial*>(polymat);
		Py_INCREF(mat);
		return mat;
	}
}
