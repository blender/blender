/* 
 * $Id: gen_utils.c 11932 2007-09-03 17:28:50Z stiv $
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
 * Contributor(s): Michel Selten, Willian P. Germano, Alex Mole, Ken Hughes,
 * Campbell Barton
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "gen_utils.h" /*This must come first*/

#include "DNA_text_types.h"
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BIF_space.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"

#include "Mathutils.h"

#include "constant.h"

/*---------------------- EXPP_FloatsAreEqual -------------------------
  Floating point comparisons 
  floatStep = number of representable floats allowable in between
   float A and float B to be considered equal. */
int EXPP_FloatsAreEqual(float A, float B, int floatSteps)
{
	int a, b, delta;
    assert(floatSteps > 0 && floatSteps < (4 * 1024 * 1024));
    a = *(int*)&A;
    if (a < 0)	
		a = 0x80000000 - a;
    b = *(int*)&B;
    if (b < 0)	
		b = 0x80000000 - b;
    delta = abs(a - b);
    if (delta <= floatSteps)	
		return 1;
    return 0;
}
/*---------------------- EXPP_VectorsAreEqual -------------------------
  Builds on EXPP_FloatsAreEqual to test vectors */
int EXPP_VectorsAreEqual(float *vecA, float *vecB, int size, int floatSteps){

	int x;
	for (x=0; x< size; x++){
		if (EXPP_FloatsAreEqual(vecA[x], vecB[x], floatSteps) == 0)
			return 0;
	}
	return 1;
}
/*---------------------- EXPP_GetModuleConstant -------------------------
  Helper function for returning a module constant */
PyObject *EXPP_GetModuleConstant(char *module, char *constant)
{
	PyObject *py_module = NULL, *py_dict = NULL, *py_constant = NULL;

	/*Careful to pass the correct Package.Module string here or
	* else you add a empty module somewhere*/
	py_module = PyImport_AddModule(module);
	if(!py_module){   /*null = error returning module*/
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"error encountered with returning module constant..." ) );
	}
	py_dict = PyModule_GetDict(py_module); /*never fails*/

	py_constant = PyDict_GetItemString(py_dict, constant);
	if(!py_constant){   /*null = key not found*/
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"error encountered with returning module constant..." ) );
	}

	return EXPP_incr_ret(py_constant);
}

/*****************************************************************************/
/* Description: This function clamps an int to the given interval	  */
/*							[min, max].	   */
/*****************************************************************************/
int EXPP_ClampInt( int value, int min, int max )
{
	if( value < min )
		return min;
	else if( value > max )
		return max;
	return value;
}

/*****************************************************************************/
/* Description: This function clamps a float to the given interval	 */
/*							[min, max].	 */
/*****************************************************************************/
float EXPP_ClampFloat( float value, float min, float max )
{
	if( value < min )
		return min;
	else if( value > max )
		return max;
	return value;
}

/*****************************************************************************/
/* Description: This function returns true if both given strings are equal,  */
/*		otherwise it returns false.				*/
/*****************************************************************************/
int StringEqual( const char *string1, const char *string2 )
{
	return ( strcmp( string1, string2 ) == 0 );
}


/*****************************************************************************/
/* Description: These functions set an internal string with the given type   */
/*		  and error_msg arguments.				     */
/*****************************************************************************/

PyObject *EXPP_ReturnPyObjError( PyObject * type, char *error_msg )
{				/* same as above, just to change its name smoothly */
	PyErr_SetString( type, error_msg );
	return NULL;
}

int EXPP_ReturnIntError( PyObject * type, char *error_msg )
{
	PyErr_SetString( type, error_msg );
	return -1;
}

int EXPP_intError(PyObject *type, const char *format, ...)
{
	PyObject *error;
	va_list vlist;

	va_start(vlist, format);
	error = PyString_FromFormatV(format, vlist);
	va_end(vlist);

	PyErr_SetObject(type, error);
	Py_DECREF(error);
	return -1;
}
/*Like EXPP_ReturnPyObjError but takes a printf format string and multiple arguments*/
PyObject *EXPP_objError(PyObject *type, const char *format, ...)
{
	PyObject *error;
	va_list vlist;

	va_start(vlist, format);
	error = PyString_FromFormatV(format, vlist);
	va_end(vlist);

	PyErr_SetObject(type, error);
	Py_DECREF(error);
	return NULL;
}

