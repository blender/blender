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

#include "Wave.h"
#include "Effect.h"


/*****************************************************************************/
/* Python C_Wave methods table:                                              */
/*****************************************************************************/
static PyMethodDef C_Wave_methods[] = {
	{"getType", (PyCFunction)Effect_getType,
	 METH_NOARGS,"() - Return Effect type"},
  {"setType", (PyCFunction)Effect_setType, 
   METH_VARARGS,"() - Set Effect type"},
  {"getFlag", (PyCFunction)Effect_getFlag, 
   METH_NOARGS,"() - Return Effect flag"},
  {"setFlag", (PyCFunction)Effect_setFlag, 
   METH_VARARGS,"() - Set Effect flag"},
  {"getStartx",(PyCFunction)Wave_getStartx,
	 METH_NOARGS,"()-Return Wave startx"},
  {"setStartx",(PyCFunction)Wave_setStartx, METH_VARARGS,
	 "()- Sets Wave startx"},
  {"getStarty",(PyCFunction)Wave_getStarty,
	 METH_NOARGS,"()-Return Wave starty"},
  {"setStarty",(PyCFunction)Wave_setStarty, METH_VARARGS,
	 "()- Sets Wave starty"},
  {"getHeight",(PyCFunction)Wave_getHeight,
	 METH_NOARGS,"()-Return Wave height"},
  {"setHeight",(PyCFunction)Wave_setHeight, METH_VARARGS,
	 "()- Sets Wave height"},
  {"getWidth",(PyCFunction)Wave_getWidth,
	 METH_NOARGS,"()-Return Wave width"},
  {"setWidth",(PyCFunction)Wave_setWidth, METH_VARARGS,
	 "()- Sets Wave width"},
  {"getNarrow",(PyCFunction)Wave_getNarrow,
	 METH_NOARGS,"()-Return Wave narrow"},
  {"setNarrow",(PyCFunction)Wave_setNarrow, METH_VARARGS,
	 "()- Sets Wave narrow"},
  {"getSpeed",(PyCFunction)Wave_getSpeed,
	 METH_NOARGS,"()-Return Wave speed"},
  {"setSpeed",(PyCFunction)Wave_setSpeed, METH_VARARGS,
	 "()- Sets Wave speed"},
  {"getMinfac",(PyCFunction)Wave_getMinfac,
	 METH_NOARGS,"()-Return Wave minfac"},
  {"setMinfac",(PyCFunction)Wave_setMinfac, METH_VARARGS,
	 "()- Sets Wave minfac"},
  {"getDamp",(PyCFunction)Wave_getDamp,
	 METH_NOARGS,"()-Return Wave damp"},
  {"setDamp",(PyCFunction)Wave_setDamp, METH_VARARGS,
	 "()- Sets Wave damp"},
  {"getTimeoffs",(PyCFunction)Wave_getTimeoffs,
	 METH_NOARGS,"()-Return Wave timeoffs"},
  {"setTimeoffs",(PyCFunction)Wave_setTimeoffs, METH_VARARGS,
	 "()- Sets Wave timeoffs"},
  {"getLifetime",(PyCFunction)Wave_getLifetime,
	 METH_NOARGS,"()-Return Wave lifetime"},
  {"setLifetime",(PyCFunction)Wave_setLifetime, METH_VARARGS,
	 "()- Sets Wave lifetime"},
	{0}
};




/*****************************************************************************/
/* Python Wave_Type structure definition:                                    */
/*****************************************************************************/
PyTypeObject Wave_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                     
  "Wave",                              
  sizeof (C_Wave),                   
  0,                                    
  /* methods */
  (destructor)WaveDeAlloc,              /* tp_dealloc */
  (printfunc)WavePrint,                 /* tp_print */
  (getattrfunc)WaveGetAttr,             /* tp_getattr */
  (setattrfunc)WaveSetAttr,             /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)WaveRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  C_Wave_methods,                       /* tp_methods */
  0,                                      /* tp_members */
};


