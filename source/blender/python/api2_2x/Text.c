/* 
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

#include "Text.h"

/*****************************************************************************/
/* Function:              M_Text_New                                       */
/* Python equivalent:     Blender.Text.New                                 */
/*****************************************************************************/
static PyObject *M_Text_New(PyObject *self, PyObject *args, PyObject *keywords)
{
  char *name = NULL;
  char buf[21];
  int follow = 0;
  Text   *bl_text; /* blender text object */
  C_Text *py_text; /* python wrapper */

  if (!PyArg_ParseTuple(args, "|si", &name, &follow))
        return EXPP_ReturnPyObjError (PyExc_AttributeError,
          "expected string and int arguments (or nothing)");

  bl_text = add_empty_text();

  if (bl_text)
    py_text = (C_Text *)PyObject_NEW(C_Text, &Text_Type);
  else
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                    "couldn't create Text Object in Blender");
  if (!py_text)
    return EXPP_ReturnPyObjError (PyExc_MemoryError,
                    "couldn't create Text Object wrapper");

  py_text->text = bl_text;

  if (follow) bl_text->flags |= EXPP_TEXT_MODE_FOLLOW;

  if (name) {
    PyOS_snprintf(buf, sizeof(buf), "%s", name);
    rename_id(&bl_text->id, buf);
  }

  return (PyObject *)py_text;
}

