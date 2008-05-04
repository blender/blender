/**
 * $Id$
 *
 * Blender.Library BPython module implementation.
 * This submodule has functions to append data from .blend files.
 * 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Campbell Barton, Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
*/

/************************************************************/
/* Original library module code                             */
/************************************************************/

#include <Python.h>

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h" /* for line linked */
#include "BKE_library.h"	/* for all_local */
#include "BKE_font.h"		/* for text_to_curve */
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BLI_blenlib.h"
#include "BLO_readfile.h"
#include "BLI_linklist.h"
#include "MEM_guardedalloc.h"
#include "gen_utils.h"

/**
 * Global variables.
 */
static BlendHandle *bpy_openlib = NULL;	/* ptr to the open .blend file */
static char *bpy_openlibname = NULL;	/* its pathname */
static int bpy_relative= 0;

/**
 * Function prototypes for the Library submodule.
 */
static PyObject *M_Library_Open( PyObject * self, PyObject * args );
static PyObject *M_Library_Close( PyObject * self );
static PyObject *M_Library_GetName( PyObject * self );
static PyObject *M_Library_Update( PyObject * self );
static PyObject *M_Library_Datablocks( PyObject * self, PyObject * value );
static PyObject *oldM_Library_Load( PyObject * self, PyObject * args );
static PyObject *M_Library_LinkableGroups( PyObject * self );
static PyObject *M_Library_LinkedLibs( PyObject * self );

PyObject *Library_Init( void );
void EXPP_Library_Close( void );

/**
 * Module doc strings.
 */
static char M_Library_doc[] = "The Blender.Library submodule:\n\n\
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

static char Library_LinkedLibs_doc[] =
	"() - Get all libs used in the the open .blend file.";
	
/**
 * Python method structure definition for Blender.Library submodule.
 */
struct PyMethodDef oldM_Library_methods[] = {
	{"Open", M_Library_Open, METH_O, Library_Open_doc},
	{"Close", ( PyCFunction ) M_Library_Close, METH_NOARGS,
	 Library_Close_doc},
	{"GetName", ( PyCFunction ) M_Library_GetName, METH_NOARGS,
	 Library_GetName_doc},
	{"Update", ( PyCFunction ) M_Library_Update, METH_NOARGS,
	 Library_Update_doc},
	{"Datablocks", M_Library_Datablocks, METH_O,
	 Library_Datablocks_doc},
	{"Load", oldM_Library_Load, METH_VARARGS, Library_Load_doc},
	{"LinkableGroups", ( PyCFunction ) M_Library_LinkableGroups,
	 METH_NOARGS, Library_LinkableGroups_doc},
	{"LinkedLibs", ( PyCFunction ) M_Library_LinkedLibs,
	 METH_NOARGS, Library_LinkedLibs_doc},
	{NULL, NULL, 0, NULL}
};

/* Submodule Python functions: */

/**
 * Open a new .blend file.
 * Only one can be open at a time, so this function also closes
 * the previously opened file, if any.
 */
static PyObject *M_Library_Open( PyObject * self, PyObject * value )
{
	char *fname = PyString_AsString(value);
	char filename[FILE_MAXDIR+FILE_MAXFILE];
	char fname1[FILE_MAXDIR+FILE_MAXFILE];
	
	int len = 0;

	bpy_relative= 0; /* assume non relative each time we load */
	
	if( !fname ) {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a .blend filename" );
	}

	if( bpy_openlib ) {
		M_Library_Close( self );
		Py_DECREF( Py_None );	/* incref'ed by above function */
	}
	
	/* copy the name to make it absolute so BLO_blendhandle_from_file doesn't complain */
	BLI_strncpy(fname1, fname, sizeof(fname1)); 
	BLI_convertstringcode(fname1, G.sce); /* make absolute */
	
   	/* G.sce = last file loaded, save for UI and restore after opening file */
	BLI_strncpy(filename, G.sce, sizeof(filename));
	bpy_openlib = BLO_blendhandle_from_file( fname1 );
	BLI_strncpy(G.sce, filename, sizeof(filename)); 

	if( !bpy_openlib )
		return EXPP_ReturnPyObjError( PyExc_IOError, "file not found" );

	/* "//someblend.blend" enables relative paths */
	if (sizeof(fname) > 2 && fname[0] == '/' && fname[1] == '/')
		bpy_relative= 1; /* global that makes the library relative on loading */ 
	
	len = strlen( fname1 ) + 1;	/* +1 for terminating '\0' */

	bpy_openlibname = MEM_mallocN( len, "bpy_openlibname" );

	if( bpy_openlibname )
		PyOS_snprintf( bpy_openlibname, len, "%s", fname1 );

	Py_RETURN_TRUE;
}

