/** Text buffer module; access to Text buffers in Blender
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
  *
  * The ownership relations of a Text buffer are in Blender pretty clear:
  * The Text editor is ALWAYS the container for all text objects.
  * Currently, the Text object is implemented as a free object though, as
  * the ownership of a Text might change in future. The reference counting of
  * a Text object IN BLENDER is not really maintained though (for the above ownership
  * reason).
  * This introduces a problem if a Text object is accessed after it was actually
  * deleted. Currently, a 'guard' is implemented for access after deletion INSIDE 
  * A SCRIPT. The Blender GUI is not aware of the wrapper though, so if a Text buffer
  * is cleared while the script is accessing the wrapper, bad results are expected.
  * BUT: This currently can not happen, unless a Python script is running in the
  * background as a separate thread...
  * 
  * TODO: 
  * 
  * either a):
  *    figure out ownership and implement each access to the text buffer by
  *    name and not by reference (pointer). This will require quite some additions
  *    in the generic DataBlock access (opy_datablock.c)
  * 
  * or     b):
  *    implement reference counting for text buffers properly, so that a deletion
  *    of a text buffer by the GUI does not result in a release of the actual
  *    Text object, but by a DECREF. The garbage collector (or wrapper deletion method)
  *    will then free the Text object.
  * 
  * To be discussed and evaluated.
  * 
  * $Id$
  *
  */


#include "Python.h"
#include "stringobject.h"

#include "BPY_macros.h"
#include "BKE_text.h"
#include "BIF_drawtext.h"
#include "DNA_text_types.h"
#include "BPY_extern.h"
#include "BKE_sca.h"

#include "b_interface.h"
#include "opy_datablock.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

DATABLOCK_GET(Textmodule, text object, getTextList())

#define CHECK_VALIDTEXT(x) CHECK_VALIDDATA(x, \
	"Text was deleted; illegal access!")

#define OFF 1

static char Textmodule_New_doc[] =
"(name = None, follow = 0) - Create new text buffer with (optionally given)\n\
name.\n\
If 'follow' == 1, the text display always follows the cursor";

static PyObject *Textmodule_New(PyObject *self, PyObject *args)
{
	Text *text;
	PyObject *textobj;
	PyObject *name = NULL;
	int follow = 0;

	text = add_empty_text();
	BPY_TRY(PyArg_ParseTuple(args, "|O!i", &PyString_Type, &name, &follow));
	textobj = DataBlock_fromData(text);
	if (follow) {
		text->flags |= TXT_FOLLOW;
	}
	if (name) {
		DataBlock_setattr(textobj, "name", name);
	}
	return textobj;
}

static char Textmodule_unlink_doc[] =
"(text) - remove text object 'text' from the text window";

/** This function removes the text entry from the text editor. 
  * The text is not freed here, but inside the garbage collector 
  */

static PyObject *Textmodule_unlink(PyObject *self, PyObject *args)
{
	PyObject *textobj;
	Text *text;

	BPY_TRY(PyArg_ParseTuple(args, "O!", &DataBlock_Type, &textobj));
	if (!DataBlock_isType((DataBlock *) textobj, ID_TXT)) {
		PyErr_SetString(PyExc_TypeError, "Text object expected!");
		return NULL;
	}

	text = PYBLOCK_AS_TEXT(textobj);
	BPY_clear_bad_scriptlinks(text);
	free_text_controllers(text);
	unlink_text(text);
	/* We actually should not free the text object here, but let the
	 * __del__ method of the wrapper do the job. This would require some
	 * changes in the GUI code though.. 
	 * So we mark the wrapper as invalid by setting wrapper->data = 0 */
	free_libblock(getTextList(), text);
	MARK_INVALID(textobj);

	Py_INCREF(Py_None);	
	return Py_None;
}


/* these are the module methods */
struct PyMethodDef Textmodule_methods[] = {
	{"get", Textmodule_get, METH_VARARGS, Textmodule_get_doc}, 
	{"New", Textmodule_New, METH_VARARGS, Textmodule_New_doc},
	{"unlink", Textmodule_unlink, METH_VARARGS, Textmodule_unlink_doc},
	{NULL, NULL}
};

// Text object properties

DataBlockProperty Text_Properties[]= {
	{NULL}
};

