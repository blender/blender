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
 * Contributor(s): Willian P. Germano, Michel Selten, Alex Mole,
 * Alexander Szakaly, Campbell Barton, Ken Hughes
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Material.h" /*This must come first*/

#include "DNA_oops_types.h"
#include "DNA_space_types.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BSE_editipo.h"
#include "BIF_space.h"
#include "mydevice.h"
#include "constant.h"
#include "MTex.h"
#include "Texture.h"
#include "Ipo.h"
#include "gen_utils.h"

/*****************************************************************************/
/* Python BPy_Material defaults: */
/*****************************************************************************/
#define EXPP_MAT_MODE_TRACEABLE			MA_TRACEBLE
#define EXPP_MAT_MODE_SHADOW			MA_SHADOW
#define EXPP_MAT_MODE_SHADELESS			MA_SHLESS
#define EXPP_MAT_MODE_WIRE			MA_WIRE
#define EXPP_MAT_MODE_VCOL_LIGHT		MA_VERTEXCOL
#define EXPP_MAT_MODE_HALO			MA_HALO
#define EXPP_MAT_MODE_ZTRANSP			MA_ZTRA
#define EXPP_MAT_MODE_VCOL_PAINT	        MA_VERTEXCOLP
#define EXPP_MAT_MODE_ZINVERT			MA_ZINV
#define EXPP_MAT_MODE_HALORINGS			MA_HALO_RINGS
#define EXPP_MAT_MODE_ENV			MA_ENV
#define EXPP_MAT_MODE_HALOLINES			MA_HALO_LINES
#define EXPP_MAT_MODE_ONLYSHADOW		MA_ONLYSHADOW
#define EXPP_MAT_MODE_HALOXALPHA		MA_HALO_XALPHA
#define EXPP_MAT_MODE_HALOSTAR			MA_STAR
#define EXPP_MAT_MODE_TEXFACE			MA_FACETEXTURE
#define EXPP_MAT_MODE_HALOTEX			MA_HALOTEX
#define EXPP_MAT_MODE_HALOPUNO		        MA_HALOPUNO
#define EXPP_MAT_MODE_NOMIST			MA_NOMIST
#define EXPP_MAT_MODE_HALOSHADE			MA_HALO_SHADE
#define EXPP_MAT_MODE_HALOFLARE			MA_HALO_FLARE
#define EXPP_MAT_MODE_RADIO			MA_RADIO
#define EXPP_MAT_MODE_RAYMIRROR			MA_RAYMIRROR
#define EXPP_MAT_MODE_ZTRA			MA_ZTRA
#define EXPP_MAT_MODE_RAYTRANSP			MA_RAYTRANSP
#define EXPP_MAT_MODE_ONLYSHADOW		MA_ONLYSHADOW
#define EXPP_MAT_MODE_NOMIST			MA_NOMIST
#define EXPP_MAT_MODE_ENV			MA_ENV
/* Material MIN, MAX values */
#define EXPP_MAT_ADD_MIN			 0.0f
#define EXPP_MAT_ADD_MAX			 1.0f
#define EXPP_MAT_ALPHA_MIN	   0.0f
#define EXPP_MAT_ALPHA_MAX		 1.0f
#define EXPP_MAT_AMB_MIN			 0.0f
#define EXPP_MAT_AMB_MAX			 1.0f
#define EXPP_MAT_COL_MIN			 0.0f /* min/max for all ... */
#define EXPP_MAT_COL_MAX			 1.0f /* ... color triplets  */
#define EXPP_MAT_EMIT_MIN			 0.0f
#define EXPP_MAT_EMIT_MAX			 1.0f
#define EXPP_MAT_REF_MIN			 0.0f
#define EXPP_MAT_REF_MAX			 1.0f
#define EXPP_MAT_SPEC_MIN			 0.0f
#define EXPP_MAT_SPEC_MAX			 2.0f
#define EXPP_MAT_SPECTRA_MIN	 0.0f
#define EXPP_MAT_SPECTRA_MAX	 1.0f

/* Shader spesific settings */
#define EXPP_MAT_SPEC_SHADER_MIN			 0
#define EXPP_MAT_SPEC_SHADER_MAX			 3
#define EXPP_MAT_DIFFUSE_SHADER_MIN			 0
#define EXPP_MAT_DIFFUSE_SHADER_MAX			 4

#define EXPP_MAT_ROUGHNESS_MIN			 0.0f
#define EXPP_MAT_ROUGHNESS_MAX			 3.140f
#define EXPP_MAT_SPECSIZE_MIN			 0.0f
#define EXPP_MAT_SPECSIZE_MAX			 1.530f
#define EXPP_MAT_DIFFUSESIZE_MIN		 0.0f
#define EXPP_MAT_DIFFUSESIZE_MAX			 3.140f
#define EXPP_MAT_SPECSMOOTH_MIN			 0.0f
#define EXPP_MAT_SPECSMOOTH_MAX			 1.0f
#define EXPP_MAT_DIFFUSESMOOTH_MIN			 0.0f
#define EXPP_MAT_DIFFUSESMOOTH_MAX			 1.0f
#define EXPP_MAT_DIFFUSE_DARKNESS_MIN			 0.0f
#define EXPP_MAT_DIFFUSE_DARKNESS_MAX			 2.0f
#define EXPP_MAT_REFRACINDEX_MIN			 1.0f
#define EXPP_MAT_REFRACINDEX_MAX			 10.0f
#define EXPP_MAT_RMS_MIN			 0.0f
#define EXPP_MAT_RMS_MAX			 0.4f
/* End shader settings */

/* diff_shader */
#define MA_DIFF_LAMBERT		0
#define MA_DIFF_ORENNAYAR	1
#define MA_DIFF_TOON		2
#define MA_DIFF_MINNAERT    3

/* spec_shader */
#define MA_SPEC_COOKTORR	0
#define MA_SPEC_PHONG		1
#define MA_SPEC_BLINN		2
#define MA_SPEC_TOON		3
#define MA_SPEC_WARDISO		4

/* shader dicts - Diffuse */
#define EXPP_MAT_SHADER_DIFFUSE_LAMBERT		MA_DIFF_LAMBERT
#define EXPP_MAT_SHADER_DIFFUSE_ORENNAYAR	MA_DIFF_ORENNAYAR
#define EXPP_MAT_SHADER_DIFFUSE_TOON		MA_DIFF_TOON
#define EXPP_MAT_SHADER_DIFFUSE_MINNAERT	MA_DIFF_MINNAERT
/* shader dicts - Specualr */
#define EXPP_MAT_SHADER_SPEC_COOKTORR		MA_SPEC_COOKTORR
#define EXPP_MAT_SHADER_SPEC_PHONG			MA_SPEC_PHONG
#define EXPP_MAT_SHADER_SPEC_BLINN			MA_SPEC_BLINN
#define EXPP_MAT_SHADER_SPEC_TOON			MA_SPEC_TOON
#define EXPP_MAT_SHADER_SPEC_WARDISO		MA_SPEC_WARDISO

#define EXPP_MAT_ZOFFS_MIN			 0.0
#define EXPP_MAT_ZOFFS_MAX			10.0
#define EXPP_MAT_HALOSIZE_MIN			 0.0
#define EXPP_MAT_HALOSIZE_MAX		 100.0
#define EXPP_MAT_FLARESIZE_MIN		 0.1f
#define EXPP_MAT_FLARESIZE_MAX		25.0
#define EXPP_MAT_FLAREBOOST_MIN		 0.1f
#define EXPP_MAT_FLAREBOOST_MAX		10.0
#define EXPP_MAT_SUBSIZE_MIN			 0.1f
#define EXPP_MAT_SUBSIZE_MAX			25.0

#define EXPP_MAT_HARD_MIN				 1
#define EXPP_MAT_HARD_MAX		 255	/* 127 with MODE HALO ON */
#define EXPP_MAT_HALOSEED_MIN		 1
#define EXPP_MAT_HALOSEED_MAX    255
#define EXPP_MAT_NFLARES_MIN		 1
#define EXPP_MAT_NFLARES_MAX		32
#define EXPP_MAT_FLARESEED_MIN	 1
#define EXPP_MAT_FLARESEED_MAX 255
#define EXPP_MAT_NSTARS_MIN			 3
#define EXPP_MAT_NSTARS_MAX			50
#define EXPP_MAT_NLINES_MIN			 0
#define EXPP_MAT_NLINES_MAX		 250
#define EXPP_MAT_NRINGS_MIN			 0
#define EXPP_MAT_NRINGS_MAX			24

#define EXPP_MAT_RAYMIRR_MIN			 0.0
#define EXPP_MAT_RAYMIRR_MAX			 1.0
#define EXPP_MAT_MIRRDEPTH_MIN			 0
#define EXPP_MAT_MIRRDEPTH_MAX			 10
#define EXPP_MAT_FRESNELMIRR_MIN			0.0
#define EXPP_MAT_FRESNELMIRR_MAX			5.0
#define EXPP_MAT_FRESNELMIRRFAC_MIN			1.0
#define EXPP_MAT_FRESNELMIRRFAC_MAX			5.0
#define EXPP_MAT_FILTER_MIN			0.0
#define EXPP_MAT_FILTER_MAX			1.0
#define EXPP_MAT_TRANSLUCENCY_MIN			0.0
#define EXPP_MAT_TRANSLUCENCY_MAX			1.0
#define EXPP_MAT_ZOFFS_MIN				0.0
#define EXPP_MAT_ZOFFS_MAX				10.0
#define EXPP_MAT_IOR_MIN				1.0
#define EXPP_MAT_IOR_MAX				3.0
#define EXPP_MAT_TRANSDEPTH_MIN				0
#define EXPP_MAT_TRANSDEPTH_MAX				10
#define EXPP_MAT_FRESNELTRANS_MIN			0.0
#define EXPP_MAT_FRESNELTRANS_MAX			5.0
#define EXPP_MAT_FRESNELTRANSFAC_MIN			1.0
#define EXPP_MAT_FRESNELTRANSFAC_MAX			5.0
#define EXPP_MAT_SPECTRANS_MIN				0.0
#define EXPP_MAT_SPECTRANS_MAX				1.0
#define EXPP_MAT_MIRRTRANSADD_MIN			0.0
#define EXPP_MAT_MIRRTRANSADD_MAX			1.0


#define IPOKEY_RGB          0
#define IPOKEY_ALPHA        1 
#define IPOKEY_HALOSIZE     2 
#define IPOKEY_MODE         3
#define IPOKEY_ALLCOLOR     10
#define IPOKEY_ALLMIRROR    14
#define IPOKEY_OFS          12
#define IPOKEY_SIZE         13
#define IPOKEY_ALLMAPPING   11




/*****************************************************************************/
/* Python API function prototypes for the Material module.	 */
/*****************************************************************************/
static PyObject *M_Material_New( PyObject * self, PyObject * args,
				 PyObject * keywords );
static PyObject *M_Material_Get( PyObject * self, PyObject * args );

/* Not exposed nor used */
Material *GetMaterialByName( char *name );


/*****************************************************************************/
/* The following string definitions are used for documentation strings.	 */
/* In Python these will be written to the console when doing a		 */
/* Blender.Material.__doc__						 */
/*****************************************************************************/
static char M_Material_doc[] = "The Blender Material module";

static char M_Material_New_doc[] =
	"(name) - return a new material called 'name'\n\
() - return a new material called 'Mat'";

static char M_Material_Get_doc[] =
	"(name) - return the material called 'name', None if not found.\n\
() - return a list of all materials in the current scene.";

