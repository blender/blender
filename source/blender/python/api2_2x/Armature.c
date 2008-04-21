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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include <stddef.h>

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
#include "gen_library.h"

#include "DNA_object_types.h" //This must come before BIF_editarmature.h...
#include "BIF_editarmature.h"

//------------------EXTERNAL PROTOTYPES--------------------
extern void make_boneList(ListBase* list, ListBase *bones, EditBone *parent);
extern void editbones_to_armature (ListBase *list, Object *ob);

//------------------------ERROR CODES---------------------------------
//This is here just to make me happy and to have more consistant error strings :)
static const char sBoneDictError[] = "ArmatureType.bones - Error: ";
static const char sBoneDictBadArgs[] = "ArmatureType.bones - Bad Arguments: ";
static const char sArmatureError[] = "ArmatureType - Error: ";
static const char sArmatureBadArgs[] = "ArmatureType - Bad Arguments: ";
static const char sModuleError[] = "Blender.Armature - Error: ";
static const char sModuleBadArgs[] = "Blender.Armature - Bad Arguments: ";

PyObject * arm_weakref_callback_weakref_dealloc(PyObject *self, PyObject *weakref);
/* python callable */
PyObject * arm_weakref_callback_weakref_dealloc__pyfunc;

//################## BonesDict_Type (internal) ########################
/*This is an internal psuedo-dictionary type that allows for manipulation
* of bones inside of an armature. It is a subobject of armature.
* i.e. Armature.bones['key']*/
//#####################################################################

//------------------METHOD IMPLEMENTATIONS-----------------------------
//------------------------Armature.bones.items()
//Returns a list of key:value pairs like dict.items()
static PyObject* BonesDict_items(BPy_BonesDict *self)
{
	if (self->editmode_flag){
		return PyDict_Items(self->editbonesMap); 
	}else{
		return PyDict_Items(self->bonesMap); 
	}
}
//------------------------Armature.bones.keys()
//Returns a list of keys like dict.keys()
static PyObject* BonesDict_keys(BPy_BonesDict *self)
{
	if (self->editmode_flag){
		return PyDict_Keys(self->editbonesMap);
	}else{
		return PyDict_Keys(self->bonesMap);
	}
}
//------------------------Armature.bones.values()
//Returns a list of values like dict.values()
static PyObject* BonesDict_values(BPy_BonesDict *self)
{
	if (self->editmode_flag){
		return PyDict_Values(self->editbonesMap);
	}else{
		return PyDict_Values(self->bonesMap);
	}
}
//------------------ATTRIBUTE IMPLEMENTATION---------------------------
//------------------TYPE_OBECT IMPLEMENTATION-----------------------
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
	{NULL, NULL, 0, NULL}
};
//-----------------(internal)
static int BoneMapping_Init(PyObject *dictionary, ListBase *bones){
	Bone *bone = NULL;
	PyObject *py_bone = NULL;

	for (bone = bones->first; bone; bone = bone->next){
		py_bone = PyBone_FromBone(bone);
		if (!py_bone)
			return -1;
		
		if(PyDict_SetItemString(dictionary, bone->name, py_bone) == -1) {
			/* unlikely but possible */
			Py_DECREF(py_bone);
			return -1;
		}
		
		Py_DECREF(py_bone);
		if (bone->childbase.first) 
			BoneMapping_Init(dictionary, &bone->childbase);
	}
	return 0;
}
//-----------------(internal)
static int EditBoneMapping_Init(PyObject *dictionary, ListBase *editbones){
	EditBone *editbone = NULL;
	PyObject *py_editbone = NULL;

	for (editbone = editbones->first; editbone; editbone = editbone->next){
		py_editbone = PyEditBone_FromEditBone(editbone);
		if (!py_editbone)
			return -1;
		
		if(PyDict_SetItemString(dictionary, editbone->name, py_editbone) == -1) {
			Py_DECREF(py_editbone);
			return -1;
		}
		Py_DECREF(py_editbone);
	}
	return 0;
}
//----------------- BonesDict_InitBones
static int BonesDict_InitBones(BPy_BonesDict *self)
{
	PyDict_Clear(self->bonesMap);
	if (BoneMapping_Init(self->bonesMap, self->bones) == -1)
		return 0;
	return 1;
} 
//----------------- BonesDict_InitEditBones
static int BonesDict_InitEditBones(BPy_BonesDict *self)
{
	PyDict_Clear(self->editbonesMap);
	if (EditBoneMapping_Init(self->editbonesMap, &self->editbones) == -1)
		return 0;
	return 1;
}
//------------------------tp_repr
//This is the string representation of the object
static PyObject *BonesDict_repr(BPy_BonesDict *self)
{
	char str[2048];
	PyObject *key, *value;
	Py_ssize_t pos = 0;
	char *p = str;
	char *keys, *vals;

	p += sprintf(str, "[Bone Dict: {");

	if (self->editmode_flag){
		while (PyDict_Next(self->editbonesMap, &pos, &key, &value)) {
			keys = PyString_AsString(key);
			vals = PyString_AsString(value->ob_type->tp_repr(value));
			if( strlen(str) + strlen(keys) + strlen(vals) < sizeof(str)-20 )
				p += sprintf(p, "%s : %s, ", keys, vals );
			else {
				p += sprintf(p, "...." );
				break;
			}
		}
	}else{
		while (PyDict_Next(self->bonesMap, &pos, &key, &value)) {
			keys = PyString_AsString(key);
			vals = PyString_AsString(value->ob_type->tp_repr(value));
			if( strlen(str) + strlen(keys) + strlen(vals) < sizeof(str)-20 )
				p += sprintf(p, "%s : %s, ", keys, vals );
			else {
				p += sprintf(p, "...." );
				break;
			}
		}
	}
	sprintf(p, "}]");
	return PyString_FromString(str);
}

