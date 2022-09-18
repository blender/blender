/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pygen
 */

/* Use a define instead of `#pragma once` because of `bmesh_py_types.h` */
#ifndef __PY_CAPI_UTILS_H__
#define __PY_CAPI_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_sys_types.h"
#include "BLI_utildefines_variadic.h"

void PyC_ObSpit(const char *name, PyObject *var);
/**
 * A version of #PyC_ObSpit that writes into a string (and doesn't take a name argument).
 * Use for logging.
 */
void PyC_ObSpitStr(char *result, size_t result_len, PyObject *var);
void PyC_LineSpit(void);
void PyC_StackSpit(void);
PyObject *PyC_ExceptionBuffer(void);
PyObject *PyC_ExceptionBuffer_Simple(void);
PyObject *PyC_Object_GetAttrStringArgs(PyObject *o, Py_ssize_t n, ...);
PyObject *PyC_FrozenSetFromStrings(const char **strings);

/**
 * Similar to #PyErr_Format(),
 *
 * Implementation - we can't actually prepend the existing exception,
 * because it could have _any_ arguments given to it, so instead we get its
 * `__str__` output and raise our own exception including it.
 */
PyObject *PyC_Err_Format_Prefix(PyObject *exception_type_prefix, const char *format, ...);
PyObject *PyC_Err_SetString_Prefix(PyObject *exception_type_prefix, const char *str);

/**
 * Use for Python callbacks run directly from C,
 * when we can't use normal methods of raising exceptions.
 */
void PyC_Err_PrintWithFunc(PyObject *py_func);

void PyC_FileAndNum(const char **r_filename, int *r_lineno);
void PyC_FileAndNum_Safe(const char **r_filename, int *r_lineno); /* checks python is running */
int PyC_AsArray_FAST(void *array,
                     size_t array_item_size,
                     PyObject *value_fast,
                     Py_ssize_t length,
                     const PyTypeObject *type,
                     const char *error_prefix);
int PyC_AsArray(void *array,
                size_t array_item_size,
                PyObject *value,
                Py_ssize_t length,
                const PyTypeObject *type,
                const char *error_prefix);

int PyC_AsArray_Multi_FAST(void *array,
                           size_t array_item_size,
                           PyObject *value_fast,
                           const int *dims,
                           int dims_len,
                           const PyTypeObject *type,
                           const char *error_prefix);

int PyC_AsArray_Multi(void *array,
                      size_t array_item_size,
                      PyObject *value,
                      const int *dims,
                      int dims_len,
                      const PyTypeObject *type,
                      const char *error_prefix);

PyObject *PyC_Tuple_PackArray_F32(const float *array, uint len);
PyObject *PyC_Tuple_PackArray_F64(const double *array, uint len);
PyObject *PyC_Tuple_PackArray_I32(const int *array, uint len);
PyObject *PyC_Tuple_PackArray_I32FromBool(const int *array, uint len);
PyObject *PyC_Tuple_PackArray_Bool(const bool *array, uint len);

