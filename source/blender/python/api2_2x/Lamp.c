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

#include "Lamp.h"

/*****************************************************************************/
/* Function:              M_Lamp_New                                         */
/* Python equivalent:     Blender.Lamp.New                                   */
/*****************************************************************************/
static PyObject *M_Lamp_New(PyObject *self, PyObject *args, PyObject *keywords)
{
  char        *type_str = "Lamp";
  char        *name_str = "LampData";
  static char *kwlist[] = {"type_str", "name_str", NULL};
  C_Lamp      *py_lamp; /* for Lamp Data object wrapper in Python */
  Lamp        *bl_lamp; /* for actual Lamp Data we create in Blender */
  char        buf[21];

  printf ("In Lamp_New()\n");

  if (!PyArg_ParseTupleAndKeywords(args, keywords, "|ss", kwlist,
                          &type_str, &name_str))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected string(s) or empty argument"));

  bl_lamp = add_lamp(); /* first create in Blender */
  if (bl_lamp) /* now create the wrapper obj in Python */
    py_lamp = (C_Lamp *)PyObject_NEW(C_Lamp, &Lamp_Type);
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                            "couldn't create Lamp Data in Blender"));

  if (py_lamp == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                            "couldn't create Lamp Data object"));

  py_lamp->lamp = bl_lamp; /* link Python lamp wrapper with Blender Lamp */

  if (strcmp (type_str, "Lamp") == 0)
    bl_lamp->type = (short)EXPP_LAMP_TYPE_LAMP;
  else if (strcmp (type_str, "Sun") == 0)
    bl_lamp->type = (short)EXPP_LAMP_TYPE_SUN;
  else if (strcmp (type_str, "Spot") == 0)
    bl_lamp->type = (short)EXPP_LAMP_TYPE_SPOT;
  else if (strcmp (type_str, "Hemi") == 0)
    bl_lamp->type = (short)EXPP_LAMP_TYPE_HEMI;
  else
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "unknown lamp type"));

  if (strcmp(name_str, "LampData") == 0)
    return (PyObject *)py_lamp;
  else { /* user gave us a name for the lamp, use it */
    PyOS_snprintf(buf, sizeof(buf), "%s", name_str);
    rename_id(&bl_lamp->id, buf);
  }

  return (PyObject *)py_lamp;
}

