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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/py_capi_utils.c
 *  \ingroup pygen
 *
 * Extend upon CPython's API, filling in some gaps, these functions use PyC_
 * prefix to distinguish them apart from CPython.
 *
 * \note
 * This module should only depend on CPython, however it currently uses
 * BLI_string_utf8() for unicode conversion.
 */

#include <Python.h>
#include <frameobject.h>

#include "BLI_utildefines.h"  /* for bool */

#include "py_capi_utils.h"

#include "python_utildefines.h"

#include "BLI_string.h"

#ifndef MATH_STANDALONE
/* only for BLI_strncpy_wchar_from_utf8, should replace with py funcs but too late in release now */
#include "BLI_string_utf8.h"
#endif

#ifdef _WIN32
#include "BLI_path_util.h"  /* BLI_setenv() */
#include "BLI_math_base.h"  /* isfinite() */
#endif

/* array utility function */
int PyC_AsArray_FAST(
        void *array, PyObject *value_fast, const Py_ssize_t length,
        const PyTypeObject *type, const bool is_double, const char *error_prefix)
{
	const Py_ssize_t value_len = PySequence_Fast_GET_SIZE(value_fast);
	PyObject **value_fast_items = PySequence_Fast_ITEMS(value_fast);
	Py_ssize_t i;

	BLI_assert(PyList_Check(value_fast) || PyTuple_Check(value_fast));

	if (value_len != length) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s: invalid sequence length. expected %d, got %d",
		             error_prefix, length, value_len);
		return -1;
	}

	/* for each type */
	if (type == &PyFloat_Type) {
		if (is_double) {
			double *array_double = array;
			for (i = 0; i < length; i++) {
				array_double[i] = PyFloat_AsDouble(value_fast_items[i]);
			}
		}
		else {
			float *array_float = array;
			for (i = 0; i < length; i++) {
				array_float[i] = PyFloat_AsDouble(value_fast_items[i]);
			}
		}
	}
	else if (type == &PyLong_Type) {
		/* could use is_double for 'long int' but no use now */
		int *array_int = array;
		for (i = 0; i < length; i++) {
			array_int[i] = PyC_Long_AsI32(value_fast_items[i]);
		}
	}
	else if (type == &PyBool_Type) {
		bool *array_bool = array;
		for (i = 0; i < length; i++) {
			array_bool[i] = (PyLong_AsLong(value_fast_items[i]) != 0);
		}
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "%s: internal error %s is invalid",
		             error_prefix, type->tp_name);
		return -1;
	}

	if (PyErr_Occurred()) {
		PyErr_Format(PyExc_TypeError,
		             "%s: one or more items could not be used as a %s",
		             error_prefix, type->tp_name);
		return -1;
	}

	return 0;
}

int PyC_AsArray(
        void *array, PyObject *value, const Py_ssize_t length,
        const PyTypeObject *type, const bool is_double, const char *error_prefix)
{
	PyObject *value_fast;
	int ret;

	if (!(value_fast = PySequence_Fast(value, error_prefix))) {
		return -1;
	}

	ret = PyC_AsArray_FAST(array, value_fast, length, type, is_double, error_prefix);
	Py_DECREF(value_fast);
	return ret;
}

/* -------------------------------------------------------------------- */
/** \name Typed Tuple Packing
 *
 * \note See #PyC_Tuple_Pack_* macros that take multiple arguments.
 *
 * \{ */

/* array utility function */
PyObject *PyC_Tuple_PackArray_F32(const float *array, uint len)
{
	PyObject *tuple = PyTuple_New(len);
	for (uint i = 0; i < len; i++) {
		PyTuple_SET_ITEM(tuple, i, PyFloat_FromDouble(array[i]));
	}
	return tuple;
}

PyObject *PyC_Tuple_PackArray_F64(const double *array, uint len)
{
	PyObject *tuple = PyTuple_New(len);
	for (uint i = 0; i < len; i++) {
		PyTuple_SET_ITEM(tuple, i, PyFloat_FromDouble(array[i]));
	}
	return tuple;
}

PyObject *PyC_Tuple_PackArray_I32(const int *array, uint len)
{
	PyObject *tuple = PyTuple_New(len);
	for (uint i = 0; i < len; i++) {
		PyTuple_SET_ITEM(tuple, i, PyLong_FromLong(array[i]));
	}
	return tuple;
}

PyObject *PyC_Tuple_PackArray_I32FromBool(const int *array, uint len)
{
	PyObject *tuple = PyTuple_New(len);
	for (uint i = 0; i < len; i++) {
		PyTuple_SET_ITEM(tuple, i, PyBool_FromLong(array[i]));
	}
	return tuple;
}

