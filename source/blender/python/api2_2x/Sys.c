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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "Sys.h"

static PyObject *g_sysmodule = Py_None; /* pointer to Blender.sys module */

PyObject *sys_Init (void)
{
  PyObject  *submodule, *dict, *sep;

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

static PyObject *M_sys_dirname (PyObject *self, PyObject *args)
{
  PyObject *c;

  char *name, dirname[256];
  char sep;
  int n;

  if (!PyArg_ParseTuple(args, "s", &name))
    return EXPP_ReturnPyObjError (PyExc_TypeError,
              "expected string argument");

  c = PyObject_GetAttrString (g_sysmodule, "dirsep");
  sep = PyString_AsString(c)[0];
  Py_DECREF(c);

  n = strrchr(name, sep) - name;
  if (n > 255) {
    PyErr_SetString(PyExc_RuntimeError, "path too long");
    return 0;
  }

  strncpy(dirname, name, n);
  dirname[n] = 0;

  return Py_BuildValue("s", dirname);
}
