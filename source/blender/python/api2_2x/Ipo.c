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

/*****************************************************************************/
/* Function:              M_Ipo_New                                          */
/* Python equivalent:     Blender.Ipo.New                                    */
/*****************************************************************************/
static PyObject *M_Ipo_New(PyObject *self, PyObject *args)
{
	Ipo *add_ipo(char *name, int idcode);
	char*name = NULL;
	int code = 0;
  C_Ipo    *pyipo;
  Ipo      *blipo;

	if (!PyArg_ParseTuple(args, "si", &name,&code))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected string int arguments"));


  blipo = add_ipo(name,code);

  if (blipo) 
		pyipo = (C_Ipo *)PyObject_NEW(C_Ipo, &Ipo_Type);
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't create Ipo Data in Blender"));

  if (pyipo == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
																	 "couldn't create Ipo Data object"));

  pyipo->ipo = blipo; 

  return (PyObject *)pyipo;
}

/*****************************************************************************/
/* Function:              M_Ipo_Get                                       */
/* Python equivalent:     Blender.Ipo.Get                                 */
/* Description:           Receives a string and returns the ipo data obj  */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all ipo data names in the  */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_Ipo_Get(PyObject *self, PyObject *args)
{
  char   *name = NULL;
  Ipo *ipo_iter;
  PyObject *ipolist, *pystr;
  C_Ipo *wanted_ipo = NULL;
  char error_msg[64];

	if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected string argument (or nothing)"));

  ipo_iter = G.main->ipo.first;

	if (name) { /* (name) - Search ipo by name */
    while ((ipo_iter) && (wanted_ipo == NULL)) {
      if (strcmp (name, ipo_iter->id.name+2) == 0) {
        wanted_ipo = (C_Ipo *)PyObject_NEW(C_Ipo, &Ipo_Type);
				if (wanted_ipo) wanted_ipo->ipo = ipo_iter;
      }
      ipo_iter = ipo_iter->id.next;
    }

    if (wanted_ipo == NULL) { /* Requested ipo doesn't exist */
      PyOS_snprintf(error_msg, sizeof(error_msg),
										"Ipo \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }

    return (PyObject *)wanted_ipo;
	}

	else { /* () - return a list of all ipos in the scene */
    int index = 0;

    ipolist = PyList_New (BLI_countlist (&(G.main->ipo)));

    if (ipolist == NULL)
      return (PythonReturnErrorObject (PyExc_MemoryError,
																			 "couldn't create PyList"));

		while (ipo_iter) {
      pystr = PyString_FromString (ipo_iter->id.name+2);

			if (!pystr)
				return (PythonReturnErrorObject (PyExc_MemoryError,
																				 "couldn't create PyString"));

			PyList_SET_ITEM (ipolist, index, pystr);

      ipo_iter = ipo_iter->id.next;
      index++;
		}

		return (ipolist);
	}
}

/*****************************************************************************/
/* Function:              M_Ipo_Init                                      */
/*****************************************************************************/
PyObject *M_Ipo_Init (void)
{
  PyObject  *submodule;

  printf ("In M_Ipo_Init()\n");
  submodule = Py_InitModule3("Blender.Ipo",
														 M_Ipo_methods, M_Ipo_doc);

  return (submodule);
}

/*****************************************************************************/
/* Python C_Ipo methods:                                                  */
/*****************************************************************************/
static PyObject *Ipo_getName(C_Ipo *self)
{
  PyObject *attr = PyString_FromString(self->ipo->id.name+2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																 "couldn't get Ipo.name attribute"));
}
static PyObject *Ipo_setName(C_Ipo *self, PyObject *args)
{
  char *name;
  char buf[21];

  if (!PyArg_ParseTuple(args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected string argument"));

  PyOS_snprintf(buf, sizeof(buf), "%s", name);

  rename_id(&self->ipo->id, buf);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Ipo_getBlocktype(C_Ipo *self)
{
  PyObject *attr = PyInt_FromLong(self->ipo->blocktype);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																 "couldn't get Ipo.blocktype attribute"));
}


static PyObject *Ipo_setBlocktype(C_Ipo *self, PyObject *args)
{
  int blocktype = 0;

  if (!PyArg_ParseTuple(args, "i", &blocktype))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected string argument"));

	self->ipo->blocktype = (short)blocktype;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Ipo_getShowkey(C_Ipo *self)
{
  PyObject *attr = PyInt_FromLong(self->ipo->showkey);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																 "couldn't get Ipo.showkey attribute"));
}


static PyObject *Ipo_setShowkey(C_Ipo *self, PyObject *args)
{
  int showkey = 0;

  if (!PyArg_ParseTuple(args, "i", &showkey))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected string argument"));

	self->ipo->showkey = (short)showkey;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Ipo_getPad(C_Ipo *self)
{
  PyObject *attr = PyInt_FromLong(self->ipo->pad);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																 "couldn't get Ipo.pad attribute"));
}