PyObject *PyC_Tuple_PackArray_Bool(const bool *array, uint len)
{
	PyObject *tuple = PyTuple_New(len);
	for (uint i = 0; i < len; i++) {
		PyTuple_SET_ITEM(tuple, i, PyBool_FromLong(array[i]));
	}
	return tuple;
}

/** \} */

/**
 * Caller needs to ensure tuple is uninitialized.
 * Handy for filling a tuple with None for eg.
 */
void PyC_Tuple_Fill(PyObject *tuple, PyObject *value)
{
	unsigned int tot = PyTuple_GET_SIZE(tuple);
	unsigned int i;

	for (i = 0; i < tot; i++) {
		PyTuple_SET_ITEM(tuple, i, value);
		Py_INCREF(value);
	}
}

void PyC_List_Fill(PyObject *list, PyObject *value)
{
	unsigned int tot = PyList_GET_SIZE(list);
	unsigned int i;

	for (i = 0; i < tot; i++) {
		PyList_SET_ITEM(list, i, value);
		Py_INCREF(value);
	}
}

/**
 * Use with PyArg_ParseTuple's "O&" formatting.
 *
 * \see #PyC_Long_AsBool for a similar function to use outside of argument parsing.
 */
int PyC_ParseBool(PyObject *o, void *p)
{
	bool *bool_p = p;
	long value;
	if (((value = PyLong_AsLong(o)) == -1) || !ELEM(value, 0, 1)) {
		PyErr_Format(PyExc_ValueError,
		             "expected a bool or int (0/1), got %s",
		             Py_TYPE(o)->tp_name);
		return 0;
	}

	*bool_p = value ? true : false;
	return 1;
}

/* silly function, we dont use arg. just check its compatible with __deepcopy__ */
int PyC_CheckArgs_DeepCopy(PyObject *args)
{
	PyObject *dummy_pydict;
	return PyArg_ParseTuple(args, "|O!:__deepcopy__", &PyDict_Type, &dummy_pydict) != 0;
}

#ifndef MATH_STANDALONE

/* for debugging */
void PyC_ObSpit(const char *name, PyObject *var)
{
	const char *null_str = "<null>";
	fprintf(stderr, "<%s> : ", name);
	if (var == NULL) {
		fprintf(stderr, "%s\n", null_str);
	}
	else {
		PyObject_Print(var, stderr, 0);
		const PyTypeObject *type = Py_TYPE(var);
		fprintf(stderr,
		        " ref:%d, ptr:%p, type: %s\n",
		        (int)var->ob_refcnt, (void *)var, type ? type->tp_name : null_str);
	}
}

/**
 * A version of #PyC_ObSpit that writes into a string (and doesn't take a name argument).
 * Use for logging.
 */
void PyC_ObSpitStr(char *result, size_t result_len, PyObject *var)
{
	/* No name, creator of string can manage that. */
	const char *null_str = "<null>";
	if (var == NULL) {
		BLI_snprintf(result, result_len, "%s", null_str);
	}
	else {
		const PyTypeObject *type = Py_TYPE(var);
		PyObject *var_str = PyObject_Repr(var);
		if (var_str == NULL) {
			/* We could print error here, but this may be used for generating errors - so don't for now. */
			PyErr_Clear();
		}
		BLI_snprintf(
		        result, result_len,
		        " ref=%d, ptr=%p, type=%s, value=%.200s",
		        (int)var->ob_refcnt,
		        (void *)var,
		        type ? type->tp_name : null_str,
		        var_str ? _PyUnicode_AsString(var_str) : "<error>");
		if (var_str != NULL) {
			Py_DECREF(var_str);
		}
	}
}

void PyC_LineSpit(void)
{

	const char *filename;
	int lineno;

	/* Note, allow calling from outside python (RNA) */
	if (!PyC_IsInterpreterActive()) {
		fprintf(stderr, "python line lookup failed, interpreter inactive\n");
		return;
	}

	PyErr_Clear();
	PyC_FileAndNum(&filename, &lineno);

	fprintf(stderr, "%s:%d\n", filename, lineno);
}

void PyC_StackSpit(void)
{
	/* Note, allow calling from outside python (RNA) */
	if (!PyC_IsInterpreterActive()) {
		fprintf(stderr, "python line lookup failed, interpreter inactive\n");
		return;
	}
	else {
		/* lame but handy */
		PyGILState_STATE gilstate = PyGILState_Ensure();
		PyRun_SimpleString("__import__('traceback').print_stack()");
		PyGILState_Release(gilstate);
	}
}

