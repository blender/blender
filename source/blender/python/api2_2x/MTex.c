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
 * Inc., 59 Temple Place - Suite 330, Boston, MA    02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Alex Mole, Yehoshua Sapir
 *
 * ***** END GPL LICENSE BLOCK *****
*/
#include "MTex.h" /*This must come first*/

#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "Texture.h"
#include "Object.h"
#include "gen_utils.h"
#include "gen_library.h"

#include <DNA_material_types.h>

/*****************************************************************************/
/* Python BPy_MTex methods declarations:                                     */
/*****************************************************************************/
static PyObject *MTex_setTexMethod( BPy_MTex * self, PyObject * args );

/*****************************************************************************/
/* Python method structure definition for Blender.Texture.MTex module:       */
/*****************************************************************************/
struct PyMethodDef M_MTex_methods[] = {
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_MTex methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_MTex_methods[] = {
	/* name, method, flags, doc */
	{"setTex", ( PyCFunction ) MTex_setTexMethod, METH_VARARGS,
	 "(i) - Set MTex Texture"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python MTex_Type callback function prototypes:                            */
/*****************************************************************************/
static int MTex_compare( BPy_MTex * a, BPy_MTex * b );
static PyObject *MTex_repr( BPy_MTex * self );

#define MTEXGET(x) \
	static PyObject *MTex_get##x( BPy_MTex *self, void *closure );
#define MTEXSET(x) \
	static int MTex_set##x( BPy_MTex *self, PyObject *value, void *closure);
#define MTEXGETSET(x) \
	MTEXGET(x) \
	MTEXSET(x)

MTEXGETSET(Tex)
MTEXGETSET(TexCo)
MTEXGETSET(Object)
MTEXGETSET(UVLayer)
MTEXGETSET(MapTo)
MTEXGETSET(Col)
MTEXGETSET(DVar)
MTEXGETSET(BlendMode)
MTEXGETSET(ColFac)
MTEXGETSET(NorFac)
MTEXGETSET(VarFac)
MTEXGETSET(DispFac)
MTEXGETSET(WarpFac)
MTEXGETSET(Ofs)
MTEXGETSET(Size)
MTEXGETSET(Mapping)
MTEXGETSET(Flag)
MTEXGETSET(ProjX)
MTEXGETSET(ProjY)
MTEXGETSET(ProjZ)
MTEXGETSET(MapToFlag)

/*****************************************************************************/
/* Python get/set methods table                                              */
/*****************************************************************************/

static PyGetSetDef MTex_getseters[] = {
	{ "tex", (getter) MTex_getTex, (setter) MTex_setTex,
		"Texture whose mapping this MTex describes", NULL },
	{ "texco", (getter) MTex_getTexCo, (setter) MTex_setTexCo,
		"Texture coordinate space (UV, Global, etc.)", NULL },
	{ "object", (getter) MTex_getObject, (setter) MTex_setObject,
		"Object whose space to use when texco is Object", NULL },
	{ "uvlayer", (getter) MTex_getUVLayer, (setter) MTex_setUVLayer,
		"Name of the UV layer to use", NULL },
	{ "mapto", (getter) MTex_getMapTo, (setter) MTex_setMapTo,
		"What values the texture affects", NULL },
	{ "col", (getter) MTex_getCol, (setter) MTex_setCol,
		"Color that the texture blends with", NULL },
	{ "dvar", (getter) MTex_getDVar, (setter) MTex_setDVar,
		"Value that the texture blends with when not blending colors", NULL },
	{ "blendmode", (getter) MTex_getBlendMode, (setter) MTex_setBlendMode,
		"Texture blending mode", NULL },
	{ "colfac", (getter) MTex_getColFac, (setter) MTex_setColFac,
		"Factor by which texture affects color", NULL },
	{ "norfac", (getter) MTex_getNorFac, (setter) MTex_setNorFac,
		"Factor by which texture affects normal", NULL },
	{ "varfac", (getter) MTex_getVarFac, (setter) MTex_setVarFac,
		"Factor by which texture affects most variables", NULL },
	{ "dispfac", (getter) MTex_getDispFac, (setter) MTex_setDispFac,
		"Factor by which texture affects displacement", NULL },
	{ "warpfac", (getter) MTex_getWarpFac, (setter) MTex_setWarpFac,
		"Factor by which texture affects warp", NULL },
	{ "ofs", (getter) MTex_getOfs, (setter) MTex_setOfs,
		"Offset to adjust texture space", NULL },
	{ "size", (getter) MTex_getSize, (setter) MTex_setSize,
		"Size to scale texture space", NULL },
	{ "mapping", (getter) MTex_getMapping, (setter) MTex_setMapping,
		"Mapping of texture coordinates (flat, cube, etc.)", NULL },
	{ "stencil", (getter) MTex_getFlag, (setter) MTex_setFlag,
		"Stencil mode", (void*) MTEX_STENCIL },
	{ "neg", (getter) MTex_getFlag, (setter) MTex_setFlag,
		"Negate texture values mode", (void*) MTEX_NEGATIVE },
	{ "noRGB", (getter) MTex_getFlag, (setter) MTex_setFlag,
		"Convert texture RGB values to intensity values",
		(void*) MTEX_RGBTOINT },
	{ "correctNor", (getter) MTex_getFlag, (setter) MTex_setFlag,
		"Correct normal mapping for Texture space and Object space",
		(void*) MTEX_VIEWSPACE },
	{ "fromDupli", (getter) MTex_getFlag, (setter) MTex_setFlag,
		"Dupli's instanced from verts, faces or particles, inherit texture coordinate from their parent",
		(void*) MTEX_DUPLI_MAPTO },
	{ "fromOrig", (getter) MTex_getFlag, (setter) MTex_setFlag,
		"Dupli's derive their object coordinates from the original objects transformation",
		(void*) MTEX_OB_DUPLI_ORIG },
	{ "xproj", (getter) MTex_getProjX, (setter) MTex_setProjX,
		"Projection of X axis to Texture space", NULL },
	{ "yproj", (getter) MTex_getProjY, (setter) MTex_setProjY,
		"Projection of Y axis to Texture space", NULL },
	{ "zproj", (getter) MTex_getProjZ, (setter) MTex_setProjZ,
		"Projection of Z axis to Texture space", NULL },
	{ "mtCol", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to color", (void*) MAP_COL },
	{ "mtNor", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to normals", (void*) MAP_NORM },
	{ "mtCsp", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to specularity color", (void*) MAP_COLSPEC },
	{ "mtCmir", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to mirror color", (void*) MAP_COLMIR },
	{ "mtRef", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to reflectivity", (void*) MAP_REF },
	{ "mtSpec", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to specularity", (void*) MAP_SPEC },
	{ "mtEmit", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to emit value", (void*) MAP_EMIT },
	{ "mtAlpha", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to alpha value", (void*) MAP_ALPHA },
	{ "mtHard", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to hardness", (void*) MAP_HAR },
	{ "mtRayMir", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to RayMir value", (void*) MAP_RAYMIRR },
	{ "mtTranslu", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to translucency", (void*) MAP_TRANSLU },
	{ "mtAmb", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to ambient value", (void*) MAP_AMB },
	{ "mtDisp", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to displacement", (void*) MAP_DISPLACE },
	{ "mtWarp", (getter) MTex_getMapToFlag, (setter) MTex_setMapToFlag,
		"How texture maps to warp", (void*) MAP_WARP },
	{ NULL, NULL, NULL, NULL, NULL }
};



/*****************************************************************************/
/* Python MTex_Type structure definition:                                    */
/*****************************************************************************/

PyTypeObject MTex_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Blender MTex",		/* tp_name */
	sizeof( BPy_MTex ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,		/* tp_dealloc */
	0,			/* tp_print */
	0,	/* tp_getattr */
	0,	/* tp_setattr */
	( cmpfunc ) MTex_compare,	/* tp_compare */
	( reprfunc ) MTex_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0,
  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,	/*    long tp_flags; */
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_MTex_methods,			/* tp_methods */
	0,			/* tp_members */
	MTex_getseters,  /*    struct PyGetSetDef *tp_getset; */
	0,			/*    struct _typeobject *tp_base; */
	0,			/*    PyObject *tp_dict; */
	0,			/*    descrgetfunc tp_descr_get; */
	0,			/*    descrsetfunc tp_descr_set; */
	0,			/*    long tp_dictoffset; */
	0,			/*    initproc tp_init; */
	0,			/*    allocfunc tp_alloc; */
	0,			/*    newfunc tp_new; */
	/*  Low-level free-memory routine */
	0,			/*    freefunc tp_free;  */
	/* For PyObject_IS_GC */
	0,			/*    inquiry tp_is_gc;  */
	0,			/*    PyObject *tp_bases; */
	/* method resolution order */
	0,			/*    PyObject *tp_mro;  */
	0,			/*    PyObject *tp_cache; */
	0,			/*    PyObject *tp_subclasses; */
	0,			/*    PyObject *tp_weaklist; */
	0
};


PyObject *MTex_Init( void )
{
	PyObject *submodule;
/*	PyObject *dict; */

	/* call PyType_Ready() to init dictionaries & such */
	if( PyType_Ready( &MTex_Type) < 0)
		Py_RETURN_NONE;

	submodule = Py_InitModule( "Blender.Texture.MTex", M_MTex_methods );

	return submodule;
}

PyObject *MTex_CreatePyObject( MTex * mtex )
{
	BPy_MTex *pymtex;

	pymtex = ( BPy_MTex * ) PyObject_NEW( BPy_MTex, &MTex_Type );
	if( !pymtex )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_MTex PyObject" );

	pymtex->mtex = mtex;
	return ( PyObject * ) pymtex;
}

MTex *MTex_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_MTex * ) pyobj )->mtex;
}

/*****************************************************************************/
/* Python BPy_MTex methods:                                                  */
/*****************************************************************************/

static PyObject *MTex_setTexMethod( BPy_MTex * self, PyObject * args )
{
	return EXPP_setterWrapper( (void *)self, args, (setter)MTex_setTex );
}

static int MTex_compare( BPy_MTex * a, BPy_MTex * b )
{
	return ( a->mtex == b->mtex ) ? 0 : -1;
}

static PyObject *MTex_repr( BPy_MTex * self )
{
	return PyString_FromFormat( "[MTex]" );
}


/*****************************************************************************/
/* Python BPy_MTex get and set functions:                                    */
/*****************************************************************************/

static PyObject *MTex_getTex( BPy_MTex *self, void *closure )
{
	if( self->mtex->tex )
		return Texture_CreatePyObject( self->mtex->tex );
	else
		Py_RETURN_NONE;
}

static int MTex_setTex( BPy_MTex *self, PyObject *value, void *closure)
{
	return GenericLib_assignData(value, (void **) &self->mtex->tex, 0, 1, ID_TE, 0);
}

static PyObject *MTex_getTexCo( BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->texco );
}

static int MTex_setTexCo( BPy_MTex *self, PyObject *value, void *closure)
{
	int texco;

	if( !PyInt_Check( value ) ) {
		return EXPP_ReturnIntError( PyExc_TypeError,
			"Value must be a member of Texture.TexCo dictionary" );
	}

	texco = PyInt_AsLong( value ) ;

	if (texco != TEXCO_ORCO && texco != TEXCO_REFL && texco != TEXCO_NORM &&
		texco != TEXCO_GLOB && texco != TEXCO_UV && texco != TEXCO_OBJECT &&
		texco != TEXCO_STRESS && texco != TEXCO_TANGENT && texco != TEXCO_WINDOW &&
		texco != TEXCO_VIEW && texco != TEXCO_STICKY )
		return EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a member of Texture.TexCo dictionary" );

	self->mtex->texco = (short)texco;

	return 0;
}

static PyObject *MTex_getObject( BPy_MTex *self, void *closure )
{
	if( self->mtex->object )
		return Object_CreatePyObject( self->mtex->object );
	else
		Py_RETURN_NONE;
}

static int MTex_setObject( BPy_MTex *self, PyObject *value, void *closure)
{
	return GenericLib_assignData(value, (void **) &self->mtex->object, 0, 1, ID_OB, 0);
}

static PyObject *MTex_getUVLayer( BPy_MTex *self, void *closure )
{
	return PyString_FromString(self->mtex->uvname);
}

static int MTex_setUVLayer( BPy_MTex *self, PyObject *value, void *closure)
{
	if ( !PyString_Check(value) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "expected string value" );
	BLI_strncpy(self->mtex->uvname, PyString_AsString(value), 31);
	return 0;
}

static PyObject *MTex_getMapTo( BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->mapto );
}

static int MTex_setMapTo( BPy_MTex *self, PyObject *value, void *closure)
{
	int mapto;

	if( !PyInt_Check( value ) ) {
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected an int" );
	}

	mapto = PyInt_AsLong( value );

	/* This method is deprecated anyway. */
	if ( mapto < 0 || mapto > 16383 ) {
		return EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a sum of values from Texture.MapTo dictionary" );
	}

	self->mtex->mapto = (short)mapto;

	return 0;
}

static PyObject *MTex_getCol( BPy_MTex *self, void *closure )
{
	return Py_BuildValue( "(f,f,f)", self->mtex->r, self->mtex->g,
		self->mtex->b );
}

static int MTex_setCol( BPy_MTex *self, PyObject *value, void *closure)
{
	float rgb[3];
	int i;

	if( !PyArg_ParseTuple( value, "fff",
		&rgb[0], &rgb[1], &rgb[2] ) )

		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 3 floats" );

	for( i = 0; i < 3; ++i )
		if( rgb[i] < 0 || rgb[i] > 1 )
			return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->r = rgb[0];
	self->mtex->g = rgb[1];
	self->mtex->b = rgb[2];

	return 0;
}

static PyObject *MTex_getDVar( BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->def_var);
}

static int MTex_setDVar( BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->def_var = f;

	return 0;
}

static PyObject *MTex_getBlendMode( BPy_MTex *self, void *closure )
{
	return PyInt_FromLong(self->mtex->blendtype);
}

static int MTex_setBlendMode( BPy_MTex *self, PyObject *value, void *closure)
{
	int n;

	if ( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
					    "Value must be member of Texture.BlendModes dictionary" );

	n = PyInt_AsLong(value);

/*	if (n != MTEX_BLEND && n != MTEX_MUL && n != MTEX_ADD &&
		n != MTEX_SUB && n != MTEX_DIV && n != MTEX_DARK &&
		n != MTEX_DIFF && n != MTEX_LIGHT && n != MTEX_SCREEN)*/
	if (n < 0 || n > 8)
	{
		return EXPP_ReturnIntError( PyExc_ValueError,
					    "Value must be member of Texture.BlendModes dictionary" );
	}

	self->mtex->blendtype = (short)n;

	return 0;
}

static PyObject *MTex_getColFac( BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->colfac);
}

static int MTex_setColFac( BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->colfac = f;

	return 0;
}

static PyObject *MTex_getNorFac( BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->norfac);
}

static int MTex_setNorFac( BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 25)
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,25]" );

	self->mtex->norfac = f;

	return 0;
}

static PyObject *MTex_getVarFac( BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->varfac);
}

static int MTex_setVarFac( BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->varfac = f;

	return 0;
}

static PyObject *MTex_getDispFac( BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->dispfac);
}

static int MTex_setDispFac( BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->dispfac = f;

	return 0;
}

static PyObject *MTex_getWarpFac( BPy_MTex *self, void *closure )
{
	return PyFloat_FromDouble(self->mtex->warpfac);
}

static int MTex_setWarpFac( BPy_MTex *self, PyObject *value, void *closure)
{
	float f;

	if ( !PyFloat_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
						"expected a float" );

	f = (float)PyFloat_AsDouble(value);

	if (f < 0 || f > 1)
		return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [0,1]" );

	self->mtex->warpfac = f;

	return 0;
}

static PyObject *MTex_getOfs( BPy_MTex *self, void *closure )
{
	return Py_BuildValue( "(f,f,f)", self->mtex->ofs[0], self->mtex->ofs[1],
		self->mtex->ofs[2] );
}

static int MTex_setOfs( BPy_MTex *self, PyObject *value, void *closure)
{
	float f[3];
	int i;

	if( !PyArg_ParseTuple( value, "fff", &f[0], &f[1], &f[2] ) )

		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 3 floats" );

	for( i = 0; i < 3; ++i )
		if( f[i] < -10 || f[i] > 10 )
			return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [-10,10]" );

	self->mtex->ofs[0] = f[0];
	self->mtex->ofs[1] = f[1];
	self->mtex->ofs[2] = f[2];

	return 0;
}

static PyObject *MTex_getSize( BPy_MTex *self, void *closure )
{
	return Py_BuildValue( "(f,f,f)", self->mtex->size[0], self->mtex->size[1],
		self->mtex->size[2] );
}

static int MTex_setSize( BPy_MTex *self, PyObject *value, void *closure)
{
	float f[3];
	int i;

	if( !PyArg_ParseTuple( value, "fff", &f[0], &f[1], &f[2] ) )

		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected tuple of 3 floats" );

	for( i = 0; i < 3; ++i )
		if( f[i] < -100 || f[i] > 100 )
			return EXPP_ReturnIntError( PyExc_ValueError,
					      "values must be in range [-100,100]" );

	self->mtex->size[0] = f[0];
	self->mtex->size[1] = f[1];
	self->mtex->size[2] = f[2];

	return 0;
}

static PyObject *MTex_getMapping( BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->mapping );
}

static int MTex_setMapping( BPy_MTex *self, PyObject *value, void *closure)
{
	int n;

	if ( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"Value must be member of Texture.Mappings dictionary" );

	n = PyInt_AsLong(value);

/*	if (n != MTEX_FLAT && n != MTEX_TUBE && n != MTEX_CUBE &&
		n != MTEX_SPHERE) */
	if (n < 0 || n > 3)
	{
		return EXPP_ReturnIntError( PyExc_ValueError,
			    "Value must be member of Texture.Mappings dictionary" );
	}

	self->mtex->mapping = (char)n;

	return 0;
}

static PyObject *MTex_getFlag( BPy_MTex *self, void *closure )
{
	return PyBool_FromLong( self->mtex->texflag & (GET_INT_FROM_POINTER(closure)) );
}

static int MTex_setFlag( BPy_MTex *self, PyObject *value, void *closure)
{
	if ( !PyBool_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a bool");

	if ( value == Py_True )
		self->mtex->texflag |= GET_INT_FROM_POINTER(closure);
	else
		self->mtex->texflag &= ~(GET_INT_FROM_POINTER(closure));

	return 0;
}

static PyObject *MTex_getProjX( BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->projx );
}

static int MTex_setProjX( BPy_MTex *self, PyObject *value, void *closure)
{
	int proj;

	if( !PyInt_Check( value ) ) {
		return EXPP_ReturnIntError( PyExc_TypeError,
			"Value must be a member of Texture.Proj dictionary" );
	}

	proj = PyInt_AsLong( value ) ;

	/* valid values are from PROJ_N to PROJ_Z = 0 to 3 */
	if (proj < 0 || proj > 3)
		return EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a member of Texture.Proj dictionary" );

	self->mtex->projx = (char)proj;

	return 0;
}

static PyObject *MTex_getProjY( BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->projy );
}

static int MTex_setProjY( BPy_MTex *self, PyObject *value, void *closure )
{
	int proj;

	if( !PyInt_Check( value ) ) {
		return EXPP_ReturnIntError( PyExc_TypeError,
			"Value must be a member of Texture.Proj dictionary" );
	}

	proj = PyInt_AsLong( value ) ;

	/* valid values are from PROJ_N to PROJ_Z = 0 to 3 */
	if (proj < 0 || proj > 3)
		return EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a member of Texture.Proj dictionary" );

	self->mtex->projy = (char)proj;

	return 0;
}

static PyObject *MTex_getProjZ( BPy_MTex *self, void *closure )
{
	return PyInt_FromLong( self->mtex->projz );
}

static int MTex_setProjZ( BPy_MTex *self, PyObject *value, void *closure)
{
	int proj;

	if( !PyInt_Check( value ) ) {
		return EXPP_ReturnIntError( PyExc_TypeError,
			"Value must be a member of Texture.Proj dictionary" );
	}

	proj = PyInt_AsLong( value ) ;

	/* valid values are from PROJ_N to PROJ_Z = 0 to 3 */
	if (proj < 0 || proj > 3)
		return EXPP_ReturnIntError( PyExc_ValueError,
			"Value must be a member of Texture.Proj dictionary" );

	self->mtex->projz = (char)proj;

	return 0;
}

static PyObject *MTex_getMapToFlag( BPy_MTex *self, void *closure )
{
	int flag = GET_INT_FROM_POINTER(closure);

	if ( self->mtex->mapto & flag )
	{
		return PyInt_FromLong( ( self->mtex->maptoneg & flag ) ? -1 : 1 );
	} else {
		return PyInt_FromLong( 0 );
	}
}

static int MTex_setMapToFlag( BPy_MTex *self, PyObject *value, void *closure)
{
	int flag = GET_INT_FROM_POINTER(closure);
	int intVal;

	if ( !PyInt_Check( value ) )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int");

	intVal = PyInt_AsLong( value );

	if (flag == MAP_COL || flag == MAP_COLSPEC || flag == MAP_COLMIR ||
		flag == MAP_WARP) {
		if (intVal < 0 || intVal > 1) {
			return EXPP_ReturnIntError( PyExc_ValueError,
				"value for that mapping must be 0 or 1" );
		}
	} else {
		if (intVal < -1 || intVal > 1) {
			return EXPP_ReturnIntError( PyExc_ValueError,
				"value for that mapping must be -1, 0 or 1" );
		}
	}

	switch (intVal)
	{
	case 0:
		self->mtex->mapto &= ~flag;
		self->mtex->maptoneg &= ~flag;
		break;

	case 1:
		self->mtex->mapto |= flag;
		self->mtex->maptoneg &= ~flag;
		break;

	case -1:
		self->mtex->mapto |= flag;
		self->mtex->maptoneg |= flag;
		break;
	}

	return 0;
}
