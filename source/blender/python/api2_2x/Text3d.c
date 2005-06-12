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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Joilnen Leite
 *                 Johnny Matthews
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_vfont_types.h"
#include "MEM_guardedalloc.h"
#include "BKE_object.h"
#include "BDR_editobject.h"
#include "BKE_displist.h"
#include "MEM_guardedalloc.h"
#include "mydevice.h"
#include "blendef.h"
#include "Text3d.h"
#include "Curve.h"
#include "constant.h"
#include "Types.h"
#include "Font.h"

#include "mydevice.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

//no prototypes declared in header files - external linkage outside of python
extern VFont *get_builtin_font(void);  
extern void freedisplist(struct ListBase *lb);
extern VFont *get_builtin_font(void);
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
	{"LoadFont", ( PyCFunction ) M_Text3d_LoadFont, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Text3d_Type callback function prototypes:                          */
/*****************************************************************************/
/* int Text3dPrint (BPy_Text3d *msh, FILE *fp, int flags); */
static void Text3dDeAlloc( BPy_Text3d * self );
static int Text3dSetAttr( BPy_Text3d * self, char *name, PyObject * value );
static PyObject *Text3dGetAttr( BPy_Text3d * self, char *name );
static PyObject *Text3dRepr( BPy_Text3d * self );

/*****************************************************************************/
/* Python BPy_Text3d methods declarations:                                   */
/*****************************************************************************/
/*PyObject *Text3d_getType(BPy_Text3d *self);*/
static PyObject *Text3d_getName( BPy_Text3d * self );
static PyObject *Text3d_setName( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_setText( BPy_Text3d * self, PyObject * args );
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
	 METH_VARARGS, "() - Sets Text3d Data"},
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
	{NULL, NULL, 0, NULL}
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
	( destructor ) Text3dDeAlloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) Text3dGetAttr,	/* tp_getattr */
	( setattrfunc ) Text3dSetAttr,	/* tp_setattr */
	0,			/* tp_compare */
	( reprfunc ) Text3dRepr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_Text3d_methods,	/* tp_methods */
	0,			/* tp_members */
};

/* 
 *   Text3d_update( )
 *   method to update display list for a Curve.
 */
static PyObject *Text3d_update( BPy_Text3d * self )
{
	freedisplist( &self->curve->disp );

	Py_INCREF( Py_None );
	return Py_None;
}

/*****************************************************************************/
/* Function:              M_Text3d_New                                       */
/* Python equivalent:     Blender.Text3d.New                                 */
/*****************************************************************************/

PyObject *M_Text3d_New( PyObject * self, PyObject * args )
{
	char buf[24];
	char *name = NULL;
	BPy_Text3d *pytext3d;	/* for Curve Data object wrapper in Python */
	Text3d *bltext3d = 0;	/* for actual Curve Data we create in Blender */

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected string argument or no argument" ) );

	bltext3d = add_curve( OB_FONT );	/* first create the Curve Data in Blender */
	bltext3d->vfont= get_builtin_font();
	bltext3d->vfont->id.us++;
	bltext3d->str= MEM_mallocN(12, "str");
	strcpy(bltext3d->str, "Text");
	bltext3d->pos= 4;
	
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
	if( name ) {
		PyOS_snprintf( buf, sizeof( buf ), "%s", name );
		rename_id( &bltext3d->id, buf );
	}
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

			curv_iter = curv_iter->id.next;
		}
		return ( curvlist );
	}
}

static PyObject *generate_ModuleIntConstant(char *name, int value)
{
	PyObject *constant = M_constant_New();

	constant_insert((BPy_constant*)constant, 
		"value", PyInt_FromLong(value));
	constant_insert((BPy_constant*)constant, 
		"name", PyString_FromString(name));

	Py_INCREF(constant);
	return constant;
}