/**
 * Close the current .blend file, if any.
 */
static PyObject *M_Library_Close( PyObject * self )
{
	if( bpy_openlib ) {
		BLO_blendhandle_close( bpy_openlib );
		bpy_openlib = NULL;
	}

	if( bpy_openlibname ) {
		MEM_freeN( bpy_openlibname );
		bpy_openlibname = NULL;
	}

	Py_RETURN_NONE;
}

/**
 * helper function for 'atexit' clean-ups, used by BPY_end_python,
 * declared in EXPP_interface.h.
 */
void EXPP_Library_Close( void )
{
	if( bpy_openlib ) {
		BLO_blendhandle_close( bpy_openlib );
		bpy_openlib = NULL;
	}

	if( bpy_openlibname ) {
		MEM_freeN( bpy_openlibname );
		bpy_openlibname = NULL;
	}
}

/**
 * Get the filename of the currently open library file, if any.
 */
static PyObject *M_Library_GetName( PyObject * self )
{
	if( bpy_openlib && bpy_openlibname )
		return Py_BuildValue( "s", bpy_openlibname );

	Py_INCREF( Py_None );
	return Py_None;
}

/**
 * Return a list with all items of a given datablock type
 * (like 'Object', 'Mesh', etc.) in the open library file.
 */
static PyObject *M_Library_Datablocks( PyObject * self, PyObject * value )
{
	char *name = PyString_AsString(value);
	int blocktype = 0;
	LinkNode *l = NULL, *names = NULL;
	PyObject *list = NULL;

	if( !bpy_openlib ) {
		return EXPP_ReturnPyObjError( PyExc_IOError,
					      "no library file: open one first with Blender.Lib_Open(filename)" );
	}

	if( !name ) {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string (datablock type) as argument." );
	}

	blocktype = ( int ) BLO_idcode_from_name( name );

	if( !blocktype ) {
		return EXPP_ReturnPyObjError( PyExc_NameError,
					      "no such Blender datablock type" );
	}

	names = BLO_blendhandle_get_datablock_names( bpy_openlib, blocktype );

	if( names ) {
		int counter = 0;
		list = PyList_New( BLI_linklist_length( names ) );
		for( l = names; l; l = l->next ) {
			PyList_SET_ITEM( list, counter,
					PyString_FromString( ( char * ) l->link ) );
			counter++;
		}
		BLI_linklist_free( names, free );	/* free linklist *and* each node's data */
	} else {
		list = PyList_New( 0 );
	}

	return list;
}

/**
 * Return a list with the names of all linkable groups in the
 * open library file.
 */
static PyObject *M_Library_LinkableGroups( PyObject * self )
{
	LinkNode *l = NULL, *names = NULL;
	PyObject *list = NULL;

	if( !bpy_openlib ) {
		return EXPP_ReturnPyObjError( PyExc_IOError,
					      "no library file: open one first with Blender.Lib_Open(filename)" );
	}

	names = BLO_blendhandle_get_linkable_groups( bpy_openlib );
	list = PyList_New( BLI_linklist_length( names ) );
	
	if( names ) {
		int counter = 0;
		
		for( l = names; l; l = l->next ) {
			PyList_SET_ITEM( list, counter, PyString_FromString( ( char * ) l->link  ) );
			counter++;
		}
		BLI_linklist_free( names, free );	/* free linklist *and* each node's data */
		return list;
	}
	return list;
}

/**
 * Return a list with the names of all externally linked libs used in the current Blend file
 */
