/*
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Jacques Guignot, Jean-Michel Soler 
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Particle.h" /*This must come first */

#include "DNA_object_types.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BLI_blenlib.h"
#include "gen_utils.h"

/*****************************************************************************/
/* Python API function prototypes for the Particle module.                   */
/*****************************************************************************/
PyObject *M_Particle_New( PyObject * self, PyObject * args );
PyObject *M_Particle_Get( PyObject * self, PyObject * args );

/*****************************************************************************/
/* Python BPy_Particle methods declarations:                                 */
/*****************************************************************************/
PyObject *Effect_getType( BPy_Effect * self );
PyObject *Effect_setType( BPy_Effect * self, PyObject * args );
PyObject *Effect_getFlag( BPy_Effect * self );
PyObject *Effect_setFlag( BPy_Effect * self, PyObject * args );
PyObject *Particle_getSta( BPy_Particle * self );
PyObject *Particle_setSta( BPy_Particle * self, PyObject * a );
PyObject *Particle_getEnd( BPy_Particle * self );
PyObject *Particle_setEnd( BPy_Particle * self, PyObject * a );
PyObject *Particle_getLifetime( BPy_Particle * self );
PyObject *Particle_setLifetime( BPy_Particle * self, PyObject * a );
PyObject *Particle_getNormfac( BPy_Particle * self );
PyObject *Particle_setNormfac( BPy_Particle * self, PyObject * a );
PyObject *Particle_getObfac( BPy_Particle * self );
PyObject *Particle_setObfac( BPy_Particle * self, PyObject * a );
PyObject *Particle_getRandfac( BPy_Particle * self );
PyObject *Particle_setRandfac( BPy_Particle * self, PyObject * a );
PyObject *Particle_getTexfac( BPy_Particle * self );
PyObject *Particle_setTexfac( BPy_Particle * self, PyObject * a );
PyObject *Particle_getRandlife( BPy_Particle * self );
PyObject *Particle_setRandlife( BPy_Particle * self, PyObject * a );
PyObject *Particle_getNabla( BPy_Particle * self );
PyObject *Particle_setNabla( BPy_Particle * self, PyObject * a );
PyObject *Particle_getVectsize( BPy_Particle * self );
PyObject *Particle_setVectsize( BPy_Particle * self, PyObject * a );
PyObject *Particle_getTotpart( BPy_Particle * self );
PyObject *Particle_setTotpart( BPy_Particle * self, PyObject * a );
PyObject *Particle_getTotkey( BPy_Particle * self );
PyObject *Particle_setTotkey( BPy_Particle * self, PyObject * a );
PyObject *Particle_getSeed( BPy_Particle * self );
PyObject *Particle_setSeed( BPy_Particle * self, PyObject * a );
PyObject *Particle_getForce( BPy_Particle * self );
PyObject *Particle_setForce( BPy_Particle * self, PyObject * a );
PyObject *Particle_getMult( BPy_Particle * self );
PyObject *Particle_setMult( BPy_Particle * self, PyObject * a );
PyObject *Particle_getLife( BPy_Particle * self );
PyObject *Particle_setLife( BPy_Particle * self, PyObject * a );
PyObject *Particle_getMat( BPy_Particle * self );
PyObject *Particle_setMat( BPy_Particle * self, PyObject * a );
PyObject *Particle_getChild( BPy_Particle * self );
PyObject *Particle_setChild( BPy_Particle * self, PyObject * a );
PyObject *Particle_getDefvec( BPy_Particle * self );
PyObject *Particle_setDefvec( BPy_Particle * self, PyObject * a );

