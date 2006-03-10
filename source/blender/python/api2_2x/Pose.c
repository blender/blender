/* 
 * $Id:
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

#include "Pose.h"


#include "mydevice.h"
#include "BKE_armature.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_action.h"
#include "BKE_utildefines.h"
#include "BIF_editaction.h"
#include "BIF_space.h"
#include "BKE_depsgraph.h"
#include "DNA_object_types.h"
#include "DNA_ipo_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"	//1 - this order
#include "BSE_editipo.h"			//2
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "Mathutils.h"
#include "Object.h"
#include "NLA.h"
#include "gen_utils.h"

extern void chan_calc_mat(bPoseChannel *chan);

//------------------------ERROR CODES---------------------------------
//This is here just to make me happy and to have more consistant error strings :)
static const char sPoseError[] = "Pose - Error: ";
//static const char sPoseBadArgs[] = "Pose - Bad Arguments: ";
static const char sPoseBoneError[] = "PoseBone - Error: ";
//static const char sPoseBoneBadArgs[] = "PoseBone - Bad Arguments: ";
static const char sPoseBonesDictError[] = "PoseBone - Error: ";
//static const char sPoseBonesDictBadArgs[] = "PoseBone - Bad Arguments: ";

//################## PoseBonesDict_Type (internal) ########################
/*This is an internal psuedo-dictionary type that allows for manipulation
* of posechannels inside of a pose structure. It is a subobject of pose.
* i.e. Pose.bones['key']*/
//################################################################

//------------------METHOD IMPLEMENTATIONS-----------------------------
//------------------------Pose.bones.items()
//Returns a list of key:value pairs like dict.items()
PyObject* PoseBonesDict_items(BPy_PoseBonesDict *self)
{
	return PyDict_Items(self->bonesMap); 
}
//------------------------Pose.bones.keys()
//Returns a list of keys like dict.keys()
PyObject* PoseBonesDict_keys(BPy_PoseBonesDict *self)
{
	return PyDict_Keys(self->bonesMap);
}
//------------------------Armature.bones.values()
//Returns a list of values like dict.values()
PyObject* PoseBonesDict_values(BPy_PoseBonesDict *self)
{
	return PyDict_Values(self->bonesMap);
}
//------------------ATTRIBUTE IMPLEMENTATION---------------------------
//------------------TYPE_OBECT IMPLEMENTATION-----------------------
//------------------------tp_doc
//The __doc__ string for this object
static char BPy_PoseBonesDict_doc[] = "This is an internal subobject of pose\
designed to act as a Py_PoseBone dictionary.";

//------------------------tp_methods
//This contains a list of all methods the object contains
static PyMethodDef BPy_PoseBonesDict_methods[] = {
	{"items", (PyCFunction) PoseBonesDict_items, METH_NOARGS, 
		"() - Returns the key:value pairs from the dictionary"},
	{"keys", (PyCFunction) PoseBonesDict_keys, METH_NOARGS, 
		"() - Returns the keys the dictionary"},
	{"values", (PyCFunction) PoseBonesDict_values, METH_NOARGS, 
		"() - Returns the values from the dictionary"},
	{NULL, NULL, 0, NULL}
};
//-----------------(internal)
static int PoseBoneMapping_Init(PyObject *dictionary, ListBase *posechannels){
	bPoseChannel *pchan = NULL;
	PyObject *py_posechannel = NULL;

	for (pchan = posechannels->first; pchan; pchan = pchan->next){
		py_posechannel = PyPoseBone_FromPosechannel(pchan);
		if (!py_posechannel)
			return -1;

		if(PyDict_SetItemString(dictionary,
					pchan->name, py_posechannel) == -1){
			return -1;
		}
		Py_DECREF(py_posechannel);
	}
	return 0;
}

