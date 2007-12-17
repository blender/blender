/* 
 * $Id: Node.c 10454 2007-04-04 11:27:43Z jesterking $
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
 * The Original Code is Copyright (C) 2006, Blender Foundation
 * All rights reserved.
 *
 * Original code is this file
 *
 * Contributor(s): Nathan Letwory
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifdef USE_PYNODES /* note: won't work without patch */
#include "Node.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "DNA_material_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "gen_utils.h"

static PyObject *Node_repr( BPy_Node * self );
static int Node_compare(BPy_Node *a, BPy_Node *b);
static PyObject *ShadeInput_repr( BPy_ShadeInput * self );
static int ShadeInput_compare(BPy_ShadeInput *a, BPy_ShadeInput *b);

/**
 * Take the descriptions from dict and create sockets for those in socks
 * socks is a socketstack from a bNodeTypeInfo
 */
static int dict_socks_to_typeinfo(PyObject *dict, bNodeSocketType **socks, int len, int stage) {
	int a = 0, pos = 0;
	PyObject *key = NULL, *value = NULL;
	bNodeSocketType *newsocks = NULL;

	if(stage!=SH_NODE_DYNAMIC_READY && stage!=SH_NODE_DYNAMIC_ADDEXIST) {
		newsocks = MEM_callocN(sizeof(bNodeSocketType)*(len+1), "bNodeSocketType");

		if (dict) {
			if(PyDict_Check(dict)) {
				while(PyDict_Next(dict, &pos, &key, &value)) {
					if(PyTuple_Check(value) && PyTuple_Size(value)==7) {
						newsocks[a].name = BLI_strdup(PyString_AsString(key));
						newsocks[a].type = (int)(PyInt_AsLong(PyTuple_GetItem(value, 0)));
						newsocks[a].val1 = (float)(PyFloat_AsDouble(PyTuple_GetItem(value, 1)));
						newsocks[a].val2 = (float)(PyFloat_AsDouble(PyTuple_GetItem(value, 2)));
						newsocks[a].val3 = (float)(PyFloat_AsDouble(PyTuple_GetItem(value, 3)));
						newsocks[a].val4 = (float)(PyFloat_AsDouble(PyTuple_GetItem(value, 4)));
						newsocks[a].min = (float)(PyFloat_AsDouble(PyTuple_GetItem(value, 5)));
						newsocks[a].max = (float)(PyFloat_AsDouble(PyTuple_GetItem(value, 6)));
						a++;
					}
				}
			} else {
				return(EXPP_ReturnIntError( PyExc_AttributeError, "INPUT must be a dict"));
			}
		}
		newsocks[a].type = -1;
		if(*socks) {
			int b = 0;
			while((*socks)[b].type!=-1) {
				MEM_freeN((*socks)[b].name);
				(*socks)[b].name = NULL;
				b++;
			}
			MEM_freeN(*socks);
		}
		*socks = newsocks;
	}
	return 0;
}

/* Get number of complying entries in a dict.
 *
 */
static int num_dict_sockets(PyObject *dict) {
	int a = 0, pos = 0;
	PyObject *key = NULL, *value = NULL;
	while(PyDict_Next(dict, &pos, &key, &value)) {
		if(PyTuple_Check(value) && PyTuple_Size(value)==7)
			a++;
	}
	return a;
}

