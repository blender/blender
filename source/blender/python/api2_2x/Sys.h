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
static PyObject *M_sys_dirname (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.sys.__doc__                                                       */
/*****************************************************************************/
static char M_sys_doc[] =
"The Blender.sys submodule\n\
\n\
This is a minimal sys module kept for compatibility.  It may also still be\n\
useful for users without full Python installations.\n";

static char M_sys_dirname_doc[] = "";

/*****************************************************************************/
/* Python method structure definition for Blender.sys module:                */
/*****************************************************************************/
struct PyMethodDef M_sys_methods[] = {
  {"dirname",     M_sys_dirname,         METH_VARARGS, M_sys_dirname_doc},
  {NULL, NULL, 0, NULL}
};

#endif /* EXPP_sys_H */
