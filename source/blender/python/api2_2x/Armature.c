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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "Armature.h" //This must come first

#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_armature.h"
#include "BKE_library.h"
#include "BKE_depsgraph.h"
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "MEM_guardedalloc.h"
#include "Bone.h"
#include "NLA.h"
#include "gen_utils.h"

#include "DNA_object_types.h" //This must come before BIF_editarmature.h...
#include "BIF_editarmature.h"

//------------------UNDECLARED EXTERNAL PROTOTYPES--------------------
//These are evil 'extern' declarations for functions with no anywhere
extern void free_editArmature(void);
extern void make_boneList(ListBase* list, ListBase *bones, EditBone *parent);
extern void editbones_to_armature (ListBase *list, Object *ob, bArmature *armature);

//------------------------ERROR CODES---------------------------------
//This is here just to make me happy and to have more consistant error strings :)
static const char sBoneDictError[] = "ArmatureType.bones - Error: ";
static const char sBoneDictBadArgs[] = "ArmatureType.bones - Bad Arguments: ";
static const char sArmatureError[] = "ArmatureType - Error: ";
static const char sArmatureBadArgs[] = "ArmatureType - Bad Arguments: ";
static const char sModuleError[] = "Blender.Armature - Error: ";
static const char sModuleBadArgs[] = "Blender.Armature - Bad Arguments: ";

//################## BonesDict_Type (internal) ########################
/*This is an internal psuedo-dictionary type that allows for manipulation
* of bones inside of an armature. It is a subobject of armature.
* i.e. Armature.bones['key']*/
//#####################################################################

//------------------METHOD IMPLEMENTATIONS-----------------------------
//------------------------Armature.bones.items()
//Returns a list of key:value pairs like dict.items()
PyObject* BonesDict_items(BPy_BonesDict *self)
{
	if (self->editmode_flag){
		return PyDict_Items(self->editBoneDict); 
	}else{
		return PyDict_Items(self->dict); 
	}
}
//------------------------Armature.bones.keys()
//Returns a list of keys like dict.keys()
PyObject* BonesDict_keys(BPy_BonesDict *self)
{
	if (self->editmode_flag){
		return PyDict_Keys(self->editBoneDict);
	}else{
		return PyDict_Keys(self->dict);
	}
}
//------------------------Armature.bones.values()
//Returns a list of values like dict.values()
PyObject* BonesDict_values(BPy_BonesDict *self)
{
	if (self->editmode_flag){
		return PyDict_Values(self->editBoneDict);
	}else{
		return PyDict_Values(self->dict);
	}
}
//------------------ATTRIBUTE IMPLEMENTATION---------------------------
//------------------TYPE_OBECT IMPLEMENTATION--------------------------
//------------------------tp_doc
//The __doc__ string for this object
static char BPy_BonesDict_doc[] = "This is an internal subobject of armature\
designed to act as a Py_Bone dictionary.";

//------------------------tp_methods
//This contains a list of all methods the object contains
static PyMethodDef BPy_BonesDict_methods[] = {
	{"items", (PyCFunction) BonesDict_items, METH_NOARGS, 
		"() - Returns the key:value pairs from the dictionary"},
	{"keys", (PyCFunction) BonesDict_keys, METH_NOARGS, 
		"() - Returns the keys the dictionary"},
	{"values", (PyCFunction) BonesDict_values, METH_NOARGS, 
		"() - Returns the values from the dictionary"},
	{NULL}
};

//------------------------tp_new
//This methods creates a new object (note it does not initialize it - only the building)
static PyObject *BonesDict_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	BPy_BonesDict *py_BonesDict = NULL;

	py_BonesDict = (BPy_BonesDict*)type->tp_alloc(type, 0); 
	if (!py_BonesDict)
		goto RuntimeError;

	py_BonesDict->dict = PyDict_New();
	if(!py_BonesDict->dict)
		goto RuntimeError;

	py_BonesDict->editBoneDict = PyDict_New();
	if (py_BonesDict->editBoneDict == NULL)
		goto RuntimeError;

	py_BonesDict->editmode_flag = 0;

	return (PyObject*)py_BonesDict;

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s", 
		sBoneDictError, "Failed to create dictionary!");
}
//------------------------tp_repr
//This is the string representation of the object
static PyObject *BonesDict_repr(BPy_BonesDict *self)
{
	char buffer[128], str[4096];
	PyObject *key, *value;
	int pos = 0;

	BLI_strncpy(str,"",4096);
	sprintf(buffer, "[Bone Dict: {");
	strcat(str,buffer);
	if (self->editmode_flag){
		while (PyDict_Next(self->editBoneDict, &pos, &key, &value)) {
			sprintf(buffer, "%s : %s, ", PyString_AsString(key), 
				PyString_AsString(value->ob_type->tp_repr(value)));
			strcat(str,buffer);
		}
	}else{
		while (PyDict_Next(self->dict, &pos, &key, &value)) {
			sprintf(buffer, "%s : %s, ", PyString_AsString(key), 
				PyString_AsString(value->ob_type->tp_repr(value)));
			strcat(str,buffer);
		}
	}
	sprintf(buffer, "}]\n");
	strcat(str,buffer);
	return PyString_FromString(str);
}

