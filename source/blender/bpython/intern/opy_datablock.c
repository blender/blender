/* Datablock handling code. This handles the generic low level access to Blender
   Datablocks. */

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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**************************************************************************
 * This code provides low level, generalized access to the Blender Datablock 
 * objects. It basically creates a descriptor Python Object of type 'DataBlock'
 * for each requested Blender datablock.
 * This introduces the question of synchronization, for example:
 *  What happens if an Object is deleted?
 * 
 * Blender Objects have their own 'reference counting', e.g. a Mesh datablock
 * used by two Objects has a user count of 2. Datablocks with user count of 0
 * are not saved to Disk -- this is the current way Blender does
 * 'garbage collection'
 * Therefore, an object should normally not be deleted by Python, but rather
 * unlinked from its parent object.
 * Still, for other objects like Scene or Text objects, deletion from 'Main'
 * is desired.
 * The current workaround:
 
 * Some objects can be explicitely deleted (not recommended, but possible) --
 * they have a user count of 1, even if they are used by objects in some way,
 * for example Text objects which are used by any other Blender object
 * through a ScriptLink.
 * 
 * Objects that are deleted through Python end up with a 'dead' descriptor;
 * accessing the descriptor after deletion causes a Python exception.
 * 
 * NASTY UGLY DIRTY, VUILE, DRECKIGES AND STILL REMAINING PROBLEM:
 * 
 * It is (in the current API) possible to construct the case, that an
 * Object is deleted in Blender, but the Python descriptor does not know
 * about this. Accessing the descriptor (which simply contains a pointer to
 * the raw datablock struct) will most probably end in colourful joy.
 * 
 * TODO:
 * possible solutions:
 * - rewrite datablock handling that way, that the descriptor uses an id 
 *   tag to retrieve that pointer through a getPointerbyID() function 
 *   (if the object exists!) on each access. Slow.
 * - make sure that deletion always happends by the descriptor and never
 *   delete the raw datastructure. This solution would imply a major
 *   redesign of user action handling (GUI actions calling python).
 *   Not likely to happen...better fusion raw and python object in this case.
 *   After all, still somewhat dirty.
 * - make sure that no deletion can happen in Blender while a script
 *   still accesses the raw data - i.e. implement user counting of raw
 *   objects with descriptors. This would need an implementation of
 *   garbage collection in Blender. This might sound like the most feasible
 *   solution...
 */


#include "Python.h"
#include "BPY_macros.h"

#include "opy_datablock.h"
#include "opy_nmesh.h"

#include "opy_vector.h" /* matrix datatypes */

#include "BPY_tools.h"
#include "BPY_types.h"
#include "BPY_main.h"

#include "MEM_guardedalloc.h"

#include "b_interface.h" /* needed for most of the DNA datatypes */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* ---------------------------------------------------------------------- */

/*********************/
/* Camera Datablocks */

DATABLOCK_GET(Cameramodule, camera, getCameraList())

static char Cameramodule_New_doc[] = 
"() - returns new Camera object"; 
 
PyObject *Cameramodule_New (PyObject *self, PyObject *args) 
{ 
	Camera *obj; 
	obj = camera_new(); 
	return DataBlock_fromData(obj); 
} 

#ifdef FUTURE_PYTHON_API

DataBlockProperty Camera_Properties[]= {
	{"lens",	"lens",		DBP_TYPE_FLO, 0, 1.0,    250.0}, 
	{"clipStart","clipsta",	DBP_TYPE_FLO, 0, 0.0,    100.0}, 
	{"clipEnd",	"clipend",	DBP_TYPE_FLO, 0, 1.0,   5000.0}, 
	{"type", "type",        DBP_TYPE_SHO, 0, 0.0,      0.0},
	{"mode", "flag",        DBP_TYPE_SHO, 0, 0.0,      0.0},

	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	
	{NULL}
};

#else

DataBlockProperty Camera_Properties[]= {
	{"Lens",	"lens",		DBP_TYPE_FLO, 0, 1.0,	250.0}, 
	{"ClSta",	"clipsta",	DBP_TYPE_FLO, 0, 0.0,	100.0}, 
	{"ClEnd",	"clipend",	DBP_TYPE_FLO, 0, 1.0,	5000.0}, 

	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	
	{NULL}
};

#endif

static struct PyMethodDef Cameramodule_methods[] = {
	{"New", Cameramodule_New, METH_VARARGS, Cameramodule_New_doc},
	{"get", Cameramodule_get, METH_VARARGS, Cameramodule_get_doc},
	{NULL, NULL}
};

DATABLOCK_ASSIGN_IPO(Camera, camera) // defines Camera_assignIpo

static struct PyMethodDef Camera_methods[] = {
	{"clrIpo", Camera_clrIpo, METH_VARARGS, Camera_clrIpo_doc},
	{"assignIpo", Camera_assignIpo, METH_VARARGS, Camera_assignIpo_doc},
	{NULL, NULL}
};

/***********************/
/* Material Datablocks */


/** returns a pointer to a new (malloced) material list created from
  * a Python material list
  */

Material **newMaterialList_fromPyList(PyObject *list)
{
	int i, len;
	DataBlock *block = 0;
	Material *mat;
	Material **matlist;

	len = PySequence_Length(list);
	if (len > 16) len = 16;

	matlist = newMaterialList(len);
	
	for (i= 0; i < len; i++) {
		
		block= (DataBlock *) PySequence_GetItem(list, i);
		
		if (DataBlock_isType(block, ID_MA)) {
			mat = (Material *) block->data;
			matlist[i] = mat;
		} else { 
			// error; illegal type in material list
			Py_DECREF(block);
			MEM_freeN(matlist);
			return NULL;
		}	
		Py_DECREF(block);
	}
	return matlist;
}