/*****************************************************************************/
/* Description: This function increments the reference count of the given   */
/*			 Python object (usually Py_None) and returns it.    */
/*****************************************************************************/

PyObject *EXPP_incr_ret( PyObject * object )
{
	Py_INCREF( object );
	return ( object );
}

/* return Py_False - to avoid warnings, we use the fact that
 * 0 == False in Python: */
PyObject *EXPP_incr_ret_False()
{
	return Py_BuildValue("i", 0);
}

/* return Py_True - to avoid warnings, we use the fact that
 * 1 == True in Python: */
PyObject *EXPP_incr_ret_True()
{
	return Py_BuildValue("i", 1);
}

void EXPP_incr2( PyObject * ob1, PyObject * ob2 )
{
  	    Py_INCREF( ob1 );
  	    Py_INCREF( ob2 );
}

void EXPP_incr3( PyObject * ob1, PyObject * ob2, PyObject * ob3 )
{
  	    Py_INCREF( ob1 );
  	    Py_INCREF( ob2 );
  	    Py_INCREF( ob3 );
}

void EXPP_decr2( PyObject * ob1, PyObject * ob2 )
{
  	    Py_DECREF( ob1 );
  	    Py_DECREF( ob2 );
}

void EXPP_decr3( PyObject * ob1, PyObject * ob2, PyObject * ob3 )
{
  	    Py_DECREF( ob1 );
  	    Py_DECREF( ob2 );
  	    Py_DECREF( ob3 );
}
/*****************************************************************************/
/* Description: This function maps the event identifier to a string.	  */
/*****************************************************************************/
char *event_to_name( short event )
{
	switch ( event ) {
	case SCRIPT_FRAMECHANGED:
		return "FrameChanged";
	case SCRIPT_ONLOAD:
		return "OnLoad";
	case SCRIPT_ONSAVE:
		return "OnSave";
	case SCRIPT_REDRAW:
		return "Redraw";
	case SCRIPT_RENDER:
		return "Render";
	case SCRIPT_POSTRENDER:
		return "PostRender";
	default:
		return "Unknown";
	}
}

/*****************************************************************************/
/* Description: Checks whether all objects in a PySequence are of a same  */
/*		given type.  Returns 0 if not, 1 on success.		 */
/*****************************************************************************/
int EXPP_check_sequence_consistency( PyObject * seq, PyTypeObject * against )
{
	PyObject *ob;
	int len = PySequence_Length( seq );
	int i, result = 1;

	for( i = 0; i < len; i++ ) {
		ob = PySequence_GetItem( seq, i );
		if( ob == Py_None )
			result = 2;
		else if( ob->ob_type != against ) {
			Py_DECREF( ob );
			return 0;
		}
		Py_DECREF( ob );
	}
	return result;		/* 1 if all of 'against' type, 2 if there are (also) Nones */
}

PyObject *EXPP_tuple_repr( PyObject * self, int size )
{
	PyObject *repr, *item;
	int i;

/*@	note: a value must be built because the list is decrefed!
 * otherwise we have nirvana pointers inside python.. */

	repr = PyString_FromString( "" );
	if( !repr )
		return 0;

	item = PySequence_GetItem( self, 0 );
	PyString_ConcatAndDel( &repr, PyObject_Repr( item ) );
	Py_DECREF( item );

	for( i = 1; i < size; i++ ) {
		item = PySequence_GetItem( self, i );
		PyString_ConcatAndDel( &repr, PyObject_Repr( item ) );
		Py_DECREF( item );
	}

	return repr;
}

/****************************************************************************/
/* Description: searches through a map for a pair with a given name. If the */
/*		 pair is present, its ival is stored in *ival and nonzero is */
/*		 returned. If the pair is absent, zero is returned.	*/
/****************************************************************************/
int EXPP_map_getIntVal( const EXPP_map_pair * map, const char *sval,
			int *ival )
{
	while( map->sval ) {
		if( StringEqual( sval, map->sval ) ) {
			*ival = map->ival;
			return 1;
		}
		++map;
	}
	return 0;
}

/* same as above, but string case is ignored */
int EXPP_map_case_getIntVal( const EXPP_map_pair * map, const char *sval,
			     int *ival )
{
	while( map->sval ) {
		if( !BLI_strcasecmp( sval, map->sval ) ) {
			*ival = map->ival;
			return 1;
		}
		++map;
	}
	return 0;
}