void PyC_FileAndNum(const char **filename, int *lineno)
{
	PyFrameObject *frame;

	if (filename) *filename = NULL;
	if (lineno)   *lineno = -1;

	if (!(frame = PyThreadState_GET()->frame)) {
		return;
	}

	/* when executing a script */
	if (filename) {
		*filename = _PyUnicode_AsString(frame->f_code->co_filename);
	}

	/* when executing a module */
	if (filename && *filename == NULL) {
		/* try an alternative method to get the filename - module based
		 * references below are all borrowed (double checked) */
		PyObject *mod_name = PyDict_GetItemString(PyEval_GetGlobals(), "__name__");
		if (mod_name) {
			PyObject *mod = PyDict_GetItem(PyImport_GetModuleDict(), mod_name);
			if (mod) {
				PyObject *mod_file = PyModule_GetFilenameObject(mod);
				if (mod_file) {
					*filename = _PyUnicode_AsString(mod_name);
					Py_DECREF(mod_file);
				}
				else {
					PyErr_Clear();
				}
			}

			/* unlikely, fallback */
			if (*filename == NULL) {
				*filename = _PyUnicode_AsString(mod_name);
			}
		}
	}

	if (lineno) {
		*lineno = PyFrame_GetLineNumber(frame);
	}
}

void PyC_FileAndNum_Safe(const char **filename, int *lineno)
{
	if (!PyC_IsInterpreterActive()) {
		return;
	}

	PyC_FileAndNum(filename, lineno);
}