#define PyC_Tuple_Pack_F32(...) \
  PyC_Tuple_PackArray_F32(((const float[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define PyC_Tuple_Pack_F64(...) \
  PyC_Tuple_PackArray_F64(((const double[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define PyC_Tuple_Pack_I32(...) \
  PyC_Tuple_PackArray_I32(((const int[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define PyC_Tuple_Pack_I32FromBool(...) \
  PyC_Tuple_PackArray_I32FromBool(((const int[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))
#define PyC_Tuple_Pack_Bool(...) \
  PyC_Tuple_PackArray_Bool(((const bool[]){__VA_ARGS__}), VA_NARGS_COUNT(__VA_ARGS__))

PyObject *PyC_Tuple_PackArray_Multi_F32(const float *array, const int dims[], int dims_len);
PyObject *PyC_Tuple_PackArray_Multi_F64(const double *array, const int dims[], int dims_len);
PyObject *PyC_Tuple_PackArray_Multi_I32(const int *array, const int dims[], int dims_len);
PyObject *PyC_Tuple_PackArray_Multi_Bool(const bool *array, const int dims[], int dims_len);

/**
 * Caller needs to ensure tuple is uninitialized.
 * Handy for filling a tuple with None for eg.
 */
void PyC_Tuple_Fill(PyObject *tuple, PyObject *value);
void PyC_List_Fill(PyObject *list, PyObject *value);

/* follow http://www.python.org/dev/peps/pep-0383/ */
PyObject *PyC_UnicodeFromByte(const char *str);
PyObject *PyC_UnicodeFromByteAndSize(const char *str, Py_ssize_t size);
const char *PyC_UnicodeAsByte(PyObject *py_str, PyObject **coerce); /* coerce must be NULL */
/**
 * String conversion, escape non-unicode chars
 * \param coerce: must be set to NULL.
 */
const char *PyC_UnicodeAsByteAndSize(PyObject *py_str, Py_ssize_t *size, PyObject **coerce);

/**
 * Description: This function creates a new Python dictionary object.
 * NOTE: dict is owned by sys.modules["__main__"] module, reference is borrowed
 * NOTE: important we use the dict from __main__, this is what python expects
 * for 'pickle' to work as well as strings like this...
 * >> foo = 10
 * >> print(__import__("__main__").foo)
 *
 * NOTE: this overwrites __main__ which gives problems with nested calls.
 * be sure to run PyC_MainModule_Backup & PyC_MainModule_Restore if there is
 * any chance that python is in the call stack.
 */
PyObject *PyC_DefaultNameSpace(const char *filename);
void PyC_RunQuicky(const char *filepath, int n, ...);
/**
 * Import `imports` into `py_dict`.
 *
 * \param py_dict: A Python dictionary, typically used as a name-space for script execution.
 * \param imports: A NULL terminated array of strings.
 * \return true when all modules import without errors, otherwise return false.
 * The caller is expected to handle the exception.
 */
bool PyC_NameSpace_ImportArray(PyObject *py_dict, const char *imports[]);

/**
 * #PyC_MainModule_Restore MUST be called after #PyC_MainModule_Backup.
 */
void PyC_MainModule_Backup(PyObject **r_main_mod);
void PyC_MainModule_Restore(PyObject *main_mod);

bool PyC_IsInterpreterActive(void);

void *PyC_RNA_AsPointer(PyObject *value, const char *type_name);

/* flag / set --- interchange */
typedef struct PyC_FlagSet {
  int value;
  const char *identifier;
} PyC_FlagSet;

PyObject *PyC_FlagSet_AsString(const PyC_FlagSet *item);
int PyC_FlagSet_ValueFromID_int(const PyC_FlagSet *item, const char *identifier, int *r_value);
int PyC_FlagSet_ValueFromID(const PyC_FlagSet *item,
                            const char *identifier,
                            int *r_value,
                            const char *error_prefix);
int PyC_FlagSet_ToBitfield(const PyC_FlagSet *items,
                           PyObject *value,
                           int *r_value,
                           const char *error_prefix);
PyObject *PyC_FlagSet_FromBitfield(PyC_FlagSet *items, int flag);

/**
 * \return success
 *
 * \note it is caller's responsibility to acquire & release GIL!
 */
bool PyC_RunString_AsNumber(const char **imports,
                            const char *expr,
                            const char *filename,
                            double *r_value);
bool PyC_RunString_AsIntPtr(const char **imports,
                            const char *expr,
                            const char *filename,
                            intptr_t *r_value);
bool PyC_RunString_AsStringAndSize(const char **imports,
                                   const char *expr,
                                   const char *filename,
                                   char **r_value,
                                   size_t *r_value_size);
bool PyC_RunString_AsString(const char **imports,
                            const char *expr,
                            const char *filename,
                            char **r_value);

/**
 * Use with PyArg_ParseTuple's "O&" formatting.
 *
 * \see #PyC_Long_AsBool for a similar function to use outside of argument parsing.
 */
int PyC_ParseBool(PyObject *o, void *p);

struct PyC_StringEnumItems {
  int value;
  const char *id;
};
struct PyC_StringEnum {
  const struct PyC_StringEnumItems *items;
  int value_found;
};

/**
 * Use with PyArg_ParseTuple's "O&" formatting.
 */
int PyC_ParseStringEnum(PyObject *o, void *p);
const char *PyC_StringEnum_FindIDFromValue(const struct PyC_StringEnumItems *items, int value);

int PyC_CheckArgs_DeepCopy(PyObject *args);

/* Integer parsing (with overflow checks), -1 on error. */
/**
 * Comparison with #PyObject_IsTrue
 * ================================
 *
 * Even though Python provides a way to retrieve the boolean value for an object,
 * in many cases it's far too relaxed, with the following examples coercing values.
 *
 * \code{.py}
 * data.value = "Text"    # True.
 * data.value = ""        # False.
 * data.value = {1, 2}    # True
 * data.value = {}        # False.
 * data.value = None      # False.
 * \endcode
 *
 * In practice this is often a mistake by the script author that doesn't behave as they expect.
 * So it's better to be more strict for attribute assignment and function arguments,
 * only accepting True/False 0/1.
 *
 * If coercing a value is desired, it can be done explicitly: `data.value = bool(value)`
 *
 * \see #PyC_ParseBool for use with #PyArg_ParseTuple and related functions.
 *
 * \note Don't use `bool` return type, so -1 can be used as an error value.
 */
int PyC_Long_AsBool(PyObject *value);
int8_t PyC_Long_AsI8(PyObject *value);
int16_t PyC_Long_AsI16(PyObject *value);
#if 0 /* inline */
int32_t PyC_Long_AsI32(PyObject *value);
int64_t PyC_Long_AsI64(PyObject *value);
#endif

uint8_t PyC_Long_AsU8(PyObject *value);
uint16_t PyC_Long_AsU16(PyObject *value);
uint32_t PyC_Long_AsU32(PyObject *value);
#if 0 /* inline */
uint64_t PyC_Long_AsU64(PyObject *value);
#endif

/* inline so type signatures match as expected */
Py_LOCAL_INLINE(int32_t) PyC_Long_AsI32(PyObject *value)
{
  return (int32_t)_PyLong_AsInt(value);
}
Py_LOCAL_INLINE(int64_t) PyC_Long_AsI64(PyObject *value)
{
  return (int64_t)PyLong_AsLongLong(value);
}
Py_LOCAL_INLINE(uint64_t) PyC_Long_AsU64(PyObject *value)
{
  return (uint64_t)PyLong_AsUnsignedLongLong(value);
}

/* utils for format string in `struct` module style syntax */
char PyC_StructFmt_type_from_str(const char *typestr);
bool PyC_StructFmt_type_is_float_any(char format);
bool PyC_StructFmt_type_is_int_any(char format);
bool PyC_StructFmt_type_is_byte(char format);
bool PyC_StructFmt_type_is_bool(char format);

#endif /* __PY_CAPI_UTILS_H__ */

#ifdef __cplusplus
}
#endif
