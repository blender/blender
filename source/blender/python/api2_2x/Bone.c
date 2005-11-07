/* 
 * $Id$
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
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Bone.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BKE_utildefines.h"
#include "gen_utils.h"
#include "BKE_armature.h"
#include "Mathutils.h"

//------------------------ERROR CODES---------------------------------
//This is here just to make me happy and to have more consistant error strings :)
static const char sEditBoneError[] = "EditBone (internal) - Error: ";
static const char sEditBoneBadArgs[] = "EditBone (internal) - Bad Arguments: ";
static const char sBoneError[] = "Bone - Error: ";
static const char sBoneBadArgs[] = "Bone - Bad Arguments: ";

//----------------------(internal)
//gets the bone->roll (which is a localspace roll) and puts it in parentspace
//(which is the 'roll' value the user sees)
double boneRoll_ToArmatureSpace(struct Bone *bone)
{
	float head[3], tail[3], delta[3];
	float premat[3][3], postmat[3][3];
	float imat[3][3], difmat[3][3];
	double roll = 0.0f;

	VECCOPY(head, bone->arm_head);
	VECCOPY(tail, bone->arm_tail);		
	VECSUB (delta, tail, head);			
	vec_roll_to_mat3(delta, 0.0f, postmat);	
	Mat3CpyMat4(premat, bone->arm_mat);		
	Mat3Inv(imat, postmat);					
	Mat3MulMat3(difmat, imat, premat);	

	roll = atan(difmat[2][0] / difmat[2][2]); 
	if (difmat[0][0] < 0.0){
		roll += M_PI;
	}
	return roll; //result is in radians
}
//################## EditBone_Type (internal) ########################
/*This type is a wrapper for a tempory bone. This is an 'unparented' bone
*object. The armature->bonebase will be calculated from these temporary
*python tracked objects.*/
//#####################################################################