//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
static void BonesDict_dealloc(BPy_BonesDict * self)
{
	Py_DECREF(self->dict);
	Py_DECREF(self->editBoneDict);
	((PyObject*)self)->ob_type->tp_free((PyObject*)self);
	return;
}
//------------------------mp_length
//This gets the size of the dictionary
int BonesDict_len(BPy_BonesDict *self)
{
	if (self->editmode_flag){
		return PyDict_Size(self->editBoneDict);
	}else{
		return PyDict_Size(self->dict);
	}
}
//-----------------------mp_subscript
//This defines getting a bone from the dictionary - x = Bones['key']
PyObject *BonesDict_GetItem(BPy_BonesDict *self, PyObject* key)
{ 
	PyObject *value = NULL;

	if (self->editmode_flag){
		value = PyDict_GetItem(self->editBoneDict, key);
	}else{
		value = PyDict_GetItem(self->dict, key);
	}
	if(value == NULL){
        return EXPP_incr_ret(Py_None);
	}
	return EXPP_incr_ret(value);
}
//-----------------------mp_ass_subscript
//This does dict assignment - Bones['key'] = value
int BonesDict_SetItem(BPy_BonesDict *self, PyObject *key, PyObject *value)
{
	char *key_str = "", *name = "", *misc = "";
	static char *kwlist[] = {"name", "misc", NULL};

	//Get the key name
	if(key && PyString_Check(key)){
		key_str = PyString_AsString(key);
	}else{
		goto AttributeError;
	}

	//Parse the value for assignment
	if(value && PyDict_Check(value)){
		if(!PyArg_ParseTupleAndKeywords(Py_BuildValue("()"), value, "|ss", kwlist, &name, &misc)){
			goto AttributeError;
		}
	}else{
		goto AttributeError;
	}
	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sBoneDictBadArgs,  "Expects (optional) name='string', misc='string'");
}
//------------------TYPE_OBECT DEFINITION--------------------------
//Mapping Protocol
static PyMappingMethods BonesDict_MapMethods = {
	(inquiry) BonesDict_len,					//mp_length
	(binaryfunc)BonesDict_GetItem,		//mp_subscript
	(objobjargproc)BonesDict_SetItem,	//mp_ass_subscript
};
//BonesDict TypeObject
PyTypeObject BonesDict_Type = {
	PyObject_HEAD_INIT(NULL)		//tp_head
	0,												//tp_internal
	"BonesDict",								//tp_name
	sizeof(BPy_BonesDict),				//tp_basicsize
	0,												//tp_itemsize
	(destructor)BonesDict_dealloc,	//tp_dealloc
	0,												//tp_print
	0,												//tp_getattr
	0,												//tp_setattr
	0,												//tp_compare
	(reprfunc) BonesDict_repr,			//tp_repr
	0,												//tp_as_number
	0,												//tp_as_sequence
	&BonesDict_MapMethods,			//tp_as_mapping
	0,												//tp_hash
	0,												//tp_call
	0,												//tp_str
	0,												//tp_getattro
	0,												//tp_setattro
	0,												//tp_as_buffer
	Py_TPFLAGS_DEFAULT,			//tp_flags
	BPy_BonesDict_doc,					//tp_doc
	0,												//tp_traverse
	0,												//tp_clear
	0,												//tp_richcompare
	0,												//tp_weaklistoffset
	0,												//tp_iter
	0,												//tp_iternext
	BPy_BonesDict_methods,			//tp_methods
	0,												//tp_members
	0,												//tp_getset
	0,												//tp_base
	0,												//tp_dict
	0,												//tp_descr_get
	0,												//tp_descr_set
	0,												//tp_dictoffset
	0,												//tp_init
	0,												//tp_alloc
	(newfunc)BonesDict_new,			//tp_new
	0,												//tp_free
	0,												//tp_is_gc
	0,												//tp_bases
	0,												//tp_mro
	0,												//tp_cache
	0,												//tp_subclasses
	0,												//tp_weaklist
	0												//tp_del
};
//-----------------(internal)
static int BonesDict_Init(PyObject *dictionary, ListBase *bones){
	Bone *bone = NULL;
	PyObject *py_bone = NULL;

	for (bone = bones->first; bone; bone = bone->next){
		py_bone = PyBone_FromBone(bone);
		if (py_bone == NULL)
			return -1;

		if(PyDict_SetItem(dictionary, PyString_FromString(bone->name), py_bone) == -1){
			goto RuntimeError;
		}
		if (bone->childbase.first) 
			BonesDict_Init(dictionary, &bone->childbase);
	}
	return 0;

RuntimeError:
	return EXPP_intError(PyExc_RuntimeError, "%s%s", 
		sBoneDictError, "Internal error trying to wrap blender bones!");
}

