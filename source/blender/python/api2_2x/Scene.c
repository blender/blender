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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/**
 * \file Scene.c
 * \ingroup scripts
 * \brief Blender.Scene Module and Scene  PyObject implementation.
 */

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_scene.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>
#include <BSE_headerbuttons.h> /* for copy_scene */
#include <BIF_drawscene.h>     /* for set_scene */
#include <BIF_space.h>         /* for copy_view3d_lock() */
#include <MEM_guardedalloc.h>  /* for MEM_callocN */
#include <mydevice.h>          /* for #define REDRAW */

#include "Object.h"
#include "Camera.h"
#include "modules.h"

#include "Scene.h"

static Base *EXPP_Scene_getObjectBase (Scene *scene, Object *object);
PyObject *M_Object_Get (PyObject *self, PyObject *args); /* from Object.c */

/*****************************************************************************/
/* Python BPy_Scene defaults:                                                */
/*****************************************************************************/
#define EXPP_SCENE_FRAME_MAX 18000

/*****************************************************************************/
/* Python API function prototypes for the Scene module.                      */
/*****************************************************************************/
static PyObject *M_Scene_New (PyObject *self, PyObject *args,
                               PyObject *keywords);
static PyObject *M_Scene_Get (PyObject *self, PyObject *args);
static PyObject *M_Scene_getCurrent (PyObject *self);
static PyObject *M_Scene_unlink (PyObject *self, PyObject *arg);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Scene.__doc__                                                     */
/*****************************************************************************/
static char M_Scene_doc[] = "";

static char M_Scene_New_doc[] = "";

static char M_Scene_Get_doc[] = "";

static char M_Scene_getCurrent_doc[] = "";

static char M_Scene_unlink_doc[] = "";