/** Return Python List from material pointer list 'matlist' with length
  * 'len'
  *
  */

PyObject *PyList_fromMaterialList(Material **matlist, int len)
{
	PyObject *list;
	int i;

	list = PyList_New(0);
	if (!matlist) return list;

	for (i = 0; i < len; i++) {
		Material *mat= matlist[i];
		PyObject *ob;
		
		if (mat) {
			ob = DataBlock_fromData(mat);
			PyList_Append(list, ob);
			Py_DECREF(ob); // because Append increfs!
		}			
	}
	return list;
}

DATABLOCK_GET(Materialmodule, material, getMaterialList())

DATABLOCK_NEW(Materialmodule, Material, material_new())

static struct PyMethodDef Materialmodule_methods[] = {
	{"get", Materialmodule_get, METH_VARARGS, Materialmodule_get_doc},
	{"New", Materialmodule_New, METH_VARARGS, Materialmodule_New_doc},
	{NULL, NULL}
};

DATABLOCK_ASSIGN_IPO(Material, material)   

static struct PyMethodDef Material_methods[] = {
	{"clrIpo", Material_clrIpo, METH_VARARGS, Material_clrIpo_doc},
	{"assignIpo", Material_assignIpo, METH_VARARGS, Material_assignIpo_doc},
	{NULL, NULL}
};

#ifdef FUTURE_PYTHON_API

DataBlockProperty Material_Properties[]= {
	{"R",		"r",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"G",		"g",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"B",		"b",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"specR",	"specr",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"specG",	"specg",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"specB",	"specb",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"mirR",	"mirr",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"mirG",	"mirg",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"mirB",	"mirb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"ref",		"ref",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"alpha",	"alpha",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"emit",	"emit",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"amb",		"amb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"spec",	"spec",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"specTransp",	"spectra",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"haloSize",	"hasize",	DBP_TYPE_FLO, 0, 0.0,	10000.0}, 

	{"mode",	"mode",		DBP_TYPE_INT, 0, 0.0,	0.0},    
	{"hard",	"har",		DBP_TYPE_SHO, 0, 1.0,	128.0},  

	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 

	{NULL} 
};

#else

DataBlockProperty Material_Properties[]= {
	{"R",		"r",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"G",		"g",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"B",		"b",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"SpecR",	"specr",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"SpecG",	"specg",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"SpecB",	"specb",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"MirR",	"mirr",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"MirG",	"mirg",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"MirB",	"mirb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"Ref",		"ref",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"Alpha",	"alpha",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"Emit",	"emit",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"Amb",		"amb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"Spec",	"spec",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"SpTra",	"spectra",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"HaSize",	"hasize",	DBP_TYPE_FLO, 0, 0.0,	10000.0}, 

	{"Mode",	"mode",		DBP_TYPE_INT, 0, 0.0,	0.0},  
	{"Hard",	"har",		DBP_TYPE_SHO, 0, 1.0,	128.0},  

	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 

	{NULL} 
};

#endif

/*******************/
/* Lamp Datablocks */

DATABLOCK_GET(Lampmodule, lamp, getLampList())

// DATABLOCK_NEW(Lampmodule, Lamp, lamp_new())

static char Lampmodule_New_doc[] = 
"() - returns new Lamp object"; 
 
PyObject *Lampmodule_New (PyObject *self, PyObject *args) 
{ 
	Lamp *obj; 
	obj = lamp_new(); 
	return DataBlock_fromData(obj); 
} 

#ifdef FUTURE_PYTHON_API

DataBlockProperty Lamp_Properties[]= {
	{"mode",	"mode",			DBP_TYPE_SHO, 0, 0.0,	0.0}, 
	{"type",	"type",			DBP_TYPE_SHO, 0, 0.0,	0.0}, 
	{"R",		"r",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"G",		"g",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"B",		"b",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"energy",	"energy",		DBP_TYPE_FLO, 0, 0.0,	10.0}, 
	{"dist",	"dist",			DBP_TYPE_FLO, 0, 0.01,	5000.0}, 
	{"spotSize",	"spotsize",		DBP_TYPE_FLO, 0, 1.0,	180.0}, 
	{"spotBlend",	"spotblend",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"haloInt",	"haint",		DBP_TYPE_FLO, 0, 0.0,	5.0}, 
	{"quad1",	"att1",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"quad2",	"att2",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"bufferSize",	"bufsize",	DBP_TYPE_SHO, 0, 0.0,	0.0}, 
	{"samples",	"samp",	DBP_TYPE_SHO, 0, 1.0,	16.0}, 
	{"haloStep",	"shadhalostep",	DBP_TYPE_SHO, 0, 0.0,	12.0}, 
	{"clipStart",	"clipsta",	DBP_TYPE_FLO, 0, 0.1,	5000.0}, 
	{"clipEnd",	"clipend",	DBP_TYPE_FLO, 0, 0.1,	5000.0}, 
	{"bias",	"bias",	DBP_TYPE_FLO, 0, 0.01,	5.0}, 
	{"softness",	"soft",	DBP_TYPE_FLO, 0, 1.00,	100.0}, 

	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	
	{NULL}
};
#else

