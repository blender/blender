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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Jordi Rovira i Bonet, Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Bone.h"

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_armature.h>
#include <BKE_library.h>
#include <MEM_guardedalloc.h>
#include <BLI_blenlib.h>
#include <DNA_action_types.h>
#include <BIF_poseobject.h>
#include <BKE_action.h>
#include <BSE_editaction.h>
#include <BKE_constraint.h>

#include "constant.h"
#include "gen_utils.h"
#include "modules.h"
#include "quat.h"
#include "NLA.h"

/*****************************************************************************/
/* Python API function prototypes for the Bone module.			 */
/*****************************************************************************/
static PyObject *M_Bone_New (PyObject * self, PyObject * args,
			     PyObject * keywords);


/*****************************************************************************/
/* The following string definitions are used for documentation strings.	  	 */
/* In Python these will be written to the console when doing a		 */
/* Blender.Armature.Bone.__doc__					 */
/*****************************************************************************/
char M_Bone_doc[] = "The Blender Bone module\n\n\
This module provides control over **Bone Data** objects in Blender.\n\n\
Example::\n\n\
	from Blender import Armature.Bone\n\
	l = Armature.Bone.New()\n";

char M_Bone_New_doc[] = "(name) - return a new Bone of name 'name'.";


/*****************************************************************************/
/* Python method structure definition for Blender.Armature.Bone module:			 */
/*****************************************************************************/
struct PyMethodDef M_Bone_methods[] = {
  {"New", (PyCFunction) M_Bone_New, METH_VARARGS | METH_KEYWORDS,
   M_Bone_New_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Bone methods declarations:																		 */
/*****************************************************************************/
static PyObject *Bone_getName (BPy_Bone * self);
static PyObject *Bone_getRoll (BPy_Bone * self);
static PyObject *Bone_getHead (BPy_Bone * self);
static PyObject *Bone_getTail (BPy_Bone * self);
static PyObject *Bone_getLoc (BPy_Bone * self);
static PyObject *Bone_getSize (BPy_Bone * self);
static PyObject *Bone_getQuat (BPy_Bone * self);
static PyObject *Bone_getParent (BPy_Bone * self);
static PyObject *Bone_hasParent (BPy_Bone * self);
static PyObject *Bone_getWeight (BPy_Bone * self);
static PyObject *Bone_getChildren (BPy_Bone * self);
static PyObject *Bone_clearParent (BPy_Bone * self);
static PyObject *Bone_clearChildren (BPy_Bone * self);
static PyObject *Bone_hide (BPy_Bone * self);
static PyObject *Bone_unhide (BPy_Bone * self);
static PyObject *Bone_setName (BPy_Bone * self, PyObject * args);
static PyObject *Bone_setRoll (BPy_Bone * self, PyObject * args);
static PyObject *Bone_setHead (BPy_Bone * self, PyObject * args);
static PyObject *Bone_setTail (BPy_Bone * self, PyObject * args);
static PyObject *Bone_setLoc (BPy_Bone * self, PyObject * args);
static PyObject *Bone_setSize (BPy_Bone * self, PyObject * args);
static PyObject *Bone_setQuat (BPy_Bone * self, PyObject * args);
static PyObject *Bone_setParent(BPy_Bone *self, PyObject *args);
static PyObject *Bone_setWeight(BPy_Bone *self, PyObject *args);
static PyObject *Bone_setPose (BPy_Bone *self, PyObject *args);

/*****************************************************************************/
/* Python BPy_Bone methods table:					 */
/*****************************************************************************/
static PyMethodDef BPy_Bone_methods[] = {
  /* name, method, flags, doc */
  {"getName", (PyCFunction) Bone_getName, METH_NOARGS,
   "() - return Bone name"},
  {"getRoll", (PyCFunction) Bone_getRoll, METH_NOARGS,
   "() - return Bone roll"},
  {"getHead", (PyCFunction) Bone_getHead, METH_NOARGS,
   "() - return Bone head"},
  {"getTail", (PyCFunction) Bone_getTail, METH_NOARGS,
   "() - return Bone tail"},
  {"getLoc", (PyCFunction) Bone_getLoc, METH_NOARGS, "() - return Bone loc"},
  {"getSize", (PyCFunction) Bone_getSize, METH_NOARGS,
   "() - return Bone size"},
  {"getQuat", (PyCFunction) Bone_getQuat, METH_NOARGS,
   "() - return Bone quat"},
  {"hide", (PyCFunction) Bone_hide, METH_NOARGS,
   "() - hides the bone"},
  {"unhide", (PyCFunction) Bone_unhide, METH_NOARGS,
   "() - unhides the bone"},
  {"getWeight", (PyCFunction) Bone_getWeight, METH_NOARGS,
   "() - return Bone weight"},
  {"getParent", (PyCFunction) Bone_getParent, METH_NOARGS,
   "() - return the parent bone of this one if it exists."
   " None if not found. You can check this condition with the "
   "hasParent() method."},
  {"hasParent", (PyCFunction) Bone_hasParent, METH_NOARGS,
   "() - return true if bone has a parent"},
  {"getChildren", (PyCFunction) Bone_getChildren, METH_NOARGS,
   "() - return Bone children list"},
  {"clearParent", (PyCFunction) Bone_clearParent, METH_NOARGS,
   "() - clears the bone's parent in the armature and makes it root"},
  {"clearChildren", (PyCFunction) Bone_clearChildren, METH_NOARGS,
   "() - remove the children associated with this bone"},
  {"setName", (PyCFunction) Bone_setName, METH_VARARGS,
   "(str) - rename Bone"},
  {"setRoll", (PyCFunction) Bone_setRoll, METH_VARARGS,
   "(float) - set Bone roll"},
  {"setHead", (PyCFunction) Bone_setHead, METH_VARARGS,
   "(float,float,float) - set Bone head pos"},
  {"setTail", (PyCFunction) Bone_setTail, METH_VARARGS,
   "(float,float,float) - set Bone tail pos"},
  {"setLoc", (PyCFunction) Bone_setLoc, METH_VARARGS,
   "(float,float,float) - set Bone loc"},
  {"setSize", (PyCFunction) Bone_setSize, METH_VARARGS,
   "(float,float,float) - set Bone size"},
  {"setQuat", (PyCFunction) Bone_setQuat, METH_VARARGS,
   "(float,float,float,float) - set Bone quat"},
  {"setParent", (PyCFunction)Bone_setParent, METH_VARARGS,  
   "() - set the Bone parent of this one."},
  {"setWeight", (PyCFunction)Bone_setWeight, METH_VARARGS,  
   "() - set the Bone weight."},
  {"setPose", (PyCFunction)Bone_setPose, METH_VARARGS,  
   "() - set a pose for this bone at a frame."},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python TypeBone callback function prototypes:				 */
/*****************************************************************************/
static void Bone_dealloc (BPy_Bone * bone);
static PyObject *Bone_getAttr (BPy_Bone * bone, char *name);
static int Bone_setAttr (BPy_Bone * bone, char *name, PyObject * v);
static int Bone_compare (BPy_Bone * a1, BPy_Bone * a2);
static PyObject *Bone_repr (BPy_Bone * bone);

/*****************************************************************************/
/* Python TypeBone structure definition:				 */
/*****************************************************************************/
PyTypeObject Bone_Type = {
  PyObject_HEAD_INIT (NULL) 0,	/* ob_size */
  "Blender Bone",		/* tp_name */
  sizeof (BPy_Bone),		/* tp_basicsize */
  0,				/* tp_itemsize */
  /* methods */
  (destructor) Bone_dealloc,	/* tp_dealloc */
  0,				/* tp_print */
  (getattrfunc) Bone_getAttr,	/* tp_getattr */
  (setattrfunc) Bone_setAttr,	/* tp_setattr */
  (cmpfunc) Bone_compare,	/* tp_compare */
  (reprfunc) Bone_repr,		/* tp_repr */
  0,				/* tp_as_number */
  0,				/* tp_as_sequence */
  0,				/* tp_as_mapping */
  0,				/* tp_as_hash */
  0, 0, 0, 0, 0, 0,
  0,				/* tp_doc */
  0, 0, 0, 0, 0, 0,
  BPy_Bone_methods,		/* tp_methods */
  0,				/* tp_members */
};


/*****************************************************************************/
/* Function:	M_Bone_New						 */
/* Python equivalent:	Blender.Armature.Bone.New			 */
/*****************************************************************************/

static PyObject *
M_Bone_New (PyObject * self, PyObject * args, PyObject * keywords)
{
  char *name_str = "BoneName";
  BPy_Bone *py_bone = NULL;	/* for Bone Data object wrapper in Python */
  Bone *bl_bone = NULL;		/* for actual Bone Data we create in Blender */

  if (!PyArg_ParseTuple (args, "|s", &name_str))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected string or empty argument"));

  /*  Create the C structure for the newq bone */
  bl_bone = (Bone *) MEM_callocN(sizeof (Bone), "bone");
  strncpy (bl_bone->name, name_str, sizeof (bl_bone->name));

  bl_bone->dist=1.0;
  bl_bone->weight=1.0;
  bl_bone->flag=32;
  bl_bone->parent = NULL;
  bl_bone->roll = 0.0;
  bl_bone->boneclass = BONE_SKINNABLE;

  // now create the wrapper obj in Python
  if (bl_bone)				
    py_bone = (BPy_Bone *) PyObject_NEW (BPy_Bone, &Bone_Type);
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				   "couldn't create Bone Data in Blender"));

