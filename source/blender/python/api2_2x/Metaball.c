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

#include "Metaball.h"

/*****************************************************************************/
/* Function:              M_Metaball_New                                     */
/* Python equivalent:     Blender.Metaball.New                               */
/*****************************************************************************/
static PyObject *M_Metaball_New(PyObject *self, PyObject *args)
{
  char*name = 0;
  C_Metaball    *pymball; /* for Data object wrapper in Python */
  MetaBall      *blmball; /* for actual Data we create in Blender */
  char        buf[21];
  if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
				   "expected string argument (or nothing)"));
  printf ("In MetaBall_New()\n");

  blmball = add_mball(); /* first create the MetaBall Data in Blender */

  if (blmball) /* now create the wrapper obj in Python */
    pymball = (C_Metaball *)PyObject_NEW(C_Metaball, &Metaball_Type);
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				   "couldn't create MetaBall Data in Blender"));

  if (pymball == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				   "couldn't create MetaBall Data object"));

  pymball->metaball = blmball; 
  /*link Python mballer wrapper to Blender MetaBall */
  if(name) { /* user gave us a name for the metaball, use it */
    PyOS_snprintf(buf, sizeof(buf), "%s", name);
    rename_id(&blmball->id, buf); 
  }
  return (PyObject *)pymball;
}