static int Map_socketdef(PyObject *self, PyObject *args, void *closure)
{
	int newincnt = 0, newoutcnt = 0;
	bNode *node = NULL;
	BPy_DefinitionMap *defs= NULL;

	Py_INCREF(args);
	Py_INCREF(self);

	defs= (BPy_DefinitionMap *)self;
	node= defs->node;

	if(!node) {
		fprintf(stderr,"! no bNode in BPy_Node (Map_socketdef)\n");
		return 0;
	}

	if(node->custom1==SH_NODE_DYNAMIC_READY && node->custom1==SH_NODE_DYNAMIC_ADDEXIST)
		return 0;

	switch((int)closure) {
		case 'I':
			if (args) {
				if(PyDict_Check(args)) {
					newincnt = num_dict_sockets(args);
					dict_socks_to_typeinfo(args, &(node->typeinfo->inputs), newincnt, node->custom1);
				} else {
					Py_DECREF(self);
					Py_DECREF(args);
					return(EXPP_ReturnIntError( PyExc_AttributeError, "INPUT must be a dict"));
				}
			}
			break;
		case 'O':
			if (args) {
				if(PyDict_Check(args)) {
					newoutcnt = num_dict_sockets(args);
					dict_socks_to_typeinfo(args, &(node->typeinfo->outputs), newoutcnt, node->custom1);
				} else {
					Py_DECREF(self);
					Py_DECREF(args);
					return(EXPP_ReturnIntError( PyExc_AttributeError, "OUTPUT must be a dict"));
				}
			}
			break;
		default:
			fprintf(stderr, "Hrm, why we got no dict? Todo: figure out proper complaint to scripter\n");
			break;
	}
	Py_DECREF(self);
	Py_DECREF(args);
	return 0;
}