//----------------- BonesDict_InitBones
static int PoseBonesDict_InitBones(BPy_PoseBonesDict *self)
{
	PyDict_Clear(self->bonesMap);
	if (PoseBoneMapping_Init(self->bonesMap, self->bones) == -1)
		return 0;
	return 1;
} 

//------------------------tp_repr
//This is the string representation of the object
static PyObject *PoseBonesDict_repr(BPy_PoseBonesDict *self)
{
	char buffer[128], str[4096];
	PyObject *key, *value;
	int pos = 0;

	BLI_strncpy(str,"",4096);
	sprintf(buffer, "[Pose Bone Dict: {");
	strcat(str,buffer);
	while (PyDict_Next(self->bonesMap, &pos, &key, &value)) {
		sprintf(buffer, "%s : %s, ", PyString_AsString(key), 
			PyString_AsString(value->ob_type->tp_repr(value)));
		strcat(str,buffer);
	}
	sprintf(buffer, "}]\n");
	strcat(str,buffer);
	return PyString_FromString(str);
}

//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
static void PoseBonesDict_dealloc(BPy_PoseBonesDict * self)
{
	Py_DECREF(self->bonesMap);
	PoseBonesDict_Type.tp_free(self);
	return;
}
//------------------------mp_length
//This gets the size of the dictionary
int PoseBonesDict_len(BPy_PoseBonesDict *self)
{
	return BLI_countlist(self->bones);
}
//-----------------------mp_subscript
//This defines getting a bone from the dictionary - x = Bones['key']
PyObject *PoseBonesDict_GetItem(BPy_PoseBonesDict *self, PyObject* key)
{ 
	PyObject *value = NULL;

	value = PyDict_GetItem(self->bonesMap, key);
	if(value == NULL){
        return EXPP_incr_ret(Py_None);
	}
	return EXPP_incr_ret(value);
}
//------------------TYPE_OBECT DEFINITION--------------------------
//Mapping Protocol
static PyMappingMethods PoseBonesDict_MapMethods = {
	(inquiry) PoseBonesDict_len,					//mp_length
	(binaryfunc)PoseBonesDict_GetItem,		//mp_subscript
	0,														//mp_ass_subscript
};
//PoseBonesDict TypeObject
PyTypeObject PoseBonesDict_Type = {
	PyObject_HEAD_INIT(NULL)			//tp_head
	0,												//tp_internal
	"PoseBonesDict",								//tp_name
	sizeof(BPy_PoseBonesDict),					//tp_basicsize
	0,												//tp_itemsize
	(destructor)PoseBonesDict_dealloc,		//tp_dealloc
	0,												//tp_print
	0,												//tp_getattr
	0,												//tp_setattr
	0,												//tp_compare
	(reprfunc) PoseBonesDict_repr,				//tp_repr
	0,												//tp_as_number
	0,												//tp_as_sequence
	&PoseBonesDict_MapMethods,				//tp_as_mapping
	0,												//tp_hash
	0,												//tp_call
	0,												//tp_str
	0,												//tp_getattro
	0,												//tp_setattro
	0,												//tp_as_buffer
	Py_TPFLAGS_DEFAULT,					//tp_flags
	BPy_PoseBonesDict_doc,						//tp_doc
	0,												//tp_traverse
	0,												//tp_clear
	0,												//tp_richcompare
	0,												//tp_weaklistoffset
	0,												//tp_iter
	0,												//tp_iternext
	BPy_PoseBonesDict_methods,				//tp_methods
	0,												//tp_members
	0,												//tp_getset
	0,												//tp_base
	0,												//tp_dict
	0,												//tp_descr_get
	0,												//tp_descr_set
	0,												//tp_dictoffset
	0, 				                                //tp_init
	0,												//tp_alloc
	0,												//tp_new
	0,												//tp_free
	0,												//tp_is_gc
	0,												//tp_bases
	0,												//tp_mro
	0,												//tp_cache
	0,												//tp_subclasses
	0,												//tp_weaklist
	0												//tp_del
};
//-----------------------PyPoseBonesDict_FromPyPose
static PyObject *PyPoseBonesDict_FromPyPose(BPy_Pose *py_pose)
{
	BPy_PoseBonesDict *py_posebonesdict = NULL;

	//create py object
	py_posebonesdict = (BPy_PoseBonesDict *)PoseBonesDict_Type.tp_alloc(&PoseBonesDict_Type, 0); 
	if (!py_posebonesdict)
		goto RuntimeError;

	//create internal dictionaries
	py_posebonesdict->bonesMap = PyDict_New();
	if (!py_posebonesdict->bonesMap)
		goto RuntimeError;

	//set listbase pointer
	py_posebonesdict->bones = &py_pose->pose->chanbase;

	//now that everything is setup - init the mappings
	if (!PoseBonesDict_InitBones(py_posebonesdict))
		goto RuntimeError;

	return (PyObject*)py_posebonesdict;

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s", 
		sPoseBonesDictError, "Failed to create class");
}