DataBlockProperty Lamp_Properties[]= {
	{"mode",	"mode",			DBP_TYPE_SHO, 0, 0.0,	0.0}, 
	{"type",	"type",			DBP_TYPE_SHO, 0, 0.0,	0.0}, 
	{"R",		"r",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"G",		"g",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"B",		"b",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"Energ",	"energy",		DBP_TYPE_FLO, 0, 0.0,	10.0}, 
	{"Dist",	"dist",			DBP_TYPE_FLO, 0, 0.01,	5000.0}, 
	{"SpotSi",	"spotsize",		DBP_TYPE_FLO, 0, 1.0,	180.0}, 
	{"SpotBl",	"spotblend",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"HaloInt",	"haint",		DBP_TYPE_FLO, 0, 1.0,	5.0}, 
	{"Quad1",	"att1",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"Quad2",	"att2",			DBP_TYPE_FLO, 0, 0.0,	1.0}, 

	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	
	{NULL}
};

#endif


static struct PyMethodDef Lampmodule_methods[] = {
	{"New", Lampmodule_New, METH_VARARGS, Lampmodule_New_doc},
	{"get", Lampmodule_get, METH_VARARGS, Lampmodule_get_doc},
#ifdef CURRENT_PYTHON_API
	{"Get", Lampmodule_get, METH_VARARGS, Lampmodule_get_doc},
#endif
	{NULL, NULL}
};

DATABLOCK_ASSIGN_IPO(Lamp, lamp) // defines Lamp_assignIpo

static struct PyMethodDef Lamp_methods[] = {
	{"clrIpo", Lamp_clrIpo, METH_VARARGS, Lamp_clrIpo_doc},
	{"assignIpo", Lamp_assignIpo, METH_VARARGS, Lamp_assignIpo_doc},
	{NULL, NULL}
};

/********************/
/* World Datablocks */

DATABLOCK_GET(Worldmodule, world, getWorldList() )

#ifdef FUTURE_PYTHON_API

