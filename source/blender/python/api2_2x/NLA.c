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
 * Contributor(s): Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "NLA.h"
#include "Object.h"
#include <BKE_action.h>
#include <BKE_global.h>
#include <BKE_main.h>
#include "Types.h"

/*****************************************************************************/
/* Python API function prototypes for the NLA module.			 */
/*****************************************************************************/
static PyObject *M_NLA_NewAction (PyObject * self, PyObject * args);
static PyObject *M_NLA_CopyAction (PyObject * self, PyObject * args);
static PyObject *M_NLA_GetActions(PyObject* self);

/*****************************************************************************/
/* The following string definitions are used for documentation strings.	  	 */
/* In Python these will be written to the console when doing a		 */
/* Blender.Armature.NLA.__doc__					 */
/*****************************************************************************/
char M_NLA_doc[] = "The Blender NLA module -This module provides control over  Armature keyframing in Blender.";
char M_NLA_NewAction_doc[] = "(name) - Create new action for linking to an object.";
char M_NLA_CopyAction_doc[] = "(name) - Copy action and return copy.";
char M_NLA_GetActions_doc[] = "(name) - Returns a dictionary of actions.";

/*****************************************************************************/
/* Python method structure definition for Blender.Armature.NLA module:			 */
/*****************************************************************************/
struct PyMethodDef M_NLA_methods[] = {
  {"NewAction", (PyCFunction) M_NLA_NewAction, METH_VARARGS,
	  M_NLA_NewAction_doc},
  {"CopyAction", (PyCFunction) M_NLA_CopyAction, METH_VARARGS,
	  M_NLA_CopyAction_doc},
  {"GetActions", (PyCFunction) M_NLA_GetActions, METH_NOARGS,
	  M_NLA_GetActions_doc},
  {NULL, NULL, 0, NULL}
};
/*****************************************************************************/
/* Python BPy_Action methods declarations:																		 */
/*****************************************************************************/
static PyObject *Action_getName (BPy_Action * self);
static PyObject *Action_setName (BPy_Action * self, PyObject * args);
static PyObject *Action_setActive (BPy_Action * self, PyObject * args);
static PyObject *Action_getChannelIpo(BPy_Action * self, PyObject * args);
static PyObject *Action_removeChannel(BPy_Action * self, PyObject * args);
static PyObject *Action_getAllChannelIpos(BPy_Action*self);

/*****************************************************************************/
/* Python BPy_Action methods table:					 */
/*****************************************************************************/
static PyMethodDef BPy_Action_methods[] = {
  /* name, method, flags, doc */
  {"getName", (PyCFunction) Action_getName, METH_NOARGS,
   "() - return Action name"},
  {"setName", (PyCFunction) Action_setName, METH_VARARGS,
   "(str) - rename Action"},
  {"setActive", (PyCFunction) Action_setActive, METH_VARARGS,
   "(str) -set this action as the active action for an object"},
  {"getChannelIpo", (PyCFunction) Action_getChannelIpo, METH_VARARGS,
   "(str) -get the Ipo from a named action channel in this action"},
  {"removeChannel", (PyCFunction) Action_removeChannel, METH_VARARGS,
   "(str) -remove the channel from the action"},
  {"getAllChannelIpos", (PyCFunction)Action_getAllChannelIpos, METH_NOARGS,
	"() - Return a dict of (name:ipo)-keys containing each channel in the object's action"},
  {NULL, NULL, 0, NULL}
};

/*****************************************************************************/
/* Python TypeAction callback function prototypes:				 */
/*****************************************************************************/
static void Action_dealloc (BPy_Action * bone);
static PyObject *Action_getAttr (BPy_Action * bone, char *name);
static int Action_setAttr (BPy_Action * bone, char *name, PyObject * v);
static PyObject *Action_repr (BPy_Action * bone);

/*****************************************************************************/
/* Python TypeAction structure definition:				 */
/*****************************************************************************/
PyTypeObject Action_Type = {
  PyObject_HEAD_INIT (NULL) 0,	  /* ob_size */
  "Blender Action",		                          /* tp_name */
  sizeof (BPy_Action),		                  /* tp_basicsize */
  0,				                                          /* tp_itemsize */
  /* methods */
  (destructor) Action_dealloc,	          /* tp_dealloc */
  0,				                                         /* tp_print */
  (getattrfunc) Action_getAttr,	         /* tp_getattr */
  (setattrfunc) Action_setAttr,	         /* tp_setattr */
  0,	                                                     /* tp_compare */
  (reprfunc) Action_repr,		             /* tp_repr */
  0,				                                         /* tp_as_number */
  0,				                                         /* tp_as_sequence */
  0,				                                         /* tp_as_mapping */
  0,				                                         /* tp_as_hash */
  0, 0, 0, 0, 0, 0,
  0,				                                         /* tp_doc */
  0, 0, 0, 0, 0, 0,
  BPy_Action_methods,		             /* tp_methods */
  0,				                                         /* tp_members */
};

