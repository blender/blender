/* 
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten, Willian P. Germano, Alex Mole, Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_gen_library_h
#define EXPP_gen_library_h

#include <Python.h>

#include "DNA_ID.h"
#include "DNA_listBase.h"

/* ID functions for all libdata */
#define	GENERIC_LIB_GETSETATTR \
	{"name",\
	 (getter)GenericLib_getName, (setter)GenericLib_setName,\
	 "name",\
	 NULL},\
	{"lib",\
	 (getter)GenericLib_getLib, (setter)NULL,\
	 "external library path",\
	 NULL},\
	{"users",\
	 (getter)GenericLib_getUsers, (setter)NULL,\
	 "user count",\
	 NULL},\
	{"fakeUser",\
	 (getter)GenericLib_getFakeUser, (setter)GenericLib_setFakeUser,\
	 "fake user state",\
	 NULL},\
	{"properties",\
	 (getter)GenericLib_getProperties, (setter)NULL,\
	 "properties",\
	 NULL},\
	{"tag",\
	 (getter)GenericLib_getTag, (setter)GenericLib_setTag,\
	 "temproary tag",\
	 NULL}

/* Dummy struct for getting the ID from a libdata BPyObject */
typedef struct {
	PyObject_HEAD		/* required python macro */
	ID *id;
} BPy_GenericLib;

int GenericLib_setName( void *self, PyObject *value );
PyObject *GenericLib_getName( void *self );
PyObject *GenericLib_getFakeUser( void *self );
int GenericLib_setFakeUser( void *self, PyObject *value );
PyObject *GenericLib_getTag( void *self );
int GenericLib_setTag( void *self, PyObject *value );
PyObject *GenericLib_getLib( void *self );
PyObject *GenericLib_getUsers( void *self );
PyObject *GenericLib_getProperties( void *self );

/* use this for oldstyle somedata.getName("name") */
PyObject * GenericLib_setName_with_method( void *self, PyObject *value ); 

int GenericLib_assignData(PyObject *value, void **data, void **ndata, short refcount, short type, short subtype);
short GenericLib_getType(PyObject * pydata);

/* Other ID functions */
ID			*GetIdFromList( ListBase * list, char *name );
PyObject	*GetPyObjectFromID( ID * id );
long GenericLib_hash(BPy_GenericLib * pydata);
#endif				/* EXPP_gen_library_h */