//######################### Armature_Type #############################
/*This type represents a thin wrapper around bArmature data types
* internal to blender. It contains the psuedo-dictionary BonesDict
* as an assistant in manipulating it's own bone collection*/
//#####################################################################

//------------------METHOD IMPLEMENTATION------------------------------
//This is a help function for Armature_makeEditable
static int PyArmature_InitEditBoneDict(PyObject *dictionary, ListBase *branch)
{
	struct Bone *bone = NULL;
	PyObject *args, *py_editBone = NULL, *py_bone = NULL;

	for (bone = branch->first; bone; bone = bone->next){

		//create a new editbone based on the bone data
		py_bone = PyBone_FromBone(bone); //new
		if (py_bone == NULL)
			goto RuntimeError;

		args = Py_BuildValue("(O)",py_bone); //new

		py_editBone = EditBone_Type.tp_new(&EditBone_Type, args, NULL); //new
		if (py_editBone == NULL) 
			goto RuntimeError;

		//add the new editbone to the dictionary
		if (PyDict_SetItemString(dictionary, bone->name, py_editBone) == -1) 
			goto RuntimeError;

		if(bone->childbase.first){
			PyArmature_InitEditBoneDict(dictionary, &bone->childbase);
		}
	}
	return 0;

RuntimeError:
	return EXPP_intError(PyExc_RuntimeError, "%s%s", 
		sArmatureError, "Internal error trying to construct an edit armature!");

}
//------------------------Armature.makeEditable()
static PyObject *Armature_makeEditable(BPy_Armature *self)
{
	if (PyArmature_InitEditBoneDict(((BPy_BonesDict*)self->Bones)->editBoneDict, 
		&self->armature->bonebase) == -1){
		return NULL;	//error already set
	}
	((BPy_BonesDict*)self->Bones)->editmode_flag = 1;
	return EXPP_incr_ret(Py_None);
}

