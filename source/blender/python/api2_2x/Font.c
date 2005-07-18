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
#include "BLI_blenlib.h"
#include "gen_utils.h"

extern PyObject *M_Text3d_LoadFont( PyObject * self, PyObject * args );

/*--------------------Python API function prototypes for the Font module----*/
PyObject *M_Font_New( PyObject * self, PyObject * args );
static PyObject *M_Font_Get( PyObject * self, PyObject * args );

/*------------------------Python API Doc strings for the Font module--------*/
char M_Font_doc[] = "The Blender Font module\n\n\
This module provides control over **Font Data** objects in Blender.\n\n\
Example::\n\n\
	from Blender import Text3d.Font\n\
	l = Text3d.Font.New()\n";
char M_Font_New_doc[] = "(name) - return a new Font of name 'name'.";
char M_Font_Get_doc[] = "(name) - return a new Font of name 'name'.";

/*----- Python method structure definition for Blender.Text3d.Font module---*/
struct PyMethodDef M_Font_methods[] = {
	{"New", ( PyCFunction ) M_Font_New, METH_VARARGS, M_Font_New_doc},
	{"Get", ( PyCFunction ) M_Font_Get, METH_VARARGS, M_Font_New_doc},
	{NULL, NULL, 0, NULL}
};

/*--------------- Python BPy_Bone methods declarations:-------------------*/
static PyObject *Font_getName( BPy_Font * self );
static PyObject *Font_setName( BPy_Font * self, PyObject * args );
static PyObject *Font_pack( BPy_Font * self, PyObject * args );
static PyObject *Font_isPacked( BPy_Font * self );

/*--------------- Python BPy_Font methods table:--------------------------*/
static PyMethodDef BPy_Font_methods[] = {
	{"GetName", ( PyCFunction ) Font_getName, METH_NOARGS,
	 "() - return Font name"},
	{"getName", ( PyCFunction ) Font_getName, METH_NOARGS,
	 "() - return Font name"},
	{"setName", ( PyCFunction ) Font_setName, METH_VARARGS,
	 "() - return Font name"},
	{"pack", ( PyCFunction ) Font_pack, METH_VARARGS,
	 "() - pack/unpack Font"},
	{"isPacked", ( PyCFunction ) Font_isPacked, METH_NOARGS,
	 "() - pack/unpack Font"},
	{NULL, NULL, 0, NULL}
};

/*--------------- Python TypeBone callback function prototypes----------*/
static void Font_dealloc( BPy_Font * font );
static PyObject *Font_getAttr( BPy_Font * bone, char *name );
static int Font_setAttr( BPy_Font * font, char *name, PyObject * v );
static int Font_compare( BPy_Font * a1, BPy_Font * a2 );
static PyObject *Font_repr( BPy_Font * font );

PyTypeObject Font_Type = {
	PyObject_HEAD_INIT( NULL )
		0,		/* ob_size */
	"Font",		/* tp_name */
	sizeof( BPy_Font ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) Font_dealloc,	/* tp_dealloc */
	0,			/* tp_print */
	( getattrfunc ) Font_getAttr,	/* tp_getattr */
	( setattrfunc ) Font_setAttr,	/* tp_setattr */
	( cmpfunc ) Font_compare,			/* tp_compare */
	( reprfunc ) Font_repr,	/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_as_hash */
	0, 0, 0, 0, 0, 0,
	0,			/* tp_doc */
	0, 0, 0, 0, 0, 0,
	BPy_Font_methods,	/* tp_methods */
	0,			/* tp_members */
};

/*--------------- Font Module Init-----------------------------*/
PyObject *Font_Init( void )
{
	PyObject *submodule;

	Font_Type.ob_type = &PyType_Type;

	submodule = Py_InitModule3( "Blender.Text3d.Font",
				    M_Font_methods, M_Font_doc );

	return ( submodule );
}

/*--------------- Bone module internal callbacks-----------------*/
/*---------------BPy_Bone internal callbacks/methods-------------*/

//--------------- dealloc------------------------------------------
static void Font_dealloc( BPy_Font * self )
{
	PyObject_DEL( self );
}

/*---------------getattr-------------------------------------------*/
static PyObject *Font_getAttr( BPy_Font * self, char *name )
{
	return Py_FindMethod( BPy_Font_methods, ( PyObject * ) self, name );
}

