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
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_vfont_types.h"

#include "BKE_object.h"
#include "BDR_editobject.h"
#include "BKE_displist.h"
#include "MEM_guardedalloc.h"

#include "blendef.h"
#include "Text3d.h"

#include "mydevice.h"

/* 
fixme hackage warning:  
this decl is copied from source/blender/src/editfont.c
it belongs in a .h file!
*/
VFont *get_builtin_font(void);

extern PyObject *Curve_getName( BPy_Text3d * self );
extern PyObject *Curve_setName( BPy_Text3d * self, PyObject * args );

/*****************************************************************************/
/* Python API function prototypes for the Effect module.                     */
/*****************************************************************************/
static PyObject *M_Text3d_New( PyObject * self, PyObject * args );
static PyObject *M_Text3d_Get( PyObject * self, PyObject * args );

/*****************************************************************************/
/* Python BPy_Text3d methods declarations:                                   */
/*****************************************************************************/
/*PyObject *Text3d_getType(BPy_Text3d *self);*/

/*****************************************************************************/
/* Python Text3d_Type callback function prototypes:                          */
/*****************************************************************************/


void Text3dDeAlloc( BPy_Text3d * msh );
/* int Text3dPrint (BPy_Text3d *msh, FILE *fp, int flags); */
int Text3dSetAttr( BPy_Text3d * msh, char *name, PyObject * v );
PyObject *Text3dGetAttr( BPy_Text3d * msh, char *name );
PyObject *Text3dRepr( BPy_Text3d * msh );
PyObject *Text3dCreatePyObject( Text3d *text3d );
int Text3dCheckPyObject( PyObject * py_obj );
struct Text3d *Text3dFromPyObject( PyObject * py_obj );

static PyObject *Text3d_getName( BPy_Text3d * self );
static PyObject *Text3d_setName( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_setText( BPy_Text3d * self, PyObject * args );
static PyObject *Text3d_getText( BPy_Text3d * self );

/*****************************************************************************/
/* Python BPy_Text3d methods table:                                            */
/*****************************************************************************/
static char text2text3_doc[] = "(str) - set Text3d string";

static PyMethodDef BPy_Text3d_methods[] = {
	{"getName", ( PyCFunction ) Text3d_getName,
	 METH_NOARGS, "() - Return Text3d Data name"},
	{"setName", ( PyCFunction ) Text3d_setName,
	 METH_VARARGS, "() - Sets Text3d Data name"},
	{"setText", ( PyCFunction ) Text3d_setText,
	 METH_VARARGS, "() - Sets Text3d Data"},
	{"getText", ( PyCFunction ) Text3d_getText,
	 METH_NOARGS, "() - Gets Text3d Data"},	
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

/*****************************************************************************/
/* Python method structure definition for Blender.Text3d module:             */
/*****************************************************************************/
struct PyMethodDef M_Text3d_methods[] = {
	{"New", ( PyCFunction ) M_Text3d_New, METH_VARARGS, NULL},
	{"Get", M_Text3d_Get, METH_VARARGS, NULL},
	{"get", M_Text3d_Get, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Text3d methods declarations:				     */
/*****************************************************************************/
static PyObject *Text2Text3d( BPy_Text3d * self, PyObject * args );


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

PyObject *Text3d_Init( void )
{
	PyObject *submodule;

	Text3d_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3( "Blender.Text3d", M_Text3d_methods, 0 );
	return ( submodule );
}

void Text3dDeAlloc( BPy_Text3d * self )
{
	PyObject_DEL( self );
}

/*****************************************************************************/
/* Function:    Text3dGetAttr                                                */
/* Description: This is a callback function for the BPy_Text3d type. It is   */
/*              the function that accesses BPy_Text3d "member variables" and */
/*              methods.                                                     */
/*****************************************************************************/


PyObject *Text3dGetAttr( BPy_Text3d * self, char *name )
{
	return Py_FindMethod( BPy_Text3d_methods, ( PyObject * ) self, name );
}

/*****************************************************************************/
/* Function:    EffectSetAttr                                                */
/* Description: This is a callback function for the BPy_Effect type. It   */
/*              sets Effect Data attributes (member variables). */
/*****************************************************************************/

int Text3dSetAttr( BPy_Text3d * self, char *name, PyObject * value )
{
	return 0;		/* normal exit */
}

/*****************************************************************************/
/* Function:    Text3dRepr                                                   */
/* Description: This is a callback function for the BPy_Effect type. It      */
/*              builds a meaninful string to represent effcte objects.       */
/*****************************************************************************/

PyObject *Text3dRepr( BPy_Text3d * self )
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
	return ((Text3d*) blen_obj->curve );
}

static PyObject *Text3d_getName( BPy_Text3d * self )
{
	return Curve_getName( self );
}

static PyObject *Text3d_setName( BPy_Text3d * self, PyObject * args )
{
	return Curve_setName( self,args );
}

static PyObject *Text3d_setText( BPy_Text3d * self, PyObject * args )
{
	char *text;
	if( !PyArg_ParseTuple( args, "s", &text  ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string argument" ) );
	if (self) {
		MEM_freeN (self->curve->str);
		self->curve->str= MEM_mallocN (strlen (text)+1, "str");
		strcpy (self->curve->str, text);
		self->curve->pos= strlen (text);
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

