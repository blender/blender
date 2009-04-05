/* 
 * $Id: bpy_types.h 14444 2008-04-16 22:40:48Z hos $
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include "bpy_internal_import.h"
#include "DNA_text_types.h"
#include "DNA_ID.h"

#include "BKE_global.h"
#include "MEM_guardedalloc.h"
#include "BKE_text.h" /* txt_to_buf */	
#include "BKE_main.h"

static Main *bpy_import_main= NULL;

static void free_compiled_text(Text *text)
{
	if(text->compiled) {
		Py_DECREF(text->compiled);
	}
	text->compiled= NULL;
}

struct Main *bpy_import_main_get(void)
{
	return bpy_import_main;
}

void bpy_import_main_set(struct Main *maggie)
{
	bpy_import_main= maggie;
}


PyObject *importText( char *name )
{
	Text *text;
	char txtname[22]; /* 21+NULL */
	char *buf = NULL;
	int namelen = strlen( name );
	Main *maggie= bpy_import_main ? bpy_import_main:G.main;
	
	if (namelen>21-3) return NULL; /* we know this cant be importable, the name is too long for blender! */
	
	memcpy( txtname, name, namelen );
	memcpy( &txtname[namelen], ".py", 4 );

	for(text = maggie->text.first; text; text = text->id.next) {
		fprintf(stderr, "%s | %s\n", txtname, text->id.name+2);
		if( !strcmp( txtname, text->id.name+2 ) )
			break;
	}

	if( !text )
		return NULL;

	if( !text->compiled ) {
		buf = txt_to_buf( text );
		text->compiled = Py_CompileString( buf, text->id.name+2, Py_file_input );
		MEM_freeN( buf );

		if( PyErr_Occurred(  ) ) {
			PyErr_Print(  );
			free_compiled_text( text );
			return NULL;
		}
	}

	return PyImport_ExecCodeModule( name, text->compiled );
}


/*
 * find in-memory module and recompile
 */

PyObject *reimportText( PyObject *module )
{
	Text *text;
	char *txtname;
	char *name;
	char *buf = NULL;
	Main *maggie= bpy_import_main ? bpy_import_main:G.main;
	
	/* get name, filename from the module itself */

	txtname = PyModule_GetFilename( module );
	name = PyModule_GetName( module );
	if( !txtname || !name)
		return NULL;

	/* look up the text object */
	text = ( Text * ) & ( maggie->text.first );
	while( text ) {
		if( !strcmp( txtname, text->id.name+2 ) )
			break;
		text = text->id.next;
	}

	/* uh-oh.... didn't find it */
	if( !text )
		return NULL;

	/* if previously compiled, free the object */
	/* (can't see how could be NULL, but check just in case) */ 
	if( text->compiled ){
		Py_DECREF( (PyObject *)text->compiled );
	}

	/* compile the buffer */
	buf = txt_to_buf( text );
	text->compiled = Py_CompileString( buf, text->id.name+2, Py_file_input );
	MEM_freeN( buf );

	/* if compile failed.... return this error */
	if( PyErr_Occurred(  ) ) {
		PyErr_Print(  );
		free_compiled_text( text );
		return NULL;
	}

	/* make into a module */
	return PyImport_ExecCodeModule( name, text->compiled );
}


static PyObject *blender_import( PyObject * self, PyObject * args,  PyObject * kw)
{
	PyObject *exception, *err, *tb;
	char *name;
	PyObject *globals = NULL, *locals = NULL, *fromlist = NULL;
	PyObject *m;
	
	//PyObject_Print(args, stderr, 0);
#if (PY_VERSION_HEX >= 0x02060000)
	int dummy_val; /* what does this do?*/
	static char *kwlist[] = {"name", "globals", "locals", "fromlist", "level", 0};
	
	if( !PyArg_ParseTupleAndKeywords( args, kw, "s|OOOi:bpy_import", kwlist,
			       &name, &globals, &locals, &fromlist, &dummy_val) )
		return NULL;
#else
	static char *kwlist[] = {"name", "globals", "locals", "fromlist", 0};
	
	if( !PyArg_ParseTupleAndKeywords( args, kw, "s|OOO:bpy_import", kwlist,
			       &name, &globals, &locals, &fromlist ) )
		return NULL;
#endif
	m = PyImport_ImportModuleEx( name, globals, locals, fromlist );

	if( m )
		return m;
	else
		PyErr_Fetch( &exception, &err, &tb );	/*restore for probable later use */

	m = importText( name );
	if( m ) {		/* found module, ignore above exception */
		PyErr_Clear(  );
		Py_XDECREF( exception );
		Py_XDECREF( err );
		Py_XDECREF( tb );
		printf( "imported from text buffer...\n" );
	} else {
		PyErr_Restore( exception, err, tb );
	}
	return m;
}


/*
 * our reload() module, to handle reloading in-memory scripts
 */

static PyObject *blender_reload( PyObject * self, PyObject * args )
{
	PyObject *exception, *err, *tb;
	PyObject *module = NULL;
	PyObject *newmodule = NULL;

	/* check for a module arg */
	if( !PyArg_ParseTuple( args, "O:bpy_reload", &module ) )
		return NULL;

	/* try reimporting from file */
	newmodule = PyImport_ReloadModule( module );
	if( newmodule )
		return newmodule;

	/* no file, try importing from memory */
	PyErr_Fetch( &exception, &err, &tb );	/*restore for probable later use */

	newmodule = reimportText( module );
	if( newmodule ) {		/* found module, ignore above exception */
		PyErr_Clear(  );
		Py_XDECREF( exception );
		Py_XDECREF( err );
		Py_XDECREF( tb );
	} else
		PyErr_Restore( exception, err, tb );

	return newmodule;
}

PyMethodDef bpy_import[] = { {"bpy_import", blender_import, METH_KEYWORDS, "blenders import"} };
PyMethodDef bpy_reload[] = { {"bpy_reload", blender_reload, METH_VARARGS, "blenders reload"} };