/*****************************************************************************/
/* Python Particle_Type callback function prototypes:                        */
/*****************************************************************************/
void ParticleDeAlloc( BPy_Particle * msh );
//int ParticlePrint (BPy_Particle *msh, FILE *fp, int flags);
int ParticleSetAttr( BPy_Particle * msh, char *name, PyObject * v );
PyObject *ParticleGetAttr( BPy_Particle * msh, char *name );
PyObject *ParticleRepr( void );
PyObject *ParticleCreatePyObject( struct Effect *particle );
int ParticleCheckPyObject( PyObject * py_obj );
struct Particle *ParticleFromPyObject( PyObject * py_obj );

 
/*****************************************************************************/
/* Python BPy_Particle methods table:                                        */
/*****************************************************************************/
static PyMethodDef BPy_Particle_methods[] = {
	{"getType", ( PyCFunction ) Effect_getType,
	 METH_NOARGS, "() - Return Effect type"},
	{"setType", ( PyCFunction ) Effect_setType,
	 METH_VARARGS, "() - Set Effect type"},
	{"getFlag", ( PyCFunction ) Effect_getFlag,
	 METH_NOARGS, "() - Return Effect flag"},
	{"setFlag", ( PyCFunction ) Effect_setFlag,
	 METH_VARARGS, "() - Set Effect flag"},
	{"getStartTime", ( PyCFunction ) Particle_getSta,
	 METH_NOARGS, "()-Return particle start time"},
	{"setStartTime", ( PyCFunction ) Particle_setSta, METH_VARARGS,
	 "()- Sets particle start time"},
	{"getEndTime", ( PyCFunction ) Particle_getEnd,
	 METH_NOARGS, "()-Return particle end time"},
	{"setEndTime", ( PyCFunction ) Particle_setEnd, METH_VARARGS,
	 "()- Sets particle end time"},
	{"getLifetime", ( PyCFunction ) Particle_getLifetime,
	 METH_NOARGS, "()-Return particle life time"},
	{"setLifetime", ( PyCFunction ) Particle_setLifetime, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getNormfac", ( PyCFunction ) Particle_getNormfac,
	 METH_NOARGS, "()-Return particle life time"},
	{"setNormfac", ( PyCFunction ) Particle_setNormfac, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getObfac", ( PyCFunction ) Particle_getObfac,
	 METH_NOARGS, "()-Return particle life time"},
	{"setObfac", ( PyCFunction ) Particle_setObfac, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getRandfac", ( PyCFunction ) Particle_getRandfac,
	 METH_NOARGS, "()-Return particle life time"},
	{"setRandfac", ( PyCFunction ) Particle_setRandfac, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getTexfac", ( PyCFunction ) Particle_getTexfac,
	 METH_NOARGS, "()-Return particle life time"},
	{"setTexfac", ( PyCFunction ) Particle_setTexfac, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getRandlife", ( PyCFunction ) Particle_getRandlife,
	 METH_NOARGS, "()-Return particle life time"},
	{"setRandlife", ( PyCFunction ) Particle_setRandlife, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getNabla", ( PyCFunction ) Particle_getNabla,
	 METH_NOARGS, "()-Return particle life time"},
	{"setNabla", ( PyCFunction ) Particle_setNabla, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getVectsize", ( PyCFunction ) Particle_getVectsize,
	 METH_NOARGS, "()-Return particle life time"},
	{"setVectsize", ( PyCFunction ) Particle_setVectsize, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getTotpart", ( PyCFunction ) Particle_getTotpart,
	 METH_NOARGS, "()-Return particle life time"},
	{"setTotpart", ( PyCFunction ) Particle_setTotpart, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getTotkey", ( PyCFunction ) Particle_getTotkey,
	 METH_NOARGS, "()-Return particle life time"},
	{"setTotkey", ( PyCFunction ) Particle_setTotkey, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getSeed", ( PyCFunction ) Particle_getSeed,
	 METH_NOARGS, "()-Return particle life time"},
	{"setSeed", ( PyCFunction ) Particle_setSeed, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getForce", ( PyCFunction ) Particle_getForce,
	 METH_NOARGS, "()-Return particle life time"},
	{"setForce", ( PyCFunction ) Particle_setForce, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getMult", ( PyCFunction ) Particle_getMult,
	 METH_NOARGS, "()-Return particle life time"},
	{"setMult", ( PyCFunction ) Particle_setMult, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getLife", ( PyCFunction ) Particle_getLife,
	 METH_NOARGS, "()-Return particle life time"},
	{"setLife", ( PyCFunction ) Particle_setLife, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getMat", ( PyCFunction ) Particle_getMat,
	 METH_NOARGS, "()-Return particle life time"},
	{"setMat", ( PyCFunction ) Particle_setMat, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getChild", ( PyCFunction ) Particle_getChild,
	 METH_NOARGS, "()-Return particle life time"},
	{"setChild", ( PyCFunction ) Particle_setChild, METH_VARARGS,
	 "()- Sets particle life time "},
	{"getDefvec", ( PyCFunction ) Particle_getDefvec,
	 METH_NOARGS, "()-Return particle life time"},
	{"setDefvec", ( PyCFunction ) Particle_setDefvec, METH_VARARGS,
	 "()- Sets particle life time "},
	
	
	{NULL, NULL, 0, NULL}
};

