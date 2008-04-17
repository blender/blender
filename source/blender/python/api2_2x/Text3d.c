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
 * Contributor(s): Joilnen Leite
 *                 Johnny Matthews
 *                 Campbell BArton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Text3d.h" /*This must come first*/
 
#include "DNA_object_types.h"
#include "MEM_guardedalloc.h"
#include "BKE_curve.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BIF_editfont.h"	/* do_textedit() */
#include "Curve.h"
#include "constant.h"
#include "Font.h"
#include "gen_utils.h"
#include "gen_library.h"


enum t3d_consts {
	EXPP_T3D_ATTR_FRAME_WIDTH = 0,
	EXPP_T3D_ATTR_FRAME_HEIGHT,
	EXPP_T3D_ATTR_FRAME_X,
	EXPP_T3D_ATTR_FRAME_Y
};


/*no prototypes declared in header files - external linkage outside of python*/
extern VFont *get_builtin_font(void);  
extern void freedisplist(struct ListBase *lb);
extern VFont *give_vfontpointer(int);
extern VFont *exist_vfont(char *str);
extern VFont *load_vfont(char *name);
extern int BLI_exist(char *name);

/*****************************************************************************/
/* Python API function prototypes for the Text3D module.                     */
/*****************************************************************************/
static PyObject *M_Text3d_New( PyObject * self, PyObject * args );
static PyObject *M_Text3d_Get( PyObject * self, PyObject * args );
PyObject *M_Text3d_LoadFont (PyObject * self, PyObject * args );

/*****************************************************************************
 * Python callback function prototypes for the Text3D module.  
 *****************************************************************************/
static PyObject *return_ModuleConstant( char *constant_name);
static PyObject *generate_ModuleIntConstant(char *name, int value);