//-------------------------------------------------------------------------------------------------------------------------------
static PyObject *
M_NLA_NewAction (PyObject * self, PyObject * args)
{
  char *name_str = "DefaultAction";
  BPy_Action *py_action = NULL;	/* for Action Data object wrapper in Python */
  bAction *bl_action = NULL;		/* for actual Action Data we create in Blender */

  if (!PyArg_ParseTuple (args, "|s", &name_str)){
    EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected string or nothing");
	return NULL;
  }

  //Create new action globally
 bl_action = alloc_libblock(&G.main->action, ID_AC, name_str);
 bl_action->id.flag |= LIB_FAKEUSER;
 bl_action->id.us++;

  // now create the wrapper obj in Python
  if (bl_action)				
    py_action = (BPy_Action *) PyObject_NEW (BPy_Action, &Action_Type);
  else{
    EXPP_ReturnPyObjError (PyExc_RuntimeError,
				   "couldn't create Action Data in Blender");
	return NULL;
  }

  if (py_action == NULL){
    EXPP_ReturnPyObjError (PyExc_MemoryError,
				   "couldn't create Action Data object");
	return NULL;
  }

  py_action->action = bl_action;	// link Python action wrapper with Blender Action

  Py_INCREF(py_action);
  return (PyObject *) py_action;
}

static PyObject *
M_NLA_CopyAction(PyObject* self, PyObject * args)
{
	BPy_Action *py_action = NULL;
	bAction *copyAction = NULL;

	if (!PyArg_ParseTuple (args, "O!", &Action_Type, &py_action)){
		EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected python action type");
		return NULL;
  }
	copyAction =  copy_action(py_action->action);
	return Action_CreatePyObject (copyAction);
}

static PyObject *
M_NLA_GetActions(PyObject* self)
{
	PyObject *dict=PyDict_New ();
	bAction *action = NULL;

	for(action = G.main->action.first; action; action = action->id.next){
        PyObject * py_action = Action_CreatePyObject (action);
		if (py_action) {
			// Insert dict entry using the bone name as key
			if (PyDict_SetItemString (dict, action->id.name + 2, py_action) !=0) {
				Py_DECREF (py_action);
				Py_DECREF ( dict );
				
				return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"NLA_GetActions: couldn't set dict item");
			}
			Py_DECREF (py_action);
		} else {
			Py_DECREF ( dict );
			return (PythonReturnErrorObject (PyExc_RuntimeError,
				"NLA_GetActions: could not create Action object"));
		}
	}	
	return dict;
}

/*****************************************************************************/
/* Function:	NLA_Init						 */
/*****************************************************************************/
PyObject *
NLA_Init (void)
{
  PyObject *submodule;

  Action_Type.ob_type = &PyType_Type;

  submodule = Py_InitModule3 ("Blender.Armature.NLA",
			      M_NLA_methods, M_NLA_doc);

  return (submodule);
}

//-------------------------------------------------------------------------------------------------------------------------------
static PyObject *
Action_getName (BPy_Action * self)
{
  PyObject *attr = NULL;

  if (!self->action)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL action"));

  attr = PyString_FromString (self->action->id.name+2);

  if (attr)
    return attr;

  return (EXPP_ReturnPyObjError (PyExc_RuntimeError,
				 "couldn't get Action.name attribute"));
}
//-------------------------------------------------------------------------------------------------------------------------------
static PyObject *
Action_setName (BPy_Action * self, PyObject * args)
{
  char *name;

  if (!self->action)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL action"));

  if (!PyArg_ParseTuple (args, "s", &name))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected string argument"));

  //change name
  strcpy(self->action->id.name+2, name);

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
Action_setActive(BPy_Action * self, PyObject * args)
{
  BPy_Object *object;

  if (!self->action)
    (EXPP_ReturnPyObjError (PyExc_RuntimeError,
			    "couldn't get attribute from a NULL action"));

  if (!PyArg_ParseTuple (args, "O!", &Object_Type, &object))
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "expected python object argument"));

  if(object->object->type != OB_ARMATURE) {
    return (EXPP_ReturnPyObjError (PyExc_AttributeError,
				   "object not of type armature"));
  }

  //set the active action to object
  object->object->action = self->action;

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
Action_getChannelIpo(BPy_Action * self, PyObject * args)
{
	char *chanName;
	bActionChannel *chan;

	if(!PyArg_ParseTuple(args, "s", &chanName)){
		EXPP_ReturnPyObjError(PyExc_AttributeError, "string expected");
		return NULL;
	}

	chan = get_named_actionchannel(self->action,chanName);
	if(chan == NULL){
		EXPP_ReturnPyObjError(PyExc_AttributeError, "no channel with that name...");
		return NULL;
	}

	//return IPO
    return Ipo_CreatePyObject (chan->ipo);
}

