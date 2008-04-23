/* 
 * $Id$
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
 * The Original Code is Copyright (C) 2006, Blender Foundation
 * All rights reserved.
 *
 * Original code is this file
 *
 * Contributor(s): Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
*/

#include "Node.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "DNA_material_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "gen_utils.h"
#include "vector.h"

static PyObject *Node_repr( BPy_Node * self );
static int Node_compare(BPy_Node *a, BPy_Node *b);
static PyObject *ShadeInput_repr( BPy_ShadeInput * self );
static int ShadeInput_compare(BPy_ShadeInput *a, BPy_ShadeInput *b);
static BPy_ShadeInput *ShadeInput_CreatePyObject(ShadeInput *shi);

/* node socket type */

static PyObject *NodeSocket_getName(BPy_NodeSocket *self, void *unused)
{
	return PyString_FromString(self->name);
}

static int NodeSocket_setName(BPy_NodeSocket *self, PyObject *value, void *unused)
{
	char *name = NULL;

	if (!PyString_Check(value))
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected a string" );

	name = PyString_AsString(value);

	if (!name)
		return EXPP_ReturnIntError(PyExc_RuntimeError,
			"couldn't convert value to string!");

	BLI_strncpy(self->name, name, NODE_MAXSTR);

	return 0;
}

static PyObject *NodeSocket_getVal(BPy_NodeSocket *self, void *unused)
{
	PyObject *pyret = NULL;

	if (self->type == SOCK_VALUE) {
		pyret = PyFloat_FromDouble(self->val[0]);
	}
	else { /* VECTOR or RGBA */
		pyret = newVectorObject(self->val, self->type, Py_NEW);

		if (!pyret)
			return EXPP_ReturnPyObjError(PyExc_RuntimeError,
				"couldn't create vector object!");
	}

	return pyret;
}

static int NodeSocket_setVal(BPy_NodeSocket *self, PyObject *value, void *unused)
{
	int error = 0;

	if (PySequence_Check(value)) {
		PyObject *item, *fpyval;
		int i, len;

		len = PySequence_Size(value);

		if (len == 3 || len == 4) {
			for (i = 0; i < len; i++) {
				item = PySequence_GetItem(value, i);
				fpyval = PyNumber_Float(item);
				if (!fpyval) {
					Py_DECREF(item);
					error = 1;
					break;
				}
				self->val[i] = (float)PyFloat_AsDouble(fpyval);
				Py_DECREF(item);
				Py_DECREF(fpyval);
			}
			if (len == 3)
				self->type = SOCK_VECTOR;
			else /* len == 4 */
				self->type = SOCK_RGBA;
		}
		else error = 1;
	}
	else if (VectorObject_Check(value)) {
		VectorObject *vecOb = (VectorObject *)value;
		short vlen = vecOb->size;

		if (vlen == 3 || vlen == 4) {
			VECCOPY(self->val, vecOb->vec); /* copies 3 values */
			if (vlen == 3)
				self->type = SOCK_VECTOR;
			else {
				self->val[3] = vecOb->vec[3];
				self->type = SOCK_RGBA;
			}
		}
		else error = 1;
	}
	else if (PyNumber_Check(value)) {
		self->val[0] = (float)PyFloat_AsDouble(value);
		self->type = SOCK_VALUE;
	}
	else error = 1;

	if (error)
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected a float or a sequence (or vector) of 3 or 4 floats" );
	return 0;
}

static PyObject *NodeSocket_getMin(BPy_NodeSocket *self, void *unused)
{
	return PyFloat_FromDouble(self->min);
}

static int NodeSocket_setMin(BPy_NodeSocket *self, PyObject *value, void *unused)
{
	PyObject *pyval = PyNumber_Float(value);

	if (!pyval)
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected a float number" );

	self->min = (float)PyFloat_AsDouble(pyval);
	Py_DECREF(pyval);

	return 0;
}

static PyObject *NodeSocket_getMax(BPy_NodeSocket *self, void *unused)
{
	return PyFloat_FromDouble(self->max);
}

static int NodeSocket_setMax(BPy_NodeSocket *self, PyObject *value, void *unused)
{
	PyObject *pyval = PyNumber_Float(value);

	if (!pyval)
		return EXPP_ReturnIntError( PyExc_TypeError,
			"expected a float number" );

	self->max = (float)PyFloat_AsDouble(pyval);
	Py_DECREF(pyval);

	return 0;
}

static PyGetSetDef NodeSocket_getseters[] = {
	{"name", (getter)NodeSocket_getName, (setter)NodeSocket_setName,
		"This socket's name", NULL},
	{"val", (getter)NodeSocket_getVal, (setter)NodeSocket_setVal,
		"This socket's data value(s)", NULL},
	{"min", (getter)NodeSocket_getMin, (setter)NodeSocket_setMin,
		"This socket's min possible value (lower range limit)", NULL},
	{"max", (getter)NodeSocket_getMax, (setter)NodeSocket_setMax,
		"This socket's max possible value (upper range limit)", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static void NodeSocket_dealloc(BPy_NodeSocket *self)
{
	self->ob_type->tp_free((PyObject *)self);
}

static PyObject *NodeSocket_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *pysocket = type->tp_alloc(type, 0);

	if (!pysocket)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError, "couldn't create socket type!");

	return pysocket;
}

