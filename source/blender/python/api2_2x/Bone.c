/* 
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
*/

#include "Bone.h"
#include "vector.h"
#include "matrix.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BKE_utildefines.h"
#include "gen_utils.h"
#include "BKE_armature.h"
#include "Mathutils.h"
#include "BKE_library.h"

//these must come in this order
#include "DNA_object_types.h" //1
#include "BIF_editarmature.h"   //2

//------------------------ERROR CODES---------------------------------
//This is here just to make me happy and to have more consistant error strings :)
static const char sEditBoneError[] = "EditBone - Error: ";
// static const char sEditBoneBadArgs[] = "EditBone - Bad Arguments: ";
static const char sBoneError[] = "Bone - Error: ";
// static const char sBoneBadArgs[] = "Bone - Bad Arguments: ";

//----------------------(internal)
//gets the bone->roll (which is a localspace roll) and puts it in parentspace
//(which is the 'roll' value the user sees)
static double boneRoll_ToArmatureSpace(struct Bone *bone)
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

	roll = atan2(difmat[2][0], difmat[2][2]); 
	if (difmat[0][0] < 0.0){
		roll += M_PI;
	}
	return roll; //result is in radians
}

//################## EditBone_Type ########################
/*This type is a wrapper for a tempory bone. This is an 'unparented' bone
*object. The armature->bonebase will be calculated from these temporary
*python tracked objects.*/
//####################################################

//------------------METHOD IMPLEMENTATIONS-----------------------------
//-------------------------EditBone.hasParent()
static PyObject *EditBone_hasParent(BPy_EditBone *self)
{
	if (self->editbone){
		if (self->editbone->parent)
			Py_RETURN_TRUE;
		else
			Py_RETURN_FALSE;
	}else{
		goto AttributeError;
	}

AttributeError:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".hasParent: ", "EditBone must be added to the armature first");
}
//-------------------------EditBone.clearParent()
static PyObject *EditBone_clearParent(BPy_EditBone *self)
{
	if (self->editbone){
		if (self->editbone->parent)
			self->editbone->parent = NULL;
		Py_RETURN_NONE;
	}else{
		goto AttributeError;
	}

AttributeError:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".clearParent: ", "EditBone must be added to the armature first");
}
//------------------ATTRIBUTE IMPLEMENTATION---------------------------
//------------------------EditBone.name (get)
static PyObject *EditBone_getName(BPy_EditBone *self, void *closure)
{
	if (self->editbone)
		return PyString_FromString(self->editbone->name);
	else
		return PyString_FromString(self->name);
}
//------------------------EditBone.name (set)
//check for char[] overflow here...
static int EditBone_setName(BPy_EditBone *self, PyObject *value, void *closure)
{  
	char *name = "";

	if (!PyArg_Parse(value, "s", &name))
		goto AttributeError;

	if (self->editbone)
        BLI_strncpy(self->editbone->name, name, 32);
	else
		BLI_strncpy(self->name, name, 32);
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".name: ", "expects a string");
}
//------------------------EditBone.roll (get)
static PyObject *EditBone_getRoll(BPy_EditBone *self, void *closure)
{
	if (self->editbone){
		return PyFloat_FromDouble((self->editbone->roll * (180/Py_PI)));
	}else{
		return PyFloat_FromDouble((self->roll * (180/Py_PI)));
	}
}
//------------------------EditBone.roll (set)
static int EditBone_setRoll(BPy_EditBone *self, PyObject *value, void *closure)
{  
	float roll = 0.0f;

	if (!PyArg_Parse(value, "f", &roll))
		goto AttributeError;

	if (self->editbone){
		self->editbone->roll = (float)(roll * (Py_PI/180));
	}else{
		self->roll = (float)(roll * (Py_PI/180));
	}
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".roll: ", "expects a float");
}
//------------------------EditBone.head (get)
static PyObject *EditBone_getHead(BPy_EditBone *self, void *closure)
{
	if (self->editbone){
		return newVectorObject(self->editbone->head, 3, Py_WRAP);
	}else{
		return newVectorObject(self->head, 3, Py_NEW);
	}
}
//------------------------EditBone.head (set)
static int EditBone_setHead(BPy_EditBone *self, PyObject *value, void *closure)
{  
	VectorObject *vec = NULL;
	int x;

	if (!PyArg_Parse(value, "O!", &vector_Type, &vec))
		goto AttributeError;
	if (vec->size != 3)
		goto AttributeError2;

	if (self->editbone){
		for (x = 0; x < 3; x++){
			self->editbone->head[x] = vec->vec[x];
		}
	}else{
		for (x = 0; x < 3; x++){
			self->head[x] = vec->vec[x];
		}
	}
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".head: ", "expects a Vector Object");