  if (py_bone == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				   "couldn't create Bone Data object"));

  py_bone->bone = bl_bone;	// link Python bone wrapper with Blender Bone
 
  Py_INCREF(py_bone);
  return (PyObject *) py_bone;
}


/*****************************************************************************/
/* Function:	Bone_Init						 */
/*****************************************************************************/
PyObject *
Bone_Init (void)
{
  PyObject *submodule;

  Bone_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3 ("Blender.Armature.Bone",
			      M_Bone_methods, M_Bone_doc);

  PyModule_AddIntConstant(submodule, "ROT",  POSE_ROT);
  PyModule_AddIntConstant(submodule, "LOC",  POSE_LOC);
  PyModule_AddIntConstant(submodule, "SIZE", POSE_SIZE);

  return (submodule);
}

/*****************************************************************************/
/* Python BPy_Bone methods:						 */
/*****************************************************************************/
static PyObject *
Bone_getName (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  attr = PyString_FromString (self->bone->name);

  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.name attribute"));
}


static PyObject *
Bone_getRoll (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  attr = Py_BuildValue ("f", self->bone->roll);

  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.roll attribute"));
}

static PyObject *
Bone_getWeight (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  attr = Py_BuildValue ("f", self->bone->weight);

  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.weight attribute"));
}

