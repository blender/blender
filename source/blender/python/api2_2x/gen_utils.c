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
 * Contributor(s): Michel Selten, Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "gen_utils.h"
#include "constant.h"

#include <DNA_text_types.h>
#include <MEM_guardedalloc.h>

/*****************************************************************************/
/* Description: This function clamps an int to the given interval						 */
/*							[min, max].																									 */
/*****************************************************************************/
int EXPP_ClampInt (int value, int min, int max)
{
	if (value < min) return min;
	else if (value > max) return max;
	return value;
}

/*****************************************************************************/
/* Description: This function clamps a float to the given interval					 */
/*							[min, max].																									 */
/*****************************************************************************/
float EXPP_ClampFloat (float value, float min, float max)
{
	if (value < min) return min;
	else if (value > max) return max;
	return value;
}

/*****************************************************************************/
/* Description: This function returns true if both given strings are equal,  */
/*							otherwise it returns false.																	 */
/*****************************************************************************/
int StringEqual (const char * string1, const char * string2)
{
	return (strcmp(string1, string2)==0);
}

/*****************************************************************************/
/* Description: This function returns the name of the given ID struct				 */
/*							without the Object type identifying characters prepended.		 */
/*****************************************************************************/
char * GetIdName (ID *id)
{
	return ((id->name)+2);
}

/*****************************************************************************/
/* Description: This function returns the ID of the object with given name	 */
/*							from a given list.																					 */
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
/* Description: These functions set an internal string with the given type	 */
/*							and error_msg arguments.																		 */
/*****************************************************************************/

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
/* Description: This function increments the reference count of the given		 */
/*							Python object (usually Py_None) and returns it.							 */
/*****************************************************************************/

PyObject *EXPP_incr_ret (PyObject *object)
{
	Py_INCREF (object);
	return (object);
}

/*****************************************************************************/
/* Description: This function maps the event identifier to a string.				 */
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
/* Description: Checks whether all objects in a PySequence are of a same		 */
/*							given type.  Returns 0 if not, 1 on success.								 */
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
	PyObject *repr, *item;
	int i;

/*@	note: a value must be built because the list is decrefed!
 * otherwise we have nirvana pointers inside python.. */

	repr = PyString_FromString("");
	if (!repr) return 0;

	item = PySequence_GetItem(self, 0); 
	PyString_ConcatAndDel(&repr, PyObject_Repr(item));
	Py_DECREF(item);

	for (i = 1; i < size; i++) {
		item = PySequence_GetItem(self, i);
		PyString_ConcatAndDel(&repr, PyObject_Repr(item));
		Py_DECREF(item);
	}

	return repr;
}

/****************************************************************************/
/* Description: searches through a map for a pair with a given name. If the */
/*							pair is present, its ival is stored in *ival and nonzero is */
/*							returned. If the pair is absent, zero is returned.					*/
/****************************************************************************/
int EXPP_map_getIntVal (const EXPP_map_pair *map, const char *sval, int *ival)
{
		while (map->sval)
		{
				if (StringEqual(sval, map->sval))
				{
						*ival = map->ival;
						return 1;
				}
				++map;
		}
		return 0;
}

/****************************************************************************/
/* Description: searches through a map for a pair with a given name. If the */
/*							pair is present, its ival is stored in *ival and nonzero is */
/*							returned. If the pair is absent, zero is returned.					*/
/* note: this function is identical to EXPP_map_getIntVal except that the		*/
/*			 output is stored in a short value.																	*/
/****************************************************************************/
int EXPP_map_getShortVal (const EXPP_map_pair *map, 
																				const char *sval, short *ival)
{
		while (map->sval)
		{
				if (StringEqual(sval, map->sval))
				{
						*ival = map->ival;
						return 1;
				}
				++map;
		}
		return 0;
}

/****************************************************************************/
/* Description: searches through a map for a pair with a given ival. If the */
/*							pair is present, a pointer to its name is stored in *sval		*/
/*							and nonzero is returned. If the pair is absent, zero is			*/
/*							returned.																										*/
/****************************************************************************/
int EXPP_map_getStrVal (const EXPP_map_pair *map, int ival, const char **sval)
{
	while (map->sval)
	{
		if (ival == map->ival)
		{
			*sval = map->sval;
			return 1;
		}
		++map;
	}
	return 0;
}

