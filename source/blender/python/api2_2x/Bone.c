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
#include <BLI_blenlib.h>
#include <DNA_action_types.h>
#include <BIF_poseobject.h>
#include <BKE_action.h>
#include <BSE_editaction.h>
#include <BKE_constraint.h>
#include <MEM_guardedalloc.h>
#include "constant.h"
#include "gen_utils.h"
#include "modules.h"
#include "NLA.h"
#include "quat.h"
#include "matrix.h"
#include "vector.h"

//------------------------Python API function prototypes for the Bone module---------------------------
static PyObject *M_Bone_New (PyObject * self, PyObject * args);
//------------------------Python API Doc strings for the Bone module--------------------------------------
char M_Bone_doc[] = "The Blender Bone module\n\n\
This module provides control over **Bone Data** objects in Blender.\n\n\
Example::\n\n\
	from Blender import Armature.Bone\n\
	l = Armature.Bone.New()\n";
char M_Bone_New_doc[] = "(name) - return a new Bone of name 'name'.";
//--------------- Python method structure definition for Blender.Armature.Bone module------------
struct PyMethodDef M_Bone_methods[] = {
  {"New", (PyCFunction) M_Bone_New, METH_VARARGS,   M_Bone_New_doc},
  {NULL, NULL, 0, NULL}
};
//--------------- Python BPy_Bone methods declarations:---------------------------------------------------
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
//--------------- Python BPy_Bone methods table:-----------------------------------------------------------------
static PyMethodDef BPy_Bone_methods[] = {
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
//--------------- Python TypeBone callback function prototypes----------------------------------------
static void Bone_dealloc (BPy_Bone * bone);
static PyObject *Bone_getAttr (BPy_Bone * bone, char *name);
static int Bone_setAttr (BPy_Bone * bone, char *name, PyObject * v);
static int Bone_compare (BPy_Bone * a1, BPy_Bone * a2);
static PyObject *Bone_repr (BPy_Bone * bone);
//--------------- Python TypeBone structure definition------------------------------------------------------
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
//--------------- Bone Module Init----------------------------------------------------------------------------------------
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
//--------------- Bone module internal callbacks-------------------------------------------------------------
//--------------- updatePyBone-------------------------------------------------------------------------------------
int
updatePyBone(BPy_Bone *self)
{
	int x,y;
	char *parent_str = "";

	if(!self->bone){
		//nothing to update - not linked
		return 0;
	}else{
		BLI_strncpy(self->name, self->bone->name, strlen(self->bone->name) + 1);
		self->roll = self->bone->roll;
		self->flag = self->bone->flag;
		self->boneclass = self->bone->boneclass;
		self->dist = self->bone->dist;
		self->weight = self->bone->weight;

		if(self->bone->parent){    
			self->parent = BLI_strncpy(self->parent, self->bone->parent->name, strlen(self->bone->parent->name) + 1);
		}else{
			self->parent = BLI_strncpy(self->parent, parent_str, strlen(parent_str) + 1);
		}

		for(x = 0; x < 3; x++){
			self->head->vec[x] = self->bone->head[x];
			self->tail->vec[x] = self->bone->tail[x];
			self->loc->vec[x] = self->bone->loc[x];
			self->dloc->vec[x] = self->bone->dloc[x];
			self->size->vec[x] = self->bone->size[x];
			self->dsize->vec[x] = self->bone->dsize[x];
		}
		for(x = 0; x < 4; x++){
			self->quat->quat[x] = self->bone->quat[x];
			self->dquat->quat[x] = self->bone->dquat[x];
		}
		for(x = 0; x < 4; x++){
			for(y = 0; y < 4; y++){
				self->obmat->matrix[x][y] = self->bone->obmat[x][y];
				self->parmat->matrix[x][y] = self->bone->parmat[x][y];
				self->defmat->matrix[x][y] = self->bone->defmat[x][y];
				self->irestmat->matrix[x][y] = self->bone->irestmat[x][y];
				self->posemat->matrix[x][y] = self->bone->posemat[x][y];
			}
		}
		return 1;
	}
}
//--------------- updateBoneData-------------------------------------------------------------------------------------
int
updateBoneData(BPy_Bone *self, Bone *parent)
{
	//called from Armature.addBone()
	 int x,y;

	 //called in Armature.addBone() to update the Bone * data
	if(!self->bone){
		//nothing to update - not linked
		return 0;
	}else{
		BLI_strncpy(self->bone->name, self->name, strlen(self->name) + 1);
		self->bone->roll = self->roll;
		self->bone->flag = self->flag;
		self->bone->boneclass = self->boneclass;
		self->bone->dist = self->dist;
		self->bone->weight = self->weight;
		self->bone->parent = parent; //parent will be checked from self->parent string in addBone()

		for(x = 0; x < 3; x++){
			self->bone->head[x] = self->head->vec[x];
			self->bone->tail[x] = self->tail->vec[x];
			self->bone->loc[x] = self->loc->vec[x];
			self->bone->dloc[x] = self->dloc->vec[x];
			self->bone->size[x] = self->size->vec[x];
			self->bone->dsize[x] = self->dsize->vec[x];
		}
		for(x = 0; x < 4; x++){
			self->bone->quat[x] = self->quat->quat[x];
			self->bone->dquat[x] = self->dquat->quat[x];
		}
		for(x = 0; x < 4; x++){
			for(y = 0; y < 4; y++){
				self->bone->obmat[x][y] = self->obmat->matrix[x][y];
				self->bone->parmat[x][y] = self->parmat->matrix[x][y];
				self->bone->defmat[x][y] = self->defmat->matrix[x][y];
				self->bone->irestmat[x][y] = self->irestmat->matrix[x][y];
				self->bone->posemat[x][y] = self->posemat->matrix[x][y];
			}
		}
		return 1;
	}
}
//--------------- testChildbase--------------------------------------------------------------------------------
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
//---------------BPy_Bone internal callbacks/methods---------------------------------------------
//--------------- dealloc---------------------------------------------------------------------------------------
static void
Bone_dealloc (BPy_Bone * self)
{
	PyMem_Free (self->name);
	PyMem_Free (self->parent);
    PyObject_DEL (self);
}
//---------------getattr---------------------------------------------------------------------------------------
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
//--------------- setattr---------------------------------------------------------------------------------------
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
//--------------- repr---------------------------------------------------------------------------------------
static PyObject *
Bone_repr (BPy_Bone * self)
{
  if (self->bone)
    return PyString_FromFormat ("[Bone \"%s\"]", self->bone->name);
  else
    return PyString_FromString ("NULL");
}
//--------------- compare---------------------------------------------------------------------------------------
static int
Bone_compare (BPy_Bone * a, BPy_Bone * b)
{
  Bone *pa = a->bone, *pb = b->bone;
  return (pa == pb) ? 0 : -1;
}
//--------------- Bone_CreatePyObject--------------------------------------------------------------------
PyObject *
Bone_CreatePyObject (struct Bone * bone)
{
	BPy_Bone *blen_bone;

	blen_bone = (BPy_Bone *) PyObject_NEW (BPy_Bone, &Bone_Type);

	//set the all important Bone flag
	blen_bone->bone = bone;

	//allocate space for python vars
	blen_bone->name= PyMem_Malloc (32 + 1);
	blen_bone->parent =PyMem_Malloc (32 + 1);
	blen_bone->head = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);		
	blen_bone->tail = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);			
	blen_bone->loc = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);	
	blen_bone->dloc = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);	
	blen_bone->size = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);	
	blen_bone->dsize = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);	
	blen_bone->quat = (QuaternionObject*)newQuaternionObject(PyMem_Malloc (4*sizeof (float)));
	blen_bone->dquat = (QuaternionObject*)newQuaternionObject(PyMem_Malloc (4*sizeof (float)));
	blen_bone->obmat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);
	blen_bone->parmat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);
	blen_bone->defmat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);
	blen_bone->irestmat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);
	blen_bone->posemat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);	

	if(!updatePyBone(blen_bone))
		return EXPP_ReturnPyObjError (PyExc_AttributeError , "bone struct empty");

	return ((PyObject *) blen_bone);
}
//--------------- Bone_CheckPyObject--------------------------------------------------------------------
int
Bone_CheckPyObject (PyObject * py_obj)
{
  return (py_obj->ob_type == &Bone_Type);
}
//--------------- Bone_FromPyObject--------------------------------------------------------------------
struct Bone *
Bone_FromPyObject (PyObject * py_obj)
{
  BPy_Bone *blen_obj;

  blen_obj = (BPy_Bone *) py_obj;
  if (!((BPy_Bone*)py_obj)->bone) {	//test to see if linked to armature
	  //use python vars
	  return NULL;
  }else{
   	 //use bone datastruct
	return (blen_obj->bone);
  }
}
//--------------- Python Bone Module methods------------------------------------------------------------------
//--------------- Blender.Armature.Bone.New()-----------------------------------------------------------------
static PyObject *
M_Bone_New (PyObject * self, PyObject * args)
{
	char *name_str = "BoneName";
	char *parent_str = "";
	BPy_Bone *py_bone = NULL;	/* for Bone Data object wrapper in Python */

	if (!PyArg_ParseTuple (args, "|s", &name_str))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
					"expected string or empty argument"));

	//create python bone
	py_bone = (BPy_Bone *) PyObject_NEW (BPy_Bone, &Bone_Type);

	//allocate space for python vars
    py_bone->name= PyMem_Malloc (32 + 1);
	py_bone->parent =PyMem_Malloc (32 + 1);
	py_bone->head = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);		
	py_bone->tail = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);			
	py_bone->loc = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);	
	py_bone->dloc = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);	
	py_bone->size = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);	
	py_bone->dsize = (VectorObject*)newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);	
	py_bone->quat = (QuaternionObject*)newQuaternionObject(PyMem_Malloc (4*sizeof (float)));
	py_bone->dquat = (QuaternionObject*)newQuaternionObject(PyMem_Malloc (4*sizeof (float)));
	py_bone->obmat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);
	py_bone->parmat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);
	py_bone->defmat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);
	py_bone->irestmat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);
	py_bone->posemat = (MatrixObject*)newMatrixObject(PyMem_Malloc(16*sizeof(float)),4,4);	
	
	//default py values
	BLI_strncpy(py_bone->name, name_str, strlen(name_str) + 1);
	BLI_strncpy(py_bone->parent, parent_str, strlen(parent_str) + 1);
	py_bone->roll = 0.0f;
	py_bone->flag = 32;
	py_bone->boneclass = BONE_SKINNABLE;
	py_bone->dist = 1.0f;
	py_bone->weight = 1.0f;
	Vector_Zero(py_bone->head);
	Vector_Zero(py_bone->loc);
	Vector_Zero(py_bone->dloc);
	Vector_Zero(py_bone->size);
	Vector_Zero(py_bone->dsize);
    Quaternion_Identity(py_bone->quat);
	Quaternion_Identity(py_bone->dquat);
	Matrix_Identity(py_bone->obmat);
	Matrix_Identity(py_bone->parmat);
	Matrix_Identity(py_bone->defmat);
	Matrix_Identity(py_bone->irestmat);
	Matrix_Identity(py_bone->posemat);

	//default tail of 2,0,0
	py_bone->tail->vec[0] = 2.0f;
	py_bone->tail->vec[1] = 0.0f;
	py_bone->tail->vec[2] = 0.0f;

	//set the datapointer to null (unlinked)
	py_bone->bone = NULL;

	return (PyObject *) py_bone;
}
//--------------- Python BPy_Bone methods------------------------------------------------------------------
//--------------- BPy_Bone.getName()--------------------------------------------------------------------------
static PyObject *
Bone_getName (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  attr = PyString_FromString (self->name);
  }else{
	  //use bone datastruct
      attr = PyString_FromString (self->bone->name);
  }
  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.name attribute"));
}
//--------------- BPy_Bone.getRoll()------------------------------------------------------------------------------
static PyObject *
Bone_getRoll (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  attr = Py_BuildValue ("f", self->roll);
  }else{
	  //use bone datastruct
      attr = Py_BuildValue ("f", self->bone->roll);
  }
  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.roll attribute"));
}
//--------------- BPy_Bone.getWeight()----------------------------------------------------------------------------
static PyObject *
Bone_getWeight (BPy_Bone * self)
{
  PyObject *attr = NULL;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  attr = Py_BuildValue ("f", self->weight);
  }else{
	  //use bone datastruct
      attr = Py_BuildValue ("f", self->bone->weight);
  }
  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.weight attribute"));
}
//--------------- BPy_Bone.getHead()--------------------------------------------------------------------------
static PyObject *
Bone_getHead (BPy_Bone * self)
{
  PyObject *attr = NULL;
  float *vec;
  int x;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  vec = PyMem_Malloc(3 * sizeof(float));
	  for(x = 0; x < 3; x++)
	      vec[x] = self->head->vec[x];
	  attr =  (PyObject *)newVectorObject(vec, 3);
  }else{
	  //use bone datastruct
	  attr = newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);
	  ((VectorObject*)attr)->vec[0] = self->bone->head[0];
	  ((VectorObject*)attr)->vec[1] = self->bone->head[1];
	  ((VectorObject*)attr)->vec[2] = self->bone->head[2];
  }
  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.head attribute"));
}
//--------------- BPy_Bone.getTail()--------------------------------------------------------------------------
static PyObject *
Bone_getTail (BPy_Bone * self)
{
  PyObject *attr = NULL;
  float *vec;
  int x;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  vec = PyMem_Malloc(3 * sizeof(float));
	  for(x = 0; x < 3; x++)
	      vec[x] = self->tail->vec[x];
	  attr =  (PyObject *)newVectorObject(vec, 3);
  }else{
	  //use bone datastruct
	  attr = newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);
	  ((VectorObject*)attr)->vec[0] = self->bone->tail[0];
	  ((VectorObject*)attr)->vec[1] = self->bone->tail[1];
	  ((VectorObject*)attr)->vec[2] = self->bone->tail[2];
  }
  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.tail attribute"));
}
//--------------- BPy_Bone.getLoc()----------------------------------------------------------------------------
static PyObject *
Bone_getLoc (BPy_Bone * self)
{
  PyObject *attr = NULL;
  float *vec;
  int x;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  vec = PyMem_Malloc(3 * sizeof(float));
	  for(x = 0; x < 3; x++)
	      vec[x] = self->loc->vec[x];
	  attr =  (PyObject *)newVectorObject(vec, 3);
  }else{
	  //use bone datastruct
	  attr = newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);
	  ((VectorObject*)attr)->vec[0] = self->bone->loc[0];
	  ((VectorObject*)attr)->vec[1] = self->bone->loc[1];
	  ((VectorObject*)attr)->vec[2] = self->bone->loc[2];
  }
  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.loc attribute"));
}
//--------------- BPy_Bone.getSize()----------------------------------------------------------------------------
static PyObject *
Bone_getSize (BPy_Bone * self)
{
  PyObject *attr = NULL;
  float *vec;
  int x;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  vec = PyMem_Malloc(3 * sizeof(float));
	  for(x = 0; x < 3; x++)
	      vec[x] = self->size->vec[x];
	  attr =  (PyObject *)newVectorObject(vec, 3);
  }else{
	  //use bone datastruct
	  attr = newVectorObject(PyMem_Malloc (3*sizeof (float)), 3);
	  ((VectorObject*)attr)->vec[0] = self->bone->size[0];
	  ((VectorObject*)attr)->vec[1] = self->bone->size[1];
	  ((VectorObject*)attr)->vec[2] = self->bone->size[2];
  }
  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Bone.size attribute"));
}
//--------------- BPy_Bone.getQuat()----------------------------------------------------------------------------
static PyObject *
Bone_getQuat (BPy_Bone * self)
{
  PyObject *attr = NULL;
  float *quat;
  int x;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars - p.s. - you must return a copy or else
	  //python will trash the internal var
	  quat = PyMem_Malloc(4 * sizeof(float));
	  for(x = 0; x < 4; x++)
	      quat[x] = self->quat->quat[x];
	  attr =  (PyObject *)newQuaternionObject(quat);
  }else{
	  //use bone datastruct
	  attr = newQuaternionObject(PyMem_Malloc (4*sizeof (float)));
	  ((QuaternionObject*)attr)->quat[0] = self->bone->quat[0];
	  ((QuaternionObject*)attr)->quat[1] = self->bone->quat[1];
	  ((QuaternionObject*)attr)->quat[2] = self->bone->quat[2];
	  ((QuaternionObject*)attr)->quat[3] = self->bone->quat[3];
  }

  return attr;
}
//--------------- BPy_Bone.hasParent()--------------------------------------------------------------------------
static PyObject *
Bone_hasParent (BPy_Bone * self)
{
	char * parent_str = "";

	if (!self->bone) {	//test to see if linked to armature
		//use python vars
		if (BLI_streq(self->parent, parent_str)) {
				Py_INCREF (Py_False);
				return Py_False;
		}else{
				Py_INCREF (Py_True);
				return Py_True;
		}
	}else{
		//use bone datastruct
		if (self->bone->parent) {
				Py_INCREF (Py_True);
				return Py_True;
		}else{
				Py_INCREF (Py_False);
				return Py_False;
		}
	}
}
//--------------- BPy_Bone.getParent()--------------------------------------------------------------------------
static PyObject *
Bone_getParent (BPy_Bone * self)
{
	char * parent_str = "";	
	if (!self->bone) {	//test to see if linked to armature
		//use python vars
			if (BLI_streq(self->parent, parent_str)) {
					return EXPP_incr_ret  (Py_None);
			}else{
					return PyString_FromString(self->parent);
			}
	}else{
		//use bone datastruct
		if (self->bone->parent) {
				return Bone_CreatePyObject (self->bone->parent);
		}else{
				return EXPP_incr_ret  (Py_None);
		}
	}
}
//--------------- BPy_Bone.getChildren()--------------------------------------------------------------------------
static PyObject *
Bone_getChildren (BPy_Bone * self)
{
  int totbones = 0;
  Bone *current = NULL;
  PyObject *listbones = NULL;
  int i;

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
      return EXPP_incr_ret  (Py_None);
  }else{
	  //use bone datastruct
	  current = self->bone->childbase.first;
	  for (; current; current = current->next)
	    	totbones++;

	  /* Create a list with a bone wrapper for each bone */
	  current = self->bone->childbase.first;
	  listbones = PyList_New (totbones);
	  for (i = 0; i < totbones; i++){
		  assert (current);
		  PyList_SetItem (listbones, i, Bone_CreatePyObject (current));
		  current = current->next;
	  }
	  return listbones;
  }
}
//--------------- BPy_Bone.setName()--------------------------------------------------------------------------
static PyObject *
Bone_setName (BPy_Bone * self, PyObject * args)
{
  char *name;

  if (!PyArg_ParseTuple (args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,	"expected string argument"));

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  BLI_strncpy(self->name, name, strlen(name) + 1);
  }else{
	  //use bone datastruct
	  BLI_strncpy(self->bone->name, name, strlen(name) + 1);
  }
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setRoll()--------------------------------------------------------------------------
PyObject *
Bone_setRoll (BPy_Bone * self, PyObject * args)
{
  float roll;

  if (!PyArg_ParseTuple (args, "f", &roll))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected float argument"));

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  self->roll = roll;
  }else{
	  //use bone datastruct
      self->bone->roll = roll;
  }
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setHead()--------------------------------------------------------------------------
static PyObject *
Bone_setHead (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3;
  int status;

  if (PyObject_Length (args) == 3)
    status = PyArg_ParseTuple (args, "fff", &f1, &f2, &f3);
  else
    status = PyArg_ParseTuple (args, "(fff)", &f1, &f2, &f3);

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected 3 (or a list of 3) float arguments"));

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  self->head->vec[0] = f1;
	  self->head->vec[1] = f2;
	  self->head->vec[2] = f3;
  }else{
	  //use bone datastruct
	self->bone->head[0] = f1;
	self->bone->head[1] = f2;
	self->bone->head[2] = f3;
  }
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setTail()--------------------------------------------------------------------------
static PyObject *
Bone_setTail (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3;
  int status;

  if (PyObject_Length (args) == 3)
    status = PyArg_ParseTuple (args, "fff", &f1, &f2, &f3);
  else
    status = PyArg_ParseTuple (args, "(fff)", &f1, &f2, &f3);

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected 3 (or a list of 3) float arguments"));

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  self->tail->vec[0] = f1;
	  self->tail->vec[1] = f2;
	  self->tail->vec[2] = f3;
  }else{
	  //use bone datastruct
	 self->bone->tail[0] = f1;
	 self->bone->tail[1] = f2;
	 self->bone->tail[2] = f3;
  }
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setLoc()--------------------------------------------------------------------------
static PyObject *
Bone_setLoc (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3;
  int status;

  if (PyObject_Length (args) == 3)
    status = PyArg_ParseTuple (args, "fff", &f1, &f2, &f3);
  else
    status = PyArg_ParseTuple (args, "(fff)", &f1, &f2, &f3);

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected 3 (or a list of 3) float arguments"));

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  self->loc->vec[0] = f1;
	  self->loc->vec[1] = f2;
	  self->loc->vec[2] = f3;
  }else{
	  //use bone datastruct
	 self->bone->loc[0] = f1;
	 self->bone->loc[1] = f2;
	 self->bone->loc[2] = f3;
  }
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setSize()--------------------------------------------------------------------------
static PyObject *
Bone_setSize (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3;
  int status;

  if (PyObject_Length (args) == 3)
    status = PyArg_ParseTuple (args, "fff", &f1, &f2, &f3);
  else
    status = PyArg_ParseTuple (args, "(fff)", &f1, &f2, &f3);

  if (!status)
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected 3 (or a list of 3) float arguments"));

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  self->size->vec[0] = f1;
	  self->size->vec[1] = f2;
	  self->size->vec[2] = f3;
  }else{
	  //use bone datastruct
	 self->bone->size[0] = f1;
	 self->bone->size[1] = f2;
	 self->bone->size[2] = f3;
  }
   return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setQuat()--------------------------------------------------------------------------