static PyObject *
Bone_getHead (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  attr = Py_BuildValue ("[fff]", self->bone->head[0], self->bone->head[1],
			self->bone->head[2]);

  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.head attribute"));
}


static PyObject *
Bone_getTail (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  attr = Py_BuildValue ("[fff]", self->bone->tail[0], self->bone->tail[1],
			self->bone->tail[2]);

  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.tail attribute"));
}


static PyObject *
Bone_getLoc (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  attr = Py_BuildValue ("[fff]", self->bone->loc[0], self->bone->loc[1],
			self->bone->loc[2]);

  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.loc attribute"));
}


static PyObject *
Bone_getSize (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  attr = Py_BuildValue ("[fff]", self->bone->size[0], self->bone->size[1],
			self->bone->size[2]);

  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.size attribute"));
}


static PyObject *
Bone_getQuat (BPy_Bone * self)
{
  float *quat;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  quat = PyMem_Malloc (4*sizeof (float));
  quat[0] = self->bone->quat[0];
  quat[1] = self->bone->quat[1];
  quat[2] = self->bone->quat[2];
  quat[3] = self->bone->quat[3];

  return (PyObject*)newQuaternionObject(quat);
}

static PyObject *
Bone_hasParent (BPy_Bone * self)
{

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  /*
     return Bone_CreatePyObject(self->bone->parent);
   */
  if (self->bone->parent)
    {
      Py_INCREF (Py_True);
      return Py_True;
    }
  else
    {
      Py_INCREF (Py_False);
      return Py_False;
    }

}