AttributeError2:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".head: ", "Vector Object needs to be (x,y,z)");
}
//------------------------EditBone.tail (get)
static PyObject *EditBone_getTail(BPy_EditBone *self, void *closure)
{
	if (self->editbone){
		return newVectorObject(self->editbone->tail, 3, Py_WRAP);
	}else{
		return newVectorObject(self->tail, 3, Py_NEW);
	}
}
//------------------------EditBone.tail (set)
static int EditBone_setTail(BPy_EditBone *self, PyObject *value, void *closure)
{  
	VectorObject *vec = NULL;
	int x;

	if (!PyArg_Parse(value, "O!", &vector_Type, &vec))
		goto AttributeError;
	if (vec->size != 3)
		goto AttributeError2;

	if (self->editbone){
		for (x = 0; x < 3; x++){
			self->editbone->tail[x] = vec->vec[x];
		}
	}else{
		for (x = 0; x < 3; x++){
			self->tail[x] = vec->vec[x];
		}
	}
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".tail: ", "expects a Vector Object");

AttributeError2:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".tail: ", "Vector Object needs to be (x,y,z)");
}
//------------------------EditBone.weight (get)
static PyObject *EditBone_getWeight(BPy_EditBone *self, void *closure)
{
	if (self->editbone)
		return PyFloat_FromDouble(self->editbone->weight);
	else
		return PyFloat_FromDouble(self->weight);
}
//------------------------EditBone.weight (set)
static int EditBone_setWeight(BPy_EditBone *self, PyObject *value, void *closure)
{  
	float weight;

	if (!PyArg_Parse(value, "f", &weight))
		goto AttributeError;
	CLAMP(weight, 0.0f, 1000.0f);

	if (self->editbone)
		self->editbone->weight = weight;
	else
		self->weight = weight;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".weight: ", "expects a float");
}
//------------------------EditBone.deform_dist (get)
static PyObject *EditBone_getDeform_dist(BPy_EditBone *self, void *closure)
{
	if (self->editbone)
		return PyFloat_FromDouble(self->editbone->dist);
	else
		return PyFloat_FromDouble(self->dist);
}
//------------------------EditBone.deform_dist (set)
static int EditBone_setDeform_dist(BPy_EditBone *self, PyObject *value, void *closure)
{  
	float deform;

	if (!PyArg_Parse(value, "f", &deform))
		goto AttributeError;
	CLAMP(deform, 0.0f, 1000.0f);

	if (self->editbone)
		self->editbone->dist = deform;
	else
		self->dist = deform;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".deform_dist: ", "expects a float");
}
//------------------------EditBone.subdivisions (get)
static PyObject *EditBone_getSubdivisions(BPy_EditBone *self, void *closure)
{
	if (self->editbone)
		return PyInt_FromLong(self->editbone->segments);
	else
		return PyInt_FromLong(self->segments);
}
//------------------------EditBone.subdivisions (set)
static int EditBone_setSubdivisions(BPy_EditBone *self, PyObject *value, void *closure)
{  
	int segs;

	if (!PyArg_Parse(value, "i", &segs))
		goto AttributeError;
	CLAMP(segs, 1, 32);

	if (self->editbone)
		self->editbone->segments = (short)segs;
	else
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
	if (!list)
		goto RuntimeError;

	if(self->editbone){
		if(self->editbone->flag & BONE_CONNECTED)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "CONNECTED")) == -1)
				goto RuntimeError;
		if(self->editbone->flag & BONE_HINGE)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "HINGE")) == -1)
				goto RuntimeError;
		if(self->editbone->flag & BONE_NO_DEFORM)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "NO_DEFORM")) == -1)
				goto RuntimeError;
		if(self->editbone->flag & BONE_MULT_VG_ENV)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "MULTIPLY")) == -1)
				goto RuntimeError;
		if(self->editbone->flag & BONE_HIDDEN_A)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "HIDDEN_EDIT")) == -1)
				goto RuntimeError;
		if(self->editbone->flag & BONE_ROOTSEL)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "ROOT_SELECTED")) == -1)
				goto RuntimeError;
		if(self->editbone->flag & BONE_SELECTED)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "BONE_SELECTED")) == -1)
				goto RuntimeError;
		if(self->editbone->flag & BONE_TIPSEL)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "TIP_SELECTED")) == -1)
				goto RuntimeError;
		if(self->editbone->flag & BONE_EDITMODE_LOCKED)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "LOCKED_EDIT")) == -1)
				goto RuntimeError;
	}else{
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
		if(self->flag & BONE_ROOTSEL)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "ROOT_SELECTED")) == -1)
				goto RuntimeError;
		if(self->flag & BONE_SELECTED)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "BONE_SELECTED")) == -1)
				goto RuntimeError;
		if(self->flag & BONE_TIPSEL)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "TIP_SELECTED")) == -1)
				goto RuntimeError;
		if(self->flag & BONE_EDITMODE_LOCKED)
			if (PyList_Append(list, 
				EXPP_GetModuleConstant("Blender.Armature", "LOCKED_EDIT")) == -1)
 				goto RuntimeError;
	}

	return list;

