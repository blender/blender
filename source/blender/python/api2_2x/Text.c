/* 
 * $Id: Text.c 11123 2007-06-29 08:59:26Z campbellbarton $
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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Text.h" /*This must come first*/

#include "BKE_library.h"
#include "BKE_sca.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BIF_drawtext.h"
#include "BKE_text.h"
#include "BLI_blenlib.h"
#include "DNA_space_types.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "../BPY_extern.h"

#define EXPP_TEXT_MODE_FOLLOW TXT_FOLLOW

/*****************************************************************************/
/* Python API function prototypes for the Text module.                       */
/*****************************************************************************/
static PyObject *M_Text_New( PyObject * self, PyObject * args);
static PyObject *M_Text_Get( PyObject * self, PyObject * args );
static PyObject *M_Text_Load( PyObject * self, PyObject * value );
static PyObject *M_Text_unlink( PyObject * self, PyObject * args );

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Text.__doc__                                                      */
/*****************************************************************************/
static char M_Text_doc[] = "The Blender Text module\n\n";

static char M_Text_New_doc[] = "() - return a new Text object";

static char M_Text_Get_doc[] = "(name) - return the Text with name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all Texts in the\ncurrent scene.";

static char M_Text_Load_doc[] =
	"(filename) - return text from file filename as a Text Object, \
returns None if not found.\n";

static char M_Text_unlink_doc[] =
	"(text) - remove Text object 'text' from Blender";

/*****************************************************************************/
/* Python method structure definition for Blender.Text module:               */
/*****************************************************************************/
struct PyMethodDef M_Text_methods[] = {
	{"New", M_Text_New, METH_VARARGS, M_Text_New_doc},
	{"Get", M_Text_Get, METH_VARARGS, M_Text_Get_doc},
	{"get", M_Text_Get, METH_VARARGS, M_Text_Get_doc},
	{"Load", M_Text_Load, METH_O, M_Text_Load_doc},
	{"unlink", M_Text_unlink, METH_VARARGS, M_Text_unlink_doc},
	{NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Python BPy_Text methods declarations:                                     */
/*****************************************************************************/
static PyObject *Text_getFilename( BPy_Text * self );
static PyObject *Text_getNLines( BPy_Text * self );
static PyObject *Text_clear( BPy_Text * self );
static PyObject *Text_write( BPy_Text * self, PyObject * value );
static PyObject *Text_set( BPy_Text * self, PyObject * args );
static PyObject *Text_asLines( BPy_Text * self );

/*****************************************************************************/
/* Python BPy_Text methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_Text_methods[] = {
	/* name, method, flags, doc */
	{"getName", ( PyCFunction ) GenericLib_getName, METH_NOARGS,
	 "() - Return Text Object name"},
	{"getFilename", ( PyCFunction ) Text_getFilename, METH_VARARGS,
	 "() - Return Text Object filename"},
	{"getNLines", ( PyCFunction ) Text_getNLines, METH_VARARGS,
	 "() - Return number of lines in text buffer"},
	{"setName", ( PyCFunction ) GenericLib_setName_with_method, METH_VARARGS,
	 "(str) - Change Text Object name"},
	{"clear", ( PyCFunction ) Text_clear, METH_NOARGS,
	 "() - Clear Text buffer"},
	{"write", ( PyCFunction ) Text_write, METH_O,
	 "(line) - Append string 'str' to Text buffer"},
	{"set", ( PyCFunction ) Text_set, METH_VARARGS,
	 "(name, val) - Set attribute 'name' to value 'val'"},
	{"asLines", ( PyCFunction ) Text_asLines, METH_NOARGS,
	 "() - Return text buffer as a list of lines"},
	{NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Text_Type callback function prototypes:                            */
/*****************************************************************************/
static int Text_compare( BPy_Text * a, BPy_Text * b );
static PyObject *Text_repr( BPy_Text * self );

/*****************************************************************************/
/* Function:              M_Text_New                                         */
/* Python equivalent:     Blender.Text.New                                   */
/*****************************************************************************/
static PyObject *M_Text_New( PyObject * self, PyObject * args)
{
	char *name = "Text";
	int follow = 0;
	Text *bl_text;		/* blender text object */
	PyObject *py_text;	/* python wrapper */

	if( !PyArg_ParseTuple( args, "|si", &name, &follow ) )
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
					      "expected string and int arguments (or nothing)" );

	bl_text = add_empty_text( name );

	if( bl_text ) {
		/* do not set user count because Text is already linked */

		/* create python wrapper obj */
		py_text = Text_CreatePyObject( bl_text );
	} else
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "couldn't create Text Object in Blender" );
	if( !py_text )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create Text Object wrapper" );

	if( follow )
		bl_text->flags |= EXPP_TEXT_MODE_FOLLOW;

	return py_text;
}

/*****************************************************************************/
/* Function:              M_Text_Get                                         */
/* Python equivalent:     Blender.Text.Get                                   */
/* Description:           Receives a string and returns the text object      */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all text names in the current */
/*                        scene is returned.                                 */
/*****************************************************************************/
static PyObject *M_Text_Get( PyObject * self, PyObject * args )
{
	char *name = NULL;
	Text *txt_iter;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument (or nothing)" ) );

	txt_iter = G.main->text.first;

	if( name ) {		/* (name) - Search text by name */

		PyObject *wanted_txt = NULL;

		while( ( txt_iter ) && ( wanted_txt == NULL ) ) {

			if( strcmp( name, txt_iter->id.name + 2 ) == 0 ) {
				wanted_txt = Text_CreatePyObject( txt_iter );
			}

			txt_iter = txt_iter->id.next;
		}

		if( wanted_txt == NULL ) {	/* Requested text doesn't exist */
			char error_msg[64];
			PyOS_snprintf( error_msg, sizeof( error_msg ),
				"Text \"%s\" not found", name );
			return ( EXPP_ReturnPyObjError
				( PyExc_NameError, error_msg ) );
		}

		return wanted_txt;
	}

	else {			/* () - return a list of all texts in the scene */
		int index = 0;
		PyObject *txtlist, *pyobj;

		txtlist = PyList_New( BLI_countlist( &( G.main->text ) ) );

		if( txtlist == NULL )
			return ( EXPP_ReturnPyObjError( PyExc_MemoryError,
							"couldn't create PyList" ) );

		while( txt_iter ) {
			pyobj = Text_CreatePyObject( txt_iter );

			if( !pyobj ) {
				Py_DECREF(txtlist);
				return ( EXPP_ReturnPyObjError
					 ( PyExc_MemoryError,
					   "couldn't create PyString" ) );
			}
			PyList_SET_ITEM( txtlist, index, pyobj );

			txt_iter = txt_iter->id.next;
			index++;
		}

		return ( txtlist );
	}
}

/*****************************************************************************/
/* Function:              M_Text_Load                                        */
/* Python equivalent:     Blender.Text.Load                                  */
/* Description:           Receives a filename and returns the text object    */
/*                        created from the corresponding file.               */
/*****************************************************************************/
static PyObject *M_Text_Load( PyObject * self, PyObject * value )
{
	char *fname = PyString_AsString(value);
	char fpath[FILE_MAXDIR + FILE_MAXFILE];
	Text *txt_ptr = NULL;
	unsigned int maxlen = FILE_MAXDIR + FILE_MAXFILE;

	if( !fname )
		return ( EXPP_ReturnPyObjError( PyExc_TypeError,
						"expected string argument" ) );

	if (strlen(fname) > (maxlen - 1))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"text filename too long");
	else if (!BLI_exists(fname))
		return EXPP_ReturnPyObjError (PyExc_AttributeError,
			"text file not found");

