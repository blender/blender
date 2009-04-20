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
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include "Text.h" /*This must come first*/

#include "BKE_library.h"
#include "BKE_sca.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BIF_drawtext.h"
#include "BIF_screen.h"
#include "BKE_text.h"
#include "BKE_suggestions.h"
#include "BLI_blenlib.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "MEM_guardedalloc.h"
#include "gen_utils.h"
#include "gen_library.h"
#include "../BPY_extern.h"

#define EXPP_TEXT_MODE_FOLLOW TXT_FOLLOW

/* checks for the group being removed */
#define TEXT_DEL_CHECK_PY(bpy_text) if (!(bpy_text->text)) return ( EXPP_ReturnPyObjError( PyExc_RuntimeError, "Text has been removed" ) )
#define TEXT_DEL_CHECK_INT(bpy_text) if (!(bpy_text->text)) return ( EXPP_ReturnIntError( PyExc_RuntimeError, "Text has been removed" ) )

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
static PyObject *Text_reset( BPy_Text * self );
static PyObject *Text_readline( BPy_Text * self );
static PyObject *Text_write( BPy_Text * self, PyObject * value );
static PyObject *Text_insert( BPy_Text * self, PyObject * value );
static PyObject *Text_delete( BPy_Text * self, PyObject * value );
static PyObject *Text_set( BPy_Text * self, PyObject * args );
static PyObject *Text_asLines( BPy_Text * self, PyObject * args );
static PyObject *Text_getCursorPos( BPy_Text * self );
static PyObject *Text_setCursorPos( BPy_Text * self, PyObject * args );
static PyObject *Text_getSelectPos( BPy_Text * self );
static PyObject *Text_setSelectPos( BPy_Text * self, PyObject * args );
static PyObject *Text_markSelection( BPy_Text * self, PyObject * args );
static PyObject *Text_suggest( BPy_Text * self, PyObject * args );
static PyObject *Text_showDocs( BPy_Text * self, PyObject * args );

static void text_reset_internal( BPy_Text * self ); /* internal func */

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
	{"reset", ( PyCFunction ) Text_reset, METH_NOARGS,
	 "() - Moves the IO pointer back to the start of the Text buffer for reading"},
	{"readline", ( PyCFunction ) Text_readline, METH_NOARGS,
	 "() - Reads a line of text from the buffer and returns it incrementing the internal IO pointer."},
	{"write", ( PyCFunction ) Text_write, METH_O,
	 "(line) - Append string 'str' to Text buffer"},
	{"insert", ( PyCFunction ) Text_insert, METH_O,
	 "(line) - Insert string 'str' to Text buffer at cursor location"},
	{"delete", ( PyCFunction ) Text_delete, METH_O,
	 "(chars) - Deletes a number of characters to the left (chars<0) or right (chars>0)"},
	{"set", ( PyCFunction ) Text_set, METH_VARARGS,
	 "(name, val) - Set attribute 'name' to value 'val'"},
	{"asLines", ( PyCFunction ) Text_asLines, METH_VARARGS,
	 "(start=0, end=nlines) - Return text buffer as a list of lines between start and end"},
	{"getCursorPos", ( PyCFunction ) Text_getCursorPos, METH_NOARGS,
	 "() - Return cursor position as (row, col) tuple"},
	{"setCursorPos", ( PyCFunction ) Text_setCursorPos, METH_VARARGS,
	 "(row, col) - Set the cursor position to (row, col)"},
	{"getSelectPos", ( PyCFunction ) Text_getSelectPos, METH_NOARGS,
	 "() - Return the selection cursor position as (row, col) tuple"},
	{"setSelectPos", ( PyCFunction ) Text_setSelectPos, METH_VARARGS,
	 "(row, col) - Set the selection cursor position to (row, col)"},
	{"markSelection", ( PyCFunction ) Text_markSelection, METH_VARARGS,
	 "(group, (r, g, b), flags) - Places a marker over the current selection. Group: number > 0, flags: TMARK_TEMP, TMARK_EDITALL, etc."},
	{"suggest", ( PyCFunction ) Text_suggest, METH_VARARGS,
	 "(list, prefix='') - Presents a list of suggestions. List is of strings, or tuples. Tuples must be of the form (name, type) where type is one of 'm', 'v', 'f', 'k' for module, variable, function and keyword respectively or '?' for other types"},
	{"showDocs", ( PyCFunction ) Text_showDocs, METH_VARARGS,
	 "(docs) - Documentation string"},
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
	BPY_free_pyconstraint_links( text );
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
	PyObject *submodule, *dict;

	if( PyType_Ready( &Text_Type ) < 0 )
		return NULL;

	submodule =
		Py_InitModule3( "Blender.Text", M_Text_methods, M_Text_doc );

	dict = PyModule_GetDict( submodule );
	