static int NodeSocket_init(BPy_NodeSocket *self, PyObject *args, PyObject *kwargs)
{
	char *name = NULL;
	float min = 0.0f, max = 1.0f;
	PyObject *val = NULL;
	static char *kwlist[] = {"name", "val", "min", "max", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|Off", kwlist, &name, &val, &min, &max)){
		return EXPP_ReturnIntError(PyExc_AttributeError, "expected a string and optionally:\n1) a float or a sequence (or vector) of 3 or 4 floats and\n2) two floats");
	}

	BLI_strncpy(self->name, name, NODE_MAXSTR);

	self->min = min;
	self->max = max;

	if (val)
		return NodeSocket_setVal(self, val, NULL);
	/* else */
	self->type = SOCK_VALUE;
	self->val[0] = 0.0f;

	return 0;
}

static PyObject *NodeSocket_copy(BPy_NodeSocket *self)
{
	BPy_NodeSocket *copied;

	copied = (BPy_NodeSocket*)NodeSocket_new(&NodeSocket_Type, NULL, NULL);

	if (!copied) return NULL; /* error already set in NodeSocket_new */

	BLI_strncpy(copied->name, self->name, NODE_MAXSTR);
	copied->min = self->min;
	copied->max = self->max;
	copied->type = self->type;
	memcpy(copied->val, self->val, 4*sizeof(float));

	return (PyObject *)copied;
}

static PyMethodDef BPy_NodeSocket_methods[] = {
	{"__copy__", ( PyCFunction ) NodeSocket_copy, METH_NOARGS,
	 "() - Makes a copy of this node socket."},
	{"copy", ( PyCFunction ) NodeSocket_copy, METH_NOARGS,
	 "() - Makes a copy of this node socket."},
	{NULL, NULL, 0, NULL}
};

PyTypeObject NodeSocket_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Node.Socket",           /* char *tp_name; */
	sizeof( BPy_NodeSocket ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor)NodeSocket_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,      					/* PyNumberMethods *tp_as_number; */
	NULL,					    /* PySequenceMethods *tp_as_sequence; */
	NULL,      /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/input buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /* long tp_flags; */

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
	0, //( getiterfunc) MVertSeq_getIter, /* getiterfunc tp_iter; */
	0, //( iternextfunc ) MVertSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_NodeSocket_methods,     /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NodeSocket_getseters,       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	(initproc)NodeSocket_init,  /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NodeSocket_new,				/* newfunc tp_new; */
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

/**
 * Take the descriptions from tuple and create sockets for those in socks
 * socks is a socketstack from a bNodeTypeInfo
 */
static int pysockets_to_blendersockets(PyObject *tuple, bNodeSocketType **socks, int stage, int limit) {
	int len = 0, a = 0, pos = 0, retval = 0;
	short stype;
	BPy_NodeSocket *pysock;
	bNodeSocketType *nsocks = NULL;

	if (BTST2(stage, NODE_DYNAMIC_READY, NODE_DYNAMIC_ADDEXIST))
		return 0; /* already has sockets */

	len = PyTuple_Size(tuple);

	nsocks = MEM_callocN(sizeof(bNodeSocketType)*(len+1), "bNodeSocketType in Node.c");

	for (pos = 0, a = 0; pos< len; pos++, a++) {
		pysock = (BPy_NodeSocket *)PyTuple_GetItem(tuple, pos);/*borrowed*/

		if (!BPy_NodeSocket_Check(pysock)) {
			PyErr_SetString(PyExc_AttributeError, "expected a sequence of node sockets");
			retval = -1;
			break;
		}

		stype = pysock->type;

		nsocks[a].type = stype;
		nsocks[a].limit = limit;

		nsocks[a].name = BLI_strdupn(pysock->name, NODE_MAXSTR);

		nsocks[a].min = pysock->min;
		nsocks[a].max = pysock->max;

		if (stype > SOCK_VALUE) {
			float *vec = pysock->val;

			nsocks[a].val1 = vec[0];
			nsocks[a].val2 = vec[1];
			nsocks[a].val3 = vec[2];

			if (stype == SOCK_RGBA)
				nsocks[a].val4 = vec[3];
		}
		else /* SOCK_VALUE */
			nsocks[a].val1 = pysock->val[0];
	}

	nsocks[a].type = -1;

	*socks = nsocks;

	return retval;
}

static void NodeSocketLists_dealloc(BPy_NodeSocketLists *self)
{
	Py_DECREF(self->input);
	Py_DECREF(self->output);
	self->ob_type->tp_free((PyObject *)self);
}

static PyObject *Map_socketdef_getter(BPy_NodeSocketLists *self, void *closure)
{
	PyObject *sockets = NULL;

	switch (GET_INT_FROM_POINTER(closure)) {
		case 'I': /* inputs */
			Py_INCREF(self->input);
			sockets = self->input;
			break;
		case 'O': /* outputs */
			Py_INCREF(self->output);
			sockets = self->output;
			break;
		default:
			fprintf(stderr, "DEBUG pynodes: wrong option in Map_socketdef_getter\n");
			Py_INCREF(Py_None);
			sockets = Py_None;
			break;
	}

	return sockets;
}

