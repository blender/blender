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

#ifndef EXPP_gen_utils_h
#define EXPP_gen_utils_h

#include <Python.h>
#include "DNA_ID.h"
#include "DNA_scriptlink_types.h"
#include "DNA_listBase.h"

#include "constant.h"

#define Py_PI  3.14159265358979323846
#define Py_WRAP 1024
#define Py_NEW  2048

/* 
   Py_RETURN_NONE
   Python 2.4 macro.  
   defined here until we switch to 2.4
*/
#ifndef Py_RETURN_NONE
#define Py_RETURN_NONE  Py_INCREF( Py_None ); return Py_None 
#endif

PyObject *EXPP_GetModuleConstant(char *module, char *constant);

int StringEqual( const char *string1, const char *string2 );
char *GetIdName( ID * id );
ID *GetIdFromList( ListBase * list, char *name );

PyObject *PythonReturnErrorObject( PyObject * type, char *error_msg );
PyObject *PythonIncRef( PyObject * object );

char *event_to_name( short event );

float EXPP_ClampFloat( float value, float min, float max );
int EXPP_ClampInt( int value, int min, int max );

void EXPP_incr2( PyObject * ob1, PyObject * ob2 );
void EXPP_incr3( PyObject * ob1, PyObject * ob2, PyObject * ob3 );
void EXPP_decr2( PyObject * ob1, PyObject * ob2 );
void EXPP_decr3( PyObject * ob1, PyObject * ob2, PyObject * ob3 );
PyObject *EXPP_incr_ret( PyObject * object );
PyObject *EXPP_incr_ret_True(void);
PyObject *EXPP_incr_ret_False(void);
PyObject *EXPP_ReturnPyObjError( PyObject * type, char *error_msg );
int EXPP_ReturnIntError( PyObject * type, char *error_msg );

PyObject *EXPP_objError(PyObject *type, const char *format, ...);
int EXPP_intError(PyObject *type, const char *format, ...);

int EXPP_check_sequence_consistency( PyObject * seq, PyTypeObject * against );
PyObject *EXPP_tuple_repr( PyObject * self, int size );

/* mapping utilities - see Texture.c for an example of how to use these */
typedef struct {
	const char *sval;
	int ival;
} EXPP_map_pair;

/* maps must end with a pair that has NULL as sval */
int EXPP_map_getIntVal( const EXPP_map_pair * map,
			const char *sval, int *ival );
int EXPP_map_case_getIntVal( const EXPP_map_pair * map,
			     const char *sval, int *ival );
int EXPP_map_getShortVal( const EXPP_map_pair * map,
			  const char *sval, short *ival );
int EXPP_map_getStrVal( const EXPP_map_pair * map,
			int ival, const char **sval );

/* clamping and range-checking utilities */

int EXPP_setIValueClamped( PyObject *value, void *param,
		int min, int max, char type );
int EXPP_setFloatClamped ( PyObject *value, float *param,
			float min, float max);
int EXPP_setIValueRange( PyObject *value, void *param,
		int min, int max, char type );
int EXPP_setFloatRange ( PyObject *value, float *param,
			float min, float max);

/* utility routine for PyType attributes setters with module constant */

int EXPP_setModuleConstant ( BPy_constant *constant, void *param,
			char type );

/* utilities to get/set bits in bitfields */

PyObject *EXPP_getBitfield( void *param, int setting, char type );
int EXPP_setBitfield( PyObject * value, void *param, int setting, char type );

/*
 * Procedures to handle older setStuff() methods, which now access 
 * a PyType's setter attributes using the tp_getset mechanism.
 */

PyObject *EXPP_setterWrapper ( PyObject * self, PyObject * args,
				setter func);

PyObject *EXPP_setterWrapperTuple ( PyObject * self, PyObject * args,
				setter func);

/* scriplinks-related: */
PyObject *EXPP_getScriptLinks(ScriptLink *slink, PyObject *args, int is_scene);
PyObject *EXPP_addScriptLink(ScriptLink *slink, PyObject *args, int is_scene);
PyObject *EXPP_clearScriptLinks(ScriptLink *slink, PyObject *args);

/* this queues redraws if we're not in background mode: */
void EXPP_allqueue(unsigned short event, short val);

#endif				/* EXPP_gen_utils_h */

