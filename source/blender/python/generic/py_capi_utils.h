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

/** \file blender/python/generic/py_capi_utils.h
 *  \ingroup pygen
 */


#ifndef __PY_CAPI_UTILS_H__
#define __PY_CAPI_UTILS_H__

void			PyC_ObSpit(const char *name, PyObject *var);
void			PyC_LineSpit(void);
void			PyC_StackSpit(void);
PyObject *		PyC_ExceptionBuffer(void);
PyObject *		PyC_Object_GetAttrStringArgs(PyObject *o, Py_ssize_t n, ...);
PyObject *		PyC_FrozenSetFromStrings(const char **strings);
PyObject *		PyC_Err_Format_Prefix(PyObject *exception_type_prefix, const char *format, ...);
void			PyC_FileAndNum(const char **filename, int *lineno);
void			PyC_FileAndNum_Safe(const char **filename, int *lineno); /* checks python is running */
int				PyC_AsArray(void *array, PyObject *value, const Py_ssize_t length,
                            const PyTypeObject *type, const bool is_double, const char *error_prefix);
PyObject *      PyC_FromArray(const void *array, int length, const PyTypeObject *type,
                              const bool is_double, const char *error_prefix);
void            PyC_Tuple_Fill(PyObject *tuple, PyObject *value);

/* follow http://www.python.org/dev/peps/pep-0383/ */
PyObject *      PyC_UnicodeFromByte(const char *str);
PyObject *      PyC_UnicodeFromByteAndSize(const char *str, Py_ssize_t size);
const char *    PyC_UnicodeAsByte(PyObject *py_str, PyObject **coerce); /* coerce must be NULL */

/* name namespace function for bpy & bge */
PyObject *		PyC_DefaultNameSpace(const char *filename);
void			PyC_RunQuicky(const char *filepath, int n, ...);

void PyC_MainModule_Backup(PyObject **main_mod);
void PyC_MainModule_Restore(PyObject *main_mod);

void PyC_SetHomePath(const char *py_path_bundle);

bool PyC_IsInterpreterActive(void);

void *PyC_RNA_AsPointer(PyObject *value, const char *type_name);

/* flag / set --- interchange */
typedef struct PyC_FlagSet {
	int value;
	const char *identifier;
} PyC_FlagSet;

char     *PyC_FlagSet_AsString(PyC_FlagSet *item);
int       PyC_FlagSet_ValueFromID_int(PyC_FlagSet *item, const char *identifier, int *value);
int       PyC_FlagSet_ValueFromID(PyC_FlagSet *item, const char *identifier, int *value, const char *error_prefix);
int       PyC_FlagSet_ToBitfield(PyC_FlagSet *items, PyObject *value, int *r_value, const char *error_prefix);
PyObject *PyC_FlagSet_FromBitfield(PyC_FlagSet *items, int flag);

int PyC_RunString_AsNumber(const char *expr, double *value, const char *filename);

#endif  /* __PY_CAPI_UTILS_H__ */