/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Wave.__doc__                                                      */
/*****************************************************************************/
static char M_Wave_doc[] = "The Blender Wave module\n\n\
This module provides access to **Object Data** in Blender.\n\
Functions :\n\
	New(opt name) : creates a new wave object with the given name (optional)\n\
	Get(name) : retreives a wave  with the given name (mandatory)\n\
	get(name) : same as Get. Kept for compatibility reasons";
static char M_Wave_New_doc[] ="";
static char M_Wave_Get_doc[] ="xxx";
/*****************************************************************************/
/* Python method structure definition for Blender.Wave module:               */
/*****************************************************************************/
struct PyMethodDef M_Wave_methods[] = {
  {"New",(PyCFunction)M_Wave_New, METH_VARARGS,M_Wave_New_doc},
  {"Get",         M_Wave_Get,         METH_VARARGS, M_Wave_Get_doc},
  {"get",         M_Wave_Get,         METH_VARARGS, M_Wave_Get_doc},
  {NULL, NULL, 0, NULL}
};



/*   -Sta x et Sta y : là où débute la vague
        -X, Y : la vague se propagera dans les directions X et/ou Y
        -Cycl : la vague se répétera ou ne se produira qu'une fois
        -Speed, Height, Width, Narrow : joues avec ces paramètres, je
         te fais confiance. Ils interagissent et s'influencent alors
         mieux vaut y aller un à la fois et qu'un peu à la fois.
        -Time sta: la frame où l'effet commence à se produire.
        -Lifetime: durée en frames de l'effet.
        -Damptime: le temps, en frames, que met une vague à mourir.
*/

/*****************************************************************************/
/* Function:              M_Wave_New                                         */
/* Python equivalent:     Blender.Effect.Wave.New                            */
/*****************************************************************************/
PyObject *M_Wave_New(PyObject *self, PyObject *args)
{
int type =   EFF_WAVE;
  C_Effect    *pyeffect;
  Effect      *bleffect = 0; 
  
  printf ("In Effect_New()\n");

  bleffect = add_effect(type); 
  if (bleffect == NULL) 
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
	     "couldn't create Effect Data in Blender"));

  pyeffect = (C_Effect *)PyObject_NEW(C_Effect, &Effect_Type);

     
  if (pyeffect == NULL) return (EXPP_ReturnPyObjError (PyExc_MemoryError,
		     "couldn't create Effect Data object"));

  pyeffect->effect = bleffect; 

  return (PyObject *)pyeffect;
  return 0;
}

/*****************************************************************************/
/* Function:              M_Wave_Get                                         */
/* Python equivalent:     Blender.Effect.Wave.Get                            */
/*****************************************************************************/
PyObject *M_Wave_Get(PyObject *self, PyObject *args)
{
  /*arguments : string object name
    int : position of effect in the obj's effect list  */
  char     *name = 0;
  Object   *object_iter;
  Effect *eff;
  C_Wave *wanted_eff;
  int num,i;
  printf ("In Effect_Get()\n");
  if (!PyArg_ParseTuple(args, "si", &name, &num ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected string int argument"));

  object_iter = G.main->object.first;
  if (!object_iter)return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
						"Scene contains no object"));

  while (object_iter)
    {
      if (strcmp(name,object_iter->id.name+2))
	{
	  object_iter = object_iter->id.next;
	  continue;
	}

      
      if (object_iter->effect.first != NULL)
	{
	  eff = object_iter->effect.first;
	  for(i = 0;i<num;i++)
	    {
	      if (eff->type != EFF_WAVE)continue;
	      eff = eff->next;
	      if (!eff)
		return(EXPP_ReturnPyObjError(PyExc_AttributeError,"bject"));
	    }
	  wanted_eff = (C_Wave *)PyObject_NEW(C_Wave, &Wave_Type);
	  wanted_eff->wave = eff;
	  return (PyObject*)wanted_eff;  
	}
      object_iter = object_iter->id.next;
    }
  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************************/
/* Function:              M_Wave_Init                                        */
/*****************************************************************************/
PyObject *M_Wave_Init (void)
{
  PyObject  *submodule;
  printf ("In M_Wave_Init()\n");
  Wave_Type.ob_type = &PyType_Type;
  submodule = Py_InitModule3("Blender.Wave",M_Wave_methods, M_Wave_doc);
  return (submodule);
}

/*****************************************************************************/
/* Python C_Wave methods:                                                    */
/*****************************************************************************/

PyObject *Wave_getStartx(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->startx);
}

