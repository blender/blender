/* 
 * $Id: sceneRadio.c 10270 2007-03-15 01:47:53Z campbellbarton $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can Redistribute it and/or
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

#include "sceneRadio.h"		/*This must come first*/

#include "BKE_global.h"
#include "BKE_object.h"		/* disable_where_script() */
#include "gen_utils.h"
#include "constant.h"
#include "radio.h"


/* bitflags */
#define EXPP_RADIO_flag_SHOWLIM 1
#define EXPP_RADIO_flag_Z 2
/* shorts */
#define EXPP_RADIO_hemires_MIN 100
#define EXPP_RADIO_hemires_MAX 1000
#define EXPP_RADIO_maxiter_MIN 0
#define EXPP_RADIO_maxiter_MAX 10000
#define EXPP_RADIO_subshootp_MIN 0
#define EXPP_RADIO_subshootp_MAX 10
#define EXPP_RADIO_subshoote_MIN 0
#define EXPP_RADIO_subshoote_MAX 10
#define EXPP_RADIO_nodelim_MIN 0
#define EXPP_RADIO_nodelim_MAX 50
#define EXPP_RADIO_maxsublamp_MIN 1
#define EXPP_RADIO_maxsublamp_MAX 250
#define EXPP_RADIO_pama_MIN 10
#define EXPP_RADIO_pama_MAX 1000
#define EXPP_RADIO_pami_MIN 10
#define EXPP_RADIO_pami_MAX 1000
#define EXPP_RADIO_elma_MIN 1
#define EXPP_RADIO_elma_MAX 500
#define EXPP_RADIO_elmi_MIN 1
#define EXPP_RADIO_elmi_MAX 100
/* ints */
#define EXPP_RADIO_maxnode_MIN 1
#define EXPP_RADIO_maxnode_MAX 250000
/* floats */
#define EXPP_RADIO_convergence_MIN 0.0
#define EXPP_RADIO_convergence_MAX 0.1f
#define EXPP_RADIO_radfac_MIN 0.001f
#define EXPP_RADIO_radfac_MAX 250.0
#define EXPP_RADIO_gamma_MIN 0.2f
#define EXPP_RADIO_gamma_MAX 10.0
/* drawtypes */
#define EXPP_RADIO_drawtype_WIRE 0
#define EXPP_RADIO_drawtype_SOLID 1
#define EXPP_RADIO_drawtype_GOURAUD 2

static int EXPP_check_scene( Scene * scene )
{
	if( scene != G.scene ) {
		PyErr_SetString( PyExc_EnvironmentError,
				 "\nradiosity only works on the current scene, check scene.makeCurrent()." );
		return 0;
	} else if( !scene->radio ) {
		PyErr_SetString( PyExc_EnvironmentError,
				 "\nradiosity data was deleted from scene!" );
		return 0;
	}

	return 1;
}

static PyObject *Radio_collectMeshes( BPy_Radio * self );
static PyObject *Radio_go( BPy_Radio * self );
static PyObject *Radio_freeData( BPy_Radio * self );
static PyObject *Radio_replaceMeshes( BPy_Radio * self );
static PyObject *Radio_addMesh( BPy_Radio * self );
static PyObject *Radio_filterFaces( BPy_Radio * self );
static PyObject *Radio_filterElems( BPy_Radio * self );
static PyObject *Radio_limitSubdivide( BPy_Radio * self );
static PyObject *Radio_subdividePatches( BPy_Radio * self );
static PyObject *Radio_subdivideElems( BPy_Radio * self );
static PyObject *Radio_removeDoubles( BPy_Radio * self );

static PyObject *Radio_repr( BPy_Radio * self );

static PyObject *EXPP_create_ret_PyInt( int value )
{
	PyObject *pyval = PyInt_FromLong( value );

	if( !pyval )
		PyErr_SetString( PyExc_MemoryError,
				 "couldn't create py int!" );

	return pyval;
}

static PyObject *EXPP_create_ret_PyFloat( float value )
{
	PyObject *pyval = PyFloat_FromDouble( ( double ) value );

	if( !pyval )
		PyErr_SetString( PyExc_MemoryError,
				 "couldn't create py int!" );

	return pyval;
}

static PyObject *Radio_get_hemires( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->hemires );
}

static PyObject *Radio_get_maxiter( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->maxiter );
}

