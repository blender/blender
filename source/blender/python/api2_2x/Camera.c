/* TODO: 'print dir(CameraObject)' doesn't work, attribs are not recognized;
 *   how to assign methods to objects? PyMethod_New() ? Maybe separate
 *   CameraModule and Camera; found: check Extending ... section 2.2.3
 *   correctly handle references;
 *   Make all functions but module init's static ?
 *   'print Blender.Camera.__doc__' doesn't work (still gives 'None')*/

#include <Python.h>
#include <stdio.h>

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <DNA_camera_types.h>

#include "gen_utils.h"
#include "modules.h"

/*****************************************************************************/
/* Python API function prototypes for the Camera module.                    */
/******************************************************************************/
PyObject *Camera_New (PyObject *self, PyObject *args, PyObject *keywords);
PyObject *Camera_Get (PyObject *self, PyObject *args);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.      */
/* In Python these will be written to the console when doing a               */
/* Blender.Camera.__doc__                                                     */
/*****************************************************************************/
char CameraModule_doc[] =
"The Blender Camera module\n\n\
This module provides access to **Camera** objects in Blender\n\n\
Example::\n\n\
  from Blender import Camera, Object, Scene\n\
  c = Camera.New('ortho')      # create new ortho camera data\n\
  c.lens = 35.0                # set lens value\n\
  cur = Scene.getCurrent()     # get current Scene\n\
  ob = Object.New('Camera')    # make camera object\n\
  ob.link(c)                   # link camera data with this object\n\
  cur.link(ob)                 # link object into scene\n\
  cur.setCurrentCamera(ob)     # make this camera the active\n";

char Camera_New_doc[] =
"(type) - returns a new Camera object of type 'type', \
which can be 'persp' or 'ortho'.\n\
() - returns a new Camera object of type 'persp'.";

char Camera_Get_doc[] =
"(name) - return the camera with the name 'name', \
returns None if not found.\n If 'name' is not specified, \
it returns a list of all cameras in the\ncurrent scene.";

/*****************************************************************************/
/* Python BlenderCamera structure definition.                                */
/*****************************************************************************/
typedef struct {
  PyObject_HEAD
  struct Camera    *camera;
} BlenCamera;

/*****************************************************************************/
/* PythonTypeCamera callback function prototypes                             */
/*****************************************************************************/
void CameraDeAlloc (BlenCamera *cam);
PyObject* CameraGetAttr (BlenCamera *cam, char *name);
int CameraSetAttr (BlenCamera *cam, char *name, PyObject *v);
PyObject* CameraRepr (BlenCamera *cam);

/*****************************************************************************/
/* Python TypeCamera structure definition.                                   */
/*****************************************************************************/
static PyTypeObject camera_type =
{
  PyObject_HEAD_INIT(&PyType_Type)
  0,								/* ob_size */
  "Camera",						/* tp_name */
  sizeof (BlenCamera),			/* tp_basicsize */
  0,								/* tp_itemsize */
  /* methods */
  (destructor)CameraDeAlloc,		/* tp_dealloc */
  0,								/* tp_print */
  (getattrfunc)CameraGetAttr,		/* tp_getattr */
  (setattrfunc)CameraSetAttr,		/* tp_setattr */
  0,								/* tp_compare */
  (reprfunc)CameraRepr,					/* tp_repr */
  0,								/* tp_as_number */
  0,								/* tp_as_sequence */
  0,								/* tp_as_mapping */
  0,								/* tp_as_hash */
  0,
  0,
  0,
  0,
  0,
  0,
  CameraModule_doc,  /* tp_doc Isn't working for some reason */ 
  0,
};

/*****************************************************************************/
/* Python method structure definition.                                       */
/*****************************************************************************/
struct PyMethodDef Camera_methods[] = {
  {"New",(PyCFunction)Camera_New, METH_VARARGS|METH_KEYWORDS, Camera_New_doc},
  {"Get",         Camera_Get,         METH_VARARGS, Camera_Get_doc},
  {"get",         Camera_Get,         METH_VARARGS, Camera_Get_doc},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Function:              Camera_New                                         */
/* Python equivalent:     Blender.Camera.New                                 */
/*****************************************************************************/
PyObject *Camera_New(PyObject *self, PyObject *args, PyObject *keywords)
{
  char          *type_str = "persp"; /* "persp" is type 0, "ortho" is type 1 */
  static char   *kwlist[] = {"type_str", NULL};
  Camera	      * cam;
  BlenCamera	  * blen_camera;

  printf ("In Camera_New()\n");

  if (!PyArg_ParseTupleAndKeywords(args, keywords, "|s", kwlist, &type_str))
  /* We expected a string (or nothing) as an argument, but we didn't get one. */
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected string (or empty) argument"));

  cam = add_camera();
  cam->id.us = 0; /* new camera: no user yet */

  if (StringEqual (type_str, "persp"))
    cam->type = 0;
  else if (StringEqual (type_str, "ortho"))
    cam->type = 1;
  else
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "unknown camera type"));

  blen_camera = (BlenCamera*)PyObject_NEW(BlenCamera, &camera_type);
  blen_camera->camera = cam;

  return ((PyObject*)blen_camera);
}

