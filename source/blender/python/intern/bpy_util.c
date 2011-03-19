/*
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_util.c
 *  \ingroup pythonintern
 */


#include <Python.h>

#include "bpy_util.h"
#include "BLI_dynstr.h"
#include "MEM_guardedalloc.h"
#include "BKE_report.h"
#include "BKE_context.h"

#include "../generic/py_capi_utils.h"

static bContext*	__py_context= NULL;
bContext*	BPy_GetContext(void) { return __py_context; }
void		BPy_SetContext(bContext *C) { __py_context= C; }

char *BPy_enum_as_string(EnumPropertyItem *item)
{
	DynStr *dynstr= BLI_dynstr_new();
	EnumPropertyItem *e;
	char *cstring;

	for (e= item; item->identifier; item++) {
		if(item->identifier[0])
			BLI_dynstr_appendf(dynstr, (e==item)?"'%s'":", '%s'", item->identifier);
	}

	cstring= BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

short BPy_reports_to_error(ReportList *reports, PyObject *exception, const short clear)
{
	char *report_str;

	report_str= BKE_reports_string(reports, RPT_ERROR);

	if(clear) {
		BKE_reports_clear(reports);
	}

	if(report_str) {
		PyErr_SetString(exception, report_str);
		MEM_freeN(report_str);
	}

	return (report_str == NULL) ? 0 : -1;
}


short BPy_errors_to_report(ReportList *reports)
{
	PyObject *pystring;
	PyObject *pystring_format= NULL; // workaround, see below
	char *cstring;

	const char *filename;
	int lineno;

	if (!PyErr_Occurred())
		return 1;
	
	/* less hassle if we allow NULL */
	if(reports==NULL) {
		PyErr_Print();
		PyErr_Clear();
		return 1;
	}
	
	pystring= PyC_ExceptionBuffer();
	
	if(pystring==NULL) {
		BKE_report(reports, RPT_ERROR, "unknown py-exception, could not convert");
		return 0;
	}
	
	PyC_FileAndNum(&filename, &lineno);
	if(filename==NULL)
		filename= "<unknown location>";
	
	cstring= _PyUnicode_AsString(pystring);

#if 0 // ARG!. workaround for a bug in blenders use of vsnprintf
	BKE_reportf(reports, RPT_ERROR, "%s\nlocation:%s:%d\n", cstring, filename, lineno);
#else
	pystring_format= PyUnicode_FromFormat("%s\nlocation:%s:%d\n", cstring, filename, lineno);
	cstring= _PyUnicode_AsString(pystring_format);
	BKE_report(reports, RPT_ERROR, cstring);
#endif
	
	fprintf(stderr, "%s\nlocation:%s:%d\n", cstring, filename, lineno); // not exactly needed. just for testing
	
	Py_DECREF(pystring);
	Py_DECREF(pystring_format); // workaround
	return 1;
}

/* array utility function */
int PyC_AsArray(void *array, PyObject *value, int length, PyTypeObject *type, const char *error_prefix)
{
	PyObject *value_fast;
	int value_len;
	int i;

	if(!(value_fast=PySequence_Fast(value, error_prefix))) {
		return -1;
	}

	value_len= PySequence_Fast_GET_SIZE(value_fast);

	if(value_len != length) {
		Py_DECREF(value);
		PyErr_Format(PyExc_TypeError, "%.200s: invalid sequence length. expected %d, got %d", error_prefix, length, value_len);
		return -1;
	}

	/* for each type */
	if(type == &PyFloat_Type) {
		float *array_float= array;
		for(i=0; i<length; i++) {
			array_float[i]= PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value_fast, i));
		}
	}
	else if(type == &PyLong_Type) {
		int *array_int= array;
		for(i=0; i<length; i++) {
			array_int[i]= PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value_fast, i));
		}
	}
	else if(type == &PyBool_Type) {
		int *array_bool= array;
		for(i=0; i<length; i++) {
			array_bool[i]= (PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value_fast, i)) != 0);
		}
	}
	else {
		Py_DECREF(value_fast);
		PyErr_Format(PyExc_TypeError, "%s: internal error %s is invalid", error_prefix, type->tp_name);
		return -1;
	}

	Py_DECREF(value_fast);

	if(PyErr_Occurred()) {
		PyErr_Format(PyExc_TypeError, "%s: one or more items could not be used as a %s", error_prefix, type->tp_name);
		return -1;
	}

	return 0;
}