static PyObject *Ipo_setPad(C_Ipo *self, PyObject *args)
{
  int pad = 0;

  if (!PyArg_ParseTuple(args, "i", &pad))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected string argument"));

	self->ipo->pad = pad;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Ipo_getRctf(C_Ipo *self)
{
	PyObject* l = PyList_New(0);
  PyList_Append( l, PyFloat_FromDouble(self->ipo->cur.xmin));
  PyList_Append( l, PyFloat_FromDouble(self->ipo->cur.xmax));
  PyList_Append( l, PyFloat_FromDouble(self->ipo->cur.ymin));
  PyList_Append( l, PyFloat_FromDouble(self->ipo->cur.ymax));
  return l;

}


static PyObject *Ipo_setRctf(C_Ipo *self, PyObject *args)
{
	float v[4];
  if (!PyArg_ParseTuple(args, "ffff",v,v+1,v+2,v+3))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected 4 float argument"));

	self->ipo->cur.xmin  = v[0];
	self->ipo->cur.xmax  = v[1];
	self->ipo->cur.ymin  = v[2];
	self->ipo->cur.ymax  = v[3];

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Ipo_getNcurves(C_Ipo *self)
{ 
	int i = 0;

	IpoCurve *icu;
	for (icu=self->ipo->curve.first; icu; icu=icu->next){
		i++;
	}

  return (PyInt_FromLong(i) );
}


static PyObject *Ipo_getCurveBP(C_Ipo *self, PyObject *args)
{ 	
	struct BPoint *ptrbpoint;
	int num = 0,i;
	IpoCurve *icu;
	PyObject* l;

  if (!PyArg_ParseTuple(args, "i",&num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected int argument"));
	icu =self->ipo->curve.first;
	if(!icu) return (EXPP_ReturnPyObjError (PyExc_TypeError,"No IPO curve"));
	for(i = 0;i<num;i++)
		{
			if(!icu) return (EXPP_ReturnPyObjError (PyExc_TypeError,"Bad ipo number"));
			icu=icu->next;
		 
    }
	ptrbpoint = icu->bp;
	if(!ptrbpoint)return EXPP_ReturnPyObjError(PyExc_TypeError,"No base point");

	l = PyList_New(0);
	for(i=0;i<4;i++)
		PyList_Append( l, PyFloat_FromDouble(ptrbpoint->vec[i]));
	return l;
}

static PyObject *Ipo_getCurveBeztriple(C_Ipo *self, PyObject *args)
{ 	
	struct BezTriple *ptrbt;
	int num = 0,pos,i,j;
	IpoCurve *icu;
	PyObject* l = PyList_New(0);

  if (!PyArg_ParseTuple(args, "ii",&num,&pos))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected int argument"));
	icu =self->ipo->curve.first;
	if(!icu) return (EXPP_ReturnPyObjError (PyExc_TypeError,"No IPO curve"));
	for(i = 0;i<num;i++)
		{
			if(!icu) return (EXPP_ReturnPyObjError (PyExc_TypeError,"Bad ipo number"));
			icu=icu->next;
		}
	if (pos >= icu->totvert)
		return EXPP_ReturnPyObjError(PyExc_TypeError,"Bad bezt number");

	ptrbt = icu->bezt + pos;
	if(!ptrbt)
		return EXPP_ReturnPyObjError(PyExc_TypeError,"No bez triple");

	for(i=0;i<3;i++)
		for(j=0;j<3;j++)
			PyList_Append( l, PyFloat_FromDouble(ptrbt->vec[i][j]));
	return l;
}


static PyObject *Ipo_setCurveBeztriple(C_Ipo *self, PyObject *args)
{ 	
	struct BezTriple *ptrbt;
	int num = 0,pos,i;
	IpoCurve *icu;
	PyObject *listargs=0;

  if (!PyArg_ParseTuple(args, "iiO",&num,&pos,&listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected int argument"));
	icu =self->ipo->curve.first;
	if(!icu) return (EXPP_ReturnPyObjError (PyExc_TypeError,"No IPO curve"));
	for(i = 0;i<num;i++)
		{
			if(!icu) return (EXPP_ReturnPyObjError (PyExc_TypeError,"Bad ipo number"));
			icu=icu->next;
		}
	if (pos >= icu->totvert)
		return EXPP_ReturnPyObjError(PyExc_TypeError,"Bad bezt number");

	ptrbt = icu->bezt + pos;
	if(!ptrbt)
		return EXPP_ReturnPyObjError(PyExc_TypeError,"No bez triple");
	
	for(i=0;i<9;i++)
		{
			PyObject * xx = PyList_GetItem(listargs,i);
			ptrbt->vec[i/3][i%3] = PyFloat_AsDouble(xx);
		}

  Py_INCREF(Py_None);
  return Py_None;
}




static PyObject *Ipo_getCurvecurval(C_Ipo *self, PyObject *args)
{ 	
	int num = 0,i;
	IpoCurve *icu;

  if (!PyArg_ParseTuple(args, "i",&num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected int argument"));
	icu =self->ipo->curve.first;
	if(!icu) return (EXPP_ReturnPyObjError (PyExc_TypeError,"No IPO curve"));
	for(i = 0;i<num;i++)
		{
			if(!icu) return (EXPP_ReturnPyObjError (PyExc_TypeError,"Bad ipo number"));
			icu=icu->next;
		 
    }
	return PyFloat_FromDouble(icu->curval);
}



/*****************************************************************************/
/* Function:    IpoDeAlloc                                                   */
/* Description: This is a callback function for the C_Ipo type. It is        */
/*              the destructor function.                                     */
/*****************************************************************************/
static void IpoDeAlloc (C_Ipo *self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    IpoGetAttr                                                   */
/* Description: This is a callback function for the C_Ipo type. It is        */
/*              the function that accesses C_Ipo "member variables" and      */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *IpoGetAttr (C_Ipo *self, char *name)
{
  return Py_FindMethod(C_Ipo_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    IpoSetAttr                                                */
/* Description: This is a callback function for the C_Ipo type. It is the */
/*              function that sets Ipo Data attributes (member variables).*/
/*****************************************************************************/
static int IpoSetAttr (C_Ipo *self, char *name, PyObject *value)
{
  return 0; /* normal exit */
}

/*****************************************************************************/
/* Function:    IpoPrint                                                  */
/* Description: This is a callback function for the C_Ipo type. It        */
/*              builds a meaninful string to 'print' ipo objects.         */
/*****************************************************************************/
static int IpoPrint(C_Ipo *self, FILE *fp, int flags)
{ 
  fprintf(fp, "[Ipo \"%s\"]", self->ipo->id.name+2);
  return 0;
}

/*****************************************************************************/
/* Function:    IpoRepr                                                   */
/* Description: This is a callback function for the C_Ipo type. It        */
/*              builds a meaninful string to represent ipo objects.       */
/*****************************************************************************/
static PyObject *IpoRepr (C_Ipo *self)
{
  return PyString_FromString(self->ipo->id.name+2);
}