/*****************************************************************************/
/* Function:              M_Metaball_Get                                     */
/* Python equivalent:     Blender.Metaball.Get                               */
/* Description:           Receives a string and returns the metaball data obj */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all metaball data names in the */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_Metaball_Get(PyObject *self, PyObject *args)
{
      char error_msg[64];
  char   *name = NULL;
  MetaBall *mball_iter;

  if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
				   "expected string argument (or nothing)"));

  mball_iter = G.main->mball.first;

  if (name) { /* (name) - Search mball by name */

    C_Metaball *wanted_mball = NULL;

    while ((mball_iter) && (wanted_mball == NULL)) {
      if (strcmp (name, mball_iter->id.name+2) == 0) {
	wanted_mball=(C_Metaball*)PyObject_NEW(C_Metaball,&Metaball_Type);
	if (wanted_mball) 
	  wanted_mball->metaball = mball_iter;
      }
      mball_iter = mball_iter->id.next;
    }

    if (wanted_mball == NULL) { /* Requested mball doesn't exist */
      PyOS_snprintf(error_msg, sizeof(error_msg),
		    "MetaBall \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }

    return (PyObject *)wanted_mball;
  }

  else { /* () - return a list of all mballs in the scene */
    int index = 0;
    PyObject *mballlist, *pystr;

    mballlist = PyList_New (BLI_countlist (&(G.main->mball)));

    if (mballlist == NULL)
      return (PythonReturnErrorObject (PyExc_MemoryError,
				       "couldn't create PyList"));

    while (mball_iter) {
      pystr = PyString_FromString (mball_iter->id.name+2);

      if (!pystr)
	return (PythonReturnErrorObject (PyExc_MemoryError,
					 "couldn't create PyString"));

      PyList_SET_ITEM (mballlist, index, pystr);

      mball_iter = mball_iter->id.next;
      index++;
    }

    return (mballlist);
  }

}

/*******************************************************************************/
/* Function:              M_Metaball_Init                                      */
/*******************************************************************************/
PyObject *M_Metaball_Init (void)
{
  PyObject  *submodule;

  printf ("In M_Metaball_Init()\n");
  submodule = Py_InitModule3("Blender.Metaball",
			     M_Metaball_methods, M_Metaball_doc);

  return (submodule);
}

/*******************************************************************************/
/* Python C_Metaball methods:                                                  */
/*******************************************************************************/
static PyObject *Metaball_getName(C_Metaball *self)
{

  PyObject *attr = PyString_FromString(self->metaball->id.name+2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Metaball.name attribute"));
}



static PyObject *Metaball_setName(C_Metaball *self,PyObject*args)
{
	char   *name = NULL;
	char buf[20];


	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected string argument"));
	PyOS_snprintf(buf, sizeof(buf), "%s", name);
	rename_id(&self->metaball->id, buf); 
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *Metaball_getBbox(C_Metaball *self)
{
	int i,j;
	PyObject* ll;
  PyObject* l = PyList_New(0);
	if (self->metaball->bb == NULL) {Py_INCREF(Py_None);return Py_None;}
	for(i = 0;i<8;i++)
		{
			ll = PyList_New(0);
			for(j = 0;j<3;j++)
				PyList_Append( ll, PyFloat_FromDouble(self->metaball->bb->vec[i][j]));
			PyList_Append( l, ll);
		}

  return l;
}

static PyObject *Metaball_getNMetaElems(C_Metaball *self)
{
  int i = 0;
  MetaElem*ptr = self->metaball->elems.first;
  if(!ptr) return (PyInt_FromLong(0) );
  while(ptr)
    {
      i++;
      ptr = ptr->next;
    }
  return (PyInt_FromLong(i) );
}

static PyObject *Metaball_getNMetaElems1(C_Metaball *self)
{
  int i = 0;
  MetaElem*ptr = self->metaball->disp.first;
  if(!ptr) return (PyInt_FromLong(0) );
  while(ptr)
    {
      i++;
      ptr = ptr->next;
    }
  return (PyInt_FromLong(i) );
}


static PyObject *Metaball_getloc(C_Metaball *self)
{
  PyObject* l = PyList_New(0);
  PyList_Append( l, PyFloat_FromDouble(self->metaball->loc[0]));
  PyList_Append( l, PyFloat_FromDouble(self->metaball->loc[1]));
  PyList_Append( l, PyFloat_FromDouble(self->metaball->loc[2]));
  return l;
}

static PyObject *Metaball_setloc(C_Metaball *self,PyObject*args)
{	
	
  float val[3];
  if (!PyArg_ParseTuple(args, "fff", val,val+1,val+2))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected three float args"));
 
  self->metaball->loc[0] = val[0];
  self->metaball->loc[1] = val[1];
  self->metaball->loc[2] = val[2];

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getrot(C_Metaball *self)
{
  PyObject* l = PyList_New(0);
  PyList_Append( l, PyFloat_FromDouble(self->metaball->rot[0]));
  PyList_Append( l, PyFloat_FromDouble(self->metaball->rot[1]));
  PyList_Append( l, PyFloat_FromDouble(self->metaball->rot[2]));
  return l;
}

static PyObject *Metaball_setrot(C_Metaball *self,PyObject*args)
{	
	
  float val[3];
  if (!PyArg_ParseTuple(args, "fff", val,val+1,val+2))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected three float args"));
 
  self->metaball->rot[0] = val[0];
  self->metaball->rot[1] = val[1];
  self->metaball->rot[2] = val[2];

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getsize(C_Metaball *self)
{
  PyObject* l = PyList_New(0);
  PyList_Append( l, PyFloat_FromDouble(self->metaball->size[0]));
  PyList_Append( l, PyFloat_FromDouble(self->metaball->size[1]));
  PyList_Append( l, PyFloat_FromDouble(self->metaball->size[2]));
  return l;
}

static PyObject *Metaball_setsize(C_Metaball *self,PyObject*args)
{	
	
  float val[3];
  if (!PyArg_ParseTuple(args, "fff", val,val+1,val+2))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected three float args"));
 
  self->metaball->size[0] = val[0];
  self->metaball->size[1] = val[1];
  self->metaball->size[2] = val[2];

  Py_INCREF(Py_None);
  return Py_None;

}
static PyObject *Metaball_getWiresize(C_Metaball *self)
{
  return PyFloat_FromDouble(self->metaball->wiresize);
}

static PyObject *Metaball_setWiresize(C_Metaball *self,PyObject*args)
{	
	
  float val;
  if (!PyArg_ParseTuple(args, "f", &val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected float args"));
 
  self->metaball->wiresize = val;

  Py_INCREF(Py_None);
  return Py_None;

}
static PyObject *Metaball_getRendersize(C_Metaball *self)
{
  return PyFloat_FromDouble(self->metaball->rendersize);
}

static PyObject *Metaball_setRendersize(C_Metaball *self,PyObject*args)
{	
	
  float val;
  if (!PyArg_ParseTuple(args, "f", &val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected float args"));
 
  self->metaball->rendersize = val;

  Py_INCREF(Py_None);
  return Py_None;

}
static PyObject *Metaball_getThresh(C_Metaball *self)
{
  return PyFloat_FromDouble(self->metaball->thresh);
}

static PyObject *Metaball_setThresh(C_Metaball *self,PyObject*args)
{	
	
  float val;
  if (!PyArg_ParseTuple(args, "f", &val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected float args"));
 
  self->metaball->thresh = val;

  Py_INCREF(Py_None);
  return Py_None;

}


/*******************************************************************************/
/* get/set metaelems data,                                                     */
/*******************************************************************************/

static PyObject *Metaball_getMetadata(C_Metaball *self,PyObject*args)
{	
  int num;
  int i = 0;
  char*name = NULL;
	MetaElem *ptr;

  if (!PyArg_ParseTuple(args, "si", &name,&num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, \
				   "expected (string int) argument"));
  /*jump to the num-th MetaElem*/
  ptr = self->metaball->elems.first;
  if(!ptr) 
    return  (EXPP_ReturnPyObjError (PyExc_TypeError, "no metaelem found"));
  for(i = 0;i<num;i++)
    {
      if(!ptr) 
	return  (EXPP_ReturnPyObjError (PyExc_TypeError, "metaelem not found"));
      ptr = ptr->next;
    }
  if(!strcmp(name,"type"))
    return (PyInt_FromLong(ptr->type));
  if(!strcmp(name,"lay"))
    return (PyInt_FromLong(ptr->lay));
  if(!strcmp(name,"selcol"))
    return (PyInt_FromLong(ptr->selcol));
  if(!strcmp(name,"flag"))
    return (PyInt_FromLong(ptr->flag));
  if(!strcmp(name,"pad"))
    return (PyInt_FromLong(ptr->pad));
  if(!strcmp(name,"x"))
    return (PyFloat_FromDouble(ptr->x));
  if(!strcmp(name,"y"))
    return (PyFloat_FromDouble(ptr->y));
  if(!strcmp(name,"z"))
    return (PyFloat_FromDouble(ptr->z));
  if(!strcmp(name,"expx"))
    return (PyFloat_FromDouble(ptr->expx));
  if(!strcmp(name,"expy"))
    return (PyFloat_FromDouble(ptr->expy));
  if(!strcmp(name,"expz"))
    return (PyFloat_FromDouble(ptr->expz));
  if(!strcmp(name,"rad"))
    return (PyFloat_FromDouble(ptr->rad));
  if(!strcmp(name,"rad2"))
    return (PyFloat_FromDouble(ptr->rad2));
  if(!strcmp(name,"s"))
    return (PyFloat_FromDouble(ptr->s));
  if(!strcmp(name,"len"))
    return (PyFloat_FromDouble(ptr->len));
  if(!strcmp(name,"maxrad2"))
    return (PyFloat_FromDouble(ptr->maxrad2));




  return (EXPP_ReturnPyObjError (PyExc_TypeError, "unknown name "));
}



static PyObject *Metaball_setMetadata(C_Metaball *self,PyObject*args)
{	
  int num;
  int i = 0;
  char*name = NULL;
  int intval=-1;
  float floatval=FP_INFINITE;
  int ok = 0;
	MetaElem *ptr;

	/* XXX: This won't work.  PyArg_ParseTuple will unpack 'args' in its first
	 * call, so it can't be called again.  Better get the value as a float then
	 * compare it with its int part and, if equal, consider it an int.  Note 
	 * that 'ok' isn't used in this function at all, whatever it was meant for */
  if (PyArg_ParseTuple(args, "sii", &name,&num,&intval))ok=1;
  if (PyArg_ParseTuple(args, "sif", &name,&num,&floatval)) ok = 2;
  if (!ok)
return (EXPP_ReturnPyObjError (PyExc_TypeError, \
				   "expected string,int,int or float  arguments"));
  if (floatval == FP_INFINITE) floatval = (float)intval;
  /*jump to the num-th MetaElem*/
  ptr = self->metaball->elems.first;
  if(!ptr) 
    return  (EXPP_ReturnPyObjError (PyExc_TypeError, "metaelem not found"));
  for(i = 0;i<num;i++)
    {
      if(!ptr) 
	return  (EXPP_ReturnPyObjError (PyExc_TypeError, "metaelem not found"));
      ptr = ptr->next;
    }
  if(!strcmp(name,"type"))
    {ptr->type=intval;return (PyInt_FromLong(intval));}
  if(!strcmp(name,"lay"))
    {ptr->lay=intval;return (PyInt_FromLong(intval));}
  if(!strcmp(name,"selcol"))
    {ptr->selcol=intval;return (PyInt_FromLong(intval));}
  if(!strcmp(name,"flag"))
    {ptr->flag=intval;return (PyInt_FromLong(intval));}
  if(!strcmp(name,"pad"))
    {ptr->pad=intval;return (PyInt_FromLong(intval));}

  if(!strcmp(name,"x"))
    {ptr->x=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"y"))
    {ptr->y=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"z"))
    {ptr->z=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"expx"))
    {ptr->expx=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"expy"))
    {ptr->expy=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"expz"))
    {ptr->expz=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"rad"))
    {ptr->rad=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"rad2"))
    {ptr->rad2=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"s"))
    {ptr->s=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"len"))
    {ptr->len=floatval;return (PyFloat_FromDouble(floatval));}
  if(!strcmp(name,"maxrad2"))
    {ptr->maxrad2=floatval;return (PyFloat_FromDouble(floatval));}



  return (EXPP_ReturnPyObjError (PyExc_TypeError, "unknown name "));
}

static PyObject *Metaball_getMetatype(C_Metaball *self,PyObject*args)
{	
  int num;
  int i = 0;
  MetaElem*ptr = self->metaball->elems.first;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, \
				   "expected int argument"));
  if(!ptr) return (PyInt_FromLong(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyInt_FromLong(ptr->type));
}



static PyObject *Metaball_setMetatype(C_Metaball *self,PyObject*args)
{	
  int num,val, i = 0;
  MetaElem*ptr = self->metaball->elems.first;
  if (!PyArg_ParseTuple(args, "ii", &num,&val))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected int int arguments"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->type = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetalay(C_Metaball *self,PyObject*args)
{	
  int num;
  int i = 0;
  MetaElem*ptr = self->metaball->elems.first;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyInt_FromLong(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyInt_FromLong(ptr->lay));
}



static PyObject *Metaball_setMetalay(C_Metaball *self,PyObject*args)
{	
  int num,val, i = 0;
  MetaElem*ptr = self->metaball->elems.first;
  if (!PyArg_ParseTuple(args, "ii", &num,&val))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected int int arguments"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->lay = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetaflag(C_Metaball *self,PyObject*args)
{	
  int num;
  int i = 0;
  MetaElem*ptr = self->metaball->elems.first;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyInt_FromLong(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyInt_FromLong(ptr->flag));
}



static PyObject *Metaball_setMetaflag(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num,val, i = 0;
  if (!PyArg_ParseTuple(args, "ii", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int int argnts"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->flag = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetaselcol(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyInt_FromLong(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyInt_FromLong(ptr->selcol));
}



static PyObject *Metaball_setMetaselcol(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num,val, i = 0;
  if (!PyArg_ParseTuple(args, "ii", &num,&val))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected int int arguments"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->selcol = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetapad(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyInt_FromLong(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyInt_FromLong(ptr->pad));
}



static PyObject *Metaball_setMetapad(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num,val, i = 0;
  if (!PyArg_ParseTuple(args, "ii", &num,&val))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected int int arguments"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->pad = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetax(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->x));
}



static PyObject *Metaball_setMetax(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num, i = 0;
  float val;
  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError(PyExc_TypeError,"expected int float args"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->x = val;

  Py_INCREF(Py_None);
  return Py_None;

}
static PyObject *Metaball_getMetay(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->y));
}



static PyObject *Metaball_setMetay(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num, i = 0;
  float val;
  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float args"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->y = val;

  Py_INCREF(Py_None);
  return Py_None;

}


static PyObject *Metaball_getMetaz(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->z));
}

static PyObject *Metaball_setMetaz(C_Metaball *self,PyObject*args)
{	
  int num, i = 0;
  MetaElem*ptr = self->metaball->elems.first;
  float val;
  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float args"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->z = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetaexpx(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->expx));
}

static PyObject *Metaball_setMetaexpx(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num, i = 0;
  float val;
  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float args"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->expx = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetaexpy(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->expy));
}

static PyObject *Metaball_setMetaexpy(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num, i = 0;
  float val;
  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float argts"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->expy = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetaexpz(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->expz));
}

static PyObject *Metaball_setMetaexpz(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num, i = 0;
  float val;
  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float argts"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->expz = val;

  Py_INCREF(Py_None);
  return Py_None;

}


static PyObject *Metaball_getMetarad(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->rad));
}

