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

#ifndef EXPP_gen_utils_h
#define EXPP_gen_utils_h

#include <Python.h>
#include <stdio.h>
#include <string.h>

#include <BKE_global.h>
#include <BKE_main.h>
#include <DNA_ID.h>
#include <DNA_object_types.h>
#include <DNA_material_types.h>
#include <DNA_scriptlink_types.h>
#include <DNA_listBase.h>

int StringEqual (char * string1, char * string2);
char * GetIdName (ID *id);
ID *GetIdFromList(ListBase *list, char *name);

PyObject *PythonReturnErrorObject (PyObject * type, char * error_msg);
PyObject *PythonIncRef (PyObject *object);

char * event_to_name (short event);

float EXPP_ClampFloat (float value, float min, float max);
int   EXPP_ClampInt (int value, int min, int max);

PyObject *EXPP_incr_ret (PyObject *object);
PyObject *EXPP_ReturnPyObjError (PyObject * type, char * error_msg);
int EXPP_ReturnIntError (PyObject *type, char *error_msg);

int EXPP_check_sequence_consistency (PyObject *seq, PyTypeObject *against);
PyObject *EXPP_tuple_repr(PyObject *self, int size);

#endif /* EXPP_gen_utils_h */