/****************************************************************************/
/* Description: searches through a map for a pair with a given name. If the */
/*		 pair is present, its ival is stored in *ival and nonzero is */
/*	     	returned. If the pair is absent, zero is returned.	     */
/* note: this function is identical to EXPP_map_getIntVal except that the  */
/*		output is stored in a short value.	                   */
/****************************************************************************/
int EXPP_map_getShortVal( const EXPP_map_pair * map,
			  const char *sval, short *ival )
{
	while( map->sval ) {
		if( StringEqual( sval, map->sval ) ) {
			*ival = (short)map->ival;
			return 1;
		}
		++map;
	}
	return 0;
}

/****************************************************************************/
/* Description: searches through a map for a pair with a given ival. If the */
/*		pair is present, a pointer to its name is stored in *sval */
/*		and nonzero is returned. If the pair is absent, zero is	*/
/*		returned.		                                */
/****************************************************************************/
int EXPP_map_getStrVal( const EXPP_map_pair * map, int ival,
			const char **sval )
{
	while( map->sval ) {
		if( ival == map->ival ) {
			*sval = map->sval;
			return 1;
		}
		++map;
	}
	return 0;
}

/* Redraw wrappers */

/* this queues redraws if we're not in background mode: */
void EXPP_allqueue(unsigned short event, short val)
{
	if (!G.background) allqueue(event, val);
}

/************************************************************************/
/* Scriptlink-related functions, used by scene, object, etc. bpyobjects */
/************************************************************************/
PyObject *EXPP_getScriptLinks( ScriptLink * slink, PyObject * value,
			       int is_scene )
{
	PyObject *list = NULL, *tmpstr;
	char *eventname = PyString_AsString(value);
	int i, event = 0;


	if( !eventname )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected event name (string) as argument" );
	
	list = PyList_New( 0 );
	if( !list )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create PyList!" );

	/* actually !scriptlink shouldn't happen ... */
	if( !slink || !slink->totscript )
		return list;
	
	if( !strcmp( eventname, "FrameChanged" ) )
		event = SCRIPT_FRAMECHANGED;
	else if( !strcmp( eventname, "Redraw" ) )
		event = SCRIPT_REDRAW;
	else if( !strcmp( eventname, "Render" ) )
		event = SCRIPT_RENDER;
	else if( is_scene && !strcmp( eventname, "OnLoad" ) )
		event = SCRIPT_ONLOAD;
	else if( is_scene && !strcmp( eventname, "OnSave" ) )
		event = SCRIPT_ONSAVE;
	else {
		Py_DECREF(list);
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "invalid event name" );
	}
	
	for( i = 0; i < slink->totscript; i++ ) {
		if( ( slink->flag[i] == event ) && slink->scripts[i] ) {
			tmpstr =PyString_FromString( slink->scripts[i]->name + 2 ); 
			PyList_Append( list, tmpstr );
			Py_DECREF(tmpstr);
		}
	}

	return list;
}

