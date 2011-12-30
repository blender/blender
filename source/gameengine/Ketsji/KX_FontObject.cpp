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

/** \file gameengine/Ketsji/KX_FontObject.cpp
 *  \ingroup ketsji
 */

#include "KX_FontObject.h"
#include "DNA_curve_types.h"
#include "KX_Scene.h"
#include "KX_PythonInit.h"
#include "BLI_math.h"

extern "C" {
#include "BLF_api.h"
}

#define BGE_FONT_RES 100

std::vector<STR_String> split_string(STR_String str)
{
	std::vector<STR_String> text = std::vector<STR_String>();

	/* Split the string upon new lines */
	int begin=0, end=0;
	while (end < str.Length())
	{
		if(str.GetAt(end) == '\n')
		{
			text.push_back(str.Mid(begin, end-begin));
			begin = end+1;
		}
		end++;
	}
	//Now grab the last line
	text.push_back(str.Mid(begin, end-begin));

	return text;
}
KX_FontObject::KX_FontObject(	void* sgReplicationInfo,
								SG_Callbacks callbacks,
								RAS_IRenderTools* rendertools,
								Object *ob):
	KX_GameObject(sgReplicationInfo, callbacks),
	m_object(ob),
	m_dpi(72),
	m_resolution(1.f),
	m_rendertools(rendertools)
{
	Curve *text = static_cast<Curve *> (ob->data);
	m_text = split_string(text->str);
	m_fsize = text->fsize;
	m_line_spacing = text->linedist;
	m_offset = MT_Vector3(text->xof, text->yof, 0);

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

	/* initialize the color with the object color and store it in the KX_Object class
	   This is a workaround waiting for the fix:
	   [#25487] BGE: Object Color only works when it has a keyed frame */
	copy_v4_v4(m_color, (const float*) ob->col);
	this->SetObjectColor((const MT_Vector4&) m_color);
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
	/* Allow for some logic brick control */
	if(this->GetProperty("text"))
		m_text = split_string(this->GetProperty("text")->GetText());

	/* only draws the text if visible */
	if(this->GetVisible() == 0) return;

	/* update the animated color */
	this->GetObjectColor().getValue(m_color);

	/* HARDCODED MULTIPLICATION FACTOR - this will affect the render resolution directly */
	float RES = BGE_FONT_RES * m_resolution;

	float size = m_fsize * m_object->size[0] * RES;
	float aspect = 1.f / (m_object->size[0] * RES);

	/* Get a working copy of the OpenGLMatrix to use */
	double mat[16];
	memcpy(mat, this->GetOpenGLMatrix(), sizeof(double)*16);

	/* Account for offset */
	MT_Vector3 offset = this->NodeGetWorldOrientation() * m_offset * this->NodeGetWorldScaling();
	mat[12] += offset[0]; mat[13] += offset[1]; mat[14] += offset[2];

	/* Orient the spacing vector */
	MT_Vector3 spacing = MT_Vector3(0, m_fsize*m_line_spacing, 0);
	spacing = this->NodeGetWorldOrientation() * spacing * this->NodeGetWorldScaling()[1];

	/* Draw each line, taking spacing into consideration */
	for(int i=0; i<m_text.size(); ++i)
	{
		if (i!=0)
		{
			mat[12] -= spacing[0];
			mat[13] -= spacing[1];
			mat[14] -= spacing[2];
		}
		m_rendertools->RenderText3D(m_fontid, m_text[i], int(size), m_dpi, m_color, mat, aspect);
	}
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
	//KX_PYATTRIBUTE_STRING_RW("text", 0, 280, false, KX_FontObject, m_text[0]), //arbitrary limit. 280 = 140 unicode chars in unicode
	KX_PYATTRIBUTE_RW_FUNCTION("text", KX_FontObject, pyattr_get_text, pyattr_set_text),
	KX_PYATTRIBUTE_FLOAT_RW("size", 0.0001f, 10000.0f, KX_FontObject, m_fsize),
	KX_PYATTRIBUTE_FLOAT_RW("resolution", 0.0001f, 10000.0f, KX_FontObject, m_resolution),
	/* KX_PYATTRIBUTE_INT_RW("dpi", 0, 10000, false, KX_FontObject, m_dpi), */// no real need for expose this I think
	{ NULL }	//Sentinel
};

PyObject* KX_FontObject::pyattr_get_text(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_FontObject* self= static_cast<KX_FontObject*>(self_v);
	STR_String str = STR_String();
	for(int i=0; i<self->m_text.size(); ++i)
	{
		if(i!=0)
			str += '\n';
		str += self->m_text[i];
	}
	return PyUnicode_FromString(str.ReadPtr());
}

int KX_FontObject::pyattr_set_text(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_FontObject* self= static_cast<KX_FontObject*>(self_v);
	if(!PyUnicode_Check(value))
		return PY_SET_ATTR_FAIL;
	char* chars = _PyUnicode_AsString(value);
	self->m_text = split_string(STR_String(chars));
	return PY_SET_ATTR_SUCCESS;
}

#endif // WITH_PYTHON
