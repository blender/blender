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
 * Contributor(s): Jacques Guignot, Nathan Letwory
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "Ipo.h"

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BSE_editipo.h>
#include <BLI_blenlib.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"


/* forward declarations */
char *GetIpoCurveName (IpoCurve * icu);

/*****************************************************************************/
/* Python API function prototypes for the Ipo module.                        */
/*****************************************************************************/
static PyObject *M_Ipo_New (PyObject * self, PyObject * args);
static PyObject *M_Ipo_Get (PyObject * self, PyObject * args);
static PyObject *M_Ipo_Recalc (PyObject * self, PyObject * args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Ipo.__doc__                                                       */
/*****************************************************************************/
char M_Ipo_doc[] = "";
char M_Ipo_New_doc[] = "";
char M_Ipo_Get_doc[] = "";


/*****************************************************************************/
/* Python method structure definition for Blender.Ipo module:             */
/*****************************************************************************/

struct PyMethodDef M_Ipo_methods[] = {
  {"New", (PyCFunction) M_Ipo_New, METH_VARARGS | METH_KEYWORDS,
   M_Ipo_New_doc},
  {"Get", M_Ipo_Get, METH_VARARGS, M_Ipo_Get_doc},
  {"get", M_Ipo_Get, METH_VARARGS, M_Ipo_Get_doc},
  {"Recalc", M_Ipo_Recalc, METH_VARARGS, M_Ipo_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Ipo methods declarations:                                     */
/*****************************************************************************/
static PyObject *Ipo_getName (BPy_Ipo * self);
static PyObject *Ipo_setName (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_getBlocktype (BPy_Ipo * self);
static PyObject *Ipo_setBlocktype (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_getRctf (BPy_Ipo * self);
static PyObject *Ipo_setRctf (BPy_Ipo * self, PyObject * args);

static PyObject *Ipo_getCurve (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_getCurves (BPy_Ipo * self);
static PyObject *Ipo_addCurve (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_getNcurves (BPy_Ipo * self);
static PyObject *Ipo_getNBezPoints (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_DeleteBezPoints (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_getCurveBP (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_getCurvecurval (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_EvaluateCurveOn (BPy_Ipo * self, PyObject * args);


static PyObject *Ipo_setCurveBeztriple (BPy_Ipo * self, PyObject * args);
static PyObject *Ipo_getCurveBeztriple (BPy_Ipo * self, PyObject * args);

/*****************************************************************************/
/* Python BPy_Ipo methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_Ipo_methods[] = {
  /* name, method, flags, doc */
  {"getName", (PyCFunction) Ipo_getName, METH_NOARGS,
   "() - Return Ipo Data name"},
  {"setName", (PyCFunction) Ipo_setName, METH_VARARGS,
   "(str) - Change Ipo Data name"},
  {"getBlocktype", (PyCFunction) Ipo_getBlocktype, METH_NOARGS,
   "() - Return Ipo blocktype -"},
  {"setBlocktype", (PyCFunction) Ipo_setBlocktype, METH_VARARGS,
   "(str) - Change Ipo blocktype"},
  {"getRctf", (PyCFunction) Ipo_getRctf, METH_NOARGS,
   "() - Return Ipo rctf - "},
  {"setRctf", (PyCFunction) Ipo_setRctf, METH_VARARGS,
   "(str) - Change Ipo rctf"},
  {"addCurve", (PyCFunction) Ipo_addCurve, METH_VARARGS,
   "() - Return Ipo ncurves"},
  {"getNcurves", (PyCFunction) Ipo_getNcurves, METH_NOARGS,
   "() - Return Ipo ncurves"},
  {"getNBezPoints", (PyCFunction) Ipo_getNBezPoints, METH_VARARGS,
   "() - Return curve number of Bez points"},
  {"delBezPoint", (PyCFunction) Ipo_DeleteBezPoints, METH_VARARGS,
   "() - Return curve number of Bez points"},
  {"getCurveBP", (PyCFunction) Ipo_getCurveBP, METH_VARARGS,
   "() - Return Ipo ncurves"},
  {"EvaluateCurveOn", (PyCFunction) Ipo_EvaluateCurveOn, METH_VARARGS,
   "() - Return curve value at given time"},
  {"getCurveCurval", (PyCFunction) Ipo_getCurvecurval, METH_VARARGS,
   "() - Return curval"},
  {"getCurveBeztriple", (PyCFunction) Ipo_getCurveBeztriple, METH_VARARGS,
   "() - Return Ipo ncurves"},
  {"setCurveBeztriple", (PyCFunction) Ipo_setCurveBeztriple, METH_VARARGS,
   "() - Return curval"},
  {"getCurves", (PyCFunction) Ipo_getCurves, METH_NOARGS,
   "() - Return curval"},
  {"getCurve", (PyCFunction) Ipo_getCurve, METH_VARARGS,
   "() - Return curval"},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python Ipo_Type callback function prototypes:                          */
/*****************************************************************************/
static void IpoDeAlloc (BPy_Ipo * self);
//static int IpoPrint (BPy_Ipo *self, FILE *fp, int flags);
static int IpoSetAttr (BPy_Ipo * self, char *name, PyObject * v);
static PyObject *IpoGetAttr (BPy_Ipo * self, char *name);
static PyObject *IpoRepr (BPy_Ipo * self);

/*****************************************************************************/
/* Python Ipo_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject Ipo_Type = {
  PyObject_HEAD_INIT (NULL) 0,	/* ob_size */
  "Ipo",			/* tp_name */
  sizeof (BPy_Ipo),		/* tp_basicsize */
  0,				/* tp_itemsize */
  /* methods */
  (destructor) IpoDeAlloc,	/* tp_dealloc */
  0,				/* tp_print */
  (getattrfunc) IpoGetAttr,	/* tp_getattr */
  (setattrfunc) IpoSetAttr,	/* tp_setattr */
  0,				/* tp_compare */
  (reprfunc) IpoRepr,		/* tp_repr */
  0,				/* tp_as_number */
  0,				/* tp_as_sequence */
  0,				/* tp_as_mapping */
  0,				/* tp_as_hash */
  0, 0, 0, 0, 0, 0,
  0,				/* tp_doc */
  0, 0, 0, 0, 0, 0,
  BPy_Ipo_methods,		/* tp_methods */
  0,				/* tp_members */
};

/*****************************************************************************/
/* Function:              M_Ipo_New                                          */
/* Python equivalent:     Blender.Ipo.New                                    */
/*****************************************************************************/

static PyObject *
M_Ipo_New (PyObject * self, PyObject * args)
{
  Ipo *add_ipo (char *name, int idcode);
  char *name = NULL, *code = NULL;
  int idcode = -1;
  BPy_Ipo *pyipo;
  Ipo *blipo;

  if (!PyArg_ParseTuple (args, "ss", &code, &name))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string string arguments"));

  if (!strcmp (code, "Object"))
    idcode = ID_OB;
  if (!strcmp (code, "Camera"))
    idcode = ID_CA;
  if (!strcmp (code, "World"))
    idcode = ID_WO;
  if (!strcmp (code, "Material"))
    idcode = ID_MA;
  if (!strcmp (code, "Texture"))
    idcode = ID_TE;
  if (!strcmp (code, "Lamp"))
    idcode = ID_LA;
  if (!strcmp (code, "Action"))
    idcode = ID_AC;
  if (!strcmp (code, "Constraint"))
    idcode = IPO_CO;
  if (!strcmp (code, "Sequence"))
    idcode = ID_SEQ;
  if (!strcmp (code, "Curve"))
    idcode = ID_CU;
  if (!strcmp (code, "Key"))
    idcode = ID_KE;

  if (idcode == -1)
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "Bad code"));


  blipo = add_ipo (name, idcode);

  if (blipo)
    {
      /* return user count to zero because add_ipo() inc'd it */
      blipo->id.us = 0;
      /* create python wrapper obj */
      pyipo = (BPy_Ipo *) PyObject_NEW (BPy_Ipo, &Ipo_Type);
    }
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				   "couldn't create Ipo Data in Blender"));

  if (pyipo == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				   "couldn't create Ipo Data object"));

  pyipo->ipo = blipo;

  return (PyObject *) pyipo;
}

/*****************************************************************************/
/* Function:              M_Ipo_Get                                       */
/* Python equivalent:     Blender.Ipo.Get                                 */
/* Description:           Receives a string and returns the ipo data obj  */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all ipo data names in the  */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *
M_Ipo_Get (PyObject * self, PyObject * args)
{
  char *name = NULL;
  Ipo *ipo_iter;
  PyObject *ipolist, *pyobj;
  BPy_Ipo *wanted_ipo = NULL;
  char error_msg[64];

  if (!PyArg_ParseTuple (args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
				   "expected string argument (or nothing)"));

  ipo_iter = G.main->ipo.first;

  if (name)
    {				/* (name) - Search ipo by name */
      while ((ipo_iter) && (wanted_ipo == NULL))
	{
	  if (strcmp (name, ipo_iter->id.name + 2) == 0)
	    {
	      wanted_ipo = (BPy_Ipo *) PyObject_NEW (BPy_Ipo, &Ipo_Type);
	      if (wanted_ipo)
		wanted_ipo->ipo = ipo_iter;
	    }
	  ipo_iter = ipo_iter->id.next;
	}

      if (wanted_ipo == NULL)
	{			/* Requested ipo doesn't exist */
	  PyOS_snprintf (error_msg, sizeof (error_msg),
			 "Ipo \"%s\" not found", name);
	  return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
	}

      return (PyObject *) wanted_ipo;
    }

  else
    {				/* () - return a list with all ipos in the scene */
      int index = 0;

      ipolist = PyList_New (BLI_countlist (&(G.main->ipo)));

      if (ipolist == NULL)
	return (EXPP_ReturnPyObjError (PyExc_MemoryError,
					 "couldn't create PyList"));

      while (ipo_iter)
	{
	  pyobj = Ipo_CreatePyObject (ipo_iter);

	  if (!pyobj)
	    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
					     "couldn't create PyString"));

	  PyList_SET_ITEM (ipolist, index, pyobj);

	  ipo_iter = ipo_iter->id.next;
	  index++;
	}

      return (ipolist);
    }
}


static PyObject *
M_Ipo_Recalc (PyObject * self, PyObject * args)
{
  void testhandles_ipocurve (IpoCurve * icu);
  PyObject *a;
  IpoCurve *icu;
  if (!PyArg_ParseTuple (args, "O", &a))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected ipo argument)"));
  icu = IpoCurve_FromPyObject (a);
  testhandles_ipocurve (icu);

  Py_INCREF (Py_None);
  return Py_None;

}

/*****************************************************************************/
/* Function:              Ipo_Init                                           */
/*****************************************************************************/
PyObject *
Ipo_Init (void)
{
  PyObject *submodule;

  Ipo_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3 ("Blender.Ipo", M_Ipo_methods, M_Ipo_doc);

  return (submodule);
}

/*****************************************************************************/
/* Python BPy_Ipo methods:                                                  */
/*****************************************************************************/
static PyObject *
Ipo_getName (BPy_Ipo * self)
{
  PyObject *attr = PyString_FromString (self->ipo->id.name + 2);

  if (attr)
    return attr;
  Py_INCREF (Py_None);
  return Py_None;
}


static PyObject *
Ipo_setName (BPy_Ipo * self, PyObject * args)
{
  char *name;
  char buf[21];

  if (!PyArg_ParseTuple (args, "s", &name))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string argument"));

  PyOS_snprintf (buf, sizeof (buf), "%s", name);

  rename_id (&self->ipo->id, buf);

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
Ipo_getBlocktype (BPy_Ipo * self)
{
  PyObject *attr = PyInt_FromLong (self->ipo->blocktype);
  if (attr)
    return attr;
  return (EXPP_ReturnPyObjError
	  (PyExc_RuntimeError, "couldn't get Ipo.blocktype attribute"));
}


static PyObject *
Ipo_setBlocktype (BPy_Ipo * self, PyObject * args)
{
  int blocktype = 0;

  if (!PyArg_ParseTuple (args, "i", &blocktype))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string argument"));

  self->ipo->blocktype = (short) blocktype;

  Py_INCREF (Py_None);
  return Py_None;
}


static PyObject *
Ipo_getRctf (BPy_Ipo * self)
{
  PyObject *l = PyList_New (0);
  PyList_Append (l, PyFloat_FromDouble (self->ipo->cur.xmin));
  PyList_Append (l, PyFloat_FromDouble (self->ipo->cur.xmax));
  PyList_Append (l, PyFloat_FromDouble (self->ipo->cur.ymin));
  PyList_Append (l, PyFloat_FromDouble (self->ipo->cur.ymax));
  return l;

}


static PyObject *
Ipo_setRctf (BPy_Ipo * self, PyObject * args)
{
  float v[4];
  if (!PyArg_ParseTuple (args, "ffff", v, v + 1, v + 2, v + 3))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected 4 float argument"));

  self->ipo->cur.xmin = v[0];
  self->ipo->cur.xmax = v[1];
  self->ipo->cur.ymin = v[2];
  self->ipo->cur.ymax = v[3];

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
Ipo_getNcurves (BPy_Ipo * self)
{
  int i = 0;

  IpoCurve *icu;
  for (icu = self->ipo->curve.first; icu; icu = icu->next)
    {
      i++;
    }

  return (PyInt_FromLong (i));
}

int Ipo_laIcuName(char * s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "Energy"))
    {
      *param = LA_ENERGY;
      ok = 1;
    }
  if (!strcmp (s, "R"))
    {
      *param = LA_COL_R;
      ok = 1;
    }
  if (!strcmp (s, "G"))
    {
      *param = LA_COL_G;
      ok = 1;
    }
  if (!strcmp (s, "B"))
    {
      *param = LA_COL_B;
      ok = 1;
    }
  if (!strcmp (s, "Dist"))
    {
      *param = LA_DIST;
      ok = 1;
    }
  if (!strcmp (s, "SpoSi"))
    {
      *param = LA_SPOTSI;
      ok = 1;
    }
  if (!strcmp (s, "SpoBl"))
    {
      *param = LA_SPOTBL;
      ok = 1;
    }
  if (!strcmp (s, "Quad1"))
    {
      *param = LA_QUAD1;
      ok = 1;
    }
  if (!strcmp (s, "Quad2"))
    {
      *param = LA_QUAD2;
      ok = 1;
    }
  if (!strcmp (s, "HaInt"))
    {
      *param = LA_HALOINT;
      ok = 1;
    }
  return ok;
}

int Ipo_woIcuName(char * s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "HorR"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "HorG"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "HorB"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "ZenR"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "ZenG"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "ZenB"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "Expos"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "Misi"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "MisDi"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "MisHi"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "StarR"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "StarB"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "StarG"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "ClSta"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "StarDi"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "StarSi"))
    {
      *param = CAM_END;
      ok = 1;
    }
  return ok;
}

