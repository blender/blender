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

#include "Camera.h"

/*****************************************************************************/
/* Function:              M_Camera_New                                       */
/* Python equivalent:     Blender.Camera.New                                 */
/*****************************************************************************/
static PyObject *M_Camera_New(PyObject *self, PyObject *args, PyObject *keywords)
{
  char        *type_str = "persp"; /* "persp" is type 0, "ortho" is type 1 */
  char        *name_str = "Data";
  static char *kwlist[] = {"type_str", "name_str", NULL};
  C_Camera    *cam;
  PyObject    *type, *name;
  int         type_int;
  char        buf[21];

  printf ("In Camera_New()\n");

  if (!PyArg_ParseTupleAndKeywords(args, keywords, "|ss", kwlist,
                                   &type_str, &name_str))
  {
  /* We expected string(s) (or nothing) as argument, but we didn't get it. */
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected zero, one or two strings as arguments"));
  }

  if (strcmp (type_str, "persp") == 0)
    type_int = EXPP_CAM_TYPE_PERSP;
  else
  {
    if (strcmp (type_str, "ortho") == 0)
    {
      type_int = EXPP_CAM_TYPE_ORTHO;
    }
    else
    {
      return (PythonReturnErrorObject (PyExc_AttributeError,
              "unknown camera type"));
    }
  }

  cam = (C_Camera *)CameraCreatePyObject(NULL);

  if (cam == NULL)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create Camera Data object"));
  }
  cam->linked = 0; /* only Camera Data, not linked */

  type = PyInt_FromLong(type_int);
  if (type)
  {
    CameraSetAttr(cam, "type", type);
  }
  else
  {
    Py_DECREF((PyObject *)cam);
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyString"));
  }

  if (strcmp(name_str, "Data") == 0)
  {
    return (PyObject *)cam;
  }

  PyOS_snprintf(buf, sizeof(buf), "%s", name_str);
  name = PyString_FromString(buf);
  if (name)
  {
    CameraSetAttr(cam, "name", name);
  }
  else
  {
    Py_DECREF((PyObject *)cam);
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyString"));
  }

  return (PyObject *)cam;
}

/*****************************************************************************/
/* Function:              M_Camera_Get                                       */
/* Python equivalent:     Blender.Camera.Get                                 */
/*****************************************************************************/
static PyObject *M_Camera_Get(PyObject *self, PyObject *args)
{
  char     *name;
  Camera   *cam_iter;
  C_Camera *wanted_cam;

  printf ("In Camera_Get()\n");
  if (!PyArg_ParseTuple(args, "s", &name))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected string argument"));
  }

  /* Use the name to search for the camera requested. */
  wanted_cam = NULL;
  cam_iter = G.main->camera.first;

  while ((cam_iter) && (wanted_cam == NULL))
  {
    if (strcmp (name, GetIdName (&(cam_iter->id))) == 0)
    {
      wanted_cam = (C_Camera *)CameraCreatePyObject(cam_iter);
      cam_iter = cam_iter->id.next;
    }
  }

  if (wanted_cam == NULL)
  {
    /* No camera exists with the name specified in the argument name. */
    char error_msg[64];
    PyOS_snprintf(error_msg, sizeof(error_msg),
                    "Camera \"%s\" not found", name);
    return (PythonReturnErrorObject (PyExc_NameError, error_msg));
  }

  wanted_cam->linked = 1; /* TRUE: linked to a Blender Camera Object */
  return ((PyObject*)wanted_cam);
}

/*****************************************************************************/
/* Function:              M_Camera_Init                                      */
/*****************************************************************************/
PyObject *M_Camera_Init (void)
{
  PyObject  *module;

  printf ("In M_Camera_Init()\n");

  module = Py_InitModule3("Camera", M_Camera_methods, M_Camera_doc);

  return (module);
}

/*****************************************************************************/
/* Python C_Camera methods:                                                  */
/*****************************************************************************/
static PyObject *Camera_getName(C_Camera *self)
{
  PyObject *attr;
  attr = PyDict_GetItemString(self->dict, "name");
  if (attr)
  {
    Py_INCREF(attr);
    return attr;
  }
  return (PythonReturnErrorObject (PyExc_RuntimeError,
                                   "couldn't get Camera.name attribute"));
}

