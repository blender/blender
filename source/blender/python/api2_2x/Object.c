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

#include <BKE_global.h>
#include <BKE_main.h>
#include <DNA_ika_types.h>
#include <DNA_object_types.h>

#include "gen_utils.h"

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
/* Python BlenderObject structure definition.                                */
/*****************************************************************************/
typedef struct {
	PyObject_HEAD
	struct Object    *object;
} BlenObject;

/*****************************************************************************/
/* PythonTypeObject callback function prototypes                             */
/*****************************************************************************/
void ObjectDeAlloc (BlenObject *obj);
PyObject* ObjectGetAttr (BlenObject *obj, char *name);
int ObjectSetAttr (BlenObject *obj, char *name, PyObject *v);

/*****************************************************************************/
/* Python TypeObject structure definition.                                   */
/*****************************************************************************/
static PyTypeObject object_type =
{
	PyObject_HEAD_INIT(&PyType_Type)
	0,								/* ob_size */
	"Object",						/* tp_name */
	sizeof (BlenObject),			/* tp_basicsize */
	0,								/* tp_itemsize */
	/* methods */
	(destructor)ObjectDeAlloc,		/* tp_dealloc */
	0,								/* tp_print */
	(getattrfunc)ObjectGetAttr,		/* tp_getattr */
	(setattrfunc)ObjectSetAttr,		/* tp_setattr */
	0,								/* tp_compare */
	0,								/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_as_hash */
};

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
	char            * name;
	PyObject        * arg;
	struct Object	* object;
	BlenObject      * blen_object;

	printf ("In Object_Get()\n");

	if (!PyArg_ParseTuple(args, "O", &arg))
	{
		/* We expected a string as an argument, but we didn't get one. */
		return (PythonReturnErrorObject (PyExc_AttributeError,
					"expected string argument"));
	}

	if (!PyString_Check (arg))
	{
		return (PythonReturnErrorObject (PyExc_AttributeError,
					"expected string argument"));
	}

	name = PyString_AsString (arg);
	object = GetObjectByName (name);

	if (object == NULL)
	{
		/* No object exists with the name specified in the argument name. */
		return (PythonReturnErrorObject (PyExc_AttributeError,
					"Unknown object specified."));
	}
	blen_object = (BlenObject*)PyObject_NEW (BlenObject, &object_type); 
	blen_object->object = object;

	return ((PyObject*)blen_object);
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

	printf ("In initObject()\n");

	module = Py_InitModule("Object", Object_methods);

	return (module);
}

/*****************************************************************************/
/* Function:    ObjectCreatePyObject                                         */
/* Description: This function will create a new BlenObject from an existing  */
/*              Object structure.                                            */
/*****************************************************************************/
PyObject* ObjectCreatePyObject (struct Object *obj)
{
	BlenObject      * blen_object;

	printf ("In ObjectCreatePyObject\n");

	blen_object = (BlenObject*)PyObject_NEW
		(BlenObject,
		 &object_type);

	blen_object->object = obj;
	return ((PyObject*)blen_object);
}

/*****************************************************************************/
/* Function:    ObjectDeAlloc                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the destructor function.                                     */
/*****************************************************************************/
void ObjectDeAlloc (BlenObject *obj)
{
	PyObject_DEL (obj);
}