/*****************************************************************************/
/* Python method structure definition for Blender.Text3d module:             */
/*****************************************************************************/
struct PyMethodDef M_Text3d_methods[] = {
	{"New", ( PyCFunction ) M_Text3d_New, METH_VARARGS, NULL},
	{"Get", ( PyCFunction ) M_Text3d_Get, METH_VARARGS, NULL},
	{"LoadFont", ( PyCFunction ) M_Text3d_LoadFont, METH_O, NULL},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Text3d_Type callback function prototypes:                          */
/*****************************************************************************/
/* int Text3dPrint (BPy_Text3d *msh, FILE *fp, int flags); */


static PyObject *Text3d_repr( BPy_Text3d * self );
static int Text3d_compare( BPy_Text3d * a, BPy_Text3d * b );

/*****************************************************************************/
/* Python BPy_Text3d methods declarations:                                   */
/*****************************************************************************/
/*PyObject *Text3d_getType(BPy_Text3d *self);*/
static PyObject *Text3d_getName( BPy_Text3d * self );
static PyObject *Text3d_setName( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_setText( BPy_Text3d * self, PyObject * value );
static PyObject *Text3d_getText( BPy_Text3d * self );
static PyObject *Text3d_getDrawMode( BPy_Text3d * self );
static PyObject *Text3d_setDrawMode( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getUVorco( BPy_Text3d * self );
static PyObject *Text3d_setUVorco( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getBevelAmount( BPy_Text3d * self );
static PyObject *Text3d_setBevelAmount( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getDefaultResolution( BPy_Text3d * self );
static PyObject *Text3d_setDefaultResolution( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getWidth( BPy_Text3d * self );
static PyObject *Text3d_setWidth( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getExtrudeDepth( BPy_Text3d * self );
static PyObject *Text3d_setExtrudeDepth( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getExtrudeBevelDepth( BPy_Text3d * self );
static PyObject *Text3d_setExtrudeBevelDepth( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getShear( BPy_Text3d * self );
static PyObject *Text3d_setShear( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getSize( BPy_Text3d * self );
static PyObject *Text3d_setSize( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getLineSeparation( BPy_Text3d * self );
static PyObject *Text3d_setLineSeparation( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getSpacing( BPy_Text3d * self );
static PyObject *Text3d_setSpacing( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getXoffset( BPy_Text3d * self );
static PyObject *Text3d_setXoffset( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getYoffset( BPy_Text3d * self );
static PyObject *Text3d_setYoffset( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getAlignment( BPy_Text3d * self );
static PyObject *Text3d_setAlignment( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getFont( BPy_Text3d * self );
static PyObject *Text3d_setFont( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_addFrame( BPy_Text3d * self );
static PyObject *Text3d_removeFrame( BPy_Text3d * self, PyObject * args );

/*****************************************************************************/
/* Python BPy_Text3d methods table:                                            */
/*****************************************************************************/
char M_Text3D_doc[] = "The Blender Text3D module\n\n\
	This module provides control over Text Curve objects in Blender.\n";

static PyMethodDef BPy_Text3d_methods[] = {
	{"getName", ( PyCFunction ) Text3d_getName,
	 METH_NOARGS, "() - Return Text3d Data name"},
	{"setName", ( PyCFunction ) Text3d_setName,
	 METH_VARARGS, "() - Sets Text3d Data name"},
	{"setText", ( PyCFunction ) Text3d_setText,
	 METH_O, "() - Sets Text3d Data"},
	{"getText", ( PyCFunction ) Text3d_getText,
	 METH_NOARGS, "() - Gets Text3d Data"},		 
	{"getDrawMode", ( PyCFunction ) Text3d_getDrawMode,
	METH_NOARGS, "() - Return the font drawing mode"},
	{"setDrawMode", ( PyCFunction ) Text3d_setDrawMode,
	METH_VARARGS, "(int) - Set the font drawing mode"},
 	{"getUVorco", ( PyCFunction ) Text3d_getUVorco,
	METH_NOARGS, "() - Return wether UV coords are used for Texture mapping"},
	{"setUVorco", ( PyCFunction ) Text3d_setUVorco,
	METH_VARARGS, "() - Set the font to use UV coords for Texture mapping"},
	{"getBevelAmount", ( PyCFunction ) Text3d_getBevelAmount,
	METH_NOARGS, "() - Return bevel resolution"},
	{"setBevelAmount", ( PyCFunction ) Text3d_setBevelAmount,
	METH_VARARGS, "() - Sets bevel resolution"},
	{"getDefaultResolution", ( PyCFunction ) Text3d_getDefaultResolution,
	METH_NOARGS, "() - Return Default text resolution"},
	{"setDefaultResolution", ( PyCFunction ) Text3d_setDefaultResolution,
	METH_VARARGS, "() - Sets Default text Resolution"},
	{"getWidth", ( PyCFunction ) Text3d_getWidth,
	METH_NOARGS, "() - Return curve width"},
	{"setWidth", ( PyCFunction ) Text3d_setWidth,
	METH_VARARGS, "(int) - Sets curve width"},
	{"getExtrudeDepth", ( PyCFunction ) Text3d_getExtrudeDepth,
	METH_NOARGS, "() - Gets Text3d ExtrudeDepth"},
	{"setExtrudeDepth", ( PyCFunction ) Text3d_setExtrudeDepth,
	METH_VARARGS, "() - Sets Text3d ExtrudeDepth"},
	{"getExtrudeBevelDepth", ( PyCFunction ) Text3d_getExtrudeBevelDepth,
	METH_NOARGS, "() - Gets Text3d ExtrudeBevelDepth"},
	{"setExtrudeBevelDepth", ( PyCFunction ) Text3d_setExtrudeBevelDepth,
	METH_VARARGS, "() - Sets Text3d ExtrudeBevelDepth"},
	{"getShear", ( PyCFunction ) Text3d_getShear,
	METH_NOARGS, "() - Gets Text3d Shear Data"},
	{"setShear", ( PyCFunction ) Text3d_setShear,
	METH_VARARGS, "() - Sets Text3d Shear Data"},
 	{"getSize", ( PyCFunction ) Text3d_getSize,
	METH_NOARGS, "() - Gets Text3d Size Data"},
	{"setSize", ( PyCFunction ) Text3d_setSize,
	METH_VARARGS, "() - Sets Text3d Size Data"},
 	{"getLineSeparation", ( PyCFunction ) Text3d_getLineSeparation,
	METH_NOARGS, "() - Gets Text3d LineSeparation Data"},
	{"setLineSeparation", ( PyCFunction ) Text3d_setLineSeparation,
	METH_VARARGS, "() - Sets Text3d LineSeparation Data"},
 	{"getSpacing", ( PyCFunction ) Text3d_getSpacing,
	METH_NOARGS, "() - Gets Text3d letter spacing"},
	{"setSpacing", ( PyCFunction ) Text3d_setSpacing,
	METH_VARARGS, "() - Sets Text3d letter spacing"},
 	{"getXoffset", ( PyCFunction ) Text3d_getXoffset,
	METH_NOARGS, "() - Gets Text3d Xoffset Data"},
	{"setXoffset", ( PyCFunction ) Text3d_setXoffset,
	METH_VARARGS, "() - Sets Text3d Xoffset Data"},
 	{"getYoffset", ( PyCFunction ) Text3d_getYoffset,
	METH_NOARGS, "() - Gets Text3d Yoffset Data"},
	{"setYoffset", ( PyCFunction ) Text3d_setYoffset,
	METH_VARARGS, "() - Sets Text3d Yoffset Data"},
 	{"getAlignment", ( PyCFunction ) Text3d_getAlignment,
	METH_NOARGS, "() - Gets Text3d Alignment Data"},
	{"setAlignment", ( PyCFunction ) Text3d_setAlignment,
	METH_VARARGS, "() - Sets Text3d Alignment Data"},
 	{"getFont", ( PyCFunction ) Text3d_getFont,
	METH_NOARGS, "() - Gets font list for Text3d"},
 	{"setFont", ( PyCFunction ) Text3d_setFont,
 	METH_VARARGS, "() - Sets font for Text3d"},
 	{"addFrame", ( PyCFunction ) Text3d_addFrame,
 	METH_NOARGS, "() - adds a new text frame"},
 	{"removeFrame", ( PyCFunction ) Text3d_removeFrame,
 	METH_VARARGS, "(index) - remove this frame"},
	{NULL, NULL, 0, NULL}
};


static PyObject *Text3d_getTotalFrames( BPy_Text3d * self )
{
	return PyInt_FromLong( (long)(self->curve->totbox ) );
}

static PyObject *Text3d_getActiveFrame( BPy_Text3d * self )
{
	return PyInt_FromLong( (long)(self->curve->actbox-1) );
}

static int Text3d_setActiveFrame( BPy_Text3d * self, PyObject * value )
{
	struct Curve *curve= self->curve;	
	PyObject* frame_int = PyNumber_Int( value );
	int index;
	

	if( !frame_int )
		return EXPP_ReturnIntError( PyExc_TypeError,
				"expected integer argument" );
	
	index = ( int )PyInt_AS_LONG( frame_int );
	index ++;
	if (index < 1 || index > curve->totbox)
		return EXPP_ReturnIntError( PyExc_IndexError,
				"index out of range" );
	
	curve->actbox = index;
	
	return 0;
}


static PyObject *getFloatAttr( BPy_Text3d *self, void *type )
{
	float param;
	struct Curve *curve= self->curve;
	
	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_T3D_ATTR_FRAME_WIDTH: 
		param = curve->tb[curve->actbox-1].w;
		break;
	case EXPP_T3D_ATTR_FRAME_HEIGHT: 
		param = curve->tb[curve->actbox-1].h;
		break;
	case EXPP_T3D_ATTR_FRAME_X: 
		param = curve->tb[curve->actbox-1].x;
		break;
	case EXPP_T3D_ATTR_FRAME_Y: 
		param = curve->tb[curve->actbox-1].y;
		break;
	
	default:
		return EXPP_ReturnPyObjError( PyExc_RuntimeError, 
				"undefined type in getFloatAttr" );
	}
	return PyFloat_FromDouble( param );
}

static int setFloatAttrClamp( BPy_Text3d *self, PyObject *value, void *type )
{
	float *param;
	struct Curve *curve= self->curve;
	float min, max;

	switch( GET_INT_FROM_POINTER(type) ) {
	case EXPP_T3D_ATTR_FRAME_WIDTH:
		min = 0.0;
		max = 50.0;
		param = &(curve->tb[curve->actbox-1].w);
		break;
	case EXPP_T3D_ATTR_FRAME_HEIGHT:
		min = 0.0;
		max = 50.0;
		param = &(curve->tb[curve->actbox-1].h);
		break;
	case EXPP_T3D_ATTR_FRAME_X:
		min = 0.0;
		max = 50.0;
		param = &(curve->tb[curve->actbox-1].x);
		break;
	case EXPP_T3D_ATTR_FRAME_Y:
		min = 0.0;
		max = 50.0;
		param = &(curve->tb[curve->actbox-1].y);
		break;
	
	default:
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"undefined type in setFloatAttrClamp" );
	}

	return EXPP_setFloatClamped( value, param, min, max );
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_Text3d_getseters[] = {
	GENERIC_LIB_GETSETATTR, /* didnt have any attributes, at least lets have the standard ID attrs */
	{"activeFrame",
	 (getter)Text3d_getActiveFrame, (setter)Text3d_setActiveFrame,
	 "the index of the active text frame",
	 NULL},
	{"totalFrames",
	 (getter)Text3d_getTotalFrames, (setter)NULL,
	 "the total number of text frames",
	 NULL},

	{"frameWidth",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "the width of the active text frame",
	 (void *)EXPP_T3D_ATTR_FRAME_WIDTH},
	{"frameHeight",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "the height of the active text frame",
	 (void *)EXPP_T3D_ATTR_FRAME_HEIGHT},
	{"frameX",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "the X position of the active text frame",
	 (void *)EXPP_T3D_ATTR_FRAME_X},
	{"frameY",
	 (getter)getFloatAttr, (setter)setFloatAttrClamp,
	 "the Y position of the active text frame",
	 (void *)EXPP_T3D_ATTR_FRAME_Y},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Text3d_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject Text3d_Type = {
	PyObject_HEAD_INIT( NULL )
	0,		/* ob_size */
	"Text3d",		/* tp_name */
	sizeof( BPy_Text3d ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,			/* tp_dealloc */
	NULL,			/* tp_print */
	NULL,			/* tp_getattr */
	NULL,			/* tp_setattr */
	( cmpfunc ) Text3d_compare,			/* tp_compare */
	( reprfunc ) Text3d_repr,	/* tp_repr */
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
	BPy_Text3d_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Text3d_getseters,         /* struct PyGetSetDef *tp_getset; */
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


/* 
 *   Text3d_update( )
 *   method to update display list for a Curve.
 */
static PyObject *Text3d_update( BPy_Text3d * self )
{
	freedisplist( &self->curve->disp );
	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:              M_Text3d_New                                       */
/* Python equivalent:     Blender.Text3d.New                                 */
/*****************************************************************************/

PyObject *M_Text3d_New( PyObject * self, PyObject * args )
{
	char *name = NULL;
	BPy_Text3d *pytext3d;	/* for Curve Data object wrapper in Python */
	Text3d *bltext3d = 0;	/* for actual Curve Data we create in Blender */

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected string argument or no argument" ) );

	bltext3d = add_curve( "Text", OB_FONT );	/* first create the Curve Data in Blender */
	bltext3d->vfont= get_builtin_font();
	bltext3d->vfont->id.us++;
	bltext3d->str= MEM_mallocN(12, "str");
	strcpy(bltext3d->str, "Text");
	bltext3d->pos= 4;

	bltext3d->strinfo= MEM_callocN(12*sizeof(CharInfo), "strinfo");
	bltext3d->totbox= bltext3d->actbox= 1;
	bltext3d->tb= MEM_callocN(MAXTEXTBOX*sizeof(TextBox), "textbox");
	bltext3d->tb[0].w = bltext3d->tb[0].h = 0.0;
	
	if( bltext3d == NULL )	/* bail out if add_curve() failed */
		return ( EXPP_ReturnPyObjError
			 ( PyExc_RuntimeError,
			   "couldn't create Curve Data in Blender" ) );

	/* return user count to zero because add_curve() inc'd it */
	bltext3d->id.us = 0;
	/* create python wrapper obj */
	pytext3d = ( BPy_Text3d * ) PyObject_NEW( BPy_Text3d, &Text3d_Type );

	if( pytext3d == NULL )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_MemoryError,
			   "couldn't create Curve Data object" ) );

	pytext3d->curve = bltext3d;	/* link Python curve wrapper to Blender Curve */
	if( name )
		rename_id( &bltext3d->id, name );
	
	Text3d_update ( pytext3d );
	return ( PyObject * ) pytext3d;
}

PyObject *M_Text3d_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Text3d *curv_iter;
	BPy_Text3d *wanted_curv;

	if( !PyArg_ParseTuple( args, "|s", &name ) )	/* expects nothing or a string */
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string argument" ) );
	if( name ) {		/*a name has been given */
		/* Use the name to search for the curve requested */
		wanted_curv = NULL;
		curv_iter = G.main->curve.first;

		while( ( curv_iter ) && ( wanted_curv == NULL ) ) {

			if( strcmp( name, curv_iter->id.name + 2 ) == 0 ) {
				wanted_curv = ( BPy_Text3d * )
					PyObject_NEW( BPy_Text3d, &Text3d_Type );
				if( wanted_curv )
					wanted_curv->curve = curv_iter;
			}

			curv_iter = curv_iter->id.next;
		}

		if( wanted_curv == NULL ) {	/* Requested curve doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				       "Curve \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				 ( PyExc_NameError, error_msg ) );
		}

		return ( PyObject * ) wanted_curv;
	} /* end  of if(name) */
	else {
		/* no name has been given; return a list of all curves by name.  */
		PyObject *curvlist;

		curv_iter = G.main->curve.first;
		curvlist = PyList_New( 0 );

		if( curvlist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( curv_iter && curv_iter->vfont ) {
			BPy_Text3d *found_text3d =
				( BPy_Text3d * ) PyObject_NEW( BPy_Text3d,
							      &Text3d_Type );
			found_text3d->curve = curv_iter;
			PyList_Append( curvlist, ( PyObject * ) found_text3d );
			Py_DECREF(found_text3d);
			curv_iter = curv_iter->id.next;
		}
		return ( curvlist );
	}
}

static PyObject *generate_ModuleIntConstant(char *name, int value)
{
	PyObject *constant = PyConstant_New();

	PyConstant_Insert((BPy_constant*)constant, 
		"value", PyInt_FromLong(value));
	PyConstant_Insert((BPy_constant*)constant, 
		"name", PyString_FromString(name));

	Py_INCREF(constant);
	return constant;
}

PyObject *Text3d_Init( void )
{
	//module
	PyObject *submodule, *dict;

	//add module...
	if( PyType_Ready( &Text3d_Type ) < 0 )
		return NULL;
	
	submodule = Py_InitModule3( "Blender.Text3d", M_Text3d_methods, 
		M_Text3D_doc);

	//add constants to module...
	PyModule_AddObject( submodule, "LEFT", 
		generate_ModuleIntConstant("Text3d.LEFT", CU_LEFT));
	PyModule_AddObject( submodule, "MIDDLE", 
		generate_ModuleIntConstant("Text3d.MIDDLE", CU_MIDDLE));
	PyModule_AddObject( submodule, "RIGHT",
		generate_ModuleIntConstant("Text3d.RIGHT", CU_RIGHT));
	PyModule_AddObject( submodule, "FLUSH",
		generate_ModuleIntConstant("Text3d.FLUSH", CU_FLUSH));
	PyModule_AddObject( submodule, "JUSTIFY",
		generate_ModuleIntConstant("Text3d.JUSTIFY", CU_JUSTIFY));
	PyModule_AddObject( submodule, "DRAW3D",
		generate_ModuleIntConstant("Text3d.DRAW3D", CU_3D));
	PyModule_AddObject( submodule, "DRAWFRONT",
		generate_ModuleIntConstant("Text3d.DRAWFRONT", CU_FRONT));
	PyModule_AddObject( submodule, "DRAWBACK",
		generate_ModuleIntConstant("Text3d.DRAWBACK", CU_BACK));
	PyModule_AddObject( submodule, "UVORCO",
		generate_ModuleIntConstant("Text3d.UVORCO", CU_UV_ORCO));
	dict = PyModule_GetDict( submodule );
	PyDict_SetItemString( dict, "Font", Font_Init(  ) );
	return ( submodule );
}

/****************************************************************************
 * Function:    Text3d_repr                                                   
 * Description: Callback function for the BPy_Text3d type to It      
 *               build a meaninful string to represent Text3d objects.      
 *
 ***************************************************************************/

static PyObject *Text3d_repr( BPy_Text3d * self )
{
	/* skip over CU in idname.  CUTEXT */
	return PyString_FromFormat( "[Text3d \"%s\"]",
								self->curve->id.name + 2 ); 
}

/****************************************************************************
 * Function:    Text3d_compare                                                   
 * Description: Callback function for the BPy_Text3d type to Compare 2 types
 *
 ***************************************************************************/

/* mat_a==mat_b or mat_a!=mat_b*/
static int Text3d_compare( BPy_Text3d * a, BPy_Text3d * b )
{
	return ( a->curve == b->curve) ? 0 : -1;
}

struct Text3d *Text3d_FromPyObject( PyObject * py_obj )
{
	BPy_Text3d *blen_obj;

	blen_obj = ( BPy_Text3d * ) py_obj;
	return ((struct Text3d*) blen_obj->curve );
}

static PyObject *return_ModuleConstant( char *constant_name){

	PyObject *module = NULL, *dict = NULL, *constant = NULL;;

	module = PyImport_AddModule("Blender.Text3d");
	if(!module){	//null = error returning module
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"error encountered with returning module constant..." ) );
	}
	dict = PyModule_GetDict(module); //never fails

	constant = PyDict_GetItemString(dict, constant_name);
	if(!constant){	//null = key not found
		return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"error encountered with returning module constant..." ) );
	}
	
	return EXPP_incr_ret( constant );
}

static PyObject *Text3d_getName( BPy_Text3d * self )
{
	return Curve_getName( (BPy_Curve*)self );
}

static PyObject *Text3d_setName( BPy_Text3d * self, PyObject * args )
{
	return Curve_setName( (BPy_Curve*)self,args );
}

static PyObject *Text3d_setText( BPy_Text3d * self, PyObject * value )
{
	char *text = PyString_AsString(value);

	if( !text )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"expected string argument" );

	/*
	 * If the text is currently being edited, then we have to put the
	 * text into the edit buffer.
	 */

	if( G.obedit && G.obedit->data == self->curve ) {
		short qual = G.qual;
		G.qual = 0;		/* save key qualifier, then clear it */
		self->curve->pos = self->curve->len = 0;
		while ( *text )
			do_textedit( 0, 0, *text++ );
		G.qual = qual;
	} else {
		short len = (short)strlen(text);
		MEM_freeN( self->curve->str );
		self->curve->str = MEM_callocN( len+sizeof(wchar_t), "str" );
		strcpy( self->curve->str, text );
		self->curve->pos = len;
		self->curve->len = len;

		if( self->curve->strinfo )
			MEM_freeN( self->curve->strinfo );
		/* don't know why this is +4, just duplicating load_editText() */
		self->curve->strinfo = MEM_callocN( (len+4) *sizeof(CharInfo),
				"strinfo");
	}
	Py_RETURN_NONE;
}

static PyObject *Text3d_getText( BPy_Text3d * self )
{
	if( self->curve->str )
		return PyString_FromString( self->curve->str );

	Py_RETURN_NONE;
}

static PyObject* Text3d_getDrawMode(BPy_Text3d* self)
{
	PyObject *tuple = NULL;
	int size = 0, pos = 0;

	//get the tuple size
	if(self->curve->flag & CU_3D)
		size++;
	if (self->curve->flag & CU_FRONT)
		size++;
	if (self->curve->flag & CU_BACK)
		size++;

	//generate tuple
	tuple = PyTuple_New(size);

	//load tuple
	if(self->curve->flag & CU_3D){
		PyTuple_SET_ITEM( tuple, pos, return_ModuleConstant("DRAW3D"));
		pos++;
	}
	if (self->curve->flag & CU_FRONT){
		PyTuple_SET_ITEM( tuple, pos, return_ModuleConstant("DRAWFRONT"));
		pos++;
	}
	if (self->curve->flag & CU_BACK){
		PyTuple_SET_ITEM( tuple, pos, return_ModuleConstant("DRAWBACK"));
		pos++;
	}

	return tuple;
}

static PyObject* Text3d_setDrawMode(BPy_Text3d* self,PyObject* args)
{
	PyObject *listObject = NULL;
	int size, i;
	short temp;

	size = PySequence_Length(args);
	if ( size == 1 ) {
		listObject = PySequence_GetItem(args, 0);
		if ( PySequence_Check(listObject) ) {
			size = PySequence_Length(listObject);
		}else{ //not a sequence but maybe a single constant
			Py_INCREF(args);
			listObject = args;
		}
	} else { //a list of objects (non-sequence)
		Py_INCREF(args);
		listObject = args;
	}
	if ( size > 3 || size < 1 ) {
		//bad number of arguments
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"too many parameters - expects 1 - 3 constants" ) );
	}
	//clear bits
	temp = self->curve->flag; //in case of failure
	if(self->curve->flag & CU_3D)
		self->curve->flag &= ~CU_3D;
	if(self->curve->flag & CU_FRONT)
		self->curve->flag &= ~CU_FRONT;
	if(self->curve->flag & CU_BACK)
		self->curve->flag &= ~CU_BACK;

	//parse and set bits
	for (i = 0; i < size; i++) {
		PyObject *v;
		int value;

		v = PySequence_GetItem(listObject, i);
		if (v == NULL) { //unable to return item - null = failure
			Py_DECREF(listObject);
			self->curve->flag = temp;
			return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
				"unable to parse list" ) );	
		}
		if( !BPy_Constant_Check(v)){
			Py_DECREF(listObject);
			Py_DECREF(v);
			self->curve->flag = temp;
			return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
				"bad argument types - expects module constants" ) );
		}
		value = PyInt_AS_LONG(PyDict_GetItemString(
			((BPy_constant*)v)->dict, "value"));
		self->curve->flag |= (short)value;
		Py_DECREF(v);
	}
	Py_DECREF(listObject);
	Py_RETURN_NONE;
}

static PyObject* Text3d_getUVorco(BPy_Text3d* self)
{
	if(self->curve->flag & CU_UV_ORCO)
		return EXPP_incr_ret_True();
	else
		return EXPP_incr_ret_False();
}

static PyObject* Text3d_setUVorco(BPy_Text3d* self,PyObject* args)
{
	int flag;

	if( !PyArg_ParseTuple( args, "i", &flag ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
			   "expected TRUE or FALSE (1 or 0)" );

	if( flag < 0 || flag > 1 )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
			   "expected TRUE or FALSE (1 or 0)" );

	if( flag )
		self->curve->flag |= CU_UV_ORCO;
	else
		self->curve->flag &= ~CU_UV_ORCO;

	Py_RETURN_NONE;
}

static PyObject* Text3d_getBevelAmount(BPy_Text3d* self)
{
	return Curve_getBevresol((BPy_Curve*)self);
}

static PyObject* Text3d_setBevelAmount(BPy_Text3d* self,PyObject* args)
{
	return Curve_setBevresol((BPy_Curve*)self,args);
}

static PyObject *Text3d_getDefaultResolution( BPy_Text3d * self )
{
	return Curve_getResolu( (BPy_Curve*)self );
}

static PyObject *Text3d_setDefaultResolution( BPy_Text3d * self, PyObject * args )
{
	return Curve_setResolu( (BPy_Curve*)self,args );
}

static PyObject *Text3d_getWidth( BPy_Text3d * self )
{
	return Curve_getWidth( (BPy_Curve*)self );
}

static PyObject *Text3d_setWidth( BPy_Text3d * self, PyObject * args )
{
	return Curve_setWidth( (BPy_Curve*)self,args );
}

static PyObject *Text3d_getExtrudeDepth( BPy_Text3d * self )
{
	return Curve_getExt1( (BPy_Curve*)self );
}

static PyObject *Text3d_setExtrudeDepth( BPy_Text3d * self, PyObject * args )
{
	return Curve_setExt1( (BPy_Curve*)self,args );
}

static PyObject *Text3d_getExtrudeBevelDepth( BPy_Text3d * self )
{
	return Curve_getExt2( (BPy_Curve*)self );
}

static PyObject *Text3d_setExtrudeBevelDepth( BPy_Text3d * self, PyObject * args )
{
	return Curve_setExt2( (BPy_Curve*)self,args );
}

static PyObject *Text3d_getShear( BPy_Text3d * self )
{
	PyObject *attr = PyFloat_FromDouble( (double) self->curve->shear );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
		"couldn't get Curve.shear attribute" ) );
}

static PyObject *Text3d_setShear( BPy_Text3d * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected float argument" ) );

	if(value > 1.0f || value < -1.0f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 1.0 and -1.0" ) );
	self->curve->shear = value;

	Py_RETURN_NONE;
}

static PyObject *Text3d_getSize( BPy_Text3d * self )
{
	PyObject *attr = PyFloat_FromDouble( (double) self->curve->fsize );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
		"couldn't get Curve.fsize attribute" ) );
}

static PyObject *Text3d_setSize( BPy_Text3d * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected float argument" ) );

	if(value > 10.0f || value < 0.1f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 10.0 and 0.1" ) );
	self->curve->fsize = value;

	Py_RETURN_NONE;
}

static PyObject *Text3d_getLineSeparation( BPy_Text3d * self )
{
	PyObject *attr = PyFloat_FromDouble( (double) self->curve->linedist );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
		"couldn't get Curve.linedist attribute" ) );
}

static PyObject *Text3d_setLineSeparation( BPy_Text3d * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected float argument" ) );

	if(value > 10.0f || value < 0.0f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 10.0 and 0.0" ) );
	self->curve->linedist = value;

	Py_RETURN_NONE;
}

static PyObject *Text3d_getSpacing( BPy_Text3d * self )
{
	PyObject *attr = PyFloat_FromDouble( (double) self->curve->spacing );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
		"couldn't get Curve.spacing attribute" ) );
}

static PyObject *Text3d_setSpacing( BPy_Text3d * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected float argument" ) );

	if(value > 10.0f || value < 0.0f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 10.0 and 0.0" ) );
	self->curve->spacing = value;

	Py_RETURN_NONE;
}
static PyObject *Text3d_getXoffset( BPy_Text3d * self )
{
	PyObject *attr = PyFloat_FromDouble( (double) self->curve->xof );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
		"couldn't get Curve.xof attribute" ) );
}

static PyObject *Text3d_setXoffset( BPy_Text3d * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
		"expected float argument" ) );

	if(value > 50.0f || value < -50.0f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 50.0 and -50.0" ) );
	self->curve->xof = value;

	Py_RETURN_NONE;
}

static PyObject *Text3d_getYoffset( BPy_Text3d * self )
{
	PyObject *attr = PyFloat_FromDouble( (double) self->curve->yof );

	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
		"couldn't get Curve.yof attribute" ) );
}

