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

#include "Ipo.h"

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"


/* forward declarations */
void GetIpoCurveName (IpoCurve * icu, char *s);
void getname_mat_ei (int nr, char *str);
void getname_world_ei (int nr, char *str);
void getname_cam_ei (int nr, char *str);
void getname_ob_ei (int nr, char *str);

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


static PyObject *
Ipo_addCurve (BPy_Ipo * self, PyObject * args)
{
  IpoCurve *get_ipocurve (ID * from, short type, int adrcode, Ipo * useipo);
  void allspace (unsigned short event, short val);
  void allqueue (unsigned short event, short val);
  int param = 0, ok = 0;
  char *s = 0;
  IpoCurve *icu;
  Link *link;
  struct Object *object = NULL;

  if (!PyArg_ParseTuple (args, "s", &s))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string argument"));

  /* insertkey demande un pointeur sur l'objet pour lequel on veut ajouter 
     une courbe IPO */
  link = G.main->object.first;
  while (link)
    {
      object = (Object *) link;
      if (object->ipo == self->ipo)
	break;
      link = link->next;
    }
  /* todo : what kind of object....
     #define GS(a)        (*((short *)(a)))
     printf("object %p\n",object);
     printf("type %d\n",GS(object->id.name));
   */

  if (!strcmp (s, "LocX"))
    {
      param = OB_LOC_X;
      ok = 1;
    }
  if (!strcmp (s, "LocY"))
    {
      param = OB_LOC_Y;
      ok = 1;
    }
  if (!strcmp (s, "LocZ"))
    {
      param = OB_LOC_Z;
      ok = 1;
    }
  if (!strcmp (s, "RotX"))
    {
      param = OB_ROT_X;
      ok = 1;
    }
  if (!strcmp (s, "RotY"))
    {
      param = OB_ROT_Y;
      ok = 1;
    }
  if (!strcmp (s, "RotZ"))
    {
      param = OB_ROT_Z;
      ok = 1;
    }
  if (!strcmp (s, "SizeX"))
    {
      param = OB_SIZE_X;
      ok = 1;
    }
  if (!strcmp (s, "SizeY"))
    {
      param = OB_SIZE_Y;
      ok = 1;
    }
  if (!strcmp (s, "SizeZ"))
    {
      param = OB_SIZE_Z;
      ok = 1;
    }

  if (!strcmp (s, "dLocX"))
    {
      param = OB_DLOC_X;
      ok = 1;
    }
  if (!strcmp (s, "dLocY"))
    {
      param = OB_DLOC_Y;
      ok = 1;
    }
  if (!strcmp (s, "dLocZ"))
    {
      param = OB_DLOC_Z;
      ok = 1;
    }
  if (!strcmp (s, "dRotX"))
    {
      param = OB_DROT_X;
      ok = 1;
    }
  if (!strcmp (s, "dRotY"))
    {
      param = OB_DROT_Y;
      ok = 1;
    }
  if (!strcmp (s, "dRotZ"))
    {
      param = OB_DROT_Z;
      ok = 1;
    }
  if (!strcmp (s, "dSizeX"))
    {
      param = OB_DSIZE_X;
      ok = 1;
    }
  if (!strcmp (s, "dSizeY"))
    {
      param = OB_DSIZE_Y;
      ok = 1;
    }
  if (!strcmp (s, "dSizeZ"))
    {
      param = OB_DSIZE_Z;
      ok = 1;
    }

  if (!strcmp (s, "Layer"))
    {
      param = OB_LAY;
      ok = 1;
    }
  if (!strcmp (s, "Time"))
    {
      param = OB_TIME;
      ok = 1;
    }

  if (!strcmp (s, "ColR"))
    {
      param = OB_COL_R;
      ok = 1;
    }
  if (!strcmp (s, "ColG"))
    {
      param = OB_COL_G;
      ok = 1;
    }
  if (!strcmp (s, "ColB"))
    {
      param = OB_COL_B;
      ok = 1;
    }
  if (!strcmp (s, "ColA"))
    {
      param = OB_COL_A;
      ok = 1;
    }
  if (!ok)
    return (EXPP_ReturnPyObjError (PyExc_ValueError, "Not a valid param."));

  /* add a new curve to the ipo.  we pass in self->ipo so a new one does 
     not get created */ 
  icu = get_ipocurve (&(object->id), ID_OB, param, self->ipo );


#define REMAKEIPO		1
#define REDRAWIPO			0x4023
  allspace (REMAKEIPO, 0);
  allqueue (REDRAWIPO, 0);

  return IpoCurve_CreatePyObject (icu);
}



static PyObject *
Ipo_getCurve (BPy_Ipo * self, PyObject * args)
{
  char *str;
  IpoCurve *icu = 0;
  if (!PyArg_ParseTuple (args, "s", &str))
    return (EXPP_ReturnPyObjError
	    (PyExc_TypeError, "expected string argument"));
  for (icu = self->ipo->curve.first; icu; icu = icu->next)
    {
      char str1[80];
      GetIpoCurveName (icu, str1);
      if (!strcmp (str1, str))
	return IpoCurve_CreatePyObject (icu);
    }

  Py_INCREF (Py_None);
  return Py_None;
}


void
GetIpoCurveName (IpoCurve * icu, char *s)
{
  switch (icu->blocktype)
    {
    case ID_MA:
      {
	getname_mat_ei (icu->adrcode, s);
	break;
      }
    case ID_WO:
      {
	getname_world_ei (icu->adrcode, s);
	break;
      }
    case ID_CA:
      {
	getname_cam_ei (icu->adrcode, s);
	break;
      }
    case ID_OB:
      {
	getname_ob_ei (icu->adrcode, s);
	break;
      }
    }
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
  char *stringname = 0;

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
	  char str1[10];
	  GetIpoCurveName (icu, str1);
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