static PyObject *Camera_getType(C_Camera *self)
{ 
  PyObject *attr;

  attr = PyDict_GetItemString(self->dict, "type");
  if (attr)
  {
    Py_INCREF(attr);
    return attr;
  }
  return (PythonReturnErrorObject (PyExc_RuntimeError,
                                   "couldn't get Camera.type attribute"));
}

static PyObject *Camera_getMode(C_Camera *self)
{
  PyObject *attr;

  attr = PyDict_GetItemString(self->dict, "mode");
  if (attr)
  {
    Py_INCREF(attr);
    return attr;
  }
  return (PythonReturnErrorObject (PyExc_RuntimeError,
                                   "couldn't get Camera.Mode attribute"));
}

static PyObject *Camera_getLens(C_Camera *self)
{
  PyObject *attr;

  attr = PyDict_GetItemString(self->dict, "lens");
  if (attr)
  {
    Py_INCREF(attr);
    return attr;
  }
  return (PythonReturnErrorObject (PyExc_RuntimeError,
                                   "couldn't get Camera.lens attribute"));
}

static PyObject *Camera_getClipStart(C_Camera *self)
{
  PyObject *attr;

  attr = PyDict_GetItemString(self->dict, "clipStart");
  if (attr)
  {
    Py_INCREF(attr);
    return attr;
  }
  return (PythonReturnErrorObject (PyExc_RuntimeError,
                                   "couldn't get Camera.clipStart attribute"));
}

static PyObject *Camera_getClipEnd(C_Camera *self)
{
  PyObject *attr;

  attr = PyDict_GetItemString(self->dict, "clipEnd");
  if (attr)
  {
    Py_INCREF(attr);
    return attr;
  }
  return (PythonReturnErrorObject (PyExc_RuntimeError,
                                   "couldn't get Camera.clipEnd attribute"));
}

static PyObject *Camera_getDrawSize(C_Camera *self)
{
  PyObject *attr;

  attr = PyDict_GetItemString(self->dict, "drawSize");
  if (attr)
  {
    Py_INCREF(attr);
    return attr;
  }
  return (PythonReturnErrorObject (PyExc_RuntimeError,
                                   "couldn't get Camera.drawSize attribute"));
}