PyObject *EXPP_clearScriptLinks( ScriptLink * slink, PyObject * args )
{
	int i, j, totLinks, deleted = 0;
	PyObject *seq = NULL;
	ID **stmp = NULL;
	short *ftmp = NULL;

	/* check for an optional list of strings */
	if( !PyArg_ParseTuple( args, "|O", &seq ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_TypeError,
			   "expected no arguments or a list of strings" ) );


	/* if there was a parameter, handle it */
	if ( seq != NULL ) {
		/* check that parameter IS list of strings */
		if ( !PyList_Check ( seq ) )
			return ( EXPP_ReturnPyObjError
				 ( PyExc_TypeError,
				   "expected a list of strings" ) );

		totLinks = PyList_Size ( seq );
		for ( i = 0 ; i < totLinks ; ++i ) {
			if ( !PyString_Check ( PySequence_GetItem( seq, i ) ) )
				return ( EXPP_ReturnPyObjError
					 ( PyExc_TypeError,
					   "expected list to contain strings" ) );
		}

		/*
		  parameters OK: now look for each script, and delete
		  its link as we find it (this handles multiple links)
		*/
		for ( i = 0 ; i < totLinks ; ++i )
		{
			char *str;
			str = PyString_AsString ( PySequence_GetItem( seq, i ) );
			for ( j = 0 ; j < slink->totscript ; ++j ) {
				if ( slink->scripts[j] && !strcmp( slink->scripts[j]->name+2, str ) )
					slink->scripts[j] = NULL;
				if( !slink->scripts[j] )
					++deleted; 
			}
		}
	}
	/* if no parameter, then delete all scripts */
	else {
		deleted = slink->totscript;
	}

	/*
	   if not all scripts deleted, create new lists and copy remaining
	   links to them
	*/

	if ( slink->totscript > deleted ) {
		slink->totscript = slink->totscript - (short)deleted;

		stmp = slink->scripts;
		slink->scripts =
			MEM_mallocN( sizeof( ID * ) * ( slink->totscript ),
				     "bpySlinkL" );

		ftmp = slink->flag;
		slink->flag =
			MEM_mallocN( sizeof( short * ) * ( slink->totscript ),
				     "bpySlinkF" );

		for ( i = 0, j = 0 ; i < slink->totscript ; ++j ) {
			if ( stmp[j] != NULL ) {
				memcpy( slink->scripts+i, stmp+j, sizeof( ID * ) );
				memcpy( slink->flag+i, ftmp+j, sizeof( short ) );
				++i;
			}
		}
		MEM_freeN( stmp );
		MEM_freeN( ftmp );

		/*EXPP_allqueue (REDRAWBUTSSCRIPT, 0 );*/
		slink->actscript = 1;
	} else {

	/* all scripts deleted, so delete entire list and free memory */

		if( slink->scripts )
			MEM_freeN( slink->scripts );
		if( slink->flag )
			MEM_freeN( slink->flag );

		slink->scripts = NULL;
		slink->flag = NULL;
		slink->totscript = slink->actscript = 0;
	}

	return EXPP_incr_ret( Py_None );
}


PyObject *EXPP_addScriptLink(ScriptLink *slink, PyObject *args, int is_scene)
{
	int event = 0, found_txt = 0;
	void *stmp = NULL, *ftmp = NULL;
	Text *bltxt = G.main->text.first;
	char *textname = NULL;
	char *eventname = NULL;

	/* !scriptlink shouldn't happen ... */
	if( !slink ) {
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"internal error: no scriptlink!" );
	}

	if( !PyArg_ParseTuple( args, "ss", &textname, &eventname ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
	    "expected two strings as arguments" );

	while( bltxt ) {
		if( !strcmp( bltxt->id.name + 2, textname ) ) {
			found_txt = 1;
			break;
		}
		bltxt = bltxt->id.next;
	}

	if( !found_txt )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
	    "no such Blender Text" );

	if( !strcmp( eventname, "FrameChanged" ) )
		event = SCRIPT_FRAMECHANGED;
	else if( !strcmp( eventname, "Redraw" ) )
		event = SCRIPT_REDRAW;
	else if( !strcmp( eventname, "Render" ) )
		event = SCRIPT_RENDER;
	else if( is_scene && !strcmp( eventname, "OnLoad" ) )
		event = SCRIPT_ONLOAD;
	else if( is_scene && !strcmp( eventname, "OnSave" ) )
		event = SCRIPT_ONSAVE;
	else
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
			"invalid event name" );

	stmp = slink->scripts;
	slink->scripts =
		MEM_mallocN( sizeof( ID * ) * ( slink->totscript + 1 ),
			     "bpySlinkL" );

	ftmp = slink->flag;
	slink->flag =
		MEM_mallocN( sizeof( short * ) * ( slink->totscript + 1 ),
			     "bpySlinkF" );

	if( slink->totscript ) {
		memcpy( slink->scripts, stmp,
			sizeof( ID * ) * ( slink->totscript ) );
		MEM_freeN( stmp );

		memcpy( slink->flag, ftmp,
			sizeof( short ) * ( slink->totscript ) );
		MEM_freeN( ftmp );
	}

	slink->scripts[slink->totscript] = ( ID * ) bltxt;
	slink->flag[slink->totscript] = (short)event;

	slink->totscript++;

	if( slink->actscript < 1 )
		slink->actscript = 1;

	return EXPP_incr_ret (Py_None);		/* normal exit */
}

/*
 * Utility routines to clamp and store various datatypes.  The object type 
 * is checked and a exception is raised if it's not the correct type.  
 *
 * Inputs:
 *    value: PyObject containing the new value
 *    param: pointer to destination variable
 *    max, min: range of values for clamping
 *    type: kind of pointer and data (uses the same characters as
 *       PyArgs_ParseTuple() and Py_BuildValue()
 *
 * Return 0 on success, -1 on error.
 */