/*****************************************************************************/
/* Function:              M_Lamp_Get                                         */
/* Python equivalent:     Blender.Lamp.Get                                   */
/* Description:           Receives a string and returns the lamp data obj    */
/*                        whose name matches the string.  If no argument is  */
/*                        passed in, a list of all lamp data names in the    */
/*                        current scene is returned.                         */
/*****************************************************************************/
static PyObject *M_Lamp_Get(PyObject *self, PyObject *args)
{
  char *name = NULL;
  Lamp *lamp_iter;

	if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument (or nothing)"));

  lamp_iter = G.main->lamp.first;

	if (name) { /* (name) - Search lamp by name */

    C_Lamp *wanted_lamp = NULL;

    while ((lamp_iter) && (wanted_lamp == NULL)) {
      if (strcmp (name, lamp_iter->id.name+2) == 0) {
        wanted_lamp = (C_Lamp *)PyObject_NEW(C_Lamp, &Lamp_Type);
				if (wanted_lamp) wanted_lamp->lamp = lamp_iter;
      }
      lamp_iter = lamp_iter->id.next;
    }

    if (wanted_lamp == NULL) { /* Requested lamp doesn't exist */
      char error_msg[64];
      PyOS_snprintf(error_msg, sizeof(error_msg),
                      "Lamp \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }

    return (PyObject *)wanted_lamp;
	}

	else { /* () - return a list of all lamps in the scene */
    int index = 0;
    PyObject *lamplist, *pystr;

    lamplist = PyList_New (BLI_countlist (&(G.main->lamp)));

    if (lamplist == NULL)
      return (PythonReturnErrorObject (PyExc_MemoryError,
              "couldn't create PyList"));

		while (lamp_iter) {
      pystr = PyString_FromString (lamp_iter->id.name+2);

			if (!pystr)
				return (PythonReturnErrorObject (PyExc_MemoryError,
									"couldn't create PyString"));

			PyList_SET_ITEM (lamplist, index, pystr);

      lamp_iter = lamp_iter->id.next;
      index++;
		}

		return (lamplist);
	}
}

/*****************************************************************************/
/* Function:              M_Lamp_Init                                        */
/*****************************************************************************/
PyObject *M_Lamp_Init (void)
{
  PyObject  *submodule;

  printf ("In M_Lamp_Init()\n");

  submodule = Py_InitModule3("Blender.Lamp", M_Lamp_methods, M_Lamp_doc);

  return (submodule);
}

/*****************************************************************************/
/* Python C_Lamp methods:                                                    */
/*****************************************************************************/
static PyObject *Lamp_getName(C_Lamp *self)
{
  PyObject *attr = PyString_FromString(self->lamp->id.name+2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.name attribute"));
}

static PyObject *Lamp_getType(C_Lamp *self)
{ 
  PyObject *attr = PyInt_FromLong(self->lamp->type);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.type attribute"));
}

static PyObject *Lamp_getMode(C_Lamp *self)
{
  PyObject *attr = PyInt_FromLong(self->lamp->mode);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.mode attribute"));
}

static PyObject *Lamp_getSamples(C_Lamp *self)
{
  PyObject *attr = PyInt_FromLong(self->lamp->samp);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.samples attribute"));
}

static PyObject *Lamp_getBufferSize(C_Lamp *self)
{
  PyObject *attr = PyInt_FromLong(self->lamp->bufsize);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.bufferSize attribute"));
}

static PyObject *Lamp_getHaloStep(C_Lamp *self)
{
  PyObject *attr = PyInt_FromLong(self->lamp->shadhalostep);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.haloStep attribute"));
}

static PyObject *Lamp_getEnergy(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->energy);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.energy attribute"));
}

static PyObject *Lamp_getDist(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->dist);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.dist attribute"));
}

static PyObject *Lamp_getSpotSize(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->spotsize);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.spotSize attribute"));
}

static PyObject *Lamp_getSpotBlend(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->spotblend);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.spotBlend attribute"));
}

static PyObject *Lamp_getClipStart(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->clipsta);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.clipStart attribute"));
}

static PyObject *Lamp_getClipEnd(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->clipend);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.clipEnd attribute"));
}

static PyObject *Lamp_getBias(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->bias);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.bias attribute"));
}

static PyObject *Lamp_getSoftness(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->soft);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.softness attribute"));
}

static PyObject *Lamp_getHaloInt(C_Lamp *self)
{
  PyObject *attr = PyFloat_FromDouble(self->lamp->haint);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.haloInt attribute"));
}

static PyObject *Lamp_getQuad1(C_Lamp *self)
{ /* should we complain if Lamp is not of type Quad? */
  PyObject *attr = PyFloat_FromDouble(self->lamp->att1);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.quad1 attribute"));
}

static PyObject *Lamp_getQuad2(C_Lamp *self)
{ /* should we complain if Lamp is not of type Quad? */
  PyObject *attr = PyFloat_FromDouble(self->lamp->att2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
          "couldn't get Lamp.quad2 attribute"));
}