int Ipo_maIcuName(char * s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "R"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "G"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "B"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "SpecR"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "SpecG"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "SpecB"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "MirR"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "MirG"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "MirB"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "Ref"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "Alpha"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "Emit"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "Amb"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "Spec"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "Hard"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "SpTra"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "Ior"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "Mode"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "HaSize"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "Translu"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "RayMir"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "FresMir"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "FresMirI"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "FresTra"))
    {
      *param = CAM_END;
      ok = 1;
    }
  if (!strcmp (s, "FresTraI"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "TraGlow"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  return ok;
}

int Ipo_keIcuName(char *s, int * param)
{
  char key[10];
  int ok = 0;
  int nr = 0;
  if (!strcmp (s, "Speed"))
    {
      *param = KEY_SPEED;
      ok = 1;
    }
  for(nr = 1; nr<64; nr++) {
    sprintf(key, "Key %d", nr);
    if (!strcmp (s, key)) {
      *param = nr;
      ok = 1;
      break;
    }
  }
  
  return ok;
}

int Ipo_seqIcuName(char *s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "Fac"))
    {
      *param = SEQ_FAC1;
      ok = 1;
    }

  return ok;
}

int Ipo_cuIcuName(char *s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "Speed"))
    {
      *param = CU_SPEED;
      ok = 1;
    }

  return ok;
}