#define EXPP_ADDCONST(x) \
	EXPP_dict_set_item_str(dict, #x, PyInt_FromLong(x))

	/* So, for example:
	 * EXPP_ADDCONST(LEFTMOUSE) becomes
	 * EXPP_dict_set_item_str(dict, "LEFTMOUSE", PyInt_FromLong(LEFTMOUSE)) 
	 */

	EXPP_ADDCONST( TMARK_TEMP );
	EXPP_ADDCONST( TMARK_EDITALL );

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
	text_reset_internal(pytxt);

	return ( PyObject * ) pytxt;
}

/*****************************************************************************/
/* Python BPy_Text methods:                                                  */
/*****************************************************************************/
static PyObject *Text_getFilename( BPy_Text * self )
{
	TEXT_DEL_CHECK_PY(self);
	if( self->text->name )
		return PyString_FromString( self->text->name );
	
	Py_RETURN_NONE;
}

static PyObject *Text_getNLines( BPy_Text * self )
{				/* text->nlines isn't updated in Blender (?) */
	int nlines = 0;
	TextLine *line;
	
	TEXT_DEL_CHECK_PY(self);
	
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

	TEXT_DEL_CHECK_PY(self);

	oldstate = txt_get_undostate(  );
	txt_set_undostate( 1 );
	txt_sel_all( self->text );
	txt_cut_sel( self->text );
	txt_set_undostate( oldstate );

	Py_RETURN_NONE;
}

static void text_reset_internal( BPy_Text * self )
{
	self->iol = NULL;
	self->ioc = -1;
}

static PyObject *Text_reset( BPy_Text * self )
{
	text_reset_internal(self);
	Py_RETURN_NONE;
}

static PyObject *Text_readline( BPy_Text * self )
{
	PyObject *tmpstr;
	
	TEXT_DEL_CHECK_PY(self);

	/* Reset */
	if (!self->iol && self->ioc == -1) {
		self->iol = self->text->lines.first;
		self->ioc = 0;
	}

	if (!self->iol) {
		PyErr_SetString( PyExc_StopIteration, "End of buffer reached" );
		return PyString_FromString( "" );
	}

	if (self->ioc > self->iol->len) {
		self->iol = NULL;
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
						  "Line length exceeded, text may have changed while reading" );
	}

	tmpstr = PyString_FromString( self->iol->line + self->ioc );
	if (self->iol->next)
		PyString_ConcatAndDel( &tmpstr, PyString_FromString("\n") );

	self->iol = self->iol->next;
	self->ioc = 0;

	return tmpstr;
}

static PyObject *Text_write( BPy_Text * self, PyObject * value )
{
	char *str = PyString_AsString(value);
	int oldstate;

	if( !str )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );

	TEXT_DEL_CHECK_PY(self);
	
	oldstate = txt_get_undostate(  );
	txt_insert_buf( self->text, str );
	txt_move_eof( self->text, 0 );
	txt_set_undostate( oldstate );

	text_reset_internal( self );

	Py_RETURN_NONE;
}