static PyObject *
Bone_getParent (BPy_Bone * self)
{

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  if (self->bone->parent)
    return Bone_CreatePyObject (self->bone->parent);
  else				/*(EXPP_ReturnPyObjError (PyExc_RuntimeError,
				   "couldn't get parent bone, because bone hasn't got a parent.")); */
    {
      Py_INCREF (Py_None);
      return Py_None;
    }

}


static PyObject *
Bone_getChildren (BPy_Bone * self)
{
  int totbones = 0;
  Bone *current = NULL;
  PyObject *listbones = NULL;
  int i;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  /* Count the number of bones to create the list */
  current = self->bone->childbase.first;
  for (; current; current = current->next)
    totbones++;

  /* Create a list with a bone wrapper for each bone */
  current = self->bone->childbase.first;
  listbones = PyList_New (totbones);
  for (i = 0; i < totbones; i++)
    {
      assert (current);
      PyList_SetItem (listbones, i, Bone_CreatePyObject (current));
      current = current->next;
    }

  return listbones;
}


static PyObject *
Bone_setName (BPy_Bone * self, PyObject * args)
{
  char *name;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  if (!PyArg_ParseTuple (args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected string argument"));

  PyOS_snprintf (self->bone->name, sizeof (self->bone->name), "%s", name);

  Py_INCREF (Py_None);
  return Py_None;
}


PyObject *
Bone_setRoll (BPy_Bone * self, PyObject * args)
{
  float roll;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  if (!PyArg_ParseTuple (args, "f", &roll))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected float argument"));

  self->bone->roll = roll;

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
Bone_setHead (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3;
  int status;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  if (PyObject_Length (args) == 3)
    status = PyArg_ParseTuple (args, "fff", &f1, &f2, &f3);
  else
    status = PyArg_ParseTuple (args, "(fff)", &f1, &f2, &f3);

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected 3 (or a list of 3) float arguments"));

  self->bone->head[0] = f1;
  self->bone->head[1] = f2;
  self->bone->head[2] = f3;

  Py_INCREF (Py_None);
  return Py_None;
}


static PyObject *
Bone_setTail (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3;
  int status;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  if (PyObject_Length (args) == 3)
    status = PyArg_ParseTuple (args, "fff", &f1, &f2, &f3);
  else
    status = PyArg_ParseTuple (args, "(fff)", &f1, &f2, &f3);

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected 3 (or a list of 3) float arguments"));

  self->bone->tail[0] = f1;
  self->bone->tail[1] = f2;
  self->bone->tail[2] = f3;

  Py_INCREF (Py_None);
  return Py_None;
}


static PyObject *
Bone_setLoc (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3;
  int status;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  if (PyObject_Length (args) == 3)
    status = PyArg_ParseTuple (args, "fff", &f1, &f2, &f3);
  else
    status = PyArg_ParseTuple (args, "(fff)", &f1, &f2, &f3);

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected 3 (or a list of 3) float arguments"));

  self->bone->loc[0] = f1;
  self->bone->loc[1] = f2;
  self->bone->loc[2] = f3;

  Py_INCREF (Py_None);
  return Py_None;
}


static PyObject *
Bone_setSize (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3;
  int status;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  if (PyObject_Length (args) == 3)
    status = PyArg_ParseTuple (args, "fff", &f1, &f2, &f3);
  else
    status = PyArg_ParseTuple (args, "(fff)", &f1, &f2, &f3);

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected 3 (or a list of 3) float arguments"));

  self->bone->size[0] = f1;
  self->bone->size[1] = f2;
  self->bone->size[2] = f3;

  Py_INCREF (Py_None);
  return Py_None;
}


static PyObject *
Bone_setQuat (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3, f4;
  PyObject *argument;
  QuaternionObject *quatOb;
  int status;

  if (!self->bone)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

   if (!PyArg_ParseTuple(args, "O", &argument))
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected quaternion or float list"));

   if(QuaternionObject_Check(argument)){
		status = PyArg_ParseTuple(args, "O!", &quaternion_Type, &quatOb);
		f1 = quatOb->quat[0];
		f2 = quatOb->quat[1];
		f3 = quatOb->quat[2];
		f4 = quatOb->quat[3];
   }else{
		status = PyArg_ParseTuple (args, "(ffff)", &f1, &f2, &f3, &f4);
   }

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
  				   "unable to parse argument"));

  self->bone->quat[0] = f1;
  self->bone->quat[1] = f2;
  self->bone->quat[2] = f3;
  self->bone->quat[3] = f4;

  Py_INCREF (Py_None);
  return Py_None;
}

static int
testChildbase(Bone *bone, Bone *test)
{
	Bone *child;

	for(child = bone->childbase.first; child; child = child->next){
		if(child == test){
			return 1;
		}
		if(child->childbase.first != NULL)
			testChildbase(child, test);
	}

	return 0;
}

static PyObject *
Bone_setParent(BPy_Bone *self, PyObject *args)
{
  BPy_Bone* py_bone;

  if (!self->bone) 
	  (EXPP_ReturnPyObjError (PyExc_RuntimeError, "bone contains no data!"));

  if (!PyArg_ParseTuple(args, "O", &py_bone))
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected bone object argument"));

  if(!py_bone->bone)
	return (EXPP_ReturnPyObjError (PyExc_TypeError, "bone contains no data!"));

  if(py_bone->bone == self->bone)
	  return (EXPP_ReturnPyObjError (PyExc_AttributeError, "Cannot parent to self"));

  //test to see if were creating an illegal loop by parenting to child
  if(testChildbase(self->bone, py_bone->bone))
	  return (EXPP_ReturnPyObjError (PyExc_AttributeError, "Cannot parent to child"));

  //set the parent of self
  self->bone->parent = py_bone->bone;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
Bone_setWeight(BPy_Bone *self, PyObject *args)
{
  float weight;

  if (!self->bone)
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL bone"));

  if (!PyArg_ParseTuple (args, "f", &weight))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected float argument"));

  self->bone->weight = weight;

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
Bone_clearParent(BPy_Bone *self)
{
  bArmature *arm = NULL;
  Bone *bone = NULL;
  Bone *parent = NULL;
  Bone *child = NULL;
  Bone *childPrev = NULL;
  int firstChild;
  float M_boneObjectspace[4][4];

  if (!self->bone) 
	  return (EXPP_ReturnPyObjError (PyExc_RuntimeError, "bone contains no data!"));

  if(self->bone->parent == NULL)
	  return EXPP_incr_ret(Py_None);

  //get parent and remove link
  parent = self->bone->parent;
  self->bone->parent = NULL;

  //remove the childbase link from the parent bone
  firstChild = 1;
  for(child = parent->childbase.first; child; child = child->next){	
	  if(child == self->bone && firstChild){
			parent->childbase.first = child->next;
			child->next = NULL;
			break;
	  }
	  if(child == self->bone && !firstChild){
		  childPrev->next = child->next;
		  child->next = NULL;
		  break;
	  }
	  firstChild = 0;
	  childPrev = child;
  }

  //now get rid of the parent transformation
  get_objectspace_bone_matrix(parent, M_boneObjectspace, 0,0);

  //transformation of local bone
  Mat4MulVecfl(M_boneObjectspace, self->bone->head);
  Mat4MulVecfl(M_boneObjectspace, self->bone->tail);


  //get the root bone
  while(parent->parent != NULL){
	  parent = parent->parent;
  }

  //add unlinked bone to the bonebase of the armature
  for (arm = G.main->armature.first; arm; arm= arm->id.next) {
	  for(bone = arm->bonebase.first; bone; bone = bone->next){
		  if(parent == bone){
			  //we found the correct armature - now add it as root bone
			  BLI_addtail (&arm->bonebase, self->bone);
			  break;
		  }
	  }			
  }
  
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *
Bone_clearChildren(BPy_Bone *self)
{ 
  Bone *root = NULL;
  Bone *child = NULL;
  bArmature *arm = NULL;
  Bone *bone = NULL;
  Bone *prev = NULL;
  Bone *next = NULL;
  float M_boneObjectspace[4][4];
  int first;

  if (!self->bone) 
	  return (EXPP_ReturnPyObjError (PyExc_RuntimeError, "bone contains no data!"));

  if(self->bone->childbase.first == NULL)
	  return EXPP_incr_ret(Py_None);

	//is this bone a part of an armature....
	//get root bone for testing
	root = self->bone->parent;
	if(root != NULL){
		while (root->parent != NULL){
			root = root->parent;
		}
	}else{
		root = self->bone;
	}
	//test armatures for root bone
	for(arm= G.main->armature.first; arm; arm  = arm->id.next){
		for(bone = arm->bonebase.first; bone; bone = bone->next){
			if(bone == root)
				break;
		}
		if(bone == root)
			break;
	}

  if(arm == NULL)
	 return (EXPP_ReturnPyObjError (PyExc_RuntimeError, "couldn't find armature that contains this bone"));

  //now get rid of the parent transformation
  get_objectspace_bone_matrix(self->bone, M_boneObjectspace, 0,0);

  //set children as root
  first = 1;
  for(child = self->bone->childbase.first; child; child = next){
      //undo transformation of local bone
      Mat4MulVecfl(M_boneObjectspace, child->head);
	  Mat4MulVecfl(M_boneObjectspace, child->tail);

	  //set next pointers to NULL
	  if(first){
		prev = child;
		first = 0;
	  }else{
		prev->next = NULL;
	    prev = child;
	  }
	  next = child->next;

	  //remove parenting and linking
	  child->parent = NULL;
	  BLI_remlink(&self->bone->childbase, child);

	  //add as root
	  BLI_addtail (&arm->bonebase, child);
  }

  Py_INCREF(Py_None);
  return Py_None;

}

static PyObject *
Bone_hide(BPy_Bone *self)
{
  if (!self->bone) 
	  return (EXPP_ReturnPyObjError (PyExc_RuntimeError, "bone contains no data!"));

  if(!(self->bone->flag & BONE_HIDDEN))
	self->bone->flag |= BONE_HIDDEN;

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *
Bone_unhide(BPy_Bone *self)
{
  if (!self->bone) 
	  return (EXPP_ReturnPyObjError (PyExc_RuntimeError, "bone contains no data!"));

  if(self->bone->flag & BONE_HIDDEN)
	self->bone->flag &= ~BONE_HIDDEN;

  Py_INCREF(Py_None);
  return Py_None;
}


static PyObject *
Bone_setPose (BPy_Bone *self, PyObject *args)
{
	Bone *root = NULL;
	bPoseChannel *chan = NULL;
	bPoseChannel *setChan = NULL;
	bPoseChannel *test = NULL;
	Object *object =NULL;
	bArmature *arm = NULL;
	Bone *bone = NULL;
	PyObject *flaglist = NULL;
	PyObject *item = NULL;
	BPy_Action *py_action = NULL;
	int x;
	int flagValue = 0;
	int makeCurve = 1;

	if (!PyArg_ParseTuple (args, "O!|O!", &PyList_Type, &flaglist, &Action_Type, &py_action))
			return (EXPP_ReturnPyObjError (PyExc_AttributeError,
															"expected list of flags and optional action"));

	for(x = 0; x <  PyList_Size(flaglist); x++){
		 item = PyList_GetItem(flaglist, x); 
		 if(PyInt_Check(item)){
			 flagValue |= PyInt_AsLong(item);
		 }else{
			return (EXPP_ReturnPyObjError (PyExc_AttributeError,
							"expected list of flags (ints)"));
		}
	}

	//is this bone a part of an armature....
	//get root bone for testing
	root = self->bone->parent;
	if(root != NULL){
		while (root->parent != NULL){
			root = root->parent;
		}
	}else{
		root = self->bone;
	}
	//test armatures for root bone
	for(arm= G.main->armature.first; arm; arm  = arm->id.next){
		for(bone = arm->bonebase.first; bone; bone = bone->next){
			if(bone == root)
				break;
		}
		if(bone == root)
			break;
	}

	if(arm == NULL)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError, 
			"bone must belong to an armature to set it's pose!"));

	//find if armature is object linked....
	for(object = G.main->object.first; object; object  = object->id.next){
		if(object->data == arm){
			break;
		}
	}

	if(object == NULL)
 		return (EXPP_ReturnPyObjError (PyExc_RuntimeError, 
			"armature must be linked to an object to set a pose!"));

	//set the active action as this one
	if(py_action !=NULL){
		if(py_action->action != NULL){
            object->action = py_action->action;
		}
	}

	//if object doesn't have a pose create one
	if (!object->pose) 
		object->pose = MEM_callocN(sizeof(bPose), "Pose");

	//if bone does have a channel create one
	verify_pose_channel(object->pose, self->bone->name);

	//create temp Pose Channel
	chan = MEM_callocN(sizeof(bPoseChannel), "PoseChannel");
	//set the variables for this pose
	memcpy (chan->loc, self->bone->loc, sizeof (chan->loc));
	memcpy (chan->quat, self->bone->quat, sizeof (chan->quat));
	memcpy (chan->size, self->bone->size, sizeof (chan->size));
	strcpy (chan->name, self->bone->name);
	chan->flag |= flagValue;

	//set it to the channel
	setChan = set_pose_channel(object->pose, chan);

	//frees unlinked pose/bone channels from object
	collect_pose_garbage(object);

	//create an action if one not already assigned to object
	if (!py_action && !object->action){
		object->action = (bAction*)add_empty_action();
		object->ipowin= ID_AC;
	}else{
		  //test if posechannel is already in action
		for(test = object->action->chanbase.first; test; test = test->next){
			if(test == setChan)
				makeCurve = 0; //already there
		}
	}

   //set posekey flag
   filter_pose_keys ();

 	//set action keys
	if (setChan->flag & POSE_ROT){
		set_action_key(object->action, setChan, AC_QUAT_X, makeCurve);
		set_action_key(object->action, setChan, AC_QUAT_Y, makeCurve);
		set_action_key(object->action, setChan, AC_QUAT_Z, makeCurve);
		set_action_key(object->action, setChan, AC_QUAT_W, makeCurve);
	}
	if (setChan->flag & POSE_SIZE){
		set_action_key(object->action, setChan, AC_SIZE_X, makeCurve);
		set_action_key(object->action, setChan, AC_SIZE_Y, makeCurve);
		set_action_key(object->action, setChan, AC_SIZE_Z, makeCurve);
	}
	if (setChan->flag & POSE_LOC){
		set_action_key(object->action, setChan, AC_LOC_X, makeCurve);
		set_action_key(object->action, setChan, AC_LOC_Y, makeCurve);
		set_action_key(object->action, setChan, AC_LOC_Z, makeCurve);
	}

	//rebuild ipos
	remake_action_ipos(object->action);

	//rebuild displists
	rebuild_all_armature_displists();

	Py_INCREF(Py_None);
    return Py_None;
}
   
/*****************************************************************************/
/* Function:	Bone_dealloc												*/
/* Description: This is a callback function for the BPy_Bone type. It is     */
/*		the destructor function.											*/
/*****************************************************************************/
static void
Bone_dealloc (BPy_Bone * self)
{
	MEM_freeN(self->bone);
    PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:	Bone_getAttr												*/
/* Description: This is a callback function for the BPy_Bone type. It is    */
/*	     	the function that accesses BPy_Bone member variables and	 */
/*		methods.							 */
/*****************************************************************************/
static PyObject *
Bone_getAttr (BPy_Bone * self, char *name)
{
  PyObject *attr = Py_None;

  if (strcmp (name, "name") == 0)
    attr = Bone_getName (self);
  else if (strcmp (name, "roll") == 0)
    attr = Bone_getRoll (self);
  else if (strcmp (name, "head") == 0)
    attr = Bone_getHead (self);
  else if (strcmp (name, "tail") == 0)
    attr = Bone_getTail (self);
  else if (strcmp (name, "size") == 0)
    attr = Bone_getSize (self);
  else if (strcmp (name, "loc") == 0)
    attr = Bone_getLoc (self);
  else if (strcmp (name, "quat") == 0)
    attr = Bone_getQuat (self);
  else if (strcmp (name, "parent") == 0)
    /*  Skip the checks for Py_None as its a valid result to this call. */
    return Bone_getParent (self);
  else if (strcmp (name, "children") == 0)
    attr = Bone_getChildren (self);
  else if (strcmp (name, "weight") == 0)
    attr = Bone_getWeight (self);
  else if (strcmp (name, "__members__") == 0)
    {
      /* 9 entries */
      attr = Py_BuildValue ("[s,s,s,s,s,s,s,s,s]",
			    "name", "roll", "head", "tail", "loc", "size",
			    "quat", "parent", "children", "weight");
    }

  if (!attr)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				   "couldn't create PyObject"));

  if (attr != Py_None)
    return attr;		/* member attribute found, return it */

  /* not an attribute, search the methods table */
  return Py_FindMethod (BPy_Bone_methods, (PyObject *) self, name);
}

/*****************************************************************************/
/* Function:		Bone_setAttr											*/
/* Description: This is a callback function for the BPy_Bone type. It is the */
/*		function that changes Bone Data members values. If this	     */
/*		data is linked to a Blender Bone, it also gets updated.	     */
/*****************************************************************************/
static int
Bone_setAttr (BPy_Bone * self, char *name, PyObject * value)
{
  PyObject *valtuple;
  PyObject *error = NULL;

  valtuple = Py_BuildValue ("(O)", value);	/* the set* functions expect a tuple */

  if (!valtuple)
    return EXPP_ReturnIntError (PyExc_MemoryError,
				"BoneSetAttr: couldn't create tuple");

  if (strcmp (name, "name") == 0)
    error = Bone_setName (self, valtuple);
  else
    {				/* Error */
      Py_DECREF (valtuple);

      /* ... member with the given name was found */
      return (EXPP_ReturnIntError (PyExc_KeyError, "attribute not found"));
    }

  Py_DECREF (valtuple);

  if (error != Py_None)
    return -1;

  Py_DECREF (Py_None);		/* was incref'ed by the called Bone_set* function */
  return 0;			/* normal exit */
}

/*****************************************************************************/
/* Function:	Bone_repr													*/
/* Description: This is a callback function for the BPy_Bone type. It	 */
/*		builds a meaninful string to represent bone objects.	 */
/*****************************************************************************/
static PyObject *
Bone_repr (BPy_Bone * self)
{
  if (self->bone)
    return PyString_FromFormat ("[Bone \"%s\"]", self->bone->name);
  else
    return PyString_FromString ("NULL");
}

/**************************************************************************/
/* Function:	Bone_compare 											*/
/* Description: This is a callback function for the BPy_Bone type. It	*/
/*		compares the two bones: translate comparison to the	*/
/*		C pointers.														*/
/**************************************************************************/
static int
Bone_compare (BPy_Bone * a, BPy_Bone * b)
{
  Bone *pa = a->bone, *pb = b->bone;
  return (pa == pb) ? 0 : -1;
}

/*****************************************************************************/
/* Function:	Bone_CreatePyObject 										*/
/* Description: This function will create a new BlenBone from an existing    */
/*		Bone structure.														*/
/*****************************************************************************/
PyObject *
Bone_CreatePyObject (struct Bone * obj)
{
  BPy_Bone *blen_bone;

  blen_bone = (BPy_Bone *) PyObject_NEW (BPy_Bone, &Bone_Type);

  if (blen_bone == NULL)
    {
      return (NULL);
    }
  blen_bone->bone = obj;
  return ((PyObject *) blen_bone);
}

/*****************************************************************************/
/* Function:	Bone_CheckPyObject											*/
/* Description: This function returns true when the given PyObject is of the */
/*		type Bone. Otherwise it will return false.							*/
/*****************************************************************************/
int
Bone_CheckPyObject (PyObject * py_obj)
{
  return (py_obj->ob_type == &Bone_Type);
}

/*****************************************************************************/
/* Function:	Bone_FromPyObject											*/
/* Description: This function returns the Blender bone from the given	 */
/*		PyObject.														*/
/*****************************************************************************/
struct Bone *
Bone_FromPyObject (PyObject * py_obj)
{
  BPy_Bone *blen_obj;

  blen_obj = (BPy_Bone *) py_obj;
  return (blen_obj->bone);
}