	BLI_strncpy(fpath, fname, maxlen);

	txt_ptr = add_text( fpath );
	if( !txt_ptr )
		return EXPP_ReturnPyObjError( PyExc_IOError,
					      "couldn't load text" );

	return Text_CreatePyObject(txt_ptr);
}

/*****************************************************************************/
/* Function:              M_Text_unlink                                      */
/* Python equivalent:     Blender.Text.unlink                                */
/* Description:           Removes the given Text object from Blender         */
/*****************************************************************************/
static PyObject *M_Text_unlink( PyObject * self, PyObject * args )
{
	BPy_Text *textobj;
	Text *text;

	if( !PyArg_ParseTuple( args, "O!", &Text_Type, &textobj ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a Text object as argument" );

	text = ( ( BPy_Text * ) textobj )->text;

	if( !text )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "this text was already unlinked!" );

	BPY_clear_bad_scriptlinks( text );
	free_text_controllers( text );
	unlink_text( text );

	free_libblock( &G.main->text, text );

	( ( BPy_Text * ) textobj )->text = NULL;

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Function:              Text_Init                                          */
/*****************************************************************************/
PyObject *Text_Init( void )
{
	PyObject *submodule;

	if( PyType_Ready( &Text_Type ) < 0 )
		return NULL;

	submodule =
		Py_InitModule3( "Blender.Text", M_Text_methods, M_Text_doc );

	return ( submodule );
}

/*****************************************************************************/
/* Function:              Text_CreatePyObject                                */
/*****************************************************************************/
PyObject *Text_CreatePyObject( Text * txt )
{
	BPy_Text *pytxt;

	pytxt = ( BPy_Text * ) PyObject_NEW( BPy_Text, &Text_Type );

	if( !pytxt )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create BPy_Text PyObject" );

	pytxt->text = txt;

	return ( PyObject * ) pytxt;
}

/*****************************************************************************/
/* Python BPy_Text methods:                                                  */
/*****************************************************************************/
static PyObject *Text_getFilename( BPy_Text * self )
{
	if( self->text->name )
		return PyString_FromString( self->text->name );
	
	Py_RETURN_NONE;
}

static PyObject *Text_getNLines( BPy_Text * self )
{				/* text->nlines isn't updated in Blender (?) */
	int nlines = 0;
	TextLine *line;

	line = self->text->lines.first;

	while( line ) {		/* so we have to count them ourselves */
		line = line->next;
		nlines++;
	}

	self->text->nlines = nlines;	/* and update Blender, too (should we?) */

	return PyInt_FromLong( nlines );
}

static PyObject *Text_clear( BPy_Text * self)
{
	int oldstate;

	if( !self->text )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "This object isn't linked to a Blender Text Object" );

	oldstate = txt_get_undostate(  );
	txt_set_undostate( 1 );
	txt_sel_all( self->text );
	txt_cut_sel( self->text );
	txt_set_undostate( oldstate );

	Py_RETURN_NONE;
}

static PyObject *Text_set( BPy_Text * self, PyObject * args )
{
	int ival;
	char *attr;

	if( !PyArg_ParseTuple( args, "si", &attr, &ival ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string and an int as arguments" );

	if( strcmp( "follow_cursor", attr ) == 0 ) {
		if( ival )
			self->text->flags |= EXPP_TEXT_MODE_FOLLOW;
		else
			self->text->flags &= EXPP_TEXT_MODE_FOLLOW;
	}

	Py_RETURN_NONE;
}

static PyObject *Text_write( BPy_Text * self, PyObject * value )
{
	char *str = PyString_AsString(value);
	int oldstate;

	if( !self->text )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "This object isn't linked to a Blender Text Object" );

	if( !str )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	oldstate = txt_get_undostate(  );
	txt_insert_buf( self->text, str );
	txt_move_eof( self->text, 0 );
	txt_set_undostate( oldstate );

	Py_RETURN_NONE;
}

static PyObject *Text_asLines( BPy_Text * self )
{
	TextLine *line;
	PyObject *list, *tmpstr;

	if( !self->text )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
					      "This object isn't linked to a Blender Text Object" );

	line = self->text->lines.first;
	list = PyList_New( 0 );

	if( !list )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create PyList" );

	while( line ) {
		tmpstr = PyString_FromString( line->line );
		PyList_Append( list, tmpstr );
		Py_DECREF(tmpstr);
		line = line->next;
	}

	return list;
}

/*****************************************************************************/
/* Function:    Text_compare                                                 */
/* Description: This is a callback function for the BPy_Text type. It        */
/*              compares two Text_Type objects. Only the "==" and "!="       */
/*              comparisons are meaninful. Returns 0 for equality and -1 if  */
/*              they don't point to the same Blender Text struct.            */
/*              In Python it becomes 1 if they are equal, 0 otherwise.       */
/*****************************************************************************/
static int Text_compare( BPy_Text * a, BPy_Text * b )
{
	return ( a->text == b->text ) ? 0 : -1;
}

/*****************************************************************************/
/* Function:    Text_repr                                                    */
/* Description: This is a callback function for the BPy_Text type. It        */
/*              builds a meaninful string to represent text objects.         */
/*****************************************************************************/
static PyObject *Text_repr( BPy_Text * self )
{
	if( self->text )
		return PyString_FromFormat( "[Text \"%s\"]",
					    self->text->id.name + 2 );
	else
		return PyString_FromString( "[Text <deleted>]" );
}

/*****************************************************************************/
/* Python attributes get/set functions:                                      */
/*****************************************************************************/
static PyObject *Text_getMode(BPy_Text * self)
{
	return PyInt_FromLong( self->text->flags );
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
static PyGetSetDef BPy_Text_getseters[] = {
	GENERIC_LIB_GETSETATTR,
	{"filename", (getter)Text_getFilename, (setter)NULL,
	 "text filename", NULL},
	{"mode", (getter)Text_getMode, (setter)NULL,
	 "text mode flag", NULL},
	{"nlines", (getter)Text_getNLines, (setter)NULL,
	 "number of lines", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

/*****************************************************************************/
/* Python Text_Type structure definition:                                    */
/*****************************************************************************/
PyTypeObject Text_Type = {
	PyObject_HEAD_INIT( NULL ) 
	0,	/* ob_size */
	"Blender Text",		/* tp_name */
	sizeof( BPy_Text ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,	/* tp_dealloc */
	NULL,			/* tp_print */
	NULL,	/* tp_getattr */
	NULL,	/* tp_setattr */
	( cmpfunc ) Text_compare,	/* tp_compare */
	( reprfunc ) Text_repr,	/* tp_repr */

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
	BPy_Text_methods,           /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Text_getseters,         /* struct PyGetSetDef *tp_getset; */
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