static PyObject *Camera_rename(C_Camera *self, PyObject *args)
{
  char      *name_str;
  char       buf[21];
  PyObject  *name;

  if (!PyArg_ParseTuple(args, "s", &name_str))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected string argument"));
  }
  
  PyOS_snprintf(buf, sizeof(buf), "%s", name_str);
  
  if (self->linked)
  {
    /* update the Blender Camera, too */
    ID *tmp_id = &self->camera->id;
    rename_id(tmp_id, buf);
    PyOS_snprintf(buf, sizeof(buf), "%s", tmp_id->name+2);/* may have changed */
  }

  name = PyString_FromString(buf);

  if (!name)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
            "couldn't create PyString Object"));
  }

  if (PyDict_SetItemString(self->dict, "name", name) != 0)
  {
    Py_DECREF(name);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                     "couldn't set Camera.name attribute"));
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Camera_setType(C_Camera *self, PyObject *args)
{
  short value;
  char *type_str;
  PyObject *type;

  if (!PyArg_ParseTuple(args, "s", &type_str))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected string argument"));
  }

  if (strcmp (type_str, "persp") == 0)
    value = EXPP_CAM_TYPE_PERSP;
  else if (strcmp (type_str, "ortho") == 0)
    value = EXPP_CAM_TYPE_ORTHO;  
  else
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "unknown camera type"));
  }

  type = PyInt_FromLong(value);
  if (!type)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyInt Object"));
  }

  if (PyDict_SetItemString(self->dict, "type", type) != 0)
  {
    Py_DECREF(type);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                     "couldn't set Camera.type attribute"));
  }

  if (self->linked)
  {
    /* update the Blender Camera, too */
    self->camera->type = value;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

/* This one is 'private'. It is not really a method, just a helper function for
 * when script writers use Camera.type = t instead of Camera.setType(t), since in
 * the first case t should be an int and in the second a string. So while the
 * method setType expects a string ('persp' or 'ortho') or an empty argument,
 * this function should receive an int (0 or 1). */
static PyObject *Camera_setIntType(C_Camera *self, PyObject *args)
{
  short value;
  PyObject *type;

  if (!PyArg_ParseTuple(args, "i", &value))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected int argument: 0 or 1"));
  }

  if (value == 0 || value == 1)
  {
    type = PyInt_FromLong(value);
  }
  else
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected int argument: 0 or 1"));
  }

  if (!type)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyInt Object"));
  }

  if (PyDict_SetItemString(self->dict, "type", type) != 0)
  {
    Py_DECREF(type);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                     "could not set Camera.type attribute"));
  }

  if (self->linked)
  {
    /* update the Blender Camera, too */
    self->camera->type = value;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Camera_setMode(C_Camera *self, PyObject *args)
{
  char *mode_str1 = NULL, *mode_str2 = NULL;
  short flag = 0;
  PyObject *mode;

  if (!PyArg_ParseTuple(args, "|ss", &mode_str1, &mode_str2))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected one or two strings as arguments"));
  } 
  
  if (mode_str1 != NULL)
  {
    if (strcmp(mode_str1, "showLimits") == 0)
      flag |= EXPP_CAM_MODE_SHOWLIMITS;
    else if (strcmp(mode_str1, "showMist") == 0)
      flag |= EXPP_CAM_MODE_SHOWMIST;
    else
    {
      return (PythonReturnErrorObject (PyExc_AttributeError,
                              "first argument is an unknown camera flag"));
    }

    if (mode_str2 != NULL)
    {
      if (strcmp(mode_str2, "showLimits") == 0)
        flag |= EXPP_CAM_MODE_SHOWLIMITS;
      else if (strcmp(mode_str2, "showMist") == 0)
        flag |= EXPP_CAM_MODE_SHOWMIST;
      else
      {
        return (PythonReturnErrorObject (PyExc_AttributeError,
                              "second argument is an unknown camera flag"));
      }
    }
  }
  
  mode = PyInt_FromLong(flag);
  if (!mode)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyInt Object"));
  }

  if (PyDict_SetItemString(self->dict, "mode", mode) != 0)
  {
    Py_DECREF(mode);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                     "couldn't set Camera.mode attribute"));
  }

  if (self->linked) /* update the Blender Camera, too */
    self->camera->flag = flag;

  Py_INCREF(Py_None);
  return Py_None;
}

/* Another helper function, for the same reason.
 * (See comment before Camera_setIntType above). */