static PyObject *
Action_removeChannel(BPy_Action * self, PyObject * args)
{
	char *chanName;
	bActionChannel *chan;

	if(!PyArg_ParseTuple(args, "s", &chanName)){
		EXPP_ReturnPyObjError(PyExc_AttributeError, "string expected");
		return NULL;
	}

	chan = get_named_actionchannel(self->action,chanName);
	if(chan == NULL){
		EXPP_ReturnPyObjError(PyExc_AttributeError, "no channel with that name...");
		return NULL;
	}

	//release ipo
	if(chan->ipo)
		chan->ipo->id.us--;

	//remove channel
	BLI_freelinkN (&self->action->chanbase, chan);

	Py_INCREF (Py_None);
	return (Py_None);
}

static PyObject *Action_getAllChannelIpos (BPy_Action *self)
{
	PyObject *dict=PyDict_New ();
	bActionChannel *chan = NULL;

	for(chan = self->action->chanbase.first; chan; chan = chan->next){
        PyObject * ipo_attr = Ipo_CreatePyObject (chan->ipo);
		if (ipo_attr) {
			// Insert dict entry using the bone name as key
			if (PyDict_SetItemString (dict, chan->name, ipo_attr) !=0) {
				Py_DECREF ( ipo_attr );
				Py_DECREF ( dict );
				
				return EXPP_ReturnPyObjError (PyExc_RuntimeError,
						"Action_getAllChannelIpos: couldn't set dict item");
			}
			Py_DECREF (ipo_attr);
		} else {
			Py_DECREF ( dict );
			return (PythonReturnErrorObject (PyExc_RuntimeError,
				"Action_getAllChannelIpos: could not create Ipo object"));
		}
	}	
	return dict;
}

//-------------------------------------------------------------------------------------------------------------------------------
static void
Action_dealloc (BPy_Action * self)
{
    PyObject_DEL (self);
}
//-------------------------------------------------------------------------------------------------------------------------------
static PyObject *
Action_getAttr (BPy_Action * self, char *name)
{
  PyObject *attr = Py_None;

  if (strcmp (name, "name") == 0)
    attr = Action_getName (self);
  else if (strcmp (name, "__members__") == 0)  {
      attr = Py_BuildValue ("[s]",
			    "name");
    }

  if (!attr)
    return (EXPP_ReturnPyObjError (PyExc_MemoryError,
				   "couldn't create PyObject"));

  if (attr != Py_None)
    return attr;		/* member attribute found, return it */

  /* not an attribute, search the methods table */
  return Py_FindMethod (BPy_Action_methods, (PyObject *) self, name);
}

//-------------------------------------------------------------------------------------------------------------------------------
static int
Action_setAttr (BPy_Action * self, char *name, PyObject * value)
{
  PyObject *valtuple;
  PyObject *error = NULL;

  valtuple = Py_BuildValue ("(O)", value);	/* the set* functions expect a tuple */

  if (!valtuple)
    return EXPP_ReturnIntError (PyExc_MemoryError,
				"ActionSetAttr: couldn't create tuple");

  if (strcmp (name, "name") == 0)
    error = Action_setName (self, valtuple);
  else
    {				/* Error */
      Py_DECREF (valtuple);

      /* ... member with the given name was found */
      return (EXPP_ReturnIntError (PyExc_KeyError, "attribute not found"));
    }

  Py_DECREF (valtuple);

  if (error != Py_None)
    return -1;

  Py_DECREF (Py_None);		/* was incref'ed by the called Action_set* function */
  return 0;			/* normal exit */
}
//-------------------------------------------------------------------------------------------------------------------------------
static PyObject *
Action_repr (BPy_Action * self)
{
  if (self->action)
    return PyString_FromFormat ("[Action \"%s\"]", self->action->id.name + 2);
  else
    return PyString_FromString ("NULL");
}
//-------------------------------------------------------------------------------------------------------------------------------
PyObject *
Action_CreatePyObject (struct bAction * act)
{
  BPy_Action *blen_action;

  blen_action = (BPy_Action *) PyObject_NEW (BPy_Action, &Action_Type);

  if (blen_action == NULL)
    {
      return (NULL);
    }
  blen_action->action	= act;
  return ((PyObject *) blen_action);
}
//-------------------------------------------------------------------------------------------------------------------------------
int
Action_CheckPyObject (PyObject * py_obj)
{
  return (py_obj->ob_type == &Action_Type);
}
//-------------------------------------------------------------------------------------------------------------------------------
struct bAction *
Action_FromPyObject (PyObject * py_obj)
{
  BPy_Action *blen_obj;

  blen_obj = (BPy_Action *) py_obj;
  return (blen_obj->action);
}