RuntimeError:
	Py_XDECREF( list );
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
			if (!name) 
				return 0;
			if (!STREQ3(PyString_AsString(name), "CONNECTED", "HINGE", "NO_DEFORM")	&&
				!STREQ3(PyString_AsString(name), "ROOT_SELECTED", "BONE_SELECTED", "TIP_SELECTED")	&&
				!STREQ3(PyString_AsString(name), "MULTIPLY", "HIDDEN_EDIT", "LOCKED_EDIT"))
				return 0;
			else
				return 1;
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

		//set the options
		if(self->editbone){
			//make sure the 'connected' property is set up correctly
			if (new_flag & BONE_CONNECTED) {
				if(!self->editbone->parent)
					goto AttributeError3;
				else
					VECCOPY(self->editbone->head, self->editbone->parent->tail);
			}
			self->editbone->flag = new_flag;
		}else{
			self->flag = new_flag;
		}
		return 0;
	}else if (BPy_Constant_Check(value)){
		if (!EditBone_CheckValidConstant(value))
			goto AttributeError2;
		val = PyDict_GetItemString(((BPy_constant*)value)->dict, "value");
		if (PyInt_Check(val)){
			numeric_value = (int)PyInt_AS_LONG(val);

			if(self->editbone){
				//make sure the 'connected' property is set up correctly
				if (numeric_value & BONE_CONNECTED) {
					if(!self->editbone->parent)
						goto AttributeError3;
					else
						VECCOPY(self->editbone->head, self->editbone->parent->tail);
				}
				self->editbone->flag = numeric_value;
			}else{
				self->flag = numeric_value;
			}
			return 0;
		}else{
			goto AttributeError2;
		}
	}else{
		goto AttributeError1;
	}

AttributeError1:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".options: ", "Expects a constant or list of constants");
AttributeError2:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".options: ", "Please use a constant defined in the Armature module");
AttributeError3:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".options: ", "You can't connect to parent because no parent is set");
}
//------------------------EditBone.parent (get)
static PyObject *EditBone_getParent(BPy_EditBone *self, void *closure)
{
	if (self->editbone){
		if (self->editbone->parent)
			return PyEditBone_FromEditBone(self->editbone->parent);
		else
			Py_RETURN_NONE;
	}else{
		Py_RETURN_NONE; //not in the list yet can't have a parent
	}
}
//------------------------EditBone.parent (set)
static int EditBone_setParent(BPy_EditBone *self, PyObject *value, void *closure)
{  
	BPy_EditBone *parent = NULL;

	if (!PyArg_Parse(value, "O!", &EditBone_Type, &parent))
		goto AttributeError;

	if (!parent->editbone)
		goto AttributeError2;

	if (self->editbone){
        self->editbone->parent = parent->editbone;
	}else{
		self->parent = parent->editbone;
	}
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".parent: ", "expects a EditBone Object");
AttributeError2:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".parent: ", "This object is not in the armature's bone list!");
}
//------------------------EditBone.matrix (get)
static PyObject *EditBone_getMatrix(BPy_EditBone *self, void *closure)
{
	float boneMatrix[3][3];
	float	axis[3];

	if (self->editbone){
		VECSUB(axis, self->editbone->tail, self->editbone->head);
		vec_roll_to_mat3(axis, self->editbone->roll, boneMatrix);
	}else{
		VECSUB(axis, self->tail, self->head);
		vec_roll_to_mat3(axis, self->roll, boneMatrix);
	}

    return newMatrixObject((float*)boneMatrix, 3, 3, Py_NEW);
}
//------------------------EditBone.matrix (set)
static int EditBone_setMatrix(BPy_EditBone *self, PyObject *value, void *closure)
{  
	PyObject *matrix;
	float roll, length, vec[3], axis[3], mat3[3][3];

	if (!PyArg_Parse(value, "O!", &matrix_Type, &matrix))
		goto AttributeError;

	//make sure we have the right sizes
	if (((MatrixObject*)matrix)->rowSize != 3 && ((MatrixObject*)matrix)->colSize != 3){
		if(((MatrixObject*)matrix)->rowSize != 4 && ((MatrixObject*)matrix)->colSize != 4){
			goto AttributeError;
		}
	}
		
	/*vec will be a normalized directional vector
	* together with the length of the old bone vec*length = the new vector*/
	/*The default rotation is 0,1,0 on the Y axis (see mat3_to_vec_roll)*/
	if (((MatrixObject*)matrix)->rowSize == 4){
		Mat3CpyMat4(mat3, ((float (*)[4])*((MatrixObject*)matrix)->matrix));
	}else{
		Mat3CpyMat3(mat3, ((float (*)[3])*((MatrixObject*)matrix)->matrix));
	}
	mat3_to_vec_roll(mat3, vec, &roll);

	//if a 4x4 matrix was passed we'll translate the vector otherwise not
	if (self->editbone){
		self->editbone->roll = roll;
		VecSubf(axis, self->editbone->tail, self->editbone->head);
		length =  VecLength(axis);
		VecMulf(vec, length);
		if (((MatrixObject*)matrix)->rowSize == 4)
			VecCopyf(self->editbone->head, ((MatrixObject*)matrix)->matrix[3]);
		VecAddf(self->editbone->tail, self->editbone->head, vec);
		return 0;
	}else{
		self->roll = roll;
		VecSubf(axis, self->tail, self->head);
		length =  VecLength(axis);
		VecMulf(vec, length);
		if (((MatrixObject*)matrix)->rowSize == 4)
			VecCopyf(self->head, ((MatrixObject*)matrix)->matrix[3]);
		VecAddf(self->tail, self->head, vec);
		return 0;
	}

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".matrix: ", "expects a 3x3 or 4x4 Matrix Object");
}
//------------------------Bone.length (get)
static PyObject *EditBone_getLength(BPy_EditBone *self, void *closure)
{
	float delta[3];
	double dot = 0.0f;
	int x;

	if (self->editbone){
		VECSUB(delta, self->editbone->tail, self->editbone->head);
		for(x = 0; x < 3; x++){
			dot += (delta[x] * delta[x]);
		}
		return PyFloat_FromDouble(sqrt(dot));
	}else{
		VECSUB(delta, self->tail, self->head);
		for(x = 0; x < 3; x++){
			dot += (delta[x] * delta[x]);
		}
		return PyFloat_FromDouble(sqrt(dot));
	}
}
//------------------------Bone.length (set)
static int EditBone_setLength(BPy_EditBone *self, PyObject *value, void *closure)
{  
	printf("Sorry this isn't implemented yet.... :/");
	return 1;
}