/*****************************************************************************/
/* Python method structure definition for Blender.Material module:	 */
/*****************************************************************************/
struct PyMethodDef M_Material_methods[] = {
	{"New", ( PyCFunction ) M_Material_New, METH_VARARGS | METH_KEYWORDS,
	 M_Material_New_doc},
	{"Get", M_Material_Get, METH_VARARGS, M_Material_Get_doc},
	{"get", M_Material_Get, METH_VARARGS, M_Material_Get_doc},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Function:	M_Material_New	 */
/* Python equivalent:		Blender.Material.New */
/*****************************************************************************/
static PyObject *M_Material_New( PyObject * self, PyObject * args,
				 PyObject * keywords )
{
	char *name = "Mat";
	static char *kwlist[] = { "name", NULL };
	BPy_Material *pymat; /* for Material Data object wrapper in Python */
	Material *blmat; /* for actual Material Data we create in Blender */
	char buf[21];

	if( !PyArg_ParseTupleAndKeywords
	    ( args, keywords, "|s", kwlist, &name ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected string or nothing as argument" ) );

	if( strcmp( name, "Mat" ) != 0 )	/* use gave us a name ? */
		PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	blmat = add_material( name );	/* first create the Material Data in Blender */

	if( blmat )		/* now create the wrapper obj in Python */
		pymat = ( BPy_Material * ) Material_CreatePyObject( blmat );
	else
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
						"couldn't create Material Data in Blender" ) );

	blmat->id.us = 0;	/* was incref'ed by add_material() above */

	if( pymat == NULL )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create Material Data object" ) );

	return ( PyObject * ) pymat;
}

/*****************************************************************************/
/* Function:	M_Material_Get	 */
/* Python equivalent:	Blender.Material.Get */
/* Description:		Receives a string and returns the material whose */
/*			name matches the string.	If no argument is */
/*			passed in, a list with all materials in the	 */
/*			current scene is returned.			 */
/*****************************************************************************/
static PyObject *M_Material_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Material *mat_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	mat_iter = G.main->mat.first;

	if( name ) {		/* (name) - Search material by name */

		BPy_Material *wanted_mat = NULL;

		while( mat_iter ) {
			if( strcmp( name, mat_iter->id.name + 2 ) == 0 ) {
				wanted_mat =
					( BPy_Material * )
					Material_CreatePyObject( mat_iter );
				break;
			}
			mat_iter = mat_iter->id.next;
		}

		if( wanted_mat == NULL ) { /* Requested material doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Material \"%s\" not found", name );
			return EXPP_ReturnPyObjError( PyExc_NameError,
						      error_msg );
		}

		return ( PyObject * ) wanted_mat;
	}

	else {			/* () - return a list with all materials in the scene */
		int index = 0;
		PyObject *matlist, *pyobj;

		matlist = PyList_New( BLI_countlist( &( G.main->mat ) ) );

		if( !matlist )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( mat_iter ) {
			pyobj = Material_CreatePyObject( mat_iter );

			if( !pyobj )
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create PyObject" ) );

			PyList_SET_ITEM( matlist, index, pyobj );

			mat_iter = mat_iter->id.next;
			index++;
		}

		return matlist;
	}
}

static PyObject *Material_ModesDict( void )
{
	PyObject *Modes = M_constant_New(  );

#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(c, #name, PyInt_FromLong(EXPP_MAT_MODE_##name))

/* So that:
 * EXPP_ADDCONST(TRACEABLE) becomes:
 * constant_insert(c, "TRACEABLE", PyInt_FromLong(EXPP_MAT_MODE_TRACEABLE))
 */

	if( Modes ) {
		BPy_constant *c = ( BPy_constant * ) Modes;

		EXPP_ADDCONST( TRACEABLE );
		EXPP_ADDCONST( SHADOW );
		EXPP_ADDCONST( SHADELESS );
		EXPP_ADDCONST( WIRE );
		EXPP_ADDCONST( VCOL_LIGHT );
		EXPP_ADDCONST( HALO );
		EXPP_ADDCONST( ZTRANSP );
		EXPP_ADDCONST( VCOL_PAINT );
		EXPP_ADDCONST( ZINVERT );
		EXPP_ADDCONST( HALORINGS );
		EXPP_ADDCONST( ENV );
		EXPP_ADDCONST( HALOLINES );
		EXPP_ADDCONST( ONLYSHADOW );
		EXPP_ADDCONST( HALOXALPHA );
		EXPP_ADDCONST( HALOSTAR );
		EXPP_ADDCONST( TEXFACE );
		EXPP_ADDCONST( HALOTEX );
		EXPP_ADDCONST( HALOPUNO );
		EXPP_ADDCONST( NOMIST );
		EXPP_ADDCONST( HALOSHADE );
		EXPP_ADDCONST( HALOFLARE );
		EXPP_ADDCONST( RADIO );
		EXPP_ADDCONST( RAYMIRROR );
		EXPP_ADDCONST( ZTRA );
		EXPP_ADDCONST( RAYTRANSP );

	}

	return Modes;
}


static PyObject *Material_ShadersDict( void )
{
	PyObject *Shaders = M_constant_New(  );

#undef EXPP_ADDCONST
#define EXPP_ADDCONST(name) \
	constant_insert(c, #name, PyInt_FromLong(EXPP_MAT_SHADER_##name))

/* So that:
 * EXPP_ADDCONST(DIFFUSE_LAMBERT) becomes:
 * constant_insert(c, "TRACEABLE", PyInt_FromLong(EXPP_MAT_SHADER_DIFFUSE_LAMBERT))
 */

	if( Shaders ) {
		BPy_constant *c = ( BPy_constant * ) Shaders;

		EXPP_ADDCONST( DIFFUSE_LAMBERT );
		EXPP_ADDCONST( DIFFUSE_ORENNAYAR );
		EXPP_ADDCONST( DIFFUSE_TOON );
		EXPP_ADDCONST( DIFFUSE_MINNAERT );
		
		EXPP_ADDCONST( SPEC_COOKTORR );
		EXPP_ADDCONST( SPEC_PHONG );
		EXPP_ADDCONST( SPEC_BLINN );
		EXPP_ADDCONST( SPEC_TOON );
		EXPP_ADDCONST( SPEC_WARDISO );
	}

	return Shaders;
}


/*****************************************************************************/
/* Function:	Material_Init */
/*****************************************************************************/
PyObject *Material_Init( void )
{
	PyObject *submodule, *Modes, *Shaders;

	Material_Type.ob_type = &PyType_Type;

	Modes = Material_ModesDict(  );
	Shaders = Material_ShadersDict(  );

	submodule = Py_InitModule3( "Blender.Material",
				    M_Material_methods, M_Material_doc );

	if( Modes )
		PyModule_AddObject( submodule, "Modes", Modes );
	if( Shaders )
		PyModule_AddObject( submodule, "Shaders", Shaders );
	
	PyModule_AddIntConstant( submodule, "RGB", IPOKEY_RGB );
	PyModule_AddIntConstant( submodule, "ALPHA", IPOKEY_ALPHA );
	PyModule_AddIntConstant( submodule, "HALOSIZE", IPOKEY_HALOSIZE );
	PyModule_AddIntConstant( submodule, "MODE", IPOKEY_MODE );
	PyModule_AddIntConstant( submodule, "ALLCOLOR", IPOKEY_ALLCOLOR );
	PyModule_AddIntConstant( submodule, "ALLMIRROR", IPOKEY_ALLMIRROR );
	PyModule_AddIntConstant( submodule, "OFS", IPOKEY_OFS );
	PyModule_AddIntConstant( submodule, "SIZE", IPOKEY_SIZE );
	PyModule_AddIntConstant( submodule, "ALLMAPPING", IPOKEY_ALLMAPPING );

	return ( submodule );
}

/***************************/
/*** The Material PyType ***/
/***************************/

/*****************************************************************************/
/* Python BPy_Material methods declarations: */
/*****************************************************************************/
static PyObject *Material_getIpo( BPy_Material * self );
static PyObject *Material_getName( BPy_Material * self );
static PyObject *Material_getMode( BPy_Material * self );
static PyObject *Material_getRGBCol( BPy_Material * self );
/*static PyObject *Material_getAmbCol(BPy_Material *self);*/
static PyObject *Material_getSpecCol( BPy_Material * self );
static PyObject *Material_getMirCol( BPy_Material * self );
static PyObject *Material_getAmb( BPy_Material * self );
static PyObject *Material_getEmit( BPy_Material * self );
static PyObject *Material_getAlpha( BPy_Material * self );
static PyObject *Material_getRef( BPy_Material * self );
static PyObject *Material_getSpec( BPy_Material * self );
static PyObject *Material_getSpecTransp( BPy_Material * self );
static PyObject *Material_getAdd( BPy_Material * self );
static PyObject *Material_getZOffset( BPy_Material * self );
static PyObject *Material_getHaloSize( BPy_Material * self );
static PyObject *Material_getHaloSeed( BPy_Material * self );
static PyObject *Material_getFlareSize( BPy_Material * self );
static PyObject *Material_getFlareSeed( BPy_Material * self );
static PyObject *Material_getFlareBoost( BPy_Material * self );
static PyObject *Material_getSubSize( BPy_Material * self );
static PyObject *Material_getHardness( BPy_Material * self );
static PyObject *Material_getNFlares( BPy_Material * self );
static PyObject *Material_getNStars( BPy_Material * self );
static PyObject *Material_getNLines( BPy_Material * self );
static PyObject *Material_getNRings( BPy_Material * self );
/* Shader settings */
static PyObject *Material_getSpecShader( BPy_Material * self );
static PyObject *Material_getDiffuseShader( BPy_Material * self );
static PyObject *Material_getRoughness( BPy_Material * self );
static PyObject *Material_getSpecSize( BPy_Material * self );
static PyObject *Material_getDiffuseSize( BPy_Material * self );
static PyObject *Material_getSpecSmooth( BPy_Material * self );
static PyObject *Material_getDiffuseSmooth( BPy_Material * self );
static PyObject *Material_getDiffuseDarkness( BPy_Material * self );
static PyObject *Material_getRefracIndex( BPy_Material * self );
static PyObject *Material_getRms( BPy_Material * self );

static PyObject *Material_getRayMirr( BPy_Material * self );
static PyObject *Material_getMirrDepth( BPy_Material * self );
static PyObject *Material_getFresnelMirr( BPy_Material * self );
static PyObject *Material_getFresnelMirrFac( BPy_Material * self );
static PyObject *Material_getIOR( BPy_Material * self );
static PyObject *Material_getTransDepth( BPy_Material * self );
static PyObject *Material_getFresnelTrans( BPy_Material * self );
static PyObject *Material_getFresnelTransFac( BPy_Material * self );
static PyObject *Material_getFilter( BPy_Material * self );
static PyObject *Material_getTranslucency( BPy_Material * self );
static PyObject *Material_getTextures( BPy_Material * self );
static PyObject *Material_setIpo( BPy_Material * self, PyObject * args );
static PyObject *Material_clearIpo( BPy_Material * self );
static PyObject *Material_setName( BPy_Material * self, PyObject * args );
static PyObject *Material_setMode( BPy_Material * self, PyObject * args );
static PyObject *Material_setIntMode( BPy_Material * self, PyObject * args );
static PyObject *Material_setRGBCol( BPy_Material * self, PyObject * args );
/*static PyObject *Material_setAmbCol(BPy_Material *self, PyObject *args);*/
static PyObject *Material_setSpecCol( BPy_Material * self, PyObject * args );
static PyObject *Material_setMirCol( BPy_Material * self, PyObject * args );
static PyObject *Material_setAmb( BPy_Material * self, PyObject * args );
static PyObject *Material_setEmit( BPy_Material * self, PyObject * args );
static PyObject *Material_setAlpha( BPy_Material * self, PyObject * args );
static PyObject *Material_setRef( BPy_Material * self, PyObject * args );
static PyObject *Material_setSpec( BPy_Material * self, PyObject * args );
static PyObject *Material_setSpecTransp( BPy_Material * self,
					 PyObject * args );
static PyObject *Material_setAdd( BPy_Material * self, PyObject * args );
static PyObject *Material_setZOffset( BPy_Material * self, PyObject * args );
static PyObject *Material_setHaloSize( BPy_Material * self, PyObject * args );
static PyObject *Material_setHaloSeed( BPy_Material * self, PyObject * args );
static PyObject *Material_setFlareSize( BPy_Material * self, PyObject * args );
static PyObject *Material_setFlareSeed( BPy_Material * self, PyObject * args );
static PyObject *Material_setFlareBoost( BPy_Material * self,
					 PyObject * args );
static PyObject *Material_setSubSize( BPy_Material * self, PyObject * args );
static PyObject *Material_setHardness( BPy_Material * self, PyObject * args );
static PyObject *Material_setNFlares( BPy_Material * self, PyObject * args );
static PyObject *Material_setNStars( BPy_Material * self, PyObject * args );
static PyObject *Material_setNLines( BPy_Material * self, PyObject * args );
static PyObject *Material_setNRings( BPy_Material * self, PyObject * args );

/* Shader */
static PyObject *Material_setSpecShader( BPy_Material * self, PyObject * args );
static PyObject *Material_setDiffuseShader( BPy_Material * self, PyObject * args );
static PyObject *Material_setRoughness( BPy_Material * self, PyObject * args );
static PyObject *Material_setSpecSize( BPy_Material * self, PyObject * args );
static PyObject *Material_setDiffuseSize( BPy_Material * self, PyObject * args );
static PyObject *Material_setSpecSmooth( BPy_Material * self, PyObject * args );
static PyObject *Material_setDiffuseSmooth( BPy_Material * self, PyObject * args );
static PyObject *Material_setDiffuseDarkness( BPy_Material * self, PyObject * args );
static PyObject *Material_setRefracIndex( BPy_Material * self, PyObject * args );
static PyObject *Material_setRms( BPy_Material * self, PyObject * args );

/* ** Mirror and transp ** */
static PyObject *Material_setRayMirr( BPy_Material * self, PyObject * args );
static PyObject *Material_setMirrDepth( BPy_Material * self, PyObject * args );
static PyObject *Material_setFresnelMirr( BPy_Material * self,
					  PyObject * args );
static PyObject *Material_setFresnelMirrFac( BPy_Material * self,
					     PyObject * args );
static PyObject *Material_setFilter( BPy_Material * self,
					     PyObject * args );
static PyObject *Material_setTranslucency( BPy_Material * self,
					     PyObject * args );
static PyObject *Material_setIOR( BPy_Material * self, PyObject * args );
static PyObject *Material_setTransDepth( BPy_Material * self,
					 PyObject * args );
static PyObject *Material_setFresnelTrans( BPy_Material * self,
					   PyObject * args );
static PyObject *Material_setFresnelTransFac( BPy_Material * self,
					      PyObject * args );
/* ** */
static PyObject *Material_setTexture( BPy_Material * self, PyObject * args );
static PyObject *Material_clearTexture( BPy_Material * self, PyObject * args );

static PyObject *Material_setColorComponent( BPy_Material * self, char *key,
					     PyObject * args );

static PyObject *Material_getScriptLinks(BPy_Material *self, PyObject * args );
static PyObject *Material_addScriptLink(BPy_Material * self, PyObject * args );
static PyObject *Material_clearScriptLinks(BPy_Material *self, PyObject *args);

static PyObject *Material_insertIpoKey( BPy_Material * self, PyObject * args );


/*****************************************************************************/
/* Python BPy_Material methods table: */
/*****************************************************************************/
static PyMethodDef BPy_Material_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) Material_getName, METH_NOARGS,
	 "() - Return Material's name"},
	{"getIpo", ( PyCFunction ) Material_getIpo, METH_NOARGS,
	 "() - Return Material's ipo or None if not found"},
	{"getMode", ( PyCFunction ) Material_getMode, METH_NOARGS,
	 "() - Return Material's mode flags"},
	{"getRGBCol", ( PyCFunction ) Material_getRGBCol, METH_NOARGS,
	 "() - Return Material's rgb color triplet"},
