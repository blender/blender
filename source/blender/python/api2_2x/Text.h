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

#ifndef EXPP_TEXT_H
#define EXPP_TEXT_H

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
static int Text_print (BPy_Text *self, FILE *fp, int flags);
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
  (printfunc)Text_print,                /* tp_print */
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

static int Text_IsLinked(BPy_Text *self);

#endif /* EXPP_TEXT_H */