int Ipo_coIcuName(char *s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "Inf"))
    {
      *param = CO_ENFORCE;
      ok = 1;
    }

  return ok;
}

int Ipo_acIcuName(char *s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "LocX"))
    {
      *param = AC_LOC_X;
      ok = 1;
    }
  if (!strcmp (s, "LocY"))
    {
      *param = AC_LOC_Y;
      ok = 1;
    }
  if (!strcmp (s, "LocZ"))
    {
      *param = AC_LOC_Z;
      ok = 1;
    }
  if (!strcmp (s, "SizeX"))
    {
      *param = AC_SIZE_X;
      ok = 1;
    }
  if (!strcmp (s, "SizeY"))
    {
      *param = AC_SIZE_Y;
      ok = 1;
    }
  if (!strcmp (s, "SizeZ"))
    {
      *param = AC_SIZE_Z;
      ok = 1;
    }
  if (!strcmp (s, "QuatX"))
    {
      *param = AC_QUAT_X;
      ok = 1;
    }
  if (!strcmp (s, "QuatY"))
    {
      *param = AC_QUAT_Y;
      ok = 1;
    }
  if (!strcmp (s, "QuatZ"))
    {
      *param = AC_QUAT_Z;
      ok = 1;
    }
  if (!strcmp (s, "QuatW"))
    {
      *param = AC_QUAT_W;
      ok = 1;
    }
  return ok;
}