static void PyArmature_FixRolls(ListBase *branch, PyObject *dictionary)
{
	float premat[3][3],postmat[3][3];
	float difmat[3][3],imat[3][3], delta[3];
	BPy_EditBone *py_editBone = NULL;
	struct Bone *bone = NULL;
	int keyCheck = -1;

	for (bone = branch->first; bone; bone = bone->next){

		where_is_armature_bone(bone, bone->parent);	//set bone_mat, arm_mat, length, etc.

		keyCheck = PySequence_Contains(dictionary, PyString_FromString(bone->name));
		if (keyCheck == 1){

			py_editBone = (BPy_EditBone*)PyDict_GetItem(dictionary, 
				PyString_FromString(bone->name)); //borrowed
			VecSubf (delta, py_editBone->tail, py_editBone->head);
			vec_roll_to_mat3(delta, py_editBone->roll, premat); //pre-matrix
			Mat3CpyMat4(postmat, bone->arm_mat); //post-matrix
			Mat3Inv(imat, premat);
			Mat3MulMat3(difmat, imat, postmat);

			bone->roll = (float)-atan(difmat[2][0]/difmat[2][2]); //YEA!!
			if (difmat[0][0]<0.0){
				bone->roll += (float)M_PI;
			}

			where_is_armature_bone(bone, bone->parent); //gotta do it again...
		}else if (keyCheck == 0){
			//oops we couldn't find it
		}else{
			//error
		}
		PyArmature_FixRolls (&bone->childbase, dictionary);
	}
}
//------------------------(internal)EditBoneDict_CheckForKey
static BPy_EditBone *EditBoneDict_CheckForKey(BPy_BonesDict *dictionary, char *name)
{
	BPy_EditBone *editbone; 
	PyObject *value, *key;
	int pos = 0;

	while (PyDict_Next(dictionary->editBoneDict, &pos, &key, &value)) {
		editbone = (BPy_EditBone *)value;
		if (STREQ(editbone->name, name)){
			Py_INCREF(editbone);
			return editbone;
		}
	}
	return NULL;
}
//------------------------Armature.saveChanges()
static PyObject *Armature_saveChanges(BPy_Armature *self)
{
	float M_boneRest[3][3], M_parentRest[3][3];
	float iM_parentRest[3][3], delta[3];
	BPy_EditBone *parent = NULL, *editbone = NULL;
	struct Bone *bone = NULL;
	struct Object *obj = NULL;
	PyObject *key, *value;
	int pos = 0;

	//empty armature of old bones
	free_bones(self->armature);

	//create a new set based on the editbones
	while (PyDict_Next(((BPy_BonesDict*)self->Bones)->editBoneDict, &pos, &key, &value)) {

		editbone = (BPy_EditBone*)value;
		bone = MEM_callocN (sizeof(Bone), "bone");	
		editbone->temp = bone;	//save temp pointer

		strcpy (bone->name, editbone->name);
		memcpy (bone->head, editbone->head, sizeof(float)*3);
		memcpy (bone->tail, editbone->tail, sizeof(float)*3);
		bone->flag= editbone->flag;
		bone->roll = 0.0f; //is fixed later
		bone->weight = editbone->weight;
		bone->dist = editbone->dist;
		bone->xwidth = editbone->xwidth;
		bone->zwidth = editbone->zwidth;
		bone->ease1= editbone->ease1;
		bone->ease2= editbone->ease2;
		bone->rad_head= editbone->rad_head;
		bone->rad_tail= editbone->rad_tail;
		bone->segments= editbone->segments;
		bone->boneclass = 0;
	}

	pos = 0;
	//place bones in their correct heirarchy
	while (PyDict_Next(((BPy_BonesDict*)self->Bones)->editBoneDict, 
		&pos, &key, &value)) {

		editbone = (BPy_EditBone*)value;
		bone = editbone->temp; //get bone pointer

		if (!STREQ(editbone->parent, "")){
			parent = EditBoneDict_CheckForKey((BPy_BonesDict*)self->Bones, editbone->parent);
			if(parent != NULL){

				//parent found in dictionary
				bone->parent = parent->temp;
				BLI_addtail (&parent->temp->childbase, bone);
				//Parenting calculations 
				VecSubf (delta, parent->tail, parent->head);
				vec_roll_to_mat3(delta, parent->roll, M_parentRest); //M_parentRest = parent matrix
				VecSubf (delta, editbone->tail, editbone->head);
				vec_roll_to_mat3(delta, editbone->roll, M_boneRest);  //M_boneRest = bone matrix
				Mat3Inv(iM_parentRest, M_parentRest); //iM_parentRest = 1/parent matrix
				//get head/tail
				VecSubf (bone->head, editbone->head, parent->tail);
				VecSubf (bone->tail, editbone->tail, parent->tail);
				//put them in parentspace
				Mat3MulVecfl(iM_parentRest, bone->head);
				Mat3MulVecfl(iM_parentRest, bone->tail);

				Py_DECREF(parent);
			}else{
				//was not found - most likely parent was deleted
				parent = NULL;
				BLI_addtail (&self->armature->bonebase, bone);
			}
		}else{
			BLI_addtail (&self->armature->bonebase, bone);
		}
	}
	//fix rolls and generate matrices
	PyArmature_FixRolls(&self->armature->bonebase, 
		((BPy_BonesDict*)self->Bones)->editBoneDict);

	//update linked objects
	for(obj = G.main->object.first; obj; obj = obj->id.next) {
		if(obj->data == self->armature){
			armature_rebuild_pose(obj, self->armature);
		}
	}
	DAG_object_flush_update(G.scene, obj, OB_RECALC_DATA);

	//clear the editbone dictionary and set edit flag
	PyDict_Clear(((BPy_BonesDict*)self->Bones)->editBoneDict);
	((BPy_BonesDict*)self->Bones)->editmode_flag = 0;

	//rebuild py_bones
	PyDict_Clear(((BPy_BonesDict*)self->Bones)->dict);
	if (BonesDict_Init(((BPy_BonesDict*)self->Bones)->dict, 
		&self->armature->bonebase) == -1)
		return NULL; //error string already set

	return EXPP_incr_ret(Py_None);
}
//------------------ATTRIBUTE IMPLEMENTATION---------------------------
//------------------------Armature.delayDeform (getter)
static PyObject *Armature_getDelayDeform(BPy_Armature *self, void *closure)
{
	if (self->armature->flag & ARM_DELAYDEFORM)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}
