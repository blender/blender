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
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* This file is the old bpython opy_matrix.c with minor modifications */

#include "vector.h"
#include "BLI_arithb.h"

static void Matrix_dealloc (MatrixObject *self)
{
    Py_DECREF (self->rows[0]);
    Py_DECREF (self->rows[1]);
    Py_DECREF (self->rows[2]);
    Py_DECREF (self->rows[3]);

    PyMem_DEL (self);
}

static PyObject * Matrix_getattr (MatrixObject *self, char *name)
{
    PyObject    * list;
    float         val[3];
    float         mat3[3][3];

    if (strcmp (name, "rot") == 0)
    {
        Mat3CpyMat4 (mat3, self->mat);
        Mat3ToEul (mat3, val);
    }
    else if (strcmp (name, "size") == 0)
    {
        Mat4ToSize (self->mat, val);
    }
    else if (strcmp (name, "loc") == 0)
    {
        VECCOPY (val, (float *)(self->mat)[3]);
    }
    else
    {
        return (EXPP_ReturnPyObjError (PyExc_AttributeError,
               "expected 'rot', 'size' or 'loc'"));
    }

    list = PyList_New (3);
    PyList_SetItem (list, 0, PyFloat_FromDouble (val[0]));
    PyList_SetItem (list, 1, PyFloat_FromDouble (val[1]));
    PyList_SetItem (list, 2, PyFloat_FromDouble (val[2]));

    return (list);
}

static int Matrix_setattr (MatrixObject *self, char *name, PyObject *v)
{
    /* This is not supported. */
    return (-1);
}

static PyObject * Matrix_repr (MatrixObject *self)
{
    return (EXPP_tuple_repr ((PyObject *) self, 4));
}

static PyObject * Matrix_item (MatrixObject *self, int i)
{
    if ((i<0) || (i>=4))
    {
        return (EXPP_ReturnPyObjError (PyExc_IndexError,
                "array index out of range"));
    }
    return (EXPP_incr_ret (self->rows[i]));
}

static PySequenceMethods Matrix_SeqMethods =
{
    (inquiry) 0,                /*sq_length*/
    (binaryfunc) 0,             /*sq_concat*/
    (intargfunc) 0,             /*sq_repeat*/
    (intargfunc) Matrix_item,   /*sq_item*/
    (intintargfunc) 0,          /*sq_slice*/
    (intobjargproc) 0,          /*sq_ass_item*/
    (intintobjargproc) 0,       /*sq_ass_slice*/
};

PyTypeObject Matrix_Type =
{
    PyObject_HEAD_INIT(NULL)
    0,                              /*ob_size*/
    "Matrix",                       /*tp_name*/
    sizeof(MatrixObject),           /*tp_basicsize*/
    0,                              /*tp_itemsize*/
    /* methods */
    (destructor)    Matrix_dealloc, /*tp_dealloc*/
    (printfunc)     0,              /*tp_print*/
    (getattrfunc)   Matrix_getattr, /*tp_getattr*/
    (setattrfunc)   Matrix_setattr, /*tp_setattr*/
    0,                              /*tp_compare*/
    (reprfunc)      Matrix_repr,    /*tp_repr*/
    0,                              /*tp_as_number*/
    &Matrix_SeqMethods,             /*tp_as_sequence*/
};

PyObject * newMatrixObject (Matrix4Ptr mat)
{
    MatrixObject    * self;

    self = PyObject_NEW (MatrixObject, &Matrix_Type);
    self->mat = mat;

    self->rows[0] = newVectorObject ((float *)(self->mat[0]), 4);
    self->rows[1] = newVectorObject ((float *)(self->mat[1]), 4);
    self->rows[2] = newVectorObject ((float *)(self->mat[2]), 4);
    self->rows[3] = newVectorObject ((float *)(self->mat[3]), 4);
    if ((self->rows[0] == NULL) ||
        (self->rows[1] == NULL) ||
        (self->rows[2] == NULL) ||
        (self->rows[3] == NULL))
    {
        return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                "Something wrong with creating a matrix object"));
    }

    return ((PyObject *)self);
}

void init_py_matrix (void)
{
    Matrix_Type.ob_type = &PyType_Type;
}