static PyObject *M_Library_LinkedLibs( PyObject * self )
{
	int counter = 0;
	Library *li;
	PyObject *list;

	list = PyList_New( BLI_countlist( &( G.main->library ) ) );
	for (li= G.main->library.first; li; li= li->id.next) {
		PyList_SET_ITEM( list, counter, PyString_FromString( li->name ));
		counter++;
	}
	return list;
}

/**
 * Load (append) a given datablock of a given datablock type
 * to the current scene.
 */
static PyObject *oldM_Library_Load( PyObject * self, PyObject * args )
{
	char *name = NULL;
	char *base = NULL;
	int update = 1;
	int blocktype = 0;
	int linked = 0;

	
	if( !bpy_openlib ) {
		return EXPP_ReturnPyObjError( PyExc_IOError,
					      "no library file: you need to open one, first." );
	}

	if( !PyArg_ParseTuple( args, "ss|ii", &name, &base, &update, &linked ) ) {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected two strings as arguments." );
	}

	blocktype = ( int ) BLO_idcode_from_name( base );

	if( !blocktype )
		return EXPP_ReturnPyObjError( PyExc_NameError,
					      "no such Blender datablock type" );

	if (linked)
		BLO_script_library_append( &bpy_openlib, bpy_openlibname, name, blocktype, FILE_LINK, G.scene);
	else
		BLO_script_library_append( &bpy_openlib, bpy_openlibname, name, blocktype, 0, G.scene);

	/*
		NOTE:  BLO_script_library_append() can close the library if there is
		an endian issue.  if this happened, reopen for the next call.
	*/
	if ( !bpy_openlib )
		bpy_openlib = BLO_blendhandle_from_file( bpy_openlibname );

	if( update ) {
		M_Library_Update( self );
		Py_DECREF( Py_None );	/* incref'ed by above function */
	}
	
	if( bpy_relative ) {
		/* and now find the latest append lib file */
		Library *lib = G.main->library.first;
		while( lib ) {
			if( strcmp( bpy_openlibname, lib->name ) == 0 ) {
				
				/* use the full path, this could have been read by other library even */
				BLI_strncpy(lib->name, lib->filename, sizeof(lib->name));
		
				/* uses current .blend file as reference */
				BLI_makestringcode(G.sce, lib->name);
				break;
			}
			lib = lib->id.next;
		}
		
	}

	Py_INCREF( Py_None );
	return Py_None;
}

/**
 * Update all links and remake displists.
 */
static PyObject *M_Library_Update( PyObject * self )
{				/* code adapted from do_library_append in src/filesel.c: */
	Library *lib = NULL;

		/* Displist code that was here is obsolete... depending on what
		 * this function is supposed to do (it should technically be unnecessary)
		 * can be replaced with depgraph calls - zr
		 */

	if( bpy_openlibname ) {
		strcpy( G.lib, bpy_openlibname );

		/* and now find the latest append lib file */
		lib = G.main->library.first;
		while( lib ) {
			if( strcmp( bpy_openlibname, lib->name ) == 0 )
				break;
			lib = lib->id.next;
		}
		all_local( lib, 0 );
	}

	Py_INCREF( Py_None );
	return Py_None;
}

/**
 * Initialize the Blender.Library submodule.
 * Called by Blender_Init in Blender.c .
 * @return the registered submodule.
 */
PyObject *oldLibrary_Init( void )
{
	PyObject *submod;

	submod = Py_InitModule3( "Blender.Library", oldM_Library_methods,
				 M_Library_doc );

	return submod;
}

/************************************************************/
/* New library (LibData) module code                        */
/************************************************************/

#include "Library.h"

/* if this module supercedes the old library module, include these instead */
#if 0
#include "BLI_blenlib.h"
#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h" /* for line linked */
#include "BKE_library.h"	/* for all_local */
#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BLO_readfile.h"
#include "BLI_linklist.h"

#include "Object.h"
#include "gen_utils.h"
#endif

#include "gen_library.h"

/* Helper function */

/*
 * Try to open a library, set Python exceptions as necessary if not
 * successful.  On success, return a valid handle; othewise return NULL.
 */

