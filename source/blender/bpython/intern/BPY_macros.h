
/* bpython library macros
 *
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 *
 */


/* Hint: use gcc -E file.c  to see what these macros are expanded in */

#include "api.h"          // temporary defines for API version 

#include "BPY_listbase_macro.h"
#include "BKE_utildefines.h"

/* CONSTANTS */

#define IDNAME 24
#define PATH_MAXCHAR 128

/* ALIASES */

#define BPY_TRY(x) {if((!(x))) return NULL;}
#define BPY_TRY_TYPEERROR(x, str) \
	{ if(!(x)) { \
		PyErr_SetString(PyExc_TypeError, str); \
		return NULL; }\
	}

#define BPY_ADDCONST(dict, name) PyDict_SetItemString(dict, #name, PyInt_FromLong(name))
#define CurrentFrame (getGlobal()->scene->r.cfra)
#define RETURN_INC(ob) {Py_INCREF(ob); return ob; }

/* Blender object internal 'reference' (user) counting */
/* 'x' must be of type (ID *)                 */

#ifdef DEBUG

	#define BOB_USERCOUNT(x) \
		(x)->us
	#define BOB_DECUSER(x) \
		printf("BOB_DECUSER: %s\n", (x)->name); \
		(x)->us ? (x)->us--:printf("FATAL: 0--\n")
	#define BOB_INCUSER(x) \
		printf("BOB_INCUSER: %s\n", (x)->name); \
		id_us_plus(x)
	/* safe ref-inc/dec */
	#define BOB_XDECUSER(x) \
		if (x) { \
			printf("BOB_XDECUSER: %s\n", (x)->name); \
			((x)->us ? (x)->us--:printf("FATAL: 0--\n")); \
		}
	#define BOB_XINCUSER(x) \
		if (x) { \
			printf("BOB_XINCUSER: %s\n", (x)->name); \
			if (x) id_us_plus(x); \
		}
#else

	#define BOB_USERCOUNT(x) \
		(x)->us
	#define BOB_DECUSER(x) \
		(x)->us ? (x)->us--:printf("FATAL: 0--\n")
	#define BOB_INCUSER(x) \
		id_us_plus(x)
	/* safe ref-inc/dec */
	#define BOB_XDECUSER(x) \
		if (x) ((x)->us ? (x)->us--:printf("FATAL: 0--\n")) 
	#define BOB_XINCUSER(x) \
		if (x) id_us_plus(x) 

#endif

/* WARNINGS, Verbose */

#define BPY_CHECKFLAG(x) (getGlobal()->f & x)
#define BPY_DEBUGFLAG BPY_CHECKFLAG(G_DEBUG)
#define BPY_debug(a) if BPY_DEBUGFLAG {printf a; }
#define BPY_warn(a) {printf a; }

/* BLENDER DATABLOCK ACCESS */

/* these are relicts... */
#define GS(a)	(*((short *)(a)))
#define STREQ(str, a)           ( strcmp((str), (a))==0 )   

/**  This macro should be used to get the (short) id type of a ID datablock
 *   structure (Object, Mesh, etc.)
 *   Usage is dangerous, so use it only if you know what you're doing :-)
 *   Otherwise, use DataBlock_type() or associated functions (datablock.c)
 */

#define GET_ID_TYPE(x) (GS((x)->name))

/* gets the datablock's ID pointer. be careful with its usage,
 * - no typechecking done! */
#define DATABLOCK_ID(x) ( (ID *) ((DataBlock *) x)->data)

/** This defines the Get method plus documentation for use in a
  * Method definition list.
  * Example: 
  *
  *    DATABLOCK_GET(modulename, objectname, listbase.first)
  *
  * This function, called in Python by:
  *
  *    modulename.Get(name)
  * 
  * returns a Python DataBlock object for the Blender object with name
  * 'name'. If 'name' omitted, a list of all the objects in the
  * given list (a linked list of ID pointers) is returned
  */


#define DATABLOCK_GET(uname, docname, list)						      \
static char uname##_get_doc[]=									      \
"([name]) - Get " #docname "s from Blender\n"					      \
"\n"															      \
"[name] The name of the " #docname " to return\n"				      \
"\n"															      \
"Returns a list of all " #docname "s if name is not specified";       \
																      \
static PyObject* uname##_get (PyObject *self, PyObject *args) {	      \
	return py_find_from_list(list, args);                             \
}

/** This macro defines the creation of new Objects */

#define DATABLOCK_NEW( modprefix, type, callfunc)               \
static char modprefix##_New_doc[] =                             \
"() - returns new " #type " object";                            \
                                                                \