static PyObject *
Bone_setQuat (BPy_Bone * self, PyObject * args)
{
  float f1, f2, f3, f4;
  PyObject *argument;
  QuaternionObject *quatOb;
  int status;

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

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  self->quat->quat[0] = f1;
	  self->quat->quat[1] = f2;
	  self->quat->quat[2] = f3;
	  self->quat->quat[3] = f4;
  }else{
	  //use bone datastruct
	 self->bone->quat[0] = f1;
	 self->bone->quat[1] = f2;
	 self->bone->quat[2] = f3;
	 self->bone->quat[3] = f4;
  }
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setParent()-------------------------------------------------------------------------
static PyObject *
Bone_setParent(BPy_Bone *self, PyObject *args)
{
	BPy_Bone* py_bone;
	float M_boneObjectspace[4][4];
	float iM_parentRest[4][4];

	if (!PyArg_ParseTuple(args, "O", &py_bone))
			return (EXPP_ReturnPyObjError (PyExc_TypeError, "expected bone object argument"));

	if (!self->bone) {	//test to see if linked to armature
		//use python vars
		BLI_strncpy(self->parent, py_bone->name, strlen(py_bone->name) + 1);
	}else{
		//use bone datastruct
		if(!py_bone->bone)
				return (EXPP_ReturnPyObjError (PyExc_TypeError, "Parent bone must be linked to armature first!"));

		if(py_bone->bone == self->bone)
			return (EXPP_ReturnPyObjError (PyExc_AttributeError, "Cannot parent to self"));

		//test to see if were creating an illegal loop by parenting to child
		if(testChildbase(self->bone, py_bone->bone))
			return (EXPP_ReturnPyObjError (PyExc_AttributeError, "Cannot parent to child"));

		//set the parent of self - in this case we are changing the parenting after this bone
		//has been linked in it's armature
		if(self->bone->parent){  //we are parenting something previously parented

			//remove the childbase link from the parent bone
			BLI_remlink(&self->bone->parent->childbase, self->bone);

			//now get rid of the parent transformation
			get_objectspace_bone_matrix(self->bone->parent, M_boneObjectspace, 0,0);
			Mat4MulVecfl(M_boneObjectspace, self->bone->head);
			Mat4MulVecfl(M_boneObjectspace, self->bone->tail);
			
			//add to the childbase of new parent
			BLI_addtail (&py_bone->bone->childbase, self->bone);

			//transform bone according to new parent
			get_objectspace_bone_matrix(py_bone->bone, M_boneObjectspace, 0,0);
			Mat4Invert (iM_parentRest, M_boneObjectspace);
			Mat4MulVecfl(iM_parentRest, self->bone->head);
			Mat4MulVecfl(iM_parentRest, self->bone->tail);

			//set parent
			self->bone->parent = py_bone->bone;

		}else{  //not previously parented

			//add to the childbase of new parent
			BLI_addtail (&py_bone->bone->childbase, self->bone);

			//transform bone according to new parent
			get_objectspace_bone_matrix(py_bone->bone, M_boneObjectspace, 0,0);
			Mat4Invert (iM_parentRest, M_boneObjectspace);
			Mat4MulVecfl(iM_parentRest, self->bone->head);
			Mat4MulVecfl(iM_parentRest, self->bone->tail);

			self->bone->parent = py_bone->bone;
		}
	}
	return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setWeight()-------------------------------------------------------------------------
static PyObject *
Bone_setWeight(BPy_Bone *self, PyObject *args)
{
  float weight;

  if (!PyArg_ParseTuple (args, "f", &weight))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected float argument"));

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  self->weight = weight;
  }else{
	  //use bone datastruct
	 self->bone->weight = weight;
  }  
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.clearParent()-------------------------------------------------------------------------
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
  char *parent_str = "";

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	 BLI_strncpy(self->parent, parent_str, strlen(parent_str) + 1);
  }else{
	 //use bone datastruct
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
  }  
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.clearChildren()-------------------------------------------------------------------------
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

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  return EXPP_incr_ret  (Py_None);
  }else{
  	 //use bone datastruct
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
  }
  Py_INCREF(Py_None);
  return Py_None;
}
//--------------- BPy_Bone.hide()---------------------------------------------------------------------------------
static PyObject *
Bone_hide(BPy_Bone *self)
{
  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  return EXPP_ReturnPyObjError (PyExc_TypeError, "link bone to armature before attempting to hide/unhide");
  }else{
   	   //use bone datastruct
		if(!(self->bone->flag & BONE_HIDDEN))
			self->bone->flag |= BONE_HIDDEN;
  }
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.unhide()---------------------------------------------------------------------------------
static PyObject *
Bone_unhide(BPy_Bone *self)
{
  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  return EXPP_ReturnPyObjError (PyExc_TypeError, "link bone to armature before attempting to hide/unhide");
  }else{
   	 //use bone datastruct
	if(self->bone->flag & BONE_HIDDEN)
		self->bone->flag &= ~BONE_HIDDEN;
  }
  return EXPP_incr_ret  (Py_None);
}
//--------------- BPy_Bone.setPose()-----------------------------------------------------------------------------------
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

  if (!self->bone) {	//test to see if linked to armature
	  //use python vars
	  return EXPP_ReturnPyObjError (PyExc_TypeError, "cannot set pose unless bone is linked to armature");
  }else{
   	 //use bone datastruct
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
	}
	return EXPP_incr_ret  (Py_None);
}