static int Map_socketdef(BPy_NodeSocketLists *self, PyObject *args, void *closure)
{
	bNode *node = NULL;
	PyObject *tuple = NULL;

	node = self->node;

	if(!node) {
		fprintf(stderr,"DEBUG pynodes: No bNode in BPy_Node (Map_socketdef)\n");
		return 0;
	}

	if(BTST2(node->custom1, NODE_DYNAMIC_READY, NODE_DYNAMIC_ADDEXIST))
		return 0;

	switch(GET_INT_FROM_POINTER(closure)) {
		case 'I':
			if (args) {
				if(PySequence_Check(args)) {
					tuple = PySequence_Tuple(args);
					pysockets_to_blendersockets(tuple,
							&(node->typeinfo->inputs), node->custom1, 1);
					Py_DECREF(self->input);
					self->input = tuple;
				} else {
					return(EXPP_ReturnIntError( PyExc_AttributeError, "INPUT must be a List of Lists or Tuples"));
				}
			}
			break;
		case 'O':
			if (args) {
				if(PyList_Check(args)) {
					tuple = PySequence_Tuple(args);
					pysockets_to_blendersockets(tuple,
							&(node->typeinfo->outputs), node->custom1, 0);
					Py_DECREF(self->output);
					self->output = tuple;
				} else {
					return(EXPP_ReturnIntError( PyExc_AttributeError, "OUTPUT must be a List of Lists or Tuples"));
				}
			}
			break;
		default:
			fprintf(stderr,"DEBUG pynodes: got no list in Map_socketdef\n");
			break;
	}
	return 0;
}