int Ipo_caIcuName(char * s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "Lens"))
    {
      *param = CAM_LENS;
      ok = 1;
    }
  if (!strcmp (s, "ClSta"))
    {
      *param = CAM_STA;
      ok = 1;
    }
  if (!strcmp (s, "ClEnd"))
    {
      *param = CAM_END;
      ok = 1;
    }
  return ok;
}

int Ipo_texIcuName(char * s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "NSize"))
    {
      *param = TE_NSIZE;
      ok = 1;
    }
  if (!strcmp (s, "NDepth"))
    {
      *param = TE_NDEPTH;
      ok = 1;
    }
  if (!strcmp (s, "NType"))
    {
      *param = TE_NTYPE;
      ok = 1;
    }
  if (!strcmp (s, "Turb"))
    {
      *param = TE_TURB;
      ok = 1;
    }
  if (!strcmp (s, "Vnw1"))
    {
      *param = TE_VNW1;
      ok = 1;
    }
  if (!strcmp (s, "Vnw2"))
    {
      *param = TE_VNW2;
      ok = 1;
    }
  if (!strcmp (s, "Vnw3"))
    {
      *param = TE_VNW3;
      ok = 1;
    }
  if (!strcmp (s, "Vnw4"))
    {
      *param = TE_VNW4;
      ok = 1;
    }
  if (!strcmp (s, "MinkMExp"))
    {
      *param = TE_VNMEXP;
      ok = 1;
    }
  if (!strcmp (s, "DistM"))
    {
      *param = TE_VN_DISTM;
      ok = 1;
    }
  if (!strcmp (s, "ColT"))
    {
      *param = TE_VN_COLT;
      ok = 1;
    }
  if (!strcmp (s, "iScale"))
    {
      *param = TE_ISCA;
      ok = 1;
    }
  if (!strcmp (s, "DistA"))
    {
      *param = TE_DISTA;
      ok = 1;
    }
  if (!strcmp (s, "MgType"))
    {
      *param = TE_MG_TYP;
      ok = 1;
    }
  if (!strcmp (s, "MgH"))
    {
      *param = TE_MGH;
      ok = 1;
    }
  if (!strcmp (s, "Lacu"))
    {
      *param = TE_MG_LAC;
      ok = 1;
    }
  if (!strcmp (s, "Oct"))
    {
      *param = TE_MG_OCT;
      ok = 1;
    }
  if (!strcmp (s, "MgOff"))
    {
      *param = TE_MG_OFF;
      ok = 1;
    }
  if (!strcmp (s, "MgGain"))
    {
      *param = TE_MG_GAIN;
      ok = 1;
    }
  if (!strcmp (s, "NBase1"))
    {
      *param = TE_N_BAS1;
      ok = 1;
    }
  if (!strcmp (s, "NBase2"))
    {
      *param = TE_N_BAS2;
      ok = 1;
    }

  return ok;
}