int EXPP_setFloatClamped( PyObject *value, float *param,
								float min, float max )
{
	if( !PyNumber_Check ( value ) ) {
		char errstr[128];
		sprintf ( errstr, "expected float argument in [%f,%f]", min, max );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

	*param = EXPP_ClampFloat( (float)PyFloat_AsDouble( value ), min, max );

	return 0;
}

int EXPP_setIValueClamped( PyObject *value, void *param,
								int min, int max, char type )
{
	int number;

	if( !PyInt_Check( value ) ) {
		char errstr[128];
		sprintf ( errstr, "expected int argument in [%d,%d]", min, max );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}

	number = PyInt_AS_LONG( value );

	switch ( type ) {
	case 'b':
		*(char *)param = (char)EXPP_ClampInt( number, min, max );
		return 0;
	case 'h':
		*(short *)param = (short)EXPP_ClampInt( number, min, max );
		return 0;
	case 'H':
		*(unsigned short *)param = (unsigned short)EXPP_ClampInt( number, min, max );
		return 0;
	case 'i':
		*(int *)param = EXPP_ClampInt( number, min, max );
		return 0;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			   "EXPP_setIValueClamped(): invalid type code" );
	}
}

int EXPP_setVec3Clamped( PyObject *value, float *param,
								float min, float max )
{	
	if( VectorObject_Check( value ) ) {
		VectorObject *vect = (VectorObject *)value;
		if( vect->size == 3 ) {
			param[0] = EXPP_ClampFloat( vect->vec[0], min, max );
			param[1] = EXPP_ClampFloat( vect->vec[1], min, max );
			param[2] = EXPP_ClampFloat( vect->vec[2], min, max );
			return 0;
		}
	}
	
	if (1) {
		char errstr[128];
		sprintf ( errstr, "expected vector argument in [%f,%f]", min, max );
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );
	}
}


/*
 * Utility routines to range-check and store various datatypes.  The object 
 * type is checked and a exception is raised if it's not the correct type.  
 * An exception is also raised if the value lies outside of the specified
 * range.  
 *
 * Inputs:
 *    value: PyObject containing the new value
 *    param: pointer to destination variable
 *    max, min: valid range for value
 *    type: kind of pointer and data (uses the same characters as
 *       PyArgs_ParseTuple() and Py_BuildValue()
 *
 * Return 0 on success, -1 on error.
 */

int EXPP_setFloatRange( PyObject *value, float *param,
								float min, float max )
{
	char errstr[128];
	float number;

	sprintf ( errstr, "expected int argument in [%f,%f]", min, max );

	if( !PyNumber_Check ( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );

	number = (float)PyFloat_AsDouble( value );
	if ( number < min || number > max )
		return EXPP_ReturnIntError( PyExc_ValueError, errstr );

	*param = number;
	return 0;
}

int EXPP_setIValueRange( PyObject *value, void *param,
								int min, int max, char type )
{
	char errstr[128];
	int number;

	sprintf ( errstr, "expected int argument in [%d,%d]", min, max );

	if( !PyInt_Check ( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError, errstr );

	number = PyInt_AS_LONG( value );
	if( number < min || number > max )
		return EXPP_ReturnIntError( PyExc_ValueError, errstr );

	switch ( type ) {
	case 'b':
		*(char *)param = (char)number;
		return 0;
	case 'h':
		*(short *)param = (short)number;
		return 0;
	case 'H':
		*(unsigned short *)param = (unsigned short)number;
		return 0;
	case 'i':
		*(int *)param = number;
		return 0;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			   "EXPP_setIValueRange(): invalid type code" );
	}
}

/*
 * Utility routines to handle all attribute setters which use module
 * constants.  Generic pointer to destination variable is used, and typecast
 * to the appropriate type based on the "type" specifier.
 *
 * Inputs:
 *    constant: constant_Type value 
 *    param: pointer to destination variable
 *    type: kind of pointer and data
 *
 * Return 0 on success, -1 on error.
 */

int EXPP_setModuleConstant ( BPy_constant *constant, void *param, char type )
{
	PyObject *item;

	if( constant->ob_type != &constant_Type )
		return EXPP_ReturnIntError( PyExc_TypeError,
			   "expected module constant" );

	item = PyDict_GetItemString( constant->dict, "value" );
	if( !item )
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			   "module constant has no \"value\" key" );

	switch ( type ) {
	case 'h':
		*(short *)param = (short)PyInt_AS_LONG( item );
		return 0;
	case 'i':
		*(int *)param = PyInt_AS_LONG( item );
		return 0;
	case 'f':
		*(float *)param = (float)PyFloat_AS_DOUBLE( item );
		Py_DECREF(item); /* line above increfs */
		return 0;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			   "EXPP_setModuleConstant(): invalid type code" );
	}
}

