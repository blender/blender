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

#include "Registry.h"

/*****************************************************************************/
/* Function:              M_Registry_Keys                                    */
/* Python equivalent:     Blender.Registry.Keys                              */
/*****************************************************************************/
PyObject *M_Registry_Keys (PyObject *self)
{
  PyObject *pydict = NULL;

  if (!bpy_registryDict)
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
              "No Registry dictionary found!");

  pydict = PyDict_Keys (bpy_registryDict);

  if (!pydict)
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
              "Registry_Keys: couldn't get keys");

  return pydict;
}

/*****************************************************************************/
/* Function:              M_Registry_GetKey                                  */
/* Python equivalent:     Blender.Registry.GetKey                            */
/*****************************************************************************/
static PyObject *M_Registry_GetKey (PyObject *self, PyObject *args)
{
  PyObject *pyentry = NULL;
  PyObject *pydict = NULL;

  if (!bpy_registryDict)
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
              "No Registry dictionary found!");

  if (!PyArg_ParseTuple (args, "O!", &PyString_Type, &pyentry))
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected a string");

  pydict = PyDict_GetItem (bpy_registryDict, pyentry); /* borrowed ... */

  if (!pydict)
/*    return EXPP_ReturnPyObjError (PyExc_KeyError,
            "no such key in the Registry"); */
		pydict = Py_None; /* better to return None than an error */

  Py_INCREF (pydict); /* ... so we incref it */
  /* should we copy the dict instead? */
  return pydict;
}

/*****************************************************************************/
/* Function:              M_Registry_SetKey                                  */
/* Python equivalent:     Blender.Registry.SetKey                            */
/*****************************************************************************/
static PyObject *M_Registry_SetKey (PyObject *self, PyObject *args)
{
  PyObject *pystr = NULL;
  PyObject *pydict = NULL;

  if (!bpy_registryDict)
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
              "No Registry dictionary found!");

  if (!PyArg_ParseTuple (args, "O!O!",
                         &PyString_Type, &pystr, &PyDict_Type, &pydict))
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected a string and a dictionary");

  if (PyDict_SetItem (bpy_registryDict, pystr, pydict)) /* 0 on success */
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
            "Registry_SetKey: couldn't update the Registry dict");

  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************************/
/* Function:              M_Registry_RemoveKey                               */
/* Python equivalent:     Blender.Registry.RemoveKey                         */
/*****************************************************************************/
static PyObject *M_Registry_RemoveKey (PyObject *self, PyObject *args)
{
  PyObject *pystr = NULL;

  if (!bpy_registryDict)
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
              "No Registry dictionary found!");

  if (!PyArg_ParseTuple (args, "O!", &PyString_Type, &pystr)) 
    return EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected a string");

  if (PyDict_DelItem (bpy_registryDict, pystr)) /* returns 0 on success */
    return EXPP_ReturnPyObjError (PyExc_KeyError,
            "no such key in the Registry");

	Py_INCREF (Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:              Registry_Init                                      */
/*****************************************************************************/
PyObject *Registry_Init (void)
{
  PyObject  *submodule;

  submodule = Py_InitModule3("Blender.Registry", M_Registry_methods,
                  M_Registry_doc);

  return submodule;
}
