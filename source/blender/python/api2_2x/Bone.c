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
 * Contributor(s): Jordi Rovira i Bonet
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Bone.h"

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_armature.h>
#include <BKE_library.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"


/*****************************************************************************/
/* Python API function prototypes for the Bone module.                       */
/*****************************************************************************/
static PyObject *M_Bone_New(PyObject *self, PyObject *args, PyObject *keywords);


/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Armature.Bone.__doc__                                             */
/*****************************************************************************/
char M_Bone_doc[] =
"The Blender Bone module\n\n\
This module provides control over **Bone Data** objects in Blender.\n\n\
Example::\n\n\
  from Blender import Armature.Bone\n\
  l = Armature.Bone.New()\n";

char M_Bone_New_doc[] =
"(name) - return a new Bone of name 'name'.";


/*****************************************************************************/
/* Python method structure definition for Blender.Armature.Bone module:      */
/*****************************************************************************/
struct PyMethodDef M_Bone_methods[] = {
  {"New",(PyCFunction)M_Bone_New, METH_VARARGS|METH_KEYWORDS,M_Bone_New_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Bone methods declarations:                                     */
/*****************************************************************************/
static PyObject *Bone_getName(BPy_Bone *self);
static PyObject *Bone_getRoll(BPy_Bone *self);
static PyObject *Bone_getHead(BPy_Bone *self);
static PyObject *Bone_getTail(BPy_Bone *self);
static PyObject *Bone_getLoc(BPy_Bone *self);
static PyObject *Bone_getSize(BPy_Bone *self);
static PyObject *Bone_getQuat(BPy_Bone *self);
static PyObject *Bone_getParent(BPy_Bone *self);
static PyObject *Bone_hasParent(BPy_Bone *self);
static PyObject *Bone_getChildren(BPy_Bone *self);
static PyObject *Bone_setName(BPy_Bone *self, PyObject *args);
static PyObject *Bone_setRoll(BPy_Bone *self, PyObject *args);
static PyObject *Bone_setHead(BPy_Bone *self, PyObject *args);
static PyObject *Bone_setTail(BPy_Bone *self, PyObject *args);
static PyObject *Bone_setLoc(BPy_Bone *self, PyObject *args);
static PyObject *Bone_setSize(BPy_Bone *self, PyObject *args);
static PyObject *Bone_setQuat(BPy_Bone *self, PyObject *args);
//static PyObject *Bone_setParent(BPy_Bone *self, PyObject *args);
//static PyObject *Bone_setChildren(BPy_Bone *self, PyObject *args);

/*****************************************************************************/
/* Python BPy_Bone methods table:                                            */
/*****************************************************************************/
static PyMethodDef BPy_Bone_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Bone_getName, METH_NOARGS,  "() - return Bone name"},
  {"getRoll", (PyCFunction)Bone_getRoll, METH_NOARGS,  "() - return Bone roll"},
  {"getHead", (PyCFunction)Bone_getHead, METH_NOARGS,  "() - return Bone head"},
  {"getTail", (PyCFunction)Bone_getTail, METH_NOARGS,  "() - return Bone tail"},
  {"getLoc", (PyCFunction)Bone_getLoc, METH_NOARGS,  "() - return Bone loc"},
  {"getSize", (PyCFunction)Bone_getSize, METH_NOARGS,  "() - return Bone size"},
  {"getQuat", (PyCFunction)Bone_getQuat, METH_NOARGS,  "() - return Bone quat"},
  {"getParent", (PyCFunction)Bone_hasParent, METH_NOARGS,
              "() - return the parent bone of this one if it exists."
              " Otherwise raise an error. Check this condition with the "
              "hasParent() method."},
  {"hasParent", (PyCFunction)Bone_hasParent, METH_NOARGS,
          "() - return true if bone has a parent"},
  {"getChildren", (PyCFunction)Bone_getChildren, METH_NOARGS,
          "() - return Bone children list"},
  {"setName", (PyCFunction)Bone_setName, METH_VARARGS, "(str) - rename Bone"},
  {"setRoll", (PyCFunction)Bone_setRoll, METH_VARARGS,
          "(float) - set Bone roll"},
  {"setHead", (PyCFunction)Bone_setHead, METH_VARARGS,
          "(float,float,float) - set Bone head pos"},
  {"setTail", (PyCFunction)Bone_setTail, METH_VARARGS,
          "(float,float,float) - set Bone tail pos"},
  {"setLoc", (PyCFunction)Bone_setLoc, METH_VARARGS,
          "(float,float,float) - set Bone loc"},
  {"setSize", (PyCFunction)Bone_setSize, METH_VARARGS,
          "(float,float,float) - set Bone size"},
  {"setQuat", (PyCFunction)Bone_setQuat, METH_VARARGS,
          "(float,float,float,float) - set Bone quat"},
  /*  {"setParent", (PyCFunction)Bone_setParent, METH_NOARGS,  "() - set the Bone parent of this one."},
      {"setChildren", (PyCFunction)Bone_setChildren, METH_NOARGS,  "() - replace the children list of the bone."},*/
  {0}
};

