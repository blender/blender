/*
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
 * Contributor(s): Joilnen Leite
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Font.h" /*This must come first*/

#include "DNA_packedFile_types.h"
#include "BKE_packedFile.h"
#include "BKE_global.h"
#include "BKE_library.h" /* for rename_id() */
#include "BLI_blenlib.h"
#include "gen_utils.h"
#include "gen_library.h"

#include "BKE_main.h" /* so we can access G.main->vfont.first */
#include "DNA_space_types.h" /* for FILE_MAXDIR only */

extern PyObject *M_Text3d_LoadFont( PyObject * self, PyObject * args );

/*--------------------Python API function prototypes for the Font module----*/
static PyObject *M_Font_Load( PyObject * self, PyObject * args );
static PyObject *M_Font_Get( PyObject * self, PyObject * args );

/*------------------------Python API Doc strings for the Font module--------*/
char M_Font_doc[] = "The Blender Font module\n\n\
This module provides control over **Font Data** objects in Blender.\n\n\
Example::\n\n\
	from Blender import Text3d.Font\n\
	l = Text3d.Font.Load('/usr/share/fonts/verdata.ttf')\n";
char M_Font_Get_doc[] = "(name) - return an existing font called 'name'\
when no argument is given it returns a list of blenders fonts.";
char M_Font_Load_doc[] =
	"(filename) - return font from file filename as Font Object, \
returns None if not found.\n";

/*----- Python method structure definition for Blender.Text3d.Font module---*/
struct PyMethodDef M_Font_methods[] = {
	{"Get", ( PyCFunction ) M_Font_Get, METH_VARARGS, M_Font_Get_doc},
	{"Load", ( PyCFunction ) M_Font_Load, METH_VARARGS, M_Font_Load_doc},
	{NULL, NULL, 0, NULL}
};

/*--------------- Python BPy_Font methods declarations:-------------------*/
static PyObject *Font_pack( BPy_Font * self );
static PyObject *Font_unpack( BPy_Font * self, PyObject * args );

/*--------------- Python BPy_Font methods table:--------------------------*/
static PyMethodDef BPy_Font_methods[] = {
	{"pack", ( PyCFunction ) Font_pack, METH_NOARGS,
	 "() - pack this Font"},
	{"unpack", ( PyCFunction ) Font_unpack, METH_VARARGS,
	 "(mode) - unpack this packed font"},
	{NULL, NULL, 0, NULL}
};

/*--------------- Python TypeFont callback function prototypes----------*/
static int Font_compare( BPy_Font * a1, BPy_Font * a2 );
static PyObject *Font_repr( BPy_Font * font );


/*--------------- Python Font Module methods------------------------*/