/**************** prototypes ********************/
PyObject *Particle_Init( void );


/*****************************************************************************/
/* Python Particle_Type structure definition:                                */
/*****************************************************************************/

PyTypeObject Particle_Type = {
	PyObject_HEAD_INIT( NULL )
		0,
	"Particle",
	sizeof( BPy_Particle ),
	0,

	( destructor ) ParticleDeAlloc,
	0,
	( getattrfunc ) ParticleGetAttr,
	( setattrfunc ) ParticleSetAttr,
	0,
	( reprfunc ) ParticleRepr,
	0,
	0,
	0,
	0,
	0, 0, 0, 0, 0, 0,
	0,
	0, 0, 0, 0, 0, 0,
	BPy_Particle_methods,
	0,
};
/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Particle.__doc__                                                  */
/*****************************************************************************/
char M_Particle_doc[] = "The Blender Particle module\n\n\
This module provides access to **Object Data** in Blender.\n\
Functions :\n\
	New(object mesh's name) : creates a new part object and adds it to the given mesh object \n\
	Get(name) : retreives a particle  with the given name (mandatory)\n\
	get(name) : same as Get.  Kept for compatibility reasons.\n";
char M_Particle_New_doc[] = "New(name) : creates a new part object and adds it to the given mesh object\n";
char M_Particle_Get_doc[] = "xxx";


/*****************************************************************************/
/* Python method structure definition for Blender.Particle module:           */
/*****************************************************************************/
struct PyMethodDef M_Particle_methods[] = {
	{"New", ( PyCFunction ) M_Particle_New, METH_VARARGS, M_Particle_New_doc},
	{"Get", M_Particle_Get, METH_VARARGS, M_Particle_Get_doc},
	{"get", M_Particle_Get, METH_VARARGS, M_Particle_Get_doc},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Function:              M_Particle_New                                     */
/* Python equivalent:     Blender.Effect.Particle.New                        */
/* Description :          Create a particle effect and add a link            */
/*                        to the given mesh-type Object                      */
/* Data  :                String  mesh object name                           */
/* Return :               pyobject particle                                  */
/*****************************************************************************/
PyObject *M_Particle_New( PyObject * self, PyObject * args )
{
	printf("warning, static particles api removed\n");
	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Function:              M_Particle_Get                                     */
/* Python equivalent:     Blender.Effect.Particle.Get                        */
/*****************************************************************************/
PyObject *M_Particle_Get( PyObject * self, PyObject * args )
{
	printf("warning, static particles api removed\n");
	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Function:              Particle_Init                                      */
/*****************************************************************************/
PyObject *Particle_Init( void )
{
	PyObject *submodule;
	
	if( PyType_Ready( &Particle_Type) < 0)
		return NULL;
	
	submodule =
		Py_InitModule3( "Blender.Particle", M_Particle_methods,	M_Particle_doc );
	return ( submodule );
}

/*****************************************************************************/
/* Python BPy_Particle methods:                                                */
/*****************************************************************************/

PyObject *Particle_getSta( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->sta );
}



PyObject *Particle_setSta( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val = 0;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->sta = val;
	Py_INCREF( Py_None );
	return Py_None;
}

PyObject *Particle_getEnd( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->end );
}



PyObject *Particle_setEnd( BPy_Particle * self, PyObject * args )
{
	float val = 0;
	PartEff *ptr = ( PartEff * ) self->particle;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->end = val;
	Py_INCREF( Py_None );
	return Py_None;
}

PyObject *Particle_getLifetime( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->lifetime );
}



PyObject *Particle_setLifetime( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val = 0;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->lifetime = val;
	Py_INCREF( Py_None );
	return Py_None;
}


PyObject *Particle_getNormfac( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->normfac );
}



PyObject *Particle_setNormfac( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val = 0;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->normfac = val;
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getObfac( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->obfac );
}



PyObject *Particle_setObfac( BPy_Particle * self, PyObject * args )
{
	float val = 0;
	PartEff *ptr = ( PartEff * ) self->particle;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->obfac = val;
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getRandfac( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->randfac );
}



PyObject *Particle_setRandfac( BPy_Particle * self, PyObject * args )
{
	float val = 0;
	PartEff *ptr = ( PartEff * ) self->particle;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->randfac = val;
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getTexfac( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->texfac );
}