//################## Pose_Type ##########################
/*This type is a wrapper for a pose*/
//####################################################
//------------------METHOD IMPLEMENTATIONS------------------------------
static PyObject *Pose_update(BPy_Pose *self)
{
	Object *daddy = NULL;

	self->pose->flag |= POSE_RECALC;

	for (daddy = G.main->object.first; daddy; daddy = daddy->id.next){
		if (daddy->pose == self->pose){
			break;
		}
	}

	if(daddy)
		where_is_pose(daddy);

	return EXPP_incr_ret(Py_None);
}
//------------------------tp_methods
//This contains a list of all methods the object contains
static PyMethodDef BPy_Pose_methods[] = {
	{"update", (PyCFunction) Pose_update, METH_NOARGS, 
		"() - Rebuilds the pose with new values"},
	{NULL, NULL, 0, NULL}
};
//------------------ATTRIBUTE IMPLEMENTATIONS---------------------------
//------------------------Pose.bones (getter)
//Gets the bones attribute
static PyObject *Pose_getBoneDict(BPy_Pose *self, void *closure)
{
    return EXPP_incr_ret((PyObject*)self->Bones);
}
//------------------------Pose.bones (setter)
//Sets the bones attribute
static int Pose_setBoneDict(BPy_Pose *self, PyObject *value, void *closure)
{
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sPoseError, "You are not allowed to change the .bones attribute");
}
//------------------TYPE_OBECT IMPLEMENTATION---------------------------
//------------------------tp_getset
//This contains methods for attributes that require checking
static PyGetSetDef BPy_Pose_getset[] = {
	{"bones", (getter)Pose_getBoneDict, (setter)Pose_setBoneDict, 
		"The pose's Bone dictionary", NULL},
	{NULL, NULL, NULL, NULL, NULL}
};
//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
static void Pose_dealloc(BPy_Pose *self)
{
	Py_DECREF(self->Bones);
	Pose_Type.tp_free(self);
	return;
}
//------------------------tp_repr
//This is the string representation of the object
static PyObject *Pose_repr(BPy_Pose *self)
{
	return PyString_FromFormat( "[Pose \"%s\"]", self->name); 
}
//------------------------tp_doc
//The __doc__ string for this object
static char BPy_Pose_doc[] = "This object wraps a Blender Pose object.";

