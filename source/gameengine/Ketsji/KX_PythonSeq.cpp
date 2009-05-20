/**
 * $Id: 
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * The Original Code is: none of this file.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 * Readonly sequence wrapper for lookups on logic bricks
 */


#include "KX_PythonSeq.h"
#include "KX_GameObject.h"
#include "SCA_ISensor.h"
#include "SCA_IController.h"
#include "SCA_IActuator.h"


PyObject *KX_PythonSeq_CreatePyObject( PyObject *base, short type )
{
	KX_PythonSeq *seq = PyObject_NEW( KX_PythonSeq, &KX_PythonSeq_Type);
	seq->base = base;
	Py_INCREF(base); /* so we can always access to check if its valid */
	seq->type = type;
	seq->iter = -1; /* init */
	return (PyObject *)seq;
 }
 
 static void KX_PythonSeq_dealloc( KX_PythonSeq * self )
{
	Py_DECREF(self->base);
	PyObject_DEL( self );
}

static Py_ssize_t KX_PythonSeq_len( PyObject * self )
{
	PyObjectPlus *self_plus= BGE_PROXY_REF(((KX_PythonSeq *)self)->base);
	 
	if(self_plus==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return -1;
	}
	
	switch(((KX_PythonSeq *)self)->type) {
	case KX_PYGENSEQ_CONT_TYPE_SENSORS:
		return ((SCA_IController *)self_plus)->GetLinkedSensors().size();
	case KX_PYGENSEQ_CONT_TYPE_ACTUATORS:
		return ((SCA_IController *)self_plus)->GetLinkedActuators().size();
	case KX_PYGENSEQ_OB_TYPE_SENSORS:
		return ((KX_GameObject *)self_plus)->GetSensors().size();
	case KX_PYGENSEQ_OB_TYPE_CONTROLLERS:
		return ((KX_GameObject *)self_plus)->GetControllers().size();
	case KX_PYGENSEQ_OB_TYPE_ACTUATORS:
		return ((KX_GameObject *)self_plus)->GetActuators().size();
	default:
		/* Should never happen */
		PyErr_SetString(PyExc_SystemError, "invalid type, internal error");
		return -1;
	}
}

