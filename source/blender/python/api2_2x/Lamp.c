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
	char        *name_str = "Data";
  static char *kwlist[] = {"type_str", "name_str", NULL};
  C_Lamp      *lamp;
	PyObject    *type, *name;
	int         type_int;
	char        buf[21];

  printf ("In Lamp_New()\n");

  if (!PyArg_ParseTupleAndKeywords(args, keywords, "|ss", kwlist,
													&type_str, &name_str))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected string(s) or empty argument"));

	if (!strcmp (type_str, "Lamp"))
		type_int = EXPP_LAMP_TYPE_LAMP;
	else if (!strcmp (type_str, "Sun"))
		type_int = EXPP_LAMP_TYPE_SUN;
	else if (!strcmp (type_str, "Spot"))
		type_int = EXPP_LAMP_TYPE_SPOT;
	else if (!strcmp (type_str, "Hemi"))
		type_int = EXPP_LAMP_TYPE_HEMI;
	else
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "unknown lamp type"));

	lamp = (C_Lamp *)LampCreatePyObject(NULL);

  if (lamp == NULL)
    return (PythonReturnErrorObject (PyExc_MemoryError,
														"couldn't create Lamp Data object"));

	type = PyInt_FromLong(type_int);
	if (type)	LampSetAttr(lamp, "type", type);
	else {
		Py_DECREF((PyObject *)lamp);
		return (PythonReturnErrorObject (PyExc_MemoryError,
														"couldn't create Python string"));
	}

	if (strcmp(name_str, "Data") == 0)
		return (PyObject *)lamp;

	PyOS_snprintf(buf, sizeof(buf), "%s", name_str);
	name = PyString_FromString(buf);
	if (name) LampSetAttr(lamp, "name", name);
	else {
		Py_DECREF((PyObject *)lamp);
		return (PythonReturnErrorObject (PyExc_MemoryError,
														"couldn't create Python string"));
	}

	return (PyObject *)lamp;
}

/*****************************************************************************/
/* Function:              M_Lamp_Get                                         */
/* Python equivalent:     Blender.Lamp.Get                                   */
/*****************************************************************************/
static PyObject *M_Lamp_Get(PyObject *self, PyObject *args)
{
  char     *name;
  Lamp   *lamp_iter;
	C_Lamp *wanted_lamp;

  printf ("In Lamp_Get()\n");
  if (!PyArg_ParseTuple(args, "s", &name))
  {
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected string argument"));
  }

  /* Use the name to search for the lamp requested. */
  wanted_lamp = NULL;
  lamp_iter = G.main->lamp.first;

  while ((lamp_iter) && (wanted_lamp == NULL)) {

	  if (strcmp (name, GetIdName (&(lamp_iter->id))) == 0)
      wanted_lamp = (C_Lamp *)LampCreatePyObject(lamp_iter);

		lamp_iter = lamp_iter->id.next;
  }

  if (wanted_lamp == NULL) {
  /* No lamp exists with the name specified in the argument name. */
    char error_msg[64];
    PyOS_snprintf(error_msg, sizeof(error_msg),
                    "Lamp \"%s\" not found", name);
    return (PythonReturnErrorObject (PyExc_NameError, error_msg));
  }
	wanted_lamp->linked = 1; /* TRUE: linked to a Blender Lamp Object */
  return ((PyObject*)wanted_lamp);
}

/*****************************************************************************/
/* Function:              M_Lamp_Init                                        */
/*****************************************************************************/
PyObject *M_Lamp_Init (void)
{
  PyObject  *module;

  printf ("In M_Lamp_Init()\n");

  module = Py_InitModule3("Lamp", M_Lamp_methods, M_Lamp_doc);

  return (module);
}

/*****************************************************************************/
/* Python C_Lamp methods:                                                    */
/*****************************************************************************/
static PyObject *Lamp_getName(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "name");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
	return (PythonReturnErrorObject (PyExc_RuntimeError,
					"couldn't get Lamp.name attribute"));
}