/*	{"getAmbCol", (PyCFunction)Material_getAmbCol, METH_NOARGS,
			"() - Return Material's ambient color"},*/
	{"getSpecCol", ( PyCFunction ) Material_getSpecCol, METH_NOARGS,
	 "() - Return Material's specular color"},
	{"getMirCol", ( PyCFunction ) Material_getMirCol, METH_NOARGS,
	 "() - Return Material's mirror color"},
	{"getAmb", ( PyCFunction ) Material_getAmb, METH_NOARGS,
	 "() - Return Material's ambient color blend factor"},
	{"getEmit", ( PyCFunction ) Material_getEmit, METH_NOARGS,
	 "() - Return Material's emitting light intensity"},
	{"getAlpha", ( PyCFunction ) Material_getAlpha, METH_NOARGS,
	 "() - Return Material's alpha (transparency) value"},
	{"getRef", ( PyCFunction ) Material_getRef, METH_NOARGS,
	 "() - Return Material's reflectivity"},
	{"getSpec", ( PyCFunction ) Material_getSpec, METH_NOARGS,
	 "() - Return Material's specularity"},
	/* Shader specific settings */
	{"getSpecShader", ( PyCFunction ) Material_getSpecShader, METH_NOARGS,
	 "() - Returns Material's specular shader" },
	{"getDiffuseShader", ( PyCFunction ) Material_getDiffuseShader, METH_NOARGS,
	 "() - Returns Material's diffuse shader" },
	 {"getRoughness", ( PyCFunction ) Material_getRoughness, METH_NOARGS,
	 "() - Returns Material's Roughness (applies to the \"Oren Nayar\" Diffuse Shader only)" },
	{"getSpecSize", ( PyCFunction ) Material_getSpecSize, METH_NOARGS,
	 "() - Returns Material's size of specular area (applies to the \"Toon\" Specular Shader only)" },
	{"getDiffuseSize", ( PyCFunction ) Material_getDiffuseSize, METH_NOARGS,
	 "() - Returns Material's size of diffuse area (applies to the \"Toon\" Diffuse Shader only)" },
	{"getSpecSmooth", ( PyCFunction ) Material_getSpecSmooth, METH_NOARGS,
	 "() - Returns Material's smoothing of specular area (applies to the \"Toon\" Diffuse Shader only)" },
	{"getDiffuseSmooth", ( PyCFunction ) Material_getDiffuseSmooth, METH_NOARGS,
	 "() - Returns Material's smoothing of diffuse area (applies to the \"Toon\" Diffuse Shader only)" },
	{"getDiffuseDarkness", ( PyCFunction ) Material_getDiffuseDarkness, METH_NOARGS,
	 "() - Returns Material's diffuse darkness (applies to the \"Minnaert\" Diffuse Shader only)" },
	{"getRefracIndex", ( PyCFunction ) Material_getRefracIndex, METH_NOARGS,
	 "() - Returns Material's Index of Refraction (applies to the \"Blinn\" Specular Shader only)" },	 
	{"getRms", ( PyCFunction ) Material_getRms, METH_NOARGS,
	 "() - Returns Material's standard deviation of surface slope (applies to the \"WardIso\" Specular Shader only)" },
	/* End shader settings */
	{"getSpecTransp", ( PyCFunction ) Material_getSpecTransp, METH_NOARGS,
	 "() - Return Material's specular transparency"},
	{"getAdd", ( PyCFunction ) Material_getAdd, METH_NOARGS,
	 "() - Return Material's glow factor"},
	{"getZOffset", ( PyCFunction ) Material_getZOffset, METH_NOARGS,
	 "() - Return Material's artificial offset for faces"},
	{"getHaloSize", ( PyCFunction ) Material_getHaloSize, METH_NOARGS,
	 "() - Return Material's halo size"},
	{"getHaloSeed", ( PyCFunction ) Material_getHaloSeed, METH_NOARGS,
	 "() - Return Material's seed for random ring dimension and line "
	 "location in halos"},
	{"getFlareSize", ( PyCFunction ) Material_getFlareSize, METH_NOARGS,
	 "() - Return Material's (flare size)/(halo size) factor"},
	{"getFlareSeed", ( PyCFunction ) Material_getFlareSeed, METH_NOARGS,
	 "() - Return Material's flare offset in the seed table"},
	{"getFlareBoost", ( PyCFunction ) Material_getFlareBoost, METH_NOARGS,
	 "() - Return Material's flare boost"},
	{"getSubSize", ( PyCFunction ) Material_getSubSize, METH_NOARGS,
	 "() - Return Material's dimension of subflare, dots and circles"},
	{"getHardness", ( PyCFunction ) Material_getHardness, METH_NOARGS,
	 "() - Return Material's specular hardness"},
	{"getNFlares", ( PyCFunction ) Material_getNFlares, METH_NOARGS,
	 "() - Return Material's number of flares in halo"},
	{"getNStars", ( PyCFunction ) Material_getNStars, METH_NOARGS,
	 "() - Return Material's number of points in the halo stars"},
	{"getNLines", ( PyCFunction ) Material_getNLines, METH_NOARGS,
	 "() - Return Material's number of lines in halo"},
	{"getNRings", ( PyCFunction ) Material_getNRings, METH_NOARGS,
	 "() - Return Material's number of rings in halo"},
	{"getRayMirr", ( PyCFunction ) Material_getRayMirr, METH_NOARGS,
	 "() - Return mount mirror"},
	{"getMirrDepth", ( PyCFunction ) Material_getMirrDepth, METH_NOARGS,
	 "() - Return amount mirror depth"},
	{"getFresnelMirr", ( PyCFunction ) Material_getFresnelMirr,
	 METH_NOARGS,
	 "() - Return fresnel power for refractions"},
	{"getFresnelMirrFac", ( PyCFunction ) Material_getFresnelMirrFac,
	 METH_NOARGS,
	 "() - Return fresnel power for refractions factor"},
	{"getFilter", ( PyCFunction ) Material_getFilter,
	 METH_NOARGS,
	 "() - Return the amount of filtering when transparent raytrace is enabled"},
	{"getTranslucency", ( PyCFunction ) Material_getTranslucency,
	 METH_NOARGS,
	 "() - Return the Translucency, the amount of diffuse shading of the back side"},
	{"getIOR", ( PyCFunction ) Material_getIOR, METH_NOARGS,
	 "() - Return IOR"},
	{"getTransDepth", ( PyCFunction ) Material_getTransDepth, METH_NOARGS,
	 "() - Return amount inter-refractions"},
	{"getFresnelTrans", ( PyCFunction ) Material_getFresnelTrans,
	 METH_NOARGS,
	 "() - Return fresnel power for refractions"},
	{"getFresnelTransFac", ( PyCFunction ) Material_getFresnelTransFac,
	 METH_NOARGS,
	 "() - Return fresnel power for refractions factor"},
	{"getTextures", ( PyCFunction ) Material_getTextures, METH_NOARGS,
	 "() - Return Material's texture list as a tuple"},
	{"setName", ( PyCFunction ) Material_setName, METH_VARARGS,
	 "(s) - Change Material's name"},
	{"setIpo", ( PyCFunction ) Material_setIpo, METH_VARARGS,
	 "(Blender Ipo) - Change Material's Ipo"},
	{"clearIpo", ( PyCFunction ) Material_clearIpo, METH_NOARGS,
	 "(Blender Ipo) - Unlink Ipo from this Material"},
	{"insertIpoKey", ( PyCFunction ) Material_insertIpoKey, METH_VARARGS,
	 "(Material Ipo Constant) - Insert IPO Key at current frame"},	 
	{"setMode", ( PyCFunction ) Material_setMode, METH_VARARGS,
	 "([s[,s]]) - Set Material's mode flag(s)"},
	{"setRGBCol", ( PyCFunction ) Material_setRGBCol, METH_VARARGS,
	 "(f,f,f or [f,f,f]) - Set Material's rgb color triplet"},