//------------------------Armature.delayDeform (setter)
static int Armature_setDelayDeform(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyBool_Check(value)){
			if (value == Py_True){
				self->armature->flag |= ARM_DELAYDEFORM;
				return 0;
			}else if (value == Py_False){
				self->armature->flag &= ~ARM_DELAYDEFORM;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects True or False");
}
//------------------------Armature.restPosition (getter)
static PyObject *Armature_getRestPosition(BPy_Armature *self, void *closure)
{
	if (self->armature->flag & ARM_RESTPOS)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}
//------------------------Armature.restPosition (setter)
static int Armature_setRestPosition(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyBool_Check(value)){
			if (value == Py_True){
				self->armature->flag |= ARM_RESTPOS;
				return 0;
			}else if (value == Py_False){
				self->armature->flag &= ~ARM_RESTPOS;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects True or False");
}
//------------------------Armature.envelopes (getter)
static PyObject *Armature_getEnvelopes(BPy_Armature *self, void *closure)
{
	if (self->armature->deformflag & ARM_DEF_ENVELOPE)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}
//------------------------Armature.envelopes (setter)
static int Armature_setEnvelopes(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyBool_Check(value)){
			if (value == Py_True){
				self->armature->deformflag |= ARM_DEF_ENVELOPE;
				return 0;
			}else if (value == Py_False){
				self->armature->deformflag &= ~ARM_DEF_ENVELOPE;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects True or False");
}
//------------------------Armature.vertexGroups (getter)
static PyObject *Armature_getVertexGroups(BPy_Armature *self, void *closure)
{
	if (self->armature->deformflag & ARM_DEF_VGROUP)
		Py_RETURN_TRUE;
	else
		Py_RETURN_FALSE;
}
//------------------------Armature.vertexGroups (setter)
static int Armature_setVertexGroups(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyBool_Check(value)){
			if (value == Py_True){
				self->armature->deformflag |= ARM_DEF_VGROUP;
				return 0;
			}else if (value == Py_False){
				self->armature->deformflag &= ~ARM_DEF_VGROUP;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects True or False");
}
//------------------------Armature.name (getter)
//Gets the name of the armature
static PyObject *Armature_getName(BPy_Armature *self, void *closure)
{
    return PyString_FromString(self->armature->id.name +2); //*new*
}
//------------------------Armature.name (setter)
//Sets the name of the armature
static int Armature_setName(BPy_Armature *self, PyObject *value, void *closure)
{
	char buffer[24];
	char *name = "";

	if(value){
		if(PyString_Check(value)){
			name = PyString_AsString(value);
			PyOS_snprintf(buffer, sizeof(buffer), "%s", name);
			rename_id(&self->armature->id, buffer);
			return 0; 
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects string");
}
//------------------------Armature.bones (getter)
//Gets the name of the armature
static PyObject *Armature_getBoneDict(BPy_Armature *self, void *closure)
{
    return EXPP_incr_ret(self->Bones);
}
//------------------------Armature.bones (setter)
//Sets the name of the armature
/*TODO*/
/*Copy Bones through x = y*/
static int Armature_setBoneDict(BPy_Armature *self, PyObject *value, void *closure)
{
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureError, "You are not allowed to change the .Bones attribute");
}
//------------------TYPE_OBECT IMPLEMENTATION--------------------------
//------------------------tp_doc
//The __doc__ string for this object
static char BPy_Armature_doc[] = "This object wraps a Blender Armature object.";

//------------------------tp_methods
//This contains a list of all methods the object contains
static PyMethodDef BPy_Armature_methods[] = {
	{"makeEditable", (PyCFunction) Armature_makeEditable, METH_NOARGS, 
		"() - Unlocks the ability to modify armature bones"},
	{"saveChanges", (PyCFunction) Armature_saveChanges, METH_NOARGS, 
		"() - Rebuilds the armature based on changes to bones since the last call to makeEditable"},
	{NULL}
};

//------------------------tp_getset
//This contains methods for attributes that require checking
static PyGetSetDef BPy_Armature_getset[] = {
	{"name", (getter)Armature_getName, (setter)Armature_setName, 
		"The armature's name", NULL},
	{"bones", (getter)Armature_getBoneDict, (setter)Armature_setBoneDict, 
		"The armature's Bone dictionary", NULL},
	{"vertexGroups", (getter)Armature_getVertexGroups, (setter)Armature_setVertexGroups, 
		"Enable/Disable vertex group defined deformation", NULL},
	{"envelopes", (getter)Armature_getEnvelopes, (setter)Armature_setEnvelopes, 
		"Enable/Disable bone envelope defined deformation", NULL},
	{"restPosition", (getter)Armature_getRestPosition, (setter)Armature_setRestPosition, 
		"Show armature rest position - disables posing", NULL},
	{"delayDeform", (getter)Armature_getDelayDeform, (setter)Armature_setDelayDeform, 
		"Don't deform children when manipulating bones in pose mode", NULL},
	{NULL}
};
//------------------------tp_new
//This methods creates a new object (note it does not initialize it - only the building)
//This can be called through python by myObject.__new__() however, tp_init is not called
static PyObject *Armature_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	BPy_Armature *py_armature = NULL;
	bArmature *bl_armature;
	int success;

	bl_armature = add_armature();
	if(bl_armature) {
		bl_armature->id.us = 0; // return count to 0 - add_armature() inc'd it 

		py_armature = (BPy_Armature*)type->tp_alloc(type, 0); //*new*
		if (py_armature == NULL)
			goto RuntimeError;

		py_armature->armature = bl_armature;

		py_armature->Bones = BonesDict_new(&BonesDict_Type, NULL, NULL);
		if (py_armature->Bones == NULL)
			goto RuntimeError;

		success = BonesDict_Init(((BPy_BonesDict*)py_armature->Bones)->dict, &bl_armature->bonebase);
		if (success == -1)
			return NULL; //error string already set
	} else {
		goto RuntimeError;
	}
	return (PyObject*)py_armature; 

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sArmatureError, " __new__: ", "couldn't create Armature Data in Blender");
}
//------------------------tp_init
//This methods does initialization of the new object
//This method will get called in python by 'myObject(argument, keyword=value)'
//tp_new will be automatically called before this
static int Armature_init(BPy_Armature *self, PyObject *args, PyObject *kwds)
{
	char buf[21];
	char *name = "myArmature";
	static char *kwlist[] = {"name", NULL};

	if(!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &name)){
		goto AttributeError;
	}

	//rename the armature if a name is supplied
	if(!BLI_streq(name, "myArmature")){
		PyOS_snprintf(buf, sizeof(buf), "%s", name);
		rename_id(&self->armature->id, buf);
	}

	return 0;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s%s", 
		sArmatureBadArgs, " __init__: ", "Expects string(name)");
}
//------------------------tp_richcompare
//This method allows the object to use comparison operators
//TODO: We need some armature comparisons
static PyObject *Armature_richcmpr(BPy_Armature *self, PyObject *v, int op)
{
	return EXPP_incr_ret(Py_None);
}

//------------------------tp_repr
//This is the string representation of the object
static PyObject *Armature_repr(BPy_Armature *self)
{
	return PyString_FromFormat( "[Armature: \"%s\"]", self->armature->id.name + 2 ); //*new*
}

//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
///tp_dealloc
static void Armature_dealloc(BPy_Armature * self)
{
	Py_DECREF(self->Bones);
	((PyObject*)self)->ob_type->tp_free((PyObject*)self);
	return;
}
//------------------TYPE_OBECT DEFINITION--------------------------
PyTypeObject Armature_Type = {
	PyObject_HEAD_INIT(NULL)		//tp_head
	0,								//tp_internal
	"Armature",						//tp_name
	sizeof(BPy_Armature),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)Armature_dealloc,	//tp_dealloc
	0,								//tp_print
	0,								//tp_getattr
	0,								//tp_setattr
	0,								//tp_compare
	(reprfunc) Armature_repr,		//tp_repr
	0,								//tp_as_number
	0,								//tp_as_sequence
	0,								//tp_as_mapping
	0,								//tp_hash
	0,								//tp_call
	0,								//tp_str
	0,								//tp_getattro
	0,								//tp_setattro
	0,								//tp_as_buffer
	Py_TPFLAGS_DEFAULT,				//tp_flags
	BPy_Armature_doc,				//tp_doc
	0,								//tp_traverse
	0,								//tp_clear
	(richcmpfunc)Armature_richcmpr,	//tp_richcompare
	0,								//tp_weaklistoffset
	0,								//tp_iter
	0,								//tp_iternext
	BPy_Armature_methods,			//tp_methods
	0,								//tp_members
	BPy_Armature_getset,			//tp_getset
	0,								//tp_base
	0,								//tp_dict
	0,								//tp_descr_get
	0,								//tp_descr_set
	0,								//tp_dictoffset
	(initproc)Armature_init,		//tp_init
	0,								//tp_alloc
	(newfunc)Armature_new,			//tp_new
	0,								//tp_free
	0,								//tp_is_gc
	0,								//tp_bases
	0,								//tp_mro
	0,								//tp_cache
	0,								//tp_subclasses
	0,								//tp_weaklist
	0								//tp_del
};