PyObject *Particle_setTexfac( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val = 0;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->texfac = val;
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getRandlife( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->randlife );
}



PyObject *Particle_setRandlife( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val = 0;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->randlife = val;
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getNabla( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->nabla );
}



PyObject *Particle_setNabla( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val = 0;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->nabla = val;
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getVectsize( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyFloat_FromDouble( ptr->vectsize );
}



PyObject *Particle_setVectsize( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val = 0;
	if( !PyArg_ParseTuple( args, "f", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected float argument" ) );
	ptr->vectsize = val;
	Py_INCREF( Py_None );
	return Py_None;
}


PyObject *Particle_getTotpart( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyInt_FromLong( ptr->totpart );
}



PyObject *Particle_setTotpart( BPy_Particle * self, PyObject * args )
{
	int val = 0;
	PartEff *ptr = ( PartEff * ) self->particle;
	if( !PyArg_ParseTuple( args, "i", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );
	ptr->totpart = val;
	Py_INCREF( Py_None );
	return Py_None;
}


PyObject *Particle_getTotkey( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyInt_FromLong( ptr->totkey );
}



PyObject *Particle_setTotkey( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	int val = 0;
	if( !PyArg_ParseTuple( args, "i", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );
	ptr->totkey = val;
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getSeed( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return PyInt_FromLong( ptr->seed );
}



PyObject *Particle_setSeed( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	int val = 0;
	if( !PyArg_ParseTuple( args, "i", &val ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) );
	ptr->seed = val;
	Py_INCREF( Py_None );
	return Py_None;
}

PyObject *Particle_getForce( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return Py_BuildValue( "(f,f,f)", ptr->force[0], ptr->force[1],
			      ptr->force[2] );
}


PyObject *Particle_setForce( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val[3];
	if( PyTuple_Size( args ) == 1 )
		args = PyTuple_GetItem( args, 0 );
	val[0] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 0 ) );
	val[1] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 1 ) );
	val[2] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 2 ) );
	/*
	   if (!PyArg_ParseTuple(args, "fff", val,val+1,val+2 ))
	   return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
	   "expected three float arguments"));
	 */
	ptr->force[0] = val[0];
	ptr->force[1] = val[1];
	ptr->force[2] = val[2];
	Py_INCREF( Py_None );
	return Py_None;
}

PyObject *Particle_getMult( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return Py_BuildValue( "(f,f,f,f)",
			      ptr->mult[0], ptr->mult[1], ptr->mult[2],
			      ptr->mult[3] );
}


PyObject *Particle_setMult( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val[4];
	if( PyTuple_Size( args ) == 1 )
		args = PyTuple_GetItem( args, 0 );
	val[0] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 0 ) );
	val[1] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 1 ) );
	val[2] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 2 ) );
	val[3] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 3 ) );
	ptr->mult[0] = val[0];
	ptr->mult[1] = val[1];
	ptr->mult[2] = val[2];
	ptr->mult[3] = val[3];
	Py_INCREF( Py_None );
	return Py_None;
}




PyObject *Particle_getLife( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return Py_BuildValue( "(f,f,f,f)",
			      ptr->life[0], ptr->life[1], ptr->life[2],
			      ptr->life[3] );
}


PyObject *Particle_setLife( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val[4];
	if( PyTuple_Size( args ) == 1 )
		args = PyTuple_GetItem( args, 0 );
	val[0] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 0 ) );
	val[1] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 1 ) );
	val[2] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 2 ) );
	val[3] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 3 ) );
	ptr->life[0] = val[0];
	ptr->life[1] = val[1];
	ptr->life[2] = val[2];
	ptr->life[3] = val[3];
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getChild( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return Py_BuildValue( "(f,f,f,f)",
			      ptr->child[0], ptr->child[1], ptr->child[2],
			      ptr->child[3] );
}


PyObject *Particle_setChild( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val[4];
	if( PyTuple_Size( args ) == 1 )
		args = PyTuple_GetItem( args, 0 );
	val[0] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 0 ) );
	val[1] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 1 ) );
	val[2] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 2 ) );
	val[3] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 3 ) );
	ptr->child[0] = (short)val[0];
	ptr->child[1] = (short)val[1];
	ptr->child[2] = (short)val[2];
	ptr->child[3] = (short)val[3];
	Py_INCREF( Py_None );
	return Py_None;
}



PyObject *Particle_getMat( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return Py_BuildValue( "(f,f,f,f)",
			      ptr->mat[0], ptr->mat[1], ptr->mat[2],
			      ptr->mat[3] );
}