/************************************************************************/
/* Scriptlink-related functions, used by scene, object, etc. bpyobjects */
/************************************************************************/
PyObject *EXPP_getScriptLinks (ScriptLink *slink, PyObject *args, int is_scene)
{
	PyObject *list = NULL;
	char *eventname = NULL;
	int i, event = 0;

	/* actually !scriptlink shouldn't happen ... */
	if (!slink || !slink->totscript)
		return EXPP_incr_ret (Py_None);

	if (!PyArg_ParseTuple(args, "s", &eventname))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected event name (string) as argument");

	list = PyList_New(0);
	if (!list)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
			"couldn't create PyList!");

	if (!strcmp(eventname, "FrameChanged"))
		event = SCRIPT_FRAMECHANGED;
	else if (!strcmp(eventname, "Redraw"))
		event = SCRIPT_REDRAW;
	else if (is_scene && !strcmp(eventname, "OnLoad"))
		event = SCRIPT_ONLOAD;
	else
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
						"invalid event name.");

	for (i = 0; i < slink->totscript; i++) {
		if ((slink->flag[i] == event) && slink->scripts[i])
			PyList_Append(list, PyString_FromString(slink->scripts[i]->name+2));
	}

	return list;
}

int EXPP_clearScriptLinks (ScriptLink *slink)
{
	/* actually !scriptlink shouldn't happen ... */
	if (!slink || !slink->totscript) return -1;

	if (slink->scripts) MEM_freeN(slink->scripts);
	if (slink->flag) MEM_freeN(slink->flag);

	slink->scripts = NULL;
	slink->flag = NULL;
	slink->totscript = slink->actscript = 0;

	return 0; /* normal return */
}

int EXPP_addScriptLink (ScriptLink *slink, PyObject *args, int is_scene)
{
	int event = 0, found_txt = 0;
	void *stmp = NULL, *ftmp = NULL;
	Text *bltxt = G.main->text.first;
	char *textname = NULL;
	char *eventname = NULL;

	/* !scriptlink shouldn't happen ... */
	if (!slink) {
		return EXPP_ReturnIntError (PyExc_RuntimeError,
			"internal error: no scriptlink!");
	}

	if (!PyArg_ParseTuple(args, "ss", &textname, &eventname))
		return EXPP_ReturnIntError (PyExc_TypeError,
			"expected two strings as arguments");

	while (bltxt) {
		if (!strcmp(bltxt->id.name+2, textname)) {
			found_txt = 1;
			break;
		}
		bltxt = bltxt->id.next;
	}

	if (!found_txt)
		return EXPP_ReturnIntError (PyExc_AttributeError,
			"no such Blender Text.");

	if (!strcmp(eventname, "FrameChanged"))
		event = SCRIPT_FRAMECHANGED;
	else if (!strcmp(eventname, "Redraw"))
		event = SCRIPT_REDRAW;
	else if (is_scene && !strcmp(eventname, "OnLoad"))
		event = SCRIPT_ONLOAD;
	else
		return EXPP_ReturnIntError (PyExc_AttributeError,
						"invalid event name.");

	stmp= slink->scripts;
	slink->scripts= MEM_mallocN(sizeof(ID*)*(slink->totscript+1), "bpySlinkL");

	ftmp= slink->flag;
	slink->flag= MEM_mallocN(sizeof(short*)*(slink->totscript+1), "bpySlinkF");
	
	if (slink->totscript) {
		memcpy(slink->scripts, stmp, sizeof(ID*)*(slink->totscript));
		MEM_freeN(stmp);

		memcpy(slink->flag, ftmp, sizeof(short)*(slink->totscript));
		MEM_freeN(ftmp);
	}

	slink->scripts[slink->totscript] = (ID*)bltxt;
	slink->flag[slink->totscript]= event;

	slink->totscript++;
				
	if (slink->actscript < 1) slink->actscript = 1;

	return 0; /* normal exit */
}