static PyObject *Text3d_setYoffset( BPy_Text3d * self, PyObject * args )
{
	float value;

	if( !PyArg_ParseTuple( args, "f", &value ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
		"expected float argument" ) );

	if(value > 50.0f || value < -50.0f)
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"acceptable values are between 50.0 and -50.0" ) );
	self->curve->yof = value;

	Py_RETURN_NONE;
}

static PyObject *Text3d_getAlignment( BPy_Text3d * self )
{
	if(self->curve->spacemode == CU_LEFT){
		return return_ModuleConstant("LEFT");
	}else if (self->curve->spacemode == CU_MIDDLE){
		return return_ModuleConstant("MIDDLE");
	}else if (self->curve->spacemode == CU_RIGHT){
		return return_ModuleConstant("RIGHT");
	}else if (self->curve->spacemode == CU_FLUSH){
		return return_ModuleConstant("FLUSH");
	}else if (self->curve->spacemode == CU_JUSTIFY){ 
		return return_ModuleConstant("JUSTIFY");
	}

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
		"couldn't get Curve.spacemode attribute" ) );
}

static PyObject *Text3d_setAlignment( BPy_Text3d * self, PyObject * args )
{
	BPy_constant *constant;
	int value;

	if( !PyArg_ParseTuple( args, "O!", &constant_Type, &constant ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
			"expected module constant" ) );

	value = PyInt_AS_LONG(PyDict_GetItemString(constant->dict, "value"));
	self->curve->spacemode = (short)value;

	Py_RETURN_NONE;
}