//------------------------tp_dealloc
//This tells how to 'tear-down' our object when ref count hits 0
static void BonesDict_dealloc(BPy_BonesDict * self)
{
	Py_DECREF(self->bonesMap);
	Py_DECREF(self->editbonesMap);
	BLI_freelistN(&self->editbones); 
	PyObject_DEL( self );
	return;
}
//------------------------mp_length
//This gets the size of the dictionary
static int BonesDict_len(BPy_BonesDict *self)
{
	if (self->editmode_flag){
		return BLI_countlist(&self->editbones);
	}else{
		return BLI_countlist(self->bones);
	}
}
//-----------------------mp_subscript
//This defines getting a bone from the dictionary - x = Bones['key']
static PyObject *BonesDict_GetItem(BPy_BonesDict *self, PyObject* key)
{ 
	PyObject *value = NULL;

	if (self->editmode_flag){
		value = PyDict_GetItem(self->editbonesMap, key);
	}else{
		value = PyDict_GetItem(self->bonesMap, key);
	}
	if(value == NULL){  /* item not found in dict. throw exception */
		char* key_str = PyString_AsString( key );
		if (key_str) {
			return EXPP_ReturnPyObjError(PyExc_KeyError, "bone key must be a string" );
		} else {
			char buffer[128];
			PyOS_snprintf( buffer, sizeof(buffer), "bone %s not found", key_str);
			return EXPP_ReturnPyObjError(PyExc_KeyError, buffer );
		}
	}
	return EXPP_incr_ret(value);
}
//-----------------------mp_ass_subscript
//This does dict assignment - Bones['key'] = value
static int BonesDict_SetItem(BPy_BonesDict *self, PyObject *key, PyObject *value)
{
	BPy_EditBone *editbone_for_deletion;
	struct EditBone *editbone = NULL;
	char *key_str = PyString_AsString(key);

	if (!self->editmode_flag)
		return EXPP_intError(PyExc_AttributeError, "%s%s", 
				sBoneDictBadArgs,  "You must call makeEditable() first");
	
	if (!key_str)
		return EXPP_intError(PyExc_AttributeError, "%s%s", 
				sBoneDictBadArgs,  "The key must be the name of an editbone");
	
	if (value && !EditBoneObject_Check(value))
		return EXPP_intError(PyExc_AttributeError, "%s%s",
				sBoneDictBadArgs,  "Can only assign editbones as values");
	
	//parse value for assignment
	if (value){ /* we know this must be an editbone from the above check */
		//create a new editbone
		editbone = MEM_callocN(sizeof(EditBone), "eBone");
		BLI_strncpy(editbone->name, key_str, 32);
		unique_editbone_name(NULL, editbone->name);
		editbone->dist = ((BPy_EditBone*)value)->dist;
		editbone->ease1 = ((BPy_EditBone*)value)->ease1;
		editbone->ease2 = ((BPy_EditBone*)value)->ease2;
		editbone->flag = ((BPy_EditBone*)value)->flag;
		editbone->parent = ((BPy_EditBone*)value)->parent;
		editbone->rad_head = ((BPy_EditBone*)value)->rad_head;
		editbone->rad_tail = ((BPy_EditBone*)value)->rad_tail;
		editbone->roll = ((BPy_EditBone*)value)->roll;
		editbone->segments = ((BPy_EditBone*)value)->segments;
		editbone->weight = ((BPy_EditBone*)value)->weight;
		editbone->xwidth = ((BPy_EditBone*)value)->xwidth;
		editbone->zwidth = ((BPy_EditBone*)value)->zwidth;
		VECCOPY(editbone->head, ((BPy_EditBone*)value)->head);
		VECCOPY(editbone->tail, ((BPy_EditBone*)value)->tail);
		editbone->layer= ((BPy_EditBone*)value)->layer;
		
		//set object pointer
		((BPy_EditBone*)value)->editbone = editbone;

		//fix the bone's head position if flags indicate that it is 'connected'
		if (editbone->flag & BONE_CONNECTED){
			if(!editbone->parent){
				((BPy_EditBone*)value)->editbone = NULL;
				MEM_freeN(editbone);
					return EXPP_intError(PyExc_AttributeError, "%s%s", 
							sBoneDictBadArgs,  "The 'connected' flag is set but the bone has no parent!");
			}else{
				VECCOPY(editbone->head, editbone->parent->tail);
			}
		}

		//set in editbonelist
		BLI_addtail(&self->editbones, editbone);

		//set the new editbone in the mapping
		if(PyDict_SetItemString(self->editbonesMap, key_str, value) == -1){
			((BPy_EditBone*)value)->editbone = NULL;
			BLI_freelinkN(&self->editbones, editbone);
			return EXPP_intError(PyExc_RuntimeError, "%s%s", 
					sBoneDictError,  "Unable to access dictionary!");
		}
	}else {
		//they are trying to delete the bone using 'del'
		editbone_for_deletion = (BPy_EditBone*)PyDict_GetItem(self->editbonesMap, key);

		if (!editbone_for_deletion)
			return EXPP_intError(PyExc_KeyError, "%s%s%s%s", 
					sBoneDictError,  "The key: ", key_str, " is not present in this dictionary!");

		/*first kill the datastruct then remove the item from the dict
		and wait for GC to pick it up.
		We have to delete the datastruct here because the tp_dealloc
		doesn't handle it*/
		
		/*this is ugly but you have to set the parent to NULL for else 
		editbones_to_armature will crash looking for this bone*/
		for (editbone = self->editbones.first; editbone; editbone = editbone->next){
			if (editbone->parent == editbone_for_deletion->editbone) {
				editbone->parent = NULL;
				 /* remove the connected flag or else the 'root' ball
				  * doesn't get drawn */
				editbone->flag &= ~BONE_CONNECTED;
			}
		}
		BLI_freelinkN(&self->editbones, editbone_for_deletion->editbone);
		if(PyDict_DelItem(self->editbonesMap, key) == -1)
			return EXPP_intError(PyExc_RuntimeError, "%s%s", 
					sBoneDictError,  "Unable to access dictionary!");
	}
	return 0;
}
//------------------TYPE_OBJECT DEFINITION--------------------------
//Mapping Protocol
static PyMappingMethods BonesDict_MapMethods = {
	(inquiry) BonesDict_len,					//mp_length
	(binaryfunc)BonesDict_GetItem,		//mp_subscript
	(objobjargproc)BonesDict_SetItem,	//mp_ass_subscript
};
//BonesDict TypeObject
PyTypeObject BonesDict_Type = {
	PyObject_HEAD_INIT(NULL)			//tp_head
	0,												//tp_internal
	"BonesDict",								//tp_name
	sizeof(BPy_BonesDict),					//tp_basicsize
	0,												//tp_itemsize
	(destructor)BonesDict_dealloc,		//tp_dealloc
	0,												//tp_print
	0,												//tp_getattr
	0,												//tp_setattr
	0,												//tp_compare
	(reprfunc) BonesDict_repr,				//tp_repr
	0,												//tp_as_number
	0,												//tp_as_sequence
	&BonesDict_MapMethods,				//tp_as_mapping
	0,												//tp_hash
	0,												//tp_call
	0,												//tp_str
	0,												//tp_getattro
	0,												//tp_setattro
	0,												//tp_as_buffer
	Py_TPFLAGS_DEFAULT,					//tp_flags
	BPy_BonesDict_doc,						//tp_doc
	0,												//tp_traverse
	0,												//tp_clear
	0,												//tp_richcompare
	0,												//tp_weaklistoffset
	0,												//tp_iter
	0,												//tp_iternext
	BPy_BonesDict_methods,				//tp_methods
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
//-----------------------PyBonesDict_FromPyArmature
static PyObject *PyBonesDict_FromPyArmature(BPy_Armature *py_armature)
{
	BPy_BonesDict *py_BonesDict = (BPy_BonesDict *)PyObject_NEW( BPy_BonesDict, &BonesDict_Type );
	if (!py_BonesDict)
		goto RuntimeError;

	py_BonesDict->bones = NULL;
	py_BonesDict->editbones.first = py_BonesDict->editbones.last = NULL;

	//create internal dictionaries
	py_BonesDict->bonesMap = PyDict_New();
	py_BonesDict->editbonesMap = PyDict_New();
	if (!py_BonesDict->bonesMap || !py_BonesDict->editbonesMap)
		goto RuntimeError;

	//set listbase pointer
	py_BonesDict->bones = &py_armature->armature->bonebase;

	//now that everything is setup - init the mappings
	if (!BonesDict_InitBones(py_BonesDict))
		goto RuntimeError;
	if (!BonesDict_InitEditBones(py_BonesDict))
		goto RuntimeError;

	//set editmode flag
	py_BonesDict->editmode_flag = 0; 

	return (PyObject*)py_BonesDict;

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s", 
		sBoneDictError, "Failed to create class");
}

