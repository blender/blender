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
 * Contributor(s): Willian P. Germano, Jacques Guignot, Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_scene.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <BSE_headerbuttons.h> /* for copy_scene */
#include <BIF_drawscene.h>		 /* for set_scene */
#include <BIF_space.h>				 /* for copy_view3d_lock() */
#include <DNA_scriptlink_types.h>
#include <MEM_guardedalloc.h>  /* for MEM_callocN */
#include <mydevice.h>					 /* for #define REDRAW */

#include "Object.h"
#include "bpy_types.h"
#include "constant.h"
#include "gen_utils.h"
#include "sceneRender.h"

#include "Scene.h"

static Base *EXPP_Scene_getObjectBase (Scene *scene, Object *object);
PyObject *M_Object_Get (PyObject *self, PyObject *args); /* from Object.c */

//----------------------------------- Python BPy_Scene defaults----------------------------------------------------------										
#define EXPP_SCENE_FRAME_MAX 18000
#define EXPP_SCENE_RENDER_WINRESOLUTION_MIN 4
#define EXPP_SCENE_RENDER_WINRESOLUTION_MAX 10000
//-----------------------Python API function prototypes for the Scene module------------------------------------
static PyObject *M_Scene_New (PyObject *self, PyObject *args,	 PyObject *keywords);
static PyObject *M_Scene_Get (PyObject *self, PyObject *args);
static PyObject *M_Scene_GetCurrent (PyObject *self);
static PyObject *M_Scene_Unlink (PyObject *self, PyObject *arg);
//-----------------------Scene module doc strings---------------------------------------------------------------------------
static char M_Scene_doc[] =
"The Blender.Scene submodule";
static char M_Scene_New_doc[] =
"(name = 'Scene') - Create a new Scene called 'name' in Blender.";
static char M_Scene_Get_doc[] =
"(name = None) - Return the scene called 'name'. If 'name' is None, return a list with all Scenes.";
static char M_Scene_GetCurrent_doc[] =
"() - Return the currently active Scene in Blender.";
static char M_Scene_Unlink_doc[] =
"(scene) - Unlink (delete) scene 'Scene' from Blender. (scene) is of type Blender scene.";
//----------------------Scene module method def---------------------------------------------------------------------------
struct PyMethodDef M_Scene_methods[] = {
	{"New",(PyCFunction)M_Scene_New, METH_VARARGS|METH_KEYWORDS,
					M_Scene_New_doc},
	{"Get",					M_Scene_Get,				 METH_VARARGS, M_Scene_Get_doc},
	{"get",					M_Scene_Get,				 METH_VARARGS, M_Scene_Get_doc},
	{"GetCurrent",(PyCFunction)M_Scene_GetCurrent,
														 METH_NOARGS,  M_Scene_GetCurrent_doc},
	{"getCurrent",(PyCFunction)M_Scene_GetCurrent,
														 METH_NOARGS,  M_Scene_GetCurrent_doc},
	{"Unlink",			M_Scene_Unlink,			 METH_VARARGS, M_Scene_Unlink_doc},
	{"unlink",			M_Scene_Unlink,			 METH_VARARGS, M_Scene_Unlink_doc},
	{NULL, NULL, 0, NULL}
};
//-----------------------BPy_Scene  method declarations-----------------------------------------------------------------
static PyObject *Scene_getName(BPy_Scene *self);
static PyObject *Scene_setName(BPy_Scene *self, PyObject *arg);
static PyObject *Scene_copy(BPy_Scene *self, PyObject *arg);
static PyObject *Scene_makeCurrent(BPy_Scene *self);
static PyObject *Scene_update(BPy_Scene *self, PyObject *args);
static PyObject *Scene_link(BPy_Scene *self, PyObject *args);
static PyObject *Scene_unlink(BPy_Scene *self, PyObject *args);
static PyObject *Scene_getChildren(BPy_Scene *self);
static PyObject *Scene_getCurrentCamera(BPy_Scene *self);
static PyObject *Scene_setCurrentCamera(BPy_Scene *self, PyObject *args);
static PyObject *Scene_getRenderingContext(BPy_Scene *self);
static PyObject *Scene_getScriptLinks(BPy_Scene *self, PyObject *args);
static PyObject *Scene_addScriptLink(BPy_Scene *self, PyObject *args);
static PyObject *Scene_clearScriptLinks(BPy_Scene *self);