/*****************************************************************************/
/* Python TypeBone callback function prototypes:                             */
/*****************************************************************************/
static void Bone_dealloc (BPy_Bone *bone);
static PyObject *Bone_getAttr (BPy_Bone *bone, char *name);
static int Bone_setAttr (BPy_Bone *bone, char *name, PyObject *v);
static int Bone_compare (BPy_Bone *a1, BPy_Bone *a2);
static PyObject *Bone_repr (BPy_Bone *bone);

/*****************************************************************************/
/* Python TypeBone structure definition:                                     */
/*****************************************************************************/
PyTypeObject Bone_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                    /* ob_size */
  "Blender Bone",                       /* tp_name */
  sizeof (BPy_Bone),                    /* tp_basicsize */
  0,                                    /* tp_itemsize */
  /* methods */
  (destructor)Bone_dealloc,             /* tp_dealloc */
  0,                                    /* tp_print */
  (getattrfunc)Bone_getAttr,            /* tp_getattr */
  (setattrfunc)Bone_setAttr,            /* tp_setattr */
  (cmpfunc)Bone_compare,                /* tp_compare */
  (reprfunc)Bone_repr,                  /* tp_repr */
  0,                                    /* tp_as_number */
  0,                                    /* tp_as_sequence */
  0,                                    /* tp_as_mapping */
  0,                                    /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                    /* tp_doc */ 
  0,0,0,0,0,0,
  BPy_Bone_methods,                     /* tp_methods */
  0,                                    /* tp_members */
};




/*****************************************************************************/
/* Function:              M_Bone_New                                         */
/* Python equivalent:     Blender.Armature.Bone.New                          */
/*****************************************************************************/
static PyObject *M_Bone_New(PyObject *self, PyObject *args, PyObject *keywords)
{
  char        *name_str = "BoneName";
  BPy_Bone    *py_bone = NULL; /* for Bone Data object wrapper in Python */
  Bone        *bl_bone = NULL; /* for actual Bone Data we create in Blender */

  if (!PyArg_ParseTuple(args, "|s", &name_str))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected string or empty argument"));

  // Create the C structure for the newq bone
  bl_bone = (Bone*)malloc(sizeof(Bone));
  strncpy(bl_bone->name,name_str,sizeof(bl_bone->name));
  
  if (bl_bone) /* now create the wrapper obj in Python */
    py_bone = (BPy_Bone *)PyObject_NEW(BPy_Bone, &Bone_Type);
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                            "couldn't create Bone Data in Blender"));

  if (py_bone == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                            "couldn't create Bone Data object"));

  py_bone->bone = bl_bone; /* link Python bone wrapper with Blender Bone */

  if (strcmp(name_str, "BoneData") == 0)
    return (PyObject *)py_bone;
  else { /* user gave us a name for the bone, use it */
    // TODO: check that name is not already in use?
    PyOS_snprintf(bl_bone->name, sizeof(bl_bone->name), "%s", name_str);
  }

  return (PyObject *)py_bone;
}


/*****************************************************************************/
/* Function:              Bone_Init                                          */
/*****************************************************************************/
PyObject *Bone_Init (void)
{
  PyObject  *submodule;

  Bone_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3("Blender.Armature.Bone",
                             M_Bone_methods, M_Bone_doc);

  return (submodule);
}

/*****************************************************************************/
/* Python BPy_Bone methods:                                                  */
/*****************************************************************************/
static PyObject *Bone_getName(BPy_Bone *self)
{
  PyObject *attr=NULL;
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  attr = PyString_FromString(self->bone->name);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Bone.name attribute"));
}


static PyObject *Bone_getRoll(BPy_Bone *self)
{
  PyObject *attr=NULL;
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  attr = Py_BuildValue("f", self->bone->roll);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Bone.roll attribute"));
}


static PyObject *Bone_getHead(BPy_Bone *self)
{
  PyObject *attr=NULL;
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  attr = Py_BuildValue("[fff]", self->bone->head[0],self->bone->head[1],
                  self->bone->head[2]);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Bone.head attribute"));
}


static PyObject *Bone_getTail(BPy_Bone *self)
{
  PyObject *attr=NULL;
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  attr = Py_BuildValue("[fff]", self->bone->tail[0],self->bone->tail[1],
                  self->bone->tail[2]);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Bone.tail attribute"));
}


static PyObject *Bone_getLoc (BPy_Bone *self)
{
  PyObject *attr=NULL;
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  attr = Py_BuildValue("[fff]", self->bone->loc[0],self->bone->loc[1],
                  self->bone->loc[2]);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Bone.loc attribute"));
}