static PyObject *Camera_setIntMode(C_Camera *self, PyObject *args)
{
  short value;
  PyObject *mode;

  if (!PyArg_ParseTuple(args, "h", &value))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected int argument in [0,3]"));
  }

  if (value >= 0 && value <= 3)
  {
    mode = PyInt_FromLong(value);
  }
  else
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected int argument in [0,3]"));
  }

  if (!mode)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyInt object"));
  }

  if (PyDict_SetItemString(self->dict, "mode", mode) != 0)
  {
    Py_DECREF(mode);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                     "could not set Camera.mode attribute"));
  }

  if (self->linked)
  {
    /* update the Blender Camera, too */
    self->camera->flag = value;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Camera_setLens(C_Camera *self, PyObject *args)
{
  float value;
  PyObject *lens;
  
  if (!PyArg_ParseTuple(args, "f", &value))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected float argument"));
  }
  
  value = EXPP_ClampFloat (value, EXPP_CAM_LENS_MIN, EXPP_CAM_LENS_MAX);
  lens = PyFloat_FromDouble(value);
  if (!lens)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyFloat Object"));
  }

  if (PyDict_SetItemString(self->dict, "lens", lens) != 0)
  {
    Py_DECREF(lens);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                     "couldn't set Camera.lens attribute"));
  }
  
  if (self->linked)
  {
    /* update the Blender Camera, too */
    self->camera->lens = value;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Camera_setClipStart(C_Camera *self, PyObject *args)
{
  float value;
  PyObject *clipStart;
  
  if (!PyArg_ParseTuple(args, "f", &value))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected a float number as argument"));
  }
  
  value = EXPP_ClampFloat (value, EXPP_CAM_CLIPSTART_MIN,
                           EXPP_CAM_CLIPSTART_MAX);
  
  clipStart = PyFloat_FromDouble(value);
  if (!clipStart)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyFloat Object"));
  }

  if (PyDict_SetItemString(self->dict, "clipStart", clipStart) != 0)
  {
    Py_DECREF(clipStart);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                  "couldn't set Camera.clipStart attribute"));
  }
  
  if (self->linked)
  {
    /* update the Blender Camera, too */
    self->camera->clipsta = value;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Camera_setClipEnd(C_Camera *self, PyObject *args)
{
  float value;
  PyObject *clipEnd;
  
  if (!PyArg_ParseTuple(args, "f", &value))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected a float number as argument"));
  }

  value = EXPP_ClampFloat (value, EXPP_CAM_CLIPEND_MIN, EXPP_CAM_CLIPEND_MAX);

  clipEnd = PyFloat_FromDouble(value);
  if (!clipEnd)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyFloat Object"));
  }

  if (PyDict_SetItemString(self->dict, "clipEnd", clipEnd) != 0)
  {
    Py_DECREF(clipEnd);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                    "couldn't set Camera.clipEnd attribute"));
  }
  
  if (self->linked)
  {
    /* update the Blender Camera, too */
    self->camera->clipend = value;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Camera_setDrawSize(C_Camera *self, PyObject *args)
{
  float value;
  PyObject *drawSize;
  
  if (!PyArg_ParseTuple(args, "f", &value))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
                                     "expected a float number as argument"));
  }

  value = EXPP_ClampFloat (value, EXPP_CAM_DRAWSIZE_MIN,
                           EXPP_CAM_DRAWSIZE_MAX);

  drawSize = PyFloat_FromDouble(value);
  if (!drawSize)
  {
    return (PythonReturnErrorObject (PyExc_MemoryError,
                                     "couldn't create PyFloat Object"));
  }

  if (PyDict_SetItemString(self->dict, "drawSize", drawSize) != 0)
  {
    Py_DECREF(drawSize);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
                                   "couldn't set Camera.drawSize attribute"));
  }
  
  if (self->linked)
  {
    /* update the Blender Camera, too */
    self->camera->drawsize = value;
  }

  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************************/
