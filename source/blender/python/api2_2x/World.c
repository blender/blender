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
 * Contributor(s): Jacques Guignot
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

/**
 * \file World.c
 * \ingroup scripts
 * \brief Blender.World Module and World Data PyObject implementation.
 *
 * Note: Parameters between "<" and ">" are optional.  But if one of them is
 * given, all preceding ones must be given, too.  Of course, this only relates
 * to the Python functions and methods described here and only inside Python
 * code. [ This will go to another file later, probably the main exppython
 * doc file].  XXX Better: put optional args with their default value:
 * (self, name = "MyName")
 */

#include <BKE_main.h>
#include <BKE_global.h>
#include <BKE_object.h>
#include <BKE_library.h>
#include <BLI_blenlib.h>

#include "World.h"


/*****************************************************************************/
/* Python World_Type callback function prototypes:                          */
/*****************************************************************************/
static void World_DeAlloc (BPy_World *self);
static int World_Print (BPy_World *self, FILE *fp, int flags);
static int World_SetAttr (BPy_World *self, char *name, PyObject *v);
static int World_Compare (BPy_World *a, BPy_World *b);
static PyObject *World_GetAttr (BPy_World *self, char *name);
static PyObject *World_Repr (BPy_World *self);

/*****************************************************************************/
/* Python World_Type structure definition:                                  */
/*****************************************************************************/
PyTypeObject World_Type =
{
  PyObject_HEAD_INIT(NULL)
  0,                                      /* ob_size */
  "World",                               /* tp_name */
  sizeof (BPy_World),                    /* tp_basicsize */
  0,                                      /* tp_itemsize */
  /* methods */
  (destructor)World_DeAlloc,             /* tp_dealloc */
  (printfunc)World_Print,                /* tp_print */
  (getattrfunc)World_GetAttr,            /* tp_getattr */
  (setattrfunc)World_SetAttr,            /* tp_setattr */
  (cmpfunc)World_Compare,                /* tp_compare */
  (reprfunc)World_Repr,                  /* tp_repr */
  0,                                      /* tp_as_number */
  0,                                      /* tp_as_sequence */
  0,                                      /* tp_as_mapping */
  0,                                      /* tp_as_hash */
  0,0,0,0,0,0,
  0,                                      /* tp_doc */ 
  0,0,0,0,0,0,
  BPy_World_methods,                     /* tp_methods */
  0,                                      /* tp_members */
};

/**
 * \defgroup World_Module Blender.World module functions
 *
 */

/*@{*/

/**
 * \brief Python module function: Blender.World.New()
 *
 * This is the .New() function of the Blender.World submodule. It creates
 * new World Data in Blender and returns its Python wrapper object.  The
 * name parameter is mandatory.
 * \param <name> - string: The World Data name.
 * \return A new World PyObject.
 */

static PyObject *M_World_New(PyObject *self, PyObject *args, PyObject *kwords)
{

	World *add_world(char *name);
	char*name = NULL;
  BPy_World    *pyworld;
  World      *blworld;

	if (!PyArg_ParseTuple(args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected  int argument"));


  blworld = add_world(name);

  if (blworld) 
		pyworld = (BPy_World *)PyObject_NEW(BPy_World, &World_Type);
  else
    return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
																	 "couldn't create World Data in Blender"));

  if (pyworld == NULL)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
																	 "couldn't create World Data object"));

  pyworld->world = blworld; 

  return (PyObject *)pyworld;
}

/**
 * \brief Python module function: Blender.World.Get()
 *
 * This is the .Get() function of the Blender.World submodule.  It searches
 * the list of current World Data objects and returns a Python wrapper for
 * the one with the name provided by the user.  If called with no arguments,
 * it returns a list of all current World Data object names in Blender.
 * \param <name> - string: The name of an existing Blender World Data object.
 * \return () - A list with the names of all current World Data objects;\n
 * \return (name) - A Python wrapper for the World Data called 'name'
 * in Blender.
 */

