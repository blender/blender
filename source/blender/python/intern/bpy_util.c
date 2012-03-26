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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_util.c
 *  \ingroup pythonintern
 *
 * This file contains blender/python utility functions for the api's internal
 * use (unrelated to 'bpy.utils')
 */

#include <Python.h>

#include "bpy_util.h"
#include "BLI_dynstr.h"
#include "MEM_guardedalloc.h"
#include "BKE_report.h"
#include "BKE_context.h"

#include "../generic/py_capi_utils.h"

static bContext*  __py_context = NULL;
bContext   *BPy_GetContext(void) { return __py_context; }
void        BPy_SetContext(bContext *C) { __py_context = C; }

char *BPy_enum_as_string(EnumPropertyItem *item)
{
	DynStr *dynstr = BLI_dynstr_new();
	EnumPropertyItem *e;
	char *cstring;

	for (e = item; item->identifier; item++) {
		if (item->identifier[0])
			BLI_dynstr_appendf(dynstr, (e == item) ? "'%s'" : ", '%s'", item->identifier);
	}

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

short BPy_reports_to_error(ReportList *reports, PyObject *exception, const short clear)
{
	char *report_str;

	report_str = BKE_reports_string(reports, RPT_ERROR);

	if (clear) {
		BKE_reports_clear(reports);
	}

	if (report_str) {
		PyErr_SetString(exception, report_str);
		MEM_freeN(report_str);
	}

	return (report_str == NULL) ? 0 : -1;
}


short BPy_errors_to_report(ReportList *reports)
{
	PyObject *pystring;
	PyObject *pystring_format = NULL; // workaround, see below
	char *cstring;

	const char *filename;
	int lineno;

	if (!PyErr_Occurred())
		return 1;
	
	/* less hassle if we allow NULL */
	if (reports == NULL) {
		PyErr_Print();
		PyErr_Clear();
		return 1;
	}
	
	pystring = PyC_ExceptionBuffer();
	
	if (pystring == NULL) {
		BKE_report(reports, RPT_ERROR, "unknown py-exception, couldn't convert");
		return 0;
	}
	
	PyC_FileAndNum(&filename, &lineno);
	if (filename == NULL)
		filename = "<unknown location>";
	
	cstring = _PyUnicode_AsString(pystring);

#if 0 // ARG!. workaround for a bug in blenders use of vsnprintf
	BKE_reportf(reports, RPT_ERROR, "%s\nlocation:%s:%d\n", cstring, filename, lineno);
#else
	pystring_format = PyUnicode_FromFormat("%s\nlocation:%s:%d\n", cstring, filename, lineno);
	cstring = _PyUnicode_AsString(pystring_format);
	BKE_report(reports, RPT_ERROR, cstring);
#endif
	
	fprintf(stderr, "%s\nlocation:%s:%d\n", cstring, filename, lineno); // not exactly needed. just for testing
	
	Py_DECREF(pystring);
	Py_DECREF(pystring_format); // workaround
	return 1;
}
