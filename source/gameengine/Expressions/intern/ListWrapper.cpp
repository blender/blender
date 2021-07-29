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
 * Contributor(s): Porteries Tristan.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ListWrapper.cpp
 *  \ingroup expressions
 */

#ifdef WITH_PYTHON

#include "EXP_ListWrapper.h"

static STR_String pythonGeneratorList = "ListWrapper";

CListWrapper::CListWrapper(void *client,
						   PyObject *base,
						   bool (*checkValid)(void *),
						   int (*getSize)(void *),
						   PyObject *(*getItem)(void *, int),
						   const char *(*getItemName)(void *, int),
						   bool (*setItem)(void *, int, PyObject *))
:m_client(client),
m_base(base),
m_checkValid(checkValid),
m_getSize(getSize),
m_getItem(getItem),
m_getItemName(getItemName),
m_setItem(setItem)
{
	// Incref to always have a existing pointer.
	Py_INCREF(m_base);
}

CListWrapper::~CListWrapper()
{
	Py_DECREF(m_base);
}

bool CListWrapper::CheckValid()
{
	if (m_base && !BGE_PROXY_REF(m_base)) {
		return false;
	}
	return m_checkValid ? (*m_checkValid)(m_client) : true;
}

int CListWrapper::GetSize()
{
	return (*m_getSize)(m_client);
}

PyObject *CListWrapper::GetItem(int index)
{
	return (*m_getItem)(m_client, index);
}

const char *CListWrapper::GetItemName(int index)
{
	return (*m_getItemName)(m_client, index);
}

bool CListWrapper::SetItem(int index, PyObject *item)
{
	return (*m_setItem)(m_client, index, item);
}

bool CListWrapper::AllowSetItem()
{
	return m_setItem != NULL;
}

bool CListWrapper::AllowGetItemByName()
{
	return m_getItemName != NULL;
}

// ================================================================

const STR_String &CListWrapper::GetText()
{
	return pythonGeneratorList;
}

void CListWrapper::SetName(const char *name)
{
}

STR_String &CListWrapper::GetName()
{
	return pythonGeneratorList;
}

CValue *CListWrapper::GetReplica()
{
	return NULL;
}

CValue *CListWrapper::Calc(VALUE_OPERATOR op, CValue *val)
{
	return NULL;
}

CValue *CListWrapper::CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val)
{
	return NULL;
}

double CListWrapper::GetNumber()
{
	return -1;
}

int CListWrapper::GetValueType()
{
	return -1;
}

// We convert all elements to python objects to make a proper repr string.
PyObject *CListWrapper::py_repr()
{
	if (!CheckValid()) {
		PyErr_SetString(PyExc_SystemError, "CListWrapper : repr, " BGE_PROXY_ERROR_MSG);
		return NULL;
	}

	PyObject *py_proxy = GetProxy();
	PyObject *py_list = PySequence_List(py_proxy);
	PyObject *py_string = PyObject_Repr(py_list);
	Py_DECREF(py_list);
	Py_DECREF(py_proxy);
	return py_string;
}


Py_ssize_t CListWrapper::py_len(PyObject *self)
{
	CListWrapper *list = (CListWrapper *)BGE_PROXY_REF(self);
	// Invalid list.
	if (!list->CheckValid()) {
		PyErr_SetString(PyExc_SystemError, "len(CListWrapper), " BGE_PROXY_ERROR_MSG);
		return 0;
	}

	return (Py_ssize_t)list->GetSize();
}

PyObject *CListWrapper::py_get_item(PyObject *self, Py_ssize_t index)
{
	CListWrapper *list = (CListWrapper *)BGE_PROXY_REF(self);
	// Invalid list.
	if (!list->CheckValid()) {
		PyErr_SetString(PyExc_SystemError, "val = CListWrapper[i], " BGE_PROXY_ERROR_MSG);
		return NULL;
	}

	int size = list->GetSize();

	if (index < 0) {
		index = size + index;
	}
	if (index < 0 || index >= size) {
		PyErr_SetString(PyExc_IndexError, "CListWrapper[i]: List index out of range in CListWrapper");
		return NULL;
	}

	PyObject *pyobj = list->GetItem(index);

	return pyobj;
}

int CListWrapper::py_set_item(PyObject *self, Py_ssize_t index, PyObject *value)
{
	CListWrapper *list = (CListWrapper *)BGE_PROXY_REF(self);
	// Invalid list.
	if (!list->CheckValid()) {
		PyErr_SetString(PyExc_SystemError, "CListWrapper[i] = val, " BGE_PROXY_ERROR_MSG);
		return -1;
	}

	if (!list->AllowSetItem()) {
		PyErr_SetString(PyExc_TypeError, "CListWrapper's item type doesn't support assignment");
		return -1;
	}

	if (!value) {
		PyErr_SetString(PyExc_TypeError, "CListWrapper doesn't support item deletion");
		return -1;
	}

	int size = list->GetSize();

	if (index < 0) {
		index = size + index;
	}
	if (index < 0 || index >= size) {
		PyErr_SetString(PyExc_IndexError, "CListWrapper[i]: List index out of range in CListWrapper");
		return -1;
	}

	if (!list->SetItem(index, value)) {
		return -1;
	}
	return 0;
}

