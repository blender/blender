/* 
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/generic/bpy_internal_import.c
 *  \ingroup pygen
 *
 * This file defines replacements for pythons '__import__' and 'imp.reload'
 * functions which can import from blender textblocks.
 *
 * \note
 * This should eventually be replaced by import hooks (pep 302).
 */


#include <Python.h>
#include <stddef.h>

#include "bpy_internal_import.h"

#include "MEM_guardedalloc.h"

#include "DNA_text_types.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

/* UNUSED */
#include "BKE_text.h" /* txt_to_buf */	
#include "BKE_main.h"

static Main *bpy_import_main = NULL;

/* 'builtins' is most likely PyEval_GetBuiltins() */
void bpy_import_init(PyObject *builtins)
{
	PyObject *item;
	PyObject *mod;

	PyDict_SetItemString(builtins, "__import__", item = PyCFunction_New(&bpy_import_meth, NULL)); Py_DECREF(item);

	/* move reload here
	 * XXX, use import hooks */
	mod = PyImport_ImportModuleLevel((char *)"imp", NULL, NULL, NULL, 0);
	if (mod) {
		PyDict_SetItemString(PyModule_GetDict(mod), "reload", item = PyCFunction_New(&bpy_reload_meth, NULL)); Py_DECREF(item);
		Py_DECREF(mod);
	}
	else {
		BLI_assert(!"unable to load 'imp' module.");
	}
}


static void free_compiled_text(Text *text)
{
	if (text->compiled) {
		Py_DECREF((PyObject *)text->compiled);
	}
	text->compiled = NULL;
}

struct Main *bpy_import_main_get(void)
{
	return bpy_import_main;
}

void bpy_import_main_set(struct Main *maggie)
{
	bpy_import_main = maggie;
}

/* returns a dummy filename for a textblock so we can tell what file a text block comes from */
void bpy_text_filename_get(char *fn, size_t fn_len, Text *text)
{
	BLI_snprintf(fn, fn_len, "%s%c%s", ID_BLEND_PATH(bpy_import_main, &text->id), SEP, text->id.name + 2);
}

PyObject *bpy_text_import(Text *text)
{
	char *buf = NULL;
	char modulename[MAX_ID_NAME + 2];
	int len;

	if (!text->compiled) {
		char fn_dummy[256];
		bpy_text_filename_get(fn_dummy, sizeof(fn_dummy), text);

		buf = txt_to_buf(text);
		text->compiled = Py_CompileString(buf, fn_dummy, Py_file_input);
		MEM_freeN(buf);

		if (PyErr_Occurred()) {
			PyErr_Print();
			PyErr_Clear();
			PySys_SetObject("last_traceback", NULL);
			free_compiled_text(text);
			return NULL;
		}
	}

	len = strlen(text->id.name + 2);
	BLI_strncpy(modulename, text->id.name + 2, len);
	modulename[len - 3] = '\0'; /* remove .py */
	return PyImport_ExecCodeModule(modulename, text->compiled);
}

PyObject *bpy_text_import_name(const char *name, int *found)
{
	Text *text;
	char txtname[MAX_ID_NAME - 2];
	int namelen = strlen(name);
//XXX	Main *maggie = bpy_import_main ? bpy_import_main:G.main;
	Main *maggie = bpy_import_main;
	
	*found = 0;

	if (!maggie) {
		printf("ERROR: bpy_import_main_set() was not called before running python. this is a bug.\n");
		return NULL;
	}

	/* we know this cant be importable, the name is too long for blender! */
	if (namelen >= (MAX_ID_NAME - 2) - 3) return NULL;

	memcpy(txtname, name, namelen);
	memcpy(&txtname[namelen], ".py", 4);

	text = BLI_findstring(&maggie->text, txtname, offsetof(ID, name) + 2);

	if (!text)
		return NULL;
	else
		*found = 1;
	
	return bpy_text_import(text);
}


/*
 * find in-memory module and recompile
 */