/*--------------- Blender.Text3d.Font.Get()-----------------------*/
static PyObject *M_Font_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	VFont *vfont_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	vfont_iter = G.main->vfont.first;

	if( name ) {		/* (name) - Search font by name */

		BPy_Font *wanted_vfont = NULL;

		while(vfont_iter) {
			if( strcmp( name, vfont_iter->id.name + 2 ) == 0 ) {
				wanted_vfont =
					( BPy_Font * )
					Font_CreatePyObject( vfont_iter );
				break;
			}
			vfont_iter = vfont_iter->id.next;
		}

		if( wanted_vfont == NULL ) { /* Requested font doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Font \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) wanted_vfont;
	}

	else {		/* () - return a list of all fonts in the scene */
		int index = 0;
		PyObject *vfontlist, *pyobj;

		vfontlist = PyList_New( BLI_countlist( &( G.main->vfont ) ) );

		if( vfontlist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create font list" ) );

		while( vfont_iter ) {
			pyobj = Font_CreatePyObject( vfont_iter );

			if( !pyobj ) {
				Py_DECREF(vfontlist);
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create Object" ) );
			}
			PyList_SET_ITEM( vfontlist, index, pyobj );

			vfont_iter = vfont_iter->id.next;
			index++;
		}

		return vfontlist;
	}
}


/*--------------- Blender.Text3d.Font.New()-----------------------*/
PyObject *M_Font_Load( PyObject * self, PyObject * args )
{
	char *filename_str;
	BPy_Font *py_font = NULL;	/* for Font Data object wrapper in Python */
	PyObject *tmp; 

	if( !PyArg_ParseTuple( args, "s", &filename_str ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string or empty argument" ) );

	/*create python font*/
	if( !S_ISDIR(BLI_exist(filename_str)) )  {
		tmp= Py_BuildValue("(s)", filename_str);
		py_font= (BPy_Font *) M_Text3d_LoadFont (self, tmp);
		Py_DECREF (tmp);
	}
	else
		Py_RETURN_NONE;
	return ( PyObject * ) py_font;
}

/*--------------- Python BPy_Font getsetattr funcs ---------------------*/


/*--------------- BPy_Font.filename-------------------------------------*/
static PyObject *Font_getFilename( BPy_Font * self )
{
	PyObject *attr = NULL;

	if( self->font )
		attr = PyString_FromString( self->font->name );
	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Font.filename attribute" ) );
}

static int Font_setFilename( BPy_Font * self, PyObject * value )
{
	char *name = NULL;

	/* max len is FILE_MAXDIR = 160 chars like done in DNA_image_types.h */
	
	name = PyString_AsString ( value );
	if( !name )
		return EXPP_ReturnIntError( PyExc_TypeError,
					      "expected string argument" );

	PyOS_snprintf( self->font->name, FILE_MAXDIR * sizeof( char ), "%s",
		       name );

	return 0;
}

/*--------------- BPy_Font.pack()---------------------------------*/
static PyObject *Font_pack( BPy_Font * self ) 
{
	if( !self->font->packedfile ) 
		self->font->packedfile = newPackedFile(self->font->name);
	Py_RETURN_NONE;
}

/*--------------- BPy_Font.unpack()---------------------------------*/
static PyObject *Font_unpack( BPy_Font * self, PyObject * args ) 
{
	int mode= 0;
	VFont *font= self->font;
	
	if( !PyArg_ParseTuple( args, "i", &mode ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected int argument from Blender.UnpackModes" ) );
	
	if (font->packedfile)
		if (unpackVFont(font, mode) == RET_ERROR)
                return EXPP_ReturnPyObjError( PyExc_RuntimeError,
                                "error unpacking font" );

	Py_RETURN_NONE;
}

/*--------------- BPy_Font.packed---------------------------------*/
static PyObject *Font_getPacked( BPy_Font * self ) 
{
	if (G.fileflags & G_AUTOPACK)
		return EXPP_incr_ret_True();
	else
		return EXPP_incr_ret_False();
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_Font_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"filename",
	 (getter)Font_getFilename, (setter)Font_setFilename,
	 "Font filepath",
	 NULL},
	{"packed",
	 (getter)Font_getPacked, (setter)NULL,
	 "Packed status",
	 NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python TypeFont structure definition:                                     */
/*****************************************************************************/
PyTypeObject Font_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender Font",             /* char *tp_name; */
	sizeof( BPy_Font ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,						/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Font_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) Font_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc ) GenericLib_hash,	/* hashfunc tp_hash; */
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
	BPy_Font_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Font_getseters,         /* struct PyGetSetDef *tp_getset; */
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






/*--------------- Font Module Init-----------------------------*/
PyObject *Font_Init( void )
{
	PyObject *submodule;

	if( PyType_Ready( &Font_Type ) < 0 )
		return NULL;

	submodule = Py_InitModule3( "Blender.Text3d.Font",
				    M_Font_methods, M_Font_doc );

	return ( submodule );
}

/*--------------- Font module internal callbacks-----------------*/
/*---------------BPy_Font internal callbacks/methods-------------*/

/*--------------- repr---------------------------------------------*/
static PyObject *Font_repr( BPy_Font * self )
{
	if( self->font )
		return PyString_FromFormat( "[Font \"%s\"]",
					    self->font->id.name+2 );
	else
		return PyString_FromString( "[Font - no data]" );
}

/*--------------- compare------------------------------------------*/
static int Font_compare( BPy_Font * a, BPy_Font * b )
{
	return ( a->font == b->font ) ? 0 : -1;
}

/*--------------- Font_CreatePyObject---------------------------------*/
PyObject *Font_CreatePyObject( struct VFont * font )
{
	BPy_Font *blen_font;

	blen_font = ( BPy_Font * ) PyObject_NEW( BPy_Font, &Font_Type );

	blen_font->font = font;

	return ( ( PyObject * ) blen_font );
}

/*--------------- Font_FromPyObject---------------------------------*/
struct VFont *Font_FromPyObject( PyObject * py_obj )
{
	BPy_Font *blen_obj;

	blen_obj = ( BPy_Font * ) py_obj;
	if( !( ( BPy_Font * ) py_obj )->font ) {	/*test to see if linked to text3d*/
		//use python vars
		return NULL;
	} else {
		return ( blen_obj->font );
	}
}