DataBlockProperty World_Properties[]= {

	{"mode", "mode", DBP_TYPE_SHO, 0, 0.0,  0.0},
	{"skyType", "skytype", DBP_TYPE_SHO, 0, 0.0,  0.0},
	{"mistType", "mistype", DBP_TYPE_SHO, 0, 0.0,  0.0},
	{"horR",	"horr",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"horG",	"horg",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"horB",	"horb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"ambR",	"ambr",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"ambG",	"ambg",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"ambB",	"ambb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"zenR",	"zenr",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"zenG",	"zeng",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"zenB",	"zenb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"exposure",	"exposure",	DBP_TYPE_FLO, 0, 0.0,	5.0}, 
	{"mistStart",	"miststa",	DBP_TYPE_FLO, 0, 0.0,	1000.0}, 
	{"mistDepth",	"mistdist",	DBP_TYPE_FLO, 0, 0.0,	1000.0}, 
	{"mistHeight",	"misthi",	DBP_TYPE_FLO, 0, 0.0,	100.0}, 
	{"starDensity",	"stardist",	DBP_TYPE_FLO, 0, 2.0,	1000.0}, 
	{"starMinDist",	"starmindist",	DBP_TYPE_FLO, 0, 0.0,	1000.0}, 
	{"starSize",	"starsize",	DBP_TYPE_FLO, 0, 0.0,	10.0}, 
	{"starColNoise",	"starcolsize",	DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"gravity",	"gravity",	DBP_TYPE_FLO, 0, 0.0,	25.0}, 

	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	
	{NULL}
};

#else

DataBlockProperty World_Properties[]= {
	{"HorR",	"horr",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"HorG",	"horg",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"HorB",	"horb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"ZenR",	"zenr",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"ZenG",	"zeng",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"ZenB",	"zenb",		DBP_TYPE_FLO, 0, 0.0,	1.0}, 
	{"Expos",	"exposure",	DBP_TYPE_FLO, 0, 0.0,	5.0}, 
	{"MisSta",	"miststa",	DBP_TYPE_FLO, 0, 0.0,	1000.0}, 
	{"MisDi",	"mistdist",	DBP_TYPE_FLO, 0, 0.0,	1000.0}, 
	{"MisHi",	"misthi",	DBP_TYPE_FLO, 0, 0.0,	100.0}, 
	{"StarDi",	"stardist",	DBP_TYPE_FLO, 0, 2.0,	1000.0}, 
	{"StarSi",	"starsize",	DBP_TYPE_FLO, 0, 0.0,	10.0}, 

	{"ipo",		"*ipo",		DBP_TYPE_FUN, 0, 0.0,	0.0, {0}, {0}, 0, 0, get_DataBlock_func}, 
	
	{NULL}
};

#endif

static char Worldmodule_getActive_doc[]="() - Returns the active world";
static PyObject *Worldmodule_getActive (PyObject *self, PyObject *args)
{
	if (scene_getCurrent()->world) 
		return DataBlock_fromData(scene_getCurrent()->world);
	else
		return BPY_incr_ret(Py_None);	
}

static struct PyMethodDef Worldmodule_methods[] = {
	// these for compatibility...
	{"get",			Worldmodule_get,			METH_VARARGS, Worldmodule_get_doc},
#ifdef CURRENT_PYTHON_API
	{"Get",			Worldmodule_get,			METH_VARARGS, Worldmodule_get_doc},
#endif
	{"getCurrent",	Worldmodule_getActive,	METH_VARARGS, Worldmodule_getActive_doc},
	{NULL, NULL}
};



/* XXX these should go elsewhere */

PyObject *BPY_PyList_FromIDList(ListBase *list, DBConvertfunc convertfunc)
{
	PyObject *pylist= PyList_New(BLI_countlist(list));
	ID *id = list->first;

	int i=0;

	while (id) {
		PyObject *ob= convertfunc(id);
		
		if (!ob) {
			Py_DECREF(pylist);
			return NULL;
		}
		PyList_SetItem(pylist, i, ob);	
		id = id->next; i++;
	}
	return pylist;
}


PyObject *py_find_from_list(ListBase *list, PyObject *args) {
	char *name= NULL;
	ID *id = list->first;
	BPY_TRY(PyArg_ParseTuple(args, "|s", &name));
	
	if (name) {
		while (id) {
			if (strcmp(name, getIDName(id))==0) 
				return DataBlock_fromData(id);

			id= id->next;
		}
		return BPY_incr_ret(Py_None);
		
	} else 
		return BPY_PyList_FromIDList(list, DataBlock_fromData);
}

PyObject *named_enum_get(int val, NamedEnum *enums) {
	while (enums->name) {
		if (enums->num == val) return PyString_FromString(enums->name);
		enums++;
	}	
	PyErr_SetString(PyExc_AttributeError, "Internal error, Unknown enumerated type");
	return NULL;
}

int named_enum_set(char *name, NamedEnum *enums) {
	while (enums->name) {
		if (STREQ(enums->name, name)) 
			return enums->num;
		enums++;
	}	
	
	return -1;
}

static int calc_offset_subsize(int *dlist, int *idx, int *subsize) {
	int n= *dlist;
	
	if (n<=0) {
		*subsize= -n;
		return 0;
	} else {
		int ss;
		int off= calc_offset_subsize(dlist+1, idx+1, &ss);
			
		*subsize= n*ss;
		return off + (*idx)*ss;
	}
}

static int calc_offset(int *dlist, int *idx) {
	int subsize;
	return calc_offset_subsize(dlist, idx, &subsize);
}

static void *get_db_ptr(DataBlockProperty *prop, char *structname, void *struct_ptr) {
	int offset= BLO_findstruct_offset(structname, prop->struct_name);
	void *ptr= struct_ptr;
	
	if (offset==-1) {
		BPY_warn(("Internal error, Invalid prop entry\n"));
		return NULL;
	}

	ptr= (void *) (((char *)ptr) + offset);
	
	offset= calc_offset(prop->dlist, prop->idx);	
	ptr= (void *) (((char *)ptr) + offset);
	
	return ptr;
}

PyObject *datablock_getattr(DataBlockProperty *props, char *structname, char *name, void *struct_ptr) {	
	if (STREQ(name, "properties") || STREQ(name, "__members__")) {
		PyObject *l= PyList_New(0);
		DataBlockProperty *p= props;
		
		while (p->public_name) {
			PyList_Append(l, PyString_FromString(p->public_name));
			p++;
		}
		
		return l;
	}
	
	while (props->public_name) {
		if (STREQ(name, props->public_name)) {
			void *ptr = struct_ptr;
			int val;
			DBPtrToObFP conv_fp;

			if (props->handling==DBP_HANDLING_NONE || 
					props->handling==DBP_HANDLING_NENM) {
				ptr= get_db_ptr(props, structname, struct_ptr);
				if (!ptr) return NULL;
				
			} else if (props->handling==DBP_HANDLING_FUNC) {
				DBGetPtrFP fp= (DBGetPtrFP) props->extra1;
				ptr= fp(struct_ptr, props->struct_name, 0);
				if (!ptr) return NULL;
			}			

			switch(props->type) {
			case DBP_TYPE_CHA:
				val= *((char	*)ptr);
				if (props->handling==DBP_HANDLING_NENM) 
					return named_enum_get(val, props->extra1);				
				else
					return PyInt_FromLong(val);
			case DBP_TYPE_SHO:
				val= *((short	*)ptr);
				if (props->handling==DBP_HANDLING_NENM) 
					return named_enum_get(val, props->extra1);				
				else
					return PyInt_FromLong(val);
			case DBP_TYPE_INT:
				val= *((int	*)ptr);
				if (props->handling==DBP_HANDLING_NENM) 
					return named_enum_get(val, props->extra1);				
				else
					return PyInt_FromLong(val);
			case DBP_TYPE_FLO:
				return PyFloat_FromDouble	( *((float	*)ptr) );
			case DBP_TYPE_VEC:
				return newVectorObject		( ((float	*)ptr), (int) props->min );
			case DBP_TYPE_FUN:
				conv_fp= (DBPtrToObFP) props->extra2;
				return conv_fp( ptr );
			default:
				PyErr_SetString(PyExc_AttributeError, "Internal error, Unknown prop type");
				return NULL;
			}
		}
		
		props++;
	}
	
	PyErr_SetString(PyExc_AttributeError, name);
	return NULL;		
}

int datablock_setattr(DataBlockProperty *props, char *structname, char *name, void *struct_ptr, PyObject *setto) {

	while (props->public_name) {
		if (STREQ(props->public_name, name)) {
			void *ptr = NULL;
			int type;
			DBSetPtrFP conv_fp;
			int clamp= props->min!=props->max;

			int enum_val= -1;
			char	cha_data;
			short	sho_data;
			int		int_data;
			float	flo_data;

			type= props->stype;
			if (type==DBP_TYPE_NON) type= props->type;

			if (props->handling==DBP_HANDLING_NONE) {
				ptr= get_db_ptr(props, structname, struct_ptr);
				if (!ptr) return 0;
				
			} else if (props->handling==DBP_HANDLING_FUNC) {
				if (type!=DBP_TYPE_FUN) {
					DBGetPtrFP fp= (DBGetPtrFP) props->extra1;
					ptr= fp(struct_ptr, props->struct_name, 1);
					if (!ptr) return 0;
				}
			} else if (props->handling==DBP_HANDLING_NENM) {
				char *str;
				if (!PyArg_Parse(setto, "s", &str)) return -1;

				ptr= get_db_ptr(props, structname, struct_ptr);
				if (!ptr) return 0;
				
				enum_val= named_enum_set(str, props->extra1);
				if (enum_val==-1)
					return py_err_ret_int(PyExc_AttributeError, "invalid setting for field");
			}

			switch(type) {
			case DBP_TYPE_CHA:
				if (enum_val==-1) {
					if (!PyArg_Parse(setto, "b", &cha_data)) return -1;
				} else cha_data= (char) enum_val;
				
				if (clamp) {
					CLAMP(cha_data, (char) props->min, (char) props->max);
				}	
				*((char		*)ptr)= cha_data;
				return 0;
			case DBP_TYPE_SHO:
				if (enum_val==-1) {
					if (!PyArg_Parse(setto, "h", &sho_data)) return -1;
				} else sho_data= (short) enum_val;

				if (clamp) {
					CLAMP(sho_data, (short) props->min, (short) props->max);
				}
				*((short	*)ptr)= sho_data;
				return 0;
			case DBP_TYPE_INT:
				if (enum_val==-1) {
					if (!PyArg_Parse(setto, "i", &int_data)) return -1;
				} else int_data= (int) enum_val;

				if (clamp) {
					CLAMP(int_data, (int) props->min, (int) props->max);
				}	
				*((int		*)ptr)= int_data;
				return 0;
			case DBP_TYPE_FLO:
				if (!PyArg_Parse(setto, "f", &flo_data)) return -1;
				if (clamp) {
					CLAMP(flo_data, (float) props->min, (float) props->max);
				}		
				*((float	*)ptr)= flo_data;
				return 0;
			case DBP_TYPE_VEC:
			/* this is very dangerous!! TYPE_VEC also can contain non floats; see
			 * ipo curve attribute h1t, etc. */
				if (props->min == 3.0 ) {  // vector triple
					return BPY_parsefloatvector(setto, (float *) ptr, 3);
				} else {
					return py_err_ret_int(PyExc_AttributeError, "cannot directly assign, use slice assignment instead");
				}
				return 0;
					
			case DBP_TYPE_FUN:
				conv_fp= (DBSetPtrFP) props->extra3;
				if (conv_fp)
					return conv_fp( struct_ptr, props->struct_name, setto );
				else
					return py_err_ret_int(PyExc_AttributeError, "cannot directly assign to item");
			default:
				PyErr_SetString(PyExc_AttributeError, "Internal error, Unknown prop type");
				return -1;
			}
		}
		
		props++;
	}

	PyErr_SetString(PyExc_AttributeError, name);
	return -1;
}

PyObject *datablock_assignIpo(DataBlock *block, DataBlock *ipoblock)
{
	Ipo **ipoptr;
	Ipo *ipo;

	if (!DataBlock_isType(ipoblock, ID_IP)) {
		PyErr_SetString(PyExc_TypeError, "expects Ipo object");
		return 0;
	}

	ipo = PYBLOCK_AS_IPO(ipoblock);

	if (DataBlock_type(block) != ipo->blocktype) {
		PyErr_SetString(PyExc_TypeError, "Ipo type does not match object type!");
		return 0;
	}

	ipoptr = get_db_ptr(block->properties, "ipo", block->data);
	if (!ipoptr) {
		PyErr_SetString(PyExc_RuntimeError, "Object does not have an ipo!");
		return 0;
	}

	*ipoptr = ipo;
	Py_INCREF(Py_None);
	return Py_None;	
}

/* deallocates a Python Datablock object */
void DataBlock_dealloc(DataBlock *self) 
{
#ifdef REF_USERCOUNT
	BOB_XDECUSER(DATABLOCK_ID(self)); // XXX abuse for ref count
#endif
	PyMem_DEL(self);
}

PyObject *DataBlock_repr(DataBlock *self) 
{
	static char s[256];
	if (self->data) 
		sprintf (s, "[%.32s %.32s]", self->type, getIDName((ID*)self->data));
	else
		sprintf (s, "[%.32s %.32s]", self->type, "<deleted>");
	return Py_BuildValue("s", s);
}

/* ************************************************************************* */
/* datablock linking */

/** Link data to Object */

static PyObject *link_Data_toObject(DataBlock *objectblk, DataBlock *datablk)
{
	Object *object = PYBLOCK_AS_OBJECT(objectblk);

	void *data = datablk->data;
	if (!object_linkdata(object, data))
	{
		PyErr_SetString(PyExc_TypeError,
		"Object type different from Data type or linking for this type\
 not supported");
		return NULL;
	}	
	Py_INCREF(Py_None);	
	return Py_None;
}


#ifdef USE_NMESH
/** Special function to link NMesh data to an Object */
static PyObject *link_NMesh_toObject(DataBlock *objectblk, NMesh *nmesh)
{
	int retval;
	Mesh *mesh = nmesh->mesh;
	Object *obj = PYBLOCK_AS_OBJECT(objectblk);

	// if mesh was not created yet (mesh == 0), then do so:
	if (!mesh) {
		mesh = Mesh_fromNMesh(nmesh); // create and convert data
		nmesh->mesh = mesh;
		nmesh_updateMaterials(nmesh);
	}

	retval = object_linkdata(obj, mesh);
	if (!retval) {
		PyErr_SetString(PyExc_RuntimeError, "failed to link NMesh data");
		if (!mesh)
			printf("mesh data was null\n"); // XXX
		return NULL;
	}
	synchronizeMaterialLists(obj, obj->data);
	return Py_BuildValue("i", retval);
}

#endif

/** This is the generic function for linking objects with each other.
  * It can be called on any DataBlock, as long as this makes sense.
  * Example:
  * 
  * from Blender import Object, Scene, NMesh
  * ob = Object.get("Plane")
  * scene = Scene.get("2")
  * ob.link(scene)
  *
  *    or
  *
  *	nmesh = NMesh.GetRaw('Mesh')
  * ob.link(nmesh)  # instanciate mesh
  *
  */

static char DataBlock_link_doc[]=
"(object) - Links 'self' with the specified object.\n\
Only the following object types can be linked to each other:\n\
	Scene  -> Object\n\
    Object -> Data (Mesh, Curve, etc.)\n\
    Object -> Materials: [Material1, Material2, ...]\n\
\n\
The order of linking does not matter, i.e. the following both expressions\n\
are valid:\n\
\n\
    scene.link(object)\n\
\n\
    object.link(scene)\n\
";
	
PyObject *DataBlock_link(PyObject *self, PyObject *args)
{
	DataBlock *blockA= (DataBlock*) self;
	PyObject *with;
	DataBlock *blockB;

#ifdef USE_NMESH
	BPY_TRY(PyArg_ParseTuple(args, "O", &with));
	
	blockB = (DataBlock *) with;
#else	
	BPY_TRY(PyArg_ParseTuple(args, "O!", &DataBlock_Type, &blockB));
#endif	

	switch (DataBlock_type(blockA)) {
		case ID_OB:
	// NMesh is no datablock object, so needs special treatment:
#ifdef USE_NMESH
			if (NMesh_Check(with)) {
				return link_NMesh_toObject(blockA, (NMesh *) with);
			}	
#endif	
			if (!DataBlock_Check(blockB)) {
				PyErr_SetString(PyExc_TypeError, "Argument must be a DataBlock object!");
				return NULL;
			}	
			return link_Data_toObject(blockA, blockB);

		default:
			PyErr_SetString(PyExc_TypeError, "FATAL: implementation error, illegal link method");
			return NULL;
	}
}

/* unlinking currently disabled, but might me needed later
   for other object types...

static char DataBlock_unlink_doc[]=
"(object) - unlinks 'self' from the specified object.\n\
See documentation for link() for valid object types.";

static PyObject *DataBlock_unlink(PyObject *self, PyObject *args)
{
	DataBlock *blockA= (DataBlock*) self;
	DataBlock *blockB;

	BPY_TRY(PyArg_ParseTuple(args, "O!", &DataBlock_Type, &blockB));
	switch (DataBlock_type(blockA)) {
		case ID_SCE:
			switch(DataBlock_type(blockB)) {
				case ID_OB:
					return unlink_Object_fromScene(blockA, blockB);
				default:
					PyErr_SetString(PyExc_TypeError, "Scene unlink: invalid Object type");
					return NULL;
			}
		default:
			PyErr_SetString(PyExc_TypeError, "cannot unlink: invalid object type");
			return NULL;

	}

}
*/

/** These are the methods common to each datablock */

static struct PyMethodDef commonDataBlock_methods[] = {
	{"link", DataBlock_link, METH_VARARGS, DataBlock_link_doc},
//	{"unlink", DataBlock_unlink, METH_VARARGS, DataBlock_unlink_doc},
	{NULL}
};

PyObject *DataBlock_getattr(PyObject *self, char *name) {
	DataBlock *block= (DataBlock*) self;
	PyObject *ret = NULL;
	CHECK_VALIDDATA(block, "block was deleted!")

	// Check for common attributes: 
	if (STREQ(name, "name"))
		return PyString_FromString((((ID*)block->data)->name)+2);
	else if (STREQ(name, "block_type"))
		return PyString_FromString(block->type);
	else if (STREQ(name, "users"))
		return PyInt_FromLong(((ID*)block->data)->us);
	
	//
	// the following datablock types have methods:
	switch (DataBlock_type(block)) {
	case ID_OB:
		ret = Py_FindMethod(Object_methods, self, name);
		break;
	case ID_IP:
		ret = Py_FindMethod(Ipo_methods, self, name);
		break;
	case ID_CA:
		ret = Py_FindMethod(Camera_methods, self, name);
		break;
	case ID_MA:
		ret = Py_FindMethod(Material_methods, self, name);
		break;
	case ID_LA:
		ret = Py_FindMethod(Lamp_methods, self, name);
		break;
	case ID_TXT:
		ret = Py_FindMethod(Text_methods, self, name);
		break;
	}	
	if (ret) return ret;
	PyErr_Clear(); // no method found, clear error

	// try common datablock methods
	ret = Py_FindMethod(commonDataBlock_methods, (PyObject*)self, name);
	if (ret) return ret;

	PyErr_Clear();

	// try attributes from property list
	ret = datablock_getattr(block->properties, block->type, name, block->data);
	return ret;
}

int DataBlock_setattr(PyObject *self, char *name, PyObject *ob) {
	DataBlock *block= (DataBlock*) self;

	CHECK_VALIDDATA(block, "block was deleted!")

	if (STREQ(name, "name")) {
		if (!PyArg_Parse(ob, "s", &name)) return -1;

		new_id(block->type_list, (ID*)block->data, name);
		
		return 0;
	}
	return datablock_setattr(block->properties, block->type, name, block->data, ob);
}


PyTypeObject DataBlock_Type = {
	PyObject_HEAD_INIT(NULL)
	0,									/*ob_size*/
	"Block",							/*tp_name*/
	sizeof(DataBlock),					/*tp_basicsize*/
	0,									/*tp_itemsize*/
	(destructor)	DataBlock_dealloc,	/*tp_dealloc*/
	(printfunc)		0,		/*tp_print*/
	(getattrfunc)	DataBlock_getattr,	/*tp_getattr*/
	(setattrfunc)	DataBlock_setattr,	/*tp_setattr*/
	(cmpfunc) 0,	/*tp_compare*/
	(reprfunc)      DataBlock_repr,		/*tp_repr*/
};

/**************************************************************************/

/**********************/
/* Texture Datablocks */
/*
DATABLOCK_GET(Texturemodule, texture, getTextureList())

static struct PyMethodDef Texture_methods[] = {
	{"Get", Texture_Get, 1, Texture_Get_doc},
	{NULL, NULL}
};
*/



/* ---------------------------------------------------------------------- */

int DataBlock_type(DataBlock *block)
{
	return (GET_ID_TYPE((ID *) block->data));
}

int ObjectDataIDType(DataBlock *block)
{
	Object *ob;
	if (!DataBlock_isType(block, ID_OB))
		return -1;

	ob = (Object *) block->data;
	return GET_ID_TYPE((ID *) ob->data);
}

int DataBlock_isType(DataBlock *block, int type)
{
	ID *id;

	if (!DataBlock_Check(block)) return 0;
	id= (ID *) block->data;
	return (GET_ID_TYPE(id))==type;
}

/** This function creates a Python datablock descriptor object from
  * the specified data pointer. This pointer must point to a structure
  * with a valid ID header.
  */

PyObject *DataBlock_fromData(void *data) {
	DataBlock *newb;
	ID *id= (ID *) data;
	int idn;
	
	if (!data) return BPY_incr_ret(Py_None);

	idn = GET_ID_TYPE(id);
	
	if (idn==ID_OB) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Object";
		newb->type_list= getObjectList();
		newb->properties= Object_Properties;

	} else if (idn==ID_ME) {
#ifdef USE_NMESH	
		return newNMesh(data);
#else
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Mesh";
		newb->type_list= getMeshList();
		newb->properties= Mesh_Properties;
#endif

//	} else if (idn==ID_CU) {
		/* Special case, should be fixed
		 * by proper high-level NURBS access.
		 * 
		 * Later.
		 */
		 
//		return newNCurveObject(data);			

	} else if (idn==ID_LA) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Lamp";
		newb->type_list= getLampList();
		newb->properties= Lamp_Properties;

	} else if (idn==ID_CA) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Camera";
		newb->type_list= getCameraList();
		newb->properties= Camera_Properties;

	} else if (idn==ID_MA) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Material";
		newb->type_list= getMaterialList();
		newb->properties= Material_Properties;
		
	} else if (idn==ID_WO) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "World";
		newb->type_list= getWorldList();
		newb->properties= World_Properties;

	} else if (idn==ID_IP) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Ipo";
		newb->type_list= getIpoList();
		newb->properties= Ipo_Properties;		