static PyObject *Lamp_getType(C_Lamp *self)
{ 
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "type");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.type attribute"));
}

static PyObject *Lamp_getMode(C_Lamp *self)
{/* XXX improve this, add the constants to the Lamp dict */
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "mode");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.mode attribute"));
}

static PyObject *Lamp_getSamples(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "samples");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.samples attribute"));
}

static PyObject *Lamp_getBufferSize(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "bufferSize");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.bufferSize attribute"));
}

static PyObject *Lamp_getHaloStep(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "haloStep");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.haloStep attribute"));
}

static PyObject *Lamp_getEnergy(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "energy");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.energy attribute"));
}

static PyObject *Lamp_getDist(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "dist");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.dist attribute"));
}

static PyObject *Lamp_getSpotSize(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "spotSize");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.spotSize attribute"));
}

static PyObject *Lamp_getSpotBlend(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "spotBlend");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.spotBlend attribute"));
}

static PyObject *Lamp_getClipStart(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "clipStart");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.clipStart attribute"));
}

static PyObject *Lamp_getClipEnd(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "clipEnd");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.clipEnd attribute"));
}

static PyObject *Lamp_getBias(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "bias");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.bias attribute"));
}

static PyObject *Lamp_getSoftness(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "softness");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.softness attribute"));
}

static PyObject *Lamp_getHaloInt(C_Lamp *self)
{
	PyObject *attr;
	attr = PyDict_GetItemString(self->dict, "haloInt");
	if (attr) {
		Py_INCREF(attr);
		return attr;
	}
  return (PythonReturnErrorObject (PyExc_RuntimeError,
          "couldn't get Lamp.haloInt attribute"));
}
								
