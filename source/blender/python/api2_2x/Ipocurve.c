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

#include "Ipocurve.h"

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python API function prototypes for the IpoCurve module.                   */
/*****************************************************************************/
static PyObject *M_IpoCurve_New (PyObject * self, PyObject * args);
static PyObject *M_IpoCurve_Get (PyObject * self, PyObject * args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.IpoCurve.__doc__                                                  */
/*****************************************************************************/
char M_IpoCurve_doc[] = "";
char M_IpoCurve_New_doc[] = "";
char M_IpoCurve_Get_doc[] = "";

/*****************************************************************************/
/* Python method structure definition for Blender.IpoCurve module:           */
/*****************************************************************************/

struct PyMethodDef M_IpoCurve_methods[] = {
  {"New", (PyCFunction) M_IpoCurve_New, METH_VARARGS | METH_KEYWORDS,
   M_IpoCurve_New_doc},
  {"Get", M_IpoCurve_Get, METH_VARARGS, M_IpoCurve_Get_doc},
  {"get", M_IpoCurve_Get, METH_VARARGS, M_IpoCurve_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python C_IpoCurve methods declarations:                                   */
/*****************************************************************************/
static PyObject *IpoCurve_getName (C_IpoCurve * self);
static PyObject *IpoCurve_Recalc (C_IpoCurve * self);
static PyObject *IpoCurve_setName (C_IpoCurve * self, PyObject * args);
static PyObject *IpoCurve_addBezier (C_IpoCurve * self, PyObject * args);
static PyObject *IpoCurve_setInterpolation (C_IpoCurve * self,
					    PyObject * args);
static PyObject *IpoCurve_getInterpolation (C_IpoCurve * self);
static PyObject *IpoCurve_setExtrapolation (C_IpoCurve * self,
					    PyObject * args);
static PyObject *IpoCurve_getExtrapolation (C_IpoCurve * self);
static PyObject *IpoCurve_getPoints (C_IpoCurve * self);

/*****************************************************************************/
/* Python C_IpoCurve methods table:                                          */
/*****************************************************************************/
static PyMethodDef C_IpoCurve_methods[] = {
  /* name, method, flags, doc */
  {"getName", (PyCFunction) IpoCurve_getName, METH_NOARGS,
   "() - Return IpoCurve Data name"},
  {"Recalc", (PyCFunction) IpoCurve_Recalc, METH_NOARGS,
   "() - Return IpoCurve Data name"},
  {"update", (PyCFunction) IpoCurve_Recalc, METH_NOARGS,
   "() - Return IpoCurve Data name"},
  {"setName", (PyCFunction) IpoCurve_setName, METH_VARARGS,
   "(str) - Change IpoCurve Data name"},
  {"addBezier", (PyCFunction) IpoCurve_addBezier, METH_VARARGS,
   "(str) - Change IpoCurve Data name"},
  {"setInterpolation", (PyCFunction) IpoCurve_setInterpolation, METH_VARARGS,
   "(str) - Change IpoCurve Data name"},
  {"getInterpolation", (PyCFunction) IpoCurve_getInterpolation, METH_NOARGS,
   "(str) - Change IpoCurve Data name"},
  {"setExtrapolation", (PyCFunction) IpoCurve_setExtrapolation, METH_VARARGS,
   "(str) - Change IpoCurve Data name"},
  {"getExtrapolation", (PyCFunction) IpoCurve_getExtrapolation, METH_NOARGS,
   "(str) - Change IpoCurve Data name"},
  {"getPoints", (PyCFunction) IpoCurve_getPoints, METH_NOARGS,
   "(str) - Change IpoCurve Data name"},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python IpoCurve_Type callback function prototypes:                        */
/*****************************************************************************/
static void IpoCurveDeAlloc (C_IpoCurve * self);
//static int IpoCurvePrint (C_IpoCurve *self, FILE *fp, int flags);
static int IpoCurveSetAttr (C_IpoCurve * self, char *name, PyObject * v);
static PyObject *IpoCurveGetAttr (C_IpoCurve * self, char *name);
static PyObject *IpoCurveRepr (C_IpoCurve * self);

/*****************************************************************************/
/* Python IpoCurve_Type structure definition:                                */
/*****************************************************************************/
PyTypeObject IpoCurve_Type = {
  PyObject_HEAD_INIT (NULL)	/* required macro */
    0,				/* ob_size */
  "IpoCurve",			/* tp_name */
  sizeof (C_IpoCurve),		/* tp_basicsize */
  0,				/* tp_itemsize */
  /* methods */
  (destructor) IpoCurveDeAlloc,	/* tp_dealloc */
  0,				/* tp_print */
  (getattrfunc) IpoCurveGetAttr,	/* tp_getattr */
  (setattrfunc) IpoCurveSetAttr,	/* tp_setattr */
  0,				/* tp_compare */
  (reprfunc) IpoCurveRepr,	/* tp_repr */
  0,				/* tp_as_number */
  0,				/* tp_as_sequence */
  0,				/* tp_as_mapping */
  0,				/* tp_as_hash */
  0, 0, 0, 0, 0, 0,
  0,				/* tp_doc */
  0, 0, 0, 0, 0, 0,
  C_IpoCurve_methods,		/* tp_methods */
  0,				/* tp_members */
};

/*****************************************************************************/
/* Function:              M_IpoCurve_New                                          */
/* Python equivalent:     Blender.IpoCurve.New                                    */
/*****************************************************************************/
static PyObject *
M_IpoCurve_New (PyObject * self, PyObject * args)
{


  return 0;
}

/*****************************************************************************/
/* Function:              Ipo_Init                                           */
/*****************************************************************************/
PyObject *
IpoCurve_Init (void)
{
  PyObject *submodule;

  IpoCurve_Type.ob_type = &PyType_Type;

  submodule =
    Py_InitModule3 ("Blender.IpoCurve", M_IpoCurve_methods, M_IpoCurve_doc);

  return (submodule);
}

/*****************************************************************************/
/* Function:              M_IpoCurve_Get                                     */
/* Python equivalent:     Blender.IpoCurve.Get                               */
/* Description:           Receives a string and returns the ipo data obj     */
/*                        whose name matches the string.  If no argument is  */
/*                           passed in, a list of all ipo data names in the  */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *
M_IpoCurve_Get (PyObject * self, PyObject * args)
{
  return 0;
}

/*****************************************************************************/
/* Python C_IpoCurve methods:                                                */
/*****************************************************************************/

static PyObject *
IpoCurve_setInterpolation (C_IpoCurve * self, PyObject * args)
{
  char *interpolationtype = 0;
  int id = -1;
  if (!PyArg_ParseTuple (args, "s", &interpolationtype))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string argument"));
  if (!strcmp (interpolationtype, "Bezier"))
    id = IPO_BEZ;
  if (!strcmp (interpolationtype, "Constant"))
    id = IPO_CONST;
  if (!strcmp (interpolationtype, "Linear"))
    id = IPO_LIN;
  if (id == -1)
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "bad interpolation type"));

  self->ipocurve->ipo = id;
  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
IpoCurve_getInterpolation (C_IpoCurve * self)
{
  char *str = 0;
  IpoCurve *icu = self->ipocurve;
  if (icu->ipo == IPO_BEZ)
    str = "Bezier";
  if (icu->ipo == IPO_CONST)
    str = "Bonstant";
  if (icu->ipo == IPO_LIN)
    str = "Linear";

  if (!str)
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "unknown interpolation type"));
  return PyString_FromString (str);
}

static PyObject *
IpoCurve_setExtrapolation (C_IpoCurve * self, PyObject * args)
{

  char *extrapolationtype = 0;
  int id = -1;
  if (!PyArg_ParseTuple (args, "s", &extrapolationtype))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string argument"));
  if (!strcmp (extrapolationtype, "Constant"))
    id = 0;
  if (!strcmp (extrapolationtype, "Extrapolation"))
    id = 1;
  if (!strcmp (extrapolationtype, "Cyclic"))
    id = 2;
  if (!strcmp (extrapolationtype, "Cyclic_extrapolation"))
    id = 3;

  if (id == -1)
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "bad interpolation type"));
  self->ipocurve->extrap = id;
  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