//------------------------Bone.headRadius (get)
static PyObject *EditBone_getHeadRadius(BPy_EditBone *self, void *closure)
{
	if (self->editbone)
		if (self->editbone->parent && self->editbone->flag & BONE_CONNECTED)
			return PyFloat_FromDouble(self->editbone->parent->rad_tail);
		else
			return PyFloat_FromDouble(self->editbone->rad_head);
	else
		if (self->parent && self->flag & BONE_CONNECTED)
			return PyFloat_FromDouble(self->parent->rad_tail);
		else
			return PyFloat_FromDouble(self->rad_head);
}
//------------------------Bone.headRadius (set)
static int EditBone_setHeadRadius(BPy_EditBone *self, PyObject *value, void *closure)
{  
	float radius;
	if (!PyArg_Parse(value, "f", &radius))
		goto AttributeError;
	CLAMP(radius, 0.0f, 10000.0f);

	if (self->editbone)
		if (self->editbone->parent && self->editbone->flag & BONE_CONNECTED)
			self->editbone->parent->rad_tail= radius;
		else
			self->editbone->rad_head= radius;
	else
		if (self->parent && self->flag & BONE_CONNECTED)
			self->parent->rad_tail= radius;
		else
			self->rad_head= radius;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".headRadius: ", "expects a float");
}


//------------------------Bone.tailRadius (get)
static PyObject *EditBone_getTailRadius(BPy_EditBone *self, void *closure)
{
	if (self->editbone)
		return PyFloat_FromDouble(self->editbone->rad_tail);
	else
		return PyFloat_FromDouble(self->rad_tail);
}
//------------------------Bone.tailRadius (set)
static int EditBone_setTailRadius(BPy_EditBone *self, PyObject *value, void *closure)
{  
	float radius;
	if (!PyArg_Parse(value, "f", &radius))
		goto AttributeError;
	CLAMP(radius, 0.0f, 10000.0f);

	if (self->editbone)
		self->editbone->rad_tail = radius;
	else
		self->rad_tail = radius;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".tailRadius: ", "expects a float");
}

//------------------------Bone.layerMask (get)
static PyObject *EditBone_getLayerMask(BPy_EditBone *self)
{
	/* do this extra stuff because the short's bits can be negative values */
	unsigned short laymask = 0;
	if (self->editbone)	laymask |= self->editbone->layer;
	else				laymask |= self->layer;
	return PyInt_FromLong((int)laymask);
}
//------------------------Bone.layerMask (set)
static int EditBone_setLayerMask(BPy_EditBone *self, PyObject *value)
{
	int laymask;
	if (!PyInt_Check(value)) {
		return EXPP_ReturnIntError( PyExc_AttributeError,
									"expected an integer (bitmask) as argument" );
	}
	
	laymask = PyInt_AsLong(value);

	if (laymask <= 0 || laymask > (1<<16) - 1)
		return EXPP_ReturnIntError( PyExc_AttributeError,
									"bitmask must have from 1 up to 16 bits set");
	
	if (self->editbone) {
		self->editbone->layer = 0;
		self->editbone->layer |= laymask;
	} else {
		self->layer = 0;
		self->layer |= laymask;
	}
	
	return 0;
}