//deprecated methods
static PyObject *Scene_currentFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_getWinSize(BPy_Scene *self);
static PyObject *Scene_setWinSize(BPy_Scene *self, PyObject *arg);
static PyObject *Scene_startFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_endFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_frameSettings (BPy_Scene *self, PyObject *args);
static PyObject *Scene_getRenderdir(BPy_Scene *self);
static PyObject *Scene_getBackbufdir(BPy_Scene *self);

//internal
static void Scene_dealloc (BPy_Scene *self);
static int Scene_setAttr (BPy_Scene *self, char *name, PyObject *v);
static int Scene_compare (BPy_Scene *a, BPy_Scene *b);
static PyObject *Scene_getAttr (BPy_Scene *self, char *name);
static PyObject *Scene_repr (BPy_Scene *self);
//-----------------------BPy_Scene method def-------------------------------------------------------------------------------
static PyMethodDef BPy_Scene_methods[] = {
 /* name, method, flags, doc */
	{"getName", (PyCFunction)Scene_getName, METH_NOARGS,
			"() - Return Scene name"},
	{"setName", (PyCFunction)Scene_setName, METH_VARARGS,
			"(str) - Change Scene name"},
	{"copy",		(PyCFunction)Scene_copy, METH_VARARGS,
			"(duplicate_objects = 1) - Return a copy of this scene\n"
	"The optional argument duplicate_objects defines how the scene\n"
	"children are duplicated:\n\t0: Link Objects\n\t1: Link Object Data"
	"\n\t2: Full copy\n"},
	{"makeCurrent", (PyCFunction)Scene_makeCurrent, METH_NOARGS,
			"() - Make self the current scene"},
	{"update", (PyCFunction)Scene_update, METH_VARARGS,
			"(full = 0) - Update scene self.\n"
			"full = 0: sort the base list of objects."
			"full = 1: full update -- also regroups, does ipos, ikas, keys"},
	{"link", (PyCFunction)Scene_link, METH_VARARGS,
			"(obj) - Link Object obj to this scene"},
	{"unlink", (PyCFunction)Scene_unlink, METH_VARARGS,
			"(obj) - Unlink Object obj from this scene"},
	{"getChildren", (PyCFunction)Scene_getChildren, METH_NOARGS,
			"() - Return list of all objects linked to scene self"},
	{"getCurrentCamera", (PyCFunction)Scene_getCurrentCamera, METH_NOARGS,
			"() - Return current active Camera"},
	{"getScriptLinks", (PyCFunction)Scene_getScriptLinks, METH_VARARGS,
			"(eventname) - Get a list of this scene's scriptlinks (Text names) "
			"of the given type\n"
	"(eventname) - string: FrameChanged, OnLoad or Redraw."},
	{"addScriptLink", (PyCFunction)Scene_addScriptLink, METH_VARARGS,
			"(text, evt) - Add a new scene scriptlink.\n"
	"(text) - string: an existing Blender Text name;\n"
	"(evt) string: FrameChanged, OnLoad or Redraw."},
	{"clearScriptLinks", (PyCFunction)Scene_clearScriptLinks, METH_NOARGS,
			"() - Delete all scriptlinks from this scene."},
	{"setCurrentCamera", (PyCFunction)Scene_setCurrentCamera, METH_VARARGS,
			"() - Set the currently active Camera"},
	//DEPRECATED
	{"getWinSize", (PyCFunction)Scene_getWinSize, METH_NOARGS,
			"() - Return Render window [x,y] dimensions"},
	{"setWinSize", (PyCFunction)Scene_setWinSize, METH_VARARGS,
			"(str) - Change Render window [x,y] dimensions"},
	{"startFrame", (PyCFunction)Scene_startFrame, METH_VARARGS,
			"(frame) - If frame is given, the start frame is set and"
			"\nreturned in any case"},
	{"endFrame", (PyCFunction)Scene_endFrame, METH_VARARGS,
			"(frame) - If frame is given, the end frame is set and"
			"\nreturned in any case"},
	{"frameSettings", (PyCFunction)Scene_frameSettings, METH_VARARGS,
			"(start, end, current) - Sets or retrieves the Scene's frame"
			" settings.\nIf the frame arguments are specified, they are set. "
			"A tuple (start, end, current) is returned in any case."},
	{"getRenderdir", (PyCFunction)Scene_getRenderdir, METH_NOARGS,
			"() - Return directory where rendered images are saved to"},
	{"getBackbufdir", (PyCFunction)Scene_getBackbufdir, METH_NOARGS,
			"() - Return location of the backbuffer image"},
	{"getRenderingContext", (PyCFunction)Scene_getRenderingContext, METH_NOARGS,
			"() - Get the rendering context for the scene and return it as a BPy_RenderData"},
	{"currentFrame", (PyCFunction)Scene_currentFrame, METH_VARARGS,
			"(frame) - If frame is given, the current frame is set and"
			"\nreturned in any case"},
	{NULL, NULL, 0, NULL}
};
//-----------------------BPy_Scene method def-------------------------------------------------------------------------
PyTypeObject Scene_Type =
{
	PyObject_HEAD_INIT(NULL)
	0,								/* ob_size */
	"Scene",						/* tp_name */
	sizeof (BPy_Scene),				/* tp_basicsize */
	0,								/* tp_itemsize */
	/* methods */
	(destructor)Scene_dealloc,		/* tp_dealloc */
	0,								/* tp_print */
	(getattrfunc)Scene_getAttr,		/* tp_getattr */
	(setattrfunc)Scene_setAttr,		/* tp_setattr */
	(cmpfunc)Scene_compare,			/* tp_compare */
	(reprfunc)Scene_repr,			/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_as_hash */
	0,0,0,0,0,0,
	0,								/* tp_doc */ 
	0,0,0,0,0,0,
	BPy_Scene_methods,				/* tp_methods */
	0,								/* tp_members */
};
//-----------------------Scene module Init())--------------------------------------------------------------------------------
PyObject *Scene_Init (void)
{
	PyObject	*submodule;
 	PyObject *dict;

	Scene_Type.ob_type = &PyType_Type;
 	submodule = Py_InitModule3("Blender.Scene",	M_Scene_methods, M_Scene_doc);

	dict = PyModule_GetDict (submodule);
    PyDict_SetItemString (dict, "Render", Render_Init ());

	return submodule;
}
//-----------------------Scene module internal callbacks------------------------------------------------------------
//-----------------------dealloc-----------------------------------------------------------------------------------------------------
static void Scene_dealloc (BPy_Scene *self)
{
	PyObject_DEL (self);
}
//-----------------------getAttr-----------------------------------------------------------------------------------------------------
static PyObject *Scene_getAttr (BPy_Scene *self, char *name)
{
	PyObject *attr = Py_None;

	if (strcmp(name, "name") == 0)
		attr = PyString_FromString(self->scene->id.name+2);

	else if (strcmp(name, "__members__") == 0)
		attr = Py_BuildValue("[s]", "name");


	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
											"couldn't create PyObject"));

	if (attr != Py_None) return attr; /* member attribute found, return it */

	/* not an attribute, search the methods table */
	return Py_FindMethod(BPy_Scene_methods, (PyObject *)self, name);
}
//-----------------------setAttr-----------------------------------------------------------------------------------------------------
static int Scene_setAttr (BPy_Scene *self, char *name, PyObject *value)
{
	PyObject *valtuple; 
	PyObject *error = NULL;

/* We're playing a trick on the Python API users here.	Even if they use
 * Scene.member = val instead of Scene.setMember(val), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Scene structure when necessary. */

/* First we put "value" in a tuple, because we want to pass it to functions
 * that only accept PyTuples. Using "N" doesn't increment value's ref count */
	valtuple = Py_BuildValue("(O)", value);

	if (!valtuple) /* everything OK with our PyObject? */
		return EXPP_ReturnIntError(PyExc_MemoryError,
												 "SceneSetAttr: couldn't create PyTuple");

/* Now we just compare "name" with all possible BPy_Scene member variables */
	if (strcmp (name, "name") == 0)
		error = Scene_setName (self, valtuple);

	else { /* Error: no member with the given name was found */
		Py_DECREF(valtuple);
		return (EXPP_ReturnIntError (PyExc_AttributeError, name));
	}

/* valtuple won't be returned to the caller, so we need to DECREF it */
	Py_DECREF(valtuple);

	if (error != Py_None) return -1;

/* Py_None was incref'ed by the called Scene_set* function. We probably
 * don't need to decref Py_None (!), but since Python/C API manual tells us
 * to treat it like any other PyObject regarding ref counting ... */
	Py_DECREF(Py_None);
	return 0; /* normal exit */
}
//-----------------------compare-----------------------------------------------------------------------------------------------------
static int Scene_compare (BPy_Scene *a, BPy_Scene *b)
{
	Scene *pa = a->scene, *pb = b->scene;
	return (pa == pb) ? 0:-1;
}
//----------------------repr--------------------------------------------------------------------------------------------------------------
static PyObject *Scene_repr (BPy_Scene *self)
{
	return PyString_FromFormat("[Scene \"%s\"]", self->scene->id.name+2);
}
//-----------------------CreatePyObject----------------------------------------------------------------------------------
PyObject *Scene_CreatePyObject (Scene *scene)
{
	BPy_Scene *pyscene;

	pyscene = (BPy_Scene *)PyObject_NEW (BPy_Scene, &Scene_Type);

	if (!pyscene)
		return EXPP_ReturnPyObjError (PyExc_MemoryError,
						"couldn't create BPy_Scene object");

	pyscene->scene = scene;

	return (PyObject *)pyscene;
}
//-----------------------CheckPyObject----------------------------------------------------------------------------------
int Scene_CheckPyObject (PyObject *pyobj)
{
	return (pyobj->ob_type == &Scene_Type);
}
//-----------------------FromPyObject----------------------------------------------------------------------------------
Scene *Scene_FromPyObject (PyObject *pyobj)
{
	return ((BPy_Scene *)pyobj)->scene;
}
//-----------------------GetSceneByName()--------------------------------------------------------------------------------------------
/* Description: Returns the object with the name specified by the argument	name. 
Note that the calling function has to remove the first two characters of the object name. 
These two characters	specify the type of the object (OB, ME, WO, ...)The function 
will return NULL when no object with the given  name is found.	 */
Scene * GetSceneByName (char * name)
{
	Scene	* scene_iter;

	scene_iter = G.main->scene.first;
	while (scene_iter)
	{
		if (StringEqual (name, GetIdName (&(scene_iter->id))))
		{
			return (scene_iter);
		}
		scene_iter = scene_iter->id.next;
	}

	/* There is no object with the given name */
	return (NULL);
}
//-----------------------EXPP_Scene_getObjectBase()--------------------------------------------------------------------------------------------
Base *EXPP_Scene_getObjectBase(Scene *scene, Object *object)
{
	Base *base = scene->base.first;

	while (base) {

		if (object == base->object) return base; /* found it? */

		base = base->next;
	}

	return NULL; /* object isn't linked to this scene */
}
//-----------------------Scene module function defintions----------------------------------------------------------------------
//-----------------------Scene.New()--------------------------------------------------------------------------------------------------
static PyObject *M_Scene_New(PyObject *self, PyObject *args, PyObject *kword)
{
	char		 *name = "Scene";
	char		 *kw[] = {"name", NULL};
	PyObject *pyscene; /* for the Scene object wrapper in Python */
	Scene		 *blscene; /* for the actual Scene we create in Blender */

	if (!PyArg_ParseTupleAndKeywords(args, kword, "|s", kw, &name))
		return (EXPP_ReturnPyObjError (PyExc_AttributeError,
						"expected a string or an empty argument list"));

	blscene = add_scene(name); /* first create the Scene in Blender */

	if (blscene){ 
	  /* normally, for most objects, we set the user count to zero here.
	   * Scene is different than most objs since it is the container
	   * for all the others. Since add_scene() has already set 
	   * the user count to one, we leave it alone.
	   */ 

	  /* now create the wrapper obj in Python */
	  pyscene = Scene_CreatePyObject (blscene);
	}
	else
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
														"couldn't create Scene obj in Blender"));

	if (pyscene == NULL)
		return (EXPP_ReturnPyObjError (PyExc_MemoryError,
														"couldn't create Scene PyObject"));

	return pyscene;
}
//-----------------------Scene.Get()--------------------------------------------------------------------------------------------------
static PyObject *M_Scene_Get(PyObject *self, PyObject *args)
{
	char	*name = NULL;
	Scene *scene_iter;

	if (!PyArg_ParseTuple(args, "|s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected string argument (or nothing)"));

	scene_iter = G.main->scene.first;

	if (name) { /* (name) - Search scene by name */

		PyObject *wanted_scene = NULL;

		while ((scene_iter) && (wanted_scene == NULL)) {

			if (strcmp (name, scene_iter->id.name+2) == 0)
				wanted_scene = Scene_CreatePyObject (scene_iter);

			scene_iter = scene_iter->id.next;
		}

		if (wanted_scene == NULL) { /* Requested scene doesn't exist */
			char error_msg[64];
			PyOS_snprintf(error_msg, sizeof(error_msg),
											"Scene \"%s\" not found", name);
			return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
		}

		return wanted_scene;
	}

	else { /* () - return a list with wrappers for all scenes in Blender */
		int index = 0;
		PyObject *sce_pylist, *pyobj;

		sce_pylist = PyList_New (BLI_countlist (&(G.main->scene)));

		if (sce_pylist == NULL)
			return (EXPP_ReturnPyObjError (PyExc_MemoryError,
							"couldn't create PyList"));

		while (scene_iter) {
			pyobj = Scene_CreatePyObject (scene_iter);

			if (!pyobj)
				return (EXPP_ReturnPyObjError (PyExc_MemoryError,
									"couldn't create PyString"));

			PyList_SET_ITEM (sce_pylist, index, pyobj);

			scene_iter = scene_iter->id.next;
			index++;
		}

		return sce_pylist;
	}
}
//-----------------------Scene.GetCurrent()---------------------------------------------------------------------------------------
static PyObject *M_Scene_GetCurrent (PyObject *self)
{
	return Scene_CreatePyObject ((Scene *)G.scene);
}
//-----------------------Scene.Unlink()--------------------------------------------------------------------------------------------
static PyObject *M_Scene_Unlink (PyObject *self, PyObject *args)
{ 
	PyObject *pyobj;
	Scene		 *scene;

	if (!PyArg_ParseTuple (args, "O!", &Scene_Type, &pyobj))
				return EXPP_ReturnPyObjError (PyExc_TypeError,
								"expected Scene PyType object");

	scene = ((BPy_Scene *)pyobj)->scene;

	if (scene == G.scene)
				return EXPP_ReturnPyObjError (PyExc_SystemError,
								"current Scene cannot be removed!");

	free_libblock(&G.main->scene, scene);

	Py_INCREF(Py_None);
	return Py_None;
}
//-----------------------BPy_Scene function defintions-----------------------------------------------------------------------
//-----------------------Scene.getName()-----------------------------------------------------------------------------------------
static PyObject *Scene_getName(BPy_Scene *self)
{
	PyObject *attr = PyString_FromString(self->scene->id.name+2);

	if (attr) return attr;

	return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't get Scene.name attribute"));
}
//-----------------------Scene.setName()-----------------------------------------------------------------------------------------
static PyObject *Scene_setName(BPy_Scene *self, PyObject *args)
{
	char *name;
	char buf[21];

	if (!PyArg_ParseTuple(args, "s", &name))
		return (EXPP_ReturnPyObjError (PyExc_TypeError,
																		 "expected string argument"));

	PyOS_snprintf(buf, sizeof(buf), "%s", name);

	rename_id(&self->scene->id, buf);

	Py_INCREF(Py_None);
	return Py_None;
}
//-----------------------Scene.copy()-----------------------------------------------------------------------------------------
static PyObject *Scene_copy (BPy_Scene *self, PyObject *args)
{
	short dup_objs = 1;
	Scene *scene = self->scene;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	if (!PyArg_ParseTuple (args, "|h", &dup_objs))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected int in [0,2] or nothing as argument");

	return Scene_CreatePyObject (copy_scene (scene, dup_objs));
}
//-----------------------Scene.makeCurrent()-----------------------------------------------------------------------------------------
static PyObject *Scene_makeCurrent (BPy_Scene *self)
{
	Scene *scene = self->scene;

	if (scene) set_scene (scene);

	Py_INCREF (Py_None);
	return Py_None;
}
//-----------------------Scene.update()-----------------------------------------------------------------------------------------
static PyObject *Scene_update (BPy_Scene *self, PyObject *args)
{
	Scene *scene = self->scene;
	int full = 0;

	if (!scene)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
							"Blender Scene was deleted!");

	if (!PyArg_ParseTuple (args, "|i", &full))
			return EXPP_ReturnPyObjError (PyExc_TypeError,
							"expected nothing or int (0 or 1) argument");