//------------------METHOD IMPLEMENTATIONS-----------------------------
//------------------ATTRIBUTE IMPLEMENTATION---------------------------
//------------------------EditBone.name (get)
static PyObject *EditBone_getName(BPy_EditBone *self, void *closure)
{
	return PyString_FromString(self->name);
}
//------------------------EditBone.name (set)
//check for char[] overflow here...
static int EditBone_setName(BPy_EditBone *self, PyObject *value, void *closure)
{  
	char *name = "";

	if (!PyArg_Parse(value, "s", &name))
		goto AttributeError;

	BLI_strncpy(self->name, name, 32);
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".name: ", "expects a string");
}
//------------------------EditBone.roll (get)
static PyObject *EditBone_getRoll(BPy_EditBone *self, void *closure)
{
	return Py_BuildValue("{s:O}", 
		"ARMATURESPACE", PyFloat_FromDouble((self->roll * (180/Py_PI))));
}
//------------------------EditBone.roll (set)
static int EditBone_setRoll(BPy_EditBone *self, PyObject *value, void *closure)
{  
	printf("Sorry this isn't implemented yet.... :/");
	return 1;
}
//------------------------EditBone.head (get)
static PyObject *EditBone_getHead(BPy_EditBone *self, void *closure)
{
	return Py_BuildValue("{s:O, s:O}", 
		"BONESPACE", newVectorObject(self->head, 3, Py_WRAP));;
}
//------------------------EditBone.head (set)
static int EditBone_setHead(BPy_EditBone *self, PyObject *value, void *closure)
{  
	printf("Sorry this isn't implemented yet.... :/");
	return 1;
}
//------------------------EditBone.tail (get)
static PyObject *EditBone_getTail(BPy_EditBone *self, void *closure)
{
    return Py_BuildValue("{s:O, s:O}", 
		"BONESPACE", newVectorObject(self->tail, 3, Py_WRAP));
}
//------------------------EditBone.tail (set)
static int EditBone_setTail(BPy_EditBone *self, PyObject *value, void *closure)
{  
	printf("Sorry this isn't implemented yet.... :/");
	return 1;
}
//------------------------EditBone.weight (get)
static PyObject *EditBone_getWeight(BPy_EditBone *self, void *closure)
{
	return PyFloat_FromDouble(self->weight);
}
//------------------------EditBone.weight (set)
static int EditBone_setWeight(BPy_EditBone *self, PyObject *value, void *closure)
{  
	float weight;

	if (!PyArg_Parse(value, "f", &weight))
		goto AttributeError;
	CLAMP(weight, 0.0f, 1000.0f);

	self->weight = weight;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".weight: ", "expects a float");
}
//------------------------EditBone.deform_dist (get)
static PyObject *EditBone_getDeform_dist(BPy_EditBone *self, void *closure)
{
    return PyFloat_FromDouble(self->dist);
}
//------------------------EditBone.deform_dist (set)
static int EditBone_setDeform_dist(BPy_EditBone *self, PyObject *value, void *closure)
{  
	float deform;

	if (!PyArg_Parse(value, "f", &deform))
		goto AttributeError;
	CLAMP(deform, 0.0f, 1000.0f);

	self->dist = deform;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".deform_dist: ", "expects a float");
}
//------------------------EditBone.subdivisions (get)
static PyObject *EditBone_getSubdivisions(BPy_EditBone *self, void *closure)
{
    return PyInt_FromLong(self->segments);
}
//------------------------EditBone.subdivisions (set)
static int EditBone_setSubdivisions(BPy_EditBone *self, PyObject *value, void *closure)
{  
	int segs;

	if (!PyArg_Parse(value, "i", &segs))
		goto AttributeError;
	CLAMP(segs, 1, 32);

	self->segments = (short)segs;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".subdivisions: ", "expects a integer");
}
//------------------------EditBone.options (get)
static PyObject *EditBone_getOptions(BPy_EditBone *self, void *closure)
{
	PyObject *list = NULL;

	list = PyList_New(0);
	if (list == NULL)
		goto RuntimeError;

	if(self->flag & BONE_CONNECTED)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "CONNECTED")) == -1)
			goto RuntimeError;
	if(self->flag & BONE_HINGE)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "HINGE")) == -1)
			goto RuntimeError;
	if(self->flag & BONE_NO_DEFORM)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "NO_DEFORM")) == -1)
			goto RuntimeError;
	if(self->flag & BONE_MULT_VG_ENV)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "MULTIPLY")) == -1)
			goto RuntimeError;
	if(self->flag & BONE_HIDDEN_A)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "HIDDEN_EDIT")) == -1)
			goto RuntimeError;

	return EXPP_incr_ret(list);

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sEditBoneError, ".options: ", "Internal failure!");
}
//----------------------(internal) EditBone_CheckValidConstant
static int EditBone_CheckValidConstant(PyObject *constant)
{
	PyObject *name = NULL;

	if (constant){
		if (BPy_Constant_Check(constant)){
			name = PyDict_GetItemString(((BPy_constant*)constant)->dict, "name");
			if (!name) return 0;
			if (!(STREQ3(PyString_AsString(name), "CONNECTED", "HINGE", "NO_DEFORM")
				|| STREQ2(PyString_AsString(name), "MULTIPLY", "HIDDEN_EDIT"))){
					return 0;
				}else{
					return 1;
				}
		}else{
			return 0;
		}
	}else{
		return 0;
	}
}