static BlendHandle *open_library( char *filename, char *longFilename )
{
	char globalFilename[FILE_MAX];
	BlendHandle *openlib = NULL;

	/* get complete file name if necessary */
	BLI_strncpy( longFilename, filename, FILE_MAX ); 
	BLI_convertstringcode( longFilename, G.sce );

	/* throw exceptions for wrong file type, cyclic reference */
	if( !BLO_has_bfile_extension(longFilename) ) {
		PyErr_SetString( PyExc_ValueError, "file not a library" );
		return NULL;
	}
	if( BLI_streq(G.main->name, longFilename) ) {
		PyErr_SetString( PyExc_ValueError,
				"cannot use current file as library" );
		return NULL;
	}

   	/* G.sce = last file loaded, save for UI and restore after opening file */
	BLI_strncpy(globalFilename, G.sce, sizeof(globalFilename));
	openlib = BLO_blendhandle_from_file( longFilename );
	BLI_strncpy(G.sce, globalFilename, sizeof(globalFilename)); 

	/* if failed, set that exception code too */
	if( !openlib )
		PyErr_SetString( PyExc_IOError, "library not found" );

	return openlib;
}

/*
 * Create a specific type of LibraryData object.  These are used for
 * .append() and .link() access, for iterators, and (for Blender Objects)
 * for defining "pseudo objects" for scene linking.
 */

static PyObject *CreatePyObject_LibData( int idtype, int kind,
		void *name, void *iter, char *filename, int rel )
{
	BPy_LibraryData *seq = PyObject_NEW( BPy_LibraryData, &LibraryData_Type);
	seq->iter = iter;		/* the name list (for iterators) */
	seq->type = idtype;		/* the Blender ID type */
	seq->rel =  rel;		/* relative or absolute library */
	seq->kind = kind;		/* used by Blender Objects */
	seq->name = name; 		/* object name, iterator name list, or NULL */
							/* save the library name */
	BLI_strncpy( seq->filename, filename, strlen(filename)+1 );
	return (PyObject *)seq;
}

/* 
 * Link/append data to the current .blend file, or create a pseudo object
 * which can be linked/appended to a scene.
 */

static PyObject *lib_link_or_append( BPy_LibraryData *self, PyObject * value,
		int mode )
{
	char *name = PyString_AsString(value);

	/* get the name of the data used wants to append */
	if( !name )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected a string" );

	/* 
	 * For everything except objects, just add to Blender's DB.  For objects,
	 * create an APPEND or LINK "pseudo object" for the Scene module.
	 */
	if( self->type != ID_OB )
		return LibraryData_importLibData( self, name, 0, NULL );
	else {
		/*
		 * If this is already a pseudo object, throw an exception: re-linking
		 * or re-appending is not allowed
		 */
		if( self->kind != OTHER ) 
			return EXPP_ReturnPyObjError( PyExc_ValueError,
				"object has already been marked for append or link" );

		/* otherwise, create a pseudo object ready for appending or linking */

		return CreatePyObject_LibData( ID_OB, mode, 
				BLI_strdupn( name, strlen( name ) ), NULL, self->filename,
				self->rel );
	}
}

/*
 * Perform the actual link or append operation.  This procedure is also
 * called externally from the Scene module using a "pseudo Object" so we
 * can be sure objects get linked to a scene.
 */