PyObject *Wave_setStartx(C_Wave *self,PyObject *args)
{ 
  WaveEff*ptr = (WaveEff*)self->wave;
  float val = 0;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr->startx = val;
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *Wave_getStarty(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->starty);
}

PyObject *Wave_setStarty(C_Wave *self,PyObject *args)
{ 
  WaveEff*ptr = (WaveEff*)self->wave;
  float val = 0;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr->starty = val;
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *Wave_getHeight(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->height);
}

PyObject *Wave_setHeight(C_Wave *self,PyObject *args)
{ 
  WaveEff*ptr = (WaveEff*)self->wave;
  float val = 0;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr->height = val;
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *Wave_getWidth(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->width);
}

PyObject *Wave_setWidth(C_Wave *self,PyObject *args)
{ 
  float val = 0;
  WaveEff*ptr;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr = (WaveEff*)self->wave;
  ptr->width = val;
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *Wave_getNarrow(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->narrow);
}

PyObject *Wave_setNarrow(C_Wave *self,PyObject *args)
{ 
  float val = 0;
  WaveEff*ptr;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr = (WaveEff*)self->wave;
  ptr->narrow = val;
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *Wave_getSpeed(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->speed);
}

PyObject *Wave_setSpeed(C_Wave *self,PyObject *args)
{ 
  float val = 0;
  WaveEff*ptr;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr = (WaveEff*)self->wave;
  ptr->speed = val;
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *Wave_getMinfac(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->minfac);
}

PyObject *Wave_setMinfac(C_Wave *self,PyObject *args)
{ 
  float val = 0;
  WaveEff*ptr;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr = (WaveEff*)self->wave;
  ptr->minfac = val;
  Py_INCREF(Py_None);
  return Py_None;
}



PyObject *Wave_getDamp(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->damp);
}

PyObject *Wave_setDamp(C_Wave *self,PyObject *args)
{ 
  WaveEff*ptr;
  float val = 0;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr = (WaveEff*)self->wave;
  ptr->damp = val;
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *Wave_getTimeoffs(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->timeoffs);
}

PyObject *Wave_setTimeoffs(C_Wave *self,PyObject *args)
{ 
  float val = 0;
  WaveEff*ptr;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr = (WaveEff*)self->wave;
  ptr->timeoffs = val;
  Py_INCREF(Py_None);
  return Py_None;
}

PyObject *Wave_getLifetime(C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  return PyFloat_FromDouble(ptr->lifetime);
}

PyObject *Wave_setLifetime(C_Wave *self,PyObject *args)
{ 
  float val = 0;
  WaveEff*ptr;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr = (WaveEff*)self->wave;
  ptr->lifetime = val;
  Py_INCREF(Py_None);
  return Py_None;
}
/*****************************************************************************/
/* Function:    WaveDeAlloc                                                  */
/* Description: This is a callback function for the C_Wave type. It is       */
/*              the destructor function.                                     */
/*****************************************************************************/
void WaveDeAlloc (C_Wave *self)
{
  WaveEff*ptr = (WaveEff*)self->wave;
  PyObject_DEL (ptr);
}

/*****************************************************************************/
/* Function:    WaveGetAttr                                                  */
/* Description: This is a callback function for the C_Wave type. It is       */
/*              the function that accesses C_Wave "member variables" and     */
/*              methods.                                                     */
/*****************************************************************************/