//------------------------EditBone.options (set)
static int EditBone_setOptions(BPy_EditBone *self, PyObject *value, void *closure)
{  
	int length, numeric_value, new_flag = 0, x;
	PyObject *val = NULL, *index = NULL;

	if (PyList_Check(value)){
		length = PyList_Size(value);
		for (x = 0; x < length; x++){
			index = PyList_GetItem(value, x);
			if (!EditBone_CheckValidConstant(index))
				goto AttributeError2;
			val = PyDict_GetItemString(((BPy_constant*)index)->dict, "value");
			if (PyInt_Check(val)){
				numeric_value = (int)PyInt_AS_LONG(val);
				new_flag |= numeric_value;
			}else{
				goto AttributeError2;
			}
		}
		self->flag = new_flag;
		return 0;
	}else if (BPy_Constant_Check(value)){
		if (!EditBone_CheckValidConstant(value))
			goto AttributeError2;
		val = PyDict_GetItemString(((BPy_constant*)value)->dict, "value");
		if (PyInt_Check(val)){
			numeric_value = (int)PyInt_AS_LONG(val);
			self->flag = numeric_value;
			return 0;
		}else{
			goto AttributeError2;
		}
	}else{
		goto AttributeError1;
	}

AttributeError1:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".options(): ", "Expects a constant or list of constants");

AttributeError2:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".options(): ", "Please use a constant defined in the Armature module");
}
//------------------------EditBone.parent (get)
static PyObject *EditBone_getParent(BPy_EditBone *self, void *closure)
{
	//if (!STREQ(self->parent, ""))
	//	return PyString_FromString(PyBone_FromBone(self->parent));
	//else
	printf("Sorry this isn't implemented yet.... :/");
	return EXPP_incr_ret(Py_None);
}
//------------------------EditBone.parent (set)
static int EditBone_setParent(BPy_EditBone *self, PyObject *value, void *closure)
{  
	printf("Sorry this isn't implemented yet.... :/");
	return 1;
}

//------------------------EditBone.children (get)
static PyObject *EditBone_getChildren(BPy_EditBone *self, void *closure)
{
	printf("Sorry this isn't implemented yet.... :/");
	return EXPP_incr_ret(Py_None);
}
//------------------------EditBone.children (set)
static int EditBone_setChildren(BPy_EditBone *self, PyObject *value, void *closure)
{  
	printf("Sorry this isn't implemented yet.... :/");
	return 1;
}
//------------------------EditBone.matrix (get)
static PyObject *EditBone_getMatrix(BPy_EditBone *self, void *closure)
{
	printf("Sorry this isn't implemented yet.... :/");
    return EXPP_incr_ret(Py_None);
}
//------------------------EditBone.matrix (set)
static int EditBone_setMatrix(BPy_EditBone *self, PyObject *value, void *closure)
{  
	printf("Sorry this isn't implemented yet.... :/");
	return 1;
}
//------------------TYPE_OBECT IMPLEMENTATION--------------------------
//TODO: We need to think about the below methods
//------------------------tp_methods
//This contains a list of all methods the object contains
//static PyMethodDef BPy_Bone_methods[] = {
//	{"clearParent", (PyCFunction) Bone_clearParent, METH_NOARGS, 
//		"() - disconnects this bone from it's parent"},
//	{"clearChildren", (PyCFunction) Bone_clearChildren, METH_NOARGS, 
//		"() - disconnects all the children from this bone"},
//	{NULL}
//};
//------------------------tp_getset
//This contains methods for attributes that require checking
static PyGetSetDef BPy_EditBone_getset[] = {
	{"name", (getter)EditBone_getName, (setter)EditBone_setName, 
		"The name of the bone", NULL},
	{"roll", (getter)EditBone_getRoll, (setter)EditBone_setRoll, 
		"The roll (or rotation around the axis) of the bone", NULL},
	{"head", (getter)EditBone_getHead, (setter)EditBone_setHead, 
		"The start point of the bone", NULL},
	{"tail", (getter)EditBone_getTail, (setter)EditBone_setTail, 
		"The end point of the bone", NULL},
	{"matrix", (getter)EditBone_getMatrix, (setter)EditBone_setMatrix, 
		"The matrix of the bone", NULL},
	{"weight", (getter)EditBone_getWeight, (setter)EditBone_setWeight, 
		"The weight of the bone in relation to a parented mesh", NULL},
	{"deform_dist", (getter)EditBone_getDeform_dist, (setter)EditBone_setDeform_dist, 
		"The distance at which deformation has effect", NULL},
	{"subdivisions", (getter)EditBone_getSubdivisions, (setter)EditBone_setSubdivisions, 
		"The number of subdivisions (for B-Bones)", NULL},
	{"options", (getter)EditBone_getOptions, (setter)EditBone_setOptions, 
		"The options effective on this bone", NULL},
	{"parent", (getter)EditBone_getParent, (setter)EditBone_setParent, 
		"The parent bone of this bone", NULL},
	{"children", (getter)EditBone_getChildren, (setter)EditBone_setChildren, 
		"The child bones of this bone", NULL},
	{NULL}
};

