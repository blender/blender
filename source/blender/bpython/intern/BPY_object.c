/** Object module; access to Object objects in Blender
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
  *
  */
#include "Python.h"
#include "BPY_macros.h"
#include "MEM_guardedalloc.h"
#include "opy_vector.h" /* matrix datatypes */

#include "b_interface.h" // most datatypes

#include "opy_datablock.h"

#include "BLI_arithb.h" /* Mat4Invert */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* PROTOS */


/* ------------------------------------------------------------------------ */

/**************************************************/
/* Object properties for access by datablock code */

#define NULLFUNC 0
#define NULLHANDLING 0

/* structure: see opy_datablock.h */
/* attrname, DNA_membername, type      stype, min,  max, index,dlist,    
		handlingflag, extra1Ptr, extra2Ptr, extra3Ptr */

DataBlockProperty Object_Properties[]= {
	{"LocX",	"loc[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {0}, {3, -sizeof(float)}}, 
	{"LocY",	"loc[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {1}, {3, -sizeof(float)}}, 
	{"LocZ",	"loc[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {2}, {3, -sizeof(float)}}, 
	{"loc",		"loc[3]",	DBP_TYPE_VEC, 0, 3.0}, 

	{"dLocX",	"dloc[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {0}, {3, -sizeof(float)}}, 
	{"dLocY",	"dloc[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {1}, {3, -sizeof(float)}}, 
	{"dLocZ",	"dloc[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {2}, {3, -sizeof(float)}}, 
	{"dloc",	"dloc[3]",	DBP_TYPE_VEC, 0, 3.0}, 

	{"RotX",	"rot[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {0}, {3, -sizeof(float)}}, 
	{"RotY",	"rot[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {1}, {3, -sizeof(float)}}, 
	{"RotZ",	"rot[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {2}, {3, -sizeof(float)}}, 
	{"rot",		"rot[3]",	DBP_TYPE_VEC, 0, 3.0}, 

	{"dRotX",	"drot[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {0}, {3, -sizeof(float)}}, 
	{"dRotY",	"drot[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {1}, {3, -sizeof(float)}}, 
	{"dRotZ",	"drot[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {2}, {3, -sizeof(float)}}, 
	{"drot",	"drot[3]",	DBP_TYPE_VEC, 0, 3.0}, 

	{"SizeX",	"size[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {0}, {3, -sizeof(float)}}, 
	{"SizeY",	"size[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {1}, {3, -sizeof(float)}}, 
	{"SizeZ",	"size[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {2}, {3, -sizeof(float)}}, 
	{"size",	"size[3]",	DBP_TYPE_VEC, 0, 3.0}, 

	{"dSizeX",	"dsize[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {0}, {3, -sizeof(float)}}, 
	{"dSizeY",	"dsize[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {1}, {3, -sizeof(float)}}, 
	{"dSizeZ",	"dsize[3]",	DBP_TYPE_FLO, 0, 0.0,	0.0, {2}, {3, -sizeof(float)}}, 
	{"dsize",	"dsize[3]",	DBP_TYPE_VEC, 0, 3.0}, 

	{"EffX",	"effx",		DBP_TYPE_FLO, DBP_TYPE_FUN, 0.0,	0.0, {0}, {0}, DBP_HANDLING_FUNC, Object_special_getattr, 0, Object_special_setattr}, 
	{"EffY",	"effy",		DBP_TYPE_FLO, DBP_TYPE_FUN, 0.0,	0.0, {0}, {0}, DBP_HANDLING_FUNC, Object_special_getattr, 0, Object_special_setattr}, 
	{"EffZ",	"effz",		DBP_TYPE_FLO, DBP_TYPE_FUN, 0.0,	0.0, {0}, {0}, DBP_HANDLING_FUNC, Object_special_getattr, 0, Object_special_setattr}, 

	{"Layer",	"layer",	DBP_TYPE_INT, DBP_TYPE_FUN, 0.0,	0.0, {0}, {0}, DBP_HANDLING_FUNC, Object_special_getattr, 0, Object_special_setattr}, 
	{"layer",	"layer",	DBP_TYPE_INT, DBP_TYPE_FUN, 0.0,	0.0, {0}, {0}, DBP_HANDLING_FUNC, Object_special_getattr, 0, Object_special_setattr}, 

	{"parent",	"*parent",	DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	{"track",	"*track",	DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	{"data",	"*data",	DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 

	{"mat",		"matrix",		DBP_TYPE_FUN, 0, 0.0, 0.0, {0}, {0}, DBP_HANDLING_FUNC, Object_special_getattr, newMatrixObject}, 
	{"matrix",		"matrix",		DBP_TYPE_FUN, 0, 0.0, 0.0, {0}, {0}, DBP_HANDLING_FUNC, Object_special_getattr, newMatrixObject}, 

	{"colbits",	"colbits",	DBP_TYPE_SHO, 0, 0.0,	0.0},  
	{"drawType",	"dt",	DBP_TYPE_CHA, 0, 0.0,	0.0},  
	{"drawMode",	"dtx",	DBP_TYPE_CHA, 0, 0.0,	0.0},  
	
	{NULL}
};

/*************************/
/* Object module methods */

DATABLOCK_GET(Objectmodule, object, getObjectList())

char Objectmodule_New_doc[] = "(type) - Add a new object of type 'type' in the current scene";
static PyObject *Objectmodule_New(PyObject *self, PyObject *args)
{
	Object *ob;
	int type;

	if (!PyArg_ParseTuple(args, "i", &type)) {
		PyErr_SetString(PyExc_TypeError, "type expected");
		return 0;
	}	
	/* add object */
	ob = object_new(type);
	return DataBlock_fromData(ob);
}

static char Objectmodule_getSelected_doc[]=
"() - Returns a list of selected Objects in the active layer(s)\n\
The active object is the first in the list, if visible";

static PyObject *Objectmodule_getSelected (PyObject *self, PyObject *args)
{
	PyObject *ob, *list;
	Base *base;
	Object *tmp;
	
	list= PyList_New(0);
	
	if (ActiveBase && SelectedAndLayer(ActiveBase)) {
		tmp = ActiveObject; /* active object is first in list */
		if (!tmp) goto no_selection;
		ob = DataBlock_fromData(tmp);
		PyList_Append(list, ob); Py_DECREF(ob); // because previous call increfs
	} 

	base = FirstBase;
	while (base) {
		if (SelectedAndLayer(base) && base != ActiveBase) {
			PyObject *ob = DataBlock_fromData(ObjectfromBase(base));
			if (!ob) goto no_selection;
			PyList_Append(list, ob); Py_DECREF(ob); 
		}
		base= base->next;
	}
	return list;
no_selection:
	Py_DECREF(list);
	Py_INCREF(Py_None);
	return Py_None;

}


struct PyMethodDef Objectmodule_methods[] = {
	{"New",			Objectmodule_New,			METH_VARARGS, Objectmodule_New_doc},
	// emulation :
	{"Get",			Objectmodule_get,			METH_VARARGS, Objectmodule_get_doc}, // XXX
	{"get",			Objectmodule_get,			METH_VARARGS, Objectmodule_get_doc},
	{"getSelected", Objectmodule_getSelected,	METH_VARARGS, Objectmodule_getSelected_doc},
	{NULL, NULL}
};

/*************************/
/* Object object methods */

/* Object_get is defined as macro; see opy_datablock.h */


static char Object_getType_doc[] = "() - returns Object type";

static PyObject *Object_getType(PyObject *self, PyObject *args)
{
	Object *ob= PYBLOCK_AS_OBJECT(self);
	return Py_BuildValue("i", (short) ob->type);
}

static char Object_getMatrix_doc[] = "() - returns 4D matrix of object";

static PyObject *Object_getMatrix(PyObject *self, PyObject *args)
{
	Object *ob= PYBLOCK_AS_OBJECT(self);
	return newMatrixObject(ob->obmat);
}

static char Object_getInverseMatrix_doc[] = "() - returns inverse 4D matrix of object";

static PyObject *Object_getInverseMatrix(PyObject *self, PyObject *args)
{
	Object *ob= PYBLOCK_AS_OBJECT(self);
	float inverse[4][4];
	Mat4Invert(inverse, ob->obmat);
	return newMatrixObject(inverse);
}

static char Object_clrParent_doc[]= 
"(mode = 0, fast = 0) - clears parent object.\n\
If specified:\n\
   mode   2: keep object transform\n\
   fast > 0: don't update scene hierarchy (faster)\n\
";

static PyObject *Object_clrParent(PyObject *self, PyObject *args)
{
	int mode = 0, ret;
	int fast = 0;
	Object *ob= PYBLOCK_AS_OBJECT(self);

	BPY_TRY(PyArg_ParseTuple(args, "|ii",  &mode, &fast));
	ret = object_clrParent(ob, mode, fast);
	return Py_BuildValue("i", ret);
}

DATABLOCK_ASSIGN_IPO(Object, object) // defines Object_assignIpo

static char Object_makeParent_doc[]=
"([obj1, obj2, ...], mode = 0, fast = 0) - makes 'self' a parent of the\n\
objects in the list.\n\
If specified:\n\
    mode <> 0: do not clear parent inverse\n\
    fast <> 0      : do not update scene hierarchy (faster)\n\
\n\
If fast is set, you will have to call Scene.getCurrent.update() before\n\
redraw.";

static PyObject *Object_makeParent(PyObject *self, PyObject *args)
{
	int i, ret;
	PyObject *list;
	int noninverse = 0;
	int fast = 0;

	DataBlock *parblk = (DataBlock*) self;

	BPY_TRY(PyArg_ParseTuple(args, "O|ii", &list, &noninverse, &fast));
	if (!PySequence_Check(list)){
		PyErr_SetString(PyExc_TypeError, "expects a list of objects");
		return 0;
	}

	for (i = 0; i < PySequence_Length(list); i ++) {
		DataBlock *childblk = (DataBlock *) PySequence_GetItem(list, i);

		if (!DataBlock_Check(childblk)) {
			PyErr_SetString(PyExc_TypeError, "Object Type expected");
			 return 0;
		}
		ret = object_makeParent((Object *) parblk->data, (Object *) childblk->data, noninverse, fast);

		Py_DECREF((PyObject *) childblk); // don't need it anymore
		if (ret == 0) { // could not parent
			PyErr_SetString(PyExc_RuntimeError, "parenting failed!");
			return 0;
		}	
	}	

	if (PyErr_Occurred()) {
		PyErr_Print();
	}
	return Py_BuildValue("i", 1);
}

static char Object_getMaterials_doc[] = "() - returns a list of object materials";

static PyObject *Object_getMaterials(PyObject *self, PyObject *args)
{
	DataBlock *objectblk = (DataBlock*) self;
	Object *object = PYBLOCK_AS_OBJECT(objectblk);
	return PyList_fromMaterialList(object->mat, object->totcol);
}

static char Object_setMaterials_doc[] = "(materialList) - sets object materials";

static PyObject *Object_setMaterials(PyObject *self, PyObject *args)
{
	int len;
	int ret;
	DataBlock *objectblk = (DataBlock*) self;
	Object *object = PYBLOCK_AS_OBJECT(objectblk);
	PyObject *list;
	Material **matlist;

	BPY_TRY(PyArg_ParseTuple(args, "O!", &PyList_Type, &list));
	len = PySequence_Length(list);
	if (len) {
		matlist = newMaterialList_fromPyList(list);
		if (!matlist) {
			PyErr_SetString(PyExc_TypeError, 
			"materialList must be a list of valid materials!");
			return 0;
		}
		ret = object_setMaterials(object, matlist, len);
	} else {
		ret = 0;
	}
	return Py_BuildValue("i", ret);
}

static char Object_copy_doc[] = "() - returns a copy of the object, sharing the same data";

static PyObject *Object_copy(PyObject *self, PyObject *args)
{
	Object *new;

	DataBlock *objectblk = (DataBlock*) self;
	Object *object = PYBLOCK_AS_OBJECT(objectblk);

	new = object_copy(object);
	return DataBlock_fromData(new);
}

static char Object_shareFrom_doc[] = "(obj) - link data of 'self' with data of 'obj' -- \n\
only if of same type!";

static PyObject *Object_shareFrom(PyObject *self, PyObject *args)
{
	DataBlock *blockA = (DataBlock*) self;
	DataBlock *blockB;
	Object *object, *other;
	int t;

	BPY_TRY(PyArg_ParseTuple(args, "O!", &DataBlock_Type, &blockB));

	if (!DataBlock_isType(blockB, ID_OB)) {
		PyErr_SetString(PyExc_TypeError, "Argument 1 is not of type 'Object'");
		return NULL;
	}

	object = PYBLOCK_AS_OBJECT(blockA);
	other  = PYBLOCK_AS_OBJECT(blockB);

	if (other->type != object->type) {
		PyErr_SetString(PyExc_TypeError, "Objects are not of same data type");
		return NULL;
	}	
	t = object->type;
	switch (t) {
		case OB_MESH:
			return Py_BuildValue("i", object_linkdata(object, other->data));
		default:
			PyErr_SetString(PyExc_TypeError, "Type not supported");
			return NULL;
	}
}

/******************/
/* get & set attr */

static float g_zero_float= 0.0;

/* Object attributes functions which require getter/setter C functions 
   different from the access provided by DataBlock support */

/* get special attributes through datablock property structure */

void *Object_special_getattr(void *vdata, char *name) 
{
	Object *ob= (Object *) vdata;
	int scriptflag;

	if (STREQ(name, "layer")) {
		return &ob->lay;
		
	} else if (strncmp(name, "eff", 3)==0) {
		Ika *ika= ob->data;

		if (ob->type==OB_IKA && ika) {
			if      (name[3]=='x') return &ika->effg[0];
			else if (name[3]=='y') return &ika->effg[1];
			else if (name[3]=='z') return &ika->effg[2];
		}
	
		return &g_zero_float;
	/* these only for compatibiliy... XXX */
	} else if (STREQ(name, "matrix")) {
		scriptflag = during_script();
		disable_where_script(1);
		where_is_object(ob);
		disable_where_script(scriptflag);
		
		return &ob->obmat;
	} else if (STREQ(name, "inverse") || STREQ(name, "inverseMatrix")) {
		return Object_getInverseMatrix(vdata, 0);
	}
	/* end compatibility */
	
	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;
}

int Object_special_setattr(void *vdata, char *name, PyObject *py_ob)
{
	Object *ob= (Object *) vdata;

	if (STREQ(name, "layer")) {
		Base *base;
		int ival;
		
		if (!PyArg_Parse(py_ob, "i", &ival)) return -1;
		
		ob->lay= ival;
	// TODO this is old stuff, maybe move to update routine at end of
	// script execution ?
		base= (G.scene->base.first);
		while (base) {
			if (base->object == ob) base->lay= ob->lay;
			base= base->next;
		}
	// end TODO

		return 0;
	} else if (strncmp(name, "eff", 3)==0) {
		Ika *ika= ob->data;
		float fval;
		
		if (!PyArg_Parse(py_ob, "f", &fval)) return -1;
		
		if (ob->type==OB_IKA && ika) {
			if (name[3]=='x') ika->effg[0]= fval;
			else if (name[3]=='y') ika->effg[1]= fval;
			else if (name[3]=='z') ika->effg[2]= fval;
			
			itterate_ika(ob);
		}
		return 0;
	}

	PyErr_SetString(PyExc_AttributeError, name);
	return -1;
}



#undef MethodDef
#define MethodDef(func) _MethodDef(func, Object)

struct PyMethodDef Object_methods[] = {
	MethodDef(makeParent),
	MethodDef(copy),
	MethodDef(shareFrom),
	MethodDef(getMatrix),
	MethodDef(getType),
	MethodDef(getInverseMatrix),
	MethodDef(clrParent),
	MethodDef(assignIpo),
	MethodDef(clrIpo),
	MethodDef(getMaterials),
	MethodDef(setMaterials),
	{NULL, NULL}
};

#undef BPY_ADDCONST
#define BPY_ADDCONST(dict, name) insertConst(dict, #name, PyInt_FromLong(OB_##name))

PyObject *initObject(void)
{
	PyObject *mod, *dict, *d;

	mod= Py_InitModule(MODNAME(BLENDERMODULE) ".Object", Objectmodule_methods);
	dict= PyModule_GetDict(mod);
	d = ConstObject_New();
	PyDict_SetItemString(dict, "Types", d);
	BPY_ADDCONST(d, EMPTY);
	BPY_ADDCONST(d, MESH);
	BPY_ADDCONST(d, LAMP);
	BPY_ADDCONST(d, CAMERA);

	d = ConstObject_New();
	PyDict_SetItemString(dict, "DrawTypes", d);
	/* dt flags */
	BPY_ADDCONST(d, BOUNDBOX);
	BPY_ADDCONST(d, WIRE);
	BPY_ADDCONST(d, SOLID);
	BPY_ADDCONST(d, SHADED);
	BPY_ADDCONST(d, TEXTURE);
	d = ConstObject_New();
	PyDict_SetItemString(dict, "DrawModes", d);
	/* dtx flags */
	BPY_ADDCONST(d, BOUNDBOX);
	BPY_ADDCONST(d, AXIS);
	BPY_ADDCONST(d, TEXSPACE);
	insertConst(d, "NAME", PyInt_FromLong(OB_DRAWNAME));
	return mod;
}