//------------------TYPE_OBECT IMPLEMENTATION--------------------------
//------------------------tp_methods
//This contains a list of all methods the object contains
static PyMethodDef BPy_EditBone_methods[] = {
	{"hasParent", (PyCFunction) EditBone_hasParent, METH_NOARGS, 
		"() - True/False - Bone has a parent"},
	{"clearParent", (PyCFunction) EditBone_clearParent, METH_NOARGS, 
		"() - sets the parent to None"},
	{NULL, NULL, 0, NULL}
};
///------------------------tp_getset
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
	{"deformDist", (getter)EditBone_getDeform_dist, (setter)EditBone_setDeform_dist, 
		"The distance at which deformation has effect", NULL},
	{"subdivisions", (getter)EditBone_getSubdivisions, (setter)EditBone_setSubdivisions, 
		"The number of subdivisions (for B-Bones)", NULL},
	{"options", (getter)EditBone_getOptions, (setter)EditBone_setOptions, 
		"The options effective on this bone", NULL},
	{"parent", (getter)EditBone_getParent, (setter)EditBone_setParent, 
		"The parent bone of this bone", NULL},
	{"length", (getter)EditBone_getLength, (setter)EditBone_setLength, 
		"The length of this bone", NULL},
	{"tailRadius", (getter)EditBone_getTailRadius, (setter)EditBone_setTailRadius, 
		"Set the radius of this bones tip", NULL},
	{"headRadius", (getter)EditBone_getHeadRadius, (setter)EditBone_setHeadRadius, 
		"Set the radius of this bones head", NULL},
	{"layerMask", (getter)EditBone_getLayerMask, (setter)EditBone_setLayerMask, 
		"Layer bitmask", NULL },
	{NULL, NULL, NULL, NULL,NULL}
};

//------------------------tp_repr
//This is the string representation of the object
static PyObject *EditBone_repr(BPy_EditBone *self)
{
	if (self->editbone)
		return PyString_FromFormat( "[EditBone \"%s\"]", self->editbone->name ); 
	else
		return PyString_FromFormat( "[EditBone \"%s\"]", self->name ); 
}

static int EditBone_compare( BPy_EditBone * a, BPy_EditBone * b )
{
	/* if they are not wrapped, then they cant be the same */
	if (a->editbone==NULL && b->editbone==NULL) return -1;
	return ( a->editbone == b->editbone ) ? 0 : -1;
}


//------------------------tp_doc
//The __doc__ string for this object
static char BPy_EditBone_doc[] = "This is an internal subobject of armature\
designed to act as a wrapper for an 'edit bone'.";

//------------------------tp_new
//This methods creates a new object (note it does not initialize it - only the building)
static PyObject *EditBone_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	char *name = "myEditBone";
	BPy_EditBone *py_editBone = NULL;
	float head[3], tail[3];

	py_editBone = (BPy_EditBone*)type->tp_alloc(type, 0); //new
	if (py_editBone == NULL)
		goto RuntimeError;

	//this pointer will be set when this bone is placed in ListBase
	//otherwise this will act as a py_object
	py_editBone->editbone = NULL;

	unique_editbone_name(NULL, name);
	BLI_strncpy(py_editBone->name, name, 32);
	py_editBone->parent = NULL;
	py_editBone->weight= 1.0f;
	py_editBone->dist= 0.25f;
	py_editBone->xwidth= 0.1f;
	py_editBone->zwidth= 0.1f;
	py_editBone->ease1= 1.0f;
	py_editBone->ease2= 1.0f;
	py_editBone->rad_head= 0.10f;
	py_editBone->rad_tail= 0.05f;
	py_editBone->segments= 1;
	py_editBone->layer= 1;
	py_editBone->flag = 0;
	py_editBone->roll = 0.0f;

	head[0] = head[1] = head[2] = 0.0f;
	tail[1] = tail[2] = 0.0f;
	tail[0] = 1.0f;
	VECCOPY(py_editBone->head, head);
	VECCOPY(py_editBone->tail, tail);

	return (PyObject*)py_editBone;

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sEditBoneError, " __new__: ", "Internal Error");
}
//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
//the struct EditBone pointer will be handled by the BPy_BonesDict class
static void EditBone_dealloc(BPy_EditBone * self)
{
	EditBone_Type.tp_free(self);
	return;
}
//------------------TYPE_OBECT DEFINITION--------------------------
PyTypeObject EditBone_Type = {
	PyObject_HEAD_INIT(NULL)       //tp_head
	0,											//tp_internal
	"EditBone",								//tp_name
	sizeof(BPy_EditBone),				//tp_basicsize
	0,											//tp_itemsize
	(destructor)EditBone_dealloc,		//tp_dealloc
	0,											//tp_print
	0,											//tp_getattr
	0,											//tp_setattr
	(cmpfunc)EditBone_compare,					//tp_compare
	(reprfunc)EditBone_repr,			//tp_repr
	0,											//tp_as_number
	0,											//tp_as_sequence
	0,											//tp_as_mapping
	0,											//tp_hash
	0,											//tp_call
	0,											//tp_str
	0,											//tp_getattro
	0,											//tp_setattro
	0,											//tp_as_buffer
	Py_TPFLAGS_DEFAULT,				//tp_flags
	BPy_EditBone_doc,					//tp_doc
	0,											//tp_traverse
	0,											//tp_clear
	0,											//tp_richcompare
	0,											//tp_weaklistoffset
	0,											//tp_iter
	0,											//tp_iternext
	BPy_EditBone_methods,				//tp_methods
	0,											//tp_members
	BPy_EditBone_getset,				//tp_getset
	0,											//tp_base
	0,											//tp_dict
	0,											//tp_descr_get
	0,											//tp_descr_set
	0,											//tp_dictoffset
	0, 											//tp_init
	0,											//tp_alloc
	(newfunc)EditBone_new,			//tp_new
	0,											//tp_free
	0,											//tp_is_gc
	0,											//tp_bases
	0,											//tp_mro
	0,											//tp_cache
	0,											//tp_subclasses
	0,											//tp_weaklist
	0											//tp_del
};