static PyObject *Lamp_setName(C_Lamp *self, PyObject *args)
{
  char *name;
  char buf[21];

  if (!PyArg_ParseTuple(args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument"));
  
  PyOS_snprintf(buf, sizeof(buf), "%s", name);

  rename_id(&self->lamp->id, buf);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setType(C_Lamp *self, PyObject *args)
{
  char *type;

  if (!PyArg_ParseTuple(args, "s", &type))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected string argument"));

  if (strcmp (type, "Lamp") == 0)
    self->lamp->type = (short)EXPP_LAMP_TYPE_LAMP;
  else if (strcmp (type, "Sun") == 0)
    self->lamp->type = (short)EXPP_LAMP_TYPE_SUN; 
  else if (strcmp (type, "Spot") == 0)
    self->lamp->type = (short)EXPP_LAMP_TYPE_SPOT;  
  else if (strcmp (type, "Hemi") == 0)
    self->lamp->type = (short)EXPP_LAMP_TYPE_HEMI;  
  else
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "unknown lamp type"));

  Py_INCREF(Py_None);
  return Py_None;
}

/* This one is 'private'. It is not really a method, just a helper function for
 * when script writers use Lamp.type = t instead of Lamp.setType(t), since in
 * the first case t shoud be an int and in the second it should be a string. So
 * while the method setType expects a string ('persp' or 'ortho') or an empty
 * argument, this function should receive an int (0 or 1). */
static PyObject *Lamp_setIntType(C_Lamp *self, PyObject *args)
{
  short value;

  if (!PyArg_ParseTuple(args, "h", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [0,3]"));

  if (value >= 0 && value <= 3)
    self->lamp->type = value;
  else
    return (EXPP_ReturnPyObjError (PyExc_ValueError,
            "expected int argument in [0,3]"));

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setMode(C_Lamp *self, PyObject *args)
{/* Quad, Sphere, Shadows, Halo, Layer, Negative, OnlyShadow, Square */
  char *m[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  short i, flag = 0;

  if (!PyArg_ParseTuple(args, "|ssssssss", &m[0], &m[1], &m[2],
                        &m[3], &m[4], &m[5], &m[6], &m[7]))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
            "expected from none to eight string argument(s)"));

  for (i = 0; i < 8; i++) {
    if (m[i] == NULL) break;
    if (strcmp(m[i], "Shadows") == 0)
      flag |= (short)EXPP_LAMP_MODE_SHADOWS;
    else if (strcmp(m[i], "Halo") == 0)
      flag |= (short)EXPP_LAMP_MODE_HALO;
    else if (strcmp(m[i], "Layer") == 0)
      flag |= (short)EXPP_LAMP_MODE_LAYER;
    else if (strcmp(m[i], "Quad") == 0)
      flag |= (short)EXPP_LAMP_MODE_QUAD;
    else if (strcmp(m[i], "Negative") == 0)
      flag |= (short)EXPP_LAMP_MODE_NEGATIVE;
    else if (strcmp(m[i], "OnlyShadow") == 0)
      flag |= (short)EXPP_LAMP_MODE_ONLYSHADOW;
    else if (strcmp(m[i], "Sphere") == 0)
      flag |= (short)EXPP_LAMP_MODE_SPHERE;
    else if (strcmp(m[i], "Square") == 0)
      flag |= (short)EXPP_LAMP_MODE_SQUARE;
    else
      return (EXPP_ReturnPyObjError (PyExc_AttributeError,
              "unknown lamp flag argument"));
  }

  self->lamp->mode = flag;

  Py_INCREF(Py_None);
  return Py_None;
}

/* Another helper function, for the same reason.
 * (See comment before Lamp_setIntType above). */
static PyObject *Lamp_setIntMode(C_Lamp *self, PyObject *args)
{
  short value;

  if (!PyArg_ParseTuple(args, "h", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument"));

/* well, with so many flag bits, we just accept any short int, no checking */
  self->lamp->mode = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setSamples(C_Lamp *self, PyObject *args)
{
  short value;

  if (!PyArg_ParseTuple(args, "h", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [1,16]"));

  if (value >= EXPP_LAMP_SAMPLES_MIN &&
      value <= EXPP_LAMP_SAMPLES_MAX)
    self->lamp->samp = value;
  else
    return (EXPP_ReturnPyObjError (PyExc_ValueError,
            "expected int argument in [1,16]"));

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setBufferSize(C_Lamp *self, PyObject *args)
{
  short value;

  if (!PyArg_ParseTuple(args, "h", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [512, 5120]"));
	
  if (value >= EXPP_LAMP_BUFFERSIZE_MIN &&
      value <= EXPP_LAMP_BUFFERSIZE_MAX)
    self->lamp->bufsize = value;
  else
    return (EXPP_ReturnPyObjError (PyExc_ValueError,
            "expected int argument in [512, 5120]"));

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setHaloStep(C_Lamp *self, PyObject *args)
{
  short value;

  if (!PyArg_ParseTuple(args, "h", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected int argument in [0,12]"));

  if (value >= EXPP_LAMP_HALOSTEP_MIN &&
      value <= EXPP_LAMP_HALOSTEP_MAX)
    self->lamp->shadhalostep = value;
  else
    return (EXPP_ReturnPyObjError (PyExc_ValueError,
            "expected int argument in [0,12]"));

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setColorComponent(C_Lamp *self, char *key, PyObject *args)
{
  float value;
  
  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument in [0.0, 1.0]"));

  value = EXPP_ClampFloat (value, 0.0, 1.0);
    
  if (!strcmp(key, "R"))
    self->lamp->r = value;
  else if (!strcmp(key, "G"))
    self->lamp->g = value;
  else if (!strcmp(key, "B"))
    self->lamp->b = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setEnergy(C_Lamp *self, PyObject *args)
{
  float value;
  
  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_ENERGY_MIN, EXPP_LAMP_ENERGY_MAX);
  self->lamp->energy = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setDist(C_Lamp *self, PyObject *args)
{
  float value;
  
  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_DIST_MIN, EXPP_LAMP_DIST_MAX);
  self->lamp->dist = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setSpotSize(C_Lamp *self, PyObject *args)
{
  float value;
  
  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_SPOTSIZE_MIN,
									EXPP_LAMP_SPOTSIZE_MAX);
  self->lamp->spotsize = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setSpotBlend(C_Lamp *self, PyObject *args)
{
  float value;
  
  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_SPOTBLEND_MIN,
                  EXPP_LAMP_SPOTBLEND_MAX);
  self->lamp->spotblend = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setClipStart(C_Lamp *self, PyObject *args)
{
  float value;
  
  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));
  
  value = EXPP_ClampFloat (value, EXPP_LAMP_CLIPSTART_MIN,
                  EXPP_LAMP_CLIPSTART_MAX);
  self->lamp->clipsta = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setClipEnd(C_Lamp *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_CLIPEND_MIN,
                  EXPP_LAMP_CLIPEND_MAX);
  self->lamp->clipend = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setBias(C_Lamp *self, PyObject *args)
{
  float value;
  
  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_BIAS_MIN, EXPP_LAMP_BIAS_MAX);
  self->lamp->bias = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setSoftness(C_Lamp *self, PyObject *args)
{
  float value;
  
  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_SOFTNESS_MIN,
                  EXPP_LAMP_SOFTNESS_MAX);
  self->lamp->soft = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setHaloInt(C_Lamp *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_HALOINT_MIN,
                  EXPP_LAMP_HALOINT_MAX);
  self->lamp->haint = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setQuad1(C_Lamp *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_QUAD1_MIN,
                  EXPP_LAMP_QUAD1_MAX);
  self->lamp->att1 = value;

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *Lamp_setQuad2(C_Lamp *self, PyObject *args)
{
  float value;

  if (!PyArg_ParseTuple(args, "f", &value))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
            "expected float argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_QUAD2_MIN,
                  EXPP_LAMP_QUAD2_MAX);
  self->lamp->att2 = value;

  Py_INCREF(Py_None);
  return Py_None;
}

/*****************************************************************************/
/* Function:    LampDeAlloc                                                  */
/* Description: This is a callback function for the C_Lamp type. It is       */
/*              the destructor function.                                     */
/*****************************************************************************/
static void LampDeAlloc (C_Lamp *self)
{
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    LampGetAttr                                                  */
/* Description: This is a callback function for the C_Lamp type. It is       */
/*              the function that accesses C_Lamp member variables and       */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject* LampGetAttr (C_Lamp *self, char *name)
{
  PyObject *attr = Py_None;

  if (strcmp(name, "name") == 0)
    attr = PyString_FromString(self->lamp->id.name+2);
  else if (strcmp(name, "type") == 0)
    attr = PyInt_FromLong(self->lamp->type);
  else if (strcmp(name, "mode") == 0)
    attr = PyInt_FromLong(self->lamp->mode);
  else if (strcmp(name, "samples") == 0)
    attr = PyInt_FromLong(self->lamp->samp);
  else if (strcmp(name, "bufferSize") == 0)
    attr = PyInt_FromLong(self->lamp->bufsize);
  else if (strcmp(name, "haloStep") == 0)
    attr = PyInt_FromLong(self->lamp->shadhalostep);
  else if (strcmp(name, "R") == 0)
    attr = PyFloat_FromDouble(self->lamp->r);
  else if (strcmp(name, "G") == 0)
    attr = PyFloat_FromDouble(self->lamp->g);
  else if (strcmp(name, "B") == 0)
    attr = PyFloat_FromDouble(self->lamp->b);
  else if (strcmp(name, "energy") == 0)
    attr = PyFloat_FromDouble(self->lamp->energy);
  else if (strcmp(name, "dist") == 0)
    attr = PyFloat_FromDouble(self->lamp->dist);
  else if (strcmp(name, "spotSize") == 0)
    attr = PyFloat_FromDouble(self->lamp->spotsize);
  else if (strcmp(name, "spotBlend") == 0)
    attr = PyFloat_FromDouble(self->lamp->spotblend);
  else if (strcmp(name, "clipStart") == 0)
    attr = PyFloat_FromDouble(self->lamp->clipsta);
  else if (strcmp(name, "clipEnd") == 0)
    attr = PyFloat_FromDouble(self->lamp->clipend);
  else if (strcmp(name, "bias") == 0)
    attr = PyFloat_FromDouble(self->lamp->bias);
  else if (strcmp(name, "softness") == 0)
    attr = PyFloat_FromDouble(self->lamp->soft);
  else if (strcmp(name, "haloInt") == 0)
    attr = PyFloat_FromDouble(self->lamp->haint);
  else if (strcmp(name, "quad1") == 0)
    attr = PyFloat_FromDouble(self->lamp->att1);
  else if (strcmp(name, "quad2") == 0)
    attr = PyFloat_FromDouble(self->lamp->att2);

  else if (strcmp(name, "Types") == 0) {
    attr = Py_BuildValue("{s:h,s:h,s:h,s:h}",
                    "Lamp", EXPP_LAMP_TYPE_LAMP,
                    "Sun" , EXPP_LAMP_TYPE_SUN,
                    "Spot", EXPP_LAMP_TYPE_SPOT,
                    "Hemi", EXPP_LAMP_TYPE_HEMI);
  }

  else if (strcmp(name, "Modes") == 0) {
    attr = Py_BuildValue("{s:h,s:h,s:h,s:h,s:h,s:h,s:h,s:h}",
                    "Shadows",    EXPP_LAMP_MODE_SHADOWS,
                    "Halo",       EXPP_LAMP_MODE_HALO,
                    "Layer",      EXPP_LAMP_MODE_LAYER,
                    "Quad",       EXPP_LAMP_MODE_QUAD,
                    "Negative",   EXPP_LAMP_MODE_NEGATIVE,
                    "OnlyShadow", EXPP_LAMP_MODE_ONLYSHADOW,
                    "Sphere",     EXPP_LAMP_MODE_SPHERE,
                    "Square",     EXPP_LAMP_MODE_SQUARE);
  }

  else if (strcmp(name, "__members__") == 0) {
    /* 22 entries */
    attr = Py_BuildValue("[s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s,s]",
                    "name", "type", "mode", "samples", "bufferSize",
                    "haloStep", "R", "G", "B", "energy", "dist",
                    "spotSize", "spotBlend", "clipStart", "clipEnd",
                    "bias", "softness", "haloInt", "quad1", "quad2",
                    "Types", "Modes");
  }

  if (!attr)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
                      "couldn't create PyObject"));

  if (attr != Py_None) return attr; /* member attribute found, return it */

  /* not an attribute, search the methods table */
  return Py_FindMethod(C_Lamp_methods, (PyObject *)self, name);
}

/*****************************************************************************/
/* Function:    LampSetAttr                                                  */
/* Description: This is a callback function for the C_Lamp type. It is the   */
/*              function that changes Lamp Data members values. If this      */
/*              data is linked to a Blender Lamp, it also gets updated.      */
/*****************************************************************************/
static int LampSetAttr (C_Lamp *self, char *name, PyObject *value)
{
  PyObject *valtuple; 
  PyObject *error = NULL;

  valtuple = Py_BuildValue("(N)", value); /* the set* functions expect a tuple */

  if (!valtuple)
    return EXPP_ReturnIntError(PyExc_MemoryError,
                  "LampSetAttr: couldn't create tuple");

  if (strcmp (name, "name") == 0)
    error = Lamp_setName (self, valtuple);
  else if (strcmp (name, "type") == 0)
    error = Lamp_setIntType (self, valtuple); /* special case */
  else if (strcmp (name, "mode") == 0)
    error = Lamp_setIntMode (self, valtuple); /* special case */
  else if (strcmp (name, "samples") == 0)
    error = Lamp_setSamples (self, valtuple);
  else if (strcmp (name, "bufferSize") == 0)
    error = Lamp_setBufferSize (self, valtuple);
  else if (strcmp (name, "haloStep") == 0)
    error = Lamp_setHaloStep (self, valtuple);
  else if (strcmp (name, "R") == 0)
    error = Lamp_setColorComponent (self, "R", valtuple);
  else if (strcmp (name, "G") == 0)
    error = Lamp_setColorComponent (self, "G", valtuple);
  else if (strcmp (name, "B") == 0)
    error = Lamp_setColorComponent (self, "B", valtuple);
  else if (strcmp (name, "energy") == 0)
    error = Lamp_setEnergy (self, valtuple);
  else if (strcmp (name, "dist") == 0)
    error = Lamp_setDist (self, valtuple);
  else if (strcmp (name, "spotSize") == 0)
    error = Lamp_setSpotSize (self, valtuple);
  else if (strcmp (name, "spotBlend") == 0)
    error = Lamp_setSpotBlend (self, valtuple);
  else if (strcmp (name, "clipStart") == 0)
    error = Lamp_setClipStart (self, valtuple);
  else if (strcmp (name, "clipEnd") == 0)
    error = Lamp_setClipEnd (self, valtuple);
  else if (strcmp (name, "bias") == 0)
    error = Lamp_setBias (self, valtuple);
  else if (strcmp (name, "softness") == 0)
    error = Lamp_setSoftness (self, valtuple);
  else if (strcmp (name, "haloInt") == 0)
    error = Lamp_setHaloInt (self, valtuple);
  else if (strcmp (name, "quad1") == 0)
    error = Lamp_setQuad1 (self, valtuple);
  else if (strcmp (name, "quad2") == 0)
    error = Lamp_setQuad2 (self, valtuple);
  
  else { /* Error */
    Py_DECREF(valtuple);
  
    if ((strcmp (name, "Types") == 0) || /* user tried to change a */
        (strcmp (name, "Modes") == 0))   /* constant dict type ... */
      return (EXPP_ReturnIntError (PyExc_AttributeError,
                   "constant dictionary -- cannot be changed"));

    else /* ... or no member with the given name was found */
      return (EXPP_ReturnIntError (PyExc_KeyError,
                   "attribute not found"));
  }

  Py_DECREF(valtuple);
  
  if (error != Py_None) return -1;

  Py_DECREF(Py_None); /* was incref'ed by the called Lamp_set* function */
  return 0; /* normal exit */
}

/*****************************************************************************/
/* Function:    LampPrint                                                    */
/* Description: This is a callback function for the C_Lamp type. It          */
/*              builds a meaninful string to 'print' lamp objects.           */
/*****************************************************************************/
static int LampPrint(C_Lamp *self, FILE *fp, int flags)
{ 
  fprintf(fp, "[Lamp \"%s\"]", self->lamp->id.name+2);
  return 0;
}

/*****************************************************************************/
/* Function:    LampRepr                                                     */
/* Description: This is a callback function for the C_Lamp type. It          */
/*              builds a meaninful string to represent lamp objects.         */
/*****************************************************************************/
static PyObject *LampRepr (C_Lamp *self)
{
  return PyString_FromString(self->lamp->id.name+2);
}