static PyObject *Lamp_rename(C_Lamp *self, PyObject *args)
{
	char *name_str;
	char buf[21];
	PyObject *name;

	if (!PyArg_ParseTuple(args, "s", &name_str))
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected string argument"));
	
	PyOS_snprintf(buf, sizeof(buf), "%s", name_str);
	
	if (self->linked) { /* update the Blender Lamp, too */
		ID *tmp_id = &self->lamp->id;
		rename_id(tmp_id, buf);
		PyOS_snprintf(buf, sizeof(buf), "%s", tmp_id->name+2);/* may have changed */
	}

	name = PyString_FromString(buf);

	if (!name)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyString Object"));

	if (PyDict_SetItemString(self->dict, "name", name) != 0) {
		Py_DECREF(name);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.name attribute"));
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setType(C_Lamp *self, PyObject *args)
{
	short value;
	char *type_str;
	PyObject *type;

	if (!PyArg_ParseTuple(args, "s", &type_str))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected one string argument"));

	if (strcmp (type_str, "Lamp") == 0)
		value = EXPP_LAMP_TYPE_LAMP;
	else if (strcmp (type_str, "Sun") == 0)
		value = EXPP_LAMP_TYPE_SUN;	
	else if (strcmp (type_str, "Spot") == 0)
		value = EXPP_LAMP_TYPE_SPOT;	
	else if (strcmp (type_str, "Hemi") == 0)
		value = EXPP_LAMP_TYPE_HEMI;	
  else
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "unknown lamp type"));

	type = PyInt_FromLong(value);
	if (!type)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyInt Object"));

	if (PyDict_SetItemString(self->dict, "type", type) != 0) {
		Py_DECREF(type);
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "couldn't set Lamp.type attribute"));
	}

	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->type = value;

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
	PyObject *type;

	if (!PyArg_ParseTuple(args, "h", &value))
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [0,3]"));

	if (value >= 0 && value <= 3)
		type = PyInt_FromLong(value);
	else
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [0,3]"));

	if (!type)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyInt Object"));

	if (PyDict_SetItemString(self->dict, "type", type) != 0) {
		Py_DECREF(type);
		return (PythonReturnErrorObject (PyExc_RuntimeError,
						"could not set Lamp.type attribute"));
	}

	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->type = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setMode(C_Lamp *self, PyObject *args)
{/* Quad, Sphere, Shadows, Halo, Layer, Negative, OnlyShadow, Square */
	char *m[8] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	short i, flag = 0;
	PyObject *mode;

	if (!PyArg_ParseTuple(args, "|ssssssss", &m[0], &m[1], &m[2],
												&m[3], &m[4], &m[5], &m[6], &m[7]))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected string argument(s)"));

	for (i = 0; i < 8; i++) {
		if (m[i] == NULL) break;
		if (strcmp(m[i], "Shadows") == 0)
			flag |= EXPP_LAMP_MODE_SHADOWS;
		else if (strcmp(m[i], "Halo") == 0)
			flag |= EXPP_LAMP_MODE_HALO;
		else if (strcmp(m[i], "Layer") == 0)
			flag |= EXPP_LAMP_MODE_LAYER;
		else if (strcmp(m[i], "Quad") == 0)
			flag |= EXPP_LAMP_MODE_QUAD;
		else if (strcmp(m[i], "Negative") == 0)
			flag |= EXPP_LAMP_MODE_NEGATIVE;
		else if (strcmp(m[i], "OnlyShadow") == 0)
			flag |= EXPP_LAMP_MODE_ONLYSHADOW;
		else if (strcmp(m[i], "Sphere") == 0)
			flag |= EXPP_LAMP_MODE_SPHERE;
		else if (strcmp(m[i], "Square") == 0)
			flag |= EXPP_LAMP_MODE_SQUARE;
  	else
    	return (PythonReturnErrorObject (PyExc_AttributeError,
      	      "unknown lamp flag argument"));
	}

	mode = PyInt_FromLong(flag);
	if (!mode)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyInt Object"));
	
	if (PyDict_SetItemString(self->dict, "mode", mode) != 0) {
		Py_DECREF(mode);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.mode attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->mode = flag;

	Py_INCREF(Py_None);
	return Py_None;
}

/* Another helper function, for the same reason.
 * (See comment before Lamp_setIntType above). */
static PyObject *Lamp_setIntMode(C_Lamp *self, PyObject *args)
{
	short value;
	PyObject *mode;

	if (!PyArg_ParseTuple(args, "h", &value))
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument"));

/* well, with so many flag bits, we just accept any short int, no checking */
	mode = PyInt_FromLong(value);

	if (!mode)
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"couldn't create PyInt object"));

	if (PyDict_SetItemString(self->dict, "mode", mode) != 0) {
		Py_DECREF(mode);
		return (PythonReturnErrorObject (PyExc_RuntimeError,
						"could not set Lamp.mode attribute"));
	}

	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->mode = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setSamples(C_Lamp *self, PyObject *args)
{
	short value;
	PyObject *samples;

	if (!PyArg_ParseTuple(args, "h", &value))
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [1,16]"));

	if (value >= EXPP_LAMP_SAMPLES_MIN &&
			value <= EXPP_LAMP_SAMPLES_MAX)
		samples = PyInt_FromLong(value);
	else
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [1,16]"));

	if (!samples)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyInt Object"));

	if (PyDict_SetItemString(self->dict, "samples", samples) != 0) {
		Py_DECREF(samples);
		return (PythonReturnErrorObject (PyExc_RuntimeError,
						"could not set Lamp.samples attribute"));
	}

	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->samp = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setBufferSize(C_Lamp *self, PyObject *args)
{
	short value;
	PyObject *bufferSize;

	if (!PyArg_ParseTuple(args, "h", &value))
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument, any of [512, 768, 1024, 1536, 2560]"));

	switch (value) {
		case  512:
		case  768:
		case 1024:
		case 1536:
		case 2560:
			bufferSize = PyInt_FromLong(value);
			break;
		default:
			return (PythonReturnErrorObject (PyExc_AttributeError,
							"expected int argument, any of [512, 768, 1024, 1536, 2560]"));
	}

	if (!bufferSize)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyInt Object"));

	if (PyDict_SetItemString(self->dict, "bufferSize", bufferSize) != 0) {
		Py_DECREF(bufferSize);
		return (PythonReturnErrorObject (PyExc_RuntimeError,
						"could not set Lamp.bufferSize attribute"));
	}

	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->bufsize = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setHaloStep(C_Lamp *self, PyObject *args)
{
	short value;
	PyObject *haloStep;

	if (!PyArg_ParseTuple(args, "h", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected int argument in [0,12]"));

	if (value >= EXPP_LAMP_HALOSTEP_MIN &&
			value <= EXPP_LAMP_HALOSTEP_MAX)
		haloStep = PyInt_FromLong(value);
	else
		return (PythonReturnErrorObject (PyExc_AttributeError,
						"expected int argument in [0,12]"));
	
	if (!haloStep)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyInt Object"));

	if (PyDict_SetItemString(self->dict, "haloStep", haloStep) != 0) {
		Py_DECREF(haloStep);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.haloStep attribute"));
	}

	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->shadhalostep = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setColorComponent(C_Lamp *self, char *key, PyObject *args)
{
	float value;
	PyObject *component;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected float argument in [0.0, 1.0]"));

	value = EXPP_ClampFloat (value, 0.0, 1.0);
	component = PyFloat_FromDouble(value);

	if (!component)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, key, component) != 0) {
		Py_DECREF(component);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp color component attribute"));
	}
	
	if (self->linked) { /* update the Blender Lamp, too */
		if (!strcmp(key, "R"))
			self->lamp->r = value;
		else if (!strcmp(key, "G"))
			self->lamp->g = value;
		else if (!strcmp(key, "B"))
			self->lamp->b = value;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setEnergy(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *energy;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected float argument"));

	value = EXPP_ClampFloat (value, EXPP_LAMP_ENERGY_MIN, EXPP_LAMP_ENERGY_MAX);
	energy = PyFloat_FromDouble(value);

	if (!energy)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "energy", energy) != 0) {
		Py_DECREF(energy);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.energy attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
		self->lamp->energy = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setDist(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *dist;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected float argument"));

	value = EXPP_ClampFloat (value, EXPP_LAMP_DIST_MIN, EXPP_LAMP_DIST_MAX);
	dist = PyFloat_FromDouble(value);
	
	if (!dist)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "dist", dist) != 0) {
		Py_DECREF(dist);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.dist attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
		self->lamp->dist = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setSpotSize(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *spotSize;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected float argument"));

	value = EXPP_ClampFloat (value, EXPP_LAMP_SPOTSIZE_MIN, EXPP_LAMP_SPOTSIZE_MAX);
	spotSize = PyFloat_FromDouble(value);
	
	if (!spotSize)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "spotSize", spotSize) != 0) {
		Py_DECREF(spotSize);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.spotSize attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
		self->lamp->spotsize = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setSpotBlend(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *spotBlend;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected float argument"));

	value = EXPP_ClampFloat (value, EXPP_LAMP_SPOTBLEND_MIN,
									EXPP_LAMP_SPOTBLEND_MAX);
	spotBlend = PyFloat_FromDouble(value);
	
	if (!spotBlend)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "spotBlend", spotBlend) != 0) {
		Py_DECREF(spotBlend);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.spotBlend attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
		self->lamp->spotblend = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setClipStart(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *clipStart;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected a float number as argument"));
	
	value = EXPP_ClampFloat (value, EXPP_LAMP_CLIPSTART_MIN,
									EXPP_LAMP_CLIPSTART_MAX);
	
	clipStart = PyFloat_FromDouble(value);
	
	if (!clipStart)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "clipStart", clipStart) != 0) {
		Py_DECREF(clipStart);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.clipStart attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->clipsta = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setClipEnd(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *clipEnd;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected a float number as argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_CLIPEND_MIN,
									EXPP_LAMP_CLIPEND_MAX);

	clipEnd = PyFloat_FromDouble(value);
	
	if (!clipEnd)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "clipEnd", clipEnd) != 0) {
		Py_DECREF(clipEnd);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.clipEnd attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->clipend = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setBias(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *bias;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected a float number as argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_BIAS_MIN, EXPP_LAMP_BIAS_MAX);

	bias = PyFloat_FromDouble(value);
	
	if (!bias)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "bias", bias) != 0) {
		Py_DECREF(bias);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.bias attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->bias = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setSoftness(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *softness;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected a float number as argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_SOFTNESS_MIN,
									EXPP_LAMP_SOFTNESS_MAX);

	softness = PyFloat_FromDouble(value);
	
	if (!softness)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "softness", softness) != 0) {
		Py_DECREF(softness);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.softness attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->soft = value;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *Lamp_setHaloInt(C_Lamp *self, PyObject *args)
{
	float value;
	PyObject *haloInt;
	
	if (!PyArg_ParseTuple(args, "f", &value))
    return (PythonReturnErrorObject (PyExc_AttributeError,
            "expected a float number as argument"));

  value = EXPP_ClampFloat (value, EXPP_LAMP_HALOINT_MIN,
									EXPP_LAMP_HALOINT_MAX);

	haloInt = PyFloat_FromDouble(value);
	
	if (!haloInt)
		return (PythonReturnErrorObject (PyExc_MemoryError,
						"couldn't create PyFloat Object"));

	if (PyDict_SetItemString(self->dict, "haloInt", haloInt) != 0) {
		Py_DECREF(haloInt);
    return (PythonReturnErrorObject (PyExc_RuntimeError,
            "couldn't set Lamp.haloInt attribute"));
	}
	
	if (self->linked) /* update the Blender Lamp, too */
    self->lamp->haint = value;

	Py_INCREF(Py_None);
	return Py_None;
}

/*****************************************************************************/
/* Function:    LampCreatePyObject                                           */
/* Description: This function will create a new C_Lamp.  If the Lamp         */
/*              struct passed to it is not NULL, it'll use its attributes.   */
/*****************************************************************************/
PyObject *LampCreatePyObject (Lamp *blenderLamp)
{
	PyObject *name, *type, *mode, *R, *G, *B, *energy, *dist;
	PyObject *spotSize, *spotBlend, *clipStart, *clipEnd;
	PyObject *bias, *softness, *samples, *bufferSize;
	PyObject *haloInt, *haloStep;
	PyObject *Types, *Lamp, *Sun, *Spot, *Hemi;
	PyObject *Modes, *Shadows, *Halo, *Layer, *Quad;
	PyObject *Negative, *OnlyShadow, *Sphere, *Square;
  C_Lamp   *lamp;

  printf ("In LampCreatePyObject\n");

	lamp = (C_Lamp *)PyObject_NEW(C_Lamp, &Lamp_Type);
	
	if (lamp == NULL)
		return NULL;

	lamp->linked = 0;
	lamp->dict = PyDict_New();

	if (lamp->dict == NULL) {
		Py_DECREF((PyObject *)lamp);
		return NULL;
	}

	if (blenderLamp == NULL) { /* Not linked to a Lamp Object yet */
		name = PyString_FromString("Data");
		type = PyInt_FromLong(EXPP_LAMP_TYPE);
		mode = PyInt_FromLong(EXPP_LAMP_MODE);
		samples = PyInt_FromLong(EXPP_LAMP_SAMPLES);
		bufferSize = PyInt_FromLong(EXPP_LAMP_BUFFERSIZE);
		haloStep = PyInt_FromLong(EXPP_LAMP_HALOSTEP);
 	  R = PyFloat_FromDouble(1.0);
 	  G = PyFloat_FromDouble(1.0);
 	  B = PyFloat_FromDouble(1.0);
 	  energy = PyFloat_FromDouble(EXPP_LAMP_ENERGY);
 	  dist = PyFloat_FromDouble(EXPP_LAMP_DIST);
 	  spotSize = PyFloat_FromDouble(EXPP_LAMP_SPOTSIZE);
 	  spotBlend = PyFloat_FromDouble(EXPP_LAMP_SPOTBLEND);
		clipStart = PyFloat_FromDouble(EXPP_LAMP_CLIPSTART);
		clipEnd = PyFloat_FromDouble(EXPP_LAMP_CLIPEND);
 	  bias = PyFloat_FromDouble(EXPP_LAMP_BIAS);
 	  softness = PyFloat_FromDouble(EXPP_LAMP_SOFTNESS);
		haloInt = PyFloat_FromDouble(EXPP_LAMP_HALOINT);
	}
	else { /* Lamp Object available, get its attributes directly */
		name = PyString_FromString(blenderLamp->id.name+2);
		type = PyInt_FromLong(blenderLamp->type);
		mode = PyInt_FromLong(blenderLamp->mode);
		samples = PyInt_FromLong(blenderLamp->samp);
		bufferSize = PyInt_FromLong(blenderLamp->bufsize);
		haloStep = PyInt_FromLong(blenderLamp->shadhalostep);
 	  R = PyFloat_FromDouble(blenderLamp->r);
 	  G = PyFloat_FromDouble(blenderLamp->g);
 	  B = PyFloat_FromDouble(blenderLamp->b);
 	  energy = PyFloat_FromDouble(blenderLamp->energy);
 	  dist = PyFloat_FromDouble(blenderLamp->dist);
 	  spotSize = PyFloat_FromDouble(blenderLamp->spotsize);
 	  spotBlend = PyFloat_FromDouble(blenderLamp->spotblend);
		clipStart = PyFloat_FromDouble(blenderLamp->clipsta);
		clipEnd = PyFloat_FromDouble(blenderLamp->clipend);
 	  bias = PyFloat_FromDouble(blenderLamp->bias);
 	  softness = PyFloat_FromDouble(blenderLamp->soft);
		haloInt = PyFloat_FromDouble(blenderLamp->haint);
		/*there's shadspotsize, too ... plus others, none in 2.25*/
	}

	Types = constant_New();
	Lamp  = PyInt_FromLong(EXPP_LAMP_TYPE_LAMP);
	Sun   = PyInt_FromLong(EXPP_LAMP_TYPE_SUN);
	Spot  = PyInt_FromLong(EXPP_LAMP_TYPE_SPOT);
	Hemi  = PyInt_FromLong(EXPP_LAMP_TYPE_HEMI);
	
	Modes = constant_New();
	Shadows = PyInt_FromLong(EXPP_LAMP_MODE_SHADOWS);
	Halo = PyInt_FromLong(EXPP_LAMP_MODE_HALO);
	Layer = PyInt_FromLong(EXPP_LAMP_MODE_LAYER);
	Quad = PyInt_FromLong(EXPP_LAMP_MODE_QUAD);
	Negative = PyInt_FromLong(EXPP_LAMP_MODE_NEGATIVE);
	OnlyShadow = PyInt_FromLong(EXPP_LAMP_MODE_ONLYSHADOW);
	Sphere = PyInt_FromLong(EXPP_LAMP_MODE_SPHERE);
	Square = PyInt_FromLong(EXPP_LAMP_MODE_SQUARE);

	if (name == NULL || type == NULL || mode == NULL ||
			samples == NULL || bufferSize == NULL || haloStep == NULL ||
			R == NULL || G == NULL || B == NULL || energy == NULL ||
			dist == NULL || spotSize == NULL || spotBlend == NULL ||
			clipStart == NULL || clipEnd == NULL || bias == NULL ||
			softness == NULL || haloInt == NULL || Types == NULL ||
			Lamp == NULL || Sun == NULL || Spot == NULL || Hemi == NULL ||
			Modes == NULL || Shadows == NULL || Halo == NULL ||
			Layer == NULL || Quad == NULL || Negative == NULL ||
			OnlyShadow == NULL || Sphere == NULL || Square == NULL) /* ack! */
		goto fail;

	if ((PyDict_SetItemString(lamp->dict, "name", name) != 0) ||
			(PyDict_SetItemString(lamp->dict, "type", type) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "mode", mode) != 0) ||
			(PyDict_SetItemString(lamp->dict, "samples", samples) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "bufferSize", bufferSize) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "R", R) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "G", G) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "B", B) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "energy", energy) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "dist", dist) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "spotSize", spotSize) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "spotBlend", spotBlend) != 0) ||
			(PyDict_SetItemString(lamp->dict, "clipStart", clipStart) != 0) ||
      (PyDict_SetItemString(lamp->dict, "clipEnd", clipEnd) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "bias", bias) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "softness", softness) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "haloInt", haloInt) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "haloStep", haloStep) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "Types", Types) != 0) ||
		  (PyDict_SetItemString(lamp->dict, "Modes", Modes) != 0) ||
			(PyDict_SetItemString(lamp->dict, "__members__",
														PyDict_Keys(lamp->dict)) != 0))
		goto fail;

	if ((PyDict_SetItemString(((C_constant *)Types)->dict,
														"Lamp", Lamp) != 0) ||
			(PyDict_SetItemString(((C_constant *)Types)->dict,
														"Sun", Sun) != 0) ||
			(PyDict_SetItemString(((C_constant *)Types)->dict,
														"Spot", Spot) != 0) ||
			(PyDict_SetItemString(((C_constant *)Types)->dict,
														"Hemi", Hemi) != 0) ||
			(PyDict_SetItemString(((C_constant *)Modes)->dict,
														"Shadows", Shadows) != 0) ||
			(PyDict_SetItemString(((C_constant *)Modes)->dict,
														"Halo", Halo) != 0) ||
			(PyDict_SetItemString(((C_constant *)Modes)->dict,
														"Layer", Layer) != 0) ||
			(PyDict_SetItemString(((C_constant *)Modes)->dict,
														"Quad", Quad) != 0) ||
			(PyDict_SetItemString(((C_constant *)Modes)->dict,
														"Negative", Negative) != 0) ||
			(PyDict_SetItemString(((C_constant *)Modes)->dict,
														"OnlyShadow", OnlyShadow) != 0) ||
			(PyDict_SetItemString(((C_constant *)Modes)->dict,
														"Sphere", Sphere) != 0) ||
			(PyDict_SetItemString(((C_constant *)Modes)->dict,
														"Square", Square) != 0))
		goto fail;

  lamp->lamp = blenderLamp; /* it's NULL when creating only lamp "data" */
  return ((PyObject*)lamp);