//------------------METHOD IMPLEMENTATIONS--------------------------------
//------------------------(internal) PyBone_ChildrenAsList
static int PyBone_ChildrenAsList(PyObject *list, ListBase *bones){
	Bone *bone = NULL;
	PyObject *py_bone = NULL;

	for (bone = bones->first; bone; bone = bone->next){
		py_bone = PyBone_FromBone(bone);
		if (py_bone == NULL)
			return 0;
		
		if(PyList_Append(list, py_bone) == -1){
			return 0;
		}
		Py_DECREF(py_bone);
		if (bone->childbase.first) 
			if (!PyBone_ChildrenAsList(list, &bone->childbase))
				return 0;
	}
	return 1;
}
//-------------------------Bone.hasParent()
static PyObject *Bone_hasParent(BPy_Bone *self)
{
	if (self->bone->parent)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}
//-------------------------Bone.hasChildren()
static PyObject *Bone_hasChildren(BPy_Bone *self)
{
	if (self->bone->childbase.first)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}
//-------------------------Bone.getAllChildren()
static PyObject *Bone_getAllChildren(BPy_Bone *self)
{
	PyObject *list = PyList_New(0);
	if (!self->bone->childbase.first) {
		/* do nothing */
	} else if (!PyBone_ChildrenAsList(list, &self->bone->childbase)) {
		Py_XDECREF(list);
		EXPP_objError(PyExc_RuntimeError, "%s%s", 
				sBoneError, "Internal error trying to wrap blender bones!");
	}
	return list;
}

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
		sBoneError, "You must first call .makeEditable() to edit the armature");
}
//------------------------Bone.roll (get)
static PyObject *Bone_getRoll(BPy_Bone *self, void *closure)
{	
	return Py_BuildValue("{s:f, s:f}", 
		"BONESPACE", self->bone->roll * (180/Py_PI),
		"ARMATURESPACE", boneRoll_ToArmatureSpace(self->bone) * (180/Py_PI));
}
//------------------------Bone.roll (set)
static int Bone_setRoll(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call .makeEditable() to edit the armature");
}
//------------------------Bone.head (get)
static PyObject *Bone_getHead(BPy_Bone *self, void *closure)
{
	PyObject *val1 = newVectorObject(self->bone->head, 3, Py_WRAP);
	PyObject *val2 = newVectorObject(self->bone->arm_head, 3, Py_WRAP);
	PyObject *ret =	Py_BuildValue(
			"{s:O, s:O}", "BONESPACE", val1, "ARMATURESPACE", val2);
	
	Py_DECREF(val1);
	Py_DECREF(val2);
	return ret;
}
//------------------------Bone.head (set)
static int Bone_setHead(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call .makeEditable() to edit the armature");
}
//------------------------Bone.tail (get)
static PyObject *Bone_getTail(BPy_Bone *self, void *closure)
{
	PyObject *val1 = newVectorObject(self->bone->tail, 3, Py_WRAP);
	PyObject *val2 = newVectorObject(self->bone->arm_tail, 3, Py_WRAP);
	PyObject *ret =	Py_BuildValue("{s:O, s:O}", 
		"BONESPACE", val1, "ARMATURESPACE", val2);
	
	Py_DECREF(val1);
	Py_DECREF(val2);
	return ret;
}
//------------------------Bone.tail (set)
static int Bone_setTail(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call .makeEditable() to edit the armature");
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
		sBoneError, "You must first call .makeEditable() to edit the armature");
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
		sBoneError, "You must first call .makeEditable() to edit the armature");
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
		sBoneError, "You must first call .makeEditable() to edit the armature");
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
	if(self->bone->flag & BONE_ROOTSEL)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "ROOT_SELECTED")) == -1)
			goto RuntimeError;
	if(self->bone->flag & BONE_SELECTED)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "BONE_SELECTED")) == -1)
			goto RuntimeError;
	if(self->bone->flag & BONE_TIPSEL)
		if (PyList_Append(list, 
			EXPP_GetModuleConstant("Blender.Armature", "TIP_SELECTED")) == -1)
			goto RuntimeError;

	return list;
	