int Ipo_obIcuName(char * s, int * param)
{
  int ok = 0;
  if (!strcmp (s, "LocX"))
    {
      *param = OB_LOC_X;
      ok = 1;
    }
  if (!strcmp (s, "LocY"))
    {
      *param = OB_LOC_Y;
      ok = 1;
    }
  if (!strcmp (s, "LocZ"))
    {
      *param = OB_LOC_Z;
      ok = 1;
    }
  if (!strcmp (s, "RotX"))
    {
      *param = OB_ROT_X;
      ok = 1;
    }
  if (!strcmp (s, "RotY"))
    {
      *param = OB_ROT_Y;
      ok = 1;
    }
  if (!strcmp (s, "RotZ"))
    {
      *param = OB_ROT_Z;
      ok = 1;
    }
  if (!strcmp (s, "SizeX"))
    {
      *param = OB_SIZE_X;
      ok = 1;
    }
  if (!strcmp (s, "SizeY"))
    {
      *param = OB_SIZE_Y;
      ok = 1;
    }
  if (!strcmp (s, "SizeZ"))
    {
      *param = OB_SIZE_Z;
      ok = 1;
    }

  if (!strcmp (s, "dLocX"))
    {
      *param = OB_DLOC_X;
      ok = 1;
    }
  if (!strcmp (s, "dLocY"))
    {
      *param = OB_DLOC_Y;
      ok = 1;
    }
  if (!strcmp (s, "dLocZ"))
    {
      *param = OB_DLOC_Z;
      ok = 1;
    }
  if (!strcmp (s, "dRotX"))
    {
      *param = OB_DROT_X;
      ok = 1;
    }
  if (!strcmp (s, "dRotY"))
    {
      *param = OB_DROT_Y;
      ok = 1;
    }
  if (!strcmp (s, "dRotZ"))
    {
      *param = OB_DROT_Z;
      ok = 1;
    }
  if (!strcmp (s, "dSizeX"))
    {
      *param = OB_DSIZE_X;
      ok = 1;
    }
  if (!strcmp (s, "dSizeY"))
    {
      *param = OB_DSIZE_Y;
      ok = 1;
    }
  if (!strcmp (s, "dSizeZ"))
    {
      *param = OB_DSIZE_Z;
      ok = 1;
    }

  if (!strcmp (s, "Layer"))
    {
      *param = OB_LAY;
      ok = 1;
    }
  if (!strcmp (s, "Time"))
    {
      *param = OB_TIME;
      ok = 1;
    }

  if (!strcmp (s, "ColR"))
    {
      *param = OB_COL_R;
      ok = 1;
    }
  if (!strcmp (s, "ColG"))
    {
      *param = OB_COL_G;
      ok = 1;
    }
  if (!strcmp (s, "ColB"))
    {
      *param = OB_COL_B;
      ok = 1;
    }
  if (!strcmp (s, "ColA"))
    {
      *param = OB_COL_A;
      ok = 1;
    }
  if (!strcmp (s, "FStreng"))
    {
      *param = OB_PD_FSTR;
      ok = 1;
    }
  if (!strcmp (s, "FFall"))
    {
      *param = OB_PD_FFALL;
      ok = 1;
    }
  if (!strcmp (s, "Damping"))
    {
      *param = OB_PD_SDAMP;
      ok = 1;
    }
  if (!strcmp (s, "RDamp"))
    {
      *param = OB_PD_RDAMP;
      ok = 1;
    }
  if (!strcmp (s, "Perm"))
    {
      *param = OB_PD_PERM;
      ok = 1;
    }

  return ok;
}