//-------------------MODULE METHODS IMPLEMENTATION------------------------
//----------------Blender.Armature.Get()
/* This function will return a Py_Armature when a single string is passed
* or else it will return a {key:value} dictionary when mutliple strings are passed
* or it will return a {key:value} dictionary of all armatures when nothing is passed*/
static PyObject *M_Armature_Get(PyObject * self, PyObject * args)
{
	PyObject *seq = NULL, *item = NULL, *dict = NULL, *py_armature = NULL;
	char *name = "", buffer[24];
	int size = 0, i;
	void *data;

	//GET ARGUMENTS - () ('s') ('s',..) (['s',..]) are exceptable
	size = PySequence_Length(args);
	if (size == 1) {
		seq = PySequence_GetItem(args, 0); //*new*
		if (seq == NULL)
			goto RuntimeError;
		if(!PyString_Check(seq)){
			if (PySequence_Check(seq)) {
				size = PySequence_Length(seq);
			} else {
				Py_DECREF(seq);
				goto AttributeError;
			}
		}
	} else {
		seq = EXPP_incr_ret(args); //*take ownership*
	}
	//'seq' should be a list, empty tuple or string - check list for strings
	if(!PyString_Check(seq)){
		for(i = 0; i < size; i++){
			item = PySequence_GetItem(seq, i); //*new*
			if (item == NULL) {
				Py_DECREF(seq);
				goto RuntimeError;
			}
			if(!PyString_Check(item)){
				EXPP_decr2(item, seq);
				goto AttributeError;
			}
			Py_DECREF(item);
		}
	}

	//GET ARMATURES
	if(size != 1){
		dict = PyDict_New(); //*new*
		if(dict == NULL){
			Py_DECREF(seq);
			goto RuntimeError;
		}
		if(size == 0){	//GET ALL ARMATURES
			data = &(G.main->armature).first; //get the first data ID from the armature library
			while (data){
				py_armature = PyArmature_FromArmature(data); //*new*
				sprintf(buffer, "%s", ((bArmature*)data)->id.name +2);
				if(PyDict_SetItemString(dict, buffer, py_armature) == -1){ //add to dictionary
					EXPP_decr3(seq, dict, py_armature);
					goto RuntimeError;
				}
				data = ((ID*)data)->next;
			}
			Py_DECREF(seq);
		}else{	//GET ARMATURE LIST
			for (i = 0; i < size; i++) {
				item = PySequence_GetItem(seq, i); //*new*
				name = PyString_AsString(item);
				Py_DECREF(item);
				data = find_id("AR", name); //get data from library
				if (data != NULL){
					py_armature = PyArmature_FromArmature(data); //*new*
					if(PyDict_SetItemString(dict, name, py_armature) == -1){ //add to dictionary
						EXPP_decr3(seq, dict, py_armature);
						goto RuntimeError;
					}
				}else{
					if(PyDict_SetItemString(dict, name, Py_None) == -1){ //add to dictionary
						EXPP_decr2(seq, dict);
						goto RuntimeError;
					}
				}
			}
			Py_DECREF(seq);
		}
		return dict; //transfering ownership to caller
	}else{	//GET SINGLE ARMATURE
		if(!PyString_Check(seq)){ //This handles the bizarre case where (['s']) is passed
			item = PySequence_GetItem(seq, 0); //*new*
			name = PyString_AsString(item);
			Py_DECREF(item);
		}else{
			name = PyString_AsString(seq);
		}
		Py_DECREF(seq);
		data = find_id("AR", name); //get data from library
		if (data != NULL){
			return PyArmature_FromArmature(data); //*new*
		}else{
			return EXPP_incr_ret(Py_None);
		}
	}

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sModuleError, "Get(): ", "Internal Error Ocurred");