PyObject *modprefix##_New (PyObject *self, PyObject *args)      \
{                                                               \
	type *obj;                                                  \
	char *name = #type;                                         \
	BPY_TRY(PyArg_ParseTuple(args, "|s", &name));               \
	obj = callfunc;                                             \
	return DataBlock_fromData(obj);                             \
}                                                               \

#define DATABLOCK_ASSIGN_IPO(type, prefix)                            \
static char type##_assignIpo_doc[]=                                   \
"(ipo) - assigns Ipo to object of type " #type ;                      \
                                                                      \
static PyObject *type##_assignIpo(PyObject *self, PyObject *args)     \
{                                                                     \
	DataBlock *ipoblock;                                              \
	Ipo *ipo;                                                         \
	type *object = PYBLOCK_AS(type, self);                            \
                                                                      \
	BPY_TRY(PyArg_ParseTuple(args, "O!", &DataBlock_Type, &ipoblock));\
	if (!DataBlock_isType(ipoblock, ID_IP)) {                         \
		PyErr_SetString(PyExc_TypeError, "expects Ipo object");       \
		return 0;                                                     \
	}                                                                 \
	ipo = PYBLOCK_AS_IPO(ipoblock);                                   \
                                                                      \
	if (ipo->blocktype != GET_ID_TYPE((ID *) object)) {               \
		PyErr_SetString(PyExc_TypeError,                              \
			"Ipo type does not match object type");                   \
		return 0;                                                     \
	}                                                                 \
	prefix##_assignIpo(object, ipo);                                  \
                                                                      \
	Py_INCREF(Py_None);                                               \
	return Py_None;	                                                  \
}                                                                     \
                                                                      \
static char type##_clrIpo_doc[]=                                      \
"(ipo) - clears Ipo" ;                                                \
                                                                      \
static PyObject *type##_clrIpo(PyObject *self, PyObject *args)        \
{                                                                     \
	type *object = PYBLOCK_AS(type, self);                            \
	BPY_TRY(PyArg_ParseTuple(args, ""));                              \
	prefix##_assignIpo(object, 0);                                    \
	Py_INCREF(Py_None);                                               \
	return Py_None;	                                                  \
}
	
/** Macro used to define the MethodDef macro which is used again for defining
  * module or object methods in the Method table, see e.g. BPY_scene.c
  * 
  * Usage:

  * _MethodDef(delete, Scene) expands to: 
  *
  *  {"delete", Scene_delete, METH_VARARGS, Scene_delete_doc}
  */
#define _MethodDef(func, prefix) \
	{#func, prefix##_##func, METH_VARARGS, prefix##_##func##_doc}

/** Mark the datablock wrapper as invalid. See BPY_text.c for details */
#define MARK_INVALID(datablock) \
	((DataBlock *) datablock)->data = NULL

/** Check whether datablock wrapper is valid */
#define CHECK_VALIDDATA(x, msg)                                       \
	if (!x) {                                                         \
		PyErr_SetString(PyExc_RuntimeError, msg);                     \
		return 0;                                                     \
	}                                                                 \


/* OBJECT ACCESS */

/** retrieves name from BPYobject */
#define getName(x) ((x)->id.name+2)
#define getUsers(x) ((x)->id.us)
#define getIDName(x) ((x)->name+2)
#define getIDUsers(x) ((x)->us)

#define object_getMaterials(object) (object)->mat

/** rename object with name */

/* ListBase of active object in scene */
#define FirstBase (getGlobal()->scene->base.first)
#define ActiveBase (getGlobal()->scene->basact)
#define ObjectfromBase(base) (base->object)
#define SelectedAndLayer(base) (((base)->flag & SELECT) && ((base)->lay & getGlobal()->vd->lay))
/* Active object (bright pink) */

#define ActiveObject (ActiveBase ? ObjectfromBase(ActiveBase) : NULL)

/* returns 1 if textureface tf is selected/active, else 0 */
#define isSelectedFace(tf) (((tf).flag & TF_SELECT) ? 1 : 0 )
#define isActiveFace(tf) (((tf).flag & TF_ACTIVE) ? 1 : 0 )

/* some conversion macros */

#define PYBLOCK_AS(x, y) (x *) ((DataBlock *) y)->data
#define PYBLOCK_AS_TEXT(x) PYBLOCK_AS(Text, x)
#define PYBLOCK_AS_MATERIAL(x) PYBLOCK_AS(Material, x)
#define PYBLOCK_AS_OBJECT(x) PYBLOCK_AS(Object, x)
#define PYBLOCK_AS_MESH(x) PYBLOCK_AS(Mesh, x)
#define PYBLOCK_AS_LAMP(x) PYBLOCK_AS(Lamp, x)
#define PYBLOCK_AS_IPO(x) PYBLOCK_AS(Ipo, x)
#define PYBLOCK_AS_DATA(x) PYBLOCK_AS(void, x)

