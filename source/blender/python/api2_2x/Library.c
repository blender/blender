/**
 * $Id$
 *
 * Blender.Library BPython module implementation.
 * This submodule has functions to append data from .blend files.
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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <Python.h>
#include <stdio.h>

#include "BKE_displist.h" /* for set_displist_onlyzero */
#include "BKE_font.h" /* for text_to_curve */
#include "BKE_library.h" /* for all_local */
#include "BLO_readfile.h"
#include "BLI_linklist.h"
#include "MEM_guardedalloc.h"

#include "gen_utils.h"
#include "modules.h"

/**
 * Global variables.
 */
static BlendHandle *bpy_openlib; /* ptr to the open .blend file */
static char *bpy_openlibname; /* its pathname */

/**
 * Function prototypes for the Library submodule.
 */
static PyObject *M_Library_Open(PyObject *self, PyObject *args);
static PyObject *M_Library_Close(PyObject *self);
static PyObject *M_Library_GetName(PyObject *self);
static PyObject *M_Library_Update(PyObject *self);
static PyObject *M_Library_Datablocks(PyObject *self, PyObject *args);
static PyObject *M_Library_Load(PyObject *self, PyObject *args);
static PyObject *M_Library_LinkableGroups(PyObject *self);

/**
 * Module doc strings.
 */	
static char M_Library_doc[]=
"The Blender.Library submodule:\n\n\
This module gives access to .blend files, using them as libraries of\n\
data that can be loaded into the current scene in Blender.";

static char Library_Open_doc[] =
"(filename) - Open the given .blend file for access to its objects.\n\
If another library file is still open, it's closed automatically.";

static char Library_Close_doc[] =
"() - Close the currently open library file, if any.";

static char Library_GetName_doc[] =
"() - Get the filename of the currently open library file, if any.";

static char Library_Datablocks_doc[] =
"(datablock) - List all datablocks of the given type in the currently\n\
open library file.\n\
(datablock) - datablock name as a string: Object, Mesh, etc.";

static char Library_Load_doc[] =
"(name, datablock [,update = 1]) - Append object 'name' of type 'datablock'\n\
from the open library file to the current scene.\n\
(name) - (str) the name of the object.\n\
(datablock) - (str) the datablock of the object.\n\
(update = 1) - (int) if non-zero, all display lists are recalculated and the\n\
links are updated.  This is slow, set it to zero if you have more than one\n\
object to load, then call Library.Update() after loading them all.";

static char Library_Update_doc[] =
"() - Update the current scene, linking all loaded library objects and\n\
remaking all display lists.  This is slow, call it only once after loading\n\
all objects (load each of them with update = 0:\n\
Library.Load(name, datablock, 0), or the update will be automatic, repeated\n\
for each loaded object.";

static char Library_LinkableGroups_doc[] =
"() - Get all linkable groups from the open .blend library file.";

/**
 * Python method structure definition for Blender.Library submodule.
 */
struct PyMethodDef M_Library_methods[] = {
	{"Open", M_Library_Open, METH_VARARGS, Library_Open_doc},
	{"Close",(PyCFunction)M_Library_Close, METH_NOARGS, Library_Close_doc},
	{"GetName",(PyCFunction)M_Library_GetName, METH_NOARGS, Library_GetName_doc},
	{"Update",(PyCFunction)M_Library_Update, METH_NOARGS, Library_Update_doc},
	{"Datablocks", M_Library_Datablocks, METH_VARARGS, Library_Datablocks_doc},
	{"Load", M_Library_Load, METH_VARARGS, Library_Load_doc},
	{"LinkableGroups",(PyCFunction)M_Library_LinkableGroups,
		METH_NOARGS, Library_LinkableGroups_doc},
	{NULL, NULL, 0, NULL}
};

/* Submodule Python functions: */

/**
 * Open a new .blend file.
 * Only one can be open at a time, so this function also closes
 * the previously opened file, if any.
 */
PyObject *M_Library_Open(PyObject *self, PyObject *args)
{
	char *fname = NULL;
	int len = 0;

	if (!PyArg_ParseTuple (args, "s", &fname)) {
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a .blend filename");
	}

	if (bpy_openlib) {
		M_Library_Close(self);
		Py_DECREF(Py_None); /* incref'ed by above function */
	}

	bpy_openlib = BLO_blendhandle_from_file(fname);

	if (!bpy_openlib) return Py_BuildValue("i", 0);

	len = strlen(fname) + 1; /* +1 for terminating '\0' */

	bpy_openlibname = MEM_mallocN(len, "bpy_openlibname");

	if (bpy_openlibname)
		PyOS_snprintf (bpy_openlibname, len, "%s", fname);

	return Py_BuildValue("i", 1);
}

/**
 * Close the current .blend file, if any.
 */
