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

#include <Python.h>
#include <stdio.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_library.h>
#include <BKE_sca.h>
#include <BIF_drawtext.h>
#include <BKE_text.h>
#include <BLI_blenlib.h>
#include <DNA_text_types.h>

#include "gen_utils.h"
#include "modules.h"
#include "../BPY_extern.h"

#define EXPP_TEXT_MODE_FOLLOW TXT_FOLLOW

/*****************************************************************************/
/* Python API function prototypes for the Text module.                       */
/*****************************************************************************/
static PyObject *M_Text_New (PyObject *self, PyObject *args,
                PyObject *keywords);
static PyObject *M_Text_Get (PyObject *self, PyObject *args);
static PyObject *M_Text_Load (PyObject *self, PyObject *args);
static PyObject *M_Text_unlink (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Text.__doc__                                                      */
/*****************************************************************************/
static char M_Text_doc[] =
"The Blender Text module\n\n";

static char M_Text_New_doc[] =
"() - return a new Text object";

static char M_Text_Get_doc[] =
"(name) - return the Text with name 'name', \
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
  {"New",(PyCFunction)M_Text_New, METH_VARARGS|METH_KEYWORDS,
          M_Text_New_doc},
  {"Get",         M_Text_Get,         METH_VARARGS, M_Text_Get_doc},
  {"get",         M_Text_Get,         METH_VARARGS, M_Text_Get_doc},
  {"Load",        M_Text_Load,        METH_VARARGS, M_Text_Load_doc},
  {"unlink",      M_Text_unlink,      METH_VARARGS, M_Text_unlink_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Text structure definition:                                     */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  Text *text;

} BPy_Text;

static int Text_IsLinked(BPy_Text *self);

/*****************************************************************************/
/* Python BPy_Text methods declarations:                                     */
/*****************************************************************************/
static PyObject *Text_getName(BPy_Text *self);
static PyObject *Text_getFilename(BPy_Text *self);
static PyObject *Text_getNLines(BPy_Text *self);
static PyObject *Text_setName(BPy_Text *self, PyObject *args);
static PyObject *Text_clear(BPy_Text *self, PyObject *args);
static PyObject *Text_write(BPy_Text *self, PyObject *args);
static PyObject *Text_set(BPy_Text *self, PyObject *args);
static PyObject *Text_asLines(BPy_Text *self, PyObject *args);

/*****************************************************************************/
/* Python BPy_Text methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_Text_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Text_getName, METH_NOARGS,
          "() - Return Text Object name"},
  {"getFilename", (PyCFunction)Text_getFilename, METH_VARARGS,
          "() - Return Text Object filename"},
  {"getNLines", (PyCFunction)Text_getNLines, METH_VARARGS,
          "() - Return number of lines in text buffer"},
  {"setName", (PyCFunction)Text_setName, METH_VARARGS,
          "(str) - Change Text Object name"},
  {"clear", (PyCFunction)Text_clear, METH_VARARGS,
          "() - Clear Text buffer"},
  {"write", (PyCFunction)Text_write, METH_VARARGS,
          "(line) - Append string 'str' to Text buffer"},
  {"set", (PyCFunction)Text_set, METH_VARARGS,
          "(name, val) - Set attribute 'name' to value 'val'"},
  {"asLines", (PyCFunction)Text_asLines, METH_VARARGS,
          "() - Return text buffer as a list of lines"},
  {0}
};

/*****************************************************************************/
/* Python Text_Type callback function prototypes:                            */
/*****************************************************************************/
static void Text_dealloc (BPy_Text *self);
static int Text_setAttr (BPy_Text *self, char *name, PyObject *v);
static PyObject *Text_getAttr (BPy_Text *self, char *name);
static int Text_compare (BPy_Text *a, BPy_Text *b);
static PyObject *Text_repr (BPy_Text *self);

/*****************************************************************************/
/* Python Text_Type structure definition:                                    */
/*****************************************************************************/
PyTypeObject Text_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                    /* ob_size */
  "Blender Text",                       /* tp_name */
  sizeof (BPy_Text),                    /* tp_basicsize */
  0,                                    /* tp_itemsize */
  /* methods */
  (destructor)Text_dealloc,             /* tp_dealloc */
  0,                                    /* tp_print */
  (getattrfunc)Text_getAttr,            /* tp_getattr */
  (setattrfunc)Text_setAttr,            /* tp_setattr */
  (cmpfunc)Text_compare,                /* tp_compare */
  (reprfunc)Text_repr,                  /* tp_repr */
  0,                                    /* tp_as_number */
  0,                                    /* tp_as_sequence */
  0,                                    /* tp_as_mapping */
  0,                                    /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                    /* tp_doc */ 
  0,0,0,0,0,0,
  BPy_Text_methods,                     /* tp_methods */
  0,                                    /* tp_members */
};