/* Would be nice if python had this built in */
PyObject *PyC_Object_GetAttrStringArgs(PyObject *o, Py_ssize_t n, ...)
{
	Py_ssize_t i;
	PyObject *item = o;
	const char *attr;

	va_list vargs;

	va_start(vargs, n);
	for (i = 0; i < n; i++) {
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

PyObject *PyC_FrozenSetFromStrings(const char **strings)
{
	const char **str;
	PyObject *ret;

	ret = PyFrozenSet_New(NULL);

	for (str = strings; *str; str++) {
		PyObject *py_str = PyUnicode_FromString(*str);
		PySet_Add(ret, py_str);
		Py_DECREF(py_str);
	}

	return ret;
}


/**
 * Similar to #PyErr_Format(),
 *
 * Implementation - we cant actually prepend the existing exception,
 * because it could have _any_ arguments given to it, so instead we get its
 * ``__str__`` output and raise our own exception including it.
 */
PyObject *PyC_Err_Format_Prefix(PyObject *exception_type_prefix, const char *format, ...)
{
	PyObject *error_value_prefix;
	va_list args;

	va_start(args, format);
	error_value_prefix = PyUnicode_FromFormatV(format, args); /* can fail and be NULL */
	va_end(args);

	if (PyErr_Occurred()) {
		PyObject *error_type, *error_value, *error_traceback;
		PyErr_Fetch(&error_type, &error_value, &error_traceback);
		PyErr_Format(exception_type_prefix,
		             "%S, %.200s(%S)",
		             error_value_prefix,
		             Py_TYPE(error_value)->tp_name,
		             error_value
		             );
	}
	else {
		PyErr_SetObject(exception_type_prefix,
		                error_value_prefix
		                );
	}

	Py_XDECREF(error_value_prefix);

	/* dumb to always return NULL but matches PyErr_Format */
	return NULL;
}

/**
 * Use for Python callbacks run directly from C,
 * when we can't use normal methods of raising exceptions.
 */
void PyC_Err_PrintWithFunc(PyObject *py_func)
{
	/* since we return to C code we can't leave the error */
	PyCodeObject *f_code = (PyCodeObject *)PyFunction_GET_CODE(py_func);
	PyErr_Print();
	PyErr_Clear();

	/* use py style error */
	fprintf(stderr, "File \"%s\", line %d, in %s\n",
	        _PyUnicode_AsString(f_code->co_filename),
	        f_code->co_firstlineno,
	        _PyUnicode_AsString(((PyFunctionObject *)py_func)->func_name)
	        );
}


/* returns the exception string as a new PyUnicode object, depends on external traceback module */
#if 0

/* this version uses traceback module but somehow fails on UI errors */

PyObject *PyC_ExceptionBuffer(void)
{
	PyObject *traceback_mod = NULL;
	PyObject *format_tb_func = NULL;
	PyObject *ret = NULL;

	if (!(traceback_mod = PyImport_ImportModule("traceback"))) {
		goto error_cleanup;
	}
	else if (!(format_tb_func = PyObject_GetAttrString(traceback_mod, "format_exc"))) {
		goto error_cleanup;
	}

	ret = PyObject_CallObject(format_tb_func, NULL);

	if (ret == Py_None) {
		Py_DECREF(ret);
		ret = NULL;
	}

error_cleanup:
	/* could not import the module so print the error and close */
	Py_XDECREF(traceback_mod);
	Py_XDECREF(format_tb_func);

	return ret;
}
#else /* verbose, non-threadsafe version */
PyObject *PyC_ExceptionBuffer(void)
{
	PyObject *stdout_backup = PySys_GetObject("stdout"); /* borrowed */
	PyObject *stderr_backup = PySys_GetObject("stderr"); /* borrowed */
	PyObject *string_io = NULL;
	PyObject *string_io_buf = NULL;
	PyObject *string_io_mod = NULL;
	PyObject *string_io_getvalue = NULL;

	PyObject *error_type, *error_value, *error_traceback;

	if (!PyErr_Occurred())
		return NULL;

	PyErr_Fetch(&error_type, &error_value, &error_traceback);

	PyErr_Clear();

	/* import io
	 * string_io = io.StringIO()
	 */

	if (!(string_io_mod = PyImport_ImportModule("io"))) {
		goto error_cleanup;
	}
	else if (!(string_io = PyObject_CallMethod(string_io_mod, "StringIO", NULL))) {
		goto error_cleanup;
	}
	else if (!(string_io_getvalue = PyObject_GetAttrString(string_io, "getvalue"))) {
		goto error_cleanup;
	}

	Py_INCREF(stdout_backup); // since these were borrowed we don't want them freed when replaced.
	Py_INCREF(stderr_backup);

	PySys_SetObject("stdout", string_io); // both of these are freed when restoring
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
#endif

PyObject *PyC_ExceptionBuffer_Simple(void)
{
	PyObject *string_io_buf;

	PyObject *error_type, *error_value, *error_traceback;

	if (!PyErr_Occurred())
		return NULL;

	PyErr_Fetch(&error_type, &error_value, &error_traceback);

	if (error_value == NULL) {
		return NULL;
	}

	string_io_buf = PyObject_Str(error_value);
	/* Python does this too */
	if (UNLIKELY(string_io_buf == NULL)) {
		string_io_buf = PyUnicode_FromFormat(
		        "<unprintable %s object>", Py_TYPE(error_value)->tp_name);
	}

	PyErr_Restore(error_type, error_value, error_traceback);

	PyErr_Print();
	PyErr_Clear();
	return string_io_buf;
}

/* string conversion, escape non-unicode chars, coerce must be set to NULL */
const char *PyC_UnicodeAsByteAndSize(PyObject *py_str, Py_ssize_t *size, PyObject **coerce)
{
	const char *result;

	result = _PyUnicode_AsStringAndSize(py_str, size);

	if (result) {
		/* 99% of the time this is enough but we better support non unicode
		 * chars since blender doesnt limit this */
		return result;
	}
	else {
		PyErr_Clear();

		if (PyBytes_Check(py_str)) {
			*size = PyBytes_GET_SIZE(py_str);
			return PyBytes_AS_STRING(py_str);
		}
		else if ((*coerce = PyUnicode_EncodeFSDefault(py_str))) {
			*size = PyBytes_GET_SIZE(*coerce);
			return PyBytes_AS_STRING(*coerce);
		}
		else {
			/* leave error raised from EncodeFS */
			return NULL;
		}
	}
}

const char *PyC_UnicodeAsByte(PyObject *py_str, PyObject **coerce)
{
	const char *result;

	result = _PyUnicode_AsString(py_str);

	if (result) {
		/* 99% of the time this is enough but we better support non unicode
		 * chars since blender doesnt limit this */
		return result;
	}
	else {
		PyErr_Clear();

		if (PyBytes_Check(py_str)) {
			return PyBytes_AS_STRING(py_str);
		}
		else if ((*coerce = PyUnicode_EncodeFSDefault(py_str))) {
			return PyBytes_AS_STRING(*coerce);
		}
		else {
			/* leave error raised from EncodeFS */
			return NULL;
		}
	}
}

PyObject *PyC_UnicodeFromByteAndSize(const char *str, Py_ssize_t size)
{
	PyObject *result = PyUnicode_FromStringAndSize(str, size);
	if (result) {
		/* 99% of the time this is enough but we better support non unicode
		 * chars since blender doesnt limit this */
		return result;
	}
	else {
		PyErr_Clear();
		/* this means paths will always be accessible once converted, on all OS's */
		result = PyUnicode_DecodeFSDefaultAndSize(str, size);
		return result;
	}
}

PyObject *PyC_UnicodeFromByte(const char *str)
{
	return PyC_UnicodeFromByteAndSize(str, strlen(str));
}

/*****************************************************************************
 * Description: This function creates a new Python dictionary object.
 * note: dict is owned by sys.modules["__main__"] module, reference is borrowed
 * note: important we use the dict from __main__, this is what python expects
 *  for 'pickle' to work as well as strings like this...
 * >> foo = 10
 * >> print(__import__("__main__").foo)
 *
 * note: this overwrites __main__ which gives problems with nested calls.
 * be sure to run PyC_MainModule_Backup & PyC_MainModule_Restore if there is
 * any chance that python is in the call stack.
 ****************************************************************************/
PyObject *PyC_DefaultNameSpace(const char *filename)
{
	PyInterpreterState *interp = PyThreadState_GET()->interp;
	PyObject *mod_main = PyModule_New("__main__");
	PyDict_SetItemString(interp->modules, "__main__", mod_main);
	Py_DECREF(mod_main); /* sys.modules owns now */
	PyModule_AddStringConstant(mod_main, "__name__", "__main__");
	if (filename) {
		/* __file__ mainly for nice UI'ness
		 * note: this wont map to a real file when executing text-blocks and buttons. */
		PyModule_AddObject(mod_main, "__file__", PyC_UnicodeFromByte(filename));
	}
	PyModule_AddObject(mod_main, "__builtins__", interp->builtins);
	Py_INCREF(interp->builtins); /* AddObject steals a reference */
	return PyModule_GetDict(mod_main);
}

/* restore MUST be called after this */
void PyC_MainModule_Backup(PyObject **main_mod)
{
	PyInterpreterState *interp = PyThreadState_GET()->interp;
	*main_mod = PyDict_GetItemString(interp->modules, "__main__");
	Py_XINCREF(*main_mod); /* don't free */
}

void PyC_MainModule_Restore(PyObject *main_mod)
{
	PyInterpreterState *interp = PyThreadState_GET()->interp;
	PyDict_SetItemString(interp->modules, "__main__", main_mod);
	Py_XDECREF(main_mod);
}

/* must be called before Py_Initialize, expects output of BKE_appdir_folder_id(BLENDER_PYTHON, NULL) */
void PyC_SetHomePath(const char *py_path_bundle)
{
	if (py_path_bundle == NULL) {
		/* Common enough to have bundled *nix python but complain on OSX/Win */
#if defined(__APPLE__) || defined(_WIN32)
		fprintf(stderr, "Warning! bundled python not found and is expected on this platform. "
		        "(if you built with CMake: 'install' target may have not been built)\n");
#endif
		return;
	}
	/* set the environment path */
	printf("found bundled python: %s\n", py_path_bundle);

#ifdef __APPLE__
	/* OSX allow file/directory names to contain : character (represented as / in the Finder)
	 * but current Python lib (release 3.1.1) doesn't handle these correctly */
	if (strchr(py_path_bundle, ':'))
		printf("Warning : Blender application is located in a path containing : or / chars\
		       \nThis may make python import function fail\n");
#endif


#if 0 /* disable for now [#31506] - campbell */
#ifdef _WIN32
	/* cmake/MSVC debug build crashes without this, why only
	 * in this case is unknown.. */
	{
		/*BLI_setenv("PYTHONPATH", py_path_bundle)*/;
	}
#endif
#endif

	{
		static wchar_t py_path_bundle_wchar[1024];

		/* cant use this, on linux gives bug: #23018, TODO: try LANG="en_US.UTF-8" /usr/bin/blender, suggested 22008 */
		/* mbstowcs(py_path_bundle_wchar, py_path_bundle, FILE_MAXDIR); */

		BLI_strncpy_wchar_from_utf8(py_path_bundle_wchar, py_path_bundle, ARRAY_SIZE(py_path_bundle_wchar));

		Py_SetPythonHome(py_path_bundle_wchar);
		// printf("found python (wchar_t) '%ls'\n", py_path_bundle_wchar);
	}
}

bool PyC_IsInterpreterActive(void)
{
	/* instead of PyThreadState_Get, which calls Py_FatalError */
	return (PyThreadState_GetDict() != NULL);
}

/* Would be nice if python had this built in
 * See: https://wiki.blender.org/wiki/Tools/Debugging/PyFromC
 */
void PyC_RunQuicky(const char *filepath, int n, ...)
{
	FILE *fp = fopen(filepath, "r");

	if (fp) {
		PyGILState_STATE gilstate = PyGILState_Ensure();

		va_list vargs;

		int *sizes = PyMem_MALLOC(sizeof(int) * (n / 2));
		int i;

		PyObject *py_dict = PyC_DefaultNameSpace(filepath);
		PyObject *values = PyList_New(n / 2); /* namespace owns this, don't free */

		PyObject *py_result, *ret;

		PyObject *struct_mod = PyImport_ImportModule("struct");
		PyObject *calcsize = PyObject_GetAttrString(struct_mod, "calcsize"); /* struct.calcsize */
		PyObject *pack = PyObject_GetAttrString(struct_mod, "pack"); /* struct.pack */
		PyObject *unpack = PyObject_GetAttrString(struct_mod, "unpack"); /* struct.unpack */

		Py_DECREF(struct_mod);

		va_start(vargs, n);
		for (i = 0; i * 2 < n; i++) {
			const char *format = va_arg(vargs, char *);
			void *ptr = va_arg(vargs, void *);

			ret = PyObject_CallFunction(calcsize, "s", format);

			if (ret) {
				sizes[i] = PyLong_AsLong(ret);
				Py_DECREF(ret);
				ret = PyObject_CallFunction(unpack, "sy#", format, (char *)ptr, sizes[i]);
			}

			if (ret == NULL) {
				printf("%s error, line:%d\n", __func__, __LINE__);
				PyErr_Print();
				PyErr_Clear();

				PyList_SET_ITEM(values, i, Py_INCREF_RET(Py_None)); /* hold user */

				sizes[i] = 0;
			}
			else {
				if (PyTuple_GET_SIZE(ret) == 1) {
					/* convenience, convert single tuples into single values */
					PyObject *tmp = PyTuple_GET_ITEM(ret, 0);
					Py_INCREF(tmp);
					Py_DECREF(ret);
					ret = tmp;
				}

				PyList_SET_ITEM(values, i, ret); /* hold user */
			}
		}
		va_end(vargs);

		/* set the value so we can access it */
		PyDict_SetItemString(py_dict, "values", values);
		Py_DECREF(values);

		py_result = PyRun_File(fp, filepath, Py_file_input, py_dict, py_dict);

		fclose(fp);

		if (py_result) {

			/* we could skip this but then only slice assignment would work
			 * better not be so strict */
			values = PyDict_GetItemString(py_dict, "values");

			if (values && PyList_Check(values)) {

				/* don't use the result */
				Py_DECREF(py_result);
				py_result = NULL;

				/* now get the values back */
				va_start(vargs, n);
				for (i = 0; i * 2 < n; i++) {
					const char *format = va_arg(vargs, char *);
					void *ptr = va_arg(vargs, void *);

					PyObject *item;
					PyObject *item_new;
					/* prepend the string formatting and remake the tuple */
					item = PyList_GET_ITEM(values, i);
					if (PyTuple_CheckExact(item)) {
						int ofs = PyTuple_GET_SIZE(item);
						item_new = PyTuple_New(ofs + 1);
						while (ofs--) {
							PyObject *member = PyTuple_GET_ITEM(item, ofs);
							PyTuple_SET_ITEM(item_new, ofs + 1, member);
							Py_INCREF(member);
						}

						PyTuple_SET_ITEM(item_new, 0, PyUnicode_FromString(format));
					}
					else {
						item_new = Py_BuildValue("sO", format, item);
					}

					ret = PyObject_Call(pack, item_new, NULL);

					if (ret) {
						/* copy the bytes back into memory */
						memcpy(ptr, PyBytes_AS_STRING(ret), sizes[i]);
						Py_DECREF(ret);
					}
					else {
						printf("%s error on arg '%d', line:%d\n", __func__, i, __LINE__);
						PyC_ObSpit("failed converting:", item_new);
						PyErr_Print();
						PyErr_Clear();
					}

					Py_DECREF(item_new);
				}
				va_end(vargs);
			}
			else {
				printf("%s error, 'values' not a list, line:%d\n", __func__, __LINE__);
			}
		}
		else {
			printf("%s error line:%d\n", __func__, __LINE__);
			PyErr_Print();
			PyErr_Clear();
		}

		Py_DECREF(calcsize);
		Py_DECREF(pack);
		Py_DECREF(unpack);

		PyMem_FREE(sizes);

		PyGILState_Release(gilstate);
	}
	else {
		fprintf(stderr, "%s: '%s' missing\n", __func__, filepath);
	}
}

/* generic function to avoid depending on RNA */
void *PyC_RNA_AsPointer(PyObject *value, const char *type_name)
{
	PyObject *as_pointer;
	PyObject *pointer;

	if (STREQ(Py_TYPE(value)->tp_name, type_name) &&
	    (as_pointer = PyObject_GetAttrString(value, "as_pointer")) != NULL &&
	    PyCallable_Check(as_pointer))
	{
		void *result = NULL;

		/* must be a 'type_name' object */
		pointer = PyObject_CallObject(as_pointer, NULL);
		Py_DECREF(as_pointer);

		if (!pointer) {
			PyErr_SetString(PyExc_SystemError, "value.as_pointer() failed");
			return NULL;
		}
		result = PyLong_AsVoidPtr(pointer);
		Py_DECREF(pointer);
		if (!result) {
			PyErr_SetString(PyExc_SystemError, "value.as_pointer() failed");
		}

		return result;
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "expected '%.200s' type found '%.200s' instead",
		             type_name, Py_TYPE(value)->tp_name);
		return NULL;
	}
}


/* PyC_FlagSet_* functions - so flags/sets can be interchanged in a generic way */
#include "BLI_dynstr.h"
#include "MEM_guardedalloc.h"

char *PyC_FlagSet_AsString(PyC_FlagSet *item)
{
	DynStr *dynstr = BLI_dynstr_new();
	PyC_FlagSet *e;
	char *cstring;

	for (e = item; item->identifier; item++) {
		BLI_dynstr_appendf(dynstr, (e == item) ? "'%s'" : ", '%s'", item->identifier);
	}

	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

int PyC_FlagSet_ValueFromID_int(PyC_FlagSet *item, const char *identifier, int *r_value)
{
	for ( ; item->identifier; item++) {
		if (STREQ(item->identifier, identifier)) {
			*r_value = item->value;
			return 1;
		}
	}

	return 0;
}

int PyC_FlagSet_ValueFromID(PyC_FlagSet *item, const char *identifier, int *r_value, const char *error_prefix)
{
	if (PyC_FlagSet_ValueFromID_int(item, identifier, r_value) == 0) {
		const char *enum_str = PyC_FlagSet_AsString(item);
		PyErr_Format(PyExc_ValueError,
		             "%s: '%.200s' not found in (%s)",
		             error_prefix, identifier, enum_str);
		MEM_freeN((void *)enum_str);
		return -1;
	}

	return 0;
}

int PyC_FlagSet_ToBitfield(PyC_FlagSet *items, PyObject *value, int *r_value, const char *error_prefix)
{
	/* set of enum items, concatenate all values with OR */
	int ret, flag = 0;

	/* set looping */
	Py_ssize_t pos = 0;
	Py_ssize_t hash = 0;
	PyObject *key;

	if (!PySet_Check(value)) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s expected a set, not %.200s",
		             error_prefix, Py_TYPE(value)->tp_name);
		return -1;
	}

	*r_value = 0;

	while (_PySet_NextEntry(value, &pos, &key, &hash)) {
		const char *param = _PyUnicode_AsString(key);

		if (param == NULL) {
			PyErr_Format(PyExc_TypeError,
			             "%.200s set must contain strings, not %.200s",
			             error_prefix, Py_TYPE(key)->tp_name);
			return -1;
		}

		if (PyC_FlagSet_ValueFromID(items, param, &ret, error_prefix) < 0) {
			return -1;
		}

		flag |= ret;
	}

	*r_value = flag;
	return 0;
}

PyObject *PyC_FlagSet_FromBitfield(PyC_FlagSet *items, int flag)
{
	PyObject *ret = PySet_New(NULL);
	PyObject *pystr;

	for ( ; items->identifier; items++) {
		if (items->value & flag) {
			pystr = PyUnicode_FromString(items->identifier);
			PySet_Add(ret, pystr);
			Py_DECREF(pystr);
		}
	}

	return ret;
}


/**
 * \return success
 *
 * \note it is caller's responsibility to acquire & release GIL!
 */
bool PyC_RunString_AsNumber(const char *expr, const char *filename, double *r_value)
{
	PyObject *py_dict, *mod, *retval;
	bool ok = true;
	PyObject *main_mod = NULL;

	PyC_MainModule_Backup(&main_mod);

	py_dict = PyC_DefaultNameSpace(filename);

	mod = PyImport_ImportModule("math");
	if (mod) {
		PyDict_Merge(py_dict, PyModule_GetDict(mod), 0); /* 0 - don't overwrite existing values */
		Py_DECREF(mod);
	}
	else { /* highly unlikely but possibly */
		PyErr_Print();
		PyErr_Clear();
	}

	retval = PyRun_String(expr, Py_eval_input, py_dict, py_dict);

	if (retval == NULL) {
		ok = false;
	}
	else {
		double val;

		if (PyTuple_Check(retval)) {
			/* Users my have typed in 10km, 2m
			 * add up all values */
			int i;
			val = 0.0;

			for (i = 0; i < PyTuple_GET_SIZE(retval); i++) {
				const double val_item = PyFloat_AsDouble(PyTuple_GET_ITEM(retval, i));
				if (val_item == -1 && PyErr_Occurred()) {
					val = -1;
					break;
				}
				val += val_item;
			}
		}
		else {
			val = PyFloat_AsDouble(retval);
		}
		Py_DECREF(retval);

		if (val == -1 && PyErr_Occurred()) {
			ok = false;
		}
		else if (!isfinite(val)) {
			*r_value = 0.0;
		}
		else {
			*r_value = val;
		}
	}

	PyC_MainModule_Restore(main_mod);

	return ok;
}

bool PyC_RunString_AsString(const char *expr, const char *filename, char **r_value)
{
	PyObject *py_dict, *retval;
	bool ok = true;
	PyObject *main_mod = NULL;

	PyC_MainModule_Backup(&main_mod);

	py_dict = PyC_DefaultNameSpace(filename);

	retval = PyRun_String(expr, Py_eval_input, py_dict, py_dict);

	if (retval == NULL) {
		ok = false;
	}
	else {
		const char *val;
		Py_ssize_t val_len;

		val = _PyUnicode_AsStringAndSize(retval, &val_len);
		if (val == NULL && PyErr_Occurred()) {
			ok = false;
		}
		else {
			char *val_alloc = MEM_mallocN(val_len + 1, __func__);
			memcpy(val_alloc, val, val_len + 1);
			*r_value = val_alloc;
		}

		Py_DECREF(retval);
	}

	PyC_MainModule_Restore(main_mod);

	return ok;
}

#endif  /* #ifndef MATH_STANDALONE */

/* -------------------------------------------------------------------- */

/** \name Int Conversion
 *
 * \note Python doesn't provide overflow checks for specific bit-widths.
 *
 * \{ */

/* Compiler optimizes out redundant checks. */
#ifdef __GNUC__
#  pragma warning(push)
#  pragma GCC diagnostic ignored "-Wtype-limits"
#endif

/**
 * Don't use `bool` return type, so -1 can be used as an error value.
 */
int PyC_Long_AsBool(PyObject *value)
{
	int test = _PyLong_AsInt(value);
	if (UNLIKELY((uint)test > 1)) {
		PyErr_SetString(PyExc_TypeError,
		                "Python number not a bool (0/1)");
		return -1;
	}
	return test;
}

int8_t PyC_Long_AsI8(PyObject *value)
{
	int test = _PyLong_AsInt(value);
	if (UNLIKELY(test < INT8_MIN || test > INT8_MAX)) {
		PyErr_SetString(PyExc_OverflowError,
		                "Python int too large to convert to C int8");
		return -1;
	}
	return (int8_t)test;
}

int16_t PyC_Long_AsI16(PyObject *value)
{
	int test = _PyLong_AsInt(value);
	if (UNLIKELY(test < INT16_MIN || test > INT16_MAX)) {
		PyErr_SetString(PyExc_OverflowError,
		                "Python int too large to convert to C int16");
		return -1;
	}
	return (int16_t)test;
}

/* Inlined in header:
 * PyC_Long_AsI32
 * PyC_Long_AsI64
 */

uint8_t PyC_Long_AsU8(PyObject *value)
{
	ulong test = PyLong_AsUnsignedLong(value);
	if (UNLIKELY(test > UINT8_MAX)) {
		PyErr_SetString(PyExc_OverflowError,
		                "Python int too large to convert to C uint8");
		return (uint8_t)-1;
	}
	return (uint8_t)test;
}

uint16_t PyC_Long_AsU16(PyObject *value)
{
	ulong test = PyLong_AsUnsignedLong(value);
	if (UNLIKELY(test > UINT16_MAX)) {
		PyErr_SetString(PyExc_OverflowError,
		                "Python int too large to convert to C uint16");
		return (uint16_t)-1;
	}
	return (uint16_t)test;
}

uint32_t PyC_Long_AsU32(PyObject *value)
{
	ulong test = PyLong_AsUnsignedLong(value);
	if (UNLIKELY(test > UINT32_MAX)) {
		PyErr_SetString(PyExc_OverflowError,
		                "Python int too large to convert to C uint32");
		return (uint32_t)-1;
	}
	return (uint32_t)test;
}

/* Inlined in header:
 * PyC_Long_AsU64
 */

#ifdef __GNUC__
#  pragma warning(pop)
#endif

/** \} */
