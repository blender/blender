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
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <Python.h>
#include <stdio.h>

/*****************************************************************************/
/* Python API function prototypes for the Blender module.                    */
/*****************************************************************************/
PyObject *Object_New(PyObject *self, PyObject *args);
PyObject *Object_Get(PyObject *self, PyObject *args);
PyObject *Object_GetSelected (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.__doc__                                                           */
/*****************************************************************************/
char Object_New_doc[] =
"(type) - Add a new object of type 'type' in the current scene";

char Object_Get_doc[] =
"(name) - return the object with the name 'name', returns None if not\
	found.\n\
	If 'name' is not specified, it returns a list of all objects in the\n\
	current scene.";

char Object_GetSelected_doc[] =
"() - Returns a list of selected Objects in the active layer(s)\n\
The active object is the first in the list, if visible";

/*****************************************************************************/
/* Python method structure definition.                                       */
/*****************************************************************************/
struct PyMethodDef Object_methods[] = {
	{"New",         Object_New,         METH_VARARGS, Object_New_doc},
	{"Get",         Object_Get,         METH_VARARGS, Object_Get_doc},
	{"get",         Object_Get,         METH_VARARGS, Object_Get_doc},
	{"getSelected", Object_GetSelected, METH_VARARGS, Object_GetSelected_doc},
	{NULL, NULL}

};

/*****************************************************************************/
/* Function:              Object_New                                         */
/* Python equivalent:     Blender.Object.New                                 */
/*****************************************************************************/
PyObject *Object_New(PyObject *self, PyObject *args)
{
	printf ("In Object_New()\n");
	return (Py_None);
}

/*****************************************************************************/
/* Function:              Object_Get                                         */
/* Python equivalent:     Blender.Object.Get                                 */
/*****************************************************************************/
PyObject *Object_Get(PyObject *self, PyObject *args)
{
	printf ("In Object_Get()\n");

	return (Py_None);
}

/*****************************************************************************/
/* Function:              Object_GetSelected                                 */
/* Python equivalent:     Blender.Object.getSelected                         */
/*****************************************************************************/
PyObject *Object_GetSelected (PyObject *self, PyObject *args)
{
	printf ("In Object_GetSelected()\n");

	return (Py_None);
}

/*****************************************************************************/
/* Function:              initObject                                         */
/*****************************************************************************/
PyObject *initObject (void)
{
	PyObject	* module;
	PyObject	* dict;

	printf ("In initObject()\n");

	module = Py_InitModule("Blender.Object", Object_methods);

	return (module);
}