fail:
	Py_XDECREF(name);
	Py_XDECREF(type);
	Py_XDECREF(mode);
	Py_XDECREF(samples);
	Py_XDECREF(bufferSize);
	Py_XDECREF(R);
	Py_XDECREF(G);
	Py_XDECREF(B);
	Py_XDECREF(energy);
	Py_XDECREF(dist);
	Py_XDECREF(spotSize);
	Py_XDECREF(spotBlend);
	Py_XDECREF(clipStart);
	Py_XDECREF(clipEnd);
	Py_XDECREF(bias);
	Py_XDECREF(softness);
	Py_XDECREF(haloInt);
	Py_XDECREF(haloStep);
	Py_XDECREF(Types);
	Py_XDECREF(Lamp);
	Py_XDECREF(Sun);
	Py_XDECREF(Spot);
	Py_XDECREF(Hemi);
	Py_XDECREF(Modes);
	Py_XDECREF(Shadows);
	Py_XDECREF(Halo);
	Py_XDECREF(Layer);
	Py_XDECREF(Quad);
	Py_XDECREF(Negative);
	Py_XDECREF(OnlyShadow);
	Py_XDECREF(Sphere);
	Py_XDECREF(Square);
  Py_DECREF(lamp->dict);
	Py_DECREF((PyObject *)lamp);
	return NULL;
}