static PyObject *Text_insert( BPy_Text * self, PyObject * value )
{
	char *str = PyString_AsString(value);
	int oldstate;

	if( !str )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected string argument" );
	
	TEXT_DEL_CHECK_PY(self);

	oldstate = txt_get_undostate(  );
	txt_insert_buf( self->text, str );
	txt_set_undostate( oldstate );

	text_reset_internal( self );

	Py_RETURN_NONE;
}

static PyObject *Text_delete( BPy_Text * self, PyObject * value )
{
	int num = PyInt_AsLong(value);
	int oldstate;

	TEXT_DEL_CHECK_PY(self);
	
	/* zero num is invalid and -1 is an error value */
	if( !num || (num==-1 && PyErr_Occurred()))
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected non-zero int argument" );

	oldstate = txt_get_undostate(  );
	while (num<0) {
		txt_backspace_char(self->text);
		num++;
	}
	while (num>0) {
		txt_delete_char(self->text);
		num--;
	}
	txt_set_undostate( oldstate );
	
	text_reset_internal( self );

	Py_RETURN_NONE;
}

static PyObject *Text_set( BPy_Text * self, PyObject * args )
{
	int ival;
	char *attr;

	TEXT_DEL_CHECK_PY(self);
	
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

static PyObject *Text_asLines( BPy_Text * self, PyObject * args )
{
	TextLine *line;
	PyObject *list, *tmpstr;
	int start=0, end=-1, i;

	TEXT_DEL_CHECK_PY(self);

	if( !PyArg_ParseTuple( args, "|ii", &start, &end ) )
			return EXPP_ReturnPyObjError( PyExc_TypeError,
							  "expected upto two optional ints as arguments" );
	
	if (start<0)
		start=0;

	line = self->text->lines.first;
	for (i = 0; i < start && line->next; i++)
		line= line->next;

	list = PyList_New( 0 );

	if( !list )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
					      "couldn't create PyList" );

	while( line && (i < end || end == -1) ) {
		tmpstr = PyString_FromString( line->line );
		PyList_Append( list, tmpstr );
		Py_DECREF(tmpstr);
		line = line->next;
		i++;
	}

	return list;
}

static PyObject *Text_getCursorPos( BPy_Text * self )
{
	Text *text;
	TextLine *linep;
	int row, col;

	TEXT_DEL_CHECK_PY(self);
	
	text = self->text;

	for (row=0,linep=text->lines.first; linep!=text->curl; linep=linep->next)
		row++;
	col= text->curc;

	return Py_BuildValue( "ii", row, col );
}

static PyObject *Text_setCursorPos( BPy_Text * self, PyObject * args )
{
	int row, col;
	SpaceText *st;

	TEXT_DEL_CHECK_PY(self);

	if (!PyArg_ParseTuple(args, "ii", &row, &col))
		return EXPP_ReturnPyObjError(PyExc_TypeError,
					      "expected two ints as arguments.");
	if (row<0) row=0;
	if (col<0) col=0;

	txt_move_to(self->text, row, col, 0);

	if (curarea->spacetype == SPACE_TEXT && (st=curarea->spacedata.first))
		pop_space_text(st);

	Py_RETURN_NONE;
}

static PyObject *Text_getSelectPos( BPy_Text * self )
{
	Text *text;
	TextLine *linep;
	int row, col;

	TEXT_DEL_CHECK_PY(self);
	
	text = self->text;

	for (row=0,linep=text->lines.first; linep!=text->sell; linep=linep->next)
		row++;
	col= text->selc;

	return Py_BuildValue( "ii", row, col );
}

static PyObject *Text_setSelectPos( BPy_Text * self, PyObject * args )
{
	int row, col;
	SpaceText *st;

	TEXT_DEL_CHECK_PY(self);

	if (!PyArg_ParseTuple(args, "ii", &row, &col))
		return EXPP_ReturnPyObjError(PyExc_TypeError,
					      "expected two ints as arguments.");
	if (row<0) row=0;
	if (col<0) col=0;

	txt_move_to(self->text, row, col, 1);

	if (curarea->spacetype == SPACE_TEXT && (st=curarea->spacedata.first))
		pop_space_text(st);

	Py_RETURN_NONE;
}