//------------------TYPE_OBECT DEFINITION--------------------------
PyTypeObject Pose_Type = {
	PyObject_HEAD_INIT(NULL)   //tp_head
	0,										//tp_internal
	"Pose",								//tp_name
	sizeof(BPy_Pose),					//tp_basicsize
	0,										//tp_itemsize
	(destructor)Pose_dealloc,		//tp_dealloc
	0,										//tp_print
	0,										//tp_getattr
	0,										//tp_setattr
	0,										//tp_compare
	(reprfunc)Pose_repr,				//tp_repr
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
	BPy_Pose_doc,					//tp_doc
	0,										//tp_traverse
	0,										//tp_clear
	0,										//tp_richcompare
	0,										//tp_weaklistoffset
	0,										//tp_iter
	0,										//tp_iternext
	BPy_Pose_methods,				//tp_methods
	0,										//tp_members
	BPy_Pose_getset,					//tp_getset
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
//################## PoseBone_Type #####################
/*This type is a wrapper for a posechannel*/
//####################################################
//------------------METHOD IMPLEMENTATIONS------------------------------
//------------------------------PoseBone.insertKey()
static PyObject *PoseBone_insertKey(BPy_PoseBone *self, PyObject *args)
{
	PyObject *parent_object = NULL;
	PyObject *constants = NULL, *item = NULL;
	int frame = 1, oldframe, length, x, numeric_value = 0, oldflag;
	bPoseChannel *pchan = NULL;
	

	if (!PyArg_ParseTuple(args, "O!i|O", &Object_Type, &parent_object, &frame, &constants ))
		goto AttributeError;

	//verify that this pchannel is part of the object->pose
	for (pchan = ((BPy_Object*)parent_object)->object->pose->chanbase.first; 
		pchan; pchan = pchan->next){
		if (pchan == self->posechannel)
			break;
	}
	if (!pchan)
		goto AttributeError2;

	//verify that there is an action bound to this object
	if (!((BPy_Object*)parent_object)->object->action){
		goto AttributeError5;
	}

	oldflag = self->posechannel->flag;
	self->posechannel->flag = 0;
	//set the flags for this posechannel
	if (constants){
		if(PySequence_Check(constants)){
			length = PySequence_Length(constants);
			for (x = 0; x < length; x++){
				item = PySequence_GetItem(constants, x);
				if (item == EXPP_GetModuleConstant("Blender.Object.Pose", "ROT")){
					numeric_value |= POSE_ROT;
				}else if (item == EXPP_GetModuleConstant("Blender.Object.Pose", "LOC")){
					numeric_value |= POSE_LOC;
				}else if (item == EXPP_GetModuleConstant("Blender.Object.Pose", "SIZE")){
					numeric_value |= POSE_SIZE;
				}else{
					Py_DECREF(item);
					self->posechannel->flag = (short)oldflag;
					goto AttributeError4;
				}
				Py_DECREF(item);
			}
			self->posechannel->flag = (short)numeric_value;
		}else if (BPy_Constant_Check(constants)){
			if (constants == EXPP_GetModuleConstant("Blender.Object.Pose", "ROT")){
				numeric_value |= POSE_ROT;
			}else if (constants == EXPP_GetModuleConstant("Blender.Object.Pose", "LOC")){
				numeric_value |= POSE_LOC;
			}else if (constants == EXPP_GetModuleConstant("Blender.Object.Pose", "SIZE")){
				numeric_value |= POSE_SIZE;
			}else{
				self->posechannel->flag = (short)oldflag;
				goto AttributeError4;
			}
			self->posechannel->flag = (short)numeric_value;
		}else{
			goto AttributeError3;
		}
	}else{ //nothing passed so set them all
		self->posechannel->flag |= POSE_ROT;
		self->posechannel->flag |= POSE_LOC;
		self->posechannel->flag |= POSE_SIZE;
	}

	//set the frame we want insertion on
	oldframe = G.scene->r.cfra;
	G.scene->r.cfra = (short)frame;

	//add the action channel if it's not there
	verify_action_channel(((BPy_Object*)parent_object)->object->action, 
		self->posechannel->name);

	//insert the pose keys
	if (self->posechannel->flag & POSE_ROT){
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_QUAT_X);
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_QUAT_Y);
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_QUAT_Z);
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_QUAT_W);
	}
	if (self->posechannel->flag & POSE_LOC){
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_LOC_X);
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_LOC_Y);
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_LOC_Z);
	}
	if (self->posechannel->flag & POSE_SIZE){
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_SIZE_X);
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_SIZE_Y);
		insertkey(&((BPy_Object*)parent_object)->object->id, 
			ID_PO, self->posechannel->name, NULL, AC_SIZE_Z);
	}

	//flip the frame back
	G.scene->r.cfra = (short)oldframe;

	//update the IPOs
	remake_action_ipos (((BPy_Object*)parent_object)->object->action);

	return EXPP_incr_ret(Py_None);