PyObject *Particle_setMat( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val[4];
	if( PyTuple_Size( args ) == 1 )
		args = PyTuple_GetItem( args, 0 );
	val[0] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 0 ) );
	val[1] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 1 ) );
	val[2] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 2 ) );
	val[3] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 3 ) );
	ptr->mat[0] = (short)val[0];
	ptr->mat[1] = (short)val[1];
	ptr->mat[2] = (short)val[2];
	ptr->mat[3] = (short)val[3];
	Py_INCREF( Py_None );
	return Py_None;
}


PyObject *Particle_getDefvec( BPy_Particle * self )
{

	PartEff *ptr = ( PartEff * ) self->particle;
	return Py_BuildValue( "(f,f,f)",
			      ptr->defvec[0], ptr->defvec[1], ptr->defvec[2] );
}


PyObject *Particle_setDefvec( BPy_Particle * self, PyObject * args )
{
	PartEff *ptr = ( PartEff * ) self->particle;
	float val[3];
	if( PyTuple_Size( args ) == 1 )
		args = PyTuple_GetItem( args, 0 );
	val[0] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 0 ) );
	val[1] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 1 ) );
	val[2] = (float)PyFloat_AsDouble( PyTuple_GetItem( args, 2 ) );
	ptr->defvec[0] = val[0];
	ptr->defvec[1] = val[1];
	ptr->defvec[2] = val[2];
	Py_INCREF( Py_None );
	return Py_None;
}


/*****************************************************************************/
/* Function:    ParticleDeAlloc                                              */
/* Description: This is a callback function for the BPy_Particle type. It is   */
/*              the destructor function.                                     */
/*****************************************************************************/
void ParticleDeAlloc( BPy_Particle * self )
{
	PartEff *ptr = ( PartEff * ) self;
	PyObject_DEL( ptr );
}

/*****************************************************************************/
/* Function:    ParticleGetAttr                                              */
/* Description: This is a callback function for the BPy_Particle type. It is */
/*              the function that accesses BPy_Particle "member variables"   */
/*              and  methods.                                                */
/*****************************************************************************/


PyObject *ParticleGetAttr( BPy_Particle * self, char *name )
{

	if( strcmp( name, "seed" ) == 0 )
		return Particle_getSeed( self );
	else if( strcmp( name, "nabla" ) == 0 )
		return Particle_getNabla( self );
	else if( strcmp( name, "sta" ) == 0 )
		return Particle_getSta( self );
	else if( strcmp( name, "end" ) == 0 )
		return Particle_getEnd( self );
	else if( strcmp( name, "lifetime" ) == 0 )
		return Particle_getLifetime( self );
	else if( strcmp( name, "normfac" ) == 0 )
		return Particle_getNormfac( self );
	else if( strcmp( name, "obfac" ) == 0 )
		return Particle_getObfac( self );
	else if( strcmp( name, "randfac" ) == 0 )
		return Particle_getRandfac( self );
	else if( strcmp( name, "texfac" ) == 0 )
		return Particle_getTexfac( self );
	else if( strcmp( name, "randlife" ) == 0 )
		return Particle_getRandlife( self );
	else if( strcmp( name, "vectsize" ) == 0 )
		return Particle_getVectsize( self );
	else if( strcmp( name, "totpart" ) == 0 )
		return Particle_getTotpart( self );
	else if( strcmp( name, "force" ) == 0 )
		return Particle_getForce( self );
	else if( strcmp( name, "mult" ) == 0 )
		return Particle_getMult( self );
	else if( strcmp( name, "life" ) == 0 )
		return Particle_getLife( self );
	else if( strcmp( name, "child" ) == 0 )
		return Particle_getChild( self );
	else if( strcmp( name, "mat" ) == 0 )
		return Particle_getMat( self );
	else if( strcmp( name, "defvec" ) == 0 )
		return Particle_getDefvec( self );


	return Py_FindMethod( BPy_Particle_methods, ( PyObject * ) self,
			      name );
}

