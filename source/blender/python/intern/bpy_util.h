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

#include <Python.h>

#ifndef BPY_UTIL_H
#define BPY_UTIL_H

#if PY_VERSION_HEX <  0x03010000
#error "Python versions below 3.1 are not supported anymore, you'll need to update your python."
#endif

#include "RNA_types.h" /* for EnumPropertyItem only */

struct EnumPropertyItem;
struct ReportList;

void PyObSpit(char *name, PyObject *var);
void PyLineSpit(void);
void BPY_getFileAndNum(char **filename, int *lineno);

PyObject *BPY_exception_buffer(void);

/* own python like utility function */
PyObject *PyObject_GetAttrStringArgs(PyObject *o, Py_ssize_t n, ...);



/* Class type checking, use for checking classes can be added as operators, panels etc */
typedef struct BPY_class_attr_check {
	const char	*name;		/* name of the class attribute */
    char		type;		/* 's' = string, 'f' = function, 'l' = list, (add as needed) */
    int			arg_count;	/* only for function types, -1 for undefined, includes self arg */
    int 		len;		/* only for string types currently */
	int			flag;		/* other options */
} BPY_class_attr_check;

/* BPY_class_attr_check, flag */
#define BPY_CLASS_ATTR_OPTIONAL 1
#define BPY_CLASS_ATTR_NONE_OK	2

int BPY_class_validate(const char *class_type, PyObject *class, PyObject *base_class, BPY_class_attr_check* class_attrs, PyObject **py_class_attrs);

char *BPy_enum_as_string(struct EnumPropertyItem *item);


#define BLANK_PYTHON_TYPE {PyVarObject_HEAD_INIT(NULL, 0) 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}

/* error reporting */
int BPy_reports_to_error(struct ReportList *reports);
int BPy_errors_to_report(struct ReportList *reports);

/* TODO - find a better solution! */
struct bContext *BPy_GetContext(void);
void BPy_SetContext(struct bContext *C);

extern void bpy_context_set(struct bContext *C, PyGILState_STATE *gilstate);
extern void bpy_context_clear(struct bContext *C, PyGILState_STATE *gilstate);

int BPyAsPrimitiveArray(void *array, PyObject *value, int length, PyTypeObject *type, char *error_prefix);
#endif