static PyObject *
Ipo_addCurve (BPy_Ipo * self, PyObject * args)
{
  IpoCurve *get_ipocurve (ID * from, short type, int adrcode, Ipo * useipo);
  void allspace (unsigned short event, short val);
  void allqueue (unsigned short event, short val);
  int param = 0, ok = 0, ipofound = 0;
  char *s = 0;
  Ipo *ipo = 0;
  IpoCurve *icu = 0;
  Link *link;

  if (!PyArg_ParseTuple (args, "s", &s))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string argument"));


  link = G.main->ipo.first;
  
  while(link)
    {
      ipo = (Ipo *)link;
      if(ipo == self->ipo) {
        ipofound = 1;
        break;
      }
      link = link->next;
    }

  if(ipo && ipofound) {
    switch(ipo->blocktype) {
      case ID_OB:
        ok = Ipo_obIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_CA:
        ok = Ipo_caIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_LA:
        ok = Ipo_laIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_TE:
        ok = Ipo_texIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_WO:
        ok = Ipo_woIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_MA:
        ok = Ipo_maIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_AC:
        ok = Ipo_acIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case IPO_CO:
        ok = Ipo_coIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_CU:
        ok = Ipo_cuIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_KE:
        ok = Ipo_keIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
      case ID_SEQ:
        ok = Ipo_seqIcuName(s, &param);
        icu = get_ipocurve (NULL, ipo->blocktype, param, self->ipo );
        break;
    }
  }

  if (icu==0) return (EXPP_ReturnPyObjError (PyExc_ValueError, "Not a valid param."));

#define REMAKEIPO		1
#define REDRAWIPO		0x4023
  allspace (REMAKEIPO, 0);
  allqueue (REDRAWIPO, 0);

  return IpoCurve_CreatePyObject (icu);
}



static PyObject *
Ipo_getCurve (BPy_Ipo * self, PyObject * args)
{
  char *str, *str1;
  IpoCurve *icu = 0;
  if (!PyArg_ParseTuple (args, "s", &str))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string argument"));
  for (icu = self->ipo->curve.first; icu; icu = icu->next)
    {
      str1 = GetIpoCurveName (icu);
      if (!strcmp (str1, str))
	return IpoCurve_CreatePyObject (icu);
    }

  Py_INCREF (Py_None);
  return Py_None;
}

char *
GetIpoCurveName(IpoCurve * icu)
{
  switch (icu->blocktype)
    {
    case ID_MA:
      {
	return getname_mat_ei (icu->adrcode);
      }
    case ID_WO:
      {
	return getname_world_ei (icu->adrcode);
      }
    case ID_CA:
      {
	return getname_cam_ei (icu->adrcode);
      }
    case ID_OB:
      {
	return getname_ob_ei (icu->adrcode, 1); /* solve: what if EffX/Y/Z are wanted? */
      }
    case ID_TE:
      {
	return getname_tex_ei (icu->adrcode);
      }
    case ID_LA:
      {
	return getname_la_ei(icu->adrcode);
      }
    case ID_AC:
      {
	return getname_ac_ei(icu->adrcode);
      }
    case ID_CU:
      {
	return getname_cu_ei(icu->adrcode);
      }
    case ID_KE:
      {
	return getname_key_ei(icu->adrcode);
      }
    case ID_SEQ:
      {
	return getname_seq_ei(icu->adrcode);
      }
    case IPO_CO:
      {
	return getname_co_ei(icu->adrcode);
      }
    }
    return NULL;
}