#ifdef EXPERIMENTAL
	} else if (idn==ID_TE) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Tex";
		newb->type_list= getTextureList();
		newb->properties= Texture_Properties;		
#endif

	} else if (idn==ID_IM) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Image";
		newb->type_list= getImageList();
		newb->properties= Image_Properties;		

	} else if (idn==ID_TXT) {
		newb= PyObject_NEW(DataBlock, &DataBlock_Type);
		newb->type= "Text";
		newb->type_list= getTextList();
		newb->properties= Text_Properties;		
	} else return BPY_err_ret_ob(PyExc_SystemError, "unable to create Block for data");
	
	newb->data= data;
#ifdef REF_USERCOUNT
	BOB_INCUSER(id); // XXX abuse for refcount
#endif
	
	return (PyObject *) newb;
}

PyObject *get_DataBlock_func(void **ptr) {
	ID *id= (ID*) *ptr;
	return DataBlock_fromData(id);
}

/* ---------------------------------------------------------------------- */
/* INIT ROUTINE */


void init_types(PyObject *dict)
{
	PyObject *tmod, *tdict;

	tmod= Py_InitModule("Blender.Types", Null_methods);
	PyDict_SetItemString(dict, "Types", tmod);
	
	tdict= PyModule_GetDict(tmod);

	PyDict_SetItemString(tdict, "IpoCurve", (PyObject *)&PyIpoCurve_Type);
	PyDict_SetItemString(tdict, "BezTriple", (PyObject *)&PyBezTriple_Type);

	PyDict_SetItemString(tdict, "ButtonType", (PyObject *)&Button_Type);
	PyDict_SetItemString(tdict, "BufferType", (PyObject *)&Buffer_Type);
	PyDict_SetItemString(tdict, "NMeshType", (PyObject *)&NMesh_Type);
	PyDict_SetItemString(tdict, "NMFaceType", (PyObject *)&NMFace_Type);
	PyDict_SetItemString(tdict, "NMVertType", (PyObject *)&NMVert_Type);
	PyDict_SetItemString(tdict, "NMColType", (PyObject *)&NMCol_Type);

	PyDict_SetItemString(tdict, "BlockType", (PyObject *)&DataBlock_Type);
	
	/* Setup external types */
	PyDict_SetItemString(tdict, "VectorType", (PyObject *)&Vector_Type);
	PyDict_SetItemString(tdict, "MatrixType", (PyObject *)&Matrix_Type);
}