/*****************************************************************************/
/* Python method structure definition for Blender.Scene module:              */
/*****************************************************************************/
struct PyMethodDef M_Scene_methods[] = {
  {"New",(PyCFunction)M_Scene_New, METH_VARARGS|METH_KEYWORDS,
          M_Scene_New_doc},
  {"Get",         M_Scene_Get,         METH_VARARGS, M_Scene_Get_doc},
  {"get",         M_Scene_Get,         METH_VARARGS, M_Scene_Get_doc},
  {"getCurrent",(PyCFunction)M_Scene_getCurrent,
                             METH_NOARGS,  M_Scene_getCurrent_doc},
  {"unlink",      M_Scene_unlink,      METH_VARARGS, M_Scene_unlink_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python BPy_Scene methods declarations:                                    */
/*****************************************************************************/
static PyObject *Scene_getName(BPy_Scene *self);
static PyObject *Scene_setName(BPy_Scene *self, PyObject *arg);
static PyObject *Scene_copy(BPy_Scene *self, PyObject *arg);
static PyObject *Scene_startFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_endFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_currentFrame(BPy_Scene *self, PyObject *args);
static PyObject *Scene_frameSettings (BPy_Scene *self, PyObject *args);
static PyObject *Scene_makeCurrent(BPy_Scene *self);
static PyObject *Scene_update(BPy_Scene *self);
static PyObject *Scene_link(BPy_Scene *self, PyObject *args);
static PyObject *Scene_unlink(BPy_Scene *self, PyObject *args);
static PyObject *Scene_getRenderdir(BPy_Scene *self);
static PyObject *Scene_getBackbufdir(BPy_Scene *self);
static PyObject *Scene_getChildren(BPy_Scene *self);
static PyObject *Scene_getCurrentCamera(BPy_Scene *self);
static PyObject *Scene_setCurrentCamera(BPy_Scene *self, PyObject *args);

/*****************************************************************************/
/* Python BPy_Scene methods table:                                           */
/*****************************************************************************/
static PyMethodDef BPy_Scene_methods[] = {
 /* name, method, flags, doc */
  {"getName", (PyCFunction)Scene_getName, METH_NOARGS,
      "() - Return Scene name"},
  {"setName", (PyCFunction)Scene_setName, METH_VARARGS,
          "(str) - Change Scene name"},
  {"copy",    (PyCFunction)Scene_copy, METH_VARARGS,
          "(duplicate_objects = 1) - Return a copy of this scene\n"
  "The optional argument duplicate_objects defines how the scene\n"
  "children are duplicated:\n\t0: Link Objects\n\t1: Link Object Data"
  "\n\t2: Full copy\n"},
  {"startFrame", (PyCFunction)Scene_startFrame, METH_VARARGS,
          "(frame) - If frame is given, the start frame is set and"
                  "\nreturned in any case"},
  {"endFrame", (PyCFunction)Scene_endFrame, METH_VARARGS,
          "(frame) - If frame is given, the end frame is set and"
                  "\nreturned in any case"},
  {"currentFrame", (PyCFunction)Scene_currentFrame, METH_VARARGS,
          "(frame) - If frame is given, the current frame is set and"
                  "\nreturned in any case"},
  {"frameSettings", (PyCFunction)Scene_frameSettings, METH_NOARGS,
          "(start, end, current) - Sets or retrieves the Scene's frame"
					" settings.\nIf the frame arguments are specified, they are set. "
					"A tuple (start, end, current) is returned in any case."},
  {"makeCurrent", (PyCFunction)Scene_makeCurrent, METH_NOARGS,
          "() - Make self the current scene"},
  {"update", (PyCFunction)Scene_update, METH_NOARGS,
          "() - Update scene self"},
  {"link", (PyCFunction)Scene_link, METH_VARARGS,
          "(obj) - Link Object obj to this scene"},
  {"unlink", (PyCFunction)Scene_unlink, METH_VARARGS,
          "(obj) - Unlink Object obj from this scene"},
  {"getRenderdir", (PyCFunction)Scene_getRenderdir, METH_NOARGS,
          "() - Return directory where rendered images are saved to"},
  {"getBackbufdir", (PyCFunction)Scene_getBackbufdir, METH_NOARGS,
          "() - Return location of the backbuffer image"},
  {"getChildren", (PyCFunction)Scene_getChildren, METH_NOARGS,
          "() - Return list of all objects linked to scene self"},
  {"getCurrentCamera", (PyCFunction)Scene_getCurrentCamera, METH_NOARGS,
          "() - Return current active Camera"},
  {"setCurrentCamera", (PyCFunction)Scene_setCurrentCamera, METH_VARARGS,
          "() - Set the currently active Camera"},
  {0}
};

/*****************************************************************************/
/* Python Scene_Type callback function prototypes:                           */
/*****************************************************************************/
static void Scene_DeAlloc (BPy_Scene *self);
static int Scene_Print (BPy_Scene *self, FILE *fp, int flags);
static int Scene_SetAttr (BPy_Scene *self, char *name, PyObject *v);
static int Scene_Compare (BPy_Scene *a, BPy_Scene *b);
static PyObject *Scene_GetAttr (BPy_Scene *self, char *name);
static PyObject *Scene_Repr (BPy_Scene *self);

/*****************************************************************************/
/* Python Scene_Type structure definition:                                   */
/*****************************************************************************/
PyTypeObject Scene_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                      /* ob_size */
  "Scene",                                /* tp_name */
  sizeof (BPy_Scene),                     /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)Scene_DeAlloc,              /* tp_dealloc */
  (printfunc)Scene_Print,                 /* tp_print */
  (getattrfunc)Scene_GetAttr,             /* tp_getattr */
  (setattrfunc)Scene_SetAttr,             /* tp_setattr */
  (cmpfunc)Scene_Compare,                 /* tp_compare */
  (reprfunc)Scene_Repr,                   /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  BPy_Scene_methods,                      /* tp_methods */
  0,                                      /* tp_members */
};

/**
 * \defgroup Scene_Module Blender.Scene module functions
 *
 */

/*@{*/