//######################### Armature_Type #############################
/*This type represents a thin wrapper around bArmature data types
* internal to blender. It contains the psuedo-dictionary BonesDict
* as an assistant in manipulating it's own bone collection*/
//#################################################################

//------------------METHOD IMPLEMENTATION------------------------------
//------------------------Armature.makeEditable()
static PyObject *Armature_makeEditable(BPy_Armature *self)
{
	if (self->armature->flag & ARM_EDITMODE)
		goto AttributeError;

	make_boneList(&self->Bones->editbones, self->Bones->bones, NULL);
	if (!BonesDict_InitEditBones(self->Bones))
		return NULL;
	self->Bones->editmode_flag = 1;
	return EXPP_incr_ret(Py_None);

AttributeError:
	return EXPP_objError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "The armature cannot be placed manually in editmode before you call makeEditable()!");
}

//------------------------Armature.update()
//This is a bit ugly because you need an object link to do this
static PyObject *Armature_update(BPy_Armature *self)
{
	Object *obj = NULL;

	for (obj = G.main->object.first; obj; obj = obj->id.next){
		if (obj->data == self->armature)
			break;
	}
	if (obj){
		editbones_to_armature (&self->Bones->editbones, obj);
		if (!BonesDict_InitBones(self->Bones))
			return NULL;
		self->Bones->editmode_flag = 0;
		BLI_freelistN(&self->Bones->editbones);
	}else{
		goto AttributeError;

	}
	return EXPP_incr_ret(Py_None);

AttributeError:
	return EXPP_objError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "The armature must be linked to an object before you can save changes!");
}

