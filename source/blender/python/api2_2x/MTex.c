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
 * Inc., 59 Temple Place - Suite 330, Boston, MA    02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Alex Mole
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <BKE_texture.h>
#include <BKE_utildefines.h>

#include "MTex.h"
#include "Texture.h"
#include "constant.h"
#include "gen_utils.h"
#include "modules.h"


/*****************************************************************************/
/* Python BPy_MTex methods declarations:                                     */
/*****************************************************************************/
static PyObject *MTex_setTex(BPy_MTex *self, PyObject *args);

/*****************************************************************************/
/* Python method structure definition for Blender.Texture.MTex module:       */
/*****************************************************************************/
struct PyMethodDef M_MTex_methods[] = {
    {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_MTex methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_MTex_methods[] = {
 /* name, method, flags, doc */
  {"setTex", (PyCFunction)MTex_setTex, METH_VARARGS,
                            "(i) - Set MTex Texture"},
  {NULL, NULL,0, NULL}
};

/*****************************************************************************/
/* Python MTex_Type callback function prototypes:                            */
/*****************************************************************************/
static void MTex_dealloc (BPy_MTex *self);
static int MTex_setAttr (BPy_MTex *self, char *name, PyObject *v);
static int MTex_compare (BPy_MTex *a, BPy_MTex *b);
static PyObject *MTex_getAttr (BPy_MTex *self, char *name);
static PyObject *MTex_repr (BPy_MTex *self);


/*****************************************************************************/
/* Python MTex_Type structure definition:                                    */
/*****************************************************************************/
PyTypeObject MTex_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /* ob_size */
    "Blender MTex",                 /* tp_name */
    sizeof (BPy_MTex),              /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */
    (destructor)MTex_dealloc,       /* tp_dealloc */
    0,                              /* tp_print */
    (getattrfunc)MTex_getAttr,      /* tp_getattr */
    (setattrfunc)MTex_setAttr,      /* tp_setattr */
    (cmpfunc)MTex_compare,          /* tp_compare */
    (reprfunc)MTex_repr,            /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_as_hash */
    0,0,0,0,0,0,
    0,                              /* tp_doc */ 
    0,0,0,0,0,0,
    0,                              /* tp_methods */
    0,                              /* tp_members */
};


PyObject *MTex_Init (void)
{
    PyObject *submodule;
    
    MTex_Type.ob_type = &PyType_Type;

    submodule = Py_InitModule("Blender.Texture.MTex", M_MTex_methods);
    
    return submodule;
}

PyObject *MTex_CreatePyObject (MTex *mtex)
{
    BPy_MTex *pymtex;

    pymtex = (BPy_MTex *) PyObject_NEW (BPy_MTex, &MTex_Type);
    if (!pymtex)
        return EXPP_ReturnPyObjError (PyExc_MemoryError,
                                  "couldn't create BPy_MTex PyObject");

    pymtex->mtex = mtex;
    return (PyObject *) pymtex;
}

MTex *MTex_FromPyObject (PyObject *pyobj)
{
    return ((BPy_MTex *)pyobj)->mtex;
}


int MTex_CheckPyObject (PyObject *pyobj)
{
    return (pyobj->ob_type == &MTex_Type);
}


/*****************************************************************************/
/* Python BPy_MTex methods:                                                  */
/*****************************************************************************/

static PyObject *MTex_setTex(BPy_MTex *self, PyObject *args)
{
    BPy_Texture *pytex = NULL;
    if (!PyArg_ParseTuple(args, "O!", &Texture_Type, &pytex))
        return EXPP_ReturnPyObjError (PyExc_TypeError,
                                           "expected Texture argument");

    if (self->mtex->tex)
        self->mtex->tex->id.us--;
    
    self->mtex->tex = Texture_FromPyObject((PyObject*)pytex);

    Py_INCREF(Py_None);
    return Py_None;
}

static void MTex_dealloc (BPy_MTex *self)
{
    PyObject_DEL (self);
}

static PyObject *MTex_getAttr (BPy_MTex *self, char *name)
{
    if (STREQ(name, "tex"))
    {
        if (self->mtex->tex)
            return Texture_CreatePyObject (self->mtex->tex);
        else
        {
            Py_INCREF (Py_None);
            return Py_None;
        }        
    }
    else if (STREQ(name, "texco"))
        return PyInt_FromLong(self->mtex->texco);
    else if (STREQ(name, "mapto"))
        return PyInt_FromLong(self->mtex->mapto);
    
    else if (STREQ(name, "__members__"))
        return Py_BuildValue("[s,s,s]", "tex", "texco", "mapto");

    /* not an attribute, search the methods table */
    return Py_FindMethod(BPy_MTex_methods, (PyObject *)self, name);
}

static int MTex_setAttr (BPy_MTex *self, char *name, PyObject *value)
{
    PyObject *valtuple; 
    PyObject *error = NULL;

    /* Put "value" in a tuple, because we want to pass it to functions  *
     * that only accept PyTuples.                                       */
    valtuple = Py_BuildValue("(O)", value);
    if (!valtuple)
        return EXPP_ReturnIntError(PyExc_MemoryError,
                                "MTex_setAttr: couldn't create PyTuple");

    if (STREQ(name, "tex"))
        error = MTex_setTex(self, valtuple);
    else if (STREQ(name, "texco"))
    {
        if (PyInt_Check(value))
        {
            int texco = PyInt_AsLong(value);
            /* TODO: sanity-check this input! */
            self->mtex->texco = texco;
            Py_INCREF (Py_None); /* because we decref it below */
            error = Py_None;
        }            
    }
    else if (STREQ(name, "mapto"))
    {
        if (PyInt_Check(value))
        {
            int mapto = PyInt_AsLong(value);
            /* TODO: sanity-check this input! */
            self->mtex->mapto = mapto;
            Py_INCREF (Py_None); /* because we decref it below */
            error = Py_None;
        }            
    }
    
    else { 
        /* Error */
        Py_DECREF(valtuple);
        return EXPP_ReturnIntError (PyExc_KeyError, "attribute not found");
    }

    Py_DECREF (valtuple);

    if (error != Py_None) return -1;

    /* Py_None was INCREF'd by the set*() function, so we need to DECREF it */
    Py_DECREF (Py_None);
    
    return 0;
}

static int MTex_compare (BPy_MTex *a, BPy_MTex *b)
{
    return (a->mtex == b->mtex) ? 0 : -1;
}

static PyObject *MTex_repr (BPy_MTex *self)
{
    return PyString_FromFormat("[MTex]");
}