PyObject *bpy_text_reimport(PyObject *module, int *found)
{
	Text *text;
	const char *name;
	char *filepath;
	char *buf = NULL;
//XXX	Main *maggie = bpy_import_main ? bpy_import_main:G.main;
	Main *maggie = bpy_import_main;
	
	if (!maggie) {
		printf("ERROR: bpy_import_main_set() was not called before running python. this is a bug.\n");
		return NULL;
	}
	
	*found = 0;
	
	/* get name, filename from the module itself */
	if ((name = PyModule_GetName(module)) == NULL)
		return NULL;

	if ((filepath = (char *)PyModule_GetFilename(module)) == NULL)
		return NULL;

	/* look up the text object */
	text = BLI_findstring(&maggie->text, BLI_path_basename(filepath), offsetof(ID, name) + 2);

	/* uh-oh.... didn't find it */
	if (!text)
		return NULL;
	else
		*found = 1;

	/* if previously compiled, free the object */
	/* (can't see how could be NULL, but check just in case) */ 
	if (text->compiled) {
		Py_DECREF((PyObject *)text->compiled);
	}

	/* compile the buffer */
	buf = txt_to_buf(text);
	text->compiled = Py_CompileString(buf, text->id.name + 2, Py_file_input);
	MEM_freeN(buf);

	/* if compile failed.... return this error */
	if (PyErr_Occurred()) {
		PyErr_Print();
		PyErr_Clear();
		PySys_SetObject("last_traceback", NULL);
		free_compiled_text(text);
		return NULL;
	}

	/* make into a module */
	return PyImport_ExecCodeModule((char *)name, text->compiled);
}


static PyObject *blender_import(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	PyObject *exception, *err, *tb;
	char *name;
	int found = 0;
	PyObject *globals = NULL, *locals = NULL, *fromlist = NULL;
	int level = -1; /* relative imports */
	
	PyObject *newmodule;
	//PyObject_Print(args, stderr, 0);
	static const char *kwlist[] = {"name", "globals", "locals", "fromlist", "level", NULL};
	
	if (!PyArg_ParseTupleAndKeywords(args, kw, "s|OOOi:bpy_import_meth", (char **)kwlist,
	                                 &name, &globals, &locals, &fromlist, &level))
	{
		return NULL;
	}

	/* import existing builtin modules or modules that have been imported already */
	newmodule = PyImport_ImportModuleLevel(name, globals, locals, fromlist, level);
	
	if (newmodule)
		return newmodule;
	
	PyErr_Fetch(&exception, &err, &tb); /* get the python error in case we cant import as blender text either */
	
	/* importing from existing modules failed, see if we have this module as blender text */
	newmodule = bpy_text_import_name(name, &found);
	
	if (newmodule) { /* found module as blender text, ignore above exception */
		PyErr_Clear();
		Py_XDECREF(exception);
		Py_XDECREF(err);
		Py_XDECREF(tb);
		/* printf("imported from text buffer...\n"); */
	}
	else if (found == 1) { /* blender text module failed to execute but was found, use its error message */
		Py_XDECREF(exception);
		Py_XDECREF(err);
		Py_XDECREF(tb);
		return NULL;
	}
	else {
		/* no blender text was found that could import the module
		 * rause the original error from PyImport_ImportModuleEx */
		PyErr_Restore(exception, err, tb);
	}
	return newmodule;
}


/*
 * our reload() module, to handle reloading in-memory scripts
 */

static PyObject *blender_reload(PyObject *UNUSED(self), PyObject *module)
{
	PyObject *exception, *err, *tb;
	PyObject *newmodule = NULL;
	int found = 0;

	/* try reimporting from file */
	newmodule = PyImport_ReloadModule(module);
	if (newmodule)
		return newmodule;

	/* no file, try importing from memory */
	PyErr_Fetch(&exception, &err, &tb); /*restore for probable later use */

	newmodule = bpy_text_reimport(module, &found);
	if (newmodule) { /* found module as blender text, ignore above exception */
		PyErr_Clear();
		Py_XDECREF(exception);
		Py_XDECREF(err);
		Py_XDECREF(tb);
		/* printf("imported from text buffer...\n"); */
	}
	else if (found == 1) { /* blender text module failed to execute but was found, use its error message */
		Py_XDECREF(exception);
		Py_XDECREF(err);
		Py_XDECREF(tb);
		return NULL;
	}
	else {
		/* no blender text was found that could import the module
		 * reuse the original error from PyImport_ImportModuleEx */
		PyErr_Restore(exception, err, tb);
	}

	return newmodule;
}

PyMethodDef bpy_import_meth = {"bpy_import_meth", (PyCFunction)blender_import, METH_VARARGS | METH_KEYWORDS, "blenders import"};
PyMethodDef bpy_reload_meth = {"bpy_reload_meth", (PyCFunction)blender_reload, METH_O, "blenders reload"};