//------------------------Armature.__copy__()
static PyObject *Armature_copy(BPy_Armature *self)
{
	PyObject *py_armature = NULL;
	bArmature *bl_armature;
	bl_armature= copy_armature(self->armature);
	bl_armature->id.us= 0;
	py_armature= Armature_CreatePyObject( bl_armature );
	return py_armature;
}

//------------------ATTRIBUTE IMPLEMENTATION---------------------------
//------------------------Armature.autoIK (getter)
static PyObject *Armature_getAutoIK(BPy_Armature *self, void *closure)
{
	if (self->armature->flag & ARM_AUTO_IK)
		return EXPP_incr_ret(Py_True);
	else
		return EXPP_incr_ret(Py_False);
}
//------------------------Armature.autoIK (setter)
static int Armature_setAutoIK(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyBool_Check(value)){
			if (value == Py_True){
				self->armature->flag |= ARM_AUTO_IK;
				return 0;
			}else if (value == Py_False){
				self->armature->flag &= ~ARM_AUTO_IK;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects True or False");
}
//------------------------Armature.layers (getter)
static PyObject *Armature_getLayers(BPy_Armature *self, void *closure)
{
	int layers, bit = 0, val = 0;
	PyObject *item = NULL, *laylist = PyList_New( 0 );

	if( !laylist )
		return EXPP_ReturnPyObjError( PyExc_MemoryError,
			"couldn't create pylist!" );

	layers = self->armature->layer;

	while( bit < 20 ) {
		val = 1 << bit;
		if( layers & val ) {
			item = Py_BuildValue( "i", bit + 1 );
			PyList_Append( laylist, item );
			Py_DECREF( item );
		}
		bit++;
	}
	return laylist;
}
//------------------------Armature.layer (setter)
static int Armature_setLayers(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyList_Check(value)){
			int layers = 0, len_list = 0;
			int val;
			PyObject *item = NULL;

			len_list = PyList_Size(value);

			if( len_list == 0 )
				return EXPP_ReturnIntError( PyExc_AttributeError,
				  "list can't be empty, at least one layer must be set" );

			while( len_list ) {
				--len_list;
				item = PyList_GetItem( value, len_list );
				if( !PyInt_Check( item ) )
					return EXPP_ReturnIntError( PyExc_AttributeError,
							"list must contain only integer numbers" );

				val = ( int ) PyInt_AsLong( item );
				if( val < 1 || val > 20 )
					return EXPP_ReturnIntError( PyExc_AttributeError,
						  "layer values must be in the range [1, 20]" );

				layers |= 1 << ( val - 1 );
			}

			/* update any bases pointing to our object */
			self->armature->layer = (short)layers;

			return 0;
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_ReturnIntError( PyExc_TypeError,
			"expected a list of integers" );
}

//------------------------Armature.mirrorEdit (getter)
static PyObject *Armature_getMirrorEdit(BPy_Armature *self, void *closure)
{
	if (self->armature->flag & ARM_MIRROR_EDIT)
		return EXPP_incr_ret(Py_True);
	else
		return EXPP_incr_ret(Py_False);
}
//------------------------Armature.mirrorEdit (setter)
static int Armature_setMirrorEdit(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyBool_Check(value)){
			if (value == Py_True){
				self->armature->flag |= ARM_MIRROR_EDIT;
				return 0;
			}else if (value == Py_False){
				self->armature->flag &= ~ARM_MIRROR_EDIT;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects True or False");
}
//------------------------Armature.drawType (getter)
static PyObject *Armature_getDrawType(BPy_Armature *self, void *closure)
{
	if (self->armature->drawtype == ARM_OCTA){
		return EXPP_GetModuleConstant("Blender.Armature", "OCTAHEDRON") ;
	}else if (self->armature->drawtype == ARM_LINE){
		return EXPP_GetModuleConstant("Blender.Armature", "STICK") ;
	}else if (self->armature->drawtype == ARM_B_BONE){
		return EXPP_GetModuleConstant("Blender.Armature", "BBONE") ;
	}else if (self->armature->drawtype == ARM_ENVELOPE){
		return EXPP_GetModuleConstant("Blender.Armature", "ENVELOPE") ;
	}else{
		goto RuntimeError;
	}

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sArmatureError, "drawType: ", "Internal failure!");
}
//------------------------Armature.drawType (setter)
static int Armature_setDrawType(BPy_Armature *self, PyObject *value, void *closure)
{
	PyObject *val = NULL, *name = NULL;
	long numeric_value;

	if(value){
		if(BPy_Constant_Check(value)){
			name = PyDict_GetItemString(((BPy_constant*)value)->dict, "name");
			if (!STREQ2(PyString_AsString(name), "OCTAHEDRON", "STICK") &&
				!STREQ2(PyString_AsString(name), "BBONE", "ENVELOPE"))
				goto ValueError;
			val = PyDict_GetItemString(((BPy_constant*)value)->dict, "value");
			if (PyInt_Check(val)){
				numeric_value = PyInt_AS_LONG(val);
				self->armature->drawtype = (int)numeric_value;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects module constant");

ValueError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Argument must be the constant OCTAHEDRON, STICK, BBONE, or ENVELOPE");
}
//------------------------Armature.ghostStep (getter)
static PyObject *Armature_getStep(BPy_Armature *self, void *closure)
{
	return PyInt_FromLong((long)self->armature->ghostsize);
}
//------------------------Armature.ghostStep (setter)
static int Armature_setStep(BPy_Armature *self, PyObject *value, void *closure)
{
	long numerical_value;

	if(value){
		if(PyInt_Check(value)){
			numerical_value = PyInt_AS_LONG(value);
			if (numerical_value > 20.0f || numerical_value < 1.0f)
				goto ValueError;
			self->armature->ghostsize = (short)numerical_value;
			return 0;
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects Integer");

ValueError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Argument must fall within 1-20");
}
//------------------------Armature.ghost (getter)
static PyObject *Armature_getGhost(BPy_Armature *self, void *closure)
{
	return PyInt_FromLong((long)self->armature->ghostep);
}
//------------------------Armature.ghost (setter)
static int Armature_setGhost(BPy_Armature *self, PyObject *value, void *closure)
{
	long numerical_value;

	if(value){
		if(PyInt_Check(value)){
			numerical_value = PyInt_AS_LONG(value);
			if (numerical_value > 30.0f || numerical_value < 0.0f)
				goto ValueError;
			self->armature->ghostep = (short)numerical_value;
			return 0;
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects Integer");

ValueError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Argument must fall within 0-30");
}
//------------------------Armature.drawNames (getter)
static PyObject *Armature_getDrawNames(BPy_Armature *self, void *closure)
{
	if (self->armature->flag & ARM_DRAWNAMES)
		return EXPP_incr_ret(Py_True);
	else
		return EXPP_incr_ret(Py_False);
}
//------------------------Armature.drawNames (setter)
static int Armature_setDrawNames(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyBool_Check(value)){
			if (value == Py_True){
				self->armature->flag |= ARM_DRAWNAMES;
				return 0;
			}else if (value == Py_False){
				self->armature->flag &= ~ARM_DRAWNAMES;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects True or False");
}
//------------------------Armature.drawAxes (getter)
static PyObject *Armature_getDrawAxes(BPy_Armature *self, void *closure)
{
	if (self->armature->flag & ARM_DRAWAXES)
		return EXPP_incr_ret(Py_True);
	else
		return EXPP_incr_ret(Py_False);
}
//------------------------Armature.drawAxes (setter)
static int Armature_setDrawAxes(BPy_Armature *self, PyObject *value, void *closure)
{
	if(value){
		if(PyBool_Check(value)){
			if (value == Py_True){
				self->armature->flag |= ARM_DRAWAXES;
				return 0;
			}else if (value == Py_False){
				self->armature->flag &= ~ARM_DRAWAXES;
				return 0;
			}
		}
	}
	goto AttributeError;

AttributeError:
	return EXPP_intError(PyExc_AttributeError, "%s%s", 
		sArmatureBadArgs, "Expects True or False");
}
//------------------------Armature.delayDeform (getter)
static PyObject *Armature_getDelayDeform(BPy_Armature *self, void *closure)
{
	if (self->armature->flag & ARM_DELAYDEFORM)
		return EXPP_incr_ret(Py_True);
	else
		return EXPP_incr_ret(Py_False);
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
		return EXPP_incr_ret(Py_True);
	else
		return EXPP_incr_ret(Py_False);
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
		return EXPP_incr_ret(Py_True);
	else
		return EXPP_incr_ret(Py_False);
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

//------------------------Armature.bones (getter)
//Gets the name of the armature
static PyObject *Armature_getBoneDict(BPy_Armature *self, void *closure)
{
    return EXPP_incr_ret((PyObject*)self->Bones);
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

//------------------------Bone.layerMask (get)
static PyObject *Armature_getLayerMask(BPy_Armature *self)
{
	/* do this extra stuff because the short's bits can be negative values */
	unsigned short laymask = 0;
	laymask |= self->armature->layer;
	return PyInt_FromLong((int)laymask);
}
//------------------------Bone.layerMask (set)
static int Armature_setLayerMask(BPy_Armature *self, PyObject *value)
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

	self->armature->layer = 0;
	self->armature->layer |= laymask;

	return 0;
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
	{"update", (PyCFunction) Armature_update, METH_NOARGS, 
		"() - Rebuilds the armature based on changes to bones since the last call to makeEditable"},
	{"__copy__", (PyCFunction) Armature_copy, METH_NOARGS, 
		"() - Return a copy of the armature."},
	{"copy", (PyCFunction) Armature_copy, METH_NOARGS, 
		"() - Return a copy of the armature."},
	{NULL, NULL, 0, NULL}
};
//------------------------tp_getset
//This contains methods for attributes that require checking
static PyGetSetDef BPy_Armature_getset[] = {
	GENERIC_LIB_GETSETATTR,
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
	{"drawAxes", (getter)Armature_getDrawAxes, (setter)Armature_setDrawAxes, 
		"Enable/Disable  drawing  the bone axes", NULL},
	{"drawNames", (getter)Armature_getDrawNames, (setter)Armature_setDrawNames, 
		"Enable/Disable  drawing the bone names", NULL},
	{"ghost", (getter)Armature_getGhost, (setter)Armature_setGhost, 
		"Draw a number of ghosts around the current frame for current Action", NULL},
	{"ghostStep", (getter)Armature_getStep, (setter)Armature_setStep, 
		"The number of frames between ghost instances", NULL},
	{"drawType", (getter)Armature_getDrawType, (setter)Armature_setDrawType, 
		"The type of drawing currently applied to the armature", NULL},
	{"mirrorEdit", (getter)Armature_getMirrorEdit, (setter)Armature_setMirrorEdit, 
		"Enable/Disable X-axis mirrored editing", NULL},
	{"autoIK", (getter)Armature_getAutoIK, (setter)Armature_setAutoIK, 
		"Adds temporal IK chains while grabbing bones", NULL},
	{"layers", (getter)Armature_getLayers, (setter)Armature_setLayers, 
		"List of layers for the armature", NULL},
	{"layerMask", (getter)Armature_getLayerMask, (setter)Armature_setLayerMask, 
		"Layer bitmask", NULL },
	{NULL, NULL, NULL, NULL, NULL}
};
//------------------------tp_new
//This methods creates a new object (note it does not initialize it - only the building)
//This can be called through python by myObject.__new__() however, tp_init is not called
static PyObject *Armature_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	BPy_Armature *py_armature = NULL;
	bArmature *bl_armature;

	bl_armature = add_armature("Armature");
	if(bl_armature) {
		bl_armature->id.us = 0; // return count to 0 - add_armature() inc'd it 

		py_armature = (BPy_Armature*)type->tp_alloc(type, 0); //*new*
		if (py_armature == NULL)
			goto RuntimeError;

		py_armature->armature = bl_armature;

		//create armature.bones
		py_armature->Bones = (BPy_BonesDict*)PyBonesDict_FromPyArmature(py_armature);
		if (!py_armature->Bones)
			goto RuntimeError;

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


/*****************************************************************************/
/* Function:	Armature_compare						 */
/* Description: This is a callback function for the BPy_Armature type. It	 */
/*		compares two Armature_Type objects. Only the "==" and "!="  */
/*		comparisons are meaninful. Returns 0 for equality and -1 if  */
/*		they don't point to the same Blender Object struct.	 */
/*		In Python it becomes 1 if they are equal, 0 otherwise.	 */
/*****************************************************************************/
static int Armature_compare( BPy_Armature * a, BPy_Armature * b )
{
	return ( a->armature == b->armature ) ? 0 : -1;
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
	if (self->weaklist != NULL)
		PyObject_ClearWeakRefs((PyObject *) self); /* this causes the weakref dealloc func to be called */
	
	Py_DECREF(self->Bones);
	PyObject_DEL( self );
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
	(cmpfunc) Armature_compare,		//tp_compare
	(reprfunc) Armature_repr,		//tp_repr
	0,								//tp_as_number
	0,								//tp_as_sequence
	0,								//tp_as_mapping
	( hashfunc ) GenericLib_hash,	//tp_hash
	0,								//tp_call
	0,								//tp_str
	0,								//tp_getattro
	0,								//tp_setattro
	0,								//tp_as_buffer
	Py_TPFLAGS_DEFAULT| Py_TPFLAGS_HAVE_WEAKREFS,				//tp_flags
	BPy_Armature_doc,				//tp_doc
	0,								//tp_traverse
	0,								//tp_clear
	0, 								//tp_richcompare
	offsetof(BPy_Armature, weaklist),	//tp_weaklistoffset
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
* or it will return a {key:value} dictionary of all armatures when nothing is passed */
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
		if (!seq)
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
			if (!item) {
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
		if(!dict){
			Py_DECREF(seq);
			goto RuntimeError;
		}
		if(size == 0){	//GET ALL ARMATURES
			data = G.main->armature.first; //get the first data ID from the armature library
			while (data){
				py_armature = Armature_CreatePyObject(data); //*new*
				if (!py_armature) {
					EXPP_decr2(seq, dict);
					return NULL; /* error is set from Armature_CreatePyObject */
				}
				sprintf(buffer, "%s", ((bArmature*)data)->id.name +2);
				if(PyDict_SetItemString(dict, buffer, py_armature) == -1){ //add to dictionary
					EXPP_decr3(seq, dict, py_armature);
					goto RuntimeError;
				}
				Py_DECREF(py_armature);
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
					py_armature = Armature_CreatePyObject(data); //*new*
					if (!py_armature) {
						EXPP_decr2(seq, dict);
						return NULL; /* error is set from Armature_CreatePyObject */
					}
					
					if(PyDict_SetItemString(dict, name, py_armature) == -1){ //add to dictionary
						EXPP_decr3(seq, dict, py_armature);
						goto RuntimeError;
					}
					Py_DECREF(py_armature);
				}else{
					if(PyDict_SetItemString(dict, name, Py_None) == -1){ //add to dictionary
						EXPP_decr2(seq, dict);
						goto RuntimeError;
					}
					Py_DECREF(Py_None);
				}
			}
			Py_DECREF(seq);
		}
		return dict;
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
			return Armature_CreatePyObject(data); //*new*
		}else{
			char buffer[128];
			PyOS_snprintf( buffer, sizeof(buffer),
						   "Armature \"%s\" not found", name);
			return EXPP_ReturnPyObjError( PyExc_ValueError,
										  buffer );
		}
	}

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sModuleError, "Get(): ", "Internal Error Ocurred");

AttributeError:
	return EXPP_objError(PyExc_AttributeError, "%s%s%s", 
		sModuleBadArgs, "Get(): ", "- Expects (optional) string sequence");
}


//----------------Blender.Armature.New()
static PyObject *M_Armature_New(PyObject * self, PyObject * args)
{
	char *name = "Armature";
	struct bArmature *armature;
	BPy_Armature *obj;

	if( !PyArg_ParseTuple( args, "|s", &name ) )
		return EXPP_ReturnPyObjError( PyExc_TypeError,
					      "expected nothing or a string as argument" );

	armature= add_armature(name);
	armature->id.us = 0;
	obj = (BPy_Armature *)Armature_CreatePyObject(armature); /*new*/

	if( !obj )
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				       "PyObject_New() failed" );	

	obj->armature = armature;
	return (PyObject *)obj;
}


//-------------------MODULE METHODS DEFINITION-----------------------------

static char M_Armature_Get_doc[] = "(name) - return the armature with the name 'name', \
  returns None if not found.\n If 'name' is not specified, it returns a list of all \
  armatures in the\ncurrent scene.";

static char M_Armature_New_doc[] = "(name) - return a new armature object.";

struct PyMethodDef M_Armature_methods[] = {
	{"Get", M_Armature_Get, METH_VARARGS, M_Armature_Get_doc},
	{"New", M_Armature_New, METH_VARARGS, M_Armature_New_doc},
	{NULL, NULL, 0, NULL}
};
//------------------VISIBLE PROTOTYPE IMPLEMENTATION-----------------------
//------------------------Armature_RebuildEditbones (internal)
PyObject * Armature_RebuildEditbones(PyObject *pyarmature)
{
	return Armature_makeEditable((BPy_Armature*)pyarmature);
}

//------------------------Armature_RebuildBones (internal)
PyObject *Armature_RebuildBones(PyObject *pyarmature)
{
	return Armature_update((BPy_Armature*)pyarmature);
}

/* internal func to remove weakref from weakref list */
PyObject * arm_weakref_callback_weakref_dealloc(PyObject *self, PyObject *weakref)
{
	char *list_name = ARM_WEAKREF_LIST_NAME;
	PyObject *maindict = NULL, *armlist = NULL;
	int i;
	
	maindict= PyModule_GetDict(PyImport_AddModule(	"__main__"));
	armlist = PyDict_GetItemString(maindict, list_name);
	if( !armlist){
		printf("Oops - update_armature_weakrefs()\n");
		Py_RETURN_NONE;
	}
	
	i = PySequence_Index(armlist, weakref);
	if (i==-1) {
		printf("callback weakref internal error, weakref not in list\n\tthis should never happen.\n");
		Py_RETURN_NONE;
	}
	PySequence_DelItem(armlist, i);
	Py_RETURN_NONE;
}

/*-----------------(internal)
 * Converts a bArmature to a PyArmature */

PyObject *Armature_CreatePyObject(struct bArmature *armature)
{
	BPy_Armature *py_armature = NULL;
	PyObject *maindict = NULL, *weakref = NULL;
	PyObject *armlist = NULL;  /* list of armature weak refs */
	char *list_name = ARM_WEAKREF_LIST_NAME;
	int i;

	//put a weakreference in __main__
	maindict= PyModule_GetDict(PyImport_AddModule(	"__main__"));

	armlist = PyDict_GetItemString(maindict, list_name);
	if(!armlist) {
		printf("Oops - can't get the armature weakref list\n");
		goto RuntimeError;
	}

	/* see if we alredy have it */
	for (i=0; i< PyList_Size(armlist); i++) { 
		py_armature = (BPy_Armature *)PyWeakref_GetObject(PyList_GET_ITEM(armlist, i));
		if (BPy_Armature_Check(py_armature) && py_armature->armature == armature) {
			Py_INCREF(py_armature);
			/*printf("reusing armature\n");*/
			return (PyObject *)py_armature;
		}
	}

	
	/*create armature type*/
	py_armature = PyObject_NEW( BPy_Armature, &Armature_Type );
	
	if (!py_armature){
		printf("Oops - can't create py armature\n");
		goto RuntimeError;
	}

	py_armature->armature = armature;
	py_armature->weaklist = NULL; //init the weaklist
	
	//create armature.bones
	py_armature->Bones = (BPy_BonesDict*)PyBonesDict_FromPyArmature(py_armature);
	if (!py_armature->Bones){
		printf("Oops - creating armature.bones\n");
		goto RuntimeError;
	}
	
	weakref = PyWeakref_NewRef((PyObject*)py_armature, arm_weakref_callback_weakref_dealloc__pyfunc);
	if (PyList_Append(armlist, weakref) == -1){
		printf("Oops - list-append failed\n");
		goto RuntimeError;
	}
	Py_DECREF(weakref);

	return (PyObject *) py_armature;

RuntimeError:
	return EXPP_objError(PyExc_RuntimeError, "%s%s%s", 
		sModuleError, "Armature_CreatePyObject: ", "Internal Error Ocurred");
}
//-----------------(internal)
//Converts a PyArmature to a bArmature
struct bArmature *PyArmature_AsArmature(BPy_Armature *py_armature)
{
	return (py_armature->armature);
}

struct bArmature *Armature_FromPyObject( PyObject * py_obj )
{
	return PyArmature_AsArmature((BPy_Armature*)py_obj);
}

/* internal use only */
static PyMethodDef bpy_arm_weakref_callback_weakref_dealloc[] = {
	{"arm_weakref_callback_weakref_dealloc", arm_weakref_callback_weakref_dealloc, METH_O, ""}
};

//-------------------MODULE INITIALIZATION--------------------------------
PyObject *Armature_Init(void)
{
	PyObject *module, *dict;

	//Initializes TypeObject.ob_type
	if (PyType_Ready(&Armature_Type) < 0 ||	PyType_Ready(&BonesDict_Type) < 0 || 
		PyType_Ready(&EditBone_Type) < 0 ||	PyType_Ready(&Bone_Type) < 0) {
		return EXPP_incr_ret(Py_None);
	}

	/* Weakref management - used for callbacks so we can
	 * tell when a callback has been removed that a UI button referenced */
	arm_weakref_callback_weakref_dealloc__pyfunc = PyCFunction_New(bpy_arm_weakref_callback_weakref_dealloc, NULL);
	
	
	//Register the module
	module = Py_InitModule3("Blender.Armature", M_Armature_methods, 
		"The Blender Armature module"); 

	//Add TYPEOBJECTS to the module
	PyModule_AddObject(module, "Armature", 
		EXPP_incr_ret((PyObject *)&Armature_Type)); //*steals*
	PyModule_AddObject(module, "Bone", 
		EXPP_incr_ret((PyObject *)&Bone_Type)); //*steals*
	PyModule_AddObject(module, "Editbone", 
		EXPP_incr_ret((PyObject *)&EditBone_Type)); //*steals*

	//Add CONSTANTS to the module
	PyModule_AddObject(module, "CONNECTED", 
		PyConstant_NewInt("CONNECTED", BONE_CONNECTED));
	PyModule_AddObject(module, "HINGE", 
		PyConstant_NewInt("HINGE", BONE_HINGE));
	PyModule_AddObject(module, "NO_DEFORM", 
		PyConstant_NewInt("NO_DEFORM", BONE_NO_DEFORM));
	PyModule_AddObject(module, "MULTIPLY", 
		PyConstant_NewInt("MULTIPLY", BONE_MULT_VG_ENV));
	PyModule_AddObject(module, "HIDDEN_EDIT", 
		PyConstant_NewInt("HIDDEN_EDIT", BONE_HIDDEN_A));
	PyModule_AddObject(module, "ROOT_SELECTED", 
		PyConstant_NewInt("ROOT_SELECTED", BONE_ROOTSEL));
	PyModule_AddObject(module, "BONE_SELECTED", 
		PyConstant_NewInt("BONE_SELECTED", BONE_SELECTED));
	PyModule_AddObject(module, "TIP_SELECTED", 
		PyConstant_NewInt("TIP_SELECTED", BONE_TIPSEL));

	PyModule_AddObject(module, "OCTAHEDRON", 
		PyConstant_NewInt("OCTAHEDRON", ARM_OCTA));
	PyModule_AddObject(module, "STICK", 
		PyConstant_NewInt("STICK", ARM_LINE));
	PyModule_AddObject(module, "BBONE", 
		PyConstant_NewInt("BBONE", ARM_B_BONE));
	PyModule_AddObject(module, "ENVELOPE", 
		PyConstant_NewInt("ENVELOPE", ARM_ENVELOPE));

	//Add SUBMODULES to the module
	dict = PyModule_GetDict( module ); //borrowed
	PyDict_SetItemString(dict, "NLA", NLA_Init()); //creates a *new* module

	return module;
}