//------------------------tp_repr
//This is the string representation of the object
static PyObject *EditBone_repr(BPy_EditBone *self)
{
	return PyString_FromFormat( "[EditBone \"%s\"]", self->name ); 
}

//------------------------tp_doc
//The __doc__ string for this object
static char BPy_EditBone_doc[] = "This is an internal subobject of armature\
designed to act as a wrapper for an 'edit bone'.";

//------------------------tp_new
//This methods creates a new object (note it does not initialize it - only the building)
static PyObject *EditBone_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	BPy_EditBone *py_editBone = NULL;
	PyObject *py_bone;
	struct Bone *bone;
	int i;

	if(!PyArg_ParseTuple(args, "O!", &Bone_Type, &py_bone))
		goto AttributeError;

	py_editBone = (BPy_EditBone*)type->tp_alloc(type, 0); //new
	if (py_editBone == NULL)
		goto RuntimeError;

	bone = ((BPy_Bone*)py_bone)->bone;

	BLI_strncpy(py_editBone->name, bone->name, 32);
	py_editBone->flag = bone->flag;
	py_editBone->length = bone->length;
	py_editBone->weight = bone->weight;
	py_editBone->dist = bone->dist;
	py_editBone->xwidth = bone->xwidth;
	py_editBone->zwidth = bone->zwidth;
	py_editBone->ease1 = bone->ease1;
	py_editBone->ease2 = bone->ease2;
	py_editBone->rad_head = bone->rad_head;
	py_editBone->rad_tail = bone->rad_tail;
	py_editBone->segments = bone->segments;
	py_editBone->temp = NULL;

	if (bone->parent){
		BLI_strncpy(py_editBone->parent, bone->parent->name, 32);
	}else{
		BLI_strncpy(py_editBone->parent, "", 32);
	}

	py_editBone->roll = (float)boneRoll_ToArmatureSpace(bone);

	for (i = 0; i < 3; i++){
		py_editBone->head[i] = bone->arm_head[i];
		py_editBone->tail[i] = bone->arm_tail[i];
	}
	return (PyObject*)py_editBone;

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sEditBoneError, " __new__: ", "Internal Error");
AttributeError:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s", 
		sEditBoneBadArgs, " __new__: ", "Expects PyBone and optional float");
}
//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
static void EditBone_dealloc(BPy_EditBone * self)
{
	((PyObject*)self)->ob_type->tp_free((PyObject*)self);
	return;
}
//------------------TYPE_OBECT DEFINITION--------------------------
PyTypeObject EditBone_Type = {
	PyObject_HEAD_INIT(NULL)       //tp_head
	0,                             //tp_internal
	"EditBone",                    //tp_name
	sizeof(BPy_EditBone),          //tp_basicsize
	0,                             //tp_itemsize
	(destructor)EditBone_dealloc,  //tp_dealloc
	0,                             //tp_print
	0,                             //tp_getattr
	0,                             //tp_setattr
	0,                             //tp_compare
	(reprfunc)EditBone_repr,       //tp_repr
	0,                             //tp_as_number
	0,                             //tp_as_sequence
	0,                             //tp_as_mapping
	0,                             //tp_hash
	0,                             //tp_call
	0,                             //tp_str
	0,                             //tp_getattro
	0,                             //tp_setattro
	0,                             //tp_as_buffer
	Py_TPFLAGS_DEFAULT,            //tp_flags
	BPy_EditBone_doc,              //tp_doc
	0,                             //tp_traverse
	0,                             //tp_clear
	0,                             //tp_richcompare
	0,                             //tp_weaklistoffset
	0,                             //tp_iter
	0,                             //tp_iternext
	0,                             //tp_methods
	0,                             //tp_members
	BPy_EditBone_getset,           //tp_getset
	0,                             //tp_base
	0,                             //tp_dict
	0,                             //tp_descr_get
	0,                             //tp_descr_set
	0,                             //tp_dictoffset
	0,                             //tp_init
	0,                             //tp_alloc
	(newfunc)EditBone_new,         //tp_new
	0,                             //tp_free
	0,                             //tp_is_gc
	0,                             //tp_bases
	0,                             //tp_mro
	0,                             //tp_cache
	0,                             //tp_subclasses
	0,                             //tp_weaklist
	0                              //tp_del
};