AttributeError:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".insertKey: ", "expects an Object, int, (optional) constants");
AttributeError2:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".insertKey: ", "wrong object detected. \
		Use the object this pose came from");
AttributeError3:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".insertKey: ", "Expects a constant or list of constants");
AttributeError4:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".insertKey: ", "Please use a constant defined in the Pose module");
AttributeError5:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".insertKey: ", "You must set up and link an Action to this object first");
}
//------------------------tp_methods
//This contains a list of all methods the object contains
static PyMethodDef BPy_PoseBone_methods[] = {
	{"insertKey", (PyCFunction) PoseBone_insertKey, METH_VARARGS, 
		"() - insert a key for this pose into an action"},
	{NULL, NULL, 0, NULL}
};
//------------------ATTRIBUTE IMPLEMENTATIONS---------------------------
//------------------------PoseBone.name (getter)
//Gets the name attribute
static PyObject *PoseBone_getName(BPy_PoseBone *self, void *closure)
{
    return PyString_FromString(self->posechannel->name);
}
//------------------------PoseBone.name (setter)
//Sets the name attribute
static int PoseBone_setName(BPy_PoseBone *self, PyObject *value, void *closure)
{
	char *name = "";

	if (!PyArg_Parse(value, "s", &name))
		goto AttributeError;

	BLI_strncpy(self->posechannel->name, name, 32);
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".name: ", "expects a string");
}
//------------------------PoseBone.loc (getter)
//Gets the loc attribute
static PyObject *PoseBone_getLoc(BPy_PoseBone *self, void *closure)
{
    return newVectorObject(self->posechannel->loc, 3, Py_WRAP);
}
//------------------------PoseBone.loc (setter)
//Sets the loc attribute
static int PoseBone_setLoc(BPy_PoseBone *self, PyObject *value, void *closure)
{
	VectorObject *vec = NULL;
	int x;

	if (!PyArg_Parse(value, "O!", &vector_Type, &vec))
		goto AttributeError;
	if (vec->size != 3)
		goto AttributeError;

	for (x = 0; x < 3; x++){
		self->posechannel->loc[x] = vec->vec[x];
	}
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".loc: ", "expects a 3d vector object");
}
//------------------------PoseBone.size (getter)
//Gets the size attribute
static PyObject *PoseBone_getSize(BPy_PoseBone *self, void *closure)
{
    return newVectorObject(self->posechannel->size, 3, Py_WRAP);
}
//------------------------PoseBone.size (setter)
//Sets the size attribute
static int PoseBone_setSize(BPy_PoseBone *self, PyObject *value, void *closure)
{
	VectorObject *vec = NULL;
	int x;

	if (!PyArg_Parse(value, "O!", &vector_Type, &vec))
		goto AttributeError;
	if (vec->size != 3)
		goto AttributeError;

	for (x = 0; x < 3; x++){
		self->posechannel->size[x] = vec->vec[x];
	}
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".size: ", "expects a 3d vector object");
}
//------------------------PoseBone.quat (getter)
//Gets the quat attribute
static PyObject *PoseBone_getQuat(BPy_PoseBone *self, void *closure)
{
    return newQuaternionObject(self->posechannel->quat, Py_WRAP);
}
//------------------------PoseBone.quat (setter)
//Sets the quat attribute
static int PoseBone_setQuat(BPy_PoseBone *self, PyObject *value, void *closure)
{
	QuaternionObject *quat = NULL;
	int x;

	if (!PyArg_Parse(value, "O!", &quaternion_Type, &quat))
		goto AttributeError;

	for (x = 0; x < 4; x++){
		self->posechannel->quat[x] = quat->quat[x];
	}
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".quat: ", "expects a quaternion object");
}
//------------------------PoseBone.localMatrix (getter)
//Gets the chan_mat
static PyObject *PoseBone_getLocalMatrix(BPy_PoseBone *self, void *closure)
{
    return newMatrixObject((float*)self->posechannel->chan_mat, 4, 4, Py_WRAP);
}
//------------------------PoseBone.localMatrix (setter)
//Sets the chan_mat 
static int PoseBone_setLocalMatrix(BPy_PoseBone *self, PyObject *value, void *closure)
{
	MatrixObject *matrix = NULL;
	float size[3], quat[4], loc[3];
	float mat3[3][3], mat4[4][4];
	int matsize = 0;

	if (!PyArg_Parse(value, "O!", &matrix_Type, &matrix))
		goto AttributeError;

	if (matrix->rowSize == 3 && matrix->colSize == 3){
		matsize = 3;
		Mat3CpyMat3(mat3, (float(*)[3])*matrix->matrix);
	}else if (matrix->rowSize == 4 && matrix->colSize == 4){
		matsize = 4;
		Mat4CpyMat4(mat4, (float(*)[4])*matrix->matrix);
	}

	if (matsize != 3 && matsize != 4){
		goto AttributeError;
	}

	//get size and rotation
	if (matsize == 3){
		Mat3ToSize(mat3, size);
		Mat3Ortho(mat3);
		Mat3ToQuat(mat3, quat);
	}else if (matsize == 4){
		Mat4ToSize(mat4, size);
		Mat4Ortho(mat4);
		Mat4ToQuat(mat4, quat);
	}

	//get loc
	if (matsize == 4){
		VECCOPY(loc, matrix->matrix[3]);
	}

	//copy new attributes
	VECCOPY(self->posechannel->size, size);
	QUATCOPY(self->posechannel->quat, quat);
	if (matsize == 4){
		VECCOPY(self->posechannel->loc, loc);
	}

	//rebuild matrix
	chan_calc_mat(self->posechannel);
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".localMatrix: ", "expects a 3x3 or 4x4 matrix object");
}
//------------------------PoseBone.poseMatrix (getter)
//Gets the pose_mat
static PyObject *PoseBone_getPoseMatrix(BPy_PoseBone *self, void *closure)
{
    return newMatrixObject((float*)self->posechannel->pose_mat, 4, 4, Py_WRAP);
}
//------------------------PoseBone.poseMatrix (setter)
//Sets the pose_mat
static int PoseBone_setPoseMatrix(BPy_PoseBone *self, PyObject *value, void *closure)
{
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".poseMatrix: ", "not able to set this property");
}
////------------------------PoseBone.constraints (getter)
////Gets the constraints list
//static PyObject *PoseBone_getConstraints(BPy_PoseBone *self, void *closure)
//{
//	PyObject *list = NULL, *py_constraint = NULL;
//	bConstraint *constraint = NULL;
//
//	list = PyList_New(0);
//	for (constraint = self->posechannel->constraints.first; constraint; constraint = constraint->next){
//		py_constraint = PyConstraint_FromConstraint(constraint);
//		if (!py_constraint)
//			return NULL;
//		if (PyList_Append(list, py_constraint) == -1){
//			Py_DECREF(py_constraint);
//			goto RuntimeError;
//		}
//		Py_DECREF(py_constraint);
//	}
//	return list;
//
//RuntimeError:
//	return EXPP_objError(PyExc_RuntimeError, "%s%s%s",
//		sPoseBoneError, ".constraints: ", "unable to build constraint list");
//}
////------------------------PoseBone.constraints (setter)
////Sets the constraints list
//static int PoseBone_setConstraints(BPy_PoseBone *self, PyObject *value, void *closure)
//{
//	printf("This is not implemented yet...");
//	return 1;
//}
//------------------------PoseBone.head (getter)
//Gets the pose head position
static PyObject *PoseBone_getHead(BPy_PoseBone *self, void *closure)
{
    return newVectorObject(self->posechannel->pose_head, 3, Py_NEW);
}
//------------------------PoseBone.head (setter)
//Sets the pose head position
static int PoseBone_setHead(BPy_PoseBone *self, PyObject *value, void *closure)
{
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".head: ", "not able to set this property");
}
//------------------------PoseBone.tail (getter)
//Gets the pose tail position
static PyObject *PoseBone_getTail(BPy_PoseBone *self, void *closure)
{
    return newVectorObject(self->posechannel->pose_tail, 3, Py_NEW);
}
//------------------------PoseBone.tail (setter)
//Sets the pose tail position
static int PoseBone_setTail(BPy_PoseBone *self, PyObject *value, void *closure)
{
	return EXPP_intError(PyExc_AttributeError, "%s%s%s",
		sPoseBoneError, ".tail: ", "not able to set this property");
}
//------------------TYPE_OBECT IMPLEMENTATION---------------------------
//------------------------tp_getset
//This contains methods for attributes that require checking
static PyGetSetDef BPy_PoseBone_getset[] = {
	{"name", (getter)PoseBone_getName, (setter)PoseBone_setName, 
		"The pose bone's name", NULL},
	{"loc", (getter)PoseBone_getLoc, (setter)PoseBone_setLoc, 
		"The pose bone's change in location as a vector", NULL},
	{"size", (getter)PoseBone_getSize, (setter)PoseBone_setSize, 
		"The pose bone's change in size as a vector", NULL},
	{"quat", (getter)PoseBone_getQuat, (setter)PoseBone_setQuat, 
		"The pose bone's change in rotation as a quat", NULL},
	{"localMatrix", (getter)PoseBone_getLocalMatrix, (setter)PoseBone_setLocalMatrix, 
		"The pose bone's change matrix built from the quat, loc, and size", NULL},
	{"poseMatrix", (getter)PoseBone_getPoseMatrix, (setter)PoseBone_setPoseMatrix, 
		"The pose bone's matrix", NULL},
	{"head", (getter)PoseBone_getHead, (setter)PoseBone_setHead, 
		"The pose bone's head positon", NULL},
	{"tail", (getter)PoseBone_getTail, (setter)PoseBone_setTail, 
		"The pose bone's tail positon", NULL},
	//{"constraints", (getter)PoseBone_getConstraints, (setter)PoseBone_setConstraints, 
	//	"The list of contraints that pertain to this pose bone", NULL},
	{NULL, NULL, NULL, NULL, NULL}
};
//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
static void PoseBone_dealloc(BPy_PoseBone *self)
{
	PoseBone_Type.tp_free(self);
	return;
}
//------------------------tp_repr
//This is the string representation of the object
static PyObject *PoseBone_repr(BPy_PoseBone *self)
{
	return PyString_FromFormat( "[PoseBone \"%s\"]", self->posechannel->name); 
}
//------------------------tp_doc
//The __doc__ string for this object
static char BPy_PoseBone_doc[] = "This object wraps a Blender PoseBone object.";