PyObject *LibraryData_importLibData( BPy_LibraryData *self, char *name,
		int mode, Scene *scene )
{
	char longFilename[FILE_MAX];
	BlendHandle *openlib;
	Library *lib;
	LinkNode *names, *ptr;
	ID *id;
	ListBase *lb;
	char newName[32];

	/* try to open the library */
	openlib = open_library( self->filename, longFilename );
	if( !openlib )
		return NULL;

	/* fix any /foo/../foo/ */
	BLI_cleanup_file(NULL, longFilename); 

	/* find all datablocks for the specified type */
	names = BLO_blendhandle_get_datablock_names ( openlib, self->type ); 

	/* now check for a match to the user-specified name */
	for( ptr = names; ptr; ptr = ptr->next )
		if( strcmp( ptr->link, name ) == 0 ) break;
	BLI_linklist_free( names, free );

	/* if no match, throw exception */
	if( !ptr ) {
		BLO_blendhandle_close( openlib );
		return EXPP_ReturnPyObjError( PyExc_ValueError,
				"library does not contain specified item" );
	}

	/*
	 * Figure out what the datablock will be named after it's imported.  If
	 * it's a link, nothing to do.  If it's an append, find what it might
	 * be renamed to.
	 */

	if( mode != FILE_LINK ) {
		flag_all_listbases_ids(LIB_APPEND_TAG, 1);

	   	/* see what new block will be called */
		strncpy( newName, name, strlen(name)+1 );
		check_for_dupid( wich_libbase(G.main, self->type), NULL, newName );
	}

	/* import from the libary */
	BLO_script_library_append( &openlib, longFilename, name, self->type, 
			mode | self->rel, scene );

	/*
	 * locate the library.  If this is an append, make the data local.  If it
	 * is link, we need the library for later
	 */
	for( lib = G.main->library.first; lib; lib = lib->id.next )
		if( strcmp( longFilename, lib->filename ) == 0) {
			if( mode != FILE_LINK ) {
				all_local( lib, 1 );
				/* important we unset, otherwise these object wont
				 * link into other scenes from this blend file */
				flag_all_listbases_ids(LIB_APPEND_TAG, 0);
			}
			break;
		}

	/* done with library; close it */
	BLO_blendhandle_close( openlib );

	/* this should not happen, but just in case */
	if( !lib )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"could not library" );

	/* find the base for this type */
	lb = wich_libbase( G.main, self->type );

	/*
	 * Check for linked data matching the name first.  Even if we are trying
	 * to append, if the data has already been linked we need to return it
	 * (it won't be appended from the library).
	 */
	for( id = lb->first; id; id = id->next ) {
		if( id->lib == lib && id->name[2]==name[0] &&
				strcmp(id->name+2, name)==0 )
			return GetPyObjectFromID( id );
	}

	/*
	 * If we didn't find it, and we're appending, then try searching for the
	 * new datablock, possibly under a new name.
	 */
	if( mode != FILE_LINK )
		for( id = lb->first; id; id = id->next ) {
			if( id->lib == NULL && id->name[2]==newName[0] &&
					strcmp(id->name+2, newName)==0 )
				return GetPyObjectFromID( id );
		}

	/* if we get here, something's really wrong */
	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"could not find data after reading from library" );
}

/************************************************************
 * Python LibraryData_Type getseters
 ************************************************************/

/* .append(): make a local copy of the library's data (except for objects) */

static PyObject *LibraryData_getAppend( BPy_LibraryData *self, PyObject * value)
{
	return lib_link_or_append( self, value, OBJECT_IS_APPEND );
}

/* .link(): make a link to the library's data (except for objects) */

static PyObject *LibraryData_getLink( BPy_LibraryData *self, PyObject * value)
{
	return lib_link_or_append( self, value, OBJECT_IS_LINK );
}

/************************************************************************
 * Python LibraryData_Type iterator
 ************************************************************************/

/* Create and initialize the interator indices */

static PyObject *LibraryData_getIter( BPy_LibraryData * self )
{
	char longFilename[FILE_MAX];
	BlendHandle *openlib;
	LinkNode *names;

	/* try to open library */
	openlib = open_library( self->filename, longFilename );

	/* if failed, return exception */
	if( !openlib )
		return NULL;

	/* find all datablocks for the specified type */
	names = BLO_blendhandle_get_datablock_names ( openlib, self->type ); 

	/* close library*/
	BLO_blendhandle_close( openlib );

	/* build an iterator object for the name list */
	return CreatePyObject_LibData( self->type, OTHER, names,
			names, self->filename, self->rel );
}

/* Return next name. */

static PyObject *LibraryData_nextIter( BPy_LibraryData * self )
{
	LinkNode *ptr = (LinkNode *)self->iter;
	PyObject *ob;

	/* if at the end of list, clean up */
	if( !ptr ) {
		/* If name list is still allocated, free storage.  This check is
		 * necessary since iter.next() can technically be called repeatedly */
		if( self->name ) {
			BLI_linklist_free( (LinkNode *)self->name, free );
			self->name = NULL;
		}
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}

	/* otherwise, return the next name in the list */
	ob = PyString_FromString( ptr->link );
	ptr = ptr->next;
	self->iter = ptr;
	return ob;
}

