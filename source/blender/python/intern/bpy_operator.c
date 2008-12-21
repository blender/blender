
/**
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "bpy_operator.h"
#include "bpy_compat.h"
#include "bpy_idprop.h"

//#include "blendef.h"
#include "BLI_dynstr.h"
#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"
#include "BKE_idprop.h"

extern ListBase global_ops; /* evil, temp use */

/* floats bigger then this are displayed as inf in the docstrings */
#define MAXFLOAT_DOC 10000000

static int pyop_func_compare( BPy_OperatorFunc * a, BPy_OperatorFunc * b )
{
	return (strcmp(a->name, b->name)==0) ? 0 : -1;
}

/*----------------------repr--------------------------------------------*/
static PyObject *pyop_base_repr( BPy_OperatorBase * self )
{
	return PyUnicode_FromFormat( "[BPy_OperatorBase]");
}

static PyObject *pyop_func_repr( BPy_OperatorFunc * self )
{
	return PyUnicode_FromFormat( "[BPy_OperatorFunc \"%s\"]", self->name);
}

//---------------getattr--------------------------------------------
static PyObject *pyop_base_getattro( BPy_OperatorBase * self, PyObject *pyname )
{
	char *name = _PyUnicode_AsString(pyname);
	PyObject *ret;
	wmOperatorType *ot;

	if( strcmp( name, "__members__" ) == 0 ) {
		PyObject *item;

		ret = PyList_New(0);

		for(ot= WM_operatortype_first(); ot; ot= ot->next) {
			item = PyUnicode_FromString( ot->idname );
			PyList_Append(ret, item);
			Py_DECREF(item);
		}
	} else {
		ot = WM_operatortype_find(name);

		if (ot) {
			return pyop_func_CreatePyObject(self->C, name);
		}
		else {
			PyErr_Format( PyExc_AttributeError, "Operator \"%s\" not found", name);
		}
	}
	
	return ret;
}

static PyObject * pyop_func_call(BPy_OperatorFunc * self, PyObject *args, PyObject *kw)
{
	IDProperty *properties = NULL;

	if (PyTuple_Size(args)) {
		PyErr_SetString( PyExc_AttributeError, "All operator args must be keywords");
		return NULL;
	}

	if (kw && PyDict_Size(kw) > 0) {
		IDPropertyTemplate val;
		val.i = 0; /* silence MSVC warning about uninitialized var when debugging */

		properties= IDP_New(IDP_GROUP, val, "property");
		BPy_IDGroup_Update(properties, kw);

		if (PyErr_Occurred()) {
			IDP_FreeProperty(properties);
			MEM_freeN(properties);
			return NULL;
		}
	}
	
	WM_operator_call(self->C, self->name, WM_OP_DEFAULT, properties);

	if (properties) {
		IDP_FreeProperty(properties);
		MEM_freeN(properties);
	}
	Py_RETURN_NONE;
}

/*-----------------------BPy_OperatorBase method def------------------------------*/
PyTypeObject pyop_base_Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif

	"Operator",		/* tp_name */
	sizeof( BPy_OperatorBase ),			/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,						/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,						/* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */
	( reprfunc ) pyop_base_repr,	/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,						/* PySequenceMethods *tp_as_sequence; */
	NULL,						/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	( getattrofunc )pyop_base_getattro, /*PyObject_GenericGetAttr - MINGW Complains, assign later */	/* getattrofunc tp_getattro; */
	NULL, /*PyObject_GenericSetAttr - MINGW Complains, assign later */	/* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,						/*  char *tp_doc;  Documentation string */
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
	NULL,						/* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,						/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,      					/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,						/* newfunc tp_new; */
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

/*-----------------------BPy_OperatorBase method def------------------------------*/
PyTypeObject pyop_func_Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif

	"OperatorFunc",		/* tp_name */
	sizeof( BPy_OperatorFunc ),			/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,						/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,						/* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	( cmpfunc ) pyop_func_compare,	/* tp_compare */
	( reprfunc ) pyop_func_repr,	/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,						/* PySequenceMethods *tp_as_sequence; */
	NULL,						/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	(ternaryfunc)pyop_func_call,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL, /*PyObject_GenericGetAttr - MINGW Complains, assign later */	/* getattrofunc tp_getattro; */
	NULL, /*PyObject_GenericSetAttr - MINGW Complains, assign later */	/* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,						/*  char *tp_doc;  Documentation string */
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
	NULL,						/* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,						/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,      					/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,						/* newfunc tp_new; */
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

PyObject *pyop_base_CreatePyObject( bContext *C )
{
	BPy_OperatorBase *pyop;

	pyop = ( BPy_OperatorBase * ) PyObject_NEW( BPy_OperatorBase, &pyop_base_Type );

	if( !pyop ) {
		PyErr_SetString( PyExc_MemoryError, "couldn't create BPy_OperatorBase object" );
		return NULL;
	}

	pyop->C = C; /* TODO - copy this? */

	return ( PyObject * ) pyop;
}

PyObject *pyop_func_CreatePyObject( bContext *C, char *name )
{
	BPy_OperatorFunc *pyop;

	pyop = ( BPy_OperatorFunc * ) PyObject_NEW( BPy_OperatorFunc, &pyop_func_Type );

	if( !pyop ) {
		PyErr_SetString( PyExc_MemoryError, "couldn't create BPy_OperatorFunc object" );
		return NULL;
	}

	strcpy(pyop->name, name);
	pyop->C= C; /* TODO - how should contexts be dealt with? */

	return ( PyObject * ) pyop;
}

PyObject *BPY_operator_module( bContext *C )
{
	if( PyType_Ready( &pyop_base_Type ) < 0 )
		return NULL;

	if( PyType_Ready( &pyop_func_Type ) < 0 )
		return NULL;

	//submodule = Py_InitModule3( "operator", M_rna_methods, "rna module" );
	return pyop_base_CreatePyObject(C);
}



