/** Scene module; access to Scene objects in Blender
  *
  * Scene objects are no longer DataBlock objects, but referred
  * by name. This makes it a little slower, but safer - Scene properties
  * can no longer be accessed after a Scene was deleted.
  *
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

#include "BKE_scene.h"
#include "BIF_drawscene.h"

#include "BSE_headerbuttons.h"

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "opy_datablock.h"
#include "b_interface.h"

#include "BPY_macros.h"
#include "BPY_window.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* DEFINES */

#define CHECK_VALIDSCENE(x) CHECK_VALIDDATA(x, \
	"Scene was deleted!")

#define PyScene_AsScene(x) \
	getSceneByName(((PyScene *) x)->name)

/* PROTOS */

PyObject *PyScene_FromScene(Scene *scene);

/************************/
/* Helper routines      */

/* returns a python list of the objects of the Base 'base' */

static PyObject *objectlist_from_base(Base *base)
{
	PyObject *pylist= PyList_New(0);
	PyObject *b;

	while (base) {	
		b = (PyObject *) DataBlock_fromData(base->object);	
		PyList_Append(pylist, b);	
		Py_XDECREF(b);             // because PyList_Append increfs!
		base = base->next;
	}	
	return pylist;
}	

/* Scene object */

typedef struct {
	PyObject_VAR_HEAD
 	char name[32];
} PyScene;

static PyObject *newPyScene(char *name);

/** Returns scene by name. Can be NULL if not found */
Scene *getSceneByName(char *name)
{
	return (Scene *) getFromList(getSceneList(), name);
}

/************************/
/* Scene object methods */

static char Scene_getChildren_doc[] =
"() - returns list of scene children objects";

static PyObject *Scene_getChildren(PyObject *self, PyObject *args)
{
	Scene *scene = PyScene_AsScene(self);

	CHECK_VALIDSCENE(scene)

	BPY_TRY(PyArg_ParseTuple(args, ""));
	return objectlist_from_base(scene->base.first);
}

static char Scene_getCurrentCamera_doc[] =
"() - returns current active camera";

static PyObject *Scene_getCurrentCamera(PyObject *self, PyObject *args)
{
	Scene *scene = PyScene_AsScene(self);
	Object *object;

	CHECK_VALIDSCENE(scene)

	object = scene->camera;
	if (!object) {
		Py_INCREF(Py_None);	
		return Py_None;
	}
	return DataBlock_fromData(object);
}

static char Scene_setCurrentCamera_doc[] =
"(camera) - sets current active camera. 'camera' must be a valid camera\n\
Object";

static PyObject *Scene_setCurrentCamera(PyObject *self, PyObject *args)
{
	Scene *scene = PyScene_AsScene(self);
	Object *object;
	DataBlock *block;

	CHECK_VALIDSCENE(scene)

	BPY_TRY(PyArg_ParseTuple(args, "O!", &DataBlock_Type, &block));
	if (!DataBlock_isType(block, ID_OB)) {
		PyErr_SetString(PyExc_TypeError, "Object type expected!");
		return NULL;
	}	

	object = PYBLOCK_AS_OBJECT(block);

	scene->camera = object;
	if (scene_getCurrent() == scene) // update current scene display
		window_update_curCamera(object);

	Py_INCREF(Py_None);	
	return Py_None;
}

#define SCENE_GETDIR(name, elem) \
static PyObject *Scene_get##name(PyObject *self, PyObject *args) \
{ \
	Scene *scene = PyScene_AsScene(self); \
	CHECK_VALIDSCENE(scene) \
	return PyString_FromString(scene->elem); \
} \

static char Scene_getRenderdir_doc[] =
"() - returns directory where rendered images are saved to";

SCENE_GETDIR(Renderdir, r.pic)

static char Scene_getBackbufdir_doc[] =
"() - returns the Backbuffer images location";

SCENE_GETDIR(Backbufdir, r.backbuf)

#define INVALID_FRAME -99999

static char Scene_frameSettings_doc[] = 
"(start, end, current) - sets the scene's frame settings:\n\
    start  : start frame\n\
	end    : end frame\n\
	current: current frame\n\
If a frame value is negative, it is not set.\n\
\n\
Return value: the current frame settings (start, end, current)";

static PyObject *Scene_frameSettings(PyObject *self, PyObject *args)
{
	RenderData *rd = 0;
	int current = INVALID_FRAME;
	int start = INVALID_FRAME;
	int end = INVALID_FRAME;
	Scene *scene = PyScene_AsScene(self);

	CHECK_VALIDSCENE(scene)

	rd = &scene->r;

	BPY_TRY(PyArg_ParseTuple(args, "|iii", &start, &end, &current));
	if (start > 0) {
		rd->sfra = start;
	}	
	if (end > 0) {
		rd->efra = end;
	}	
	if (current > 0) {
		rd->cfra = current;
	}	
	return Py_BuildValue("(iii)", rd->sfra, rd->efra, rd->cfra);
}