IpoCurve_getExtrapolation (C_IpoCurve * self)
{
  char *str = 0;
  IpoCurve *icu = self->ipocurve;
  if (icu->extrap == 0)
    str = "Constant";
  if (icu->extrap == 1)
    str = "Extrapolation";
  if (icu->extrap == 2)
    str = "Cyclic";
  if (icu->extrap == 3)
    str = "Cyclic_extrapolation";

  return PyString_FromString (str);
}

static PyObject *
IpoCurve_addBezier (C_IpoCurve * self, PyObject * args)
{
  short MEM_freeN (void *vmemh);
  void *MEM_mallocN (unsigned int len, char *str);
  float x, y;
  int npoints;
  IpoCurve *icu;
  BezTriple *bzt, *tmp;
  static char name[10] = "mlml";
  PyObject *popo = 0;
  if (!PyArg_ParseTuple (args, "O", &popo))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected tuple argument"));

  x = PyFloat_AsDouble (PyTuple_GetItem (popo, 0));
  y = PyFloat_AsDouble (PyTuple_GetItem (popo, 1));
  icu = self->ipocurve;
  npoints = icu->totvert;
  tmp = icu->bezt;
  icu->bezt = MEM_mallocN (sizeof (BezTriple) * (npoints + 1), name);
  if (tmp)
    {
      memmove (icu->bezt, tmp, sizeof (BezTriple) * npoints);
      MEM_freeN (tmp);
    }
  memmove (icu->bezt + npoints, icu->bezt, sizeof (BezTriple));
  icu->totvert++;
  bzt = icu->bezt + npoints;
  bzt->vec[0][0] = x - 1;
  bzt->vec[1][0] = x;
  bzt->vec[2][0] = x + 1;
  bzt->vec[0][1] = y - 1;
  bzt->vec[1][1] = y;
  bzt->vec[2][1] = y + 1;
  /* set handle type to Auto */
  bzt->h1 = HD_AUTO;
  bzt->h2 = HD_AUTO;

  Py_INCREF (Py_None);
  return Py_None;
}