/*	{"setAmbCol", (PyCFunction)Material_setAmbCol, METH_VARARGS,
			"(f,f,f or [f,f,f]) - Set Material's ambient color"},*/
	{"setSpecCol", ( PyCFunction ) Material_setSpecCol, METH_VARARGS,
	 "(f,f,f or [f,f,f]) - Set Material's specular color"},
	 
	/* Shader spesific settings */
	{"setSpecShader", ( PyCFunction ) Material_setSpecShader, METH_NOARGS,
	 "(i) - Set the Material's specular shader" },
	{"setDiffuseShader", ( PyCFunction ) Material_setDiffuseShader, METH_NOARGS,
	 "(i) - Set the Material's diffuse shader" },
	 {"setRoughness", ( PyCFunction ) Material_setRoughness, METH_NOARGS,
	 "(f) - Set the Material's Roughness (applies to the \"Oren Nayar\" Diffuse Shader only)" },
	{"setSpecSize", ( PyCFunction ) Material_setSpecSize, METH_NOARGS,
	 "(f) - Set the Material's size of specular area (applies to the \"Toon\" Specular Shader only)" },
	{"setDiffuseSize", ( PyCFunction ) Material_setDiffuseSize, METH_NOARGS,
	 "(f) - Set the Material's size of diffuse area (applies to the \"Toon\" Diffuse Shader only)" },
	{"setSpecSmooth", ( PyCFunction ) Material_setSpecSmooth, METH_NOARGS,
	 "(f) - Set the Material's smoothing of specular area (applies to the \"Toon\" Specular Shader only)" },
	{"setDiffuseSmooth", ( PyCFunction ) Material_setDiffuseSmooth, METH_NOARGS,
	 "(f) - Set the Material's smoothing of diffuse area (applies to the \"Toon\" Diffuse Shader only)" },
	{"setDiffuseDarkness", ( PyCFunction ) Material_setDiffuseDarkness, METH_NOARGS,
	 "(f) - Set the Material's diffuse darkness (applies to the \"Minnaert\" Diffuse Shader only)" },
	{"setRefracIndex", ( PyCFunction ) Material_setRefracIndex, METH_NOARGS,
	 "(f) - Set the Material's Index of Refraction (applies to the \"Blinn\" Specular Shader only)" },	 
	{"setRms", ( PyCFunction ) Material_setRms, METH_NOARGS,
	 "(f) - Set the Material's standard deviation of surface slope (applies to the \"WardIso\" Specular Shader only)" },
	/* End shader settings */
	 
	{"setMirCol", ( PyCFunction ) Material_setMirCol, METH_VARARGS,
	 "(f,f,f or [f,f,f]) - Set Material's mirror color"},
	{"setAmb", ( PyCFunction ) Material_setAmb, METH_VARARGS,
	 "(f) - Set how much the Material's color is affected"
	 " by \nthe global ambient colors - [0.0, 1.0]"},
	{"setEmit", ( PyCFunction ) Material_setEmit, METH_VARARGS,
	 "(f) - Set Material's emitting light intensity - [0.0, 1.0]"},
	{"setAlpha", ( PyCFunction ) Material_setAlpha, METH_VARARGS,
	 "(f) - Set Material's alpha (transparency) - [0.0, 1.0]"},
	{"setRef", ( PyCFunction ) Material_setRef, METH_VARARGS,
	 "(f) - Set Material's reflectivity - [0.0, 1.0]"},
	{"setSpec", ( PyCFunction ) Material_setSpec, METH_VARARGS,
	 "(f) - Set Material's specularity - [0.0, 2.0]"},
	{"setSpecTransp", ( PyCFunction ) Material_setSpecTransp, METH_VARARGS,
	 "(f) - Set Material's specular transparency - [0.0, 1.0]"},
	{"setAdd", ( PyCFunction ) Material_setAdd, METH_VARARGS,
	 "(f) - Set Material's glow factor - [0.0, 1.0]"},
	{"setZOffset", ( PyCFunction ) Material_setZOffset, METH_VARARGS,
	 "(f) - Set Material's artificial offset - [0.0, 10.0]"},
	{"setHaloSize", ( PyCFunction ) Material_setHaloSize, METH_VARARGS,
	 "(f) - Set Material's halo size - [0.0, 100.0]"},
	{"setHaloSeed", ( PyCFunction ) Material_setHaloSeed, METH_VARARGS,
	 "(i) - Set Material's halo seed - [0, 255]"},
	{"setFlareSize", ( PyCFunction ) Material_setFlareSize, METH_VARARGS,
	 "(f) - Set Material's factor: (flare size)/(halo size) - [0.1, 25.0]"},
	{"setFlareSeed", ( PyCFunction ) Material_setFlareSeed, METH_VARARGS,
	 "(i) - Set Material's flare seed - [0, 255]"},
	{"setFlareBoost", ( PyCFunction ) Material_setFlareBoost, METH_VARARGS,
	 "(f) - Set Material's flare boost - [0.1, 10.0]"},
	{"setSubSize", ( PyCFunction ) Material_setSubSize, METH_VARARGS,
	 "(f) - Set Material's dimension of subflare,"
	 " dots and circles - [0.1, 25.0]"},
	{"setHardness", ( PyCFunction ) Material_setHardness, METH_VARARGS,
	 "(i) - Set Material's hardness - [1, 255 (127 if halo mode is ON)]"},
	{"setNFlares", ( PyCFunction ) Material_setNFlares, METH_VARARGS,
	 "(i) - Set Material's number of flares in halo - [1, 32]"},
	{"setNStars", ( PyCFunction ) Material_setNStars, METH_VARARGS,
	 "(i) - Set Material's number of stars in halo - [3, 50]"},
	{"setNLines", ( PyCFunction ) Material_setNLines, METH_VARARGS,
	 "(i) - Set Material's number of lines in halo - [0, 250]"},
	{"setNRings", ( PyCFunction ) Material_setNRings, METH_VARARGS,
	 "(i) - Set Material's number of rings in halo - [0, 24]"},
	{"setRayMirr", ( PyCFunction ) Material_setRayMirr, METH_VARARGS,
	 "(f) - Set amount mirror - [0.0, 1.0]"},
	{"setMirrDepth", ( PyCFunction ) Material_setMirrDepth, METH_VARARGS,
	 "(i) - Set amount inter-reflections - [0, 10]"},
	{"setFresnelMirr", ( PyCFunction ) Material_setFresnelMirr,
	 METH_VARARGS,
	 "(f) - Set fresnel power for mirror - [0.0, 5.0]"},
	{"setFresnelMirrFac", ( PyCFunction ) Material_setFresnelMirrFac,
	 METH_VARARGS,
	 "(f) - Set blend fac for mirror fresnel - [1.0, 5.0]"},
	{"setFilter", ( PyCFunction ) Material_setFresnelMirrFac,
	 METH_VARARGS,
	 "(f) - Set the amount of filtering when transparent raytrace is enabled"},
	{"setTranslucency", ( PyCFunction ) Material_setTranslucency,
	 METH_VARARGS,
	 "(f) - Set the Translucency, the amount of diffuse shading of the back side"},
	{"setIOR", ( PyCFunction ) Material_setIOR, METH_VARARGS,
	 "(f) - Set IOR - [1.0, 3.0]"},
	{"setTransDepth", ( PyCFunction ) Material_setTransDepth, METH_VARARGS,
	 "(i) - Set amount inter-refractions - [0, 10]"},
	{"setFresnelTrans", ( PyCFunction ) Material_setFresnelTrans,
	 METH_VARARGS,
	 "(f) - Set fresnel power for refractions - [0.0, 5.0]"},
	{"setFresnelTransFac", ( PyCFunction ) Material_setFresnelTransFac,
	 METH_VARARGS,
	 "(f) - Set fresnel power for refractions factot- [0.0, 5.0]"},
	{"setTexture", ( PyCFunction ) Material_setTexture, METH_VARARGS,
	 "(n,tex,texco=0,mapto=0) - Set numbered texture to tex"},
	{"clearTexture", ( PyCFunction ) Material_clearTexture, METH_VARARGS,
	 "(n) - Remove texture from numbered slot"},
	{"getScriptLinks", ( PyCFunction ) Material_getScriptLinks,
	 METH_VARARGS,
	 "(eventname) - Get a list of this material's scriptlinks (Text names) "
	 "of the given type\n"
	 "(eventname) - string: FrameChanged, Redraw or Render."},
	{"addScriptLink", ( PyCFunction ) Material_addScriptLink, METH_VARARGS,
	 "(text, evt) - Add a new material scriptlink.\n"
	 "(text) - string: an existing Blender Text name;\n"
	 "(evt) string: FrameChanged, Redraw or Render."},
	{"clearScriptLinks", ( PyCFunction ) Material_clearScriptLinks,
	 METH_VARARGS,
	 "() - Delete all scriptlinks from this material.\n"
	 "([s1<,s2,s3...>]) - Delete specified scriptlinks from this material."},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Material_Type callback function prototypes: */
/*****************************************************************************/
static void Material_dealloc( BPy_Material * self );
static int Material_setAttr( BPy_Material * self, char *name, PyObject * v );
static PyObject *Material_getAttr( BPy_Material * self, char *name );
static PyObject *Material_repr( BPy_Material * self );

/*****************************************************************************/
/* Python Material_Type structure definition: */
/*****************************************************************************/
PyTypeObject Material_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Blender Material",	/* tp_name */
	sizeof( BPy_Material ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) Material_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) Material_getAttr,	/* tp_getattr */
	( setattrfunc ) Material_setAttr,	/* tp_setattr */
	0,			/* tp_compare */
	( reprfunc ) Material_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_Material_methods,	/* tp_methods */
	0,			/* tp_members */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* up to tp_del to avoid a warning */
};

/*****************************************************************************/
/* Function:	Material_dealloc          */
/* Description: This is a callback function for the BPy_Material type. It is */
/*		the destructor function.				 */
/*****************************************************************************/
static void Material_dealloc( BPy_Material * self )
{
	Py_DECREF( self->col );
	Py_DECREF( self->amb );
	Py_DECREF( self->spec );
	Py_DECREF( self->mir );
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:	Material_CreatePyObject		*/
/* Description: Create a new BPy_Material from an  existing */
/*		 Blender material structure.	 */
/*****************************************************************************/
PyObject *Material_CreatePyObject( struct Material *mat )
{
	BPy_Material *pymat;
	float *col[3], *amb[3], *spec[3], *mir[3];

	pymat = ( BPy_Material * ) PyObject_NEW( BPy_Material,
						 &Material_Type );

	if( !pymat )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Material object" );

	pymat->material = mat;

	col[0] = &mat->r;
	col[1] = &mat->g;
	col[2] = &mat->b;

	amb[0] = &mat->ambr;
	amb[1] = &mat->ambg;
	amb[2] = &mat->ambb;

	spec[0] = &mat->specr;
	spec[1] = &mat->specg;
	spec[2] = &mat->specb;

	mir[0] = &mat->mirr;
	mir[1] = &mat->mirg;
	mir[2] = &mat->mirb;

	pymat->col = ( BPy_rgbTuple * ) rgbTuple_New( col );
	pymat->amb = ( BPy_rgbTuple * ) rgbTuple_New( amb );
	pymat->spec = ( BPy_rgbTuple * ) rgbTuple_New( spec );
	pymat->mir = ( BPy_rgbTuple * ) rgbTuple_New( mir );

	return ( PyObject * ) pymat;
}

/*****************************************************************************/
/* Function:	Material_CheckPyObject  */
/* Description: This function returns true when the given PyObject is of the */
/*		type Material. Otherwise it will return false.	 */
/*****************************************************************************/
int Material_CheckPyObject( PyObject * pyobj )
{
	return ( pyobj->ob_type == &Material_Type );
}

/*****************************************************************************/
/* Function:		Material_FromPyObject	 */
/* Description: This function returns the Blender material from the given */
/*		PyObject.	 */
/*****************************************************************************/
Material *Material_FromPyObject( PyObject * pyobj )
{
	return ( ( BPy_Material * ) pyobj )->material;
}

/*****************************************************************************/
/* Description: Returns the object with the name specified by the argument  */
/*		name. Note that the calling function has to remove the first */
/*		two characters of the object name. These two characters	 */
/*		specify the type of the object (OB, ME, WO, ...)	 */
/*		The function will return NULL when no object with the given  */
/*		name is found.						 */
/*****************************************************************************/
Material *GetMaterialByName( char *name )
{
	Material *mat_iter;

	mat_iter = G.main->mat.first;
	while( mat_iter ) {
		if( StringEqual( name, GetIdName( &( mat_iter->id ) ) ) ) {
			return ( mat_iter );
		}
		mat_iter = mat_iter->id.next;
	}

	/* There is no material with the given name */
	return ( NULL );
}

/*****************************************************************************/
/* Python BPy_Material methods:		 */
/*****************************************************************************/

static PyObject *Material_getIpo( BPy_Material * self )
{
	Ipo *ipo = self->material->ipo;

	if( !ipo ) {
		Py_INCREF( Py_None );
		return Py_None;
	}

	return Ipo_CreatePyObject( ipo );
}

static PyObject *Material_getName( BPy_Material * self )
{
	PyObject *attr = PyString_FromString( self->material->id.name + 2 );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Material.name attribute" ) );
}

static PyObject *Material_getMode( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->mode );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.Mode attribute" );
}

static PyObject *Material_getRGBCol( BPy_Material * self )
{
	return rgbTuple_getCol( self->col );
}

