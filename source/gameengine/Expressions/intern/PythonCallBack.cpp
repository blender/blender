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

/** \file gameengine/Expressions/intern/PythonCallBack.cpp
 *  \ingroup expressions
 */

#include "EXP_PythonCallBack.h"
#include <iostream>
#include <stdarg.h>

#include "BLI_alloca.h"

/** Check if a python value is a function and have the correct number of arguments.
 * \param value The python value to check.
 * \param minargcount The minimum of arguments possible.
 * \param maxargcount The maximum of arguments possible.
 * \param r_argcount The number of argument of this function, this variable will be
 * changed in the function.
 */
static PyObject *CheckPythonFunction(PyObject *value, unsigned int minargcount, unsigned int maxargcount, unsigned int &r_argcount)
{
	if (PyMethod_Check(value)) {
		PyCodeObject *code = ((PyCodeObject *)PyFunction_GET_CODE(PyMethod_GET_FUNCTION(value)));
		// *args support
		r_argcount = (code->co_flags & CO_VARARGS) ? maxargcount : (code->co_argcount - 1);
	}
	else if (PyFunction_Check(value)) {
		PyCodeObject *code = ((PyCodeObject *)PyFunction_GET_CODE(value));
		// *args support
		r_argcount = (code->co_flags & CO_VARARGS) ? maxargcount : code->co_argcount;
	}
	else { // is not a methode or a function
		PyErr_Format(PyExc_TypeError, "items must be functions or methodes, not %s",
					 Py_TYPE(value)->tp_name);
		return NULL;
	}

	if (r_argcount < minargcount || r_argcount >  maxargcount) {
		// wrong number of arguments
		PyErr_Format(PyExc_TypeError, "methode or function (%s) has invalid number of arguments (%i) must be between %i and %i",
					 Py_TYPE(value)->tp_name, r_argcount, minargcount, maxargcount);
		return NULL;
	}

	return value;
}

/** Create a python tuple to call a python function
 * \param argcount The lenght of the tuple.
 * \param arglist The fully list of python arguments [size >= argcount].
 */
static PyObject *CreatePythonTuple(unsigned int argcount, PyObject **arglist)
{
	PyObject *tuple = PyTuple_New(argcount);

	for (unsigned int i = 0; i < argcount; ++i) {
		PyObject *item = arglist[i];
		// increment reference and copy it in a new tuple
		Py_INCREF(item);
		PyTuple_SET_ITEM(tuple, i, item);
	}

	return tuple;
}

void RunPythonCallBackList(PyObject *functionlist, PyObject **arglist, unsigned int minargcount, unsigned int maxargcount)
{
	unsigned int size = PyList_Size(functionlist);
	PyObject **argTuples = (PyObject **)BLI_array_alloca(argTuples, maxargcount - minargcount + 1);
	memset(argTuples, 0, sizeof(PyObject *) * (maxargcount - minargcount + 1));

	for (unsigned int i = 0; i < size; ++i) {
		unsigned int funcargcount = 0;

		PyObject *item = PyList_GET_ITEM(functionlist, i);
		PyObject *func = CheckPythonFunction(item, minargcount, maxargcount, funcargcount);
		if (!func) { // this item fails the check
			PyErr_Print();
			PyErr_Clear();
			continue;
		}

		// get correct argument tuple.
		PyObject *tuple = argTuples[funcargcount - minargcount];
		if (!tuple)
			argTuples[funcargcount - minargcount] = tuple = CreatePythonTuple(funcargcount, arglist);

		PyObject *ret = PyObject_Call(func, tuple, NULL);
		if (!ret) { // if ret is NULL this seems that the function doesn't work !
			PyErr_Print();
			PyErr_Clear();
		}
		else
			Py_DECREF(ret);
	}

	for (unsigned int i = 0; i <= (maxargcount - minargcount); ++i)
		Py_XDECREF(argTuples[i]);
}