/*****************************************************************************/
/* Function:              M_Text_New                                         */
/* Python equivalent:     Blender.Text.New                                   */
/*****************************************************************************/
static PyObject *M_Text_New(PyObject *self, PyObject *args, PyObject *keywords)
{
  char *name = NULL;
  char buf[21];
  int follow = 0;
  Text     *bl_text; /* blender text object */
  PyObject *py_text; /* python wrapper */

  if (!PyArg_ParseTuple(args, "|si", &name, &follow))
        return EXPP_ReturnPyObjError (PyExc_AttributeError,
          "expected string and int arguments (or nothing)");

  bl_text = add_empty_text();

  if (bl_text) {
    /* do not set user count because Text is already linked */

    /* create python wrapper obj */
    py_text = Text_CreatePyObject (bl_text);
  }
  else
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
                    "couldn't create Text Object in Blender");
  if (!py_text)
    return EXPP_ReturnPyObjError (PyExc_MemoryError,
                    "couldn't create Text Object wrapper");

  if (follow) bl_text->flags |= EXPP_TEXT_MODE_FOLLOW;

  if (name) {
    PyOS_snprintf(buf, sizeof(buf), "%s", name);
    rename_id(&bl_text->id, buf);
  }

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
static PyObject *M_Text_Get(PyObject *self, PyObject *args)
{
  char *name = NULL;
  Text *txt_iter;

  if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument (or nothing)"));

  txt_iter = G.main->text.first;

  if (name) { /* (name) - Search text by name */

    PyObject *wanted_txt = NULL;

    while ((txt_iter) && (wanted_txt == NULL)) {

      if (strcmp (name, txt_iter->id.name+2) == 0) {
        wanted_txt = Text_CreatePyObject (txt_iter);
      }

      txt_iter = txt_iter->id.next;
    }

    if (wanted_txt == NULL) { /* Requested text doesn't exist */
      char error_msg[64];
      PyOS_snprintf(error_msg, sizeof(error_msg),
                      "Text \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }

    return wanted_txt;
  }

  else { /* () - return a list of all texts in the scene */
    int index = 0;
    PyObject *txtlist, *pyobj;

    txtlist = PyList_New (BLI_countlist (&(G.main->text)));

    if (txtlist == NULL)
      return (EXPP_ReturnPyObjError (PyExc_MemoryError,
              "couldn't create PyList"));

    while (txt_iter) {
      pyobj = Text_CreatePyObject(txt_iter);

      if (!pyobj)
        return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                   "couldn't create PyString"));

      PyList_SET_ITEM (txtlist, index, pyobj);

      txt_iter = txt_iter->id.next;
      index++;
    }

    return (txtlist);
  }
}

/*****************************************************************************/
/* Function:              M_Text_Load                                        */
/* Python equivalent:     Blender.Text.Load                                  */
/* Description:           Receives a filename and returns the text object    */
/*                        created from the corresponding file.               */
/*****************************************************************************/
static PyObject *M_Text_Load(PyObject *self, PyObject *args)
{
  char   *fname;
  Text   *txt_ptr;
  BPy_Text *txt;

  if (!PyArg_ParseTuple(args, "s", &fname))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument"));
  
  txt = (BPy_Text *)PyObject_NEW(BPy_Text, &Text_Type);

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

/*****************************************************************************/
/* Function:              M_Text_unlink                                      */
/* Python equivalent:     Blender.Text.unlink                                */
/* Description:           Removes the given Text object from Blender         */
/*****************************************************************************/
static PyObject *M_Text_unlink(PyObject *self, PyObject *args)
{
  BPy_Text *textobj;
  Text *text;

  if (!PyArg_ParseTuple(args, "O!", &Text_Type, &textobj))
    return EXPP_ReturnPyObjError (PyExc_TypeError,
        "expected a Text object as argument");

  text = ((BPy_Text *)textobj)->text;

  if (!text)
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
        "this text was already unlinked!");

  BPY_clear_bad_scriptlinks(text);
  free_text_controllers(text);
  unlink_text(text);

  free_libblock(&G.main->text, text);

  ((BPy_Text *)textobj)->text = NULL;

  Py_INCREF(Py_None);     
  return Py_None;
}

/*****************************************************************************/
/* Function:              Text_Init                                          */
/*****************************************************************************/
PyObject *Text_Init (void)
{
  PyObject  *submodule;

  Text_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3("Blender.Text", M_Text_methods, M_Text_doc);

  return (submodule);
}