static char Scene_makeCurrent_doc[] =
"() - makes Scene the current Scene";

static PyObject *Scene_makeCurrent(PyObject *self, PyObject *args)
{
	Scene *scene = PyScene_AsScene(self);
	
	CHECK_VALIDSCENE(scene)

	set_scene(scene);
	Py_INCREF(Py_None);	
	return Py_None;
}

static char Scene_copy_doc[] =
"(duplicate_objects = 1) - make a copy of a scene\n\
'The optional argument defines, how the scene's children objects are\n\
duplicated:\n\
\n\
0: Link Objects\n\
1: Link Object data\n\
2: Full Copy";

static PyObject *Scene_copy(PyObject *self, PyObject *args)
{
	Scene *scene = PyScene_AsScene(self);

	int dup_objects = 0;

	CHECK_VALIDSCENE(scene)

	BPY_TRY(PyArg_ParseTuple(args, "|i", &dup_objects));

	return PyScene_FromScene(copy_scene(scene, dup_objects));
}

static char Scene_update_doc[]= "() - Update scene\n\
This function explicitely resorts the base list of a newly created object\n\
hierarchy.";

static PyObject *Scene_update(PyObject *self, PyObject *args)
{
	Scene *scene = PyScene_AsScene(self);

	CHECK_VALIDSCENE(scene)

	BPY_TRY(PyArg_ParseTuple(args, ""));
	sort_baselist(scene);
	Py_INCREF(Py_None);
	return Py_None;	
}

static char Scene_link_doc[]= "(object) - Links object to scene";

/** Links an object with a scene */
static PyObject *Scene_link(PyObject *self, PyObject *args)
{
	DataBlock *block;
	Object *object;
	Scene *scene = PyScene_AsScene(self);
	CHECK_VALIDSCENE(scene)

	BPY_TRY(PyArg_ParseTuple(args, "O!", &DataBlock_Type, &block));
	if (DataBlock_type(block) != ID_OB) {
		PyErr_SetString(PyExc_TypeError, "link: invalid Object type");
		return NULL;
	}
	object = PYBLOCK_AS_OBJECT(block);
	if (!scene_linkObject(scene, object))
	{	
		PyErr_SetString(PyExc_RuntimeError, "Object already in scene!");
		return NULL;
	}	
	Py_INCREF(Py_None);	
	return Py_None;
}

/** unlinks (removes) an object from a scene */

static char Scene_unlink_doc[]= "(object) - Unlinks object from scene";

static PyObject *Scene_unlink(PyObject *self, PyObject *args)
{
	PyObject *retval;
	DataBlock *block;
	Object *object;
	Scene *scene = PyScene_AsScene(self);
	CHECK_VALIDSCENE(scene)

	BPY_TRY(PyArg_ParseTuple(args, "O!", &DataBlock_Type, &block));
	if (DataBlock_type(block) != ID_OB) {
		PyErr_SetString(PyExc_TypeError, "unlink: invalid Object type");
		return NULL;
	}
	object = PYBLOCK_AS_OBJECT(block);

	if (!scene_unlinkObject(scene, object))
		retval = Py_BuildValue("i", 0);
	else	
		retval = Py_BuildValue("i", 1);
	Py_INCREF(retval);	
	return retval;
}

#undef MethodDef
#define MethodDef(func) _MethodDef(func, Scene)

/* these are the scene object methods */
static struct PyMethodDef Scene_methods[] = {
	MethodDef(copy), 
	MethodDef(link), 
	MethodDef(unlink), 
	MethodDef(getChildren),
	MethodDef(getCurrentCamera),
	MethodDef(setCurrentCamera),
	MethodDef(getRenderdir),
	MethodDef(getBackbufdir),
	MethodDef(frameSettings),
	MethodDef(makeCurrent),
	MethodDef(update),
	{NULL, NULL}
};

static void PyScene_dealloc(PyObject *self)
{
	PyMem_DEL(self);
}

static PyObject *PyScene_getattr(PyObject *self, char *attr)
{
	Scene *scene;
	if (!strcmp(attr, "name")) {
		return PyString_FromString(((PyScene *) self)->name);
	} else if (!strcmp(attr, "users")) {
		scene = PyScene_AsScene(self);
		return PyInt_FromLong(getUsers(scene));
	} else if (!strcmp(attr, "block_type")) {
		return Py_BuildValue("s", "Scene");
	}
	return Py_FindMethod(Scene_methods, (PyObject *) self, attr);
}