/* Under certain circunstances, sort_baselist *here* can crash Blender.
 * A "RuntimeError: max recursion limit" happens when a scriptlink
 * on frame change has scene.update(1).
 * Investigate better how to avoid this. */
	if (!full)
		sort_baselist (scene);

	else if (full == 1)
			set_scene_bg (scene);

	else
		return EXPP_ReturnPyObjError (PyExc_ValueError,
			"in method scene.update(full), full should be:\n"
			"0: to only sort scene elements (old behavior); or\n"
			"1: for a full update (regroups, does ipos, ikas, keys, etc.)");

	Py_INCREF (Py_None);
	return Py_None;
}
//-----------------------Scene.link()-----------------------------------------------------------------------------------------------------
static PyObject *Scene_link (BPy_Scene *self, PyObject *args)
{
	Scene *scene = self->scene;
	BPy_Object *bpy_obj;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Blender Scene was deleted!");

	if (!PyArg_ParseTuple (args, "O!", &Object_Type, &bpy_obj))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
			"expected Object argument");

	else { /* Ok, all is fine, let's try to link it */
		Object *object = bpy_obj->object;
		Base *base;

		/* We need to link the object to a 'Base', then link this base
		 * to the scene.	See DNA_scene_types.h ... */

		/* First, check if the object isn't already in the scene */
		base = EXPP_Scene_getObjectBase (scene, object);
		/* if base is not NULL ... */
		if (base) /* ... the object is already in one of the Scene Bases */
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
							"object already in scene!");

		/* not linked, go get mem for a new base object */

		base = MEM_callocN(sizeof(Base), "newbase");
 
		if (!base)
			return EXPP_ReturnPyObjError (PyExc_MemoryError,
							"couldn't allocate new Base for object");

		/* check if this object has obdata, case not, try to create it */
		if (!object->data && (object->type != OB_EMPTY))
			EXPP_add_obdata(object); /* returns -1 on error, defined in Object.c */

		base->object = object; /* link object to the new base */
		base->lay = object->lay;
		base->flag = object->flag;

		object->id.us += 1; /* incref the object user count in Blender */

		BLI_addhead(&scene->base, base); /* finally, link new base to scene */
	}

	Py_INCREF (Py_None);
	return Py_None;
}
//-----------------------Scene.unlink()-----------------------------------------------------------------------------------------------------
static PyObject *Scene_unlink (BPy_Scene *self, PyObject *args)
{ 
	BPy_Object *bpy_obj = NULL;
	Object *object;
	Scene *scene = self->scene;
	Base *base;
	short retval = 0;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender scene was deleted!");

	if (!PyArg_ParseTuple(args, "O!", &Object_Type, &bpy_obj))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Object as argument");

	object = bpy_obj->object;

	/* is the object really in the scene? */
	base = EXPP_Scene_getObjectBase(scene, object);
	 
	if (base) { /* if it is, remove it: */
		BLI_remlink(&scene->base, base);
		object->id.us -= 1;
		MEM_freeN (base);
		scene->basact = 0; /* in case the object was selected */
		retval = 1;
	}

	return Py_BuildValue ("i", PyInt_FromLong (retval));
}
//-----------------------Scene.getChildren()-----------------------------------------------------------------------------------------------------
static PyObject *Scene_getChildren (BPy_Scene *self)
{	
	Scene *scene = self->scene;
	PyObject *pylist= PyList_New(0);
	PyObject *bpy_obj;
	Object *object;
	Base *base;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	base = scene->base.first;

	while (base) {
		object = base->object;

		bpy_obj = M_Object_Get(Py_None,
										Py_BuildValue ("(s)", object->id.name+2));

		if (!bpy_obj)
			return EXPP_ReturnPyObjError (PyExc_RuntimeError,
								"couldn't create new object wrapper");

		PyList_Append (pylist, bpy_obj);
		Py_XDECREF (bpy_obj); /* PyList_Append incref'ed it */

		base = base->next;
	}

	return pylist;
}
//-----------------------Scene.getCurrentCamera()---------------------------------------------------------------------------------------
static PyObject *Scene_getCurrentCamera (BPy_Scene *self)
{	
	Object *cam_obj;
	Scene *scene = self->scene;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	cam_obj = scene->camera;

	if (cam_obj) /* if found, return a wrapper for it */
		return M_Object_Get (Py_None, Py_BuildValue ("(s)", cam_obj->id.name+2));

	Py_INCREF(Py_None); /* none found */
	return Py_None;
}
//-----------------------Scene.setCurrentCamera()---------------------------------------------------------------------------------------
static PyObject *Scene_setCurrentCamera (BPy_Scene *self, PyObject *args)
{
	Object *object;
	BPy_Object *cam_obj;
	Scene *scene = self->scene;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	if (!PyArg_ParseTuple(args, "O!", &Object_Type, &cam_obj))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected Camera Object as argument");

	object = cam_obj->object;

	scene->camera = object; /* set the current Camera */

	/* if this is the current scene, update its window now */
	if (scene == G.scene) copy_view3d_lock(REDRAW);

/* XXX copy_view3d_lock(REDRAW) prints "bad call to addqueue: 0 (18, 1)".
 * The same happens in bpython. */

	Py_INCREF(Py_None);
	return Py_None;
}
//-----------------------Scene.getRenderingContext()----------------------------------------------------------------------
static PyObject *Scene_getRenderingContext (BPy_Scene *self)
{	
	if (!self->scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Blender Scene was deleted!");

	return RenderData_CreatePyObject(self->scene);
}

/* scene.addScriptLink */
static PyObject *Scene_addScriptLink (BPy_Scene *self, PyObject *args)
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Blender Scene was deleted!");

	slink = &(scene)->scriptlink;

	if (!EXPP_addScriptLink(slink, args, 1))
		return EXPP_incr_ret (Py_None);
	else return NULL;
}

