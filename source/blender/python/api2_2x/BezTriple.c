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
 * Contributor(s): Jacques Guignot
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "BezTriple.h"

#include <Python.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <DNA_ipo_types.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python API function prototypes for the BezTriple module.                        */
/*****************************************************************************/
static PyObject *M_BezTriple_New (PyObject * self, PyObject * args);
static PyObject *M_BezTriple_Get (PyObject * self, PyObject * args);

/*****************************************************************************/
/* Python C_BezTriple instance methods declarations:                            */
/*****************************************************************************/
static PyObject *BezTriple_setPoints (C_BezTriple * self, PyObject * args);
static PyObject *BezTriple_getPoints (C_BezTriple * self);
static PyObject *BezTriple_getTriple (C_BezTriple * self);

/*****************************************************************************/
/* Python BezTriple_Type callback function prototypes:                          */
/*****************************************************************************/
static void BezTripleDeAlloc (C_BezTriple * self);
static int BezTripleSetAttr (C_BezTriple * self, char *name, PyObject * v);
static PyObject *BezTripleGetAttr (C_BezTriple * self, char *name);
static PyObject *BezTripleRepr (C_BezTriple * self);

/*****************************************************************************/
/* Python method structure definition for Blender.BezTriple module:             */
/*****************************************************************************/

struct PyMethodDef M_BezTriple_methods[] = {
  {"New", (PyCFunction) M_BezTriple_New, METH_VARARGS | METH_KEYWORDS, 0},
  {"Get", M_BezTriple_Get, METH_VARARGS, 0},
  {"get", M_BezTriple_Get, METH_VARARGS, 0},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_BezTriple methods table:                                            */
/*****************************************************************************/
static PyMethodDef C_BezTriple_methods[] = {
  /* name, method, flags, doc */
  {"setPoints", (PyCFunction) BezTriple_setPoints, METH_VARARGS,
   "(str) - Change BezTriple point coordinates"},
  {"getPoints", (PyCFunction) BezTriple_getPoints, METH_NOARGS,
   "() - return BezTriple knot point x and y coordinates"},
  {"getTriple", (PyCFunction) BezTriple_getTriple, METH_NOARGS,
   "() - return list of 3 floating point triplets.  order is H1, knot, H2"},
  {NULL, NULL, 0, NULL}
};


/*****************************************************************************/
/* Python BezTriple_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject BezTriple_Type = {
  PyObject_HEAD_INIT (NULL) 0,	/* ob_size */
  "BezTriple",			/* tp_name */
  sizeof (C_BezTriple),		/* tp_basicsize */
  0,				/* tp_itemsize */
  /* methods */
  (destructor) BezTripleDeAlloc,	/* tp_dealloc */
  0,				/* tp_print */
  (getattrfunc) BezTripleGetAttr,	/* tp_getattr */
  (setattrfunc) BezTripleSetAttr,	/* tp_setattr */
  0,				/* tp_compare */
  (reprfunc) BezTripleRepr,	/* tp_repr */
  0,				/* tp_as_number */
  0,				/* tp_as_sequence */
  0,				/* tp_as_mapping */
  0,				/* tp_as_hash */
  0, 0, 0, 0, 0, 0,
  0,				/* tp_doc */
  0, 0, 0, 0, 0, 0,
  C_BezTriple_methods,		/* tp_methods */
  0,				/* tp_members */
  0,				/* tm_getset */
  0
};


/*****************************************************************************/
/* Function:              M_BezTriple_New                                          */
/* Python equivalent:     Blender.BezTriple.New                                    */
/*****************************************************************************/
static PyObject *
M_BezTriple_New (PyObject * self, PyObject * args)
{
  return 0;
}

/*****************************************************************************/
/* Function:              M_BezTriple_Get                                       */
/* Python equivalent:     Blender.BezTriple.Get                                 */
/* Description:           Receives a string and returns the ipo data obj  */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all ipo data names in the  */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *
M_BezTriple_Get (PyObject * self, PyObject * args)
{
  return 0;
}

/*****************************************************************************/
/* Function:    BezTripleDeAlloc                                                   */
/* Description: This is a callback function for the C_BezTriple type. It is        */
/*              the destructor function.                                     */
/*****************************************************************************/
static void
BezTripleDeAlloc (C_BezTriple * self)
{
  PyObject_DEL (self);
}

static PyObject *
BezTriple_getPoints (C_BezTriple * self)
{
  struct BezTriple *bezt = self->beztriple;
  PyObject *l = PyList_New (0);
  int i;
  for (i = 0; i < 2; i++)
    {
      PyList_Append (l, PyFloat_FromDouble (bezt->vec[1][i]));
    }
  return l;
}


/*
 * BezTriple_getTriple
 * 
 * get the coordinate data for a BezTriple.
 *  returns a list of 3 points.
 * list order is handle1, knot, handle2.
 *  each point consists of a list of x,y,z float values.
 */

static PyObject *
BezTriple_getTriple (C_BezTriple * self)
{
  int i;
  struct BezTriple *bezt = self->beztriple;
  PyObject *retlist = PyList_New (0);
  PyObject *point;

  for (i = 0; i < 3; i++)
    {
      point = Py_BuildValue ("[fff]",
			     bezt->vec[i][0],
			     bezt->vec[i][1], bezt->vec[i][2]);

      PyList_Append (retlist, point);
    }

  return retlist;
}