/*****************************************************************************/
/* Function:              Text_CreatePyObject                                */
/*****************************************************************************/
PyObject *Text_CreatePyObject (Text *txt)
{
  BPy_Text *pytxt;

  pytxt = (BPy_Text *)PyObject_NEW (BPy_Text, &Text_Type);

  if (!pytxt)
    return EXPP_ReturnPyObjError (PyExc_MemoryError,
             "couldn't create BPy_Text PyObject");

  pytxt->text = txt;

  return (PyObject *)pytxt;
}

/*****************************************************************************/
/* Python BPy_Text methods:                                                  */
/*****************************************************************************/
static PyObject *Text_getName(BPy_Text *self)
{
  PyObject *attr = PyString_FromString(self->text->id.name+2);

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Text.name attribute");
}

static PyObject *Text_getFilename(BPy_Text *self)
{
  PyObject *attr;
	char *name = self->text->name;
	
	if (name) attr = PyString_FromString(self->text->name);
	else
		attr = Py_None;

  if (attr) return attr;

  return EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Text.filename attribute");
}

static PyObject *Text_getNLines(BPy_Text *self)
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

static PyObject *Text_setName(BPy_Text *self, PyObject *args)
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

static PyObject *Text_clear(BPy_Text *self, PyObject *args)
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

static PyObject *Text_set(BPy_Text *self, PyObject *args)
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

static PyObject *Text_write(BPy_Text *self, PyObject *args)
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

static PyObject *Text_asLines(BPy_Text *self, PyObject *args)
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
/* Function:    Text_dealloc                                                 */
/* Description: This is a callback function for the BPy_Text type. It is     */
/*              the destructor function.                                     */
/*****************************************************************************/
static void Text_dealloc (BPy_Text *self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    Text_getAttr                                                 */
/* Description: This is a callback function for the BPy_Text type. It is     */
/*              the function that accesses BPy_Text member variables and     */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *Text_getAttr (BPy_Text *self, char *name)
{
  PyObject *attr = Py_None;

  if (!self->text || !Text_IsLinked(self))
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
       "Text was already deleted!");

  if (strcmp(name, "name") == 0)
    attr = PyString_FromString(self->text->id.name+2);
  else if (strcmp(name, "filename") == 0)
    return Text_getFilename(self); /* special: can be null */
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
  return Py_FindMethod(BPy_Text_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    Text_setAttr                                                 */
/* Description: This is a callback function for the BPy_Text type. It is the */
/*              function that changes Text Data members values. If this      */
/*              data is linked to a Blender Text, it also gets updated.      */
/*****************************************************************************/
static int Text_setAttr (BPy_Text *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;

  if (!self->text || !Text_IsLinked(self))
    return EXPP_ReturnIntError (PyExc_RuntimeError,
       "Text was already deleted!");

/* We're playing a trick on the Python API users here.  Even if they use
 * Text.member = val instead of Text.setMember(value), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Text structure when necessary. */

  valtuple = Py_BuildValue("(O)", value);/* the set* functions expect a tuple */

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
/* Function:    Text_compare                                                 */
/* Description: This is a callback function for the BPy_Text type. It        */
/*              compares two Text_Type objects. Only the "==" and "!="       */
/*              comparisons are meaninful. Returns 0 for equality and -1 if  */
/*              they don't point to the same Blender Text struct.            */
/*              In Python it becomes 1 if they are equal, 0 otherwise.       */
/*****************************************************************************/
static int Text_compare (BPy_Text *a, BPy_Text *b)
{
  Text *pa = a->text, *pb = b->text;
  return (pa == pb) ? 0:-1;
}

/*****************************************************************************/
/* Function:    Text_repr                                                    */
/* Description: This is a callback function for the BPy_Text type. It        */
/*              builds a meaninful string to represent text objects.         */
/*****************************************************************************/
static PyObject *Text_repr (BPy_Text *self)
{
  if (self->text && Text_IsLinked(self))
    return PyString_FromFormat("[Text \"%s\"]", self->text->id.name+2);
  else
    return PyString_FromString("[Text <deleted>]");
}

/* Internal function to confirm if a Text wasn't unlinked.
 * This is necessary because without it, if a script writer
 * referenced an already unlinked Text obj, Blender would crash. */ 
static int Text_IsLinked(BPy_Text *self)
{
  Text *txt_iter = G.main->text.first;

  while (txt_iter) {
    if (self->text == txt_iter) return 1; /* ok, still linked */

    txt_iter = txt_iter->id.next;
  }
/* uh-oh, it was already deleted */
	self->text = NULL; /* so we invalidate the pointer */
  return 0;
}