static PyObject *KX_PythonSeq_getIndex(PyObject* self, int index)
{
	PyObjectPlus *self_plus= BGE_PROXY_REF(((KX_PythonSeq *)self)->base);
	 
	if(self_plus==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	switch(((KX_PythonSeq *)self)->type) {
		case KX_PYGENSEQ_CONT_TYPE_SENSORS:
		{
			vector<SCA_ISensor*>& linkedsensors = ((SCA_IController *)self_plus)->GetLinkedSensors();
			if(index<0) index += linkedsensors.size();
			if(index<0 || index>= linkedsensors.size()) {
				PyErr_SetString(PyExc_IndexError, "seq[i]: index out of range");
				return NULL;
			}
			return linkedsensors[index]->GetProxy();
		}
		case KX_PYGENSEQ_CONT_TYPE_ACTUATORS:
		{
			vector<SCA_IActuator*>& linkedactuators = ((SCA_IController *)self_plus)->GetLinkedActuators();
			if(index<0) index += linkedactuators.size();
			if(index<0 || index>= linkedactuators.size()) {
				PyErr_SetString(PyExc_IndexError, "seq[i]: index out of range");
				return NULL;
			}
			return linkedactuators[index]->GetProxy();
		}
		case KX_PYGENSEQ_OB_TYPE_SENSORS:
		{
			SCA_SensorList& linkedsensors= ((KX_GameObject *)self_plus)->GetSensors();
			if(index<0) index += linkedsensors.size();
			if(index<0 || index>= linkedsensors.size()) {
				PyErr_SetString(PyExc_IndexError, "seq[i]: index out of range");
				return NULL;
			}
			return linkedsensors[index]->GetProxy();
		}
		case KX_PYGENSEQ_OB_TYPE_CONTROLLERS:
		{
			SCA_ControllerList& linkedcontrollers= ((KX_GameObject *)self_plus)->GetControllers();
			if(index<0) index += linkedcontrollers.size();
			if(index<0 || index>= linkedcontrollers.size()) {
				PyErr_SetString(PyExc_IndexError, "seq[i]: index out of range");
				return NULL;
			}
			return linkedcontrollers[index]->GetProxy();
		}
		case KX_PYGENSEQ_OB_TYPE_ACTUATORS:
		{
			SCA_ActuatorList& linkedactuators= ((KX_GameObject *)self_plus)->GetActuators();
			if(index<0) index += linkedactuators.size();
			if(index<0 || index>= linkedactuators.size()) {
				PyErr_SetString(PyExc_IndexError, "seq[i]: index out of range");
				return NULL;
			}
			return linkedactuators[index]->GetProxy();
		}
	}
	
	PyErr_SetString(PyExc_SystemError, "invalid sequence type, this is a bug");
	return NULL;
}


static PyObject * KX_PythonSeq_subscript(PyObject * self, PyObject *key)
{
	PyObjectPlus *self_plus= BGE_PROXY_REF(((KX_PythonSeq *)self)->base);
	char *name = NULL;
	
	if(self_plus==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	if (PyInt_Check(key)) {
		return KX_PythonSeq_getIndex(self, PyInt_AS_LONG( key ));
	} else if ( PyString_Check(key) ) {
		name = PyString_AsString( key );
	} else {
		PyErr_SetString( PyExc_TypeError, "expected a string or an index" );
		return NULL;
	}
	
	switch(((KX_PythonSeq *)self)->type) {
		case KX_PYGENSEQ_CONT_TYPE_SENSORS:
		{
			vector<SCA_ISensor*>& linkedsensors = ((SCA_IController *)self_plus)->GetLinkedSensors();
			SCA_ISensor* sensor;
			for (unsigned int index=0;index<linkedsensors.size();index++) {
				sensor = linkedsensors[index];
				if (sensor->GetName() == name)
					return sensor->GetProxy();
			}
			break;
		}
		case KX_PYGENSEQ_CONT_TYPE_ACTUATORS:
		{
			vector<SCA_IActuator*>& linkedactuators = ((SCA_IController *)self_plus)->GetLinkedActuators();
			SCA_IActuator* actuator;
			for (unsigned int index=0;index<linkedactuators.size();index++) {
				actuator = linkedactuators[index];
				if (actuator->GetName() == name)
					return actuator->GetProxy();
			}
			break;
		}
		case KX_PYGENSEQ_OB_TYPE_SENSORS:
		{
			SCA_SensorList& linkedsensors= ((KX_GameObject *)self_plus)->GetSensors();
			SCA_ISensor *sensor;
			for (unsigned int index=0;index<linkedsensors.size();index++) {
				sensor= linkedsensors[index];
				if (sensor->GetName() == name)
					return sensor->GetProxy();
			}
			break;
		}
		case KX_PYGENSEQ_OB_TYPE_CONTROLLERS:
		{
			SCA_ControllerList& linkedcontrollers= ((KX_GameObject *)self_plus)->GetControllers();
			SCA_IController *controller;
			for (unsigned int index=0;index<linkedcontrollers.size();index++) {
				controller= linkedcontrollers[index];
				if (controller->GetName() == name)
					return controller->GetProxy();
			}
			break;
		}
		case KX_PYGENSEQ_OB_TYPE_ACTUATORS:
		{
			SCA_ActuatorList& linkedactuators= ((KX_GameObject *)self_plus)->GetActuators();
			SCA_IActuator *actuator;
			for (unsigned int index=0;index<linkedactuators.size();index++) {
				actuator= linkedactuators[index];
				if (actuator->GetName() == name)
					return actuator->GetProxy();
			}
			break;
		}
	}
	
	PyErr_Format( PyExc_KeyError, "requested item \"%s\" does not exist", name);
	return NULL;
}

static PyMappingMethods KX_PythonSeq_as_mapping = {
	KX_PythonSeq_len,	/* mp_length */
	KX_PythonSeq_subscript,	/* mp_subscript */
	0,	/* mp_ass_subscript */
};


/*
 * Initialize the interator index
 */

static PyObject *KX_PythonSeq_getIter(KX_PythonSeq *self)
{
	if(BGE_PROXY_REF(self->base)==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	/* create a new iterator if were alredy using this one */
	if (self->iter == -1) {
		self->iter = 0;
		Py_INCREF(self);
		return (PyObject *)self;
	} else {
		return KX_PythonSeq_CreatePyObject(self->base, self->type);
 	}
 }
 

/*
 * Return next KX_PythonSeq iter.
 */
 
static PyObject *KX_PythonSeq_nextIter(KX_PythonSeq *self)
{
	PyObject *object = KX_PythonSeq_getIndex((PyObject *)self, self->iter);
	
	self->iter++;
	if( object==NULL ) {
		self->iter= -1; /* for reuse */
		PyErr_SetString(PyExc_StopIteration,	"iterator at end");
	}
	return object; /* can be NULL for end of iterator */
}


static int KX_PythonSeq_compare( KX_PythonSeq * a, KX_PythonSeq * b ) /* TODO - python3.x wants richcmp */
{
	return ( a->type == b->type && a->base == b->base) ? 0 : -1;	
}

/*
 * repr function
 * convert to a list and get its string value
 */
static PyObject *KX_PythonSeq_repr( KX_PythonSeq * self )
{
	PyObject *list = PySequence_List((PyObject *)self);
	PyObject *repr = PyObject_Repr(list);
	Py_DECREF(list);
	return repr;
}


/*****************************************************************************/
/* Python KX_PythonSeq_Type structure definition:                               */
/*****************************************************************************/
PyTypeObject KX_PythonSeq_Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif
	/*  For printing, in format "<module>.<name>" */
	"KX_PythonSeq",           /* char *tp_name; */
	sizeof( KX_PythonSeq ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) KX_PythonSeq_dealloc, /* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) KX_PythonSeq_compare, /* cmpfunc tp_compare; */
	( reprfunc ) KX_PythonSeq_repr,   /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	    /* PySequenceMethods *tp_as_sequence; */
	&KX_PythonSeq_as_mapping,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc) KX_PythonSeq_getIter, /* getiterfunc tp_iter; */
	( iternextfunc ) KX_PythonSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};
