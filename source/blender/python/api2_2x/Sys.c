/* 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BKE_utildefines.h>
#include <BLI_blenlib.h>
#include <PIL_time.h>
#include <Python.h>
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python API function prototypes for the sys module.                        */
/*****************************************************************************/
static PyObject *M_sys_basename (PyObject *self, PyObject *args);
static PyObject *M_sys_dirname (PyObject *self, PyObject *args);
static PyObject *M_sys_splitext (PyObject *self, PyObject *args);
static PyObject *M_sys_exists (PyObject *self, PyObject *args);
static PyObject *M_sys_time (PyObject *self);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.sys.__doc__                                                       */
/*****************************************************************************/
static char M_sys_doc[] =
"The Blender.sys submodule\n\
\n\
This is a minimal system module to supply simple functionality available\n\
in the default Python module os.";

static char M_sys_basename_doc[] =
"(path) - Split 'path' in dir and filename.\n\
Return the filename.";

static char M_sys_dirname_doc[] =
"(path) - Split 'path' in dir and filename.\n\
Return the dir.";

static char M_sys_splitext_doc[] =
"(path) - Split 'path' in root and extension:\n\
/this/that/file.ext -> ('/this/that/file','.ext').\n\
Return the pair (root, extension).";

static char M_sys_time_doc[] =
"() - Return a float representing time elapsed in seconds.\n\
Each successive call is garanteed to return values greater than or\n\
equal to the previous call.";

static char M_sys_exists_doc[] =
"(path) - Return 1 if given pathname (file or dir) exists, 0 otherwise.";

/*****************************************************************************/
/* Python method structure definition for Blender.sys module:                */
/*****************************************************************************/
struct PyMethodDef M_sys_methods[] = {
  {"basename",    M_sys_basename,        METH_VARARGS, M_sys_basename_doc},
  {"dirname",     M_sys_dirname,         METH_VARARGS, M_sys_dirname_doc},
  {"splitext",    M_sys_splitext,        METH_VARARGS, M_sys_splitext_doc},
  {"exists",      M_sys_exists,          METH_VARARGS, M_sys_exists_doc},
  {"time", (PyCFunction)M_sys_time,      METH_NOARGS,  M_sys_time_doc},
  {NULL, NULL, 0, NULL}
};

/* Module Functions */

static PyObject *g_sysmodule = NULL; /* pointer to Blender.sys module */

PyObject *sys_Init (void)
{
	PyObject	*submodule, *dict, *sep;

	submodule = Py_InitModule3("Blender.sys", M_sys_methods, M_sys_doc);

	g_sysmodule = submodule;

	dict = PyModule_GetDict(submodule);

#ifdef WIN32
	sep = Py_BuildValue("s", "\\");
#else
	sep = Py_BuildValue("s", "/");
#endif

	if (sep) {
		Py_INCREF(sep);
		PyDict_SetItemString(dict, "dirsep" , sep);
		PyDict_SetItemString(dict, "sep" , sep);
	}

	return submodule;
}

static PyObject *M_sys_basename (PyObject *self, PyObject *args)
{
	PyObject *c;

	char *name, *p, basename[FILE_MAXFILE];
	char sep;
	int n, len;

	if (!PyArg_ParseTuple(args, "s", &name))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected string argument");

	len = strlen(name);

	c = PyObject_GetAttrString (g_sysmodule, "dirsep");
	sep = PyString_AsString(c)[0];
	Py_DECREF(c);

	p = strrchr(name, sep);

	if (p) {
		n = name + len - p - 1; /* - 1 because we don't want the sep */

		if (n > FILE_MAXFILE)
			return EXPP_ReturnPyObjError(PyExc_RuntimeError, "path too long");

		strncpy(basename, p+1, n); /* + 1 to skip the sep */
		basename[n] = 0;
		return Py_BuildValue("s", basename);
	}

	return Py_BuildValue("s", name);
}

static PyObject *M_sys_dirname (PyObject *self, PyObject *args)
{
	PyObject *c;

	char *name, *p, dirname[FILE_MAXDIR];
	char sep;
	int n;

	if (!PyArg_ParseTuple(args, "s", &name))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected string argument");

	c = PyObject_GetAttrString (g_sysmodule, "dirsep");
	sep = PyString_AsString(c)[0];
	Py_DECREF(c);

	p = strrchr(name, sep);

	if (p) {
		n = p - name;

		if (n > FILE_MAXDIR)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError, "path too long");

		strncpy(dirname, name, n);
		dirname[n] = 0;
		return Py_BuildValue("s", dirname);
	}

	return Py_BuildValue("s", "."); /* XXX need to fix this? (is crossplatform?)*/
}

static PyObject *M_sys_splitext (PyObject *self, PyObject *args)
{
	PyObject *c;

	char *name, *dot, *p, path[FILE_MAXFILE], ext[FILE_MAXFILE];
	char sep;
	int n, len;

	if (!PyArg_ParseTuple(args, "s", &name))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected string argument");

	len = strlen(name);

	c = PyObject_GetAttrString (g_sysmodule, "dirsep");
	sep = PyString_AsString(c)[0];
	Py_DECREF(c);

	dot = strrchr(name, '.');

	if (!dot) return Py_BuildValue("ss", name, "");

	p = strrchr(name, sep);

	if (p) {
		if (p > dot) return Py_BuildValue("ss", name, "");
	}

	n = name + len - dot;

	/* loong extensions are supported -- foolish, but Python's os.path.splitext
	 * supports them, so ... */
	if (n > FILE_MAXFILE || (len - n ) > FILE_MAXFILE)
		EXPP_ReturnPyObjError(PyExc_RuntimeError, "path too long");

	strncpy(ext, dot, n);
	ext[n] = 0;
	strncpy(path, name, dot - name);
	path[dot - name] = 0;

	return Py_BuildValue("ss", path, ext);
}

static PyObject *M_sys_time (PyObject *self)
{
	double t = PIL_check_seconds_timer();
	return Py_BuildValue("d", t);
}

static PyObject *M_sys_exists (PyObject *self, PyObject *args)
{
	char *fname = NULL;
	int i = 0;

	if (!PyArg_ParseTuple(args, "s", &fname))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected string (file path) argument");

	i = BLI_exists(fname);

	if (i) return Py_BuildValue("i", 1); /* path was found */

	return Py_BuildValue("i", 0); /* path doesn't exist */
}