static PyObject *Radio_get_subshootp( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->subshootp );
}

static PyObject *Radio_get_subshoote( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->subshoote );
}

static PyObject *Radio_get_nodelim( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->nodelim );
}

static PyObject *Radio_get_maxsublamp( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->maxsublamp );
}

static PyObject *Radio_get_pama( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->pama );
}

static PyObject *Radio_get_pami( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->pami );
}

static PyObject *Radio_get_elma( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->elma );
}

static PyObject *Radio_get_elmi( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->elmi );
}

static PyObject *Radio_get_drawtype( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->drawtype );
}

static PyObject *Radio_get_flag( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->flag );
}

static PyObject *Radio_get_maxnode( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyInt( ( int ) self->scene->radio->maxnode );
}

static PyObject *Radio_get_convergence( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyFloat( self->scene->radio->convergence );
}

static PyObject *Radio_get_radfac( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyFloat( self->scene->radio->radfac );
}

static PyObject *Radio_get_gamma( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_create_ret_PyFloat( self->scene->radio->gamma );
}

static PyObject *EXPP_unpack_set_int( PyObject * args, int *ptr,
				      int min, int max )
{
	int value;

	if( !PyArg_ParseTuple( args, "i", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int argument" );

	*ptr = EXPP_ClampInt( value, min, max );

	return EXPP_incr_ret( Py_None );
}

/* could merge with set_int, but is cleaner this way */
static PyObject *EXPP_unpack_set_short( PyObject * args, short *ptr,
					short min, short max )
{
	int value;

	if( !PyArg_ParseTuple( args, "i", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int argument" );

	*ptr = ( short ) EXPP_ClampInt( value, min, max );

	return EXPP_incr_ret( Py_None );
}

static PyObject *EXPP_unpack_set_float( PyObject * args, float *ptr,
					float min, float max )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected float argument" );

	*ptr = EXPP_ClampFloat( value, min, max );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_set_hemires( BPy_Radio * self, PyObject * args )
{
	PyObject *ret;

	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	ret = EXPP_unpack_set_short( args, &self->scene->radio->hemires,
				     EXPP_RADIO_hemires_MIN,
				     EXPP_RADIO_hemires_MAX );

	if( ret )
		rad_setlimits(  );

	return ret;
}

static PyObject *Radio_set_maxiter( BPy_Radio * self, PyObject * args )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_unpack_set_short( args, &self->scene->radio->maxiter,
				      EXPP_RADIO_maxiter_MIN,
				      EXPP_RADIO_maxiter_MAX );
}

static PyObject *Radio_set_subshootp( BPy_Radio * self, PyObject * args )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_unpack_set_short( args, &self->scene->radio->subshootp,
				      EXPP_RADIO_subshootp_MIN,
				      EXPP_RADIO_subshootp_MAX );
}

static PyObject *Radio_set_subshoote( BPy_Radio * self, PyObject * args )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_unpack_set_short( args, &self->scene->radio->subshoote,
				      EXPP_RADIO_subshoote_MIN,
				      EXPP_RADIO_subshoote_MAX );
}

static PyObject *Radio_set_nodelim( BPy_Radio * self, PyObject * args )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_unpack_set_short( args, &self->scene->radio->nodelim,
				      EXPP_RADIO_nodelim_MIN,
				      EXPP_RADIO_nodelim_MAX );
}

static PyObject *Radio_set_maxsublamp( BPy_Radio * self, PyObject * args )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_unpack_set_short( args, &self->scene->radio->maxsublamp,
				      EXPP_RADIO_maxsublamp_MIN,
				      EXPP_RADIO_maxsublamp_MAX );
}

static PyObject *Radio_set_pama( BPy_Radio * self, PyObject * args )
{
	PyObject *ret;

	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	ret = EXPP_unpack_set_short( args, &self->scene->radio->pama,
				     EXPP_RADIO_pama_MIN,
				     EXPP_RADIO_pama_MAX );

	if( ret )
		rad_setlimits(  );

	return ret;
}

static PyObject *Radio_set_pami( BPy_Radio * self, PyObject * args )
{
	PyObject *ret;

	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	ret = EXPP_unpack_set_short( args, &self->scene->radio->pami,
				     EXPP_RADIO_pami_MIN,
				     EXPP_RADIO_pami_MAX );

	if( ret )
		rad_setlimits(  );

	return ret;
}