static PyObject *M_World_Get(PyObject *self, PyObject *args)
{

  char   *name = NULL;
  World *world_iter;
  PyObject *worldlist;
  BPy_World *wanted_world = NULL;
  char error_msg[64];

	if (!PyArg_ParseTuple(args, "|s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected string argument (or nothing)"));

  world_iter = G.main->world.first;

	if (name) { /* (name) - Search world by name */
    while ((world_iter) && (wanted_world == NULL)) {
      if (strcmp (name, world_iter->id.name+2) == 0) {
        wanted_world = (BPy_World *)PyObject_NEW(BPy_World, &World_Type);
				if (wanted_world) wanted_world->world = world_iter;
      }
      world_iter = world_iter->id.next;
    }

    if (wanted_world == NULL) { /* Requested world doesn't exist */
      PyOS_snprintf(error_msg, sizeof(error_msg),
										"World \"%s\" not found", name);
      return (EXPP_ReturnPyObjError (PyExc_NameError, error_msg));
    }

    return (PyObject *)wanted_world;
	}

	else { /* return a list of all worlds in the scene */
    worldlist = PyList_New (0);
    if (worldlist == NULL)
      return (PythonReturnErrorObject (PyExc_MemoryError,
																			 "couldn't create PyList"));

		while (world_iter) {
			BPy_World *found_world = (BPy_World *)PyObject_NEW(BPy_World, &World_Type);
			found_world->world = world_iter;
			PyList_Append (worldlist ,  (PyObject *)found_world); 

      world_iter = world_iter->id.next;
		}
		return (worldlist);
	}

}
/*@}*/

/**
 * \brief Initializes the Blender.World submodule
 *
 * This function is used by Blender_Init() in Blender.c to register the
 * Blender.World submodule in the main Blender module.
 * \return PyObject*: The initialized submodule.
 */

PyObject *World_Init (void)
{
  PyObject  *submodule;

  World_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3("Blender.World",
                  M_World_methods, M_World_doc);

  return (submodule);
}


/*****************************************************************************/
/* Python BPy_World methods:                                                */
/*****************************************************************************/

/**
 * \defgroup World_Methods World Method Functions
 *
 * These are the World PyObject method functions.  They are used to get and
 * set values for the World Data member variables.
 */

/*@{*/

/**
 * \brief World PyMethod getName
 *
 * \return string: The World Data name.
 */

static PyObject *World_getName(BPy_World *self)
{
  PyObject *attr = PyString_FromString(self->world->id.name+2);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get World.name attribute"));
}
/**
 * \brief World PyMethod setName
 * \param name - string: The new World Data name.
 */