static PyObject *Metaball_setMetarad(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num, i = 0;
  float val;
  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float args"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->rad = val;

  Py_INCREF(Py_None);
  return Py_None;

}



static PyObject *Metaball_getMetarad2(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->rad2));
}

static PyObject *Metaball_setMetarad2(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num, i = 0;
  float val;
  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float args"));
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->rad2 = val;

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *Metaball_getMetas(C_Metaball *self,PyObject*args)
{	
  MetaElem*ptr = self->metaball->elems.first;
  int num;
  int i = 0;
  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->s));
}

static PyObject *Metaball_setMetas(C_Metaball *self,PyObject*args)
{	
  int num, i = 0;
  float val;
	MetaElem *ptr;

  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float args"));
  ptr = self->metaball->elems.first;
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->s = val;

  Py_INCREF(Py_None);
  return Py_None;

}



static PyObject *Metaball_getMetalen(C_Metaball *self,PyObject*args)
{	
  int num;
  int i = 0;
	MetaElem *ptr;

  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  ptr = self->metaball->elems.first;
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->len));
}

static PyObject *Metaball_setMetalen(C_Metaball *self,PyObject*args)
{	
  int num, i = 0;
  float val;
	MetaElem *ptr;

  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float args"));
  ptr = self->metaball->elems.first;
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->len = val;

  Py_INCREF(Py_None);
  return Py_None;

}