PyObject *Text3d_Init( void )
{
	//module
	PyObject *submodule, *dict;

	//add module...
	Text3d_Type.ob_type = &PyType_Type;
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

static void Text3dDeAlloc( BPy_Text3d * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:    Text3dGetAttr                                                */
/* Description: This is a callback function for the BPy_Text3d type. It is   */
/*              the function that accesses BPy_Text3d "member variables" and */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *Text3dGetAttr( BPy_Text3d * self, char *name )
{
	return Py_FindMethod( BPy_Text3d_methods, ( PyObject * ) self, name );
}

/*****************************************************************************/
/* Function:    EffectSetAttr                                                */
/* Description: This is a callback function for the BPy_Effect type. It   */
/*              sets Effect Data attributes (member variables). */
/*****************************************************************************/
static int Text3dSetAttr( BPy_Text3d * self, char *name, PyObject * value )
{
	return 0;		/* normal exit */
}

/*****************************************************************************/
/* Function:    Text3dRepr                                                   */
/* Description: This is a callback function for the BPy_Effect type. It      */
/*              builds a meaninful string to represent effcte objects.       */
/*****************************************************************************/

static PyObject *Text3dRepr( BPy_Text3d * self )
{
	char *str = "";
	return PyString_FromString( str );
}



int Text3d_CheckPyObject( PyObject * py_obj )
{
	return ( py_obj->ob_type == &Text3d_Type );
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

static PyObject *Text3d_setText( BPy_Text3d * self, PyObject * args )
{
	char *text;
	if( !PyArg_ParseTuple( args, "s", &text  ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string argument" ) );
	if( self ) {
		MEM_freeN( self->curve->str );
		self->curve->str = MEM_mallocN( strlen (text)+1, "str" );
		strcpy( self->curve->str, text );
		self->curve->pos = strlen ( text );
		self->curve->len = strlen ( text );
	}
	Py_INCREF( Py_None );
	return Py_None;
}

static PyObject *Text3d_getText( BPy_Text3d * self )
{
	if ( strlen(self->curve->str) )
		return PyString_FromString (self->curve->str);
	else 
		return Py_None;
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
	return EXPP_incr_ret( Py_None );
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

	return EXPP_incr_ret( Py_None );
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

	return EXPP_incr_ret( Py_None );
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

	return EXPP_incr_ret( Py_None );
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

	return EXPP_incr_ret( Py_None );
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

	return EXPP_incr_ret( Py_None );
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

	return EXPP_incr_ret( Py_None );
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

	return EXPP_incr_ret( Py_None );
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

	return EXPP_incr_ret( Py_None );
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
		return EXPP_incr_ret( Py_None );
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
		return EXPP_incr_ret( Py_None );
	}
	vf= exist_vfont(pyobj->font->name);
	if (vf) {
		id_us_plus((ID *)vf);
		self->curve->vfont->id.us--;
		self->curve->vfont= vf;
	}
	else {
		load_vfont (pyobj->font->name);
		vf= exist_vfont(pyobj->font->name);
		if (vf) {
			id_us_plus((ID *)vf);
			self->curve->vfont->id.us--;
			self->curve->vfont= vf;
		}	
	}
	return EXPP_incr_ret( Py_None );
}

PyObject *M_Text3d_LoadFont( PyObject * self, PyObject * args )
{
	char *fontfile= NULL;
	FILE *file= NULL;
	VFont *vf= NULL;

	if( !PyArg_ParseTuple( args, "s", &fontfile ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string" );
	vf= exist_vfont(fontfile);
	if( vf )
		return Font_CreatePyObject( vf );
	/*	return EXPP_incr_ret( Py_None ); */
	/* No use for that -- lukep
	 else
		vf= NULL;
	 */
	file= fopen( fontfile, "r");

	if( file || !strcmp (fontfile, "<builtin>") ) {
		load_vfont( fontfile );
		if(fclose) fclose( file );
		if( (vf=exist_vfont( fontfile )) )
			return Font_CreatePyObject( vf );
		return EXPP_incr_ret( Py_None );
	}

	return EXPP_ReturnPyObjError( PyExc_TypeError,
				      "string isn't filename or fontpath" );
}