RuntimeError:
	Py_XDECREF(list);
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sBoneError, "getOptions(): ", "Internal failure!");
}
//------------------------Bone.connected (set)
static int Bone_setOptions(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call .makeEditable() to edit the armature");
}
//------------------------Bone.parent (get)
static PyObject *Bone_getParent(BPy_Bone *self, void *closure)
{
	if (self->bone->parent)
		return PyBone_FromBone(self->bone->parent);
	else
		Py_RETURN_NONE;
}
//------------------------Bone.parent (set)
static int Bone_setParent(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call .makeEditable() to edit the armature");
}
//------------------------Bone.children (get)
static PyObject *Bone_getChildren(BPy_Bone *self, void *closure)
{
	PyObject *list = PyList_New(0);
	Bone *bone = NULL;
	PyObject *py_bone = NULL;

	if (self->bone->childbase.first){
		for (bone = self->bone->childbase.first; bone; bone = bone->next){
			py_bone = PyBone_FromBone(bone);
			if (py_bone == NULL)
				goto RuntimeError;
			if (PyList_Append(list, py_bone) == -1)
				goto RuntimeError;
			Py_DECREF(py_bone);
		}
	}
	return list;
	
RuntimeError:
	Py_XDECREF(list);
	Py_XDECREF(py_bone);
	return EXPP_objError(PyExc_RuntimeError, "%s%s", 
		sBoneError, "Internal error trying to wrap blender bones!");
}
//------------------------Bone.children (set)
static int Bone_setChildren(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call .makeEditable() to edit the armature");
}
//------------------------Bone.matrix (get)
static PyObject *Bone_getMatrix(BPy_Bone *self, void *closure)
{
	PyObject *val1 = newMatrixObject((float*)self->bone->bone_mat, 3,3, Py_WRAP);
	PyObject *val2 = newMatrixObject((float*)self->bone->arm_mat, 4,4, Py_WRAP);
	PyObject *ret =	Py_BuildValue("{s:O, s:O}", 
		"BONESPACE", val1, "ARMATURESPACE", val2);
	Py_DECREF(val1);
	Py_DECREF(val2);
	return ret;
    
    
}
//------------------------Bone.matrix (set)
static int Bone_setMatrix(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call .makeEditable() to edit the armature");
}
//------------------------Bone.length (get)
static PyObject *Bone_getLength(BPy_Bone *self, void *closure)
{
    return PyFloat_FromDouble(self->bone->length);
}
//------------------------Bone.length (set)
static int Bone_setLength(BPy_Bone *self, PyObject *value, void *closure)
{  
  return EXPP_intError(PyExc_ValueError, "%s%s", 
		sBoneError, "You must first call .makeEditable() to edit the armature");
}

//------------------------Bone.headRadius (get)
static PyObject *Bone_getHeadRadius(BPy_Bone *self, void *closure)
{

	if (self->bone->parent && self->bone->flag & BONE_CONNECTED)
		return PyFloat_FromDouble(self->bone->parent->rad_tail);
	else
		return PyFloat_FromDouble(self->bone->rad_head);
}
//------------------------Bone.headRadius (set)
static int Bone_setHeadRadius(BPy_Bone *self, PyObject *value, void *closure)
{  
	float radius;
	if (!PyArg_Parse(value, "f", &radius))
		goto AttributeError;
	CLAMP(radius, 0.0f, 10000.0f);

	if (self->bone->parent && self->bone->flag & BONE_CONNECTED)
		self->bone->parent->rad_tail= radius;
	else
		self->bone->rad_head= radius;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".headRadius: ", "expects a float");
}

//------------------------Bone.tailRadius (get)
static PyObject *Bone_getTailRadius(BPy_Bone *self, void *closure)
{
	return PyFloat_FromDouble(self->bone->rad_tail);
}

//------------------------Bone.headRadius (set)
static int Bone_setTailRadius(BPy_Bone *self, PyObject *value, void *closure)
{  
	float radius;
	if (!PyArg_Parse(value, "f", &radius))
		goto AttributeError;
	CLAMP(radius, 0.0f, 10000.0f);
	self->bone->rad_tail= radius;
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sEditBoneError, ".headRadius: ", "expects a float");
}

//------------------------Bone.layerMask (get)
static PyObject *Bone_getLayerMask(BPy_Bone *self)
{
	/* do this extra stuff because the short's bits can be negative values */
	unsigned short laymask = 0;
	laymask |= self->bone->layer;
	return PyInt_FromLong((int)laymask);
}
//------------------------Bone.layerMask (set)
static int Bone_setLayerMask(BPy_Bone *self, PyObject *value)
{
	int laymask;
	if (!PyInt_Check(value)) {
		return EXPP_ReturnIntError( PyExc_AttributeError,
									"expected an integer (bitmask) as argument" );
	}
	
	laymask = PyInt_AsLong(value);

	if (laymask <= 0 || laymask > (1<<16) - 1)
		return EXPP_ReturnIntError( PyExc_AttributeError,
									"bitmask must have from 1 up to 16 bits set");

	self->bone->layer = 0;
	self->bone->layer |= laymask;

	return 0;
}

