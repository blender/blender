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

/* This submodule was introduced as a way to preserve configured data in
 * scripts.  A very simple idea: the script writer saves this data in a dict
 * and registers this dict in the "Registry" dict.  This way we can discard
 * the global interpreter dictionary after a script is executed, since the
 * data meant to be kept was copied to the Registry elsewhere.  The current
 * implementation is naive: scripts can deliberately mess with data saved by
 * other scripts.  This is so new script versions can delete older entries, if
 * they need to.  XXX Or should we block this? */

#ifndef EXPP_REGISTRY_H
#define EXPP_REGISTRY_H

#include <Python.h>
#include <stdio.h>

#include "gen_utils.h"
#include "modules.h"

/* the Registry dictionary, declare here, defined in ../BPY_interface.c */
PyObject *bpy_registryDict = NULL;

/*****************************************************************************/
/* Python API function prototypes for the Registry module.                   */
/*****************************************************************************/
static PyObject *M_Registry_Keys (PyObject *self);
static PyObject *M_Registry_GetKey (PyObject *self, PyObject *args);
static PyObject *M_Registry_SetKey (PyObject *self, PyObject *args);
static PyObject *M_Registry_RemoveKey (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Registry.__doc__                                                  */
/*****************************************************************************/
char M_Registry_doc[] =
"The Blender Registry module (persistent data cache)\n\n\
    Use this module to store configuration data that a script can reload\n\
    when it is executed again.\n";

char M_Registry_Keys_doc[] =
"() - Get all keys in the Registry dictionary.\n\n\
    Each key references another dict with saved data from a specific script.\n";

char M_Registry_GetKey_doc[] =
"(name) - Get a specific entry (dict) from the Registry dictionary\n\
 (name) - a string that references a specific script.\n";

char M_Registry_SetKey_doc[] =
"(key, dict) - Store an entry in the Registry dictionary.\n\
    If an entry with the same 'key' already exists, it is substituted.\n\
 (key) - the string to use as a key for the dict being saved.\n\
 (dict) - a dictionary with the data to be stored.\n";

char M_Registry_RemoveKey_doc[] =
"(key) - Remove the dict with key 'key' from the Registry.\n";

/*****************************************************************************/
/* Python method structure definition for Blender.Registry module:           */
/*****************************************************************************/
struct PyMethodDef M_Registry_methods[] = {
  {"Keys", (PyCFunction)M_Registry_Keys, METH_VARARGS, M_Registry_Keys_doc},
  {"GetKey", M_Registry_GetKey, METH_VARARGS, M_Registry_GetKey_doc},
  {"SetKey", M_Registry_SetKey, METH_VARARGS, M_Registry_SetKey_doc},
  {"RemoveKey", M_Registry_RemoveKey, METH_VARARGS, M_Registry_RemoveKey_doc},
  {NULL, NULL, 0, NULL}
};

#endif /* EXPP_REGISTRY_H */