/*****************************************************************************/
/* Function:              M_Text_Get                                         */
/* Python equivalent:     Blender.Text.Get                                   */
/* Description:           Receives a string and returns the text object      */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all text names in the current */
/*                        scene is returned.                                 */
/*****************************************************************************/
static PyObject *M_Text_Get(PyObject *self, PyObject *args)
{
  char *name = NULL;
  Text *txt_iter;

	if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument (or nothing)"));

  txt_iter = G.main->text.first;

	if (name) { /* (name) - Search text by name */

    C_Text *wanted_txt = NULL;

    while ((txt_iter) && (wanted_txt == NULL)) {
      if (strcmp (name, txt_iter->id.name+2) == 0) {
        wanted_txt = (C_Text *)PyObject_NEW(C_Text, &Text_Type);
				if (wanted_txt) wanted_txt->text = txt_iter;
      }
      txt_iter = txt_iter->id.next;
    }

    if (wanted_txt == NULL) { /* Requested text doesn't exist */
      char error_msg[64];
      PyOS_snprintf(error_msg, sizeof(error_msg),
                      "Text \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }

    return (PyObject *)wanted_txt;
	}

	else { /* () - return a list of all texts in the scene */
    int index = 0;
    PyObject *txtlist, *pystr;

    txtlist = PyList_New (BLI_countlist (&(G.main->text)));

    if (txtlist == NULL)
      return (PythonReturnErrorObject (PyExc_MemoryError,
              "couldn't create PyList"));

		while (txt_iter) {
      pystr = PyString_FromString (txt_iter->id.name+2);

			if (!pystr)
				return (PythonReturnErrorObject (PyExc_MemoryError,
									"couldn't create PyString"));

			PyList_SET_ITEM (txtlist, index, pystr);

      txt_iter = txt_iter->id.next;
      index++;
		}

		return (txtlist);
	}
}

/*****************************************************************************/
/* Function:              M_Text_Get                                         */
/* Python equivalent:     Blender.Text.Load                                  */
/* Description:           Receives a filename and returns the text object    */
/*                        created from the corresponding file.               */
/*****************************************************************************/
static PyObject *M_Text_Load(PyObject *self, PyObject *args)
{
  char   *fname;
  Text   *txt_ptr;
  C_Text *txt;

  if (!PyArg_ParseTuple(args, "s", &fname))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument"));
  
  txt = (C_Text *)PyObject_NEW(C_Text, &Text_Type);

  if (!txt)
    return EXPP_ReturnPyObjError (PyExc_MemoryError,
            "couldn't create PyObject Text_Type");

  txt_ptr = add_text(fname);
  if (!txt_ptr)
    return EXPP_ReturnPyObjError (PyExc_IOError,
            "couldn't load text");

  txt->text = txt_ptr;

  return (PyObject *)txt;
}

/*@This function removes the text entry from the text editor.
 * The text is not freed here, but inside the garbage collector. */

/* This function actually makes Blender dump core if the script is repeatedly
 * executed, gotta investigate better */

static PyObject *M_Text_unlink(PyObject *self, PyObject *args)
{
	C_Text *textobj;

	if (!PyArg_ParseTuple(args, "O!", &Text_Type, &textobj))
    return EXPP_ReturnPyObjError (PyExc_TypeError,
          "expected a Text object as argument");

	BPY_clear_bad_scriptlinks(textobj->text);
	free_text_controllers(textobj->text);
	unlink_text(textobj->text);
	/*@We actually should not free the text object here, but let the
	 * __del__ method of the wrapper do the job. This would require some
	 * changes in the GUI code though.. 
	 * So we mark the wrapper as invalid by setting wrapper->data = 0 */
	free_libblock(&G.main->text, textobj->text);

  textobj->text = NULL; /* XXX */
  Py_XDECREF(textobj); /* XXX just a guess -- works ? */

	Py_INCREF(Py_None);	
	return Py_None;
}

/*****************************************************************************/
/* Function:              M_Text_Init                                      */
/*****************************************************************************/
PyObject *M_Text_Init (void)
{
  PyObject  *submodule;

  Text_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3("Blender.Text", M_Text_methods, M_Text_doc);

  return (submodule);
}

/*****************************************************************************/
/* Python C_Text methods:                                                  */
/*****************************************************************************/
static PyObject *Text_getName(C_Text *self)
{
  PyObject *attr = PyString_FromString(self->text->id.name+2);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Text.name attribute");
}

static PyObject *Text_getFilename(C_Text *self)
{
  PyObject *attr = PyString_FromString(self->text->name);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Text.filename attribute");
}

static PyObject *Text_getNLines(C_Text *self)
{ /* text->nlines isn't updated in Blender (?) */
  int nlines = 0;
  TextLine *line;
  PyObject *attr;

	line = self->text->lines.first;

  while (line) { /* so we have to count them ourselves */
    line = line->next;
    nlines++;
  }

  self->text->nlines = nlines; /* and update Blender, too (should we?) */

  attr = PyInt_FromLong(nlines);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Text.nlines attribute");
}

static PyObject *Text_setName(C_Text *self, PyObject *args)
{
  char *name;
  char buf[21];

  if (!PyArg_ParseTuple(args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument"));
  
  PyOS_snprintf(buf, sizeof(buf), "%s", name);
  
  rename_id(&self->text->id, buf);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Text_clear(C_Text *self, PyObject *args)
{
	int oldstate;

	if (!self->text)
     return EXPP_ReturnPyObjError (PyExc_RuntimeError,
            "This object isn't linked to a Blender Text Object");

	oldstate = txt_get_undostate();
	txt_set_undostate(1);
	txt_sel_all(self->text);
	txt_cut_sel(self->text);
	txt_set_undostate(oldstate);
	
	Py_INCREF(Py_None);	
	return Py_None;
}

static PyObject *Text_set(C_Text *self, PyObject *args)
{
	int ival;
	char *attr;

	if (!PyArg_ParseTuple(args, "si", &attr, &ival))
       return EXPP_ReturnPyObjError (PyExc_TypeError,
             "expected a string and an int as arguments");

	if (strcmp("follow_cursor", attr) == 0) {
		if (ival)
			self->text->flags |= EXPP_TEXT_MODE_FOLLOW;
		else
			self->text->flags &= EXPP_TEXT_MODE_FOLLOW;
	}

	Py_INCREF(Py_None);	
	return Py_None;
}

static PyObject *Text_write(C_Text *self, PyObject *args)
{
	char *str;
	int oldstate;

	if (!self->text)
     return EXPP_ReturnPyObjError (PyExc_RuntimeError,
            "This object isn't linked to a Blender Text Object");

	if (!PyArg_ParseTuple(args, "s", &str))
     return EXPP_ReturnPyObjError (PyExc_TypeError,
             "expected string argument");

	oldstate = txt_get_undostate();
	txt_insert_buf(self->text, str);
	txt_move_eof(self->text, 0);
	txt_set_undostate(oldstate);

	Py_INCREF(Py_None);	
	return Py_None;
}

static PyObject *Text_asLines(C_Text *self, PyObject *args)
{
	TextLine *line;
	PyObject *list, *ob;

	if (!self->text)
     return EXPP_ReturnPyObjError (PyExc_RuntimeError,
            "This object isn't linked to a Blender Text Object");

	line = self->text->lines.first;
	list= PyList_New(0);

	if (!list)
     return EXPP_ReturnPyObjError (PyExc_MemoryError,
            "couldn't create PyList");

	while (line) {
		ob = Py_BuildValue("s", line->line);
		PyList_Append(list, ob);	
		line = line->next;
	}	

  return list;
}

/*****************************************************************************/
/* Function:    TextDeAlloc                                                */
/* Description: This is a callback function for the C_Text type. It is     */
/*              the destructor function.                                     */
/*****************************************************************************/
static void TextDeAlloc (C_Text *self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    TextGetAttr                                                */
/* Description: This is a callback function for the C_Text type. It is     */
/*              the function that accesses C_Text member variables and     */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject* TextGetAttr (C_Text *self, char *name)
{
  PyObject *attr = Py_None;

  if (strcmp(name, "name") == 0)
    attr = PyString_FromString(self->text->id.name+2);
  else if (strcmp(name, "filename") == 0)
    attr = PyString_FromString(self->text->name);
  else if (strcmp(name, "mode") == 0)
    attr = PyInt_FromLong(self->text->flags);
  else if (strcmp(name, "nlines") == 0)
    attr = Text_getNLines(self);

  else if (strcmp(name, "__members__") == 0)
    attr = Py_BuildValue("[s,s,s,s]",
                    "name", "filename", "mode", "nlines");

  if (!attr)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                            "couldn't create PyObject"));

  if (attr != Py_None) return attr; /* attribute found, return its value */

  /* not an attribute, search the methods table */
  return Py_FindMethod(C_Text_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    TextSetAttr                                                */
/* Description: This is a callback function for the C_Text type. It is the */
/*              function that changes Text Data members values. If this    */
/*              data is linked to a Blender Text, it also gets updated.    */
/*****************************************************************************/
static int TextSetAttr (C_Text *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;

/* We're playing a trick on the Python API users here.  Even if they use
 * Text.member = val instead of Text.setMember(value), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Text structure when necessary. */

  valtuple = Py_BuildValue("(N)", value); /* the set* functions expect a tuple */

  if (!valtuple)
    return EXPP_ReturnIntError(PyExc_MemoryError,
                  "TextSetAttr: couldn't create PyTuple");

  if (strcmp (name, "name") == 0)
    error = Text_setName (self, valtuple);
  else { /* Error: no such member in the Text Data structure */
    Py_DECREF(value);
    Py_DECREF(valtuple);
    return (EXPP_ReturnIntError (PyExc_KeyError,
            "attribute not found or immutable"));
  }

  Py_DECREF(valtuple);

  if (error != Py_None) return -1;

  Py_DECREF(Py_None); /* incref'ed by the called set* function */
  return 0; /* normal exit */
}

/*****************************************************************************/
/* Function:    TextCompare                                                  */
/* Description: This is a callback function for the C_Text type. It          */
/*              compares two Text_Type objects. Only the "==" and "!="       */
/*              comparisons are meaninful. Returns 0 for equality and -1 if  */
/*              they don't point to the same Blender Text struct.            */
/*              In Python it becomes 1 if they are equal, 0 otherwise.       */
/*****************************************************************************/
static int TextCompare (C_Text *a, C_Text *b)
{
	Text *pa = a->text, *pb = b->text;
	return (pa == pb) ? 0:-1;
}

/*****************************************************************************/
/* Function:    TextPrint                                                  */
/* Description: This is a callback function for the C_Text type. It        */
/*              builds a meaninful string to 'print' text objects.         */
/*****************************************************************************/
static int TextPrint(C_Text *self, FILE *fp, int flags)
{ 
  fprintf(fp, "[Text \"%s\"]", self->text->id.name+2);
  return 0;
}

/*****************************************************************************/
/* Function:    TextRepr                                                   */
/* Description: This is a callback function for the C_Text type. It        */
/*              builds a meaninful string to represent text objects.       */
/*****************************************************************************/
static PyObject *TextRepr (C_Text *self)
{
  return PyString_FromString(self->text->id.name+2);
}