//------------------METHOD IMPLEMENTATIONS--------------------------------
//------------------ATTRIBUTE IMPLEMENTATIONS-----------------------------
//------------------------Bone.name (get)
static PyObject *Bone_getName(BPy_Bone *self, void *closure)
{
	return PyString_FromString(self->bone->name);
}
//------------------------Bone.name (set)
//check for char[] overflow here...
static int Bone_setName(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.roll (get)
static PyObject *Bone_getRoll(BPy_Bone *self, void *closure)
{
	return Py_BuildValue("{s:O, s:O}", 
		"BONESPACE", PyFloat_FromDouble((self->bone->roll * (180/Py_PI))),
		"ARMATURESPACE", PyFloat_FromDouble((boneRoll_ToArmatureSpace(self->bone) * (180/Py_PI))));
}
//------------------------Bone.roll (set)
static int Bone_setRoll(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.head (get)
static PyObject *Bone_getHead(BPy_Bone *self, void *closure)
{
	return Py_BuildValue("{s:O, s:O}", 
		"BONESPACE", newVectorObject(self->bone->head, 3, Py_WRAP),
		"ARMATURESPACE", newVectorObject(self->bone->arm_head, 3, Py_WRAP));
}
//------------------------Bone.head (set)
static int Bone_setHead(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.tail (get)
static PyObject *Bone_getTail(BPy_Bone *self, void *closure)
{
    return Py_BuildValue("{s:O, s:O}", 
		"BONESPACE", newVectorObject(self->bone->tail, 3, Py_WRAP),
		"ARMATURESPACE", newVectorObject(self->bone->arm_tail, 3, Py_WRAP));
}
//------------------------Bone.tail (set)
static int Bone_setTail(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.weight (get)
static PyObject *Bone_getWeight(BPy_Bone *self, void *closure)
{
	return PyFloat_FromDouble(self->bone->weight);
}
//------------------------Bone.weight (set)
static int Bone_setWeight(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.deform_dist (get)
static PyObject *Bone_getDeform_dist(BPy_Bone *self, void *closure)
{
    return PyFloat_FromDouble(self->bone->dist);
}
//------------------------Bone.deform_dist (set)
static int Bone_setDeform_dist(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.subdivisions (get)
static PyObject *Bone_getSubdivisions(BPy_Bone *self, void *closure)
{
    return PyInt_FromLong(self->bone->segments);
}
//------------------------Bone.subdivisions (set)
static int Bone_setSubdivisions(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.connected (get)
static PyObject *Bone_getOptions(BPy_Bone *self, void *closure)
{
	PyObject *list = NULL;

	list = PyList_New(0);
	if (list == NULL)
		goto RuntimeError;

	if(self->bone->flag & BONE_CONNECTED)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "CONNECTED")) == -1)
			goto RuntimeError;
	if(self->bone->flag & BONE_HINGE)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "HINGE")) == -1)
			goto RuntimeError;
	if(self->bone->flag & BONE_NO_DEFORM)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "NO_DEFORM")) == -1)
			goto RuntimeError;
	if(self->bone->flag & BONE_MULT_VG_ENV)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "MULTIPLY")) == -1)
			goto RuntimeError;
	if(self->bone->flag & BONE_HIDDEN_A)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "HIDDEN_EDIT")) == -1)
			goto RuntimeError;

	return EXPP_incr_ret(list);

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sBoneError, "getOptions(): ", "Internal failure!");
}
//------------------------Bone.connected (set)
static int Bone_setOptions(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.parent (get)
static PyObject *Bone_getParent(BPy_Bone *self, void *closure)
{
	if (self->bone->parent)
		return PyBone_FromBone(self->bone->parent);
	else
		return EXPP_incr_ret(Py_None);
}
//------------------------Bone.parent (set)
static int Bone_setParent(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------(internal) PyBone_ChildrenAsList
static int PyBone_ChildrenAsList(PyObject *list, ListBase *bones){
	Bone *bone = NULL;
	PyObject *py_bone = NULL;

	for (bone = bones->first; bone; bone = bone->next){
		py_bone = PyBone_FromBone(bone);
		if (py_bone == NULL)
			return 0;

		if(PyList_Append(list, py_bone) == -1){
			goto RuntimeError;
		}
		if (bone->childbase.first) 
			PyBone_ChildrenAsList(list, &bone->childbase);
	}
	return 1;

RuntimeError:
	return EXPP_intError(PyExc_RuntimeError, "%s%s", 
		sBoneError, "Internal error trying to wrap blender bones!");
}

//------------------------Bone.children (get)
static PyObject *Bone_getChildren(BPy_Bone *self, void *closure)
{
	PyObject *list = NULL;

	if (self->bone->childbase.first){
		list = PyList_New(0);
		if (!PyBone_ChildrenAsList(list, &self->bone->childbase))
			return NULL;
		return EXPP_incr_ret(list);
	}else{
		return EXPP_incr_ret(Py_None);
	}
}
//------------------------Bone.children (set)
static int Bone_setChildren(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------------Bone.matrix (get)
static PyObject *Bone_getMatrix(BPy_Bone *self, void *closure)
{
    return Py_BuildValue("{s:O, s:O}", 
		"BONESPACE", newMatrixObject((float*)self->bone->bone_mat, 3,3, Py_WRAP),
		"ARMATURESPACE", newMatrixObject((float*)self->bone->arm_mat, 4,4, Py_WRAP));
}
//------------------------Bone.matrix (set)
static int Bone_setMatrix(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call Armature.makeEditable() to edit the armature");
}
//------------------TYPE_OBECT IMPLEMENTATION--------------------------
//------------------------tp_getset
//This contains methods for attributes that require checking
static PyGetSetDef BPy_Bone_getset[] = {
	{"name", (getter)Bone_getName, (setter)Bone_setName, 
		"The name of the bone", NULL},
	{"roll", (getter)Bone_getRoll, (setter)Bone_setRoll, 
		"The roll (or rotation around the axis) of the bone", NULL},
	{"head", (getter)Bone_getHead, (setter)Bone_setHead, 
		"The start point of the bone", NULL},
	{"tail", (getter)Bone_getTail, (setter)Bone_setTail, 
		"The end point of the bone", NULL},
	{"matrix", (getter)Bone_getMatrix, (setter)Bone_setMatrix, 
		"The matrix of the bone", NULL},
	{"weight", (getter)Bone_getWeight, (setter)Bone_setWeight, 
		"The weight of the bone in relation to a parented mesh", NULL},
	{"deform_dist", (getter)Bone_getDeform_dist, (setter)Bone_setDeform_dist, 
		"The distance at which deformation has effect", NULL},
	{"subdivisions", (getter)Bone_getSubdivisions, (setter)Bone_setSubdivisions, 
		"The number of subdivisions (for B-Bones)", NULL},
	{"options", (getter)Bone_getOptions, (setter)Bone_setOptions, 
		"The options effective on this bone", NULL},
	{"parent", (getter)Bone_getParent, (setter)Bone_setParent, 
		"The parent bone of this bone", NULL},
	{"children", (getter)Bone_getChildren, (setter)Bone_setChildren, 
		"The child bones of this bone", NULL},
	{NULL}
};

//------------------------tp_new
static PyObject *Bone_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	return EXPP_incr_ret(Py_None);
}

//------------------------tp_richcompare
//This method allows the object to use comparison operators
static PyObject *Bone_richcmpr(BPy_Bone *self, PyObject *v, int op)
{
	return EXPP_incr_ret(Py_None);
}

//------------------------tp_repr
//This is the string representation of the object
static PyObject *Bone_repr(BPy_Bone *self)
{
	return PyString_FromFormat( "[Bone \"%s\"]", self->bone->name ); 
}

//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
static void Bone_dealloc(BPy_Bone * self)
{
	((PyObject*)self)->ob_type->tp_free((PyObject*)self);
	return;
}

//------------------------tp_doc
//The __doc__ string for this object
static char BPy_Bone_doc[] = "This object wraps a Blender Boneobject.\n\
					  This object is a subobject of the Armature object.";

//------------------TYPE_OBECT DEFINITION--------------------------
PyTypeObject Bone_Type = {
	PyObject_HEAD_INIT(NULL)       //tp_head
	0,                             //tp_internal
	"Bone",                        //tp_name
	sizeof(BPy_Bone),              //tp_basicsize
	0,                             //tp_itemsize
	(destructor)Bone_dealloc,      //tp_dealloc
	0,                             //tp_print
	0,                             //tp_getattr
	0,                             //tp_setattr
	0,                             //tp_compare
	(reprfunc) Bone_repr,          //tp_repr
	0,                             //tp_as_number
	0,                             //tp_as_sequence
	0,                             //tp_as_mapping
	0,                             //tp_hash
	0,                             //tp_call
	0,                             //tp_str
	0,                             //tp_getattro
	0,                             //tp_setattro
	0,                             //tp_as_buffer
	Py_TPFLAGS_DEFAULT,            //tp_flags
	BPy_Bone_doc,                  //tp_doc
	0,                             //tp_traverse
	0,                             //tp_clear
	(richcmpfunc)Bone_richcmpr,    //tp_richcompare
	0,                             //tp_weaklistoffset
	0,                             //tp_iter
	0,                             //tp_iternext
	0,                             //tp_methods
	0,                             //tp_members
	BPy_Bone_getset,               //tp_getset
	0,                             //tp_base
	0,                             //tp_dict
	0,                             //tp_descr_get
	0,                             //tp_descr_set
	0,                             //tp_dictoffset
	0,                             //tp_init
	0,                             //tp_alloc
	(newfunc)Bone_new,             //tp_new
	0,                             //tp_free
	0,                             //tp_is_gc
	0,                             //tp_bases
	0,                             //tp_mro
	0,                             //tp_cache
	0,                             //tp_subclasses
	0,                             //tp_weaklist
	0                              //tp_del
};
//------------------VISIBLE PROTOTYPE IMPLEMENTATION-----------------------
//-----------------(internal)
//Converts a struct Bone to a BPy_Bone
PyObject *PyBone_FromBone(struct Bone *bone)
{
	BPy_Bone *py_Bone = NULL;

	py_Bone = (BPy_Bone*)Bone_Type.tp_alloc(&Bone_Type, 0); //*new*
	if (py_Bone == NULL)
		goto RuntimeError;

	py_Bone->bone = bone;

	return (PyObject *) py_Bone;

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sBoneError, "PyBone_FromBone: ", "Internal Error Ocurred");
}
//-----------------(internal)
//Converts a PyBone to a bBone
struct Bone *PyBone_AsBone(BPy_Bone *py_Bone)
{
	return (py_Bone->bone);
}