AttributeError:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s", 
		sModuleBadArgs, "Get(): ", "- Expects (optional) string sequence");
}

//-------------------MODULE METHODS DEFINITION-----------------------------
static PyObject *M_Armature_Get( PyObject * self, PyObject * args );

static char M_Armature_Get_doc[] = "(name) - return the armature with the name 'name', \
  returns None if not found.\n If 'name' is not specified, it returns a list of all \
  armatures in the\ncurrent scene.";

struct PyMethodDef M_Armature_methods[] = {
	{"Get", M_Armature_Get, METH_VARARGS, M_Armature_Get_doc},
	{NULL}
};
//------------------VISIBLE PROTOTYPE IMPLEMENTATION-----------------------
//-----------------(internal)
//Converts a bArmature to a PyArmature
PyObject *PyArmature_FromArmature(struct bArmature *armature)
{
	BPy_Armature *py_armature = NULL;
	int success;

	py_armature = (BPy_Armature*)Armature_Type.tp_alloc(&Armature_Type, 0); //*new*
	if (py_armature == NULL)
		goto RuntimeError;

	py_armature->armature = armature;

	py_armature->Bones = BonesDict_new(&BonesDict_Type, NULL, NULL); //*new*
	if (py_armature->Bones == NULL)
		goto RuntimeError;

	success = BonesDict_Init(((BPy_BonesDict*)py_armature->Bones)->dict, &armature->bonebase);
	if (success == -1)
		return NULL; //error string already set

	return (PyObject *) py_armature; 

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sModuleError, "PyArmature_FromArmature: ", "Internal Error Ocurred");
}
//-----------------(internal)
//Converts a PyArmature to a bArmature
struct bArmature *PyArmature_AsArmature(BPy_Armature *py_armature)
{
	return (py_armature->armature);
}
//-------------------MODULE INITIALIZATION--------------------------------
PyObject *Armature_Init(void)
{
	PyObject *module, *dict;

	//Initializes TypeObject.ob_type
	if (PyType_Ready(&Armature_Type) < 0 ||	PyType_Ready(&BonesDict_Type) < 0 || 
		PyType_Ready(&EditBone_Type) < 0 ||	PyType_Ready(&Bone_Type) < 0){
		return EXPP_incr_ret(Py_None);
	}

	//Register the module
	module = Py_InitModule3("Blender.Armature", M_Armature_methods, 
		"The Blender Armature module"); 

	//Add TYPEOBJECTS to the module
	PyModule_AddObject(module, "ArmatureType", 
		EXPP_incr_ret((PyObject *)&Armature_Type)); //*steals*
	PyModule_AddObject(module, "BoneType", 
		EXPP_incr_ret((PyObject *)&Bone_Type)); //*steals*

	//Add CONSTANTS to the module
	PyModule_AddObject(module, "CONNECTED", 
		EXPP_incr_ret(PyConstant_NewInt("CONNECTED", BONE_CONNECTED)));
	PyModule_AddObject(module, "HINGE", 
		EXPP_incr_ret(PyConstant_NewInt("HINGE", BONE_HINGE)));
	PyModule_AddObject(module, "NO_DEFORM", 
		EXPP_incr_ret(PyConstant_NewInt("NO_DEFORM", BONE_NO_DEFORM)));
	PyModule_AddObject(module, "MULTIPLY", 
		EXPP_incr_ret(PyConstant_NewInt("MULTIPLY", BONE_MULT_VG_ENV)));
	PyModule_AddObject(module, "HIDDEN_EDIT", 
		EXPP_incr_ret(PyConstant_NewInt("HIDDEN_EDIT", BONE_HIDDEN_A)));

	PyModule_AddObject(module, "BONESPACE", 
		EXPP_incr_ret(PyConstant_NewString("BONESPACE", "bone_space")));
	PyModule_AddObject(module, "ARMATURESPACE", 
		EXPP_incr_ret(PyConstant_NewString("ARMATURESPACE", "armature_space")));
	PyModule_AddObject(module, "WORLDSPACE", 
		EXPP_incr_ret(PyConstant_NewString("WORLDSPACE", "world_space")));

	//Add SUBMODULES to the module
	dict = PyModule_GetDict( module ); //borrowed
	PyDict_SetItemString(dict, "NLA", NLA_Init()); //creates a *new* module

	return module;
}