/**
 * \brief Python module function: Blender.Scene.New()
 *
 * This is the .New() function of the Blender.Scene submodule. It creates
 * new Scene  in Blender and returns its Python wrapper object.  The
 * parameter is optional and defaults to name = 'Scene'.
 * \param <name> - string: The Scene  name.
 * \return A new Scene PyObject.
 */

static PyObject *M_Scene_New(PyObject *self, PyObject *args, PyObject *kword)
{
  char     *name = "Scene";
  char     *kw[] = {"name", NULL};
  PyObject *pyscene; /* for the Scene object wrapper in Python */
  Scene    *blscene; /* for the actual Scene we create in Blender */

  printf ("In Scene_New()\n");

  if (!PyArg_ParseTupleAndKeywords(args, kword, "|s", kw, &name))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected a string or an empty argument list"));

  blscene = add_scene(name); /* first create the Scene in Blender */

  if (blscene) /* now create the wrapper obj in Python */
    pyscene = Scene_CreatePyObject (blscene);
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                            "couldn't create Scene obj in Blender"));

  if (pyscene == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                            "couldn't create Scene PyObject"));

  return pyscene;
}

/**
 * \brief Python module function: Blender.Scene.Get()
 *
 * This is the .Get() function of the Blender.Scene submodule.  It searches
 * the list of current Scene objects and returns a Python wrapper for
 * the one with the name provided by the user.  If called with no arguments,
 * it returns a list of all current Scene object names in Blender.
 * \param <name> - string: The name of an existing Blender Scene object.
 * \return () - A list with the names of all current Scene objects;\n
 * \return (name) - A Python wrapper for the Scene called 'name'
 * in Blender.
 */

static PyObject *M_Scene_Get(PyObject *self, PyObject *args)
{
  char  *name = NULL;
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

  else { /* () - return a list of all scenes in Blender */
    int index = 0;
    PyObject *scenelist, *pystr;

    scenelist = PyList_New (BLI_countlist (&(G.main->scene)));

    if (scenelist == NULL)
      return (PythonReturnErrorObject (PyExc_MemoryError,
              "couldn't create PyList"));

    while (scene_iter) {
      pystr = PyString_FromString (scene_iter->id.name+2);

      if (!pystr)
        return (PythonReturnErrorObject (PyExc_MemoryError,
                  "couldn't create PyString"));

      PyList_SET_ITEM (scenelist, index, pystr);

      scene_iter = scene_iter->id.next;
      index++;
    }

    return scenelist;
  }
}

/**
 * \brief Python module function: Blender.Scene.getCurrent()
 *
 * \return A Python wrapper for the currently active scene.
 */

static PyObject *M_Scene_getCurrent (PyObject *self)
{
  return Scene_CreatePyObject ((Scene *)G.scene);
}

/**
 * \brief Python module function: Blender.Scene.unlink()
 *
 * This function actually frees the Blender Scene object linked to this
 * Python wrapper.  It calls free_libblock(), which calls free_scene(),
 * where all objects linked to this scene have their user counts decremented.
 * But there's no garbage collecting of objects in Blender yet.
 * NOTE: a SystemError is raised if the user tries to remove the currently
 * active Scene.  Letting it be done would crash Blender.
 * \param pyobj BPy_Scene*: A Scene PyObject wrapper.
 */