/* Function:    CameraCreatePyObject                                         */
/* Description: This function will create a new C_Camera.  If the Camera     */
/*              struct passed to it is not NULL, it'll use its attributes.   */
/*****************************************************************************/
PyObject *CameraCreatePyObject (Camera *blenderCam)
{
  PyObject *name, *type, *mode;
  PyObject *lens, *clipStart, *clipEnd, *drawSize; 
  PyObject *Types, *persp, *ortho;
  PyObject *Modes, *showLimits, *showMist;
  C_Camera *cam;

  printf ("In CameraCreatePyObject\n");

  cam = (C_Camera *)PyObject_NEW(C_Camera, &Camera_Type);
  
  if (cam == NULL)
  {
    return NULL;
  }

  cam->dict = PyDict_New();

  if (cam->dict == NULL)
  {
    Py_DECREF((PyObject *)cam);
    return NULL;
  }

  if (blenderCam == NULL)
  {
    /* Not linked to a Camera Object yet */
    name = PyString_FromString("Data");
    type = PyInt_FromLong(EXPP_CAM_TYPE);
    mode = PyInt_FromLong(EXPP_CAM_MODE);
    lens = PyFloat_FromDouble(EXPP_CAM_LENS);
    clipStart = PyFloat_FromDouble(EXPP_CAM_CLIPSTART);
    clipEnd = PyFloat_FromDouble(EXPP_CAM_CLIPEND);
    drawSize = PyFloat_FromDouble(EXPP_CAM_DRAWSIZE);
  }
  else
  {
    /* Camera Object available, get its attributes directly */
    name = PyString_FromString(blenderCam->id.name+2);
    type = PyInt_FromLong(blenderCam->type);
    mode = PyInt_FromLong(blenderCam->flag);
    lens = PyFloat_FromDouble(blenderCam->lens);
    clipStart = PyFloat_FromDouble(blenderCam->clipsta);
    clipEnd = PyFloat_FromDouble(blenderCam->clipend);
    drawSize = PyFloat_FromDouble(blenderCam->drawsize);
  }

  Types = constant_New();
  persp = PyInt_FromLong(EXPP_CAM_TYPE_PERSP);
  ortho = PyInt_FromLong(EXPP_CAM_TYPE_ORTHO);
  
  Modes = constant_New();
  showLimits = PyInt_FromLong(EXPP_CAM_MODE_SHOWLIMITS);
  showMist = PyInt_FromLong(EXPP_CAM_MODE_SHOWMIST);

  if (name == NULL || type == NULL || mode == NULL|| lens == NULL ||
      clipStart == NULL || clipEnd == NULL || drawSize == NULL ||
      Types == NULL || persp == NULL || ortho == NULL ||
      Modes == NULL || showLimits == NULL || showMist == NULL)
  {
    /* Some object creation has gone wrong. Clean up. */
    goto fail;
  }

  if ((PyDict_SetItemString(cam->dict, "name", name) != 0) ||
      (PyDict_SetItemString(cam->dict, "type", type) != 0) ||
      (PyDict_SetItemString(cam->dict, "mode", mode) != 0) ||
      (PyDict_SetItemString(cam->dict, "lens", lens) != 0) ||
      (PyDict_SetItemString(cam->dict, "clipStart", clipStart) != 0) ||
      (PyDict_SetItemString(cam->dict, "clipEnd", clipEnd) != 0) ||
      (PyDict_SetItemString(cam->dict, "drawSize", drawSize) != 0) ||
      (PyDict_SetItemString(cam->dict, "Types", Types) != 0) ||
      (PyDict_SetItemString(cam->dict, "Modes", Modes) != 0) ||
      (PyDict_SetItemString(cam->dict, "__members__",
                            PyDict_Keys(cam->dict)) != 0))
  {
    /* One or more value setting actions has gone wwrong. Clean up. */
    goto fail;
  }

  if ((PyDict_SetItemString(((C_constant *)Types)->dict,
                            "persp", persp) != 0) ||
      (PyDict_SetItemString(((C_constant *)Types)->dict,
                            "ortho", ortho) != 0) ||
      (PyDict_SetItemString(((C_constant *)Modes)->dict,
                            "showLimits", showLimits) != 0) ||
      (PyDict_SetItemString(((C_constant *)Modes)->dict,
                             "showMist", showMist) != 0))
  {
    /* One or more value setting actions has gone wwrong. Clean up. */
    goto fail;
  }

  cam->camera = blenderCam; /* it's NULL when creating only camera "data" */
  return ((PyObject*)cam);

fail:
  Py_XDECREF(name);
  Py_XDECREF(type);
  Py_XDECREF(mode);
  Py_XDECREF(lens);
  Py_XDECREF(clipStart);
  Py_XDECREF(clipEnd);
  Py_XDECREF(drawSize);
  Py_XDECREF(Types);
  Py_XDECREF(persp);
  Py_XDECREF(ortho);
  Py_XDECREF(Modes);
  Py_XDECREF(showLimits);
  Py_XDECREF(showMist);
  Py_DECREF(cam->dict);
  Py_DECREF((PyObject *)cam);
  return NULL;
}