static PyObject *Bone_getSize(BPy_Bone *self)
{
  PyObject *attr=NULL;
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  attr = Py_BuildValue("[fff]", self->bone->size[0],self->bone->size[1],
                  self->bone->size[2]);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Bone.size attribute"));
}


static PyObject *Bone_getQuat(BPy_Bone *self)
{
  PyObject *attr=NULL;
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  attr = Py_BuildValue("[ffff]", self->bone->quat[0],self->bone->quat[1],
                  self->bone->quat[2],self->bone->quat[3]);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Bone.tail attribute"));
}


static PyObject *Bone_hasParent(BPy_Bone *self)
{
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));

  /*
  return Bone_CreatePyObject(self->bone->parent);
  */
  if (self->bone->parent)
    {
      Py_INCREF(Py_True);
      return Py_True;
    }
  else
    {
      Py_INCREF(Py_False);
      return Py_False;
    }
  
}


static PyObject *Bone_getParent(BPy_Bone *self)
{
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));

  if (self->bone->parent) return Bone_CreatePyObject(self->bone->parent);
  else /*(EXPP_ReturnPyObjError (PyExc_RuntimeError,
         "couldn't get parent bone, because bone hasn't got a parent."));*/
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
  
}


static PyObject *Bone_getChildren(BPy_Bone *self)
{
  int totbones = 0;
  Bone* current = NULL;
  PyObject *listbones = NULL;
  int i;
  
  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  /* Count the number of bones to create the list */
  current = self->bone->childbase.first;
  for (;current; current=current->next) totbones++;

  /* Create a list with a bone wrapper for each bone */
  current = self->bone->childbase.first;  
  listbones = PyList_New(totbones);
  for (i=0; i<totbones; i++) {
    assert(current);
    PyList_SetItem(listbones, i, Bone_CreatePyObject(current));
    current = current->next;
  }

  return listbones;  
}