//------------------TYPE_OBECT IMPLEMENTATION--------------------------
//------------------------tp_methods
//This contains a list of all methods the object contains
static PyMethodDef BPy_Bone_methods[] = {
	{"hasParent", (PyCFunction) Bone_hasParent, METH_NOARGS, 
		"() - True/False - Bone has a parent"},
	{"hasChildren", (PyCFunction) Bone_hasChildren, METH_NOARGS, 
		"() - True/False - Bone has 1 or more children"},
	{"getAllChildren", (PyCFunction) Bone_getAllChildren, METH_NOARGS, 
		"() - All the children for this bone - including children's children"},
	{NULL, NULL, 0, NULL}
};
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
	{"length", (getter)Bone_getLength, (setter)Bone_setLength, 
		"The length of this bone", NULL},
	{"tailRadius", (getter)Bone_getTailRadius, (setter)Bone_setTailRadius, 
		"Set the radius of this bones tip", NULL},
	{"headRadius", (getter)Bone_getHeadRadius, (setter)Bone_setHeadRadius, 
		"Set the radius of this bones head", NULL},
	{"layerMask", (getter)Bone_getLayerMask, (setter)Bone_setLayerMask, 
		"Layer bitmask", NULL },
	{NULL, NULL, NULL, NULL,NULL}
};
//------------------------tp_repr
//This is the string representation of the object
static PyObject *Bone_repr(BPy_Bone *self)
{
	return PyString_FromFormat( "[Bone \"%s\"]", self->bone->name ); 
}
static int Bone_compare( BPy_Bone * a, BPy_Bone * b )
{
	return ( a->bone == b->bone ) ? 0 : -1;
}
//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
static void Bone_dealloc(BPy_Bone * self)
{
	Bone_Type.tp_free(self);
	return;
}
//------------------------tp_doc
//The __doc__ string for this object
static char BPy_Bone_doc[] = "This object wraps a Blender Boneobject.\n\
					  This object is a subobject of the Armature object.";

//------------------TYPE_OBECT DEFINITION--------------------------
PyTypeObject Bone_Type = {
	PyObject_HEAD_INIT(NULL)   //tp_head
	0,										//tp_internal
	"Bone",									//tp_name
	sizeof(BPy_Bone),						//tp_basicsize
	0,										//tp_itemsize
	(destructor)Bone_dealloc,				//tp_dealloc
	0,										//tp_print
	0,										//tp_getattr
	0,										//tp_setattr
	(cmpfunc) Bone_compare,					//tp_compare
	(reprfunc) Bone_repr,					//tp_repr
	0,										//tp_as_number
	0,										//tp_as_sequence
	0,										//tp_as_mapping
	0,										//tp_hash
	0,										//tp_call
	0,										//tp_str
	0,										//tp_getattro
	0,										//tp_setattro
	0,										//tp_as_buffer
	Py_TPFLAGS_DEFAULT,         //tp_flags
	BPy_Bone_doc,					//tp_doc
	0,										//tp_traverse
	0,										//tp_clear
	0,										//tp_richcompare
	0,										//tp_weaklistoffset
	0,										//tp_iter
	0,										//tp_iternext
	BPy_Bone_methods,				//tp_methods
	0,										//tp_members
	BPy_Bone_getset,				//tp_getset
	0,										//tp_base
	0,										//tp_dict
	0,										//tp_descr_get
	0,										//tp_descr_set
	0,										//tp_dictoffset
	0,										//tp_init
	0,										//tp_alloc
	0,										//tp_new
	0,										//tp_free
	0,										//tp_is_gc
	0,										//tp_bases
	0,										//tp_mro
	0,										//tp_cache
	0,										//tp_subclasses
	0,										//tp_weaklist
	0										//tp_del
};
//------------------VISIBLE PROTOTYPE IMPLEMENTATION-----------------------
//-----------------(internal)
//Converts a struct EditBone to a BPy_EditBone
PyObject *PyEditBone_FromEditBone(struct EditBone *editbone)
{
	BPy_EditBone *py_editbone = NULL;

	py_editbone = (BPy_EditBone*)EditBone_Type.tp_alloc(&EditBone_Type, 0); //*new*
	if (!py_editbone)
		goto RuntimeError;

	py_editbone->editbone = editbone;

	return (PyObject *) py_editbone;

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sEditBoneError, "PyEditBone_FromEditBone: ", "Internal Error Ocurred");
}
//-----------------(internal)
//Converts a struct Bone to a BPy_Bone
PyObject *PyBone_FromBone(struct Bone *bone)
{
	BPy_Bone *py_Bone = ( BPy_Bone * ) PyObject_NEW( BPy_Bone, &Bone_Type );
	
	py_Bone->bone = bone;

	return (PyObject *) py_Bone;
}
//-----------------(internal)
//Converts a PyBone to a bBone
struct Bone *PyBone_AsBone(BPy_Bone *py_Bone)
{
	return (py_Bone->bone);
}