static PyObject *
Ipo_getCurves (BPy_Ipo * self)
{
  PyObject *attr = PyList_New (0);
  IpoCurve *icu;
  for (icu = self->ipo->curve.first; icu; icu = icu->next)
    {
      PyList_Append (attr, IpoCurve_CreatePyObject (icu));
    }
  return attr;
}


static PyObject *
Ipo_getNBezPoints (BPy_Ipo * self, PyObject * args)
{
  int num = 0, i = 0;
  IpoCurve *icu = 0;
  if (!PyArg_ParseTuple (args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  icu = self->ipo->curve.first;
  if (!icu)
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "No IPO curve"));
  for (i = 0; i < num; i++)
    {
      if (!icu)
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "Bad curve number"));
      icu = icu->next;

    }
  return (PyInt_FromLong (icu->totvert));
}

static PyObject *
Ipo_DeleteBezPoints (BPy_Ipo * self, PyObject * args)
{
  int num = 0, i = 0;
  IpoCurve *icu = 0;
  if (!PyArg_ParseTuple (args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  icu = self->ipo->curve.first;
  if (!icu)
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "No IPO curve"));
  for (i = 0; i < num; i++)
    {
      if (!icu)
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "Bad curve number"));
      icu = icu->next;

    }
  icu->totvert--;
  return (PyInt_FromLong (icu->totvert));
}


static PyObject *
Ipo_getCurveBP (BPy_Ipo * self, PyObject * args)
{
  struct BPoint *ptrbpoint;
  int num = 0, i;
  IpoCurve *icu;
  PyObject *l;

  if (!PyArg_ParseTuple (args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  icu = self->ipo->curve.first;
  if (!icu)
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "No IPO curve"));
  for (i = 0; i < num; i++)
    {
      if (!icu)
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "Bad curve number"));
      icu = icu->next;

    }
  ptrbpoint = icu->bp;
  if (!ptrbpoint)
    return EXPP_ReturnPyObjError (PyExc_TypeError, "No base point");

  l = PyList_New (0);
  for (i = 0; i < 4; i++)
    PyList_Append (l, PyFloat_FromDouble (ptrbpoint->vec[i]));
  return l;
}

static PyObject *
Ipo_getCurveBeztriple (BPy_Ipo * self, PyObject * args)
{
  struct BezTriple *ptrbt;
  int num = 0, pos, i, j;
  IpoCurve *icu;
  PyObject *l = PyList_New (0);

  if (!PyArg_ParseTuple (args, "ii", &num, &pos))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  icu = self->ipo->curve.first;
  if (!icu)
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "No IPO curve"));
  for (i = 0; i < num; i++)
    {
      if (!icu)
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "Bad ipo number"));
      icu = icu->next;
    }
  if (pos >= icu->totvert)
    return EXPP_ReturnPyObjError (PyExc_TypeError, "Bad bezt number");

  ptrbt = icu->bezt + pos;
  if (!ptrbt)
    return EXPP_ReturnPyObjError (PyExc_TypeError, "No bez triple");

  for (i = 0; i < 3; i++)
    for (j = 0; j < 3; j++)
      PyList_Append (l, PyFloat_FromDouble (ptrbt->vec[i][j]));
  return l;
}


static PyObject *
Ipo_setCurveBeztriple (BPy_Ipo * self, PyObject * args)
{
  struct BezTriple *ptrbt;
  int num = 0, pos, i;
  IpoCurve *icu;
  PyObject *listargs = 0;

  if (!PyArg_ParseTuple (args, "iiO", &num, &pos, &listargs))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected int int object argument"));
  if (!PyTuple_Check (listargs))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "3rd arg should be a tuple"));
  icu = self->ipo->curve.first;
  if (!icu)
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "No IPO curve"));
  for (i = 0; i < num; i++)
    {
      if (!icu)
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "Bad ipo number"));
      icu = icu->next;
    }
  if (pos >= icu->totvert)
    return EXPP_ReturnPyObjError (PyExc_TypeError, "Bad bezt number");

  ptrbt = icu->bezt + pos;
  if (!ptrbt)
    return EXPP_ReturnPyObjError (PyExc_TypeError, "No bez triple");

  for (i = 0; i < 9; i++)
    {
      PyObject *xx = PyTuple_GetItem (listargs, i);
      ptrbt->vec[i / 3][i % 3] = PyFloat_AsDouble (xx);
    }

  Py_INCREF (Py_None);
  return Py_None;
}