static PyObject *World_setName(BPy_World *self, PyObject *args)
{
  char *name = 0;
  char buf[21];
	puts("mlmlml");
  if (!PyArg_ParseTuple(args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected string argument"));
	puts(name);
  snprintf(buf, sizeof(buf), "%s", name);

	puts("mlmlml");
  rename_id(&self->world->id, buf);

  Py_INCREF(Py_None);
  return Py_None;
}


/**
 * \brief World PyMethod getColormodel
 *
 * \return int : The World Data colormodel.
 */

static PyObject *World_getColormodel(BPy_World *self)
{
  PyObject *attr = PyInt_FromLong((long)self->world->colormodel);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get World.colormodel attribute"));
}


/**
 * \brief World PyMethod setColormodel
 *
 * \return int : The World Data colormodel.
 */

static PyObject *World_setColormodel(BPy_World *self, PyObject *args )
{
  int colormodel;

  if (!PyArg_ParseTuple(args, "i", &colormodel))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected int argument"));
	self->world->colormodel = colormodel;
  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * \brief World PyMethod getFastcol
 *
 * \return int : The World Data fastcol.
 */

static PyObject *World_getFastcol(BPy_World *self)
{
  PyObject *attr = PyInt_FromLong((long)self->world->fastcol);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get World.fastcol attribute"));
}


/**
 * \brief World PyMethod setFastcol
 *
 * \return int : The World Data fastcol.
 */

static PyObject *World_setFastcol(BPy_World *self, PyObject *args )
{
  int fastcol;

  if (!PyArg_ParseTuple(args, "i", &fastcol))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected int argument"));
	self->world->fastcol = fastcol;
  Py_INCREF(Py_None);
  return Py_None;
}




/**
 * \brief World PyMethod getSkytype
 *
 * \return int : The World Data skytype.
 */

static PyObject *World_getSkytype(BPy_World *self)
{
  PyObject *attr = PyInt_FromLong((long)self->world->skytype);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get World.skytype attribute"));
}


/**
 * \brief World PyMethod setSkytype
 *
 * \return int : The World Data skytype.
 */

static PyObject *World_setSkytype(BPy_World *self, PyObject *args )
{
  int skytype;

  if (!PyArg_ParseTuple(args, "i", &skytype))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected int argument"));
	self->world->skytype = skytype;
  Py_INCREF(Py_None);
  return Py_None;
}


/**
 * \brief World PyMethod getMode
 *
 * \return int : The World Data mode.
 */

static PyObject *World_getMode(BPy_World *self)
{
  PyObject *attr = PyInt_FromLong((long)self->world->mode);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get World.mode attribute"));
}


/**
 * \brief World PyMethod setMode
 *
 * \return int : The World Data mode.
 */

static PyObject *World_setMode(BPy_World *self, PyObject *args )
{
  int mode;

  if (!PyArg_ParseTuple(args, "i", &mode))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected int argument"));
	self->world->mode = mode;
  Py_INCREF(Py_None);
  return Py_None;
}

























/**
 * \brief World PyMethod getTotex
 *
 * \return int : The World Data totex.
 */

static PyObject *World_getTotex(BPy_World *self)
{
  PyObject *attr = PyInt_FromLong((long)self->world->totex);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get World.totex attribute"));
}


/**
 * \brief World PyMethod setTotex
 *
 * \return int : The World Data totex.
 */

static PyObject *World_setTotex(BPy_World *self, PyObject *args )
{
  int totex;

  if (!PyArg_ParseTuple(args, "i", &totex))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected int argument"));
	self->world->totex = totex;
  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * \brief World PyMethod getTexact
 *
 * \return int : The World Data texact.
 */

static PyObject *World_getTexact(BPy_World *self)
{
  PyObject *attr = PyInt_FromLong((long)self->world->texact);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get World.texact attribute"));
}


/**
 * \brief World PyMethod setTexact
 *
 * \return int : The World Data texact.
 */

static PyObject *World_setTexact(BPy_World *self, PyObject *args )
{
  int texact;

  if (!PyArg_ParseTuple(args, "i", &texact))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected int argument"));
	self->world->texact = texact;
  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * \brief World PyMethod getMistype
 *
 * \return int : The World Data mistype.
 */

static PyObject *World_getMistype(BPy_World *self)
{
  PyObject *attr = PyInt_FromLong((long)self->world->mistype);

  if (attr) return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't get World.mistype attribute"));
}


/**
 * \brief World PyMethod setMistype
 *
 * \return int : The World Data mistype.
 */

static PyObject *World_setMistype(BPy_World *self, PyObject *args )
{
  int mistype;

  if (!PyArg_ParseTuple(args, "i", &mistype))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
                                     "expected int argument"));
	self->world->mistype = mistype;
  Py_INCREF(Py_None);
  return Py_None;
}









static PyObject *World_getHor(BPy_World *self)
{
  PyObject *attr = PyList_New(0);
	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't create list"));
	PyList_Append(attr, PyFloat_FromDouble(self->world->horr));
	PyList_Append(attr, PyFloat_FromDouble(self->world->horg));
	PyList_Append(attr, PyFloat_FromDouble(self->world->horb));
	return attr;
}


static PyObject *World_setHor(BPy_World *self, PyObject *args )
{
	PyObject *listargs=0;
  if (!PyArg_ParseTuple(args, "O", &listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected list argument"));
	self->world->horr =  PyFloat_AsDouble(PyList_GetItem(listargs,0));
	self->world->horg =  PyFloat_AsDouble(PyList_GetItem(listargs,1));
	self->world->horb = PyFloat_AsDouble(PyList_GetItem(listargs,2));
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *World_getZen(BPy_World *self)
{
  PyObject *attr = PyList_New(0);
	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't create list"));
	PyList_Append(attr, PyFloat_FromDouble(self->world->zenr));
	PyList_Append(attr, PyFloat_FromDouble(self->world->zeng));
	PyList_Append(attr, PyFloat_FromDouble(self->world->zenb));
	return attr;
}


static PyObject *World_setZen(BPy_World *self, PyObject *args )
{
	PyObject *listargs=0;
  if (!PyArg_ParseTuple(args, "O", &listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,
																	 "expected list argument"));
	self->world->zenr =  PyFloat_AsDouble(PyList_GetItem(listargs,0));
	self->world->zeng =  PyFloat_AsDouble(PyList_GetItem(listargs,1));
	self->world->zenb = PyFloat_AsDouble(PyList_GetItem(listargs,2));
	Py_INCREF(Py_None);
	return Py_None;
}




static PyObject *World_getAmb(BPy_World *self)
{
  PyObject *attr = PyList_New(0);
	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
                                   "couldn't create list"));
	PyList_Append(attr, PyFloat_FromDouble(self->world->ambr));
	PyList_Append(attr, PyFloat_FromDouble(self->world->ambg));
	PyList_Append(attr, PyFloat_FromDouble(self->world->ambb));
	return attr;
}


static PyObject *World_setAmb(BPy_World *self, PyObject *args )
{
	PyObject *listargs=0;
  if (!PyArg_ParseTuple(args, "O", &listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected list argument"));
	if (!PyList_Check(listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected list argument"));
	if (PyList_Size(listargs)!=3)
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"wrong list size"));
	self->world->ambr =  PyFloat_AsDouble(PyList_GetItem(listargs,0));
	self->world->ambg =  PyFloat_AsDouble(PyList_GetItem(listargs,1));
	self->world->ambb = PyFloat_AsDouble(PyList_GetItem(listargs,2));
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject *World_getStar(BPy_World *self)
{
  PyObject *attr = PyList_New(0);
	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError,"couldn't create list"));
	PyList_Append(attr, PyFloat_FromDouble(self->world->starr));
	PyList_Append(attr, PyFloat_FromDouble(self->world->starg));
	PyList_Append(attr, PyFloat_FromDouble(self->world->starb));
	PyList_Append(attr, PyFloat_FromDouble(self->world->starsize));
	PyList_Append(attr, PyFloat_FromDouble(self->world->starmindist));
	PyList_Append(attr, PyFloat_FromDouble(self->world->stardist));
	PyList_Append(attr, PyFloat_FromDouble(self->world->starcolnoise));
	return attr;
}


static PyObject *World_setStar(BPy_World *self, PyObject *args )
{
	PyObject *listargs=0;
  if (!PyArg_ParseTuple(args, "O", &listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected list argument"));
	if (!PyList_Check(listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected list argument"));
	if (PyList_Size(listargs)!=7)
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"wrong list size"));
	self->world->starr =  PyFloat_AsDouble(PyList_GetItem(listargs,0));
	self->world->starg =  PyFloat_AsDouble(PyList_GetItem(listargs,1));
	self->world->starb = PyFloat_AsDouble(PyList_GetItem(listargs,2));
	self->world->starsize = PyFloat_AsDouble(PyList_GetItem(listargs,3));
	self->world->starmindist = PyFloat_AsDouble(PyList_GetItem(listargs,4));
	self->world->stardist = PyFloat_AsDouble(PyList_GetItem(listargs,5));
	self->world->starcolnoise = PyFloat_AsDouble(PyList_GetItem(listargs,6));
	Py_INCREF(Py_None);
	return Py_None;
}



static PyObject *World_getDof(BPy_World *self)
{
  PyObject *attr = PyList_New(0);
	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError, "couldn't create list"));
	PyList_Append(attr, PyFloat_FromDouble(self->world->dofsta));
	PyList_Append(attr, PyFloat_FromDouble(self->world->dofend));
	PyList_Append(attr, PyFloat_FromDouble(self->world->dofmin));
	PyList_Append(attr, PyFloat_FromDouble(self->world->dofmax));
	return attr;
}


static PyObject *World_setDof(BPy_World *self, PyObject *args )
{
	PyObject *listargs=0;
  if (!PyArg_ParseTuple(args, "O", &listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected list argument"));
	if (!PyList_Check(listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected list argument"));
	if (PyList_Size(listargs)!=4)
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"wrong list size"));
	self->world->dofsta =  PyFloat_AsDouble(PyList_GetItem(listargs,0));
	self->world->dofend =  PyFloat_AsDouble(PyList_GetItem(listargs,1));
	self->world->dofmin = PyFloat_AsDouble(PyList_GetItem(listargs,2));
	self->world->dofmax= PyFloat_AsDouble(PyList_GetItem(listargs,3));
	Py_INCREF(Py_None);
	return Py_None;
}




static PyObject *World_getMist(BPy_World *self)
{
  PyObject *attr = PyList_New(0);
	if (!attr)
		return (EXPP_ReturnPyObjError (PyExc_RuntimeError, "couldn't create list"));
	PyList_Append(attr, PyFloat_FromDouble(self->world->misi));
	PyList_Append(attr, PyFloat_FromDouble(self->world->miststa));
	PyList_Append(attr, PyFloat_FromDouble(self->world->mistdist));
	PyList_Append(attr, PyFloat_FromDouble(self->world->misthi));
	return attr;
}


static PyObject *World_setMist(BPy_World *self, PyObject *args )
{
	PyObject *listargs=0;
  if (!PyArg_ParseTuple(args, "O", &listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected list argument"));
	if (!PyList_Check(listargs))
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"expected list argument"));
	if (PyList_Size(listargs)!=4)
    return (EXPP_ReturnPyObjError (PyExc_TypeError,"wrong list size"));
	self->world->misi =  PyFloat_AsDouble(PyList_GetItem(listargs,0));
	self->world->miststa =  PyFloat_AsDouble(PyList_GetItem(listargs,1));
	self->world->mistdist =  PyFloat_AsDouble(PyList_GetItem(listargs,2));
	self->world->misthi = PyFloat_AsDouble(PyList_GetItem(listargs,3));
	Py_INCREF(Py_None);
	return Py_None;
}




/*@{*/

/**
 * \brief The World PyType destructor
 */

static void World_DeAlloc (BPy_World *self)
{
  PyObject_DEL (self);
}

/**
 * \brief The World PyType attribute getter
 *
 * This is the callback called when a user tries to retrieve the contents of
 * World PyObject data members.  Ex. in Python: "print myworld.lens".
 */

static PyObject *World_GetAttr (BPy_World *self, char *name)
{

if (strcmp (name, "name") == 0)return  World_getName (self);
if (strcmp (name, "colormodel") == 0)return  World_getColormodel (self);
if (strcmp (name, "fastcol") == 0)return  World_getFastcol (self);
if (strcmp (name, "skytype") == 0)return  World_getSkytype (self);
if (strcmp (name, "mode") == 0)return  World_getMode (self);
if (strcmp (name, "totex") == 0)return  World_getTotex (self);
if (strcmp (name, "texact") == 0)return  World_getTexact (self);
if (strcmp (name, "mistype") == 0)return  World_getMistype (self);
if (strcmp (name, "hor") == 0)return  World_getHor (self);
if (strcmp (name, "zen") == 0)return  World_getZen (self);
if (strcmp (name, "amb") == 0)return  World_getAmb (self);
if (strcmp (name, "star") == 0)return  World_getStar (self);
if (strcmp (name, "dof") == 0)return  World_getDof (self);
if (strcmp (name, "mist") == 0)return  World_getMist (self);
  return Py_FindMethod(BPy_World_methods, (PyObject *)self, name);
}

/**
 * \brief The World PyType attribute setter
 *
 * This is the callback called when the user tries to change the value of some
 * World data member.  Ex. in Python: "myworld.lens = 45.0".
 */

static int World_SetAttr (BPy_World *self, char *name, PyObject *value)
{
  PyObject *valtuple  = Py_BuildValue("(O)", value);

  if (!valtuple) 
    return EXPP_ReturnIntError(PyExc_MemoryError,
															 "WorldSetAttr: couldn't parse args");
		if (strcmp (name, "name") == 0) World_setName (self,valtuple);
if (strcmp (name, "colormodel") == 0) World_setColormodel (self,valtuple);
if (strcmp (name, "fastcol") == 0) World_setFastcol (self,valtuple);
if (strcmp (name, "skytype") == 0) World_setSkytype (self,valtuple);
if (strcmp (name, "mode") == 0) World_setMode (self,valtuple);
if (strcmp (name, "totex") == 0) World_setTotex (self,valtuple);
if (strcmp (name, "texact") == 0) World_setTexact (self,valtuple);
if (strcmp (name, "mistype") == 0) World_setMistype (self,valtuple);
if (strcmp (name, "hor") == 0) World_setHor (self,valtuple);
if (strcmp (name, "zen") == 0) World_setZen (self,valtuple);
if (strcmp (name, "amb") == 0) World_setAmb (self,valtuple);
if (strcmp (name, "star") == 0) World_setStar (self,valtuple);
if (strcmp (name, "dof") == 0) World_setDof (self,valtuple);
if (strcmp (name, "mist") == 0) World_setMist (self,valtuple);
return 0; /* normal exit */
}

/**
 * \brief The World PyType compare function
 *
 * This function compares two given World PyObjects, returning 0 for equality
 * and -1 otherwise.  In Python it becomes 1 if they are equal and 0 case not.
 * The comparison is done with their pointers to Blender World Data objects,
 * so any two wrappers pointing to the same Blender World Data will be
 * considered the same World PyObject.  Currently, only the "==" and "!="
 * comparisons are meaninful -- the "<", "<=", ">" or ">=" are not.
 */

static int World_Compare (BPy_World *a, BPy_World *b)
{
  World *pa = a->world, *pb = b->world;
  return (pa == pb) ? 0:-1;
}

/**
 * \brief The World PyType print callback
 *
 * This function is called when the user tries to print a PyObject of type
 * World.  It builds a string with the name of the wrapped Blender World.
 */

static int World_Print(BPy_World *self, FILE *fp, int flags)
{ 
  fprintf(fp, "[World \"%s\"]", self->world->id.name+2);
  return 0;
}

/**
 * \brief The World PyType repr callback
 *
 * This function is called when the statement "repr(myworld)" is executed in
 * Python.  Repr gives a string representation of a PyObject.
 */

static PyObject *World_Repr (BPy_World *self)
{
  return PyString_FromString(self->world->id.name+2);
}

/*@}*/