//------------------TYPE_OBECT DEFINITION--------------------------
PyTypeObject PoseBone_Type = {
	PyObject_HEAD_INIT(NULL)   //tp_head
	0,										//tp_internal
	"PoseBone",							//tp_name
	sizeof(BPy_PoseBone),			//tp_basicsize
	0,										//tp_itemsize
	(destructor)PoseBone_dealloc,		//tp_dealloc
	0,										//tp_print
	0,										//tp_getattr
	0,										//tp_setattr
	0,										//tp_compare
	(reprfunc)PoseBone_repr,		//tp_repr
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
	BPy_PoseBone_doc,				//tp_doc
	0,										//tp_traverse
	0,										//tp_clear
	0,										//tp_richcompare
	0,										//tp_weaklistoffset
	0,										//tp_iter
	0,										//tp_iternext
	BPy_PoseBone_methods,		//tp_methods
	0,										//tp_members
	BPy_PoseBone_getset,			//tp_getset
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
//-------------------MODULE METHODS IMPLEMENTATION------------------------
//-------------------MODULE METHODS DEFINITION-----------------------------
struct PyMethodDef M_Pose_methods[] = {
	{NULL, NULL, 0, NULL}
};
//-------------------MODULE INITIALIZATION--------------------------------
PyObject *Pose_Init(void)
{
	PyObject *module;

	//Initializes TypeObject.ob_type
	if (PyType_Ready(&Pose_Type) < 0 || PyType_Ready(&PoseBone_Type)  < 0 ||
		PyType_Ready(&PoseBonesDict_Type) < 0) {
		return EXPP_incr_ret(Py_None);
	}

	//Register the module
	module = Py_InitModule3("Blender.Object.Pose", M_Pose_methods, 
		"The Blender Pose module"); 

	//Add TYPEOBJECTS to the module
	PyModule_AddObject(module, "Pose", 
		EXPP_incr_ret((PyObject *)&Pose_Type)); //*steals*
	PyModule_AddObject(module, "PoseBone", 
		EXPP_incr_ret((PyObject *)&PoseBone_Type)); //*steals*

	//Add CONSTANTS to the module
	PyModule_AddObject(module, "ROT", 
		EXPP_incr_ret(PyConstant_NewInt("ROT", POSE_ROT)));
	PyModule_AddObject(module, "LOC", 
		EXPP_incr_ret(PyConstant_NewInt("LOC", POSE_LOC)));
	PyModule_AddObject(module, "SIZE", 
		EXPP_incr_ret(PyConstant_NewInt("SIZE", POSE_SIZE)));

	return module;
}
//------------------VISIBLE PROTOTYPE IMPLEMENTATION-----------------------
//------------------------------PyPose_FromPose (internal)
//Returns a PyPose from a bPose - return PyNone if bPose is NULL
PyObject *PyPose_FromPose(bPose *pose, char *name)
{
	BPy_Pose *py_pose = NULL;

	if (pose){
		py_pose = (BPy_Pose*)Pose_Type.tp_alloc(&Pose_Type, 0);
		if (!py_pose)
			goto RuntimeError;

		py_pose->pose = pose;
		BLI_strncpy(py_pose->name, name, 24);

		//create armature.bones
		py_pose->Bones = (BPy_PoseBonesDict*)PyPoseBonesDict_FromPyPose(py_pose);
		if (!py_pose->Bones)
			goto RuntimeError;

		return (PyObject*)py_pose;
	}else{
		return EXPP_incr_ret(Py_None);
	}

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sPoseError, "PyPose_FromPose: ", "Internal Error Ocurred");
}
//------------------------------PyPoseBone_FromPosechannel (internal)
//Returns a PyPoseBone from a bPoseChannel - return PyNone if bPoseChannel is NULL
PyObject *PyPoseBone_FromPosechannel(bPoseChannel *pchan)
{
	BPy_PoseBone *py_posechannel = NULL;

	if (pchan){
		py_posechannel = (BPy_PoseBone*)PoseBone_Type.tp_alloc(&PoseBone_Type, 0);
		if (!py_posechannel)
			goto RuntimeError;
		py_posechannel->posechannel = pchan;
		return (PyObject*)py_posechannel;
	}else{
		return EXPP_incr_ret(Py_None);
	}

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sPoseBoneError, "PyPoseBone_FromPosechannel: ", "Internal Error Ocurred");
}