/*****************************************************************************/
/* Function:    CameraDeAlloc                                                */
/* Description: This is a callback function for the C_Camera type. It is     */
/*              the destructor function.                                     */
/*****************************************************************************/
static void CameraDeAlloc (C_Camera *self)
{
  Py_DECREF(self->dict);
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    CameraGetAttr                                                */
/* Description: This is a callback function for the C_Camera type. It is     */
/*              the function that accesses C_Camera member variables and     */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject* CameraGetAttr (C_Camera *cam, char *name)
{
  /* first try the attributes dictionary */
  if (cam->dict)
  {
    PyObject *v = PyDict_GetItemString(cam->dict, name);
    if (v)
    {
      Py_INCREF(v); /* was a borrowed ref */
      return v;
    }
  }

  /* not an attribute, search the methods table */
  return Py_FindMethod(C_Camera_methods, (PyObject *)cam, name);
}

/*****************************************************************************/
/* Function:    CameraSetAttr                                                */
/* Description: This is a callback function for the C_Camera type. It is the */
/*              function that changes Camera Data members values. If this    */
/*              data is linked to a Blender Camera, it also gets updated.    */
/*****************************************************************************/
static int CameraSetAttr (C_Camera *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;

  if (self->dict == NULL)
  {
    return -1;
  }

/* We're playing a trick on the Python API users here.  Even if they use
 * Camera.member = val instead of Camera.setMember(value), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Camera structure when necessary. */

  valtuple = PyTuple_New(1); /* the set* functions expect a tuple */

  if (!valtuple)
  {
    return EXPP_intError(PyExc_MemoryError,
                         "CameraSetAttr: couldn't create PyTuple");
  }

  if (PyTuple_SetItem(valtuple, 0, value) != 0)
  {
    Py_DECREF(value); /* PyTuple_SetItem incref's value even when it fails */
    Py_DECREF(valtuple);
    return EXPP_intError(PyExc_RuntimeError,
                        "CameraSetAttr: couldn't fill tuple");
  }

  if (strcmp (name, "name") == 0)
    error = Camera_rename (self, valtuple);
  else if (strcmp (name, "type") == 0)
    error = Camera_setIntType (self, valtuple); /* special case */
  else if (strcmp (name, "mode") == 0)
    error = Camera_setIntMode (self, valtuple); /* special case */
  else if (strcmp (name, "lens") == 0)
    error = Camera_setLens (self, valtuple);
  else if (strcmp (name, "clipStart") == 0)
    error = Camera_setClipStart (self, valtuple);
  else if (strcmp (name, "clipEnd") == 0)
    error = Camera_setClipEnd (self, valtuple);
  else if (strcmp (name, "drawSize") == 0)
    error = Camera_setDrawSize (self, valtuple);
  else
  {
    /* Error: no such member in the Camera Data structure */
    Py_DECREF(value);
    Py_DECREF(valtuple);
    return (EXPP_intError (PyExc_KeyError,
                           "attribute not found"));
  }

  if (error == Py_None)
  {
    return 0; /* normal exit */
  }

  Py_DECREF(value);
  Py_DECREF(valtuple);

  return -1;
}

/*****************************************************************************/
/* Function:    CameraPrint                                                  */
/* Description: This is a callback function for the C_Camera type. It        */
/*              builds a meaninful string to 'print' camera objects.         */
/*****************************************************************************/
static int CameraPrint(C_Camera *self, FILE *fp, int flags)
{ 
  char *lstate = "unlinked";
  char *name;

  if (self->linked)
  {
    lstate = "linked";
  }

  name = PyString_AsString(Camera_getName(self));

  fprintf(fp, "[Camera \"%s\" (%s)]", name, lstate);

  return 0;
}

/*****************************************************************************/
/* Function:    CameraRepr                                                   */
/* Description: This is a callback function for the C_Camera type. It        */
/*              builds a meaninful string to represent camera objects.       */
/*****************************************************************************/
static PyObject *CameraRepr (C_Camera *self)
{
  char buf[64];
  char *lstate = "unlinked";
  char *name;

  if (self->linked)
  {
    lstate = "linked";
  }

  name = PyString_AsString(Camera_getName(self));

  PyOS_snprintf(buf, sizeof(buf), "[Camera \"%s\" (%s)]", name, lstate);

  return PyString_FromString(buf);
}