/*****************************************************************************
 * Function:    Text3d_CreatePyObject                                       
 * Description: This function will create a new BPy_Text3d from an existing   
 *               Blender structure.                                     
 *****************************************************************************/

PyObject *Text3d_CreatePyObject( Text3d * text3d )
{
	BPy_Text3d *pytext3d;

	pytext3d = ( BPy_Text3d * ) PyObject_NEW( BPy_Text3d, &Text3d_Type );

	if( !pytext3d )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Text3d object" );

	pytext3d->curve = text3d;

	return ( PyObject * ) pytext3d;
}

static PyObject *Text3d_getFont( BPy_Text3d * self )
{
	if (self->curve) 
		return Font_CreatePyObject (self->curve->vfont);
	else
		Py_RETURN_NONE;
}

static PyObject *Text3d_setFont( BPy_Text3d * self, PyObject * args )
{
	BPy_Font  *pyobj= NULL;
	VFont *vf; //, *vfont;
	if( !PyArg_ParseTuple( args, "|O!",&Font_Type, &pyobj) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string" );
	if( !pyobj ) {
	//	pyobj= M_Text3d_LoadFont (self, Py_BuildValue("(s)", "<builtin>"));
		self->curve->vfont= get_builtin_font ();
		Py_RETURN_NONE;
	}
	vf= exist_vfont(pyobj->font->name);
	if (vf) {
		id_us_plus((ID *)vf);
		self->curve->vfont->id.us--;
		self->curve->vfont= vf;
	}
	else {
		vf= load_vfont (pyobj->font->name);
		if (vf) {
			id_us_plus((ID *)vf);
			self->curve->vfont->id.us--;
			self->curve->vfont= vf;
		}	
	}
	Py_RETURN_NONE;
}

static PyObject *Text3d_addFrame( BPy_Text3d * self )
{
	Curve *cu = self->curve;
	
	if (cu->totbox >= 256)	
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"limited to 256 frames" );
	
	cu->totbox++;	
	cu->tb[cu->totbox-1]= cu->tb[cu->totbox-2];
	Py_RETURN_NONE;
}