static PyObject *Radio_set_elma( BPy_Radio * self, PyObject * args )
{
	PyObject *ret;

	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	ret = EXPP_unpack_set_short( args, &self->scene->radio->elma,
				     EXPP_RADIO_elma_MIN,
				     EXPP_RADIO_elma_MAX );

	if( ret )
		rad_setlimits(  );

	return ret;
}

static PyObject *Radio_set_elmi( BPy_Radio * self, PyObject * args )
{
	PyObject *ret;

	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	ret = EXPP_unpack_set_short( args, &self->scene->radio->elmi,
				     EXPP_RADIO_elmi_MIN,
				     EXPP_RADIO_elmi_MAX );

	if( ret )
		rad_setlimits(  );

	return ret;
}

static PyObject *Radio_set_drawtype( BPy_Radio * self, PyObject * args )
{
	PyObject *pyob = NULL;
	char *str = NULL;
	short dt = EXPP_RADIO_drawtype_WIRE;

	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( !PyArg_ParseTuple( args, "O", &pyob ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int or string and another optional int as arguments" );

	if( PyString_Check( pyob ) ) {
		str = PyString_AsString( pyob );
		if( !str )
			return EXPP_ReturnPyObjError( PyExc_MemoryError,
						      "couldn't create py string!" );
		else if( !strcmp( str, "Wire" ) )
			dt = EXPP_RADIO_drawtype_WIRE;
		else if( !strcmp( str, "Solid" ) )
			dt = EXPP_RADIO_drawtype_SOLID;
		else if( !strcmp( str, "Gouraud" ) )
			dt = EXPP_RADIO_drawtype_GOURAUD;
		else
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
						      "unknown drawtype string" );
	} else if( PyInt_Check( pyob ) ) {
		dt = ( short ) EXPP_ClampInt( PyInt_AsLong( pyob ),
					      EXPP_RADIO_drawtype_WIRE,
					      EXPP_RADIO_drawtype_GOURAUD );
	} else
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int or string as argument" );

	self->scene->radio->drawtype = dt;

	set_radglobal(  );	/* needed to update 3d view(s) */

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_set_flag( BPy_Radio * self, PyObject * args )
{
	int i, imode = 0;
	char *mode[2] = { NULL, NULL };

	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( !PyArg_ParseTuple( args, "|ss", &mode[0], &mode[1] ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string arguments (or nothing)" );

	for( i = 0; i < 2; i++ ) {
		if( !mode[i] )
			break;
		else if( !strcmp( mode[i], "ShowLimits" ) )
			imode |= EXPP_RADIO_flag_SHOWLIM;
		else if( !strcmp( mode[i], "Z" ) )
			imode |= EXPP_RADIO_flag_Z;
		else
			return EXPP_ReturnPyObjError( PyExc_AttributeError,
						      "unknown mode string" );
	}

	self->scene->radio->flag = ( short ) EXPP_ClampInt( imode, 0, 3 );

	set_radglobal(  );	/* needed to update 3d view(s) */

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_set_maxnode( BPy_Radio * self, PyObject * args )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_unpack_set_int( args, &self->scene->radio->maxnode,
				    EXPP_RADIO_maxnode_MIN,
				    EXPP_RADIO_maxnode_MAX );
}

static PyObject *Radio_set_convergence( BPy_Radio * self, PyObject * args )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_unpack_set_float( args, &self->scene->radio->convergence,
				      EXPP_RADIO_convergence_MIN,
				      EXPP_RADIO_convergence_MAX );
}

static PyObject *Radio_set_radfac( BPy_Radio * self, PyObject * args )
{
	PyObject *ret;

	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	ret = EXPP_unpack_set_float( args, &self->scene->radio->radfac,
				     EXPP_RADIO_radfac_MIN,
				     EXPP_RADIO_radfac_MAX );

	if( ret ) {
		set_radglobal(  );
		if( rad_phase(  ) & RAD_PHASE_FACES )
			make_face_tab(  );
		else
			make_node_display(  );
	}

	return ret;
}

static PyObject *Radio_set_gamma( BPy_Radio * self, PyObject * args )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;
	return EXPP_unpack_set_float( args, &self->scene->radio->gamma,
				      EXPP_RADIO_gamma_MIN,
				      EXPP_RADIO_gamma_MAX );
}

static PyMethodDef BPy_Radio_methods[] = {
	{"collectMeshes", ( PyCFunction ) Radio_collectMeshes, METH_NOARGS,
	 "() - Convert selected meshes to patches."},
	{"go", ( PyCFunction ) Radio_go, METH_NOARGS,
	 "() - Start radiosity calculations."},
	{"freeData", ( PyCFunction ) Radio_freeData, METH_NOARGS,
	 "() - Free all memory used by radiosity."},
	{"addMesh", ( PyCFunction ) Radio_addMesh, METH_NOARGS,
	 "() - Add a new mesh with the radio values as vertex colors to Blender."},
	{"replaceMeshes", ( PyCFunction ) Radio_replaceMeshes, METH_NOARGS,
	 "() - Replace input meshes with the one created by radiosity simulation."},
	{"limitSubdivide", ( PyCFunction ) Radio_limitSubdivide, METH_NOARGS,
	 "() - Subdivide patches."},
	{"filterFaces", ( PyCFunction ) Radio_filterFaces, METH_NOARGS,
	 "() - Force an extra smoothing."},
	{"filterElems", ( PyCFunction ) Radio_filterElems, METH_NOARGS,
	 "() - Filter elements to remove aliasing artifacts."},
	{"subdividePatches", ( PyCFunction ) Radio_subdividePatches,
	 METH_NOARGS,
	 "() - Pre-subdivision: detect high-energy patches and subdivide them."},
	{"subdivideElems", ( PyCFunction ) Radio_subdivideElems, METH_NOARGS,
	 "() - Pre-subdivision: detect high-energy elements and subdivide them."},
	{"removeDoubles", ( PyCFunction ) Radio_removeDoubles, METH_NOARGS,
	 "() - Join elements which differ less than the defined node limit."},
	{"getHemiRes", ( PyCFunction ) Radio_get_hemires, METH_NOARGS,
	 "() - Get hemicube size."},
	{"setHemiRes", ( PyCFunction ) Radio_set_hemires, METH_VARARGS,
	 "(int) - Set hemicube size, the range is [100, 1000]."},
	{"getMaxIter", ( PyCFunction ) Radio_get_maxiter, METH_NOARGS,
	 "() - Get maximum number of radiosity rounds."},
	{"setMaxIter", ( PyCFunction ) Radio_set_maxiter, METH_VARARGS,
	 "(i) - Set maximum number of radiosity rounds in [0, 10000]."},
	{"getSubShPatch", ( PyCFunction ) Radio_get_subshootp, METH_NOARGS,
	 "() - Get max number of times environment is tested to detect patches."},
	{"setSubShPatch", ( PyCFunction ) Radio_set_subshootp, METH_VARARGS,
	 "(i) - Set max number of times environment is tested to detect patches.\n\
	Range is [0, 10]."},
	{"getSubShElem", ( PyCFunction ) Radio_get_subshoote, METH_NOARGS,
	 "() - Get number of times environment is tested to detect elements."},
	{"setSubShElem", ( PyCFunction ) Radio_set_subshoote, METH_VARARGS,
	 "(i) - Set number of times environment is tested to detect elements.\n\
	Range is [0, 10]."},
	{"getElemLimit", ( PyCFunction ) Radio_get_nodelim, METH_NOARGS,
	 "() - Get the range for removing doubles."},
	{"setElemLimit", ( PyCFunction ) Radio_set_nodelim, METH_VARARGS,
	 "(i) - Set the range for removing doubles in [0, 50]."},
	{"getMaxSubdivSh", ( PyCFunction ) Radio_get_maxsublamp, METH_NOARGS,
	 "() - Get max number of initial shoot patches evaluated."},
	{"setMaxSubdivSh", ( PyCFunction ) Radio_set_maxsublamp, METH_VARARGS,
	 "(i) - Set max number of initial shoot patches evaluated in [1, 250]."},
	{"getPatchMax", ( PyCFunction ) Radio_get_pama, METH_NOARGS,
	 "() - Get max size of a patch."},
	{"setPatchMax", ( PyCFunction ) Radio_set_pama, METH_VARARGS,
	 "(i) - Set max size of a patch in [10, 1000]."},
	{"getPatchMin", ( PyCFunction ) Radio_get_pami, METH_NOARGS,
	 "() - Get minimum size of a patch."},
	{"setPatchMin", ( PyCFunction ) Radio_set_pami, METH_VARARGS,
	 "(i) - Set minimum size of a patch in [10, 1000]."},
	{"getElemMax", ( PyCFunction ) Radio_get_elma, METH_NOARGS,
	 "() - Get max size of an element."},
	{"setElemMax", ( PyCFunction ) Radio_set_elma, METH_VARARGS,
	 "(i) - Set max size of an element in [1, 100]."},
	{"getElemMin", ( PyCFunction ) Radio_get_elmi, METH_NOARGS,
	 "() - Get minimum size of an element."},
	{"setElemMin", ( PyCFunction ) Radio_set_elmi, METH_VARARGS,
	 "(i) - Set minimum size of an element in [1, 100]."},
	{"getMaxElems", ( PyCFunction ) Radio_get_maxnode, METH_NOARGS,
	 "() - Get maximum number of elements."},
	{"setMaxElems", ( PyCFunction ) Radio_set_maxnode, METH_VARARGS,
	 "(i) - Set maximum nunber of elements in [1, 250000]."},
	{"getConvergence", ( PyCFunction ) Radio_get_convergence, METH_NOARGS,
	 "() - Get lower threshold of unshot energy."},
	{"setConvergence", ( PyCFunction ) Radio_set_convergence, METH_VARARGS,
	 "(f) - Set lower threshold of unshot energy in [0.0, 1.0]."},
	{"getMult", ( PyCFunction ) Radio_get_radfac, METH_NOARGS,
	 "() - Get energy value multiplier."},
	{"setMult", ( PyCFunction ) Radio_set_radfac, METH_VARARGS,
	 "(f) - Set energy value multiplier in [0.001, 250.0]."},
	{"getGamma", ( PyCFunction ) Radio_get_gamma, METH_NOARGS,
	 "() - Get change in the contrast of energy values."},
	{"setGamma", ( PyCFunction ) Radio_set_gamma, METH_VARARGS,
	 "(f) - Set change in the contrast of energy values in [0.2, 10.0]."},
	{"getDrawType", ( PyCFunction ) Radio_get_drawtype, METH_NOARGS,
	 "() - Get the draw type: Wire, Solid or Gouraud as an int value."},
	{"setDrawType", ( PyCFunction ) Radio_set_drawtype, METH_VARARGS,
	 "(i or s) - Set the draw type: wire, solid (default) or gouraud."},
	{"getMode", ( PyCFunction ) Radio_get_flag, METH_NOARGS,
	 "() - Get mode as int (or'ed bitflags), see Radio.Modes dict."},
	{"setMode", ( PyCFunction ) Radio_set_flag, METH_VARARGS,
	 "(|ss) - Set mode flags as strings: 'ShowLimits', 'Z'."},
	{NULL, NULL, 0, NULL}
};

static PyTypeObject Radio_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/*ob_size */
	"Blender Radiosity",	/*tp_name */
	sizeof( BPy_Radio ),	/*tp_basicsize */
	0,			/*tp_itemsize */
	NULL,		/*tp_dealloc */
	0,			/*tp_print */
	0,			/*tp_getattr */
	0,			/*tp_setattr */
	0,			/*tp_compare */
	( reprfunc ) Radio_repr,	/*tp_repr */
	0,			/*tp_as_number */
	0,			/*tp_as_sequence */
	0,			/*tp_as_mapping */
	0,			/*tp_hash */
	0,			/*tp_call */
	0,			/*tp_str */
	0,			/*tp_getattro */
	0,			/*tp_setattro */
	0,			/*tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/*tp_flags */
	"Blender radiosity",	/* tp_doc */
	0,			/* tp_traverse */
	0,			/* tp_clear */
	0,			/* tp_richcompare */
	0,			/* tp_weaklistoffset */
	0,			/* tp_iter */
	0,			/* tp_iternext */
	BPy_Radio_methods,	/* tp_methods */
	0,			/* tp_members */
	0,			/* tp_getset */
	0,			/* tp_base */
	0,			/* tp_dict */
	0,			/* tp_descr_get */
	0,			/* tp_descr_set */
	0,			/* tp_dictoffset */
	0,			/* tp_init */
	0,			/* tp_alloc */
	0,			/* tp_new */
	0, 0, 0, 0, 0, 0, 0, 0,	/* up to tp_del, so we don't get a warning */
};

static PyObject *Radio_repr( BPy_Radio * self )
{
	if( self->radio )
		return PyString_FromFormat( "[Radiosity \"%s\"]",
					    self->scene->id.name + 2 );
	else
		return PyString_FromString( "NULL" );
}

PyObject *Radio_CreatePyObject( struct Scene * scene )
{
	BPy_Radio *py_radio;

	if( scene != G.scene ) {
		return EXPP_ReturnPyObjError( PyExc_EnvironmentError,
					      "\nradiosity only works on the current scene, check scene.makeCurrent()." );
	}

	py_radio = ( BPy_Radio * ) PyObject_NEW( BPy_Radio, &Radio_Type );

	if( !py_radio )
		return NULL;

	if( !scene->radio )
		add_radio(  );	/* adds to G.scene */

	py_radio->radio = scene->radio;
	py_radio->scene = scene;

	return ( ( PyObject * ) py_radio );
}

static PyObject *Radio_collectMeshes( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	disable_where_script( 1 );	/* used to avoid error popups */
	rad_collect_meshes(  );
	disable_where_script( 0 );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_freeData( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	delete_radio(  );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_go( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) == RAD_PHASE_PATCHES )
		rad_go(  );
	else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call radio.collectMeshes() first." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_replaceMeshes( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) & RAD_PHASE_FACES )
		rad_replacemesh(  );
	else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call radio.collectMeshes() and radio.go() first." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_addMesh( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) & RAD_PHASE_FACES )
		rad_addmesh(  );
	else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call radio.collectMeshes() and radio.go() first." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_filterFaces( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) & RAD_PHASE_FACES )
		filterFaces(  );
	else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call radio.collectMeshes() and radio.go() first." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_filterElems( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) & RAD_PHASE_FACES ) {
		set_radglobal(  );
		filterNodes(  );
		make_face_tab(  );
	} else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call radio.collectMeshes() and radio.go() first." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_limitSubdivide( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) == RAD_PHASE_PATCHES )
		rad_limit_subdivide(  );
	else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call this before calculating the radiosity simulation." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_subdividePatches( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) == RAD_PHASE_PATCHES )
		rad_subdivshootpatch(  );
	else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call this before calculating the radiosity simulation." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_subdivideElems( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) == RAD_PHASE_PATCHES )
		rad_subdivshootelem(  );
	else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call radio.collectMeshes() and radio.go() first." );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Radio_removeDoubles( BPy_Radio * self )
{
	if( !EXPP_check_scene( self->scene ) )
		return NULL;

	if( rad_phase(  ) == RAD_PHASE_FACES ) {
		set_radglobal(  );
		removeEqualNodes( self->scene->radio->nodelim );
		make_face_tab(  );
	} else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "you need to call radio.collectMeshes() and radio.go() first." );

	return EXPP_incr_ret( Py_None );
}