/*
 * Utility routines to get/set bits in bitfields.  Adapted from code in 
 * sceneRender.c (thanks, ascotan!).  
 *
 * Inputs:
 *    param: pointer to source/destination variable
 *    setting: the bit to get/set
 *    type: pointer type ('h' == short, 'i' == integer)
 */

PyObject *EXPP_getBitfield( void *param, int setting, char type )
{
	switch ( type ) {
	case 'b':
		return (*(char *)param & setting)
				? EXPP_incr_ret_True() : EXPP_incr_ret_False();
	case 'h':
		return (*(short *)param & setting)
				? EXPP_incr_ret_True() : EXPP_incr_ret_False();
	case 'i':
		return (*(int *)param & setting)
				? EXPP_incr_ret_True() : EXPP_incr_ret_False();
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			   "EXPP_getBit(): invalid type code" );
	}
}

int EXPP_setBitfield( PyObject * value, void *param, int setting, char type )
{
	int param_bool = PyObject_IsTrue( value );

	if( param_bool == -1 )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected True/False or 0/1" );
	
	switch ( type ) {
	case 'b':
		if ( param_bool )
			*(char *)param |= setting;
		else
			*(char *)param &= ~setting;
		return 0;
	case 'h':
		if ( param_bool )
			*(short *)param |= setting;
		else
			*(short *)param &= ~setting;
		return 0;
	case 'i':
		if ( param_bool )
			*(int *)param |= setting;
		else
			*(int *)param &= ~setting;
		return 0;
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
			   "EXPP_setBit(): invalid type code" );
	}
}

/*
 * Procedure to handle older setStuff() methods.  Assumes that argument 
 * is a tuple with one object, and so grabs the object and passes it to
 * the specified tp_getset setter for the corresponding attribute.
 */

PyObject *EXPP_setterWrapper ( PyObject * self, PyObject * args,
				setter func)
{
	int error;

	if ( !PyTuple_Check( args ) || PyTuple_Size( args ) != 1 )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "expected tuple of one item" );

	error = func ( self, PyTuple_GET_ITEM( args, 0 ), NULL );
	if ( !error ) {
		Py_INCREF( Py_None );
		return Py_None;
	} else
		return NULL;
}

/*
 * Procedure to handle older setStuff() methods.  Assumes that argument 
 * is a tuple, so just passes it to the specified tp_getset setter for 
 * the corresponding attribute.
 */

PyObject *EXPP_setterWrapperTuple ( PyObject * self, PyObject * args,
									setter func)
{
	int error;

	if ( !PyTuple_Check( args ) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "expected tuple" );

	error = func ( self, args, NULL );
	if ( !error ) {
		Py_INCREF( Py_None );
		return Py_None;
	} else
		return NULL;
}

/*
 * Helper to keep dictionaries from causing memory leaks.  When some object
 * is just created to be added to the dictionary, its reference count needs
 * to be decremented so it can be reclaimed.
 */

int EXPP_dict_set_item_str( PyObject *dict, char *key, PyObject *value)
{
   	/* add value to dictionary */
	int ret = PyDict_SetItemString(dict, key, value);
	Py_DECREF( value ); /* delete original */
	return ret;
}

/*
 * Helper function for subtypes that what the base types methods.
 * The command below needs to have args modified to have 'self' added at the start
 * ret = PyObject_Call(PyDict_GetItemString(PyList_Type.tp_dict, "sort"), newargs, keywds);
 * 
 * This is not easy with the python API so adding a function here,
 * remember to Py_DECREF the tuple after
 */

PyObject * EXPP_PyTuple_New_Prepend(PyObject *tuple, PyObject *value)
{
	PyObject *item;
	PyObject *new_tuple;
	int i;
	
	i = PyTuple_Size(tuple);
	new_tuple = PyTuple_New(i+1);
	PyTuple_SetItem(new_tuple, 0, value);
	Py_INCREF(value);
	while (i) {
		i--;
		item = PyTuple_GetItem(tuple, i);
		PyTuple_SetItem(new_tuple, i+1, item);
		Py_INCREF(item);
	}
	return new_tuple;
}