/*
static PyObject *Material_getAmbCol(BPy_Material *self)
{
	return rgbTuple_getCol(self->amb);
}
*/
static PyObject *Material_getSpecCol( BPy_Material * self )
{
	return rgbTuple_getCol( self->spec );
}

static PyObject *Material_getMirCol( BPy_Material * self )
{
	return rgbTuple_getCol( self->mir );
}

static PyObject *Material_getSpecShader( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->spec_shader );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.specShader attribute" );
}

static PyObject *Material_getDiffuseShader( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->diff_shader );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.diffuseShader attribute" );
}

static PyObject *Material_getRoughness( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->roughness );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					  "couldn't get Material.roughness attribute" );
}

static PyObject *Material_getSpecSize( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->param[2] );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					  "couldn't get Material.specSize attribute" );
}

static PyObject *Material_getDiffuseSize( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->param[0] );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					  "couldn't get Material.diffuseSize attribute" );
}

static PyObject *Material_getSpecSmooth( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->param[3] );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					  "couldn't get Material.specSmooth attribute" );
}

static PyObject *Material_getDiffuseSmooth( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->param[1] );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					  "couldn't get Material.diffuseSmooth( attribute" );
}

static PyObject *Material_getDiffuseDarkness( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->darkness );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					  "couldn't get Material.diffuseDarkness attribute" );
}

static PyObject *Material_getRefracIndex( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->refrac );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					  "couldn't get Material.refracIndex attribute" );
}
	
static PyObject *Material_getRms( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->rms );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					  "couldn't get Material.rms attribute" );
}

static PyObject *Material_getAmb( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->amb );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.amb attribute" );
}

static PyObject *Material_getEmit( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->emit );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.emit attribute" );
}

static PyObject *Material_getAlpha( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->alpha );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.alpha attribute" );
}

static PyObject *Material_getRef( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->ref );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.ref attribute" );
}

static PyObject *Material_getSpec( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->spec );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.spec attribute" );
}

static PyObject *Material_getSpecTransp( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->spectra );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.specTransp attribute" );
}

static PyObject *Material_getAdd( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->add );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.add attribute" );
}

static PyObject *Material_getZOffset( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->zoffs );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.zOffset attribute" );
}

static PyObject *Material_getHaloSize( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->hasize );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.haloSize attribute" );
}

static PyObject *Material_getFlareSize( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->flaresize );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.flareSize attribute" );
}

static PyObject *Material_getFlareBoost( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->flareboost );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.flareBoost attribute" );
}

static PyObject *Material_getSubSize( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->subsize );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.subSize attribute" );
}

static PyObject *Material_getHaloSeed( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->seed1 );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.haloSeed attribute" );
}

static PyObject *Material_getFlareSeed( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->seed2 );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.flareSeed attribute" );
}

static PyObject *Material_getHardness( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->har );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.hard attribute" );
}

static PyObject *Material_getNFlares( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->flarec );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.nFlares attribute" );
}

static PyObject *Material_getNStars( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->starc );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.nStars attribute" );
}

static PyObject *Material_getNLines( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->linec );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.nLines attribute" );
}

static PyObject *Material_getNRings( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->ringc );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.nRings attribute" );
}

static PyObject *Material_getRayMirr( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->ray_mirror );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.rayMirr attribute" );
}

static PyObject *Material_getMirrDepth( BPy_Material * self )
{
	PyObject *attr = PyInt_FromLong( ( long ) self->material->ray_depth );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.rayMirrDepth attribute" );
}

static PyObject *Material_getFresnelMirr( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->fresnel_mir );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.fresnelDepth attribute" );
}

static PyObject *Material_getFresnelMirrFac( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->fresnel_mir_i );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.fresnelDepthFac attribute" );
}

static PyObject *Material_getFilter( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->filter );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.filter attribute" );
}

static PyObject *Material_getTranslucency( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->translucency );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.translucency attribute" );
}

static PyObject *Material_getIOR( BPy_Material * self )
{
	PyObject *attr = PyFloat_FromDouble( ( double ) self->material->ang );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.nRings attribute" );
}

static PyObject *Material_getTransDepth( BPy_Material * self )
{
	PyObject *attr =
		PyInt_FromLong( ( long ) self->material->ray_depth_tra );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.nRings attribute" );
}

static PyObject *Material_getFresnelTrans( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->fresnel_tra );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.nRings attribute" );
}

static PyObject *Material_getFresnelTransFac( BPy_Material * self )
{
	PyObject *attr =
		PyFloat_FromDouble( ( double ) self->material->fresnel_tra_i );

	if( attr )
		return attr;

	return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				      "couldn't get Material.nRings attribute" );
}

static PyObject *Material_getTextures( BPy_Material * self )
{
	int i;
	struct MTex *mtex;
	PyObject *t[MAX_MTEX];
	PyObject *tuple;

	/* build a texture list */
	for( i = 0; i < MAX_MTEX; ++i ) {
		mtex = self->material->mtex[i];

		if( mtex ) {
			t[i] = MTex_CreatePyObject( mtex );
		} else {
			Py_INCREF( Py_None );
			t[i] = Py_None;
		}
	}

	/* turn the array into a tuple */
	tuple = Py_BuildValue( "NNNNNNNN", t[0], t[1], t[2], t[3],
			       t[4], t[5], t[6], t[7] );
	if( !tuple )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "Material_getTextures: couldn't create PyTuple" );

	return tuple;
}

PyObject *Material_setIpo( BPy_Material * self, PyObject * args )
{
	PyObject *pyipo = 0;
	Ipo *ipo = NULL;
	Ipo *oldipo;

	if( !PyArg_ParseTuple( args, "O!", &Ipo_Type, &pyipo ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected Ipo as argument" );

	ipo = Ipo_FromPyObject( pyipo );

	if( !ipo )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "null ipo!" );

	if( ipo->blocktype != ID_MA )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "this ipo is not a Material type ipo" );

	oldipo = self->material->ipo;
	if( oldipo ) {
		ID *id = &oldipo->id;
		if( id->us > 0 )
			id->us--;
	}

	( ( ID * ) & ipo->id )->us++;

	self->material->ipo = ipo;

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Material_clearIpo( BPy_Material * self )
{
	Material *mat = self->material;
	Ipo *ipo = ( Ipo * ) mat->ipo;

	if( ipo ) {
		ID *id = &ipo->id;
		if( id->us > 0 )
			id->us--;
		mat->ipo = NULL;

		return EXPP_incr_ret_True();
	}

	return EXPP_incr_ret_False(); /* no ipo found */
}


/* 
 *  Material_insertIpoKey( key )
 *   inserts Material IPO key at current frame
 */

static PyObject *Material_insertIpoKey( BPy_Material * self, PyObject * args )
{
    int key = 0, map;
    
	if( !PyArg_ParseTuple( args, "i", &( key ) ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected int argument" ) ); 
    				
	map = texchannel_to_adrcode(self->material->texact);
	
	if(key==IPOKEY_RGB || key==IPOKEY_ALLCOLOR) {
		insertkey((ID *)self->material, MA_COL_R);
		insertkey((ID *)self->material, MA_COL_G);
		insertkey((ID *)self->material, MA_COL_B);
	}
	if(key==IPOKEY_ALPHA || key==IPOKEY_ALLCOLOR) {
		insertkey((ID *)self->material, MA_ALPHA);
	}
	if(key==IPOKEY_HALOSIZE || key==IPOKEY_ALLCOLOR) {
		insertkey((ID *)self->material, MA_HASIZE);
	}
	if(key==IPOKEY_MODE || key==IPOKEY_ALLCOLOR) {
		insertkey((ID *)self->material, MA_MODE);
	}
	if(key==IPOKEY_ALLCOLOR) {
		insertkey((ID *)self->material, MA_SPEC_R);
		insertkey((ID *)self->material, MA_SPEC_G);
		insertkey((ID *)self->material, MA_SPEC_B);
		insertkey((ID *)self->material, MA_REF);
		insertkey((ID *)self->material, MA_EMIT);
		insertkey((ID *)self->material, MA_AMB);
		insertkey((ID *)self->material, MA_SPEC);
		insertkey((ID *)self->material, MA_HARD);
		insertkey((ID *)self->material, MA_MODE);
		insertkey((ID *)self->material, MA_TRANSLU);
		insertkey((ID *)self->material, MA_ADD);
	}
	if(key==IPOKEY_ALLMIRROR) {
		insertkey((ID *)self->material, MA_RAYM);
		insertkey((ID *)self->material, MA_FRESMIR);
		insertkey((ID *)self->material, MA_FRESMIRI);
		insertkey((ID *)self->material, MA_FRESTRA);
		insertkey((ID *)self->material, MA_FRESTRAI);
	}
	if(key==IPOKEY_OFS || key==IPOKEY_ALLMAPPING) {
		insertkey((ID *)self->material, map+MAP_OFS_X);
		insertkey((ID *)self->material, map+MAP_OFS_Y);
		insertkey((ID *)self->material, map+MAP_OFS_Z);
	}
	if(key==IPOKEY_SIZE || key==IPOKEY_ALLMAPPING) {
		insertkey((ID *)self->material, map+MAP_SIZE_X);
		insertkey((ID *)self->material, map+MAP_SIZE_Y);
		insertkey((ID *)self->material, map+MAP_SIZE_Z);
	}
	if(key==IPOKEY_ALLMAPPING) {
		insertkey((ID *)self->material, map+MAP_R);
		insertkey((ID *)self->material, map+MAP_G);
		insertkey((ID *)self->material, map+MAP_B);
		insertkey((ID *)self->material, map+MAP_DVAR);
		insertkey((ID *)self->material, map+MAP_COLF);
		insertkey((ID *)self->material, map+MAP_NORF);
		insertkey((ID *)self->material, map+MAP_VARF);
		insertkey((ID *)self->material, map+MAP_DISP);
	}

	allspace(REMAKEIPO, 0);
	EXPP_allqueue(REDRAWIPO, 0);
	EXPP_allqueue(REDRAWVIEW3D, 0);
	EXPP_allqueue(REDRAWACTION, 0);
	EXPP_allqueue(REDRAWNLA, 0);

	return  EXPP_incr_ret( Py_None );		
}


static PyObject *Material_setName( BPy_Material * self, PyObject * args )
{
	char *name;
	char buf[21];

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument" ) );

	PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	rename_id( &self->material->id, buf );

	Py_INCREF( Py_None );
	return Py_None;
}

/* Possible modes are traceable, shadow, shadeless, wire, vcolLight,
 * vcolPaint, halo, ztransp, zinvert, haloRings, env, haloLines,
 * onlyShadow, xalpha, star, faceTexture, haloTex, haloPuno, noMist,
 * haloShaded, haloFlare */
static PyObject *Material_setMode( BPy_Material * self, PyObject * args )
{
	unsigned int i, flag = 0, ok = 0;

	char *m[28] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL
	};

	/* 
	 * check for a single integer argument; do a quick check for now
	 * that the value is not larger than double the highest flag bit
	 */

	if ( (PySequence_Size( args ) == 1)
		    && PyInt_Check ( PySequence_Fast_GET_ITEM ( args , 0 ) )
		    && PyArg_ParseTuple( args, "i", &flag ) 
		    && flag < (EXPP_MAT_MODE_RAYMIRROR >> 1) ) {
			ok = 1;

	/*
	 * check for either an empty argument list, or up to 22 strings
	 */

	} else if( PyArg_ParseTuple( args, "|ssssssssssssssssssssssssssss",
			       &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6],
			       &m[7], &m[8], &m[9], &m[10], &m[11], &m[12],
			       &m[13], &m[14], &m[15], &m[16], &m[17], &m[18],
			       &m[19], &m[20], &m[21], &m[22], &m[23], &m[24],
			       &m[25], &m[26], &m[27] ) ) {
		for( i = 0; i < 28; i++ ) {
			if( m[i] == NULL )
				break;
			if( strcmp( m[i], "Traceable" ) == 0 )
				flag |= EXPP_MAT_MODE_TRACEABLE;
			else if( strcmp( m[i], "Shadow" ) == 0 )
				flag |= EXPP_MAT_MODE_SHADOW;
			else if( strcmp( m[i], "Shadeless" ) == 0 )
				flag |= EXPP_MAT_MODE_SHADELESS;
			else if( strcmp( m[i], "Wire" ) == 0 )
				flag |= EXPP_MAT_MODE_WIRE;
			else if( strcmp( m[i], "VColLight" ) == 0 )
				flag |= EXPP_MAT_MODE_VCOL_LIGHT;
			else if( strcmp( m[i], "VColPaint" ) == 0 )
				flag |= EXPP_MAT_MODE_VCOL_PAINT;
			else if( strcmp( m[i], "Halo" ) == 0 )
				flag |= EXPP_MAT_MODE_HALO;
			else if( strcmp( m[i], "ZTransp" ) == 0 )
				flag |= EXPP_MAT_MODE_ZTRANSP;
			else if( strcmp( m[i], "ZInvert" ) == 0 )
				flag |= EXPP_MAT_MODE_ZINVERT;
			else if( strcmp( m[i], "HaloRings" ) == 0 )
				flag |= EXPP_MAT_MODE_HALORINGS;
			else if( strcmp( m[i], "HaloLines" ) == 0 )
				flag |= EXPP_MAT_MODE_HALOLINES;
			else if( strcmp( m[i], "OnlyShadow" ) == 0 )
				flag |= EXPP_MAT_MODE_ONLYSHADOW;
			else if( strcmp( m[i], "HaloXAlpha" ) == 0 )
				flag |= EXPP_MAT_MODE_HALOXALPHA;
			else if( strcmp( m[i], "HaloStar" ) == 0 )
				flag |= EXPP_MAT_MODE_HALOSTAR;
			else if( strcmp( m[i], "TexFace" ) == 0 )
				flag |= EXPP_MAT_MODE_TEXFACE;
			else if( strcmp( m[i], "HaloTex" ) == 0 )
				flag |= EXPP_MAT_MODE_HALOTEX;
			else if( strcmp( m[i], "HaloPuno" ) == 0 )
				flag |= EXPP_MAT_MODE_HALOPUNO;
			else if( strcmp( m[i], "NoMist" ) == 0 )
				flag |= EXPP_MAT_MODE_NOMIST;
			else if( strcmp( m[i], "HaloShaded" ) == 0 )
				flag |= EXPP_MAT_MODE_HALOSHADE;
			else if( strcmp( m[i], "HaloFlare" ) == 0 )
				flag |= EXPP_MAT_MODE_HALOFLARE;
			else if( strcmp( m[i], "Radio" ) == 0 )
				flag |= EXPP_MAT_MODE_RADIO;
			/* ** Mirror ** */
			else if( strcmp( m[i], "RayMirr" ) == 0 )
				flag |= EXPP_MAT_MODE_RAYMIRROR;
			else if( strcmp( m[i], "ZTransp" ) == 0 )
				flag |= EXPP_MAT_MODE_ZTRA;
			else if( strcmp( m[i], "RayTransp" ) == 0 )
				flag |= EXPP_MAT_MODE_RAYTRANSP;
			else if( strcmp( m[i], "OnlyShadow" ) == 0 )
				flag |= EXPP_MAT_MODE_ONLYSHADOW;
			else if( strcmp( m[i], "NoMist" ) == 0 )
				flag |= EXPP_MAT_MODE_NOMIST;
			else if( strcmp( m[i], "Env" ) == 0 )
				flag |= EXPP_MAT_MODE_ENV;
			/* ** */
			else
				return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
								"unknown Material mode argument" ) );
		}
	    	ok = 1;
	}

	/* if neither input method worked, then throw an exception */

	if ( ok == 0 )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected nothing, an integer or up to 22 string argument(s)" ) );

	/* update the mode flag, return None */

	self->material->mode = flag;

	Py_INCREF( Py_None );
	return Py_None;
}