static PyObject *
IpoCurve_setName (C_IpoCurve * self, PyObject * args)
{
  return 0;
}


static PyObject *
IpoCurve_Recalc (C_IpoCurve * self)
{
  void testhandles_ipocurve (IpoCurve * icu);
  IpoCurve *icu = self->ipocurve;
  testhandles_ipocurve (icu);
  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
IpoCurve_getName (C_IpoCurve * self)
{
  char *nametab[24] =
    { "LocX", "LocY", "LocZ", "dLocX", "dLocY", "dLocZ", "RotX", "RotY",
    "RotZ", "dRotX", "dRotY", "dRotZ", "SizeX", "SizeY", "SizeZ", "dSizeX",
    "dSizeY",
    "dSizeZ", "Layer", "Time", "ColR", "ColG", "ColB", "ColA"
  };

  if (self->ipocurve->blocktype != ID_OB)
    return EXPP_ReturnPyObjError (PyExc_TypeError,
				  "This function doesn't support this ipocurve type yet");

  //      printf("IpoCurve_getName %d\n",self->ipocurve->vartype);
  if (self->ipocurve->adrcode <= 0)
    return PyString_FromString ("Index too small");
  if (self->ipocurve->adrcode >= 25)
    return PyString_FromString ("Index too big");

  return PyString_FromString (nametab[self->ipocurve->adrcode - 1]);

}


static void
IpoCurveDeAlloc (C_IpoCurve * self)
{
  PyObject_DEL (self);
}

static PyObject *
IpoCurve_getPoints (C_IpoCurve * self)
{
  struct BezTriple *bezt;
  PyObject *po;

  PyObject *list = PyList_New (0);
  int i;

  for (i = 0; i < self->ipocurve->totvert; i++)
    {
      bezt = self->ipocurve->bezt + i;
      po = BezTriple_CreatePyObject (bezt);
#if 0
      if (BezTriple_CheckPyObject (po))
	printf ("po is ok\n");
      else
	printf ("po is hosed\n");
#endif
      PyList_Append (list, po);
      /*
         PyList_Append( list, BezTriple_CreatePyObject(bezt));
       */
    }
  return list;
}


int
IpoCurve_setPoints (C_IpoCurve * self, PyObject * value)
{
  struct BezTriple *bezt;
  PyObject *l = PyList_New (0);
  int i;
  for (i = 0; i < self->ipocurve->totvert; i++)
    {
      bezt = self->ipocurve->bezt + i;
      PyList_Append (l, BezTriple_CreatePyObject (bezt));
    }
  return 0;
}


/*****************************************************************************/
/* Function:    IpoCurveGetAttr                                                   */
/* Description: This is a callback function for the C_IpoCurve type. It is        */
/*              the function that accesses C_IpoCurve "member variables" and      */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *
IpoCurveGetAttr (C_IpoCurve * self, char *name)
{
  if (strcmp (name, "bezierPoints") == 0)
    return IpoCurve_getPoints (self);
  if (strcmp (name, "name") == 0)
    return IpoCurve_getName (self);
  return Py_FindMethod (C_IpoCurve_methods, (PyObject *) self, name);
}

/*****************************************************************************/
/* Function:    IpoCurveSetAttr                                                */
/* Description: This is a callback function for the C_IpoCurve type. It is the */
/*              function that sets IpoCurve Data attributes (member variables).*/
/*****************************************************************************/
static int
IpoCurveSetAttr (C_IpoCurve * self, char *name, PyObject * value)
{
  if (strcmp (name, "bezierPoints") == 0)
    return IpoCurve_setPoints (self, value);
  return 0;			/* normal exit */
}

/*****************************************************************************/
/* Function:    IpoCurveRepr                                                      */
/* Description: This is a callback function for the C_IpoCurve type. It           */
/*              builds a meaninful string to represent ipo objects.          */
/*****************************************************************************/
static PyObject *
IpoCurveRepr (C_IpoCurve * self)
{
  void GetIpoCurveName (IpoCurve * icu, char *s);
  char s[100], s1[100];
  GetIpoCurveName (self->ipocurve, s1);
  sprintf (s, "IpoCurve %s \n", s1);
  return PyString_FromString (s);
}

/* Three Python IpoCurve_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    IpoCurve_CreatePyObject                                           */
/* Description: This function will create a new C_IpoCurve from an existing       */
/*              Blender ipo structure.                                       */
/*****************************************************************************/
PyObject *
IpoCurve_CreatePyObject (IpoCurve * ipo)
{
  C_IpoCurve *pyipo;

  pyipo = (C_IpoCurve *) PyObject_NEW (C_IpoCurve, &IpoCurve_Type);

  if (!pyipo)
    return EXPP_ReturnPyObjError (PyExc_MemoryError,
				  "couldn't create C_IpoCurve object");

  pyipo->ipocurve = ipo;

  return (PyObject *) pyipo;
}

/*****************************************************************************/
/* Function:    IpoCurve_CheckPyObject                                            */
/* Description: This function returns true when the given PyObject is of the */
/*              type IpoCurve. Otherwise it will return false.                    */
/*****************************************************************************/
int
IpoCurve_CheckPyObject (PyObject * pyobj)
{
  return (pyobj->ob_type == &IpoCurve_Type);
}

/*****************************************************************************/
/* Function:    IpoCurve_FromPyObject                                             */
/* Description: This function returns the Blender ipo from the given         */
/*              PyObject.                                                    */
/*****************************************************************************/
IpoCurve *
IpoCurve_FromPyObject (PyObject * pyobj)
{
  return ((C_IpoCurve *) pyobj)->ipocurve;
}
