/**
 * $Id$
 *
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
#include "KX_FontObject.h"
#include "DNA_curve_types.h"
#include "KX_Scene.h"
#include "KX_PythonInit.h"

extern "C" {
#include "BLF_api.h"
}

#define BGE_FONT_RES 100

KX_FontObject::KX_FontObject(	void* sgReplicationInfo,
								SG_Callbacks callbacks,
								RAS_IRenderTools* rendertools,
								Object *ob):
	KX_GameObject(sgReplicationInfo, callbacks),
	m_object(ob),
	m_dpi(72),
	m_resolution(1.f),
	m_color(ob->col), /* initial color - non-animatable */
	m_rendertools(rendertools)
{
	Curve *text = static_cast<Curve *> (ob->data);
	m_text = text->str;
	m_fsize = text->fsize;

	/* FO_BUILTIN_NAME != "default"	*/
	/* I hope at some point Blender (2.5x) can have a single font	*/
	/* with unicode support for ui and OB_FONT			*/
	/* once we have packed working we can load the FO_BUILTIN_NAME font	*/
	const char* filepath = text->vfont->name;
	if (strcmp(FO_BUILTIN_NAME, filepath) == 0)
		filepath = "default";

	/* XXX - if it's packed it will not work. waiting for bdiego (Diego) fix for that. */
	m_fontid = BLF_load(filepath);
	if (m_fontid == -1)
		m_fontid = BLF_load("default");
}

KX_FontObject::~KX_FontObject()
{
	//remove font from the scene list
	//it's handled in KX_Scene::NewRemoveObject
}

CValue* KX_FontObject::GetReplica() {
	KX_FontObject* replica = new KX_FontObject(*this);
	replica->ProcessReplica();
	return replica;
}

void KX_FontObject::ProcessReplica()
{
	KX_GameObject::ProcessReplica();
	KX_GetActiveScene()->AddFont(this);
}

void KX_FontObject::DrawText()
{
	/* only draws the text if visible */
	if(this->GetVisible() == 0) return;

	/* XXX 2DO - handle multiple lines */
	/* HARDCODED MULTIPLICATION FACTOR - this will affect the render resolution directly */
	float RES = BGE_FONT_RES * m_resolution;

	float size = m_fsize * m_object->size[0] * RES;
	float aspect = 1.f / (m_object->size[0] * RES);
	m_rendertools->RenderText3D(m_fontid, m_text, int(size), m_dpi, m_color, this->GetOpenGLMatrix(), aspect);
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python Integration Hooks					                                 */
/* ------------------------------------------------------------------------- */

PyTypeObject KX_FontObject::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_FontObject",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	&KX_GameObject::Sequence,
	&KX_GameObject::Mapping,
	0,0,0,
	NULL,
	NULL,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&KX_GameObject::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_FontObject::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_FontObject::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW("text", 0, 140, false, KX_FontObject, m_text),
	KX_PYATTRIBUTE_FLOAT_RW("size", 0.0001f, 10000.0f, KX_FontObject, m_fsize),
	KX_PYATTRIBUTE_FLOAT_RW("resolution", 0.0001f, 10000.0f, KX_FontObject, m_resolution),
	/* KX_PYATTRIBUTE_INT_RW("dpi", 0, 10000, false, KX_FontObject, m_dpi), */// no real need for expose this I think
	{ NULL }	//Sentinel
};

#endif // WITH_PYTHON