/************************************************************************
 * Python LibraryData_type methods structure
 ************************************************************************/

static struct PyMethodDef BPy_LibraryData_methods[] = {
	{"append", (PyCFunction)LibraryData_getAppend, METH_O,
	 "(str) - create new data from library"},
	{"link", (PyCFunction)LibraryData_getLink, METH_O,
	 "(str) - link data from library"},
	{NULL, NULL, 0, NULL}
};

/* Deallocate object and its data */

static void LibraryData_dealloc( BPy_LibraryData * self )
{
	if( self->name )
		MEM_freeN( self->name );

	PyObject_DEL( self );
}

/* Display representation of what Library Data is wrapping */

static PyObject *LibraryData_repr( BPy_LibraryData * self )
{
	char *linkstate = "";
	char *str;

	switch (self->type) {
	case ID_OB:
		/* objects can be lib data or pseudo objects */
		switch( self->kind ) {
		case OBJECT_IS_APPEND :
			linkstate = ", appended";
			break;
		case OBJECT_IS_LINK :
			linkstate = ", linked";
			break;
		default:
			break;
		}
		str = "Object";
		break;
	case ID_SCE:
		str = "Scene";
		break;
	case ID_ME:
		str = "Mesh";
		break;
	case ID_CU:
		str = "Curve";
		break;
	case ID_MB:
		str = "Metaball";
		break;
	case ID_MA:
		str = "Material";
		break;
	case ID_TE:
		str = "Texture";
		break;
	case ID_IM: 
		str = "Image";
		break;
	case ID_LT:
		str = "Lattice";
		break;
	case ID_LA:
		str = "Lamp";
		break;
	case ID_CA:
		str = "Camera";
		break;
	case ID_IP:
		str = "Ipo";
		break;
	case ID_WO:
		str = "World";
		break;
	case ID_VF:
		str = "Font";
		break;
	case ID_TXT:
		str = "Text";
		break;
	case ID_SO:
		str = "Sound";
		break;
	case ID_GR:	
		str = "Group";
		break;
	case ID_AR:
		str = "Armature";
		break;
	case ID_AC:
		str = "Action";
		break;
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"unsupported ID type" );
	}

	return PyString_FromFormat( "[Library Data (%s%s)]", str, linkstate );
}

PyTypeObject LibraryData_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender LibData",          /* char *tp_name; */
	sizeof( BPy_LibraryData ),  /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) LibraryData_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) NULL,           /* cmpfunc tp_compare; */
	( reprfunc ) LibraryData_repr, /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	                    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	(getiterfunc)LibraryData_getIter, /* getiterfunc tp_iter; */
	(iternextfunc)LibraryData_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_LibraryData_methods,    /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*
 * Create a LibraryData object for a specific type of Blender Group (ID_OB,
 * ID_MA, etc).  These can then be used to link or append the data.
 */

static PyObject *LibraryData_CreatePyObject( BPy_Library *self, void *mode )
{
	return CreatePyObject_LibData( GET_INT_FROM_POINTER(mode), OTHER, NULL, NULL,
			self->filename, self->rel);
}

/************************************************************
 * Python Library_Type getseters
 ************************************************************/

/*
 * Return the library's filename.
 */

static PyObject *Library_getFilename( BPy_Library * self )
{
	return PyString_FromString( self->filename );
}

/*
 * Set/change the library's filename.
 */

static int Library_setFilename( BPy_Library * self, PyObject * args )
{
	char *filename = PyString_AsString( args );
	if( !filename )
		return EXPP_ReturnIntError( PyExc_TypeError, "expected a string" );

	BLI_strncpy( self->filename, filename, sizeof(self->filename) );
	return 0;
}

/*
 * Return the library's name.  The format depends on whether the library is 
 * accessed as relative or absolute.
 */