static PyObject *
BezTriple_setPoints (C_BezTriple * self, PyObject * args)
{

  int i;
  struct BezTriple *bezt = self->beztriple;
  PyObject *popo = 0;
  if (!PyArg_ParseTuple (args, "O", &popo))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected tuple argument"));
  if (PySequence_Check (popo) == 0)
    {
      puts ("error in   BezTriple_setPoints");
      Py_INCREF (Py_None);
      return Py_None;
    }
  for (i = 0; i < 2; i++)
    {
      PyObject *o = PySequence_GetItem( popo, i );
      if( !o )
	printf("\n bad o. o no!\n");

	       /*      bezt->vec[1][i] = PyFloat_AsDouble (PyTuple_GetItem (popo, i));*/
      bezt->vec[1][i] = PyFloat_AsDouble ( o );
      bezt->vec[0][i] = bezt->vec[1][i] - 1;
      bezt->vec[2][i] = bezt->vec[1][i] + 1;
    }


  Py_INCREF (Py_None);
  return Py_None;
}


/*****************************************************************************/
/* Function:    BezTripleGetAttr                                                   */
/* Description: This is a callback function for the C_BezTriple type. It is        */
/*              the function that accesses C_BezTriple "member variables" and      */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *
BezTripleGetAttr (C_BezTriple * self, char *name)
{
  if (strcmp (name, "pt") == 0)
    return BezTriple_getPoints (self);
  else if (strcmp (name, "vec") == 0)
    return BezTriple_getTriple (self);

  /* look for default methods */
  return Py_FindMethod (C_BezTriple_methods, (PyObject *) self, name);
}

/*****************************************************************************/
/* Function:    BezTripleSetAttr                                                */
/* Description: This is a callback function for the C_BezTriple type. It is the */
/*              function that sets BezTriple Data attributes (member variables).*/
/*****************************************************************************/
static int
BezTripleSetAttr (C_BezTriple * self, char *name, PyObject * value)
{
  if (strcmp (name, "pt") == 0)
    BezTriple_setPoints (self, value);
  return 0;			/* normal exit */
}

/*****************************************************************************/
/* Function:    BezTripleRepr                                                */
/* Description: This is a callback function for the C_BezTriple type. It     */
/*              builds a meaninful string to represent  BezTriple objects.   */
/*****************************************************************************/
static PyObject *
BezTripleRepr (C_BezTriple * self)
{
  /*      float vec[3][3];
     float alfa;
     short s[3][2];
     short h1, h2;
     char f1, f2, f3, hide;
   */
  char str[1000];
  sprintf (str,
	   "BezTriple %f %f %f %f %f %f %f %f %f %f\n %d %d %d %d %d %d %d %d %d %d %d %d\n",
	   self->beztriple->vec[0][0], self->beztriple->vec[0][1],
	   self->beztriple->vec[0][2], self->beztriple->vec[1][0],
	   self->beztriple->vec[1][1], self->beztriple->vec[1][2],
	   self->beztriple->vec[2][0], self->beztriple->vec[2][1],
	   self->beztriple->vec[2][2], self->beztriple->alfa,
	   self->beztriple->s[0][0], self->beztriple->s[0][1],
	   self->beztriple->s[1][0], self->beztriple->s[1][1],
	   self->beztriple->s[2][0], self->beztriple->s[1][1],
	   self->beztriple->h1, self->beztriple->h2, self->beztriple->f1,
	   self->beztriple->f2, self->beztriple->f3, self->beztriple->hide);
  return PyString_FromString (str);
}

/* Three Python BezTriple_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    BezTriple_CreatePyObject                                           */
/* Description: This function will create a new C_BezTriple from an existing       */
/*              Blender ipo structure.                                       */
/*****************************************************************************/
PyObject *
BezTriple_CreatePyObject (BezTriple * bzt)
{
  C_BezTriple *pybeztriple;

  pybeztriple = (C_BezTriple *) PyObject_NEW (C_BezTriple, &BezTriple_Type);

  if (!pybeztriple)
    return EXPP_ReturnPyObjError (PyExc_MemoryError,
				  "couldn't create C_BezTriple object");

  pybeztriple->beztriple = bzt;

  return (PyObject *) pybeztriple;
}

/*****************************************************************************/
/* Function:    BezTriple_CheckPyObject                                            */
/* Description: This function returns true when the given PyObject is of the */
/*              type BezTriple. Otherwise it will return false.                    */
/*****************************************************************************/
int
BezTriple_CheckPyObject (PyObject * pyobj)
{
  return (pyobj->ob_type == &BezTriple_Type);
}

/*****************************************************************************/
/* Function:    BezTriple_FromPyObject                                             */
/* Description: This function returns the Blender beztriple from the given         */
/*              PyObject.                                                    */
/*****************************************************************************/
BezTriple *
BezTriple_FromPyObject (PyObject * pyobj)
{
  return ((C_BezTriple *) pyobj)->beztriple;
}