/* Another helper function, for the same reason.
 * (See comment before Material_setIntType above). */
static PyObject *Material_setIntMode( BPy_Material * self, PyObject * args )
{
	int value;

	if( !PyArg_ParseTuple( args, "i", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );

	self->material->mode = value;

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Material_setRGBCol( BPy_Material * self, PyObject * args )
{
	return rgbTuple_setCol( self->col, args );
}

/*
static PyObject *Material_setAmbCol (BPy_Material *self, PyObject *args)
{
	return rgbTuple_setCol(self->amb, args);
}
*/
static PyObject *Material_setSpecCol( BPy_Material * self, PyObject * args )
{
	return rgbTuple_setCol( self->spec, args );
}

static PyObject *Material_setMirCol( BPy_Material * self, PyObject * args )
{
	return rgbTuple_setCol( self->mir, args );
}


static PyObject *Material_setSpecShader( BPy_Material * self, PyObject * args )
{
	int value;

	if( !PyArg_ParseTuple( args, "i", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );

	self->material->spec_shader = (short)EXPP_ClampInt( value, EXPP_MAT_SPEC_SHADER_MIN,
					       EXPP_MAT_SPEC_SHADER_MAX );
	
	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Material_setDiffuseShader( BPy_Material * self, PyObject * args )
{
	int value;

	if( !PyArg_ParseTuple( args, "i", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument" ) );

	self->material->diff_shader = (short)EXPP_ClampInt( value, EXPP_MAT_DIFFUSE_SHADER_MIN,
					       EXPP_MAT_DIFFUSE_SHADER_MAX );
	
	Py_INCREF( Py_None );
	return Py_None;
}


static PyObject *Material_setRoughness( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 3.14]" ) );

	self->material->roughness = EXPP_ClampFloat( value, EXPP_MAT_ROUGHNESS_MIN,
						EXPP_MAT_ROUGHNESS_MAX );

	return EXPP_incr_ret( Py_None );
}


static PyObject *Material_setSpecSize( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.53]" ) );

	self->material->param[2] = EXPP_ClampFloat( value, EXPP_MAT_SPECSIZE_MIN,
						EXPP_MAT_SPECSIZE_MAX );

	return EXPP_incr_ret( Py_None );
}


static PyObject *Material_setDiffuseSize( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 3.14]" ) );

	self->material->param[0] = EXPP_ClampFloat( value, EXPP_MAT_DIFFUSESIZE_MIN,
						EXPP_MAT_DIFFUSESIZE_MAX );

	return EXPP_incr_ret( Py_None );
}


static PyObject *Material_setSpecSmooth( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->param[2] = EXPP_ClampFloat( value, EXPP_MAT_SPECSMOOTH_MIN,
						EXPP_MAT_SPECSMOOTH_MAX );

	return EXPP_incr_ret( Py_None );
}


static PyObject *Material_setDiffuseSmooth( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->param[1] = EXPP_ClampFloat( value, EXPP_MAT_DIFFUSESMOOTH_MIN,
						EXPP_MAT_DIFFUSESMOOTH_MAX );

	return EXPP_incr_ret( Py_None );
}
	

static PyObject *Material_setDiffuseDarkness( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 2.0]" ) );

	self->material->darkness = EXPP_ClampFloat( value, EXPP_MAT_DIFFUSE_DARKNESS_MIN,
						EXPP_MAT_DIFFUSE_DARKNESS_MAX );

	return EXPP_incr_ret( Py_None );
}


static PyObject *Material_setRefracIndex( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [1.0, 10.0]" ) );

	self->material->refrac = EXPP_ClampFloat( value, EXPP_MAT_REFRACINDEX_MIN,
						EXPP_MAT_REFRACINDEX_MAX );

	return EXPP_incr_ret( Py_None );
}


static PyObject *Material_setRms( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 0.4]" ) );

	self->material->rms = EXPP_ClampFloat( value, EXPP_MAT_RMS_MIN,
						EXPP_MAT_RMS_MAX );

	return EXPP_incr_ret( Py_None );
}


static PyObject *Material_setColorComponent( BPy_Material * self, char *key,
					     PyObject * args )
{				/* for compatibility with old bpython */
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	value = EXPP_ClampFloat( value, EXPP_MAT_COL_MIN, EXPP_MAT_COL_MAX );

	if( !strcmp( key, "R" ) )
		self->material->r = value;
	else if( !strcmp( key, "G" ) )
		self->material->g = value;
	else if( !strcmp( key, "B" ) )
		self->material->b = value;
	else if( !strcmp( key, "specR" ) )
		self->material->specr = value;
	else if( !strcmp( key, "specG" ) )
		self->material->specg = value;
	else if( !strcmp( key, "specB" ) )
		self->material->specb = value;

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setAmb( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->amb = EXPP_ClampFloat( value, EXPP_MAT_AMB_MIN,
					       EXPP_MAT_AMB_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setEmit( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->emit = EXPP_ClampFloat( value, EXPP_MAT_EMIT_MIN,
						EXPP_MAT_EMIT_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setSpecTransp( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->spectra = EXPP_ClampFloat( value, EXPP_MAT_SPECTRA_MIN,
						   EXPP_MAT_SPECTRA_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setAlpha( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->alpha = EXPP_ClampFloat( value, EXPP_MAT_ALPHA_MIN,
						 EXPP_MAT_ALPHA_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setRef( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->ref = EXPP_ClampFloat( value, EXPP_MAT_REF_MIN,
					       EXPP_MAT_REF_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setSpec( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->spec = EXPP_ClampFloat( value, EXPP_MAT_SPEC_MIN,
						EXPP_MAT_SPEC_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setZOffset( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 10.0]" ) );

	self->material->zoffs = EXPP_ClampFloat( value, EXPP_MAT_ZOFFS_MIN,
						 EXPP_MAT_ZOFFS_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setAdd( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->add = EXPP_ClampFloat( value, EXPP_MAT_ADD_MIN,
					       EXPP_MAT_ADD_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setHaloSize( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 100.0]" ) );

	self->material->hasize = EXPP_ClampFloat( value, EXPP_MAT_HALOSIZE_MIN,
						  EXPP_MAT_HALOSIZE_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setFlareSize( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.1, 25.0]" ) );

	self->material->flaresize =
		EXPP_ClampFloat( value, EXPP_MAT_FLARESIZE_MIN,
				 EXPP_MAT_FLARESIZE_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setFlareBoost( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.1, 10.0]" ) );

	self->material->flareboost =
		EXPP_ClampFloat( value, EXPP_MAT_FLAREBOOST_MIN,
				 EXPP_MAT_FLAREBOOST_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setSubSize( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.1, 25.0]" ) );

	self->material->subsize = EXPP_ClampFloat( value, EXPP_MAT_SUBSIZE_MIN,
						   EXPP_MAT_SUBSIZE_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setHaloSeed( BPy_Material * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [1, 255]" ) );

	self->material->seed1 = (char)EXPP_ClampInt( value, EXPP_MAT_HALOSEED_MIN,
					       EXPP_MAT_HALOSEED_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setFlareSeed( BPy_Material * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [1, 255]" ) );

	self->material->seed2 = (char)EXPP_ClampInt( value, EXPP_MAT_FLARESEED_MIN,
					       EXPP_MAT_FLARESEED_MAX );

	return EXPP_incr_ret( Py_None );
}


static PyObject *Material_setHardness( BPy_Material * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [1, 255]" ) );

	self->material->har = (short)EXPP_ClampInt( value, EXPP_MAT_HARD_MIN,
					     EXPP_MAT_HARD_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setNFlares( BPy_Material * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [1, 32]" ) );

	self->material->flarec = (short)EXPP_ClampInt( value, EXPP_MAT_NFLARES_MIN,
						EXPP_MAT_NFLARES_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setNStars( BPy_Material * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [3, 50]" ) );

	self->material->starc = (short)EXPP_ClampInt( value, EXPP_MAT_NSTARS_MIN,
					       EXPP_MAT_NSTARS_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setNLines( BPy_Material * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [0, 250]" ) );

	self->material->linec = (short)EXPP_ClampInt( value, EXPP_MAT_NLINES_MIN,
					       EXPP_MAT_NLINES_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setNRings( BPy_Material * self, PyObject * args )
{
	short value;

	if( !PyArg_ParseTuple( args, "h", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected int argument in [0, 24]" ) );

	self->material->ringc = (short)EXPP_ClampInt( value, EXPP_MAT_NRINGS_MIN,
					       EXPP_MAT_NRINGS_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setRayMirr( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->ray_mirror =
		EXPP_ClampFloat( value, EXPP_MAT_RAYMIRR_MIN,
				 EXPP_MAT_RAYMIRR_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setMirrDepth( BPy_Material * self, PyObject * args )
{
	int value;

	if( !PyArg_ParseTuple( args, "i", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0, 10]" ) );

	self->material->ray_depth =
		(short)EXPP_ClampInt( value, EXPP_MAT_MIRRDEPTH_MIN,
			       EXPP_MAT_MIRRDEPTH_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setFresnelMirr( BPy_Material * self,
					  PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 5.0]" ) );

	self->material->fresnel_mir =
		EXPP_ClampFloat( value, EXPP_MAT_FRESNELMIRR_MIN,
				 EXPP_MAT_FRESNELMIRR_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setFresnelMirrFac( BPy_Material * self,
					     PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 5.0]" ) );

	self->material->fresnel_mir_i =
		EXPP_ClampFloat( value, EXPP_MAT_FRESNELMIRRFAC_MIN,
				 EXPP_MAT_FRESNELMIRRFAC_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setFilter( BPy_Material * self,
					     PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->filter =
		EXPP_ClampFloat( value, EXPP_MAT_FILTER_MIN,
				 EXPP_MAT_FILTER_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setTranslucency( BPy_Material * self,
					     PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 1.0]" ) );

	self->material->translucency =
		EXPP_ClampFloat( value, EXPP_MAT_TRANSLUCENCY_MIN,
				 EXPP_MAT_TRANSLUCENCY_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setIOR( BPy_Material * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 5.0]" ) );

	self->material->ang = EXPP_ClampFloat( value, EXPP_MAT_IOR_MIN,
					       EXPP_MAT_IOR_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setTransDepth( BPy_Material * self, PyObject * args )
{
	int value;

	if( !PyArg_ParseTuple( args, "i", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0, 10]" ) );

	self->material->ray_depth_tra =
		(short)EXPP_ClampInt( value, EXPP_MAT_TRANSDEPTH_MIN,
			       EXPP_MAT_TRANSDEPTH_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setFresnelTrans( BPy_Material * self,
					   PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 5.0]" ) );

	self->material->fresnel_tra =
		EXPP_ClampFloat( value, EXPP_MAT_FRESNELTRANS_MIN,
				 EXPP_MAT_FRESNELTRANS_MAX );

	return EXPP_incr_ret( Py_None );
}

static PyObject *Material_setFresnelTransFac( BPy_Material * self,
					      PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected float argument in [0.0, 5.0]" ) );

	self->material->fresnel_tra_i =
		EXPP_ClampFloat( value, EXPP_MAT_FRESNELTRANSFAC_MIN,
				 EXPP_MAT_FRESNELTRANSFAC_MAX );

	return EXPP_incr_ret( Py_None );

}

static PyObject *Material_setTexture( BPy_Material * self, PyObject * args )
{
	int texnum;
	PyObject *pytex;
	Tex *bltex;
	int texco = TEXCO_ORCO, mapto = MAP_COL;

	if( !PyArg_ParseTuple( args, "iO!|ii", &texnum, &Texture_Type, &pytex,
			       &texco, &mapto ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int in [0,9] and Texture" );
	if( ( texnum < 0 ) || ( texnum >= MAX_MTEX ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int in [0,9] and Texture" );

	bltex = Texture_FromPyObject( pytex );

	if( !self->material->mtex[texnum] ) {
		/* there isn't an mtex for this slot so we need to make one */
		self->material->mtex[texnum] = add_mtex(  );
	} else {
		/* we already had a texture here so deal with the old one first */
		self->material->mtex[texnum]->tex->id.us--;
	}

	self->material->mtex[texnum]->tex = bltex;
	id_us_plus( &bltex->id );
	self->material->mtex[texnum]->texco = (short)texco;
	self->material->mtex[texnum]->mapto = (short)mapto;

	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Material_clearTexture( BPy_Material * self, PyObject * args )
{
	int texnum;
	struct MTex *mtex;

	if( !PyArg_ParseTuple( args, "i", &texnum ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int in [0,9]" );
	if( ( texnum < 0 ) || ( texnum >= MAX_MTEX ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected int in [0,9]" );

	mtex = self->material->mtex[texnum];
	if( mtex ) {
		if( mtex->tex )
			mtex->tex->id.us--;
		MEM_freeN( mtex );
		self->material->mtex[texnum] = NULL;
	}

	Py_INCREF( Py_None );
	return Py_None;
}

/* mat.addScriptLink */
static PyObject *Material_addScriptLink( BPy_Material * self, PyObject * args )
{
	Material *mat = self->material;
	ScriptLink *slink = NULL;

	slink = &( mat )->scriptlink;

	return EXPP_addScriptLink( slink, args, 0 );
}

/* mat.clearScriptLinks */
static PyObject *Material_clearScriptLinks(BPy_Material *self, PyObject *args )
{
	Material *mat = self->material;
	ScriptLink *slink = NULL;

	slink = &( mat )->scriptlink;

	return EXPP_clearScriptLinks( slink, args );
}

/* mat.getScriptLinks */
static PyObject *Material_getScriptLinks( BPy_Material * self,
					  PyObject * args )
{
	Material *mat = self->material;
	ScriptLink *slink = NULL;
	PyObject *ret = NULL;

	slink = &( mat )->scriptlink;

	ret = EXPP_getScriptLinks( slink, args, 0 );

	if( ret )
		return ret;
	else
		return NULL;
}

/*****************************************************************************/
/* Function:	Material_getAttr	 */
/* Description: This is a callback function for the BPy_Material type. It is */
/*		the function that accesses BPy_Material "member variables"  */
/*		and methods.                                         */
/*****************************************************************************/
static PyObject *Material_getAttr( BPy_Material * self, char *name )
{
	PyObject *attr = Py_None;

	if( strcmp( name, "name" ) == 0 )
		attr = PyString_FromString( self->material->id.name + 2 );
	else if( strcmp( name, "mode" ) == 0 )
		attr = PyInt_FromLong( self->material->mode );
	else if( strcmp( name, "rgbCol" ) == 0 )
		attr = Material_getRGBCol( self );
/*	else if (strcmp(name, "ambCol") == 0)
		attr = Material_getAmbCol(self);*/
	else if( strcmp( name, "specCol" ) == 0 )
		attr = Material_getSpecCol( self );
	else if( strcmp( name, "mirCol" ) == 0 )
		attr = Material_getMirCol( self );
	else if( strcmp( name, "R" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->r );
	else if( strcmp( name, "G" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->g );
	else if( strcmp( name, "B" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->b );
	else if( strcmp( name, "specR" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->specr );
	else if( strcmp( name, "specG" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->specg );
	else if( strcmp( name, "specB" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->specb );
	else if( strcmp( name, "amb" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->amb );
	else if( strcmp( name, "emit" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->emit );
	else if( strcmp( name, "alpha" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->alpha );
	else if( strcmp( name, "ref" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->ref );
	else if( strcmp( name, "spec" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->spec );
	else if( strcmp( name, "specTransp" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   spectra );
	else if( strcmp( name, "add" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->add );
	else if( strcmp( name, "zOffset" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->zoffs );
	else if( strcmp( name, "haloSize" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->hasize );
	else if( strcmp( name, "haloSeed" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->seed1 );
	else if( strcmp( name, "flareSize" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   flaresize );
	else if( strcmp( name, "flareBoost" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   flareboost );
	else if( strcmp( name, "flareSeed" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->seed2 );
	else if( strcmp( name, "subSize" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   subsize );
	else if( strcmp( name, "hard" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->har );
	else if( strcmp( name, "nFlares" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->flarec );
	else if( strcmp( name, "nStars" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->starc );
	else if( strcmp( name, "nLines" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->linec );
	else if( strcmp( name, "nRings" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->ringc );
	else if( strcmp( name, "rayMirr" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   ray_mirror );
	else if( strcmp( name, "rayMirrDepth" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->ray_depth );
	else if( strcmp( name, "fresnelDepth" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   fresnel_mir );
	else if( strcmp( name, "fresnelDepthFac" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   fresnel_mir_i );
	else if( strcmp( name, "filter" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   filter );
	else if( strcmp( name, "translucency" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   translucency );
	else if( strcmp( name, "IOR" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->ang );
	else if( strcmp( name, "transDepth" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->
				       ray_depth_tra );
	else if( strcmp( name, "fresnelTrans" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   fresnel_tra );
	else if( strcmp( name, "fresnelTransFac" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   fresnel_tra_i );
	else if( strcmp( name, "users" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->
					   id.us );
	/* Shader settings*/
	else if( strcmp( name, "specShader" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->
					   spec_shader );
	else if( strcmp( name, "diffuseShader" ) == 0 )
		attr = PyInt_FromLong( ( long ) self->material->
					   diff_shader );
	else if( strcmp( name, "roughness" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   roughness );
	else if( strcmp( name, "specSize" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   param[2] );
	else if( strcmp( name, "diffuseSize" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   param[0] );
	else if( strcmp( name, "specSmooth" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   param[3] );
	else if( strcmp( name, "diffuseSmooth" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   param[1] );
	else if( strcmp( name, "diffuseDarkness" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   darkness );
	else if( strcmp( name, "refracIndex" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   refrac );
	else if( strcmp( name, "rms" ) == 0 )
		attr = PyFloat_FromDouble( ( double ) self->material->
					   rms );
	
  else if (strcmp(name, "oopsLoc") == 0) {
    if (G.soops) { 
      Oops *oops= G.soops->oops.first;
      while(oops) {
        if(oops->type==ID_MA) {
          if ((Material *)oops->id == self->material) {
            return (Py_BuildValue ("ff", oops->x, oops->y));
          }
        }
        oops= oops->next;
      }
    }
    Py_INCREF (Py_None);
    return (Py_None);
  }
  /* Select in the oops view only since its a mesh */
  else if (strcmp(name, "oopsSel") == 0) {
    if (G.soops) {
      Oops *oops= G.soops->oops.first;
      while(oops) {
        if(oops->type==ID_MA) {
          if ((Material *)oops->id == self->material) {
            if (oops->flag & SELECT)
							return EXPP_incr_ret_True();
            else
							return EXPP_incr_ret_False();
          }
        }
        oops= oops->next;
      }
    }
    Py_INCREF (Py_None);
		return (Py_None);    
  }    
	else if( strcmp( name, "__members__" ) == 0 ) {
		attr =		/* 30 items */
			Py_BuildValue
			( "[s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,					s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s]",
			  "name", "mode", "rgbCol", "specCol", "mirCol", "R",
			  "G", "B", "alpha", "amb", "emit", "ref",
				"spec", "specTransp", "add", "zOffset", "haloSize", "haloSeed",
				"flareSize", "flareBoost", "flareSeed", "subSize", "hard", "nFlares",
				"nStars", "nLines", "nRings", "rayMirr", "rayMirrDepth", "fresnelDepth",
			  "fresnelDepthFac", "IOR", "transDepth",
				"fresnelTrans", "fresnelTransFac", "users",
				"oopsLoc", "oopsSel", "filter", "translucency", "shader", "roughness",
				"specSize", "diffuseSize", "specSmooth",
			  "diffuseSmooth", "diffuseDarkness", "refracIndex", "rms");
	}	
	
	if( !attr )
		return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
						"couldn't create PyObject" ) );

	if( attr != Py_None )
		return attr;	/* member attribute found, return it */

	/* not an attribute, search the methods table */
	return Py_FindMethod( BPy_Material_methods, ( PyObject * ) self,
			      name );
}

/****************************************************************************/
/* Function:	Material_setAttr	*/
/* Description: This is a callback function for the BPy_Material type.	*/
/*		It is the function that sets Material attributes (member */
/*		variables).				*/
/****************************************************************************/
static int Material_setAttr( BPy_Material * self, char *name,
			     PyObject * value )
{
	PyObject *valtuple;
	PyObject *error = NULL;

/* We're playing a trick on the Python API users here.	Even if they use
 * Material.member = val instead of Material.setMember(val), we end up using 
 * the  function anyway, since it already has error checking, clamps to the 
 * right  interval and updates the Blender Material structure when necessary. 
 */

/* First we put "value" in a tuple, because we want to pass it to functions
 * that only accept PyTuples. */
	valtuple = Py_BuildValue( "(O)", value );

	if( !valtuple )		/* everything OK with our PyObject? */
		return EXPP_ReturnIntError( PyExc_MemoryError,
					    "MaterialSetAttr: couldn't create PyTuple" );

/* Now we just compare "name" with all possible BPy_Material member variables */
	if( strcmp( name, "name" ) == 0 )
		error = Material_setName( self, valtuple );
	else if( strcmp( name, "mode" ) == 0 )
		error = Material_setIntMode( self, valtuple );	/* special case */
	else if( strcmp( name, "rgbCol" ) == 0 )
		error = Material_setRGBCol( self, valtuple );
/*	else if (strcmp (name, "ambCol") == 0)
		error = Material_setAmbCol (self, valtuple);*/
	else if( strcmp( name, "specCol" ) == 0 )
		error = Material_setSpecCol( self, valtuple );
	else if( strcmp( name, "mirCol" ) == 0 )
		error = Material_setMirCol( self, valtuple );
	else if( strcmp( name, "R" ) == 0 )
		error = Material_setColorComponent( self, "R", valtuple );
	else if( strcmp( name, "G" ) == 0 )
		error = Material_setColorComponent( self, "G", valtuple );
	else if( strcmp( name, "B" ) == 0 )
		error = Material_setColorComponent( self, "B", valtuple );
	else if( strcmp( name, "specR" ) == 0 )
		error = Material_setColorComponent( self, "specR", valtuple );
	else if( strcmp( name, "specG" ) == 0 )
		error = Material_setColorComponent( self, "specG", valtuple );
	else if( strcmp( name, "specB" ) == 0 )
		error = Material_setColorComponent( self, "specB", valtuple );
	else if( strcmp( name, "amb" ) == 0 )
		error = Material_setAmb( self, valtuple );
	else if( strcmp( name, "emit" ) == 0 )
		error = Material_setEmit( self, valtuple );
	else if( strcmp( name, "alpha" ) == 0 )
		error = Material_setAlpha( self, valtuple );
	else if( strcmp( name, "ref" ) == 0 )
		error = Material_setRef( self, valtuple );
	else if( strcmp( name, "spec" ) == 0 )
		error = Material_setSpec( self, valtuple );
	else if( strcmp( name, "specTransp" ) == 0 )
		error = Material_setSpecTransp( self, valtuple );
	else if( strcmp( name, "add" ) == 0 )
		error = Material_setAdd( self, valtuple );
	else if( strcmp( name, "zOffset" ) == 0 )
		error = Material_setZOffset( self, valtuple );
	else if( strcmp( name, "haloSize" ) == 0 )
		error = Material_setHaloSize( self, valtuple );
	else if( strcmp( name, "haloSeed" ) == 0 )
		error = Material_setHaloSeed( self, valtuple );
	else if( strcmp( name, "flareSize" ) == 0 )
		error = Material_setFlareSize( self, valtuple );
	else if( strcmp( name, "flareBoost" ) == 0 )
		error = Material_setFlareBoost( self, valtuple );
	else if( strcmp( name, "flareSeed" ) == 0 )
		error = Material_setFlareSeed( self, valtuple );
	else if( strcmp( name, "subSize" ) == 0 )
		error = Material_setSubSize( self, valtuple );
	else if( strcmp( name, "hard" ) == 0 )
		error = Material_setHardness( self, valtuple );
	else if( strcmp( name, "nFlares" ) == 0 )
		error = Material_setNFlares( self, valtuple );
	else if( strcmp( name, "nStars" ) == 0 )
		error = Material_setNStars( self, valtuple );
	else if( strcmp( name, "nLines" ) == 0 )
		error = Material_setNLines( self, valtuple );
	else if( strcmp( name, "nRings" ) == 0 )
		error = Material_setNRings( self, valtuple );
	else if( strcmp( name, "rayMirr" ) == 0 )
		error = Material_setRayMirr( self, valtuple );
	else if( strcmp( name, "rayMirrDepth" ) == 0 )
		error = Material_setMirrDepth( self, valtuple );
	else if( strcmp( name, "fresnelDepth" ) == 0 )
		error = Material_setFresnelMirr( self, valtuple );
	else if( strcmp( name, "fresnelDepthFac" ) == 0 )
		error = Material_setFresnelMirrFac( self, valtuple );
	else if( strcmp( name, "filter" ) == 0 )
		error = Material_setFilter( self, valtuple );
	else if( strcmp( name, "translucency" ) == 0 )
		error = Material_setTranslucency( self, valtuple );
	else if( strcmp( name, "IOR" ) == 0 )
		error = Material_setIOR( self, valtuple );
	else if( strcmp( name, "transDepth" ) == 0 )
		error = Material_setTransDepth( self, valtuple );
	else if( strcmp( name, "fresnelTrans" ) == 0 )
		error = Material_setFresnelTrans( self, valtuple );
	else if( strcmp( name, "fresnelTransFac" ) == 0 )
		error = Material_setFresnelTransFac( self, valtuple );
	/* Shader settings */
	else if( strcmp( name, "specShader" ) == 0 )
		error = Material_setSpecShader( self, valtuple );
	else if( strcmp( name, "diffuseShader" ) == 0 )
		error = Material_setDiffuseShader( self, valtuple );	
	else if( strcmp( name, "roughness" ) == 0 )
		error = Material_setRoughness( self, valtuple );
	else if( strcmp( name, "specSize" ) == 0 )
		error = Material_setSpecSize( self, valtuple );
	else if( strcmp( name, "diffuseSize" ) == 0 )
		error = Material_setDiffuseSize( self, valtuple );
	else if( strcmp( name, "specSmooth" ) == 0 )
		error = Material_setSpecSmooth( self, valtuple );
	else if( strcmp( name, "diffuseSmooth" ) == 0 )
		error = Material_setDiffuseSmooth( self, valtuple );
	else if( strcmp( name, "diffuseDarkness" ) == 0 )
		error = Material_setDiffuseDarkness( self, valtuple );
	else if( strcmp( name, "refracIndex" ) == 0 )
		error = Material_setRefracIndex( self, valtuple );
	else if( strcmp( name, "rms" ) == 0 )
		error = Material_setRms( self, valtuple );	
  else if (strcmp (name, "oopsLoc") == 0) {
    if (G.soops) {
      Oops *oops= G.soops->oops.first;
      while(oops) {
        if(oops->type==ID_MA) {
          if ((Material *)oops->id == self->material) {
            if (!PyArg_ParseTuple  (value, "ff", &(oops->x),&(oops->y)))
							PyErr_SetString(PyExc_AttributeError,
								"expected two floats as arguments");
            break;
          }
        }
        oops= oops->next;
      }
			if (!oops)
				PyErr_SetString(PyExc_RuntimeError,
					"couldn't find oopsLoc data for this material!");
			else error = EXPP_incr_ret (Py_None);
    }
  }
  /* Select in the oops view only since its a mesh */
  else if (strcmp (name, "oopsSel") == 0) {
    int sel;
    if (!PyArg_Parse (value, "i", &sel))
      PyErr_SetString (PyExc_TypeError, "expected an integer, 0 or 1");
		else if (G.soops) {
      Oops *oops= G.soops->oops.first;
      while(oops) {
        if(oops->type==ID_MA) {
          if ((Material *)oops->id == self->material) {
            if(sel == 0) oops->flag &= ~SELECT;
            else oops->flag |= SELECT;
            break;
          }
        }
        oops= oops->next;
      }
    	error = EXPP_incr_ret (Py_None);
    }
  }	
	else {			/* Error */
		Py_DECREF( valtuple );
		return ( EXPP_ReturnIntError( PyExc_AttributeError, name ) );
	}

/* valtuple won't be returned to the caller, so we need to DECREF it */
	Py_DECREF( valtuple );

	if( error != Py_None )
		return -1;

/* Py_None was incref'ed by the called Material_set* function. We probably
 * don't need to decref Py_None (!), but since Python/C API manual tells us
 * to treat it like any other PyObject regarding ref counting ... */
	Py_DECREF( Py_None );
	return 0;		/* normal exit */
}

/*****************************************************************************/
/* Function:	Material_repr	 */
/* Description: This is a callback function for the BPy_Material type. It  */
/*		 builds a meaninful string to represent material objects.   */
/*****************************************************************************/
static PyObject *Material_repr( BPy_Material * self )
{
	return PyString_FromFormat( "[Material \"%s\"]",
				    self->material->id.name + 2 );
}

/*****************************************************************************/
/* These functions are used in NMesh.c and Object.c	 */
/*****************************************************************************/
PyObject *EXPP_PyList_fromMaterialList( Material ** matlist, int len, int all )
{
	PyObject *list;
	int i;

	list = PyList_New( 0 );
	if( !matlist )
		return list;

	for( i = 0; i < len; i++ ) {
		Material *mat = matlist[i];
		PyObject *ob;

		if( mat ) {
			ob = Material_CreatePyObject( mat );
			PyList_Append( list, ob );
			Py_DECREF( ob );	/* because Append increfs */
		} else if( all ) {	/* return NULL mats (empty slots) as Py_None */
			PyList_Append( list, Py_None );
		}
	}

	return list;
}

Material **EXPP_newMaterialList_fromPyList( PyObject * list )
{
	int i, len;
	BPy_Material *pymat = 0;
	Material *mat;
	Material **matlist;

	len = PySequence_Length( list );
	if( len > 16 )
		len = 16;
	else if( len <= 0 )
		return NULL;

	matlist = EXPP_newMaterialList( len );

	for( i = 0; i < len; i++ ) {

		pymat = ( BPy_Material * ) PySequence_GetItem( list, i );

		if( Material_CheckPyObject( ( PyObject * ) pymat ) ) {
			mat = pymat->material;
			matlist[i] = mat;
		} else if( ( PyObject * ) pymat == Py_None ) {
			matlist[i] = NULL;
		} else {	/* error; illegal type in material list */
			Py_DECREF( pymat );
			MEM_freeN( matlist );
			return NULL;
		}

		Py_DECREF( pymat );
	}

	return matlist;
}

Material **EXPP_newMaterialList( int len )
{
	Material **matlist =
		( Material ** ) MEM_mallocN( len * sizeof( Material * ),
					     "MaterialList" );

	return matlist;
}

int EXPP_releaseMaterialList( Material ** matlist, int len )
{
	int i;
	Material *mat;

	if( ( len < 0 ) || ( len > MAXMAT ) ) {
		printf( "illegal matindex!\n" );
		return 0;
	}

	for( i = 0; i < len; i++ ) {
		mat = matlist[i];
		if( mat ) {
			if( ( ( ID * ) mat )->us > 0 )
				( ( ID * ) mat )->us--;
			else
				printf( "FATAL: material usage=0: %s",
					( ( ID * ) mat )->name );
		}
	}
	MEM_freeN( matlist );

	return 1;
}

/** expands pointer array of length 'oldsize' to length 'newsize'.
	* A pointer to the (void *) array must be passed as first argument 
	* The array pointer content can be NULL, in this case a new array of length
	* 'newsize' is created.
	*/

static int expandPtrArray( void **p, int oldsize, int newsize )
{
	void *newarray;

	if( newsize < oldsize ) {
		return 0;
	}
	newarray = MEM_callocN( sizeof( void * ) * newsize, "PtrArray" );
	if( *p ) {
		memcpy( newarray, *p, sizeof( void * ) * oldsize );
		MEM_freeN( *p );
	}
	*p = newarray;
	return 1;
}

int EXPP_synchronizeMaterialLists( Object * object )
{
	Material ***p_dataMaterials = give_matarar( object );
	short *nmaterials = give_totcolp( object );
	int result = 0;

	if( object->totcol > *nmaterials ) {
		/* More object mats than data mats */
		result = expandPtrArray( ( void * ) p_dataMaterials,
					 *nmaterials, object->totcol );
		*nmaterials = object->totcol;
	} else {
		if( object->totcol < *nmaterials ) {
			/* More data mats than object mats */
			result = expandPtrArray( ( void * ) &object->mat,
						 object->totcol, *nmaterials );
			object->totcol = (char)*nmaterials;
		}
	}			/* else no synchronization needed, they are of equal length */

	return result;		/* 1 if changed, 0 otherwise */
}

void EXPP_incr_mats_us( Material ** matlist, int len )
{
	int i;
	Material *mat;

	if( len <= 0 )
		return;

	for( i = 0; i < len; i++ ) {
		mat = matlist[i];
		if( mat )
			mat->id.us++;
	}

	return;
}