static PyMethodDef M_Radio_methods[] = { {NULL, NULL, 0, NULL} };

PyObject *Radio_Init( void )
{
	PyObject *submodule, *Modes, *DrawTypes;

	if( PyType_Ready( &Radio_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.Scene.Radio", M_Radio_methods,
				    "The Blender Radiosity submodule" );

	Modes = PyConstant_New(  );
	DrawTypes = PyConstant_New(  );

	if( Modes ) {
		BPy_constant *d = ( BPy_constant * ) Modes;

		PyConstant_Insert( d, "ShowLimits",
				 PyInt_FromLong( EXPP_RADIO_flag_SHOWLIM ) );
		PyConstant_Insert( d, "Z", PyInt_FromLong( EXPP_RADIO_flag_Z ) );

		PyModule_AddObject( submodule, "Modes", Modes );
	}

	if( DrawTypes ) {
		BPy_constant *d = ( BPy_constant * ) DrawTypes;

		PyConstant_Insert( d, "Wire",
				 PyInt_FromLong( EXPP_RADIO_drawtype_WIRE ) );
		PyConstant_Insert( d, "Solid",
				 PyInt_FromLong( EXPP_RADIO_drawtype_SOLID ) );
		PyConstant_Insert( d, "Gouraud",
				 PyInt_FromLong
				 ( EXPP_RADIO_drawtype_GOURAUD ) );

		PyModule_AddObject( submodule, "DrawTypes", DrawTypes );
	}

	return submodule;
}
