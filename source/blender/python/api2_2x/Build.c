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

#include "Build.h"
#include "Effect.h"

/*****************************************************************************/
/* Python Build_Type structure definition:                                   */
/*****************************************************************************/
PyTypeObject Build_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                      /* ob_size */
  "Build",                               /* tp_name */
  sizeof (BPy_Build),                      /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)BuildDeAlloc,              /* tp_dealloc */
  (printfunc)BuildPrint,                 /* tp_print */
  (getattrfunc)BuildGetAttr,             /* tp_getattr */
  (setattrfunc)BuildSetAttr,             /* tp_setattr */
  0,                                      /* tp_compare */
  (reprfunc)BuildRepr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  BPy_Build_methods,                       /* tp_methods */
  0,                                      /* tp_members */
};



/*****************************************************************************/
/* Function:              M_Build_New                                        */
/* Python equivalent:     Blender.Effect.Build.New                           */
/*****************************************************************************/
PyObject *M_Build_New(PyObject *self, PyObject *args)
{
int type =   EFF_BUILD;
  BPy_Effect    *pyeffect; 
  Effect      *bleffect = 0; 
  
  printf ("In Effect_New()\n");

  bleffect = add_effect(type);
  if (bleffect == NULL) 
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
	     "couldn't create Effect Data in Blender"));

  pyeffect = (BPy_Effect *)PyObject_NEW(BPy_Effect, &Effect_Type);

     
  if (pyeffect == NULL) return (EXPP_ReturnPyObjError (PyExc_MemoryError,
		     "couldn't create Effect Data object"));

  pyeffect->effect = bleffect; 

  return (PyObject *)pyeffect;
  return 0;
}

/*****************************************************************************/
/* Function:              M_Build_Get                                        */
/* Python equivalent:     Blender.Effect.Build.Get                           */
/*****************************************************************************/
PyObject *M_Build_Get(PyObject *self, PyObject *args)
{
  /*arguments : string object name
    int : position of effect in the obj's effect list  */
  char     *name = 0;
  Object   *object_iter;
  Effect *eff;
  BPy_Build *wanted_eff;
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
	      if (eff->type != EFF_BUILD)continue;
	      eff = eff->next;
	      if (!eff)
		return(EXPP_ReturnPyObjError(PyExc_AttributeError,"object not created"));
	    }
	  wanted_eff = (BPy_Build *)PyObject_NEW(BPy_Build, &Build_Type);
	  wanted_eff->build = eff;
	  return (PyObject*)wanted_eff;  
	}
      object_iter = object_iter->id.next;
    }
  Py_INCREF(Py_None);
  return Py_None;
}



struct PyMethodDef M_Build_methods[] = {
  {"New",(PyCFunction)M_Build_New, METH_VARARGS, M_Build_New_doc},
  {"Get",         M_Build_Get,         METH_VARARGS, M_Build_Get_doc},
  {"get",         M_Build_Get,         METH_VARARGS,M_Build_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Function:              M_Build_Init                                       */
/*****************************************************************************/
PyObject *M_Build_Init (void)
{
  PyObject  *submodule;
  printf ("In M_Build_Init()\n");
  Build_Type.ob_type = &PyType_Type;
  submodule = Py_InitModule3("Blender.Build",M_Build_methods,M_Build_doc );
  return (submodule);
}

/*****************************************************************************/
/* Python BPy_Build methods:                                                  */
/*****************************************************************************/

PyObject *Build_getLen(BPy_Build *self)
{
  BuildEff*ptr = (BuildEff*)self->build;
  return PyFloat_FromDouble(ptr->len);
}


PyObject *Build_setLen(BPy_Build *self,PyObject *args)
{ 
  BuildEff*ptr = (BuildEff*)self->build;
  float val = 0;
if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,\
				 "expected float argument"));
  ptr->len = val;
  Py_INCREF(Py_None);
  return Py_None;
}


PyObject *Build_getSfra(BPy_Build *self)
{
  BuildEff*ptr = (BuildEff*)self->build;
  return PyFloat_FromDouble(ptr->sfra);
}

PyObject *Build_setSfra(BPy_Build *self,PyObject *args)
{ 
  BuildEff*ptr = (BuildEff*)self->build;
  float val = 0;
 if (!PyArg_ParseTuple(args, "f", &val ))
    return(EXPP_ReturnPyObjError(PyExc_AttributeError,"expected float argument"));
	
  ptr->sfra = val;
  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************************/
/* Function:    BuildDeAlloc                                                 */
/* Description: This is a callback function for the BPy_Build type. It is      */
/*              the destructor function.                                     */
/*****************************************************************************/
void BuildDeAlloc (BPy_Build *self)
{
  BuildEff*ptr = (BuildEff*)self;
  PyObject_DEL (ptr);
}

/*****************************************************************************/
/* Function:    BuildGetAttr                                                 */
/* Description: This is a callback function for the BPy_Build type. It is      */
/*              the function that accesses BPy_Build "member variables" and    */
/*              methods.                                                     */
/*****************************************************************************/

PyObject *BuildGetAttr (BPy_Build *self, char *name)
{
	if (!strcmp(name,"sfra"))return Build_getSfra( self);
	if (!strcmp(name,"len"))return Build_getLen( self);
  return Py_FindMethod(BPy_Build_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    BuildSetAttr                                                  */
/* Description: This is a callback function for the BPy_Build type. It is the   */
/*              function that sets Build Data attributes (member variables).  */
/*****************************************************************************/
int BuildSetAttr (BPy_Build *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;
  valtuple = Py_BuildValue("(N)", value);

  if (!valtuple) 
    return EXPP_ReturnIntError(PyExc_MemoryError,
                         "CameraSetAttr: couldn't create PyTuple");

  if (!strcmp (name, "sfra")) error = Build_setSfra (self, valtuple);
  else if (!strcmp (name, "len"))  error = Build_setLen (self, valtuple);

  else { 
    Py_DECREF(valtuple);
      return (EXPP_ReturnIntError (PyExc_KeyError,
                   "attribute not found"));
  }

	/*  Py_DECREF(valtuple);*/

  if (error != Py_None) return -1;

  Py_DECREF(Py_None);
  return 0;
}

/*****************************************************************************/
/* Function:    BuildPrint                                                   */
/* Description: This is a callback function for the BPy_Build type. It         */
/*              builds a meaninful string to 'print' build objects.          */
/*****************************************************************************/
int BuildPrint(BPy_Build *self, FILE *fp, int flags) 
{ 
  return 0;
}

/*****************************************************************************/
/* Function:    BuildRepr                                                    */
/* Description: This is a callback function for the BPy_Build type. It         */
/*              builds a meaninful string to represent build objects.        */
/*****************************************************************************/
PyObject *BuildRepr (BPy_Build *self) 
{
  return 0;
}

PyObject* BuildCreatePyObject (struct Effect *build)
{
 BPy_Build    * blen_object;

    printf ("In BuildCreatePyObject\n");

    blen_object = (BPy_Build*)PyObject_NEW (BPy_Build, &Build_Type);

    if (blen_object == NULL)
    {
        return (NULL);
    }
    blen_object->build = build;
    return ((PyObject*)blen_object);

}

int BuildCheckPyObject (PyObject *py_obj)
{
return (py_obj->ob_type == &Build_Type);
}


struct Build* BuildFromPyObject (PyObject *py_obj)
{
 BPy_Build    * blen_obj;

    blen_obj = (BPy_Build*)py_obj;
    return ((struct Build*)blen_obj->build);

}