static PyObject *
Ipo_EvaluateCurveOn (BPy_Ipo * self, PyObject * args)
{
  float eval_icu (IpoCurve * icu, float ipotime);

  int num = 0, i;
  IpoCurve *icu;
  float time = 0;

  if (!PyArg_ParseTuple (args, "if", &num, &time))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));

  icu = self->ipo->curve.first;
  if (!icu)
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "No IPO curve"));

  for (i = 0; i < num; i++)
    {
      if (!icu)
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "Bad ipo number"));
      icu = icu->next;

    }
  return PyFloat_FromDouble (eval_icu (icu, time));
}


static PyObject *
Ipo_getCurvecurval (BPy_Ipo * self, PyObject * args)
{
  int numcurve = 0, i;
  IpoCurve *icu;
  char *stringname = 0, *str1 = 0;

  icu = self->ipo->curve.first;
  if (!icu)
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "No IPO curve"));

  if (PyNumber_Check (PySequence_GetItem (args, 0)))	// args is an integer
    {
      if (!PyArg_ParseTuple (args, "i", &numcurve))
	return (EXPP_ReturnPyObjError
		(PyExc_TypeError, "expected int or string argument"));
      for (i = 0; i < numcurve; i++)
	{
	  if (!icu)
	    return (EXPP_ReturnPyObjError
		    (PyExc_TypeError, "Bad ipo number"));
	  icu = icu->next;
	}
    }

  else				// args is a string
    {
      if (!PyArg_ParseTuple (args, "s", &stringname))
	return (EXPP_ReturnPyObjError
		(PyExc_TypeError, "expected int or string argument"));
      while (icu)
	{
	  /*char str1[10];
	  GetIpoCurveName (icu, str1);*/
	  str1 = GetIpoCurveName(icu);
	  if (!strcmp (str1, stringname))
	    break;
	  icu = icu->next;
	}
    }

  if (icu)
    return PyFloat_FromDouble (icu->curval);
  Py_INCREF (Py_None);
  return Py_None;
}


/*****************************************************************************/
/* Function:    IpoDeAlloc                                                   */
/* Description: This is a callback function for the BPy_Ipo type. It is        */
/*              the destructor function.                                     */
/*****************************************************************************/
static void
IpoDeAlloc (BPy_Ipo * self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    IpoGetAttr                                                   */
/* Description: This is a callback function for the BPy_Ipo type. It is        */
/*              the function that accesses BPy_Ipo "member variables" and      */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *
IpoGetAttr (BPy_Ipo * self, char *name)
{
  if (strcmp (name, "curves") == 0)
    return Ipo_getCurves (self);
  return Py_FindMethod (BPy_Ipo_methods, (PyObject *) self, name);
}

/*****************************************************************************/
/* Function:    IpoSetAttr                                                */
/* Description: This is a callback function for the BPy_Ipo type. It is the */
/*              function that sets Ipo Data attributes (member variables).*/
/*****************************************************************************/
static int
IpoSetAttr (BPy_Ipo * self, char *name, PyObject * value)
{
  return 0;			/* normal exit */
}

/*****************************************************************************/
/* Function:    IpoRepr                                                      */
/* Description: This is a callback function for the BPy_Ipo type. It           */
/*              builds a meaninful string to represent ipo objects.          */
/*****************************************************************************/
static PyObject *
IpoRepr (BPy_Ipo * self)
{
  return PyString_FromFormat ("[Ipo \"%s\" %d]", self->ipo->id.name + 2,
			      self->ipo->blocktype);
}

/* Three Python Ipo_Type helper functions needed by the Object module: */

/*****************************************************************************/
/* Function:    Ipo_CreatePyObject                                           */
/* Description: This function will create a new BPy_Ipo from an existing       */
/*              Blender ipo structure.                                       */
/*****************************************************************************/
PyObject *
Ipo_CreatePyObject (Ipo * ipo)
{
  BPy_Ipo *pyipo;
  pyipo = (BPy_Ipo *) PyObject_NEW (BPy_Ipo, &Ipo_Type);
  if (!pyipo)
    return EXPP_ReturnPyObjError (PyExc_MemoryError,
				  "couldn't create BPy_Ipo object");
  pyipo->ipo = ipo;
  return (PyObject *) pyipo;
}

/*****************************************************************************/
/* Function:    Ipo_CheckPyObject                                            */
/* Description: This function returns true when the given PyObject is of the */
/*              type Ipo. Otherwise it will return false.                    */
/*****************************************************************************/
int
Ipo_CheckPyObject (PyObject * pyobj)
{
  return (pyobj->ob_type == &Ipo_Type);
}

/*****************************************************************************/
/* Function:    Ipo_FromPyObject                                             */
/* Description: This function returns the Blender ipo from the given         */
/*              PyObject.                                                    */
/*****************************************************************************/
Ipo *
Ipo_FromPyObject (PyObject * pyobj)
{
  return ((BPy_Ipo *) pyobj)->ipo;
}