static PyObject *Metaball_getMetamaxrad2(C_Metaball *self,PyObject*args)
{	
  int num;
  int i = 0;
	MetaElem *ptr;

  if (!PyArg_ParseTuple(args, "i", &num))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int argument"));
  ptr = self->metaball->elems.first;
  if(!ptr) return (PyFloat_FromDouble(0));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  return (PyFloat_FromDouble(ptr->maxrad2));
}

static PyObject *Metaball_setMetamaxrad2(C_Metaball *self,PyObject*args)
{	
  int num, i = 0;
  float val;
	MetaElem *ptr;

  if (!PyArg_ParseTuple(args, "if", &num,&val))
    return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected int float args"));
  ptr = self->metaball->elems.first;
  if(!ptr)  return (EXPP_ReturnPyObjError (PyExc_TypeError, "No MetaElem"));
  for(i = 0;i<num;i++){ptr = ptr->next;}
  ptr->maxrad2 = val;

  Py_INCREF(Py_None);
  return Py_None;

}






/*****************************************************************************/
/* Function:    MetaballDeAlloc                                              */
/* Description: This is a callback function for the C_Metaball type. It is   */
/*              the destructor function.                                     */
/*****************************************************************************/
static void MetaballDeAlloc (C_Metaball *self)
{
  PyObject_DEL (self);
}