/*--------------- setattr-------------------------------------------*/
static int Font_setAttr( BPy_Font * self, char *name, PyObject * value )
{
	return 0;
}

/*--------------- repr---------------------------------------------*/
static PyObject *Font_repr( BPy_Font * self )
{
	if( self->font )
		return PyString_FromFormat( "[Font \"%s\"]",
					    self->font->name );
	else
		return PyString_FromString( "NULL" );
}

/*--------------- compare------------------------------------------*/
static int Font_compare( BPy_Font * a, BPy_Font * b )
{
	VFont *pa = a->font, *pb = b->font;
	return ( pa == pb ) ? 0 : -1;
}

/*--------------- Font_CreatePyObject---------------------------------*/
PyObject *Font_CreatePyObject( struct VFont * font )
{
	BPy_Font *blen_font;

	blen_font = ( BPy_Font * ) PyObject_NEW( BPy_Font, &Font_Type );

	/*set the all important Bone flag*/
	blen_font->font = font;

	return ( ( PyObject * ) blen_font );
}

/*--------------- Font_CheckPyObject--------------------------------*/
int Font_CheckPyObject( PyObject * py_obj )
{
	return ( py_obj->ob_type == &Font_Type );
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
		//use bone datastruct
		return ( blen_obj->font );
	}
}

/*--------------- Python Font Module methods------------------------*/

/*--------------- Blender.Text3d.Font.New()-----------------------*/
PyObject *M_Font_New( PyObject * self, PyObject * args )
{
	char *name_str = "<builtin>";
//	char *parent_str = "";
	BPy_Font *py_font = NULL;	/* for Font Data object wrapper in Python */
	PyObject *tmp; 

	if( !PyArg_ParseTuple( args, "|s", &name_str ) )
		return ( EXPP_ReturnPyObjError( PyExc_AttributeError,
						"expected string or empty argument" ) );

	/*create python font*/
	if( !S_ISDIR(BLI_exist(name_str)) )  {
		tmp= Py_BuildValue("(s)", name_str);
		py_font= (BPy_Font *) M_Text3d_LoadFont (self, Py_BuildValue("(s)", name_str));
		Py_DECREF (tmp);
	}
	else
		return EXPP_incr_ret( Py_None );
	return ( PyObject * ) py_font;
}

/*--------------- Blender.Text3d.Font.Get()-----------------------*/
static PyObject *M_Font_Get( PyObject * self, PyObject * args )
{
	return M_Font_New (self, args);
}

/*--------------- Python BPy_Font methods---------------------------*/

/*--------------- BPy_Font.getName()--------------------------------*/
static PyObject *Font_getName( BPy_Font * self )
{
	PyObject *attr = NULL;

	if( self->font )
		attr = PyString_FromString( self->font->name );
	if( attr )
		return attr;

	return ( EXPP_ReturnPyObjError( PyExc_RuntimeError,
					"couldn't get Bone.name attribute" ) );
}

/*--------------- BPy_Font.setName()---------------------------------*/
static PyObject *Font_setName( BPy_Font * self, PyObject * args )
{
	char *name;
	char buf[256];

	if( !PyArg_ParseTuple( args, "s", &name ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected string argument" ) );

	/* guarantee a null terminated string of reasonable size */
	PyOS_snprintf( buf, sizeof( buf ), "%s", name );

	if( self->font )
		BLI_strncpy( self->font->name, buf, sizeof( buf )  );
	return EXPP_incr_ret( Py_None );
}

/*--------------- BPy_Font.pack()---------------------------------*/
static PyObject *Font_pack( BPy_Font * self, PyObject * args ) 
{
	int pack= 0;
	if( !PyArg_ParseTuple( args, "i", &pack ) )
		return ( EXPP_ReturnPyObjError
			 ( PyExc_AttributeError,
			   "expected string argument" ) );
	if( pack && !self->font->packedfile ) 
		self->font->packedfile = newPackedFile(self->font->name);
	else if (self->font->packedfile)
		if (unpackVFont(self->font, PF_ASK) == RET_OK)
			G.fileflags &= ~G_AUTOPACK;
	

	return EXPP_incr_ret( Py_None );
}

/*--------------- BPy_Font.ispack()---------------------------------*/
static PyObject *Font_isPacked( BPy_Font * self ) 
{
	if (G.fileflags & G_AUTOPACK)
		return EXPP_incr_ret_True();
	else
		return EXPP_incr_ret_False();
}

