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
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_PolyProxy",
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
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_PolyProxy::Methods[] = {
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterialIndex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getNumVertex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,isVisible),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,isCollider),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterialName),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getTextureName),
	KX_PYMETHODTABLE(KX_PolyProxy,getVertexIndex),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMesh),
	KX_PYMETHODTABLE_NOARGS(KX_PolyProxy,getMaterial),
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_PolyProxy::Attributes[] = {
	/* All dummy's so they come up in a dir() */
	//KX_PYATTRIBUTE_TODO("DummyProps"),
	KX_PYATTRIBUTE_DUMMY("matname"),
	KX_PYATTRIBUTE_DUMMY("texture"),
	KX_PYATTRIBUTE_DUMMY("material"),
	KX_PYATTRIBUTE_DUMMY("matid"),
	KX_PYATTRIBUTE_DUMMY("v1"),
	KX_PYATTRIBUTE_DUMMY("v2"),
	KX_PYATTRIBUTE_DUMMY("v3"),
	KX_PYATTRIBUTE_DUMMY("v4"),
	KX_PYATTRIBUTE_DUMMY("visible"),
	KX_PYATTRIBUTE_DUMMY("collide"),
	{ NULL }	//Sentinel
};

#if 0
PyObject* KX_PolyProxy::py_getattro(PyObject *attr)
{
	char *attr_str= _PyUnicode_AsString(attr);
	if (!strcmp(attr_str, "matname"))
	{
		return PyUnicode_FromString(m_polygon->GetMaterial()->GetPolyMaterial()->GetMaterialName());
	}
	if (!strcmp(attr_str, "texture"))
	{
		return PyUnicode_FromString(m_polygon->GetMaterial()->GetPolyMaterial()->GetTextureName());
	}
	if (!strcmp(attr_str, "material"))
	{
		RAS_IPolyMaterial *polymat = m_polygon->GetMaterial()->GetPolyMaterial();
		if(polymat->GetFlag() & RAS_BLENDERMAT)
		{
			KX_BlenderMaterial* mat = static_cast<KX_BlenderMaterial*>(polymat);
			return mat->GetProxy();
		}
		else
		{
			KX_PolygonMaterial* mat = static_cast<KX_PolygonMaterial*>(polymat);
			return mat->GetProxy();
		}
	}
	if (!strcmp(attr_str, "matid"))
	{
		// we'll have to scan through the material bucket of the mes and compare with 
		// the one of the polygon
		RAS_MaterialBucket* polyBucket = m_polygon->GetMaterial();
		unsigned int matid;
		for (matid=0; matid<(unsigned int)m_mesh->NumMaterials(); matid++)
		{
			RAS_MeshMaterial* meshMat = m_mesh->GetMeshMaterial(matid);
			if (meshMat->m_bucket == polyBucket)
				// found it
				break;
		}
		return PyLong_FromSsize_t(matid);
	}
	if (!strcmp(attr_str, "v1"))
	{
		return PyLong_FromSsize_t(m_polygon->GetVertexOffsetAbs(m_mesh, 0));
	}
	if (!strcmp(attr_str, "v2"))
	{
		return PyLong_FromSsize_t(m_polygon->GetVertexOffsetAbs(m_mesh, 1));
	}
	if (!strcmp(attr_str, "v3"))
	{
		return PyLong_FromSsize_t(m_polygon->GetVertexOffsetAbs(m_mesh, 2));
	}
	if (!strcmp(attr_str, "v4"))
	{
		return PyLong_FromSsize_t(((m_polygon->VertexCount()>3)?m_polygon->GetVertexOffsetAbs(m_mesh, 3):0));
	}
	if (!strcmp(attr_str, "visible"))
	{
		return PyLong_FromSsize_t(m_polygon->IsVisible());
	}
	if (!strcmp(attr_str, "collide"))
	{
		return PyLong_FromSsize_t(m_polygon->IsCollider());
	}
	// py_getattro_up(CValue); // XXX -- todo, make all these attributes
}
#endif