static PyObject *M_Scene_unlink (PyObject *self, PyObject *args)
{ 
  PyObject *pyobj;
  Scene    *scene;

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

/*@}*/

/**
 * \brief Initializes the Blender.Scene submodule
 *
 * This function is used by Blender_Init() in Blender.c to register the
 * Blender.Scene submodule in the main Blender module.
 * \return PyObject*: The initialized submodule.
 */

PyObject *M_Scene_Init (void)
{
  PyObject  *submodule;

  printf ("In M_Scene_Init()\n");

  Scene_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3("Blender.Scene",
                  M_Scene_methods, M_Scene_doc);

  return submodule;
}

/**
 * \brief Creates a new Python wrapper from an existing Blender Scene obj
 *
 * \param scene - Scene*: A pointer to an existing Blender Scene object.
 * \return PyObject*: The Scene wrapper created.
 */

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

/**
 * \brief Checks if the given object is of type BPy_Scene
 *
 * \param pyobj - PyObject*: A pointer to a Scene PyObject.
 * \return int: True or false.
 */

int Scene_CheckPyObject (PyObject *pyobj)
{
  return (pyobj->ob_type == &Scene_Type);
}

/**
 * \brief Returns the Blender Scene object from the given PyObject
 *
 * \param pyobj - PyObject*: A pointer to a Scene PyObject.
 * \return Scene*: A pointer to the wrapped Blender Scene object.
 */

Scene *Scene_FromPyObject (PyObject *pyobj)
{
  return ((BPy_Scene *)pyobj)->scene;
}

/*****************************************************************************/
/* Python BPy_Scene methods:                                                 */
/*****************************************************************************/

/**
 * \defgroup Scene_Methods Scene Method Functions
 *
 * These are the Scene PyObject method functions.  They are used to get and
 * set values for the Scene  member variables.
 */

/*@{*/

/**
 * \brief Scene PyMethod getName
 *
 * \return string: The Scene name.
 */

static PyObject *Scene_getName(BPy_Scene *self)
{
  PyObject *attr = PyString_FromString(self->scene->id.name+2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get Scene.name attribute"));
}

/**
 * \brief Scene PyMethod setName
 * \param name - string: The new Scene name.
 */

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

/**
 * \brief Scene PyMethod copy
 *
 * This function makes a copy of the scene (self).  The optional argument
 * can be:\n 0: Link Objects \n1: Link Object Data (default)\n2: Full copy
 * \param dup_objs - int: how the scene children are duplicated.
 * \return PyObject*: A pointer to the created copy of the scene.
 */

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

/**
 * \brief Scene PyMethod currentFrame
 *
 * If frame is given, the current frame is set and returned in any case.
 * \param frame int: The value for the current frame.
 * \return int: The current frame.
 */

/* Blender seems to accept any positive value up to 18000 for start, end and
 * current frames, independently. */

static PyObject *Scene_currentFrame (BPy_Scene *self, PyObject *args)
{
  short frame = -1;
  RenderData *rd = &self->scene->r;

  if (!PyArg_ParseTuple (args, "|h", &frame))
    return EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument or nothing");

  if (frame > 0) rd->cfra = EXPP_ClampInt(frame, 1, EXPP_SCENE_FRAME_MAX);

  return PyInt_FromLong (rd->cfra);
}

/**
 * \brief Scene PyMethod startFrame
 *
 * If frame is given, the start frame is set and returned in any case.
 * \param frame int: The value for the start frame.
 * \return int: The start frame.
 */

static PyObject *Scene_startFrame (BPy_Scene *self, PyObject *args)
{
  short frame = -1;
  RenderData *rd = &self->scene->r;

  if (!PyArg_ParseTuple (args, "|h", &frame))
    return EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument or nothing");

  if (frame > 0) rd->sfra = EXPP_ClampInt (frame, 1, EXPP_SCENE_FRAME_MAX);

  return PyInt_FromLong (rd->sfra);
}

/**
 * \brief Scene PyMethod endFrame
 *
 * If frame is given, the end frame is set and returned in any case.
 * \param frame int: The value for the end frame.
 * \return int: The end frame.
 */

static PyObject *Scene_endFrame (BPy_Scene *self, PyObject *args)
{
  short frame = -1;
  RenderData *rd = &self->scene->r;

  if (!PyArg_ParseTuple (args, "|h", &frame))
    return EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument or nothing");

  if (frame > 0) rd->efra = EXPP_ClampInt (frame, 1, EXPP_SCENE_FRAME_MAX);

  return PyInt_FromLong (rd->efra);
}

/**
 * \brief Scene PyMethod makeCurrent
 *
 * Make self the current scene.
 */

static PyObject *Scene_makeCurrent (BPy_Scene *self)
{
  Scene *scene = self->scene;

  if (scene) set_scene (scene);

  Py_INCREF (Py_None);
  return Py_None;
}

/**
 * \brief Scene PyMethod update
 *
 * Updates scene self.  This function explicitely resorts the base list of
 * a newly created object hierarchy.
 */

static PyObject *Scene_update (BPy_Scene *self)
{
  Scene *scene = self->scene;

  if (scene) sort_baselist (scene);

  Py_INCREF (Py_None);
  return Py_None;
}

/**
 * \brief Scene PyMethod link
 *
 * Link the given object to this scene.
 * \param object PyObject*: A pointer to an Object Python wrapper.
 */

static PyObject *Scene_link (BPy_Scene *self, PyObject *args)
{
  Scene    *scene = self->scene;
  C_Object *bpy_obj; /* XXX Change to BPy or whatever is chosen */

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
     * to the scene.  See DNA_scene_types.h ... */

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

    base->object = object; /* link object to the new base */
    base->lay = object->lay;
    base->flag = object->flag;

    object->id.us += 1; /* incref the object user count in Blender */

    BLI_addhead(&scene->base, base); /* finally, link new base to scene */
  }

  Py_INCREF (Py_None);
  return Py_None;
}

/**
 * \brief Scene PyMethod unlink
 *
 * Unlink (delete) the given object from this scene.
 * \param object PyObject*:  A pointer to a Blender Object Python wrapper.
 * \return int: 1 for success, 0 for failure.
 */

static PyObject *Scene_unlink (BPy_Scene *self, PyObject *args)
{ 
  C_Object *bpy_obj = NULL;
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

/**
 * \brief Scene PyMethod getRenderdir
 *
 * \return string: The directory where rendered images are saved to.
 */

static PyObject *Scene_getRenderdir (BPy_Scene *self)
{
  if (self->scene)
    return PyString_FromString (self->scene->r.pic);
  else
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
            "Blender Scene was deleted!");
}

/**
 * \brief Scene PyMethod getBackbufdir
 *
 * \return string: The backbuffer image location
 */

static PyObject *Scene_getBackbufdir (BPy_Scene *self)
{
  if (self->scene)
    return PyString_FromString (self->scene->r.backbuf);
  else
    return EXPP_ReturnPyObjError (PyExc_RuntimeError,
            "Blender Scene already deleted");
}

/**
 * \brief Scene PyMethod frameSettings
 *
 * This method can be used to set (if the values are given) and in any case
 * get a tuple representing the start, end and current frame values.
 * \param start int: The optional start frame value;
 * \param end   int: The optional end frame value;
 * \param current int: The optional current frame value.
 * \return tuple: (start, end, current) frame values.
 */

static PyObject *Scene_frameSettings (BPy_Scene *self, PyObject *args)
{	
	int start = -1;
	int end = -1;
	int current = -1;
	RenderData *rd = NULL;
	Scene *scene = self->scene;

	if (!scene)
		return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Blender Scene was deleted!");

	rd = &scene->r;

	if (!PyArg_ParseTuple (args, "|iii", &start, &end, &current))
		return EXPP_ReturnPyObjError (PyExc_TypeError,
						"expected three ints or nothing as arguments");

	if (start > 0)   rd->sfra = EXPP_ClampInt (start, 1, EXPP_SCENE_FRAME_MAX);
	if (end > 0)     rd->efra = EXPP_ClampInt (end, 1, EXPP_SCENE_FRAME_MAX);
	if (current > 0) rd->cfra = EXPP_ClampInt (current, 1, EXPP_SCENE_FRAME_MAX);

	return Py_BuildValue("(iii)", rd->sfra, rd->efra, rd->cfra);
}

/**
 * \brief Scene PyMethod getChildren
 *
 * \return PyList: a list of all objects linked to Scene self.
 */

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

/**
 * \brief Scene PyMethod getCurrentCamera
 *
 * \return PyObject*: A wrapper for the currently active camera
 */

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

/**
 * \brief Scene PyMethod setCurrentCamera
 *
 * Set the currently active Camera Object in Blender.
 * \param cam_obj PyObject*: A Camera PyObject.
 */

static PyObject *Scene_setCurrentCamera (BPy_Scene *self, PyObject *args)
{
	Object *object;
	C_Object *cam_obj;
	Scene  *scene = self->scene;

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

/*@}*/

/**
 * \defgroup Scene_callbacks Callback functions for the Scene PyType
 *
 * These callbacks are called by the Python interpreter when dealing with
 * PyObjects of type Scene.
 */

/*@{*/

/**
 * \brief The Scene PyType destructor
 */

static void Scene_DeAlloc (BPy_Scene *self)
{
  PyObject_DEL (self);
}

/**
 * \brief The Scene PyType attribute getter
 *
 * This is the callback called when a user tries to retrieve the contents of
 * Scene PyObject data members.  Ex. in Python: "print myscene.lens".
 */

static PyObject *Scene_GetAttr (BPy_Scene *self, char *name)
{
  PyObject *attr = Py_None;

  if (strcmp(name, "name") == 0)
    attr = PyString_FromString(self->scene->id.name+2);

  else if (strcmp(name, "__members__") == 0) {
    attr = Py_BuildValue("[s,s,s,s,s,s,s,s,s]",
                    "name", "type", "mode", "lens", "clipStart",
                    "clipEnd", "drawSize", "Types", "Modes");
  }

  if (!attr)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                      "couldn't create PyObject"));

  if (attr != Py_None) return attr; /* member attribute found, return it */

  /* not an attribute, search the methods table */
  return Py_FindMethod(BPy_Scene_methods, (PyObject *)self, name);
}