static PyGetSetDef NodeSocketLists_getseters[] = {
	{"input", (getter)Map_socketdef_getter, (setter)Map_socketdef,
		"Set this node's input sockets (list of lists or tuples)",
		(void *)'I'},
	{"i" /*alias*/, (getter)Map_socketdef_getter, (setter)Map_socketdef,
		"Set this node's input sockets (list of lists or tuples)",
		(void *)'I'},
	{"output", (getter)Map_socketdef_getter, (setter)Map_socketdef,
		"Set this node's output sockets (list of lists or tuples)",
		(void *)'O'},
	{"o" /*alias*/, (getter)Map_socketdef_getter, (setter)Map_socketdef,
		"Set this node's output sockets (list of lists or tuples)",
		(void *)'O'},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

PyTypeObject NodeSocketLists_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Node.SocketLists",           /* char *tp_name; */
	sizeof( BPy_NodeSocketLists ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	(destructor)NodeSocketLists_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,      					/* PyNumberMethods *tp_as_number; */
	NULL,					    /* PySequenceMethods *tp_as_sequence; */
	NULL,      /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/input buffer */
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
	0, //( getiterfunc) MVertSeq_getIter, /* getiterfunc tp_iter; */
	0, //( iternextfunc ) MVertSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	0, //BPy_MVertSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NodeSocketLists_getseters,      /* struct PyGetSetDef *tp_getset; */
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

BPy_NodeSocketLists *Node_CreateSocketLists(bNode *node) {
	BPy_NodeSocketLists *socklists = PyObject_NEW(BPy_NodeSocketLists, &NodeSocketLists_Type);
	socklists->node = node;
	socklists->input = PyList_New(0);
	socklists->output = PyList_New(0);
	return socklists;
}

/***************************************/

static int Sockinmap_len ( BPy_SockMap * self) {
	bNode *node = self->node;
	bNodeType *tinfo;
	int a = 0;

	if (!node) return 0;

	tinfo = node->typeinfo;

	if (BNTST(node->custom1, NODE_DYNAMIC_READY)) return 0;

	if (tinfo && tinfo->inputs) {
		while(self->node->typeinfo->inputs[a].type!=-1)
			a++;
	}
	return a;
}

static int sockinmap_has_key( BPy_SockMap *self, char *strkey) {
	bNode *node = self->node;
	bNodeType *tinfo;
	int a = 0;

	if (!node || !strkey) return -1;

	tinfo = node->typeinfo;

	if(tinfo && tinfo->inputs){
		while(self->node->typeinfo->inputs[a].type!=-1) {
			if(BLI_strcaseeq(self->node->typeinfo->inputs[a].name, strkey)) {
				return a;
			}
			a++;
		}
	}
	return -1;
}

PyObject *Sockinmap_subscript(BPy_SockMap *self, PyObject *pyidx) {
	int idx;

	if (!self->node)
		return EXPP_ReturnPyObjError(PyExc_RuntimeError, "no access to Blender node data!");

	if (PyString_Check(pyidx)) {
		idx = sockinmap_has_key(self, PyString_AsString(pyidx));
	}
	else if(PyInt_Check(pyidx)) {
		int len = Sockinmap_len(self);
		idx = (int)PyInt_AsLong(pyidx);
		if (idx < 0 || idx >= len)
			return EXPP_ReturnPyObjError(PyExc_IndexError, "index out of range");
	}
	else if (PySlice_Check(pyidx)) {
		return EXPP_ReturnPyObjError(PyExc_ValueError, "slices not implemented");
	} else {
		return EXPP_ReturnPyObjError(PyExc_IndexError, "index must be an int or a string");
	}

	if(idx<0) { /* we're not as nice as Python */
		return EXPP_ReturnPyObjError(PyExc_IndexError, "invalid socket index");
	}
	
	switch(self->node->typeinfo->inputs[idx].type) {
		case SOCK_VALUE:
			return Py_BuildValue("f", self->stack[idx]->vec[0]);
			break;
		case SOCK_VECTOR:
			return Py_BuildValue("(fff)", self->stack[idx]->vec[0], self->stack[idx]->vec[1], self->stack[idx]->vec[2]);
			break;
		case SOCK_RGBA:
			/* otherwise RGBA tuple */
			return Py_BuildValue("(ffff)", self->stack[idx]->vec[0], self->stack[idx]->vec[1], self->stack[idx]->vec[2], self->stack[idx]->vec[3]);
			break;
		default:
			break;
	}

	Py_RETURN_NONE;
}

static PyObject *Sockinmap_getAttr(BPy_SockMap *self, char *attr)
{
	PyObject *pyob = NULL;
	int idx;

	idx = sockinmap_has_key(self, attr);

	if (idx < 0)
		return EXPP_ReturnPyObjError(PyExc_AttributeError, "unknown input socket name");

	switch(self->node->typeinfo->inputs[idx].type) {
		case SOCK_VALUE:
			pyob = Py_BuildValue("f", self->stack[idx]->vec[0]);
			break;
		case SOCK_VECTOR:
			pyob = Py_BuildValue("(fff)", self->stack[idx]->vec[0], self->stack[idx]->vec[1], self->stack[idx]->vec[2]);
			break;
		case SOCK_RGBA:
			pyob = Py_BuildValue("(ffff)", self->stack[idx]->vec[0], self->stack[idx]->vec[1], self->stack[idx]->vec[2], self->stack[idx]->vec[3]);
			break;
		default:
			break;
	}

	return pyob;
}

/* read only */
static PyMappingMethods Sockinmap_as_mapping = {
	( inquiry ) Sockinmap_len,  /* mp_length */
	( binaryfunc ) Sockinmap_subscript, /* mp_subscript */
	( objobjargproc ) 0 /* mp_ass_subscript */
};

PyTypeObject SockInMap_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Node.InputSockets",           /* char *tp_name; */
	sizeof( BPy_SockMap ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	(getattrfunc) Sockinmap_getAttr,/* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,      					/* PyNumberMethods *tp_as_number; */
	NULL,					    /* PySequenceMethods *tp_as_sequence; */
	&Sockinmap_as_mapping,      /* PyMappingMethods *tp_as_mapping; */

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
	0, //( getiterfunc) MVertSeq_getIter, /* getiterfunc tp_iter; */
	0, //( iternextfunc ) MVertSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	0, //BPy_MVertSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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

static int Sockoutmap_len ( BPy_SockMap * self) {
	bNode *node = self->node;
	bNodeType *tinfo;
	int a = 0;

	if (!node) return 0;

	tinfo = node->typeinfo;

	if (tinfo && tinfo->outputs) {
		while(self->node->typeinfo->outputs[a].type!=-1)
			a++;
	}
	return a;
}

static int sockoutmap_has_key(BPy_SockMap *self, char *strkey) {
	bNode *node = self->node;
	bNodeType *tinfo;
	int a = 0;

	if (!node) return -1;

	tinfo = node->typeinfo;

	if(tinfo && tinfo->outputs){
		while(self->node->typeinfo->outputs[a].type!=-1) {
			if(BLI_strcaseeq(self->node->typeinfo->outputs[a].name, strkey)) {
				return a;
			}
			a++;
		}
	}
	return -1;
}

static int Sockoutmap_assign_subscript(BPy_SockMap *self, PyObject *pyidx, PyObject *value) {
	int i, idx, len, type, wanted_len = 0;
	PyObject *val;
	PyObject *items[4];

	if (!self->node)
		return EXPP_ReturnIntError(PyExc_RuntimeError, "no access to Blender node data!");

	if (PyInt_Check(pyidx)) {
		idx = (int)PyInt_AsLong(pyidx);
		if (idx < 0 || idx >= Sockinmap_len(self))
			return EXPP_ReturnIntError(PyExc_IndexError, "index out of range");
	}
	else if (PyString_Check(pyidx)) {
		idx = sockoutmap_has_key(self, PyString_AsString(pyidx));
	}
	else if (PySlice_Check(pyidx)) {
		return EXPP_ReturnIntError(PyExc_ValueError, "slices not yet implemented");
	} else {
		return EXPP_ReturnIntError(PyExc_IndexError, "index must be a positive int or a string");
	}

	if (idx < 0)
		return EXPP_ReturnIntError(PyExc_IndexError, "index must be a positive int or a string");

	type = self->node->typeinfo->outputs[idx].type;

	if (type == SOCK_VALUE) {
		val = PyNumber_Float(value);
		if (!val)
			return EXPP_ReturnIntError(PyExc_AttributeError, "expected a float value");
		self->stack[idx]->vec[0] = (float)PyFloat_AsDouble(val);
		Py_DECREF(val);
	}
	else {
		val = PySequence_Fast(value, "expected a numeric tuple or list");
		if (!val) return -1;

		len = PySequence_Fast_GET_SIZE(val);

		if (type == SOCK_VECTOR) {
			wanted_len = 3;
		} else { /* SOCK_RGBA */
			wanted_len = 4;
		}

		if (len != wanted_len) {
			Py_DECREF(val);
			PyErr_SetString(PyExc_AttributeError, "wrong number of items in list or tuple");
			fprintf(stderr, "\nExpected %d numeric values, got %d.", wanted_len, len);
			return -1;
		}

		for (i = 0; i < len; i++) {
			items[i] = PySequence_Fast_GET_ITEM(val, i); /* borrowed */
			if (!PyNumber_Check(items[i])) {
				Py_DECREF(val);
				return EXPP_ReturnIntError(PyExc_AttributeError, "expected a *numeric* tuple or list");
			}
		}

		self->stack[idx]->vec[0] = (float)PyFloat_AsDouble(items[0]);
		self->stack[idx]->vec[1] = (float)PyFloat_AsDouble(items[1]);
		self->stack[idx]->vec[2] = (float)PyFloat_AsDouble(items[2]);

		if (type == SOCK_RGBA)
			self->stack[idx]->vec[3] = (float)PyFloat_AsDouble(items[3]);

		Py_DECREF(val);
	}

	return 0;
}

static int sockoutmap_set_attr(bNodeStack **stack, short type, short idx, PyObject *value)
{
	PyObject *val;
	PyObject *items[4];
	int i;
	short len, wanted_len;

	if (type == SOCK_VALUE) {
		val = PyNumber_Float(value);
		if (!val)
			return EXPP_ReturnIntError(PyExc_AttributeError, "expected a float value");
		stack[idx]->vec[0] = (float)PyFloat_AsDouble(val);
		Py_DECREF(val);
	}
	else {
		val = PySequence_Fast(value, "expected a numeric tuple or list");
		if (!val) return -1;

		len = PySequence_Fast_GET_SIZE(val);

		if (type == SOCK_VECTOR) {
			wanted_len = 3;
		} else { /* SOCK_RGBA */
			wanted_len = 4;
		}

		if (len != wanted_len) {
			Py_DECREF(val);
			PyErr_SetString(PyExc_AttributeError, "wrong number of items in list or tuple");
			fprintf(stderr, "\nExpected %d numeric values, got %d.", wanted_len, len);
			return -1;
		}

		for (i = 0; i < len; i++) {
			items[i] = PySequence_Fast_GET_ITEM(val, i); /* borrowed */
			if (!PyNumber_Check(items[i])) {
				Py_DECREF(val);
				return EXPP_ReturnIntError(PyExc_AttributeError, "expected a *numeric* tuple or list");
			}
		}

		stack[idx]->vec[0] = (float)PyFloat_AsDouble(items[0]);
		stack[idx]->vec[1] = (float)PyFloat_AsDouble(items[1]);
		stack[idx]->vec[2] = (float)PyFloat_AsDouble(items[2]);

		if (type == SOCK_RGBA)
			stack[idx]->vec[3] = (float)PyFloat_AsDouble(items[3]);

		Py_DECREF(val);
	}

	return 0;
}

static int Sockoutmap_setAttr(BPy_SockMap *self, char *name, PyObject *value) {
	short idx, type;

	if (!self->node)
		return EXPP_ReturnIntError(PyExc_RuntimeError, "no access to Blender node data!");

	idx = sockoutmap_has_key(self, name);

	if (idx < 0)
		return EXPP_ReturnIntError(PyExc_AttributeError, "unknown output socket name");

	type = self->node->typeinfo->outputs[idx].type;

	return sockoutmap_set_attr(self->stack, type, idx, value);
}
/* write only */
static PyMappingMethods Sockoutmap_as_mapping = {
	( inquiry ) Sockoutmap_len,  /* mp_length */
	( binaryfunc ) 0, /* mp_subscript */
	( objobjargproc ) Sockoutmap_assign_subscript /* mp_ass_subscript */
};

PyTypeObject SockOutMap_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Node.OutputSockets",           /* char *tp_name; */
	sizeof( BPy_SockMap ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	(setattrfunc) Sockoutmap_setAttr,/* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,      					/* PyNumberMethods *tp_as_number; */
	NULL,					    /* PySequenceMethods *tp_as_sequence; */
	&Sockoutmap_as_mapping,      /* PyMappingMethods *tp_as_mapping; */

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
	0, //( getiterfunc) MVertSeq_getIter, /* getiterfunc tp_iter; */
	0, //( iternextfunc ) MVertSeq_nextIter, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	0, //BPy_MVertSeq_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
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


static BPy_SockMap *Node_CreateInputMap(bNode *node, bNodeStack **stack) {
	BPy_SockMap *map = PyObject_NEW(BPy_SockMap, &SockInMap_Type);
	map->node = node;
	map->stack = stack;
	return map;
}

static PyObject *Node_GetInputMap(BPy_Node *self) {
	BPy_SockMap *inmap = Node_CreateInputMap(self->node, self->in);
	return (PyObject *)(inmap);
}

#define SURFACEVIEWVECTOR	0
#define VIEWNORMAL			1
#define SURFACENORMAL		2
#define GLOBALTEXTURE		3
#define TEXTURE				4
#define PIXEL				5
#define COLOR				6
#define SPECULAR_COLOR		7
#define MIRROR_COLOR		8
#define AMBIENT_COLOR		9
#define AMBIENT				10
#define EMIT				11
#define DISPLACE			12
#define STRAND				13
#define STRESS				14
#define TANGENT				15
#define SURFACE_D			30
#define TEXTURE_D			31
#define GLOBALTEXTURE_D		32
#define REFLECTION_D		33
#define NORMAL_D			34
#define STICKY_D			35
#define REFRACT_D			36
#define STRAND_D			37

/* MACRO time: defining shi getters */

/* a couple checks that we can redefine to nothing for a tiny performance
 * gain */

#define SHI_CHECK_SHI\
	if (!self->shi)\
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,\
				"no shade input data!");

#define SHI_CHECK_OB\
	if (!ob)\
		return EXPP_ReturnPyObjError(PyExc_RuntimeError,\
				"couldn't create vector object!");

/* for shi getters: */

#define SHI_GETATTR_f(name, var)\
static PyObject *ShadeInput_get##name(BPy_ShadeInput *self, void *unused)\
{\
	PyObject *ob = NULL;\
\
	SHI_CHECK_SHI\
\
	ob = PyFloat_FromDouble((double)(self->shi->var));\
\
	SHI_CHECK_OB\
\
	return ob;\
}

#define SHI_GETATTR_fvec(name, var, len)\
static PyObject *ShadeInput_get##name(BPy_ShadeInput *self, void *unused)\
{\
	PyObject *ob = NULL;\
\
	SHI_CHECK_SHI\
\
	ob = newVectorObject(self->shi->var, len, Py_NEW);\
\
	SHI_CHECK_OB\
\
	return ob;\
}

#define SHI_GETATTR_2fvec(name, v1, v2, len)\
static PyObject *ShadeInput_get##name(BPy_ShadeInput *self, void *unused)\
{\
	PyObject *ob = NULL;\
	PyObject *tuple = NULL;\
\
	SHI_CHECK_SHI\
\
	tuple = PyTuple_New(2);\
\
	ob = newVectorObject(self->shi->v1, len, Py_NEW);\
	PyTuple_SET_ITEM(tuple, 0, ob);\
\
	ob = newVectorObject(self->shi->v2, len, Py_NEW);\
	PyTuple_SET_ITEM(tuple, 1, ob);\
\
	return tuple;\
}

#define SHI_GETATTR_3f(name, v1, v2, v3)\
static PyObject *ShadeInput_get##name(BPy_ShadeInput *self, void *unused)\
{\
	PyObject *ob = NULL;\
	float vec[3];\
\
	SHI_CHECK_SHI\
\
	vec[0] = self->shi->v1;\
	vec[1] = self->shi->v2;\
	vec[2] = self->shi->v3;\
\
	ob = newVectorObject(vec, 3, Py_NEW);\
\
	SHI_CHECK_OB\
\
	return ob;\
}

/* float */

SHI_GETATTR_f(Ambient, amb);
SHI_GETATTR_f(Emit, emit);
SHI_GETATTR_f(Strand, strandco);
SHI_GETATTR_f(Stress, stress);

/* 3 float vars */

SHI_GETATTR_3f(Color, r, g, b)
SHI_GETATTR_3f(ColorSpecular, specr, specg, specb)
SHI_GETATTR_3f(ColorMirror, mirr, mirg, mirb)
SHI_GETATTR_3f(ColorAmbient, ambr, ambg, ambb)

/* float vector */

SHI_GETATTR_fvec(SurfaceViewVector, view, 3)
SHI_GETATTR_fvec(SurfaceNormal, facenor, 3)
SHI_GETATTR_fvec(ViewNormal, vn, 3)
SHI_GETATTR_fvec(TextureGlobal, gl, 3)
SHI_GETATTR_fvec(Texture, lo, 3)
SHI_GETATTR_fvec(Displace, displace, 3)
SHI_GETATTR_fvec(Tangent, tang, 3)

/* two float vectors */

SHI_GETATTR_2fvec(SurfaceD, dxco, dyco, 3)
SHI_GETATTR_2fvec(TextureD, dxlo, dylo, 3)
SHI_GETATTR_2fvec(TextureGlobalD, dxgl, dygl, 3)
SHI_GETATTR_2fvec(ReflectionD, dxref, dyref, 3)
SHI_GETATTR_2fvec(NormalD, dxno, dyno, 3)
SHI_GETATTR_2fvec(StickyD, dxsticky, dysticky, 3)
SHI_GETATTR_2fvec(RefractD, dxrefract, dyrefract, 3)

/* single cases (for now), not macros: */

static PyObject *ShadeInput_getPixel(BPy_ShadeInput *self, void *unused)
{
	PyObject *ob = NULL;

	SHI_CHECK_SHI

	ob = Py_BuildValue("(ii)", self->shi->xs, self->shi->ys);

	SHI_CHECK_OB

	return ob;
}

static PyObject *ShadeInput_getStrandD(BPy_ShadeInput *self, void *unused)
{
	PyObject *ob = NULL;
	float vec[2];

	SHI_CHECK_SHI

	vec[0] = self->shi->dxstrand;
	vec[1] = self->shi->dystrand;

	ob = newVectorObject(vec, 2, Py_NEW);

	SHI_CHECK_OB

	return ob;
}

static BPy_SockMap *Node_CreateOutputMap(bNode *node, bNodeStack **stack) {
	BPy_SockMap *map = PyObject_NEW(BPy_SockMap, &SockOutMap_Type);
	map->node = node;
	map->stack = stack;
	return map;
}

static PyObject *Node_GetOutputMap(BPy_Node *self) {
	BPy_SockMap *outmap = Node_CreateOutputMap(self->node, self->out);
	return (PyObject *)outmap;
}

static PyObject *Node_GetShi(BPy_Node *self) {
	BPy_ShadeInput *shi = ShadeInput_CreatePyObject(self->shi);
	return (PyObject *)shi;
}

static PyObject *node_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *self;
	self = type->tp_alloc(type, 1);
	return self;
}

static int node_init(BPy_Node *self, PyObject *args, PyObject *kwds)
{
	return 0;
}

static PyGetSetDef BPy_Node_getseters[] = {
	{"input",
		(getter)Node_GetInputMap, (setter)NULL,
		"Get the input sockets mapping (dictionary)",
		NULL},
	{"i", /* alias */
		(getter)Node_GetInputMap, (setter)NULL,
		"Get the input sockets mapping (dictionary)",
		NULL},
	{"output",
		(getter)Node_GetOutputMap, (setter)NULL,
		"Get the output sockets mapping (dictionary)",
		NULL},
	{"o", /* alias */
		(getter)Node_GetOutputMap, (setter)NULL,
		"Get the output sockets mapping (dictionary)",
		NULL},
	{"shi",
		(getter)Node_GetShi, (setter)NULL,
		"Get the Shade Input data (ShadeInput)",
		NULL},
	{"s", /* alias */
		(getter)Node_GetShi, (setter)NULL,
		"Get the Shade Input data (ShadeInput)",
		NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static PyGetSetDef BPy_ShadeInput_getseters[] = {
	{"texture",
	  (getter)ShadeInput_getTexture, (setter)NULL,
	  "Get the current texture coordinate (3-vector)", NULL},
	{"textureGlobal",
	  (getter)ShadeInput_getTextureGlobal, (setter)NULL,
	  "Get the current global texture coordinate (3-vector)", NULL},
	{"surfaceNormal",
	  (getter)ShadeInput_getSurfaceNormal, (setter)NULL,
	  "Get the current surface normal (3-vector)", NULL},
	{"viewNormal",
	  (getter)ShadeInput_getViewNormal, (setter)NULL,
	  "Get the current view normal (3-vector)", NULL},
	{"surfaceViewVector",
	  (getter)ShadeInput_getSurfaceViewVector, (setter)NULL,
	  "Get the vector pointing to the viewpoint from the point being shaded (3-vector)", NULL},
	{"pixel",
	  (getter)ShadeInput_getPixel, (setter)NULL,
	  "Get the x,y-coordinate for the pixel rendered (3-vector)", NULL},
	{"color",
	  (getter)ShadeInput_getColor, (setter)NULL,
	  "Get the color for the point being shaded (3-vector)", NULL},
	{"specularColor",
	  (getter)ShadeInput_getColorSpecular, (setter)NULL,
	  "Get the specular color for the point being shaded (3-vector)", NULL},
	{"mirrorColor",
	  (getter)ShadeInput_getColorMirror, (setter)NULL,
	  "Get the mirror color for the point being shaded (3-vector)", NULL},
	{"ambientColor",
	  (getter)ShadeInput_getColorAmbient, (setter)NULL,
	  "Get the ambient color for the point being shaded (3-vector)", NULL},
	{"ambient",
	  (getter)ShadeInput_getAmbient, (setter)NULL,
	  "Get the ambient factor for the point being shaded (float)", NULL},
	{"emit",
	  (getter)ShadeInput_getEmit, (setter)NULL,
	  "Get the emit factor for the point being shaded (float)", NULL},
	{"displace",
	  (getter)ShadeInput_getDisplace, (setter)NULL,
	  "Get the displace vector for the point being shaded (3-vector)", NULL},
	{"strand",
	  (getter)ShadeInput_getStrand, (setter)NULL,
	  "Get the strand factor(float)", NULL},
	{"stress",
	  (getter)ShadeInput_getStress, (setter)NULL,
	  "Get the stress factor(float)", NULL},
	{"tangent",
	  (getter)ShadeInput_getTangent, (setter)NULL,
	  "Get the tangent vector (3-vector)", NULL},
	{"surfaceD",
	  (getter)ShadeInput_getSurfaceD, (setter)NULL,
	  "Get the surface d (tuple with pair of 3-vectors)", NULL},
	{"textureD",
	  (getter)ShadeInput_getTextureD, (setter)NULL,
	  "Get the texture d (tuple with pair of 3-vectors)", NULL},
	{"textureGlobalD",
	  (getter)ShadeInput_getTextureGlobalD, (setter)NULL,
	  "Get the global texture d (tuple with pair of 3-vectors)", NULL},
	{"reflectionD",
	  (getter)ShadeInput_getReflectionD, (setter)NULL,
	  "Get the reflection d (tuple with pair of 3-vectors)", NULL},
	{"normalD",
	  (getter)ShadeInput_getNormalD, (setter)NULL,
	  "Get the normal d (tuple with pair of 3-vectors)", NULL},
	{"stickyD",
	  (getter)ShadeInput_getStickyD, (setter)NULL,
	  "Get the sticky d (tuple with pair of 3-vectors)", NULL},
	{"refractD",
	  (getter)ShadeInput_getRefractD, (setter)NULL,
	  "Get the refract d (tuple with pair of 3-vectors)", NULL},
	{"strandD",
	  (getter)ShadeInput_getStrandD, (setter)NULL,
	  "Get the strand d (2-vector)", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

PyTypeObject Node_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Node.node",             /* char *tp_name; */
	sizeof( BPy_Node ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL /*( getattrfunc ) PyObject_GenericGetAttr*/,                       /* getattrfunc tp_getattr; */
	NULL /*( setattrfunc ) PyObject_GenericSetAttr*/,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) Node_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) Node_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,         /* long tp_flags; */

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
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL, /*BPy_Node_methods,*/          /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_Node_getseters,        /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	(initproc)node_init,                       /* initproc tp_init; */
	/*PyType_GenericAlloc*/NULL,                       /* allocfunc tp_alloc; */
	node_new,                       /* newfunc tp_new; */
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

PyTypeObject ShadeInput_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Node.ShadeInput",             /* char *tp_name; */
	sizeof( BPy_ShadeInput ),         /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) ShadeInput_compare,   /* cmpfunc tp_compare; */
	( reprfunc ) ShadeInput_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

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
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL, /*BPy_Node_methods,*/          /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	BPy_ShadeInput_getseters,   /* struct PyGetSetDef *tp_getset; */
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

/* Initialise Node module */
PyObject *Node_Init(void)
{
	PyObject *submodule;

	if( PyType_Ready( &Node_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &ShadeInput_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &NodeSocket_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &NodeSocketLists_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &SockInMap_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &SockOutMap_Type ) < 0 )
		return NULL;
	submodule = Py_InitModule3( "Blender.Node", NULL, "");

	PyModule_AddIntConstant(submodule, "VALUE", SOCK_VALUE);
	PyModule_AddIntConstant(submodule, "RGBA", SOCK_RGBA);
	PyModule_AddIntConstant(submodule, "VECTOR", SOCK_VECTOR);

	Py_INCREF(&NodeSocket_Type);
	PyModule_AddObject(submodule, "Socket", (PyObject *)&NodeSocket_Type);

	Py_INCREF(&Node_Type);
	PyModule_AddObject(submodule, "Scripted", (PyObject *)&Node_Type);

	return submodule;
}

static int Node_compare(BPy_Node *a, BPy_Node *b)
{
	bNode *pa = a->node, *pb = b->node;
	return (pa == pb) ? 0 : -1;
}

static PyObject *Node_repr(BPy_Node *self)
{
	return PyString_FromFormat( "[Node \"%s\"]",
		self->node ? self->node->id->name+2 : "empty node");
}

BPy_Node *Node_CreatePyObject(bNode *node)
{
	BPy_Node *pynode;

	pynode = (BPy_Node *)PyObject_NEW(BPy_Node, &Node_Type);
	if(!pynode) {
		fprintf(stderr,"Couldn't create BPy_Node object\n");
		return (BPy_Node *)(EXPP_ReturnPyObjError(PyExc_MemoryError, "couldn't create BPy_Node object"));
	}

	pynode->node = node;

	return pynode;
}

int pytype_is_pynode(PyObject *pyob)
{
	return PyObject_TypeCheck(pyob, &Node_Type);
}

void InitNode(BPy_Node *self, bNode *node) {
	self->node = node;
}

bNode *Node_FromPyObject(PyObject *pyobj)
{
	return ((BPy_Node *)pyobj)->node;
}

void Node_SetStack(BPy_Node *self, bNodeStack **stack, int type)
{
	if(type == NODE_INPUTSTACK) {
		self->in = stack;
	} else if(type == NODE_OUTPUTSTACK) {
		self->out = stack;
	}
}

void Node_SetShi(BPy_Node *self, ShadeInput *shi)
{
	self->shi = shi;
}

/*********************/

static int ShadeInput_compare(BPy_ShadeInput *a, BPy_ShadeInput *b)
{
	ShadeInput *pa = a->shi, *pb = b->shi;
	return (pa == pb) ? 0 : -1;
}

static PyObject *ShadeInput_repr(BPy_ShadeInput *self)
{
	return PyString_FromFormat( "[ShadeInput at \"%p\"]", self);
}

BPy_ShadeInput *ShadeInput_CreatePyObject(ShadeInput *shi)
{
	BPy_ShadeInput *pyshi;

	pyshi = (BPy_ShadeInput *)PyObject_NEW(BPy_ShadeInput, &ShadeInput_Type);
	if(!pyshi) {
		fprintf(stderr,"Couldn't create BPy_ShadeInput object\n");
		return (BPy_ShadeInput *)(EXPP_ReturnPyObjError(PyExc_MemoryError, "couldn't create BPy_ShadeInput object"));
	}

	pyshi->shi = shi;

	return pyshi;
}