/* This is uncommented only for an example on how (probably) not to
 * do it :-)
 * It's a bad idea in this case to have a wrapper object destroy its wrapped object
 * because checks have to be done whether the wrapper is still accessed after
 * the wrapped objects deletion. 
 * Better: unlink the object from it's owner: Blender.Text.unlink(text)
 * That way the object is not yet freed, but its refcount set to 0.
 * The garbage collector takes care of the rest..
 * But it has to be made sure that the wrapper object is no longer kept around
 * after the script ends.
 *

static char Text_delete_doc[] =
"() - delete text from Text window";

static PyObject *Text_delete(PyObject *self, PyObject *args)
{
	Text *text = PYBLOCK_AS_TEXT(self);
	// we have to check for validity, as the Text object is only a
	// descriptor...
	CHECK_VALIDTEXT(text)
	BPY_TRY(PyArg_ParseTuple(args, ""));
	BPY_clear_bad_scriptlinks(text);
	free_text_controllers(text);
	unlink_text(text);
	free_libblock(&getGlobal()->main->text, text);
	((DataBlock *) self)->data = NULL;
	Py_INCREF(Py_None);	
	return Py_None;
}

*/

/** This method gets called on the wrapper objects deletion.
  * Here we release the Text object if its refcount is == 0

	-- CURRENTLY UNCOMMENTED -- needs change in Blender kernel..

static PyObject *Text_del(PyObject *self, PyObject *args)
{
	Text *text = PYBLOCK_AS_TEXT(self);
	if (BOB_REFCNT((ID *) text) == 0) {
		free_libblock(&getGlobal()->main->text, text);
	}	
	Py_INCREF(Py_None);	
	return Py_None;
}
*/

static char Text_clear_doc[] =
"() - clear the text buffer";

static PyObject *Text_clear(PyObject *self, PyObject *args)
{
	Text *text = PYBLOCK_AS_TEXT(self);
	int oldstate;
	CHECK_VALIDTEXT(text)
	
	oldstate = txt_get_undostate();
	txt_set_undostate(OFF);
	txt_sel_all(text);
	txt_cut_sel(text);
	txt_set_undostate(oldstate);
	
	Py_INCREF(Py_None);	
	return Py_None;
}

static char Text_set_doc[] =
"(name, val) - set attribute name to val";

static PyObject *Text_set(PyObject *self, PyObject *args)
{
	int ival;
	char *attr;
	Text *text = PYBLOCK_AS_TEXT(self);

	BPY_TRY(PyArg_ParseTuple(args, "si", &attr, &ival));
	if (STREQ("follow_cursor", attr)) {
		if (ival) {
			text->flags |= TXT_FOLLOW;
		} else {
			text->flags &= TXT_FOLLOW;
		}
	}	
	Py_INCREF(Py_None);	
	return Py_None;
}

static char Text_write_doc[] =
"(line) - append string 'line' to the text buffer";

static PyObject *Text_write(PyObject *self, PyObject *args)
{
	char *str;
	Text *text = PYBLOCK_AS_TEXT(self);
	int oldstate;

	CHECK_VALIDTEXT(text)

	BPY_TRY(PyArg_ParseTuple(args, "s", &str));
	oldstate = txt_get_undostate();
	txt_insert_buf(text, str);
	txt_move_eof(text, 0);
	txt_set_undostate(oldstate);

	Py_INCREF(Py_None);	
	return Py_None;
}

static char Text_asLines_doc[] =
"() - returns the lines of the text buffer as list of strings";

static PyObject *Text_asLines(PyObject *self, PyObject *args)
{
	TextLine *line;
	PyObject *list, *ob;
	Text *text = (Text *) ((DataBlock *) self)->data;

	CHECK_VALIDTEXT(text)

	line = text->lines.first;
	list= PyList_New(0);
	while (line) {
		ob = Py_BuildValue("s", line->line);
		PyList_Append(list, ob);	
		line = line->next;
	}	
	return list;
}

/* these are the text object methods */
struct PyMethodDef Text_methods[] = {
	{"clear", Text_clear, METH_VARARGS, Text_clear_doc},
	{"write", Text_write, METH_VARARGS, Text_write_doc},
	{"set", Text_set, METH_VARARGS, Text_set_doc},
	{"asLines", Text_asLines, METH_VARARGS, Text_asLines_doc},
	{NULL, NULL}
};