PyObject *PyScene_repr(PyScene *self) 
{
	char s[256];
	Scene *scene = PyScene_AsScene(self);
	if (scene) 
		sprintf (s, "[Scene %.32s]", getName(scene));
	else
		sprintf (s, "[deleted Scene]");
	return Py_BuildValue("s", s);
}

static PyTypeObject PyScene_Type = {
	PyObject_HEAD_INIT(NULL)
	0,                                /*ob_size*/
	"Scene",                          /*tp_name*/
	sizeof(PyScene),                  /*tp_basicsize*/
	0,                                /*tp_itemsize*/
	/* methods */
	(destructor) PyScene_dealloc,     /*tp_dealloc*/
	(printfunc)0,                     /*tp_print*/
	(getattrfunc)PyScene_getattr,     /*tp_getattr*/
	(setattrfunc)0,                   /*tp_setattr*/
	(cmpfunc)0,                       /*tp_compare*/
	(reprfunc)PyScene_repr,           /*tp_repr*/
	0,                                /*tp_as_number*/
	0,                                /*tp_as_sequence*/
	0,                                /*tp_as_mapping*/
	(hashfunc)0,                      /*tp_hash*/
	(ternaryfunc)0,                   /*tp_call*/
	(reprfunc)0,                      /*tp_str*/

	/* Space for future expansion */
	0L,0L,0L,0L,
	0 /* Documentation string */
};

static PyObject *newPyScene(char *name)
{
	PyScene *scene = PyObject_NEW(PyScene, &PyScene_Type);
	strncpy(scene->name, name, 31);
	return (PyObject *) scene;
}

PyObject *PyScene_FromScene(Scene *scene)
{
	if (scene) 
		return newPyScene(getName(scene));
	else {
		Py_INCREF(Py_None);
		return Py_None;
	}
}

PyObject *PyScene_FromVoid(void *scene)
{
	return PyScene_FromScene((Scene *) scene);
}


/************************/
/* Scene module methods */

static char Scenemodule_get_doc[] =
"(name = None) - get Scene 'name' from Blender, if 'name' specified.\n\
Otherwise, a list of all Scenes is returned";

static PyObject *Scenemodule_get(PyObject *self, PyObject *args)
{
	char *name= NULL;
	BPY_TRY(PyArg_ParseTuple(args, "|s", &name));

	if (name) {
		return PyScene_FromScene(getSceneByName(name));
	} else {
		return BPY_PyList_FromIDList(getSceneList(), PyScene_FromVoid);
	}
}

static char Scenemodule_New_doc[] =
"(name = None) - Create new scene with (optionally given)\n\
name.";

static PyObject *Scenemodule_New(PyObject *self, PyObject *args)
{
	Scene *scene;
	char *name = "Scene";

	BPY_TRY(PyArg_ParseTuple(args, "|s", &name));
	scene = add_scene(name);
	return newPyScene(name);
}

static char Scenemodule_getCurrent_doc[] =
"() - returns currently active Scene";

static PyObject *Scenemodule_getCurrent(PyObject *self, PyObject *args)
{
	return newPyScene(getName(scene_getCurrent()));
}

static char Scenemodule_unlink_doc[] =
"(scene) - deletes the Scene 'scene' from Blender\n\
The Scene must be empty before removing it";

static PyObject *Scenemodule_unlink(PyObject *self, PyObject *args)
{
	PyObject *sceneobj;
	Scene *scene;

	if(!PyArg_ParseTuple(args, "O!", &PyScene_Type, &sceneobj)) {
		PyErr_SetString(PyExc_TypeError, "Scene object expected!");
		return NULL;
	}

	scene = PyScene_AsScene(sceneobj);
	free_libblock(getSceneList(), scene);
	Py_INCREF(Py_None);	
	return Py_None;
}


/*****************/
/* METHOD TABLES */

/* these are the module methods */

#undef MethodDef
#define MethodDef(func) _MethodDef(func, Scenemodule)

struct PyMethodDef Scenemodule_methods[] = {
	MethodDef(get),
	MethodDef(getCurrent),
	MethodDef(New),
	MethodDef(unlink),
	{NULL, NULL}
};

PyObject *initScene()
{
	PyObject *mod;
	PyScene_Type.ob_type = &PyType_Type;
	mod= Py_InitModule(MODNAME(BLENDERMODULE) ".Scene", 
	                   Scenemodule_methods);
	return mod;
}