static int MetaballPrint (C_Metaball *self, FILE *fp, int flags)
{
  fprintf(fp, "[MetaBall \"%s\"]", self->metaball->id.name+2);
  return 0;

}
/*****************************************************************************/
/* Function:    MetaballGetAttr                                              */
/* Description: This is a callback function for the C_Metaball type. It is   */
/*              the function that accesses C_Metaball "member variables" and */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject *MetaballGetAttr (C_Metaball *self, char *name)
{

if (strcmp (name, "name") == 0)return  Metaball_getName (self);
if (strcmp (name, "rot") == 0)return  Metaball_getrot (self);
  return Py_FindMethod(C_Metaball_methods, (PyObject *)self, name);
}

/*******************************************************************************/
/* Function:    MetaballSetAttr                                                */
/* Description: This is a callback function for the C_Metaball type. It is the */
/*              function that sets Metaball Data attributes (member variables).*/
/*******************************************************************************/
static int MetaballSetAttr (C_Metaball *self, char *name, PyObject *value)
{
  PyObject *valtuple  = Py_BuildValue("(N)", value);

  if (!valtuple) 
    return EXPP_ReturnIntError(PyExc_MemoryError,
															 "MetaballSetAttr: couldn't create PyTuple");

  if (strcmp (name, "name") == 0) 
		{
			Metaball_setName (self, valtuple);
			return 0;
		}

  if (strcmp (name, "rot") == 0)
		{
			Metaball_setrot (self, valtuple);
			return 0;
		}
 
  return (EXPP_ReturnIntError (PyExc_KeyError,"attribute not found"));
}

/*****************************************************************************/
/* Function:    MetaballRepr                                                  */
/* Description: This is a callback function for the C_Metaball type. It       */
/*              builds a meaninful string to represent metaball objects.      */
/*****************************************************************************/
static PyObject *MetaballRepr (C_Metaball *self)
{
  return PyString_FromString(self->metaball->id.name+2);
}

