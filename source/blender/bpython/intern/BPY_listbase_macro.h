/** 
	$Id$
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
	These are macros to create python lists from base lists.
*/



/** example: DEFFUNC_GETLIST(text) defines a method for getting a list
 * of text blocks */

// Example: _GETLIST(name) -> get_namelist
#define _GETLIST(x) get_##x##list 

// Function definition:
// DEFFUNC_GETLIST_MAIN(name) -> get_namelist(PyObject *self, PyObject *args)
#define DEFFUNC_GETLIST_MAIN(x) \
	PyObject *_GETLIST(x)(PyObject *self, PyObject *args)	\
	{														\
		ID *id;												\
		PyObject *list;										\
		list = PyList_New(0);								\
		id = G.main->##x##.first;  							\
		while (id)											\
		{													\
			PyList_Append(list, PyString_FromString(id->name+2)); \
			id = id->next; \
		} \
		return list; \
	} \

// call the above function
#define GETLISTFUNC(x) _GETLIST(x)
// Prototype for the above function
#define GETLISTPROTO(x) PyObject *_GETLIST(x)(PyObject *, PyObject *)