/* scene.clearScriptLinks */
static PyObject *Scene_clearScriptLinks (BPy_Scene *self)
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Blender Scene was deleted!");

	slink = &(scene)->scriptlink;

	return EXPP_incr_ret(Py_BuildValue("i", EXPP_clearScriptLinks (slink)));
}

/* scene.getScriptLinks */
static PyObject *Scene_getScriptLinks (BPy_Scene *self, PyObject *args)
{
	Scene *scene = self->scene;
	ScriptLink *slink = NULL;
	PyObject *ret = NULL;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Blender Scene was deleted!");

	slink = &(scene)->scriptlink;

	ret = EXPP_getScriptLinks(slink, args, 1);

	if (ret) return ret;
	else return NULL;
}

/*****************************************************************************/
// DEPRECATED 	
/*****************************************************************************/
//-----------------------Scene.getRenderdir ()----------------------------------------------------------------------
static PyObject *Scene_getRenderdir (BPy_Scene *self)
{
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Deprecated:use RenderData.getRenderPath()");
}
//-----------------------Scene.getBackbufdir ()----------------------------------------------------------------------
static PyObject *Scene_getBackbufdir (BPy_Scene *self)
{
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Deprecated:use RenderData.getBackbufPath()");
}
//-----------------------Scene.startFrame ()----------------------------------------------------------------------
static PyObject *Scene_startFrame (BPy_Scene *self, PyObject *args)
{
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Deprecated:use RenderData.startFrame()");
}
//-----------------------Scene.endFrame ()----------------------------------------------------------------------
static PyObject *Scene_endFrame (BPy_Scene *self, PyObject *args)
{
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Deprecated:use RenderData.endFrame()");
}
//-----------------------Scene.getWinSize ()----------------------------------------------------------------------
static PyObject *Scene_getWinSize(BPy_Scene *self)
{
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Deprecated:use RenderData.imageSizeX() and RenderData.imageSizeY");
}
//-----------------------Scene.setWinSize()----------------------------------------------------------------------
static PyObject *Scene_setWinSize(BPy_Scene *self, PyObject *args)
{
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Deprecated:use RenderData.imageSizeX() and RenderData.imageSizeY");
}
//-----------------------Scene.frameSettings()----------------------------------------------------------------------
static PyObject *Scene_frameSettings (BPy_Scene *self, PyObject *args)
{	
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Deprecated:use RenderData.startFrame(),  RenderData.endFrame, RenderData.currentFrame");
}
//-----------------------Scene.currentFrame()-----------------------------------------------------------------------------------------
static PyObject *Scene_currentFrame (BPy_Scene *self, PyObject *args)
{
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
			"Deprecated:use RenderData.currentFrame");
}