static PyObject *Library_getName( BPy_Library * self )
{
	Library *lib;
	BlendHandle *openlib;
	char longFilename[FILE_MAX];

	/* try to open the library */
	openlib = open_library( self->filename, longFilename );
	if( openlib ) {
		BLO_blendhandle_close( openlib );
		/* remove any /../ or /./ junk */
		BLI_cleanup_file(NULL, longFilename); 

		/* search the loaded libraries for a match */
		for( lib = G.main->library.first; lib; lib = lib->id.next )
			if( strcmp( longFilename, lib->filename ) == 0) {
				return PyString_FromString( lib->name );
			}

		/* library not found in memory */
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"library not loaded" );
	}
	/* could not load library */
	return EXPP_ReturnPyObjError( PyExc_IOError, "library not found" );
}


/************************************************************************
 * Python Library_type attributes get/set structure
 ************************************************************************/

static PyGetSetDef Library_getseters[] = {
	{"filename",
	 (getter)Library_getFilename, (setter)Library_setFilename,
	 "library filename",
	 NULL},
	{"name",
	 (getter)Library_getName, (setter)NULL,
	 "library name (as used by Blender)",
	 NULL},
	{"objects",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "objects from the library",
	 (void *)ID_OB},
	{"scenes",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "scenes from the library",
	 (void *)ID_SCE},
	{"meshes",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "meshes from the library",
	 (void *)ID_ME},
	{"curves",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "curves from the library",
	 (void *)ID_CU},
	{"metaballs",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "metaballs from the library",
	 (void *)ID_MB},
	{"lattices",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "lattices from the library",
	 (void *)ID_LT},
	{"lamps",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "lamps from the library",
	 (void *)ID_LA},
	{"cameras",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "cameras from the library",
	 (void *)ID_CA},
	{"materials",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "objects from the library",
	 (void *)ID_MA},
	{"textures",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "textures from the library",
	 (void *)ID_TE},
	{"images",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "images from the library",
	 (void *)ID_IM},
	{"ipos",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "ipos from the library",
	 (void *)ID_IP},
	{"worlds",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "worlds from the library",
	 (void *)ID_WO},
	{"fonts",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "fonts from the library",
	 (void *)ID_VF},
	{"texts",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "texts from the library",
	 (void *)ID_TXT},
	{"groups",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "groups from the library",
	 (void *)ID_GR},
	{"sounds",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "sounds from the library",
	 (void *)ID_SO},
	{"actions",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "actions from the library",
	 (void *)ID_AC},
	{"armatures",
	 (getter)LibraryData_CreatePyObject, (setter)NULL,
	 "armatures from the library",
	 (void *)ID_AR},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*
 * Define a new library and create a library object.  We don't actually test
 * if the library is valid here since we have to do it when the file is
 * actually accessed later. 
 */

static PyObject *M_Library_Load(PyObject *self, PyObject * args)
{
	char *filename = NULL;
	PyObject *relative = NULL;
	BPy_Library *lib;

	if( !PyArg_ParseTuple( args, "s|O", &filename, &relative ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"expected strings and optional bool as arguments." );

	/* try to create a new object */
	lib = (BPy_Library *)PyObject_NEW( BPy_Library, &Library_Type );
	if( !lib )
		return NULL;

	/* save relative flag value */
	if( relative && PyObject_IsTrue(relative) )
		lib->rel = FILE_STRINGCODE;
	else
		lib->rel = 0;

	/* assign the library filename for future use, then return */
	BLI_strncpy( lib->filename, filename, sizeof(lib->filename) );

	return (PyObject *)lib;
}

static struct PyMethodDef M_Library_methods[] = {
	{"load", (PyCFunction)M_Library_Load, METH_VARARGS,
	"(string) - declare a .blend file for use as a library"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Library_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject Library_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Library",          /* char *tp_name; */
	sizeof( BPy_Library ),      /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,                       /* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) NULL,           /* cmpfunc tp_compare; */
	( reprfunc ) NULL,          /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	                    /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	Library_getseters,          /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*
 * Library module initialization
 */

static char M_newLibrary_doc[] = "The Blender.lib submodule";

PyObject *Library_Init( void )
{
	PyObject *submodule;

	if( PyType_Ready( &Library_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &LibraryData_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.lib", M_Library_methods,
			M_newLibrary_doc );
	return submodule;
}
