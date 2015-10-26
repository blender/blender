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

/** \file EXP_ListWrapper.h
 *  \ingroup expressions
 */

#ifdef WITH_PYTHON

#ifndef __EXP_LISTWRAPPER_H__
#define __EXP_LISTWRAPPER_H__

#include "EXP_Value.h"

class CListWrapper : public CValue  
{
	Py_Header
private:
	/** The client instance passed as first argument of each callback.
	 * We use a void * instead of a template to avoid to declare this class
	 * for each use in KX_PythonInitTypes.
	 */
	void *m_client;

	// The python object which owned this list.
	PyObject *m_base;

	/// Returns true if the list is still valid, else each call will raise an error.
	bool (*m_checkValid)(void *);

	/// Returns the list size.
	int (*m_getSize)(void *);

	/// Returns the list item for the giving index.
	PyObject *(*m_getItem)(void *, int);

	/// Returns name item for the giving index, used for python operator list["name"].
	const char *(*m_getItemName)(void *, int);

	/// Sets the nex item to the index place, return false when failed item conversion.
	bool (*m_setItem)(void *, int, PyObject *);

public:
	CListWrapper(void *client,
						PyObject *base,
						bool (*checkValid)(void *),
						int (*getSize)(void *),
						PyObject *(*getItem)(void *, int),
						const char *(*getItemName)(void *, int),
						bool (*setItem)(void *, int, PyObject *));
	~CListWrapper();

	/// \section Python Interface
	bool CheckValid();
	int GetSize();
	PyObject *GetItem(int index);
	const char *GetItemName(int index);
	bool SetItem(int index, PyObject *item);
	bool AllowSetItem();
	bool AllowGetItemByName();

	/// \section CValue Inherited Functions.
	virtual const STR_String &GetText();
	virtual void SetName(const char *name);
	virtual STR_String &GetName();
	virtual CValue *GetReplica();
	virtual CValue *Calc(VALUE_OPERATOR op, CValue *val);
	virtual CValue *CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	virtual double GetNumber();
	virtual int GetValueType();
	virtual PyObject *py_repr();

	// Python list operators.
	static PySequenceMethods py_as_sequence;
	// Python dictionnary operators.
	static PyMappingMethods py_as_mapping;

	static Py_ssize_t py_len(PyObject *self);
	static PyObject *py_get_item(PyObject *self, Py_ssize_t index);
	static int py_set_item(PyObject *self, Py_ssize_t index, PyObject *value);
	static PyObject *py_mapping_subscript(PyObject *self, PyObject *key);
	static int py_mapping_ass_subscript(PyObject *self, PyObject *key, PyObject *value);
	static int py_contains(PyObject *self, PyObject *key);

	KX_PYMETHOD_VARARGS(CListWrapper, Get);
};

#endif // __EXP_LISTWRAPPER_H__

#endif // WITH_PYTHON
