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

#ifndef EXPP_sys_H
#define EXPP_sys_H

#include <Python.h>
#include <BLI_blenlib.h> /* for BLI_last_slash() */
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python API function prototypes for the sys module.                        */
/*****************************************************************************/
static PyObject *M_sys_basename (PyObject *self, PyObject *args);
static PyObject *M_sys_dirname (PyObject *self, PyObject *args);
static PyObject *M_sys_splitext (PyObject *self, PyObject *args);
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

static char M_sys_basename_doc[]="(path) - Split 'path' in dir and filename.\n\
Return the filename.";

static char M_sys_dirname_doc[]="(path) - Split 'path' in dir and filename.\n\
Return the dir.";

static char M_sys_splitext_doc[]="(path) - Split 'path' in root and \
extension:\n/this/that/file.ext -> ('/this/that/file','.ext').\n\
Return the pair (root, extension).";

static char M_sys_time_doc[]="() - Return a float representing time elapsed \
in seconds.\n\
Each successive call is garanteed to return values greater than or\n\
equal to the previous call.";

/*****************************************************************************/
/* Python method structure definition for Blender.sys module:                */
/*****************************************************************************/
struct PyMethodDef M_sys_methods[] = {
  {"basename",    M_sys_basename,        METH_VARARGS, M_sys_basename_doc},
  {"dirname",     M_sys_dirname,         METH_VARARGS, M_sys_dirname_doc},
  {"splitext",    M_sys_splitext,        METH_VARARGS, M_sys_splitext_doc},
  {"time", (PyCFunction)M_sys_time,      METH_NOARGS,  M_sys_time_doc},
  {NULL, NULL, 0, NULL}
};

#endif /* EXPP_sys_H */