/*****************************************************************************/
/* Function:              Camera_Get                                         */
/* Python equivalent:     Blender.Camera.Get                                 */
/*****************************************************************************/
PyObject *Camera_Get(PyObject *self, PyObject *args)
{
  char            * name;
  struct Camera	  * cam_iter;
  BlenCamera      * blen_camera;

  printf ("In Camera_Get()\n");
  if (!PyArg_ParseTuple(args, "s", &name))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected string argument"));
  }

  /* Use the name to search for the camera requested. */
  blen_camera = NULL;
  cam_iter = G.main->camera.first;
  while ((cam_iter) && (blen_camera == NULL))
  {
    if (StringEqual (name, GetIdName (&(cam_iter->id))))
    {
      blen_camera = (BlenCamera*)PyObject_NEW(BlenCamera, &camera_type);
      blen_camera->camera = cam_iter;
    }
    cam_iter = cam_iter->id.next;
  }

  if (blen_camera == NULL)
  {
    /* No camera exists with the name specified in the argument name. */
    char error_msg[256];
    sprintf(error_msg, "camera \"%s\" not found", name);
    return (PythonReturnErrorObject (PyExc_AttributeError, error_msg));
  }

  return ((PyObject*)blen_camera);
}

/*****************************************************************************/
/* Function:              initCamera                                         */
/*****************************************************************************/
PyObject *initCamera (void)
{
  PyObject	* module;

  printf ("In initCamera()\n");

  module = Py_InitModule("Camera", Camera_methods);

  return (module);
}

/*****************************************************************************/
/* Function:    CameraCreatePyObject                                         */
/* Description: This function will create a new BlenCamera from an existing  */
/*              Camera structure.                                            */
/*****************************************************************************/
PyObject* CameraCreatePyObject (struct Camera *cam)
{
  BlenCamera      * blen_camera;

  printf ("In CameraCreatePyObject\n");

  blen_camera = (BlenCamera*)PyObject_NEW(BlenCamera, &camera_type);

  blen_camera->camera = cam;
  return ((PyObject*)blen_camera);
}

/*****************************************************************************/
/* Function:    CameraDeAlloc                                                */
/* Description: This is a callback function for the BlenCamera type. It is   */
/*              the destructor function.                                     */
/*****************************************************************************/
void CameraDeAlloc (BlenCamera *cam)
{
  PyObject_DEL (cam);
}

/*****************************************************************************/
/* Function:    CameraGetAttr                                                */
/* Description: This is a callback function for the BlenCamera type. It is   */
/*              the function that retrieves any value from Blender and       */
/*              passes it to Python.                                         */
/*****************************************************************************/
PyObject* CameraGetAttr (BlenCamera *cam, char *name)
{
  struct Camera   * camera;

  camera = cam->camera;
  if (StringEqual (name, "lens"))
    return (PyFloat_FromDouble(camera->lens));
  if (StringEqual (name, "clipStart"))
    return (PyFloat_FromDouble(camera->clipsta));
  if (StringEqual (name, "clipEnd"))
    return (PyFloat_FromDouble(camera->clipend));
  if (StringEqual (name, "type"))
    return (PyInt_FromLong(camera->type));
  if (StringEqual (name, "mode"))
    return (PyInt_FromLong(camera->flag));

  printf ("Unknown variable.\n");
  Py_INCREF (Py_None); 
  return (Py_None);
}

/*****************************************************************************/
/* Function:    CameraSetAttr                                                */
/* Description: This is a callback function for the BlenCamera type. It is   */
/*              the function that retrieves any value from Python and sets   */
/*              it accordingly in Blender.                                   */
/*****************************************************************************/
int CameraSetAttr (BlenCamera *cam, char *name, PyObject *value)
{
  struct Camera	* camera;

  camera = cam->camera;
  if (StringEqual (name, "lens"))
    return (!PyArg_Parse (value, "f", &(camera->lens)));
  if (StringEqual (name, "clipStart"))
    return (!PyArg_Parse (value, "f", &(camera->clipsta)));
  if (StringEqual (name, "clipEnd"))
    return (!PyArg_Parse (value, "f", &(camera->clipend)));
  if (StringEqual (name, "type"))
    return (!PyArg_Parse (value, "i", &(camera->type)));
  if (StringEqual (name, "mode"))
    return (!PyArg_Parse (value, "i", &(camera->flag)));

  printf ("Unknown variable.\n");
  return (0);
}
/*****************************************************************************/
/* Function:    CameraRepr                                                   */
/* Description: This is a callback function for the BlenCamera type. It      */
/*              builds a meaninful string to represent camera objects.       */
/*****************************************************************************/
PyObject* CameraRepr (BlenCamera *self)
{
  static char s[256];
  sprintf (s, "[Camera \"%s\"]", self->camera->id.name+2);
  return Py_BuildValue("s", s);
}
