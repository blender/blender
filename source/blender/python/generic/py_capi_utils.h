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

#include "BLI_sys_types.h"
#include "BLI_utildefines_variadic.h"

void      PyC_ObSpit(const char *name, PyObject *var);
void      PyC_ObSpitStr(char *result, size_t result_len, PyObject *var);
void      PyC_LineSpit(void);
void      PyC_StackSpit(void);
PyObject *PyC_ExceptionBuffer(void);
PyObject *PyC_ExceptionBuffer_Simple(void);
PyObject *PyC_Object_GetAttrStringArgs(PyObject *o, Py_ssize_t n, ...);
PyObject *PyC_FrozenSetFromStrings(const char **strings);
PyObject *PyC_Err_Format_Prefix(PyObject *exception_type_prefix, const char *format, ...);
void      PyC_Err_PrintWithFunc(PyObject *py_func);

void	PyC_FileAndNum(const char **filename, int *lineno);
void	PyC_FileAndNum_Safe(const char **filename, int *lineno); /* checks python is running */
int             PyC_AsArray_FAST(
        void *array, PyObject *value_fast, const Py_ssize_t length,
        const PyTypeObject *type, const bool is_double, const char *error_prefix);
int             PyC_AsArray(
        void *array, PyObject *value, const Py_ssize_t length,
        const PyTypeObject *type, const bool is_double, const char *error_prefix);

PyObject       *PyC_Tuple_PackArray_F32(const float *array, uint len);
PyObject       *PyC_Tuple_PackArray_F64(const double *array, uint len);
PyObject       *PyC_Tuple_PackArray_I32(const int *array, uint len);
PyObject       *PyC_Tuple_PackArray_I32FromBool(const int *array, uint len);
PyObject       *PyC_Tuple_PackArray_Bool(const bool *array, uint len);

#define PyC_Tuple_Pack_F32(...) \
	PyC_Tuple_PackArray_F32(((const float []){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define PyC_Tuple_Pack_F64(...) \
	PyC_Tuple_PackArray_F64(((const double []){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define PyC_Tuple_Pack_I32(...) \
	PyC_Tuple_PackArray_I32(((const int []){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define PyC_Tuple_Pack_I32FromBool(...) \
	PyC_Tuple_PackArray_I32FromBool(((const int []){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define PyC_Tuple_Pack_Bool(...) \
	PyC_Tuple_PackArray_Bool(((const bool []){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))

void            PyC_Tuple_Fill(PyObject *tuple, PyObject *value);
void            PyC_List_Fill(PyObject *list, PyObject *value);

/* follow http://www.python.org/dev/peps/pep-0383/ */
PyObject   *PyC_UnicodeFromByte(const char *str);
PyObject   *PyC_UnicodeFromByteAndSize(const char *str, Py_ssize_t size);
const char *PyC_UnicodeAsByte(PyObject *py_str, PyObject **coerce); /* coerce must be NULL */
const char *PyC_UnicodeAsByteAndSize(PyObject *py_str, Py_ssize_t *size, PyObject **coerce);

/* name namespace function for bpy & bge */
PyObject *PyC_DefaultNameSpace(const char *filename);
void PyC_RunQuicky(const char *filepath, int n, ...);
bool PyC_NameSpace_ImportArray(PyObject *py_dict, const char *imports[]);

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
int       PyC_FlagSet_ValueFromID_int(PyC_FlagSet *item, const char *identifier, int *r_value);
int       PyC_FlagSet_ValueFromID(PyC_FlagSet *item, const char *identifier, int *r_value, const char *error_prefix);
int       PyC_FlagSet_ToBitfield(PyC_FlagSet *items, PyObject *value, int *r_value, const char *error_prefix);
PyObject *PyC_FlagSet_FromBitfield(PyC_FlagSet *items, int flag);

bool PyC_RunString_AsNumber(const char **imports, const char *expr, const char *filename, double *r_value);
bool PyC_RunString_AsIntPtr(const char **imports, const char *expr, const char *filename, intptr_t *r_value);
bool PyC_RunString_AsString(const char **imports, const char *expr, const char *filename, char **r_value);

int PyC_ParseBool(PyObject *o, void *p);

int PyC_CheckArgs_DeepCopy(PyObject *args);

/* Integer parsing (with overflow checks), -1 on error. */
int     PyC_Long_AsBool(PyObject *value);
int8_t  PyC_Long_AsI8(PyObject *value);
int16_t PyC_Long_AsI16(PyObject *value);
#if 0 /* inline */
int32_t PyC_Long_AsI32(PyObject *value);
int64_t PyC_Long_AsI64(PyObject *value);
#endif

uint8_t  PyC_Long_AsU8(PyObject *value);
uint16_t PyC_Long_AsU16(PyObject *value);
uint32_t PyC_Long_AsU32(PyObject *value);
#if 0 /* inline */
uint64_t PyC_Long_AsU64(PyObject *value);
#endif

/* inline so type signatures match as expected */
Py_LOCAL_INLINE(int32_t)  PyC_Long_AsI32(PyObject *value) { return (int32_t)_PyLong_AsInt(value); }
Py_LOCAL_INLINE(int64_t)  PyC_Long_AsI64(PyObject *value) { return (int64_t)PyLong_AsLongLong(value); }
Py_LOCAL_INLINE(uint64_t) PyC_Long_AsU64(PyObject *value) { return (uint64_t)PyLong_AsUnsignedLongLong(value); }

#endif  /* __PY_CAPI_UTILS_H__ */