static PyObject *Bone_setName(BPy_Bone *self, PyObject *args)
{
  char *name;

  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  if (!PyArg_ParseTuple(args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected string argument"));
  
  PyOS_snprintf(self->bone->name, sizeof(self->bone->name), "%s", name);

  Py_INCREF(Py_None);
  return Py_None;
}


PyObject *Bone_setRoll(BPy_Bone *self, PyObject *args)
{
  float roll;

  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  if (!PyArg_ParseTuple(args, "f", &roll))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected float argument"));
  
  self->bone->roll = roll;

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Bone_setHead(BPy_Bone *self, PyObject *args)
{
  float f1,f2,f3;

  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  if (!PyArg_ParseTuple(args, "fff", &f1,&f2,&f3))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected 3 float arguments"));
  
  self->bone->head[0] = f1;
  self->bone->head[1] = f2;
  self->bone->head[2] = f3;

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Bone_setTail(BPy_Bone *self, PyObject *args)
{
  float f1,f2,f3;

  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  if (!PyArg_ParseTuple(args, "fff", &f1,&f2,&f3))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected 3 float arguments"));
  
  self->bone->tail[0] = f1;
  self->bone->tail[1] = f2;
  self->bone->tail[2] = f3;

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Bone_setLoc(BPy_Bone *self, PyObject *args)
{
  float f1,f2,f3;

  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  if (!PyArg_ParseTuple(args, "fff", &f1,&f2,&f3))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected 3 float arguments"));
  
  self->bone->loc[0] = f1;
  self->bone->loc[1] = f2;
  self->bone->loc[2] = f3;

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Bone_setSize(BPy_Bone *self, PyObject *args)
{
  float f1,f2,f3;

  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  if (!PyArg_ParseTuple(args, "fff", &f1,&f2,&f3))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected 3 float arguments"));
  
  self->bone->size[0] = f1;
  self->bone->size[1] = f2;
  self->bone->size[2] = f3;

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *Bone_setQuat(BPy_Bone *self, PyObject *args)
{
  float f1,f2,f3,f4;

  if (!self->bone) (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                          "couldn't get attribute from a NULL bone"));
    
  if (!PyArg_ParseTuple(args, "ffff", &f1,&f2,&f3,&f4))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected 4 float arguments"));
  
  self->bone->head[0] = f1;
  self->bone->head[1] = f2;
  self->bone->head[2] = f3;
  self->bone->head[3] = f4;

  Py_INCREF(Py_None);
  return Py_None;
}


/*****************************************************************************/
/* Function:    Bone_dealloc                                                 */
/* Description: This is a callback function for the BPy_Bone type. It is     */
/*              the destructor function.                                     */
/*****************************************************************************/
static void Bone_dealloc (BPy_Bone *self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    Bone_getAttr                                                 */
/* Description: This is a callback function for the BPy_Bone type. It is     */
/*              the function that accesses BPy_Bone member variables and     */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject* Bone_getAttr (BPy_Bone *self, char *name)
{
  PyObject *attr = Py_None;

  if (strcmp(name, "name") == 0)
    attr = Bone_getName(self);
  else if (strcmp(name, "roll") == 0)
    attr = Bone_getRoll(self);
  else if (strcmp(name, "head") == 0)
    attr = Bone_getHead(self);
  else if (strcmp(name, "tail") == 0)
    attr = Bone_getTail(self);
  else if (strcmp(name, "size") == 0)
    attr = Bone_getSize(self);
  else if (strcmp(name, "loc") == 0)
    attr = Bone_getLoc(self);
  else if (strcmp(name, "quat") == 0)
    attr = Bone_getQuat(self);
  else if (strcmp(name, "parent") == 0)
    // Skip the checks for Py_None as its a valid result to this call.
    return Bone_getParent(self);  
  else if (strcmp(name, "children") == 0)
    attr = Bone_getChildren(self);
  else if (strcmp(name, "__members__") == 0) {
    /* 9 entries */
    attr = Py_BuildValue("[s,s,s,s,s,s,s,s,s]",
       "name","roll","head","tail","loc","size",
       "quat","parent","children");
  }

  if (!attr)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                      "couldn't create PyObject"));

  if (attr != Py_None) return attr; /* member attribute found, return it */

  /* not an attribute, search the methods table */
  return Py_FindMethod(BPy_Bone_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    Bone_setAttr                                                 */
/* Description: This is a callback function for the BPy_Bone type. It is the */
/*              function that changes Bone Data members values. If this      */
/*              data is linked to a Blender Bone, it also gets updated.      */
/*****************************************************************************/
static int Bone_setAttr (BPy_Bone *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;

  valtuple = Py_BuildValue("(O)", value); /* the set* functions expect a tuple */

  if (!valtuple)
    return EXPP_ReturnIntError(PyExc_MemoryError,
                  "BoneSetAttr: couldn't create tuple");

  if (strcmp (name, "name") == 0)
    error = Bone_setName (self, valtuple);
  else { /* Error */
    Py_DECREF(valtuple);
  
    /* ... member with the given name was found */
    return (EXPP_ReturnIntError (PyExc_KeyError,
         "attribute not found"));
  }

  Py_DECREF(valtuple);
  
  if (error != Py_None) return -1;

  Py_DECREF(Py_None); /* was incref'ed by the called Bone_set* function */
  return 0; /* normal exit */
}

/*****************************************************************************/
/* Function:    Bone_repr                                                    */
/* Description: This is a callback function for the BPy_Bone type. It        */
/*              builds a meaninful string to represent bone objects.         */
/*****************************************************************************/
static PyObject *Bone_repr (BPy_Bone *self)
{
  if (self->bone)
		return PyString_FromFormat("[Bone \"%s\"]", self->bone->name);
  else return PyString_FromString("NULL");
}

/**************************************************************************/
/* Function:    Bone_compare                                              */
/* Description: This is a callback function for the BPy_Bone type. It     */
/*              compares the two bones: translate comparison to the       */
/*              C pointers.                                               */
/**************************************************************************/
static int Bone_compare (BPy_Bone *a, BPy_Bone *b)
{
  Bone *pa = a->bone, *pb = b->bone;
	return (pa == pb) ? 0:-1;
}




/*****************************************************************************/
/* Function:    Bone_CreatePyObject                                          */
/* Description: This function will create a new BlenBone from an existing    */
/*              Bone structure.                                              */
/*****************************************************************************/
PyObject* Bone_CreatePyObject (struct Bone *obj)
{
  BPy_Bone    * blen_bone;

  blen_bone = (BPy_Bone*)PyObject_NEW (BPy_Bone, &Bone_Type);

  if (blen_bone == NULL)
    {
      return (NULL);
    }
  blen_bone->bone = obj;
  return ((PyObject*)blen_bone);
}

/*****************************************************************************/
/* Function:    Bone_CheckPyObject                                           */
/* Description: This function returns true when the given PyObject is of the */
/*              type Bone. Otherwise it will return false.                   */
/*****************************************************************************/
int Bone_CheckPyObject (PyObject *py_obj)
{
  return (py_obj->ob_type == &Bone_Type);
}

/*****************************************************************************/
/* Function:    Bone_FromPyObject                                            */
/* Description: This function returns the Blender bone from the given        */
/*              PyObject.                                                    */
/*****************************************************************************/
struct Bone* Bone_FromPyObject (PyObject *py_obj)
{
  BPy_Bone    * blen_obj;

  blen_obj = (BPy_Bone*)py_obj;
  return (blen_obj->bone);
}
