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
 * Contributor(s): Michel Selten, Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "gen_utils.h"

/*****************************************************************************/
/* Description: This function clamps an int to the given interval            */
/*              [min, max].                                                  */
/*****************************************************************************/
int EXPP_ClampInt (int value, int min, int max)
{
	if (value < min) return min;
	else if (value > max) return max;
	return value;
}

/*****************************************************************************/
/* Description: This function clamps a float to the given interval           */
/*              [min, max].                                                  */
/*****************************************************************************/
float EXPP_ClampFloat (float value, float min, float max)
{
	if (value < min) return min;
	else if (value > max) return max;
	return value;
}

/*****************************************************************************/
/* Description: This function returns true if both given strings are equal,  */
/*              otherwise it returns false.                                  */
/*****************************************************************************/
int StringEqual (char * string1, char * string2)
{
	return (strcmp(string1, string2)==0);
}

/*****************************************************************************/
/* Description: This function returns the name of the given ID struct        */
/*              without the Object type identifying characters prepended.    */
/*****************************************************************************/
char * GetIdName (ID *id)
{
	return ((id->name)+2);
}

/*****************************************************************************/
/* Description: This function returns the ID of the object with given name   */
/*              from a given list.                                           */
/*****************************************************************************/
ID *GetIdFromList(ListBase *list, char *name)
{
	ID *id = list->first;

	while (id) {
		if(strcmp(name, id->name+2) == 0) break;
			id= id->next;
	}

	return id;
}

/*****************************************************************************/
/* Description: These functions set an internal string with the given type   */
/*              and error_msg arguments.                                     */
/*****************************************************************************/
PyObject * PythonReturnErrorObject (PyObject * type, char * error_msg)
{
	PyErr_SetString (type, error_msg);
	return (NULL);
}

PyObject *EXPP_ReturnPyObjError (PyObject * type, char * error_msg)
{ /* same as above, just to change its name smoothly */
	PyErr_SetString (type, error_msg);
	return NULL;
}

int EXPP_ReturnIntError (PyObject *type, char *error_msg)
{
	PyErr_SetString (type, error_msg);
	return -1;
}

/*****************************************************************************/
/* Description: This function increments the reference count of the given    */
/*              Python object (usually Py_None) and returns it.              */
/*****************************************************************************/
PyObject * PythonIncRef (PyObject *object)
{
	Py_INCREF (object);
	return (object);
}

PyObject *EXPP_incr_ret (PyObject *object)
{
	Py_INCREF (object);
	return (object);
}
/*****************************************************************************/
/* Description: This function maps the event identifier to a string.         */
/*****************************************************************************/
char * event_to_name(short event)
{
	switch (event)
	{
		case SCRIPT_FRAMECHANGED:
			return "FrameChanged";
		case SCRIPT_ONLOAD:
			return "OnLoad";
		case SCRIPT_REDRAW:
			return "Redraw";
		default:
			return "Unknown";
	}
}	

/*****************************************************************************/
/* Description: Returns the object with the name specified by the argument   */
/*              name. Note that the calling function has to remove the first */
/*              two characters of the object name. These two characters      */
/*              specify the type of the object (OB, ME, WO, ...)             */
/*              The function will return NULL when no object with the given  */
/*              name is found.                                               */
/*****************************************************************************/
struct Object * GetObjectByName (char * name)
{
	Object	* obj_iter;

	obj_iter = G.main->object.first;
	while (obj_iter)
	{
		if (StringEqual (name, GetIdName (&(obj_iter->id))))
		{
			return (obj_iter);
		}
		obj_iter = obj_iter->id.next;
	}

	/* There is no object with the given name */
	return (NULL);
}

/*****************************************************************************/
/* Description: Checks whether all objects in a PySequence are of a same     */
/*              given type.  Returns 0 if not, 1 on success.                 */
/*****************************************************************************/
int EXPP_check_sequence_consistency(PyObject *seq, PyTypeObject *against)
{
	PyObject *ob;
	int len = PySequence_Length(seq);
	int i;

	for (i = 0; i < len; i++) {
		ob = PySequence_GetItem(seq, i);
		if (ob->ob_type != against) {
			Py_DECREF(ob);
			return 0;
		}
		Py_DECREF(ob);
	}
	return 1;
}

PyObject *EXPP_tuple_repr(PyObject *self, int size)
{
	PyObject *repr, *comma, *item;
	int i;

/*@	note: a value must be built because the list is decrefed!
 * otherwise we have nirvana pointers inside python.. */

	repr = PyString_FromString("(");
	if (!repr) return 0;

	item = PySequence_GetItem(self, 0); 
	PyString_ConcatAndDel(&repr, PyObject_Repr(item));
	Py_DECREF(item);

	comma = PyString_FromString(", ");

	for (i = 1; i < size; i++) {
		PyString_Concat(&repr, comma);
		item = PySequence_GetItem(self, i);
		PyString_ConcatAndDel(&repr, PyObject_Repr(item));
		Py_DECREF(item);
	}

	PyString_ConcatAndDel(&repr, PyString_FromString(")"));
	Py_DECREF(comma);

	return repr;

}