static PyObject *Text_markSelection( BPy_Text * self, PyObject * args )
{
	int group = 0, flags = 0,r, g, b;
	Text *text;
	char color[4];

	TEXT_DEL_CHECK_PY(self);
	
	text = self->text;

	if (!PyArg_ParseTuple(args, "i(iii)i", &group, &r, &g, &b, &flags))
		return EXPP_ReturnPyObjError(PyExc_TypeError,
					      "expected int, 3-tuple of ints and int as arguments.");

	if (text->curl != text->sell)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
					      "Cannot mark multi-line selection.");

	color[0] = (char) (r&0xFF);
	color[1] = (char) (g&0xFF);
	color[2] = (char) (b&0xFF);
	color[3] = 255;

	group &= 0xFFFF;

	txt_add_marker(text, text->curl, text->curc, text->selc, color, group, flags);
	
	Py_RETURN_NONE;
}

static PyObject *Text_suggest( BPy_Text * self, PyObject * args )
{
	PyObject *item = NULL, *tup1 = NULL, *tup2 = NULL;
	PyObject *list = NULL;
	int list_len, i;
	char *prefix = NULL, *name, type;
	SpaceText *st;

	TEXT_DEL_CHECK_PY(self);

	/* Parse args for a list of strings/tuples */
	if (!PyArg_ParseTuple(args, "O!|s", &PyList_Type, &list, &prefix))
		return EXPP_ReturnPyObjError(PyExc_TypeError,
				"expected list of strings or tuples followed by an optional string");

	if (curarea->spacetype != SPACE_TEXT)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"Active space type is not text");
	
	st = curarea->spacedata.first;
	if (!st || !st->text)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"Active text area has no Text object");
	
	texttool_suggest_clear();
	texttool_text_set_active(st->text);
	list_len = PyList_Size(list);
	
	for (i = 0; i < list_len; i++) {
		item = PyList_GetItem(list, i);

		if (PyString_Check(item)) {
			name = PyString_AsString(item);
			type = '?';
		} else if (PyTuple_Check(item) && PyTuple_GET_SIZE(item) == 2) {
			tup1 = PyTuple_GetItem(item, 0);
			tup2 = PyTuple_GetItem(item, 1);
			if (PyString_Check(tup1) && PyString_Check(tup2)) {
				name = PyString_AsString(tup1);
				type = PyString_AsString(tup2)[0];
			} else
				return EXPP_ReturnPyObjError(PyExc_AttributeError,
						"list must contain tuples of two strings only: (name, type)" );
		} else
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
					"list must contain only individual strings or tuples of size 2" );

		if (!strlen(name) || (type!='m' && type!='v' && type!='f' && type!='k' && type!='?'))
			return EXPP_ReturnPyObjError(PyExc_AttributeError,
					"names must be non-empty and types in ['m', 'v', 'f', 'k', '?']" );

		texttool_suggest_add(name, type);
	}
	if (!prefix)
		prefix = "";
	texttool_suggest_prefix(prefix);
	scrarea_queue_redraw(curarea);

	Py_RETURN_NONE;
}

static PyObject *Text_showDocs( BPy_Text * self, PyObject * args )
{
	char *docs;
	SpaceText *st;
	
	TEXT_DEL_CHECK_PY(self);

	if (!PyArg_ParseTuple(args, "s", &docs))
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected a string as argument" );

	if (curarea->spacetype != SPACE_TEXT)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"Active space type is not text");
	
	st = curarea->spacedata.first;
	if (!st || !st->text)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"Active text area has no Text object");

	texttool_text_set_active(st->text);
	texttool_docs_show(docs);
	scrarea_queue_redraw(curarea);

	Py_RETURN_NONE;
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
	TEXT_DEL_CHECK_PY(self);
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