PyObject *M_Library_Close(PyObject *self)
{
	if (bpy_openlib) {
		BLO_blendhandle_close(bpy_openlib);
		bpy_openlib = NULL;
	}

	if (bpy_openlibname) {
		MEM_freeN (bpy_openlibname);
		bpy_openlibname = NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/**
 * helper function for 'atexit' clean-ups, used by BPY_end_python,
 * declared in EXPP_interface.h.
 */
void EXPP_Library_Close(void)
{
	if (bpy_openlib) {
		BLO_blendhandle_close(bpy_openlib);
		bpy_openlib = NULL;
	}

	if (bpy_openlibname) {
		MEM_freeN (bpy_openlibname);
		bpy_openlibname = NULL;
	}
}

/**
 * Get the filename of the currently open library file, if any.
 */
PyObject *M_Library_GetName(PyObject *self)
{
	if (bpy_openlib && bpy_openlibname)
		return Py_BuildValue("s", bpy_openlibname);

	Py_INCREF (Py_None);
	return Py_None;
}

/**
 * Return a list with all items of a given datablock type
 * (like 'Object', 'Mesh', etc.) in the open library file.
 */
PyObject *M_Library_Datablocks(PyObject *self, PyObject *args)
{
	char *name = NULL;
	int blocktype = 0;
	LinkNode *l = NULL, *names = NULL;
	PyObject *list = NULL;

	if (!bpy_openlib) {
		return EXPP_ReturnPyObjError(PyExc_IOError,
			"no library file: open one first with Blender.Lib_Open(filename)");
	}

	if (!PyArg_ParseTuple (args, "s", &name)) {
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected a string (datablock type) as argument.");
	}

	blocktype = (int)BLO_idcode_from_name(name);

	if (!blocktype) {
		return EXPP_ReturnPyObjError (PyExc_NameError,
			"no such Blender datablock type");
	}

	names = BLO_blendhandle_get_datablock_names(bpy_openlib, blocktype);

	if (names) {
		int counter = 0;
		list = PyList_New(BLI_linklist_length(names));
		for (l = names; l; l = l->next) {
			PyList_SET_ITEM(list, counter, Py_BuildValue("s", (char *)l->link));
			counter++;
		}
		BLI_linklist_free(names, free); /* free linklist *and* each node's data */
		return list;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

/**
 * Return a list with the names of all linkable groups in the
 * open library file.
 */
PyObject *M_Library_LinkableGroups(PyObject *self)
{
	LinkNode *l = NULL, *names = NULL;
	PyObject *list = NULL;

	if (!bpy_openlib) {
		return EXPP_ReturnPyObjError(PyExc_IOError,
			"no library file: open one first with Blender.Lib_Open(filename)");
	}

	names = BLO_blendhandle_get_linkable_groups(bpy_openlib);

	if (names) {
		int counter = 0;
		list = PyList_New(BLI_linklist_length(names));
		for (l = names; l; l = l->next) {
			PyList_SET_ITEM(list, counter, Py_BuildValue("s", (char *)l->link));
			counter++;
		}
		BLI_linklist_free(names, free); /* free linklist *and* each node's data */
		return list;
	}

	Py_INCREF (Py_None);
	return Py_None;
}

/**
 * Load (append) a given datablock of a given datablock type
 * to the current scene.
 */
PyObject *M_Library_Load(PyObject *self, PyObject *args)
{
	char *name = NULL;
	char *base = NULL;
	int update = 1;
	int blocktype = 0;

	if (!bpy_openlib) {
		return EXPP_ReturnPyObjError(PyExc_IOError,
			"no library file: you need to open one, first.");
	}

	if (!PyArg_ParseTuple (args, "ss|i", &name, &base, &update)) {
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected two strings as arguments.");
	}

	blocktype = (int)BLO_idcode_from_name(base);

	if (!blocktype) {
		return EXPP_ReturnPyObjError (PyExc_NameError,
			"no such Blender datablock type");
	}

	BLO_script_library_append(bpy_openlib, bpy_openlibname, name, blocktype);

	if (update) {
		M_Library_Update(self);
		Py_DECREF(Py_None); /* incref'ed by above function */
	}

	Py_INCREF (Py_None);
	return Py_None;
}

/**
 * Update all links and remake displists.
 */
PyObject *M_Library_Update(PyObject *self)
{ /* code adapted from do_library_append in src/filesel.c: */ 
	Object *ob = NULL;
	Library *lib = NULL;

	ob = G.main->object.first;
	set_displist_onlyzero(1);
	while (ob) {
		if (ob->id.lib) {
			if (ob->type==OB_FONT) {
				Curve *cu= ob->data;
				if (cu->nurb.first==0) text_to_curve(ob, 0);
			}
			makeDispList(ob);
		}
		else {
			if (ob->type == OB_MESH && ob->parent && ob->parent->type == OB_LATTICE)
				makeDispList(ob);
		}

		ob = ob->id.next;
	}
	set_displist_onlyzero(0);

	if (bpy_openlibname) {
		strcpy(G.lib, bpy_openlibname);

		/* and now find the latest append lib file */
		lib = G.main->library.first;
		while (lib) {
			if (strcmp(bpy_openlibname, lib->name) == 0) break;
			lib = lib->id.next;
		}
		all_local(lib);
	}

	Py_INCREF (Py_None);
	return Py_None;
}

/**
 * Initialize the Blender.Library submodule.
 * Called by Blender_Init in Blender.c .
 * @return the registered submodule.
 */
PyObject *Library_Init (void)
{
	PyObject *submod;

	submod = Py_InitModule3("Blender.Library", M_Library_methods, M_Library_doc);

	return submod;
}