#undef BPY_ADDCONST
#define BPY_ADDCONST(dict, name) insertConst(dict, #name, PyInt_FromLong(LA_##name))

PyObject *initLamp(void)
{
	PyObject *mod, *dict, *d;

	mod= Py_InitModule(MODNAME(BLENDERMODULE) ".Lamp", Lampmodule_methods);
	dict= PyModule_GetDict(mod);
	d = ConstObject_New();
	PyDict_SetItemString(dict, "Types", d);

	/* type */
	BPY_ADDCONST(d, LOCAL);
	BPY_ADDCONST(d, SUN);
	BPY_ADDCONST(d, SPOT);
	BPY_ADDCONST(d, HEMI);

	d = ConstObject_New();
	PyDict_SetItemString(dict, "Modes", d);

	/* mode */
	BPY_ADDCONST(d, SHAD);
	BPY_ADDCONST(d, HALO);
	BPY_ADDCONST(d, LAYER);
	BPY_ADDCONST(d, QUAD);
	BPY_ADDCONST(d, NEG);
	BPY_ADDCONST(d, ONLYSHADOW);
	BPY_ADDCONST(d, SPHERE);
	BPY_ADDCONST(d, SQUARE);
	BPY_ADDCONST(d, TEXTURE);
	BPY_ADDCONST(d, OSATEX);
	BPY_ADDCONST(d, DEEP_SHADOW);

	return mod;
}