static PyObject *Text3d_removeFrame( BPy_Text3d * self, PyObject * args )
{
	Curve *cu = self->curve;
	int index, i;
	
	if (cu->totbox == 1)
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"cannot remove the last frame" );
	
	index = cu->totbox-1;
	
	if( !PyArg_ParseTuple( args, "|i", &index ) )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
			"expected an int" );
	
	if (index < 0 || index >= cu->totbox )
		return EXPP_ReturnPyObjError( PyExc_IndexError,
			"index out of range" );
	
	for (i = index; i < cu->totbox; i++) cu->tb[i]= cu->tb[i+1];
	cu->totbox--;
	cu->actbox--;
	Py_RETURN_NONE;
}


PyObject *M_Text3d_LoadFont( PyObject * self, PyObject * value )
{
	char *fontfile= PyString_AsString(value);
	FILE *file= NULL;
	VFont *vf= NULL;

	if( !fontfile )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string" );
	vf= exist_vfont(fontfile);
	if( vf )
		return Font_CreatePyObject( vf );
	/* No use for that -- lukep
	 else
		vf= NULL;
	 */
	file= fopen( fontfile, "r");

	if( file || !strcmp (fontfile, "<builtin>") ) {
		load_vfont( fontfile );
		if(file) fclose( file );
		vf = exist_vfont( fontfile );
		if(vf)
			return Font_CreatePyObject( vf );
		Py_RETURN_NONE;
	}

	return EXPP_ReturnPyObjError( PyExc_TypeError,
				      "string isn't filename or fontpath" );
}