/*****************************************************************************/
/* Function:    LampDeAlloc                                                  */
/* Description: This is a callback function for the C_Lamp type. It is       */
/*              the destructor function.                                     */
/*****************************************************************************/
static void LampDeAlloc (C_Lamp *self)
{
	Py_DECREF(self->dict);
  PyObject_DEL (self);
}

/*****************************************************************************/
/* Function:    LampGetAttr                                                  */
/* Description: This is a callback function for the C_Lamp type. It is       */
/*              the function that accesses C_Lamp member variables and       */
/*              methods.                                                     */
/*****************************************************************************/
static PyObject* LampGetAttr (C_Lamp *lamp, char *name)
{/* first try the attributes dictionary */
	if (lamp->dict) {
		PyObject *v = PyDict_GetItemString(lamp->dict, name);
		if (v) {
			Py_INCREF(v); /* was a borrowed ref */
			return v;
		}
	}
/* not an attribute, search the methods table */
	return Py_FindMethod(C_Lamp_methods, (PyObject *)lamp, name);
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

	if (self->dict == NULL) return -1;

/* We're playing a trick on the Python API users here.  Even if they use
 * Lamp.member = val instead of Lamp.setMember(value), we end up using the
 * function anyway, since it already has error checking, clamps to the right
 * interval and updates the Blender Lamp structure when necessary. */

	valtuple = PyTuple_New(1); /* the set* functions expect a tuple */

	if (!valtuple)
		return EXPP_intError(PyExc_MemoryError,
									"LampSetAttr: couldn't create tuple");

	if (PyTuple_SetItem(valtuple, 0, value) != 0) {
		Py_DECREF(value); /* PyTuple_SetItem incref's value even when it fails */
		Py_DECREF(valtuple);
		return EXPP_intError(PyExc_MemoryError,
									 "LampSetAttr: couldn't fill tuple");
	}

	if (strcmp (name, "name") == 0)
    error = Lamp_rename (self, valtuple);
	else if	(strcmp (name, "type") == 0)
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
	else { /* Error: no such member in the Lamp Data structure */
		Py_DECREF(value);
		Py_DECREF(valtuple);
  	return (EXPP_intError (PyExc_AttributeError,
    	      "attribute not found"));
	}

	if (error == Py_None) return 0; /* normal exit */

	Py_DECREF(value);
	Py_DECREF(valtuple);

	return -1;
}

/*****************************************************************************/
/* Function:    LampPrint                                                    */
/* Description: This is a callback function for the C_Lamp type. It          */
/*              builds a meaninful string to 'print' lamp objects.           */
/*****************************************************************************/
static int LampPrint(C_Lamp *self, FILE *fp, int flags)
{ 
	char *lstate = "unlinked";
	char *name;

	if (self->linked)
		lstate = "linked";
	
	name = PyString_AsString(Lamp_getName(self));

	fprintf(fp, "[Lamp \"%s\" (%s)]", name, lstate);

  return 0;
}

/*****************************************************************************/
/* Function:    LampRepr                                                     */
/* Description: This is a callback function for the C_Lamp type. It          */
/*              builds a meaninful string to represent lamp objects.         */
/*****************************************************************************/
static PyObject *LampRepr (C_Lamp *self)
{
	char buf[64];
	char *lstate = "unlinked";
	char *name;

	if (self->linked)
		lstate = "linked";

	name = PyString_AsString(Lamp_getName(self));

	PyOS_snprintf(buf, sizeof(buf), "[Lamp \"%s\" (%s)]", name, lstate);

  return PyString_FromString(buf);
}