/*****************************************************************************/
/* Function:    ParticleSetAttr                                              */
/* Description: This is a callback function for the BPy_Particle type.     */
/*              It is the  function that sets Particle Data attributes     */
/*              (member vars)    */
/*****************************************************************************/
int ParticleSetAttr( BPy_Particle * self, char *name, PyObject * value )
{

	PyObject *valtuple;
	PyObject *error = NULL;

	valtuple = Py_BuildValue( "(N)", value );

	if( !valtuple )
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "ParticleSetAttr: couldn't create PyTuple" );

	if( strcmp( name, "seed" ) == 0 )
		error = Particle_setSeed( self, valtuple );
	else if( strcmp( name, "nabla" ) == 0 )
		error = Particle_setNabla( self, valtuple );
	else if( strcmp( name, "sta" ) == 0 )
		error = Particle_setSta( self, valtuple );
	else if( strcmp( name, "end" ) == 0 )
		error = Particle_setEnd( self, valtuple );
	else if( strcmp( name, "lifetime" ) == 0 )
		error = Particle_setLifetime( self, valtuple );
	else if( strcmp( name, "normfac" ) == 0 )
		error = Particle_setNormfac( self, valtuple );
	else if( strcmp( name, "obfac" ) == 0 )
		error = Particle_setObfac( self, valtuple );
	else if( strcmp( name, "randfac" ) == 0 )
		error = Particle_setRandfac( self, valtuple );
	else if( strcmp( name, "texfac" ) == 0 )
		error = Particle_setTexfac( self, valtuple );
	else if( strcmp( name, "randlife" ) == 0 )
		error = Particle_setRandlife( self, valtuple );
	else if( strcmp( name, "nabla" ) == 0 )
		error = Particle_setNabla( self, valtuple );
	else if( strcmp( name, "vectsize" ) == 0 )
		error = Particle_setVectsize( self, valtuple );
	else if( strcmp( name, "totpart" ) == 0 )
		error = Particle_setTotpart( self, valtuple );
	else if( strcmp( name, "seed" ) == 0 )
		error = Particle_setSeed( self, valtuple );
	else if( strcmp( name, "force" ) == 0 )
		error = Particle_setForce( self, valtuple );
	else if( strcmp( name, "mult" ) == 0 )
		error = Particle_setMult( self, valtuple );
	else if( strcmp( name, "life" ) == 0 )
		error = Particle_setLife( self, valtuple );
	else if( strcmp( name, "child" ) == 0 )
		error = Particle_setChild( self, valtuple );
	else if( strcmp( name, "mat" ) == 0 )
		error = Particle_setMat( self, valtuple );
	else if( strcmp( name, "defvec" ) == 0 )
		error = Particle_setDefvec( self, valtuple );

	else {
		Py_DECREF( valtuple );

		if( ( strcmp( name, "Types" ) == 0 ) ||
		    ( strcmp( name, "Modes" ) == 0 ) )
			return ( EXPP_ReturnIntError( PyExc_AttributeError,
						      "constant dictionary -- cannot be changed" ) );

		else
			return ( EXPP_ReturnIntError( PyExc_KeyError,
						      "attribute not found" ) );
	}

	Py_DECREF(valtuple);
	if( error != Py_None )
		return -1;

	Py_DECREF( Py_None );
	return 0;
}

/*****************************************************************************/
/* Function:    ParticlePrint                                                */
/* Description: This is a callback function for the BPy_Particle type. It    */
/*              particles a meaninful string to 'print' particle objects.    */
/*****************************************************************************/
/*
int ParticlePrint(BPy_Particle *self, FILE *fp, int flags) 
{ 
  printf("Hi, I'm a particle!");	
  return 0;
}
*/
/*****************************************************************************/
/* Function:    ParticleRepr                                                 */
/* Description: This is a callback function for the BPy_Particle type. It    */
/*              particles a meaninful string to represent particle objects.  */
/*****************************************************************************/
PyObject *ParticleRepr( void )
{
	return PyString_FromString( "Particle" );
}

PyObject *ParticleCreatePyObject( struct Effect * particle )
{
	BPy_Particle *blen_object;


	blen_object =
		( BPy_Particle * ) PyObject_NEW( BPy_Particle,
						 &Particle_Type );

	if( blen_object == NULL ) {
		return ( NULL );
	}
	blen_object->particle = particle;
	return ( ( PyObject * ) blen_object );

}

int ParticleCheckPyObject( PyObject * py_obj )
{
	return ( py_obj->ob_type == &Particle_Type );
}


struct Particle *ParticleFromPyObject( PyObject * py_obj )
{
	BPy_Particle *blen_obj;

	blen_obj = ( BPy_Particle * ) py_obj;
	return ( ( struct Particle * ) blen_obj->particle );

}