PyObject *WaveGetAttr (C_Wave *self, char *name)
{
	if (!strcmp(name,"lifetime"))return Wave_getLifetime( self);
	else if (!strcmp(name,"timeoffs"))return Wave_getTimeoffs( self);
	else if (!strcmp(name,"damp"))return Wave_getDamp( self);
	else if (!strcmp(name,"minfac"))return Wave_getMinfac( self);
	else if (!strcmp(name,"speed"))return Wave_getSpeed( self);
	else if (!strcmp(name,"narrow"))return Wave_getNarrow( self);
	else if (!strcmp(name,"width"))return Wave_getWidth( self);
	else if (!strcmp(name,"height"))return Wave_getHeight( self);
	else if (!strcmp(name,"startx"))return Wave_getStartx( self);
	else if (!strcmp(name,"starty"))return Wave_getStarty( self);
  return Py_FindMethod(C_Wave_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    WaveSetAttr                                                  */
/* Description: This is a callback function for the C_Wave type. It is the   */
/*              function that sets Wave Data attributes (member variables).  */
/*****************************************************************************/
int WaveSetAttr (C_Wave *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;

  valtuple = Py_BuildValue("(N)", value);

  if (!valtuple)
    return EXPP_ReturnIntError(PyExc_MemoryError,
															 "ParticleSetAttr: couldn't create PyTuple");

	if (!strcmp(name,"lifetime"))error = Wave_setLifetime( self,valtuple);
	else if (!strcmp(name,"timeoffs"))error = Wave_setTimeoffs( self,valtuple);
	else if (!strcmp(name,"damp"))    error = Wave_setDamp( self,valtuple);
	else if (!strcmp(name,"minfac"))  error = Wave_setMinfac( self,valtuple);
	else if (!strcmp(name,"speed"))   error = Wave_setSpeed( self,valtuple);
	else if (!strcmp(name,"narrow"))  error = Wave_setNarrow( self,valtuple);
	else if (!strcmp(name,"width"))   error = Wave_setWidth( self,valtuple);
	else if (!strcmp(name,"height"))  error = Wave_setHeight( self,valtuple);
	else if (!strcmp(name,"startx"))  error = Wave_setStartx( self,valtuple);
	else if (!strcmp(name,"starty"))  error = Wave_setStarty( self,valtuple);


  else { 
    Py_DECREF(valtuple);

    if ((strcmp (name, "Types") == 0) ||
        (strcmp (name, "Modes") == 0))  
      return (EXPP_ReturnIntError (PyExc_AttributeError,
																	 "constant dictionary -- cannot be changed"));

    else 
      return (EXPP_ReturnIntError (PyExc_KeyError,
																	 "attribute not found"));
  }

  /*Py_DECREF(valtuple);*/

  if (error != Py_None) return -1;

  Py_DECREF(Py_None);
  return 0; 
}

/*****************************************************************************/
/* Function:    WavePrint                                                    */
/* Description: This is a callback function for the C_Wave type. It          */
/*              builds a meaninful string to 'print' wave objects.           */
/*****************************************************************************/
int WavePrint(C_Wave *self, FILE *fp, int flags)
{ 

  printf("I'm a wave...Cool, no?");	
  return 0;
}

/*****************************************************************************/
/* Function:    WaveRepr                                                     */
/* Description: This is a callback function for the C_Wave type. It          */
/*              builds a meaninful string to represent wave objects.         */
/*****************************************************************************/
PyObject *WaveRepr (C_Wave *self) 
{
  return 0;
}

PyObject* WaveCreatePyObject (struct Effect *wave)
{
 C_Wave    * blen_object;

    printf ("In WaveCreatePyObject\n");

    blen_object = (C_Wave*)PyObject_NEW (C_Wave, &Wave_Type);

    if (blen_object == NULL)
    {
        return (NULL);
    }
    blen_object->wave = wave;
    return ((PyObject*)blen_object);

}

int WaveCheckPyObject (PyObject *py_obj)
{
return (py_obj->ob_type == &Wave_Type);
}


struct Wave* WaveFromPyObject (PyObject *py_obj)
{
 C_Wave    * blen_obj;

    blen_obj = (C_Wave*)py_obj;
    return ((struct Wave*)blen_obj->wave);

}