/*****************************************************************************/
/* Function:    ObjectGetAttr                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the function that retrieves any value from Blender and       */
/*              passes it to Python.                                         */
/*****************************************************************************/
PyObject* ObjectGetAttr (BlenObject *obj, char *name)
{
	struct Object   * object;
	struct Ika      * ika;

	object = obj->object;
	if (StringEqual (name, "LocX"))
		return (PyFloat_FromDouble(object->loc[0]));
	if (StringEqual (name, "LocY"))
		return (PyFloat_FromDouble(object->loc[1]));
	if (StringEqual (name, "LocZ"))
		return (PyFloat_FromDouble(object->loc[2]));
	if (StringEqual (name, "loc"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (Py_None);
	}
	if (StringEqual (name, "dLocX"))
		return (PyFloat_FromDouble(object->dloc[0]));
	if (StringEqual (name, "dLocY"))
		return (PyFloat_FromDouble(object->dloc[1]));
	if (StringEqual (name, "dLocZ"))
		return (PyFloat_FromDouble(object->dloc[2]));
	if (StringEqual (name, "dloc"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (Py_None);
	}
	if (StringEqual (name, "RotX"))
		return (PyFloat_FromDouble(object->rot[0]));
	if (StringEqual (name, "RotY"))
		return (PyFloat_FromDouble(object->rot[1]));
	if (StringEqual (name, "RotZ"))
		return (PyFloat_FromDouble(object->rot[2]));
	if (StringEqual (name, "rot"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (Py_None);
	}
	if (StringEqual (name, "dRotX"))
		return (PyFloat_FromDouble(object->drot[0]));
	if (StringEqual (name, "dRotY"))
		return (PyFloat_FromDouble(object->drot[1]));
	if (StringEqual (name, "dRotZ"))
		return (PyFloat_FromDouble(object->drot[2]));
	if (StringEqual (name, "drot"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (Py_None);
	}
	if (StringEqual (name, "SizeX"))
		return (PyFloat_FromDouble(object->size[0]));
	if (StringEqual (name, "SizeY"))
		return (PyFloat_FromDouble(object->size[1]));
	if (StringEqual (name, "SizeZ"))
		return (PyFloat_FromDouble(object->size[2]));
	if (StringEqual (name, "size"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (Py_None);
	}
	if (StringEqual (name, "dSizeX"))
		return (PyFloat_FromDouble(object->dsize[0]));
	if (StringEqual (name, "dSizeY"))
		return (PyFloat_FromDouble(object->dsize[1]));
	if (StringEqual (name, "dSizeZ"))
		return (PyFloat_FromDouble(object->dsize[2]));
	if (StringEqual (name, "dsize"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (Py_None);
	}
	if (strncmp (name,"Eff", 3) == 0)
	{
		if ( (object->type == OB_IKA) && (object->data != NULL) )
		{
			ika = object->data;
			switch (name[3])
			{
				case 'X':
					return (PyFloat_FromDouble (ika->effg[0]));
				case 'Y':
					return (PyFloat_FromDouble (ika->effg[1]));
				case 'Z':
					return (PyFloat_FromDouble (ika->effg[2]));
				default:
					/* Do we need to display a sensible error message here? */
					return (NULL);
			}
		}
		return (NULL);
	}
	if (StringEqual (name, "Layer"))
		return (PyInt_FromLong(object->lay));
	if (StringEqual (name, "parent"))
	{
		printf ("This is not implemented yet.\n");
		return (Py_None);
	}
	if (StringEqual (name, "track"))
	{
		printf ("This is not implemented yet.\n");
		return (Py_None);
	}
	if (StringEqual (name, "data"))
	{
		printf ("This is not implemented yet.\n");
		return (Py_None);
	}
	if (StringEqual (name, "ipo"))
	{
		printf ("This is not implemented yet.\n");
		return (Py_None);
	}
	if (StringEqual (name, "mat"))
	{
		printf ("This is not implemented yet. (matrix)\n");
		return (Py_None);
	}
	if (StringEqual (name, "matrix"))
	{
		printf ("This is not implemented yet. (matrix)\n");
		return (Py_None);
	}
	if (StringEqual (name, "colbits"))
	{
		printf ("This is not implemented yet.\n");
		return (Py_None);
	}
	if (StringEqual (name, "drawType"))
	{
		printf ("This is not implemented yet.\n");
		return (Py_None);
	}
	if (StringEqual (name, "drawMode"))
	{
		printf ("This is not implemented yet.\n");
		return (Py_None);
	}
	printf ("Unknown variable.\n");
	return (Py_None);
}

/*****************************************************************************/
/* Function:    ObjectSetAttr                                                */
/* Description: This is a callback function for the BlenObject type. It is   */
/*              the function that retrieves any value from Python and sets   */
/*              it accordingly in Blender.                                   */
/*****************************************************************************/
int ObjectSetAttr (BlenObject *obj, char *name, PyObject *value)
{
	struct Object	* object;
	struct Ika      * ika;

	object = obj->object;
	if (StringEqual (name, "LocX"))
		return (!PyArg_Parse (value, "f", &(object->loc[0])));
	if (StringEqual (name, "LocY"))
		return (!PyArg_Parse (value, "f", &(object->loc[1])));
	if (StringEqual (name, "LocZ"))
		return (!PyArg_Parse (value, "f", &(object->loc[2])));
	if (StringEqual (name, "loc"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (0);
	}
	if (StringEqual (name, "dLocX"))
		return (!PyArg_Parse (value, "f", &(object->dloc[0])));
	if (StringEqual (name, "dLocY"))
		return (!PyArg_Parse (value, "f", &(object->dloc[1])));
	if (StringEqual (name, "dLocZ"))
		return (!PyArg_Parse (value, "f", &(object->dloc[2])));
	if (StringEqual (name, "dloc"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (0);
	}
	if (StringEqual (name, "RotX"))
		return (!PyArg_Parse (value, "f", &(object->rot[0])));
	if (StringEqual (name, "RotY"))
		return (!PyArg_Parse (value, "f", &(object->rot[1])));
	if (StringEqual (name, "RotZ"))
		return (!PyArg_Parse (value, "f", &(object->rot[2])));
	if (StringEqual (name, "rot"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (0);
	}
	if (StringEqual (name, "dRotX"))
		return (!PyArg_Parse (value, "f", &(object->drot[0])));
	if (StringEqual (name, "dRotY"))
		return (!PyArg_Parse (value, "f", &(object->drot[1])));
	if (StringEqual (name, "dRotZ"))
		return (!PyArg_Parse (value, "f", &(object->drot[2])));
	if (StringEqual (name, "drot"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (0);
	}
	if (StringEqual (name, "SizeX"))
		return (!PyArg_Parse (value, "f", &(object->size[0])));
	if (StringEqual (name, "SizeY"))
		return (!PyArg_Parse (value, "f", &(object->size[1])));
	if (StringEqual (name, "SizeZ"))
		return (!PyArg_Parse (value, "f", &(object->size[2])));
	if (StringEqual (name, "size"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (0);
	}
	if (StringEqual (name, "dSizeX"))
		return (!PyArg_Parse (value, "f", &(object->dsize[0])));
	if (StringEqual (name, "dSizeY"))
		return (!PyArg_Parse (value, "f", &(object->dsize[1])));
	if (StringEqual (name, "dSizeZ"))
		return (!PyArg_Parse (value, "f", &(object->dsize[2])));
	if (StringEqual (name, "dsize"))
	{
		printf ("This is not implemented yet. (vector)\n");
		return (0);
	}
	if (strncmp (name,"Eff", 3) == 0)
	{
		if ( (object->type == OB_IKA) && (object->data != NULL) )
		{
			ika = object->data;
			switch (name[3])
			{
				case 'X':
					return (!PyArg_Parse (value, "f", &(ika->effg[0])));
				case 'Y':
					return (!PyArg_Parse (value, "f", &(ika->effg[1])));
				case 'Z':
					return (!PyArg_Parse (value, "f", &(ika->effg[2])));
				default:
					/* Do we need to display a sensible error message here? */
					return (0);
			}
		}
		return (0);
	}
	if (StringEqual (name, "Layer"))
		return (!PyArg_Parse (value, "i", &(object->lay)));
	if (StringEqual (name, "parent"))
	{
		printf ("This is not implemented yet.\n");
		return (1);
	}
	if (StringEqual (name, "track"))
	{
		printf ("This is not implemented yet.\n");
		return (1);
	}
	if (StringEqual (name, "data"))
	{
		printf ("This is not implemented yet.\n");
		return (1);
	}
	if (StringEqual (name, "ipo"))
	{
		printf ("This is not implemented yet.\n");
		return (1);
	}
	if (StringEqual (name, "mat"))
	{
		printf ("This is not implemented yet. (matrix)\n");
		return (1);
	}
	if (StringEqual (name, "matrix"))
	{
		printf ("This is not implemented yet. (matrix)\n");
		return (1);
	}
	if (StringEqual (name, "colbits"))
	{
		printf ("This is not implemented yet.\n");
		return (1);
	}
	if (StringEqual (name, "drawType"))
	{
		printf ("This is not implemented yet.\n");
		return (1);
	}
	if (StringEqual (name, "drawMode"))
	{
		printf ("This is not implemented yet.\n");
		return (1);
	}

	printf ("Unknown variable.\n");
	return (0);
}