static PyGetSetDef InputDefMap_getseters[] = {
	{"definitions", (getter)NULL, (setter)Map_socketdef,
		"Set the inputs definition (dictionary)",
		(void *)'I'},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

PyTypeObject InputDefMap_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Node.InputDefinitions",           /* char *tp_name; */
	sizeof( BPy_DefinitionMap ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,/* destructor tp_dealloc; */
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
	InputDefMap_getseters,                       /* struct PyGetSetDef *tp_getset; */
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

BPy_DefinitionMap *Node_CreateInputDefMap(bNode *node) {
	BPy_DefinitionMap *map = PyObject_NEW(BPy_DefinitionMap, &InputDefMap_Type);
	map->node = node;
	return map;
}

/***************************************/

static PyGetSetDef OutputDefMap_getseters[] = {
	{"definitions", (getter)NULL, (setter)Map_socketdef,
		"Set the outputs definition (dictionary)",
		(void *)'O'},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

PyTypeObject OutputDefMap_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender.Node.OutputDefinitions",           /* char *tp_name; */
	sizeof( BPy_DefinitionMap ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	NULL,/* destructor tp_dealloc; */
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
	OutputDefMap_getseters,                       /* struct PyGetSetDef *tp_getset; */
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

BPy_DefinitionMap *Node_CreateOutputDefMap(bNode *node) {
	BPy_DefinitionMap *map = PyObject_NEW(BPy_DefinitionMap, &OutputDefMap_Type);
	map->node = node;
	return map;
}

/***************************************/

static int sockinmap_len ( BPy_SockMap * self) {
	int a = 0;
	if(self->typeinfo) {
		while(self->typeinfo->inputs[a].type!=-1)
			a++;
	}
	return a;
}

static int sockinmap_has_key( BPy_SockMap *self, PyObject *key) {
	int a = 0;
	char *strkey = PyString_AsString(key);

	if(self->typeinfo){
		while(self->typeinfo->inputs[a].type!=-1) {
			if(BLI_strcaseeq(self->typeinfo->inputs[a].name, strkey)) {
				return a;
			}
			a++;
		}
	}
	return -1;
}

PyObject *sockinmap_subscript(BPy_SockMap *self, PyObject *idx) {
	int a, _idx;
	a = sockinmap_len(self);

	if (PyString_Check(idx)) {
		_idx = sockinmap_has_key( self, idx);
	}
	else if(PyInt_Check(idx)) {
		PyErr_SetString(PyExc_ValueError, "int index not implemented");
		Py_RETURN_NONE;
	}
	else if (PySlice_Check(idx)) {
		PyErr_SetString(PyExc_ValueError, "slices not implemented");
		Py_RETURN_NONE;
	} else {
		PyErr_SetString(PyExc_IndexError, "Index must be string");
		Py_RETURN_NONE;
	}

	
	switch(self->typeinfo->inputs[_idx].type) {
		case SOCK_VALUE:
			return Py_BuildValue("f", self->stack[_idx]->vec[0]);
			break;
		case SOCK_VECTOR:
			return Py_BuildValue("(fff)", self->stack[_idx]->vec[0], self->stack[_idx]->vec[1], self->stack[_idx]->vec[2]);
			break;
		case SOCK_RGBA:
			return Py_BuildValue("(ffff)", self->stack[_idx]->vec[0], self->stack[_idx]->vec[1], self->stack[_idx]->vec[2], self->stack[_idx]->vec[3]);
			break;
		default:
			break;
	}
	Py_RETURN_NONE;
}

/* read only */
static PyMappingMethods sockinmap_as_mapping = {
	( inquiry ) sockinmap_len,  /* mp_length */
	( binaryfunc ) sockinmap_subscript, /* mp_subscript */
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
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,      					/* PyNumberMethods *tp_as_number; */
	NULL,					    /* PySequenceMethods *tp_as_sequence; */
	&sockinmap_as_mapping,      /* PyMappingMethods *tp_as_mapping; */

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

static int sockoutmap_len ( BPy_SockMap * self) {
	int a = 0;
	if(self->typeinfo) {
		while(self->typeinfo->outputs[a].type!=-1)
			a++;
	}
	return a;
}

static int sockoutmap_has_key( BPy_SockMap *self, PyObject *key) {
	int a = 0;
	char *strkey = PyString_AsString(key);

	if(self->typeinfo){
		while(self->typeinfo->outputs[a].type!=-1) {
			if(BLI_strcaseeq(self->typeinfo->outputs[a].name, strkey)) {
				return a;
			}
			a++;
		}
	}
	return -1;
}

static int sockoutmap_assign_subscript(BPy_SockMap *self, PyObject *idx, PyObject *value) {
	int a, _idx;
	a = sockoutmap_len(self);
	if(PyInt_Check(idx)) {
		_idx = (int)PyInt_AsLong(idx);
	}
	else if (PyString_Check(idx)) {
		_idx = sockoutmap_has_key( self, idx);
	}
	else if (PySlice_Check(idx)) {
        PyErr_SetString(PyExc_ValueError, "slices not implemented, yet");
        return -1;
    } else {
        PyErr_SetString(PyExc_IndexError, "Index must be int or string");
		return -1;
    }
	if(_idx > -1) {
		switch(self->typeinfo->outputs[_idx].type) {
			case SOCK_VALUE:
				if(PyTuple_Size(value)==1)
					self->stack[_idx]->vec[0] = (float)PyFloat_AsDouble(PyTuple_GetItem(value, 0));
				return 0;
				break;
			case SOCK_VECTOR:
				if(PyTuple_Size(value)==3) {
					self->stack[_idx]->vec[0] = (float)PyFloat_AsDouble(PyTuple_GetItem(value, 0));
					self->stack[_idx]->vec[1] = (float)PyFloat_AsDouble(PyTuple_GetItem(value, 1));
					self->stack[_idx]->vec[2] = (float)PyFloat_AsDouble(PyTuple_GetItem(value, 2));
				}
				return 0;
				break;
			case SOCK_RGBA:
				if(PyTuple_Size(value)==4) {
					self->stack[_idx]->vec[0] = (float)PyFloat_AsDouble(PyTuple_GetItem(value, 0));
					self->stack[_idx]->vec[1] = (float)PyFloat_AsDouble(PyTuple_GetItem(value, 1));
					self->stack[_idx]->vec[2] = (float)PyFloat_AsDouble(PyTuple_GetItem(value, 2));
					self->stack[_idx]->vec[3] = (float)PyFloat_AsDouble(PyTuple_GetItem(value, 3));
				}
				return 0;
				break;
			default:
				break;
		}
	}
	return 0;
}

/* write only */
static PyMappingMethods sockoutmap_as_mapping = {
	( inquiry ) sockoutmap_len,  /* mp_length */
	( binaryfunc ) 0, /* mp_subscript */
	( objobjargproc ) sockoutmap_assign_subscript /* mp_ass_subscript */
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
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	NULL,                       /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,      					/* PyNumberMethods *tp_as_number; */
	NULL,					    /* PySequenceMethods *tp_as_sequence; */
	&sockoutmap_as_mapping,      /* PyMappingMethods *tp_as_mapping; */

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
	BPy_SockMap *map= PyObject_NEW(BPy_SockMap, &SockInMap_Type);
	map->typeinfo= node->typeinfo;
	map->stack= stack;
	return map;
}

static PyObject *Node_GetInputMap(BPy_Node *self) {
	BPy_SockMap *inmap= Node_CreateInputMap(self->node, self->in);
	return (PyObject *)(inmap);
}

#define SURFACEVIEWVECTOR	0
#define VIEWNORMAL			1
#define SURFACENORMAL		2
#define GLOBALTEXTURE		3
#define TEXTURE				4
#define PIXEL				5
#define COLOR				6
#define SPECULAR			7
#define MIRROR				8
#define AMBIENT				9
#define AMBIENTFACTOR		10
#define EMITFACTOR			11
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

static PyObject *ShadeInput_getAttribute(BPy_ShadeInput *self, void *type) {
	PyObject *obj= NULL;
	if(self->shi) {
		switch((int)type) {
			case SURFACEVIEWVECTOR:
				obj= Py_BuildValue("(fff)", self->shi->view[0], self->shi->view[1], self->shi->view[2]);
				break;
			case VIEWNORMAL:
				obj= Py_BuildValue("(fff)", self->shi->vn[0], self->shi->vn[1], self->shi->vn[2]);
				break;
			case SURFACENORMAL:
				obj= Py_BuildValue("(fff)", self->shi->facenor[0], self->shi->facenor[1], self->shi->facenor[2]);
				break;
			case GLOBALTEXTURE:
				obj= Py_BuildValue("(fff)", self->shi->gl[0], self->shi->gl[1], self->shi->gl[2]);
				break;
			case TEXTURE:
				obj= Py_BuildValue("(fff)", self->shi->lo[0], self->shi->lo[1], self->shi->lo[2]);
				break;
			case PIXEL:
				obj= Py_BuildValue("(ii)", self->shi->xs, self->shi->ys);
				break;
			case COLOR:
				obj= Py_BuildValue("(fff)", self->shi->r, self->shi->g, self->shi->b);
				break;
			case SPECULAR:
				obj= Py_BuildValue("(fff)", self->shi->specr, self->shi->specg, self->shi->specb);
				break;
			case MIRROR:
				obj= Py_BuildValue("(fff)", self->shi->mirr, self->shi->mirg, self->shi->mirb);
				break;
			case AMBIENT:
				obj= Py_BuildValue("(fff)", self->shi->ambr, self->shi->ambg, self->shi->ambb);
				break;
			case AMBIENTFACTOR:
				obj= PyFloat_FromDouble((double)(self->shi->amb));
				break;
			case EMITFACTOR:
				obj= PyFloat_FromDouble((double)(self->shi->emit));
				break;
			case DISPLACE:
				obj= Py_BuildValue("(fff)", self->shi->displace[0], self->shi->displace[1], self->shi->displace[2]);
				break;
			case STRAND:
				obj= PyFloat_FromDouble((double)(self->shi->strand));
				break;
			case STRESS:
				obj= PyFloat_FromDouble((double)(self->shi->stress));
				break;
			case TANGENT:
				obj= Py_BuildValue("(fff)", self->shi->tang[0], self->shi->tang[1], self->shi->tang[2]);
				break;
			case SURFACE_D:
				obj= Py_BuildValue("(fff)(fff)", self->shi->dxco[0], self->shi->dxco[1], self->shi->dxco[2], self->shi->dyco[0], self->shi->dyco[1], self->shi->dyco[2]);
				break;
			case TEXTURE_D:
				obj= Py_BuildValue("(fff)(fff)", self->shi->dxlo[0], self->shi->dxlo[1], self->shi->dxlo[2], self->shi->dylo[0], self->shi->dylo[1], self->shi->dylo[2]);
				break;
			case GLOBALTEXTURE_D:
				obj= Py_BuildValue("(fff)(fff)", self->shi->dxgl[0], self->shi->dxgl[1], self->shi->dxgl[2], self->shi->dygl[0], self->shi->dygl[1], self->shi->dygl[2]);
				break;
			case REFLECTION_D:
				obj= Py_BuildValue("(fff)(fff)", self->shi->dxref[0], self->shi->dxref[1], self->shi->dxref[2], self->shi->dyref[0], self->shi->dyref[1], self->shi->dyref[2]);
				break;
			case NORMAL_D:
				obj= Py_BuildValue("(fff)(fff)", self->shi->dxno[0], self->shi->dxno[1], self->shi->dxno[2], self->shi->dyno[0], self->shi->dyno[1], self->shi->dyno[2]);
				break;
			case STICKY_D:
				obj= Py_BuildValue("(fff)(fff)", self->shi->dxsticky[0], self->shi->dxsticky[1], self->shi->dxsticky[2], self->shi->dysticky[0], self->shi->dysticky[1], self->shi->dysticky[2]);
				break;
			case REFRACT_D:
				obj= Py_BuildValue("(fff)(fff)", self->shi->dxrefract[0], self->shi->dxrefract[1], self->shi->dxrefract[2], self->shi->dyrefract[0], self->shi->dyrefract[1], self->shi->dyrefract[2]);
				break;
			case STRAND_D:
				obj= Py_BuildValue("(ff)", self->shi->dxstrand, self->shi->dystrand);
				break;
			default:
				break;
		}
	}

	if(!obj) {
		Py_RETURN_NONE;
	}
	return obj;
}

static BPy_SockMap *Node_CreateOutputMap(bNode *node, bNodeStack **stack) {
	BPy_SockMap *map = PyObject_NEW(BPy_SockMap, &SockOutMap_Type);
	map->typeinfo= node->typeinfo;
	map->stack= stack;
	return map;
}

static PyObject *Node_GetOutputMap(BPy_Node *self) {
	BPy_SockMap *outmap= Node_CreateOutputMap(self->node, self->out);
	return (PyObject *)outmap;
}

static PyObject *Node_GetShi(BPy_Node *self) {
	BPy_ShadeInput *shi= ShadeInput_CreatePyObject(self->shi);
	return (PyObject *)shi;

}

static PyObject *node_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyObject *self;
	assert(type!=NULL && type->tp_alloc!=NULL);
	self= type->tp_alloc(type, 1);
	return self;
}

static int node_init(BPy_Node *self, PyObject *args, PyObject *kwds)
{
	return 0;
}

static PyGetSetDef BPy_Node_getseters[] = {
	{"ins",
		(getter)Node_GetInputMap, (setter)NULL,
		"Get the ShadeInput mapping (dictionary)",
		NULL},
	{"outs",
		(getter)Node_GetOutputMap, (setter)NULL,
		"Get the ShadeInput mapping (dictionary)",
		NULL},
	{"shi",
		(getter)Node_GetShi, (setter)NULL,
		"Get the ShadeInput (ShadeInput)",
		NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static PyGetSetDef BPy_ShadeInput_getseters[] = {
	{"texture",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the current texture coordinate (tuple)",
	  (void*)TEXTURE},
	{"texture_global",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the current global texture coordinate (tuple)",
	  (void*)GLOBALTEXTURE},
	{"surface_normal",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the current surface normal (tuple)",
	  (void*)SURFACENORMAL},
	{"view_normal",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the current view normal (tuple)",
	  (void*)VIEWNORMAL},
	{"surface_view_vec",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the vector pointing to the viewpoint from the point being shaded (tuple)",
	  (void*)SURFACEVIEWVECTOR},
	{"pixel",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the x,y-coordinate for the pixel rendered (tuple)",
	  (void*)PIXEL},
	{"color",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the color for the point being shaded (tuple)",
	  (void*)COLOR},
	{"specular",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the specular color for the point being shaded (tuple)",
	  (void*)SPECULAR},
	{"mirror",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the mirror color for the point being shaded (tuple)",
	  (void*)MIRROR},
	{"ambient",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the ambient color for the point being shaded (tuple)",
	  (void*)AMBIENT},
	{"ambient_factor",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the ambient factor for the point being shaded (float)",
	  (void*)AMBIENTFACTOR},
	{"emit_factor",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the emit factor for the point being shaded (float)",
	  (void*)EMITFACTOR},
	{"displace",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the displace vector for the point being shaded (tuple)",
	  (void*)DISPLACE},
	{"strand",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the strand factor(float)",
	  (void*)STRAND},
	{"stress",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the stress factor(float)",
	  (void*)STRESS},
	{"tangent",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the tangent vector (tuple)",
	  (void*)TANGENT},
	{"surface_d",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the surface d (tuple of tuples)",
	  (void*)SURFACE_D},
	{"texture_d",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the texture d (tuple of tuples)",
	  (void*)TEXTURE_D},
	{"texture_global_d",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the global texture d (tuple of tuples)",
	  (void*)GLOBALTEXTURE_D},
	{"reflection_d",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the reflection d (tuple of tuples)",
	  (void*)REFLECTION_D},
	{"normal_d",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the normal d (tuple of tuples)",
	  (void*)NORMAL_D},
	{"sticky_d",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the sticky d (tuple of tuples)",
	  (void*)STICKY_D},
	{"refract_d",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the refract d (tuple of tuples)",
	  (void*)REFRACT_D},
	{"strand_d",
	  (getter)ShadeInput_getAttribute, (setter)NULL,
	  "Get the strand d (tuple)",
	  (void*)REFRACT_D},
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
	BPy_ShadeInput_getseters,        /* struct PyGetSetDef *tp_getset; */
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
	if( PyType_Ready( &OutputDefMap_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &InputDefMap_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &SockInMap_Type ) < 0 )
		return NULL;
	if( PyType_Ready( &SockOutMap_Type ) < 0 )
		return NULL;
	submodule = Py_InitModule3( "Blender.Node", NULL, "");

	PyModule_AddIntConstant(submodule, "VALUE", SOCK_VALUE);
	PyModule_AddIntConstant(submodule, "RGBA", SOCK_RGBA);
	PyModule_AddIntConstant(submodule, "VECTOR", SOCK_VECTOR);

	Py_INCREF(&Node_Type);
	PyModule_AddObject(submodule, "node", (PyObject *)&Node_Type);

	return submodule;

}

static int Node_compare(BPy_Node *a, BPy_Node *b)
{
	bNode *pa = a->node, *pb = b->node;
	return (pa==pb) ? 0 : -1;
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

	pynode->node= node;

	return pynode;
}

void InitNode(BPy_Node *self, bNode *node) {
	self->node= node;
}

bNode *Node_FromPyObject(PyObject *pyobj)
{
	return ((BPy_Node *)pyobj)->node;
}

void Node_SetStack(BPy_Node *self, bNodeStack **stack, int type)
{
	if(type==NODE_INPUTSTACK) {
		self->in= stack;
	} else if(type==NODE_OUTPUTSTACK) {
		self->out= stack;
	}
}

void Node_SetShi(BPy_Node *self, ShadeInput *shi)
{
	self->shi= shi;
}

/*********************/

static int ShadeInput_compare(BPy_ShadeInput *a, BPy_ShadeInput *b)
{
	ShadeInput *pa = a->shi, *pb = b->shi;
	return (pa==pb) ? 0 : -1;
}

static PyObject *ShadeInput_repr(BPy_ShadeInput *self)
{
	return PyString_FromFormat( "[ShadeInput @ \"%p\"]", self);
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
#endif