PyObject *initMaterial(void)
{
	PyObject *mod, *dict, *d;

	mod= Py_InitModule(MODNAME(BLENDERMODULE) ".Material", 
	                   Materialmodule_methods);
	dict= PyModule_GetDict(mod);
	d = ConstObject_New();
	PyDict_SetItemString(dict, "Modes", d);

	/* MATERIAL MODES 
	 * ...some of these have really cryptic defines :-) 
	 * We try to match them to the GUI descriptions...  */

#undef BPY_ADDCONST
#define BPY_ADDCONST(dict, name) \
	insertConst(dict, #name, PyInt_FromLong(MA_##name))

	insertConst(d, "TRACEABLE", PyInt_FromLong(MA_TRACEBLE));
	BPY_ADDCONST(d, SHADOW);
	insertConst(d, "SHADELESS", PyInt_FromLong(MA_SHLESS));
	BPY_ADDCONST(d, WIRE);
	insertConst(d, "VCOL_LIGHT", PyInt_FromLong(MA_VERTEXCOL));
	BPY_ADDCONST(d, HALO);
	insertConst(d, "ZTRANSP", PyInt_FromLong(MA_ZTRA));
	insertConst(d, "VCOL_PAINT", PyInt_FromLong(MA_VERTEXCOLP));
	insertConst(d, "ZINVERT", PyInt_FromLong(MA_ZINV));
	BPY_ADDCONST(d, ONLYSHADOW);
	BPY_ADDCONST(d, STAR);
	insertConst(d, "TEXFACE", PyInt_FromLong(MA_FACETEXTURE));
	BPY_ADDCONST(d, NOMIST);

	/* HALO MODES */
	d = ConstObject_New();
	PyDict_SetItemString(dict, "HaloModes", d);

#undef BPY_ADDCONST
#define BPY_ADDCONST(dict, name) \
	insertConst(dict, #name, PyInt_FromLong(MA_HALO_##name))

	BPY_ADDCONST(d, RINGS);
	BPY_ADDCONST(d, LINES);
	insertConst(d, "TEX", PyInt_FromLong(MA_HALOTEX));
	insertConst(d, "PUNO", PyInt_FromLong(MA_HALOPUNO));
	BPY_ADDCONST(d, SHADE);
	BPY_ADDCONST(d, FLARE);

	return mod;
}

void init_Datablockmodules(PyObject *dict) {
#define MODLOAD(name)	PyDict_SetItemString(dict, #name, Py_InitModule(MODNAME(BLENDERMODULE) "." #name, name##module_methods))

	DataBlock_Type.ob_type = &PyType_Type;
	PyIpoCurve_Type.ob_type= &PyType_Type;
	PyBezTriple_Type.ob_type= &PyType_Type;

	PyDict_SetItemString(dict, "Object", initObject());
	PyDict_SetItemString(dict, "Lamp", initLamp());
	PyDict_SetItemString(dict, "Material", initMaterial());
	PyDict_SetItemString(dict, "Ipo", initIpo());
	PyDict_SetItemString(dict, "Scene", initScene());
	MODLOAD(Text);
//	MODLOAD(Mesh);
	MODLOAD(Camera);
	MODLOAD(World);
	MODLOAD(Image);
/* 	MODLOAD(Texture); */
}
