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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "bpy_util.h"
#include "BLI_dynstr.h"
#include "MEM_guardedalloc.h"
#include "BKE_report.h"
#include "BKE_context.h"

#include "../generic/py_capi_utils.h"

bContext*	__py_context = NULL;
bContext*	BPy_GetContext(void) { return __py_context; };
void		BPy_SetContext(bContext *C) { __py_context= C; };

int BPY_class_validate(const char *class_type, PyObject *class, PyObject *base_class, BPY_class_attr_check* class_attrs, PyObject **py_class_attrs)
{
	PyObject *item, *fitem;
	PyObject *py_arg_count;
	int i, arg_count;

	if (base_class) {
		if (!PyObject_IsSubclass(class, base_class)) {
			PyObject *name= PyObject_GetAttrString(base_class, "__name__");
			PyErr_Format( PyExc_AttributeError, "expected %s subclass of class \"%s\"", class_type, name ? _PyUnicode_AsString(name):"<UNKNOWN>");
			Py_XDECREF(name);
			return -1;
		}
	}
	
	for(i= 0;class_attrs->name; class_attrs++, i++) {
		item = PyObject_GetAttrString(class, class_attrs->name);

		if (py_class_attrs)
			py_class_attrs[i]= item;
		
		if (item==NULL) {
			if ((class_attrs->flag & BPY_CLASS_ATTR_OPTIONAL)==0) {
				PyErr_Format( PyExc_AttributeError, "expected %s class to have an \"%s\" attribute", class_type, class_attrs->name);
				return -1;
			}

			PyErr_Clear();
		}
		else {
			Py_DECREF(item); /* no need to keep a ref, the class owns it */

			if((item==Py_None) && (class_attrs->flag & BPY_CLASS_ATTR_NONE_OK)) {
				/* dont do anything, this is ok, dont bother checking other types */
			}
			else {
				switch(class_attrs->type) {
				case 's':
					if (PyUnicode_Check(item)==0) {
						PyErr_Format( PyExc_AttributeError, "expected %s class \"%s\" attribute to be a string", class_type, class_attrs->name);
						return -1;
					}
					if(class_attrs->len != -1 && class_attrs->len < PyUnicode_GetSize(item)) {
						PyErr_Format( PyExc_AttributeError, "expected %s class \"%s\" attribute string to be shorter then %d", class_type, class_attrs->name, class_attrs->len);
						return -1;
					}

					break;
				case 'l':
					if (PyList_Check(item)==0) {
						PyErr_Format( PyExc_AttributeError, "expected %s class \"%s\" attribute to be a list", class_type, class_attrs->name);
						return -1;
					}
					if(class_attrs->len != -1 && class_attrs->len < PyList_GET_SIZE(item)) {
						PyErr_Format( PyExc_AttributeError, "expected %s class \"%s\" attribute list to be shorter then %d", class_type, class_attrs->name, class_attrs->len);
						return -1;
					}
					break;
				case 'f':
					if (PyMethod_Check(item))
						fitem= PyMethod_Function(item); /* py 2.x */
					else
						fitem= item; /* py 3.x */

					if (PyFunction_Check(fitem)==0) {
						PyErr_Format( PyExc_AttributeError, "expected %s class \"%s\" attribute to be a function", class_type, class_attrs->name);
						return -1;
					}
					if (class_attrs->arg_count >= 0) { /* -1 if we dont care*/
						py_arg_count = PyObject_GetAttrString(PyFunction_GET_CODE(fitem), "co_argcount");
						arg_count = PyLong_AsSsize_t(py_arg_count);
						Py_DECREF(py_arg_count);

						if (arg_count != class_attrs->arg_count) {
							PyErr_Format( PyExc_AttributeError, "expected %s class \"%s\" function to have %d args", class_type, class_attrs->name, class_attrs->arg_count);
							return -1;
						}
					}
					break;
				}
			}
		}
	}
	return 0;
}



char *BPy_enum_as_string(EnumPropertyItem *item)
{
	DynStr *dynstr= BLI_dynstr_new();
	EnumPropertyItem *e;
	char *cstring;

	for (e= item; item->identifier; item++) {
		if(item->identifier[0])
			BLI_dynstr_appendf(dynstr, (e==item)?"'%s'":", '%s'", item->identifier);
	}

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

int BPy_reports_to_error(ReportList *reports)
{
	char *report_str;

	report_str= BKE_reports_string(reports, RPT_ERROR);

	if(report_str) {
		PyErr_SetString(PyExc_SystemError, report_str);
		MEM_freeN(report_str);
	}

	return (report_str != NULL);
}


int BPy_errors_to_report(ReportList *reports)
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
int PyC_AsArray(void *array, PyObject *value, int length, PyTypeObject *type, char *error_prefix)
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
		PyErr_Format(PyExc_TypeError, "%s: invalid sequence length. expected %d, got %d.", error_prefix, length, value_len);
		return -1;
	}

	/* for each type */
	if(type == &PyFloat_Type) {
		float *array_float= array;
		for(i=0; i<length; i++) {
			array_float[i] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value_fast, i));
		}
	}
	else if(type == &PyLong_Type) {
		int *array_int= array;
		for(i=0; i<length; i++) {
			array_int[i] = PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value_fast, i));
		}
	}
	else if(type == &PyBool_Type) {
		int *array_bool= array;
		for(i=0; i<length; i++) {
			array_bool[i] = (PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value_fast, i)) != 0);
		}
	}
	else {
		Py_DECREF(value_fast);
		PyErr_Format(PyExc_TypeError, "%s: internal error %s is invalid.", error_prefix, type->tp_name);
		return -1;
	}

	Py_DECREF(value_fast);

	if(PyErr_Occurred()) {
		PyErr_Format(PyExc_TypeError, "%s: one or more items could not be used as a %s.", error_prefix, type->tp_name);
		return -1;
	}

	return 0;
}