PyObject *CListWrapper::py_mapping_subscript(PyObject *self, PyObject *key)
{
	CListWrapper *list = (CListWrapper *)BGE_PROXY_REF(self);
	// Invalid list.
	if (!list->CheckValid()) {
		PyErr_SetString(PyExc_SystemError, "val = CListWrapper[key], " BGE_PROXY_ERROR_MSG);
		return NULL;
	}

	if (PyIndex_Check(key)) {
		Py_ssize_t index = PyLong_AsSsize_t(key);
		return py_get_item(self, index);
	}
	else if (PyUnicode_Check(key)) {
		if (!list->AllowGetItemByName()) {
			PyErr_SetString(PyExc_SystemError, "CListWrapper's item type doesn't support access by key");
			return NULL;
		}

		const char *name = _PyUnicode_AsString(key);
		int size = list->GetSize();

		for (unsigned int i = 0; i < size; ++i) {
			if (strcmp(list->GetItemName(i), name) == 0) {
				return list->GetItem(i);
			}
		}

		PyErr_Format(PyExc_KeyError, "requested item \"%s\" does not exist", name);
		return NULL;
	}

	PyErr_Format(PyExc_KeyError, "CListWrapper[key]: '%R' key not in list", key);
	return NULL;
}

int CListWrapper::py_mapping_ass_subscript(PyObject *self, PyObject *key, PyObject *value)
{
	CListWrapper *list = (CListWrapper *)BGE_PROXY_REF(self);
	// Invalid list.
	if (!list->CheckValid()) {
		PyErr_SetString(PyExc_SystemError, "val = CListWrapper[key], " BGE_PROXY_ERROR_MSG);
		return -1;
	}

	if (!list->AllowSetItem()) {
		PyErr_SetString(PyExc_TypeError, "CListWrapper's item type doesn't support assignment");
		return -1;
	}

	if (PyIndex_Check(key)) {
		Py_ssize_t index = PyLong_AsSsize_t(key);
		return py_set_item(self, index, value);
	}
	else if (PyUnicode_Check(key)) {
		if (!list->AllowGetItemByName()) {
			PyErr_SetString(PyExc_SystemError, "CListWrapper's item type doesn't support access by key");
			return -1;
		}

		const char *name = _PyUnicode_AsString(key);
		int size = list->GetSize();

		for (unsigned int i = 0; i < size; ++i) {
			if (strcmp(list->GetItemName(i), name) == 0) {
				if (!list->SetItem(i, value)) {
					return -1;
				}
				return 0;
			}
		}

		PyErr_Format(PyExc_KeyError, "requested item \"%s\" does not exist", name);
		return -1;
	}

	PyErr_Format(PyExc_KeyError, "CListWrapper[key]: '%R' key not in list", key);
	return -1;
}

int CListWrapper::py_contains(PyObject *self, PyObject *key)
{
	CListWrapper *list = (CListWrapper *)BGE_PROXY_REF(self);
	// Invalid list.
	if (!list->CheckValid()) {
		PyErr_SetString(PyExc_SystemError, "val = CListWrapper[i], " BGE_PROXY_ERROR_MSG);
		return -1;
	}

	if (!list->AllowGetItemByName()) {
		PyErr_SetString(PyExc_SystemError, "CListWrapper's item type doesn't support access by key");
		return -1;
	}

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(PyExc_SystemError, "key in list, CListWrapper: key must be a string");
		return -1;
	}

	const char *name = _PyUnicode_AsString(key);
	int size = list->GetSize();

	for (unsigned int i = 0; i < size; ++i) {
		if (strcmp(list->GetItemName(i), name) == 0) {
			return 1;
		}
	}

	return 0;
}

PySequenceMethods CListWrapper::py_as_sequence = {
	py_len, // sq_length
	NULL, // sq_concat
	NULL, // sq_repeat
	py_get_item, // sq_item
	NULL, // sq_slice
	py_set_item, // sq_ass_item
	NULL, // sq_ass_slice
	(objobjproc)py_contains, // sq_contains
	(binaryfunc) NULL, // sq_inplace_concat
	(ssizeargfunc) NULL, // sq_inplace_repeat
};

PyMappingMethods CListWrapper::py_as_mapping = {
	py_len, // mp_length
	py_mapping_subscript, // mp_subscript
	py_mapping_ass_subscript // mp_ass_subscript
};

PyTypeObject CListWrapper::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"CListWrapper", // tp_name
	sizeof(PyObjectPlus_Proxy), // tp_basicsize
	0, // tp_itemsize
	py_base_dealloc, // tp_dealloc
	0, // tp_print
	0, // tp_getattr
	0, // tp_setattr
	0, // tp_compare
	py_base_repr, // tp_repr
	0, // tp_as_number
	&py_as_sequence, // tp_as_sequence
	&py_as_mapping, // tp_as_mapping
	0, // tp_hash
	0, // tp_call
	0,
	NULL,
	NULL,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef CListWrapper::Methods[] = {
	{"get", (PyCFunction)CListWrapper::sPyGet, METH_VARARGS},
	{NULL, NULL} //Sentinel
};

PyAttributeDef CListWrapper::Attributes[] = {
	{NULL} //Sentinel
};

/* Matches python dict.get(key, [default]) */
PyObject *CListWrapper::PyGet(PyObject *args)
{
	char *name;
	PyObject *def = Py_None;

	// Invalid list.
	if (!CheckValid()) {
		PyErr_SetString(PyExc_SystemError, "val = CListWrapper[i], " BGE_PROXY_ERROR_MSG);
		return NULL;
	}

	if (!AllowGetItemByName()) {
		PyErr_SetString(PyExc_SystemError, "CListWrapper's item type doesn't support access by key");
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "s|O:get", &name, &def)) {
		return NULL;
	}

	for (unsigned int i = 0; i < GetSize(); ++i) {
		if (strcmp(GetItemName(i), name) == 0) {
			return GetItem(i);
		}
	}

	Py_INCREF(def);
	return def;
}

#endif // WITH_PYTHON
