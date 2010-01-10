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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_listBase.h"
#include "RNA_access.h"
#include "bpy_util.h"
#include "BLI_dynstr.h"
#include "MEM_guardedalloc.h"
#include "BKE_report.h"


#include "BKE_context.h"
bContext*	__py_context = NULL;
bContext*	BPy_GetContext(void) { return __py_context; };
void		BPy_SetContext(bContext *C) { __py_context= C; };

/* for debugging */
void PyObSpit(char *name, PyObject *var) {
	fprintf(stderr, "<%s> : ", name);
	if (var==NULL) {
		fprintf(stderr, "<NIL>");
	}
	else {
		PyObject_Print(var, stderr, 0);
		fprintf(stderr, " ref:%d ", (int)var->ob_refcnt);
		fprintf(stderr, " ptr:%p", (void *)var);
		
		fprintf(stderr, " type:");
		if(Py_TYPE(var))
			fprintf(stderr, "%s", Py_TYPE(var)->tp_name);
		else
			fprintf(stderr, "<NIL>");
	}
	fprintf(stderr, "\n");
}

void PyLineSpit(void) {
	char *filename;
	int lineno;

	PyErr_Clear();
	BPY_getFileAndNum(&filename, &lineno);
	
	fprintf(stderr, "%s:%d\n", filename, lineno);
}

void BPY_getFileAndNum(char **filename, int *lineno)
{
	PyObject *getframe, *frame;
	PyObject *f_lineno= NULL, *co_filename= NULL;
	
	if (filename)	*filename= NULL;
	if (lineno)		*lineno = -1;
	
	getframe = PySys_GetObject("_getframe"); // borrowed
	if (getframe==NULL) {
		PyErr_Clear();
		return;
	}
	
	frame = PyObject_CallObject(getframe, NULL);
	if (frame==NULL) {
		PyErr_Clear();
		return;
	}
	
	if (filename) {
		co_filename= PyObject_GetAttrStringArgs(frame, 1, "f_code", "co_filename");
		if (co_filename==NULL) {
			PyErr_SetString(PyExc_SystemError, "Could not access sys._getframe().f_code.co_filename");
			Py_DECREF(frame);
			return;
		}
		
		*filename = _PyUnicode_AsString(co_filename);
		Py_DECREF(co_filename);
	}
	
	if (lineno) {
		f_lineno= PyObject_GetAttrString(frame, "f_lineno");
		if (f_lineno==NULL) {
			PyErr_SetString(PyExc_SystemError, "Could not access sys._getframe().f_lineno");
			Py_DECREF(frame);
			return;
		}
		
		*lineno = (int)PyLong_AsSsize_t(f_lineno);
		Py_DECREF(f_lineno);
	}

	Py_DECREF(frame);
}

/* Would be nice if python had this built in */
PyObject *PyObject_GetAttrStringArgs(PyObject *o, Py_ssize_t n, ...)
{
	Py_ssize_t i;
	PyObject *item= o;
	char *attr;
	
	va_list vargs;

	va_start(vargs, n);
	for (i=0; i<n; i++) {
		attr = va_arg(vargs, char *);
		item = PyObject_GetAttrString(item, attr);
		
		if (item) 
			Py_DECREF(item);
		else /* python will set the error value here */
			break;
		
	}
	va_end(vargs);
	
	Py_XINCREF(item); /* final value has is increfed, to match PyObject_GetAttrString */
	return item;
}

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



/* returns the exception string as a new PyUnicode object, depends on external StringIO module */
PyObject *BPY_exception_buffer(void)
{
	PyObject *stdout_backup = PySys_GetObject("stdout"); /* borrowed */
	PyObject *stderr_backup = PySys_GetObject("stderr"); /* borrowed */
	PyObject *string_io = NULL;
	PyObject *string_io_buf = NULL;
	PyObject *string_io_mod= NULL;
	PyObject *string_io_getvalue= NULL;
	
	PyObject *error_type, *error_value, *error_traceback;
	
	if (!PyErr_Occurred())
		return NULL;
	
	PyErr_Fetch(&error_type, &error_value, &error_traceback);
	
	PyErr_Clear();
	
	/* import StringIO / io
	 * string_io = StringIO.StringIO()
	 */
	
	if(! (string_io_mod= PyImport_ImportModule("io")) ) {
		goto error_cleanup;
	} else if (! (string_io = PyObject_CallMethod(string_io_mod, "StringIO", NULL))) {
		goto error_cleanup;
	} else if (! (string_io_getvalue= PyObject_GetAttrString(string_io, "getvalue"))) {
		goto error_cleanup;
	}
	
	Py_INCREF(stdout_backup); // since these were borrowed we dont want them freed when replaced.
	Py_INCREF(stderr_backup);
	
	PySys_SetObject("stdout", string_io); // both of these are free'd when restoring
	PySys_SetObject("stderr", string_io);
	
	PyErr_Restore(error_type, error_value, error_traceback);
	PyErr_Print(); /* print the error */
	PyErr_Clear();
	
	string_io_buf = PyObject_CallObject(string_io_getvalue, NULL);
	
	PySys_SetObject("stdout", stdout_backup);
	PySys_SetObject("stderr", stderr_backup);
	
	Py_DECREF(stdout_backup); /* now sys owns the ref again */
	Py_DECREF(stderr_backup);
	
	Py_DECREF(string_io_mod);
	Py_DECREF(string_io_getvalue);
	Py_DECREF(string_io); /* free the original reference */
	
	PyErr_Clear();
	return string_io_buf;
	
	
error_cleanup:
	/* could not import the module so print the error and close */
	Py_XDECREF(string_io_mod);
	Py_XDECREF(string_io);
	
	PyErr_Restore(error_type, error_value, error_traceback);
	PyErr_Print(); /* print the error */
	PyErr_Clear();
	
	return NULL;
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

	char *filename;
	int lineno;

	if (!PyErr_Occurred())
		return 1;
	
	/* less hassle if we allow NULL */
	if(reports==NULL) {
		PyErr_Print();
		PyErr_Clear();
		return 1;
	}
	
	pystring= BPY_exception_buffer();
	
	if(pystring==NULL) {
		BKE_report(reports, RPT_ERROR, "unknown py-exception, could not convert");
		return 0;
	}
	
	BPY_getFileAndNum(&filename, &lineno);
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