/**
 * \brief The Scene PyType attribute setter
 *
 * This is the callback called when the user tries to change the value of some
 * Scene data member.  Ex. in Python: "myscene.lens = 45.0".
 */

static int Scene_SetAttr (BPy_Scene *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;

/* We're playing a trick on the Python API users here.  Even if they use
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

/**
 * \brief The Scene PyType compare function
 *
 * This function compares two given Scene PyObjects, returning 0 for equality
 * and -1 otherwise.  In Python it becomes 1 if they are equal and 0 case not.
 * The comparison is done with their pointers to Blender Scene  objects,
 * so any two wrappers pointing to the same Blender Scene  will be
 * considered the same Scene PyObject.  Currently, only the "==" and "!="
 * comparisons are meaninful -- the "<", "<=", ">" or ">=" are not.
 */

static int Scene_Compare (BPy_Scene *a, BPy_Scene *b)
{
  Scene *pa = a->scene, *pb = b->scene;
  return (pa == pb) ? 0:-1;
}

/**
 * \brief The Scene PyType print callback
 *
 * This function is called when the user tries to print a PyObject of type
 * Scene.  It builds a string with the name of the wrapped Blender Scene.
 */

static int Scene_Print(BPy_Scene *self, FILE *fp, int flags)
{ 
  fprintf(fp, "[Scene \"%s\"]", self->scene->id.name+2);
  return 0;
}

/**
 * \brief The Scene PyType repr callback
 *
 * This function is called when the statement "repr(myscene)" is executed in
 * Python.  Repr gives a string representation of a PyObject.
 */

static PyObject *Scene_Repr (BPy_Scene *self)
{
  return PyString_FromString(self->scene->id.name+2);
}

/*@}*/

/**
 * \brief Internal helper function to search the Base of an Object
 *
 * This function looks up the linked list of Bases in a scene, searching
 * for a given object.
 * \param scene Scene*: A pointer to a Blender Scene;
 * \param object Object*: A pointer to a Blender Object.
 * \return The Base* to the Base where object was stored or NULL if the
 * object isn't linked to this scene.
 */

Base *EXPP_Scene_getObjectBase(Scene *scene, Object *object)
{
  Base *base = scene->base.first;

  while (base) {

    if (object == base->object) return base; /* found it? */

    base = base->next;
  }

  return NULL; /* object isn't linked to this scene */
}