KX_PolyProxy::KX_PolyProxy(const RAS_MeshObject*mesh, RAS_Polygon* polygon)
:	m_polygon(polygon),
	m_mesh((RAS_MeshObject*)mesh)
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
double		KX_PolyProxy::GetNumber() { return -1;}
STR_String&	KX_PolyProxy::GetName() { return sPolyName;}
void		KX_PolyProxy::SetName(const char *) { };
CValue*		KX_PolyProxy::GetReplica() { return NULL;}

// stuff for python integration

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterialIndex, 
"getMaterialIndex() : return the material index of the polygon in the mesh\n")
{
	RAS_MaterialBucket* polyBucket = m_polygon->GetMaterial();
	unsigned int matid;
	for (matid=0; matid<(unsigned int)m_mesh->NumMaterials(); matid++)
	{
		RAS_MeshMaterial* meshMat = m_mesh->GetMeshMaterial(matid);
		if (meshMat->m_bucket == polyBucket)
			// found it
			break;
	}
	return PyLong_FromSsize_t(matid);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getNumVertex,
"getNumVertex() : returns the number of vertex of the polygon, 3 or 4\n")
{
	return PyLong_FromSsize_t(m_polygon->VertexCount());
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, isVisible,
"isVisible() : returns whether the polygon is visible or not\n")
{
	return PyLong_FromSsize_t(m_polygon->IsVisible());
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, isCollider,
"isCollider() : returns whether the polygon is receives collision or not\n")
{
	return PyLong_FromSsize_t(m_polygon->IsCollider());
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterialName,
"getMaterialName() : returns the polygon material name, \"NoMaterial\" if no material\n")
{
	return PyUnicode_FromString(m_polygon->GetMaterial()->GetPolyMaterial()->GetMaterialName());
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getTextureName,
"getTexturelName() : returns the polygon texture name, \"NULL\" if no texture\n")
{
	return PyUnicode_FromString(m_polygon->GetMaterial()->GetPolyMaterial()->GetTextureName());
}

KX_PYMETHODDEF_DOC(KX_PolyProxy, getVertexIndex,
"getVertexIndex(vertex) : returns the mesh vertex index of a polygon vertex\n"
"vertex: index of the vertex in the polygon: 0->3\n"
"return value can be used to retrieve the vertex details through mesh proxy\n"
"Note: getVertexIndex(3) on a triangle polygon returns 0\n")
{
	int index;
	if (!PyArg_ParseTuple(args,"i:getVertexIndex",&index))
	{
		return NULL;
	}
	if (index < 0 || index > 3)
	{
		PyErr_SetString(PyExc_AttributeError, "poly.getVertexIndex(int): KX_PolyProxy, expected an index between 0-3");
		return NULL;
	}
	if (index < m_polygon->VertexCount())
	{
		return PyLong_FromSsize_t(m_polygon->GetVertexOffsetAbs(m_mesh, index));
	}
	return PyLong_FromSsize_t(0);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMesh,
"getMesh() : returns a mesh proxy\n")
{
	KX_MeshProxy* meshproxy = new KX_MeshProxy((RAS_MeshObject*)m_mesh);
	return meshproxy->NewProxy(true);
}

KX_PYMETHODDEF_DOC_NOARGS(KX_PolyProxy, getMaterial,
"getMaterial() : returns a material\n")
{
	RAS_IPolyMaterial *polymat = m_polygon->GetMaterial()->GetPolyMaterial();
	if(polymat->GetFlag() & RAS_BLENDERMAT)
	{
		KX_BlenderMaterial* mat = static_cast<KX_BlenderMaterial*>(polymat);
		return mat->GetProxy();
	}
	else
	{
		KX_PolygonMaterial* mat = static_cast<KX_PolygonMaterial*>(polymat);
		return mat->GetProxy();
	}
}
