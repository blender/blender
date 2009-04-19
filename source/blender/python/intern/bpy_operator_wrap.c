
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


#include "bpy_operator_wrap.h"
#include "BLI_listbase.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "DNA_windowmanager_types.h"
#include "MEM_guardedalloc.h"
#include "WM_api.h"
#include "WM_types.h"
#include "ED_screen.h"

#include "RNA_define.h"

#include "bpy_rna.h"
#include "bpy_compat.h"
#include "bpy_util.h"

#define PYOP_ATTR_PROP			"__props__"
#define PYOP_ATTR_UINAME		"__label__"
#define PYOP_ATTR_IDNAME		"__name__"	/* use pythons class name */
#define PYOP_ATTR_DESCRIPTION	"__doc__"	/* use pythons docstring */

static PyObject *pyop_dict_from_event(wmEvent *event)
{
	PyObject *dict= PyDict_New();
	PyObject *item;
	char *cstring, ascii[2];

	/* type */
	item= PyUnicode_FromString(WM_key_event_string(event->type));
	PyDict_SetItemString(dict, "type", item);	Py_DECREF(item);

	/* val */
	switch(event->val) {
	case KM_ANY:
		cstring = "ANY";
		break;
	case KM_RELEASE:
		cstring = "RELEASE";
		break;
	case KM_PRESS:
		cstring = "PRESS";
		break;
	default:
		cstring = "UNKNOWN";
		break;
	}

	item= PyUnicode_FromString(cstring);
	PyDict_SetItemString(dict, "val", item);	Py_DECREF(item);

	/* x, y (mouse) */
	item= PyLong_FromLong(event->x);
	PyDict_SetItemString(dict, "x", item);		Py_DECREF(item);

	item= PyLong_FromLong(event->y);
	PyDict_SetItemString(dict, "y", item);		Py_DECREF(item);

	item= PyLong_FromLong(event->prevx);
	PyDict_SetItemString(dict, "prevx", item);	Py_DECREF(item);

	item= PyLong_FromLong(event->prevy);
	PyDict_SetItemString(dict, "prevy", item);	Py_DECREF(item);

	/* ascii */
	ascii[0]= event->ascii;
	ascii[1]= '\0';
	item= PyUnicode_FromString(ascii);
	PyDict_SetItemString(dict, "ascii", item);	Py_DECREF(item);

	/* modifier keys */
	item= PyLong_FromLong(event->shift);
	PyDict_SetItemString(dict, "shift", item);	Py_DECREF(item);

	item= PyLong_FromLong(event->ctrl);
	PyDict_SetItemString(dict, "ctrl", item);	Py_DECREF(item);

	item= PyLong_FromLong(event->alt);
	PyDict_SetItemString(dict, "alt", item);	Py_DECREF(item);

	item= PyLong_FromLong(event->oskey);
	PyDict_SetItemString(dict, "oskey", item);	Py_DECREF(item);



	/* modifier */
#if 0
	item= PyTuple_New(0);
	if(event->keymodifier & KM_SHIFT) {
		_PyTuple_Resize(&item, size+1);
		PyTuple_SET_ITEM(item, size, _PyUnicode_AsString("SHIFT"));
		size++;
	}
	if(event->keymodifier & KM_CTRL) {
		_PyTuple_Resize(&item, size+1);
		PyTuple_SET_ITEM(item, size, _PyUnicode_AsString("CTRL"));
		size++;
	}
	if(event->keymodifier & KM_ALT) {
		_PyTuple_Resize(&item, size+1);
		PyTuple_SET_ITEM(item, size, _PyUnicode_AsString("ALT"));
		size++;
	}
	if(event->keymodifier & KM_OSKEY) {
		_PyTuple_Resize(&item, size+1);
		PyTuple_SET_ITEM(item, size, _PyUnicode_AsString("OSKEY"));
		size++;
	}
	PyDict_SetItemString(dict, "keymodifier", item);	Py_DECREF(item);
#endif

	return dict;
}

/* TODO - a whole traceback would be ideal */
static void pyop_error_report(ReportList *reports)
{
	PyObject *exception, *v, *tb;
	PyErr_Fetch(&exception, &v, &tb);
	if (exception == NULL)
		return;
	/* Now we know v != NULL too */
	BKE_report(reports, RPT_ERROR, _PyUnicode_AsString(v));
	
	PyErr_Print();
}

static struct BPY_flag_def pyop_ret_flags[] = {
	{"RUNNING_MODAL", OPERATOR_RUNNING_MODAL},
	{"CANCELLED", OPERATOR_CANCELLED},
	{"FINISHED", OPERATOR_FINISHED},
	{"PASS_THROUGH", OPERATOR_PASS_THROUGH},
	{NULL, 0}
};

/* This invoke function can take events and
 *
 * It is up to the pyot->py_invoke() python func to run pyot->py_exec()
 * the invoke function gets the keyword props as a dict, but can parse them
 * to py_exec like this...
 *
 * def op_exec(x=-1, y=-1, text=""):
 *     ...
 *
 * def op_invoke(event, prop_defs):
 *     prop_defs['x'] = event['x']
 *     ...
 *     op_exec(**prop_defs)
 *
 * when there is no invoke function, C calls exec and sets the props.
 * python class instance is stored in op->customdata so exec() can access
 */


#define PYOP_EXEC 1
#define PYOP_INVOKE 2
#define PYOP_POLL 3
	
extern void BPY_update_modules( void ); //XXX temp solution

static int PYTHON_OT_generic(int mode, bContext *C, wmOperator *op, wmEvent *event)
{
	PyObject *py_class = op->type->pyop_data;
	PyObject *args;
	PyObject *ret= NULL, *py_class_instance, *item;
	int ret_flag= (mode==PYOP_POLL ? 0:OPERATOR_CANCELLED);

	PyGILState_STATE gilstate = PyGILState_Ensure();
	
	BPY_update_modules(); // XXX - the RNA pointers can change so update before running, would like a nicer solutuon for this.

	args = PyTuple_New(1);
	PyTuple_SET_ITEM(args, 0, PyObject_GetAttrString(py_class, "__rna__")); // need to use an rna instance as the first arg
	py_class_instance = PyObject_Call(py_class, args, NULL);
	Py_DECREF(args);
	
	if (py_class_instance) { /* Initializing the class worked, now run its invoke function */
		
		
		/* Assign instance attributes from operator properties */
		{
			PropertyRNA *prop, *iterprop;
			CollectionPropertyIterator iter;
			const char *arg_name;

			iterprop= RNA_struct_iterator_property(op->ptr->type);
			RNA_property_collection_begin(op->ptr, iterprop, &iter);

			for(; iter.valid; RNA_property_collection_next(&iter)) {
				prop= iter.ptr.data;
				arg_name= RNA_property_identifier(prop);

				if (strcmp(arg_name, "rna_type")==0) continue;

				item = pyrna_prop_to_py(op->ptr, prop);
				PyObject_SetAttrString(py_class_instance, arg_name, item);
				Py_DECREF(item);
			}

			RNA_property_collection_end(&iter);
		}
		
		
		if (mode==PYOP_INVOKE) {
			item= PyObject_GetAttrString(py_class, "invoke");
			args = PyTuple_New(2);
			PyTuple_SET_ITEM(args, 1, pyop_dict_from_event(event));
		}
		else if (mode==PYOP_EXEC) {
			item= PyObject_GetAttrString(py_class, "exec");
			args = PyTuple_New(1);
		}
		else if (mode==PYOP_POLL) {
			item= PyObject_GetAttrString(py_class, "poll");
			args = PyTuple_New(2);
			//XXX  Todo - wrap context in a useful way, None for now.
			PyTuple_SET_ITEM(args, 1, Py_None);
		}
		PyTuple_SET_ITEM(args, 0, py_class_instance);
	
		ret = PyObject_Call(item, args, NULL);
		
		Py_DECREF(args);
		Py_DECREF(item);
	}
	
	if (ret == NULL) { /* covers py_class_instance failing too */
		pyop_error_report(op->reports);
	}
	else {
		if (mode==PYOP_POLL) {
			if (PyBool_Check(ret) == 0) {
				PyErr_SetString(PyExc_ValueError, "Python poll function return value ");
				pyop_error_report(op->reports);
			}
			else {
				ret_flag= ret==Py_True ? 1:0;
			}
			
		} else if (BPY_flag_from_seq(pyop_ret_flags, ret, &ret_flag) == -1) {
			 /* the returned value could not be converted into a flag */
			pyop_error_report(op->reports);
			
		}
		/* there is no need to copy the py keyword dict modified by
		 * pyot->py_invoke(), back to the operator props since they are just
		 * thrown away anyway
		 *
		 * If we ever want to do this and use the props again,
		 * it can be done with - PYOP_props_from_dict(op->ptr, kw)
		 */
		
		Py_DECREF(ret);
	}

	PyGILState_Release(gilstate);

	return ret_flag;
}

static int PYTHON_OT_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return PYTHON_OT_generic(PYOP_INVOKE, C, op, event);	
}

static int PYTHON_OT_exec(bContext *C, wmOperator *op)
{
	return PYTHON_OT_generic(PYOP_EXEC, C, op, NULL);
}

static int PYTHON_OT_poll(bContext *C)
{
	// XXX TODO - no way to get the operator type (and therefor class) from the poll function.
	//return PYTHON_OT_generic(PYOP_POLL, C, NULL, NULL);
	return 1;
}

void PYTHON_OT_wrapper(wmOperatorType *ot, void *userdata)
{
	PyObject *py_class = (PyObject *)userdata;
	PyObject *props, *item;

	/* identifiers */
	item= PyObject_GetAttrString(py_class, PYOP_ATTR_IDNAME);
	Py_DECREF(item);
	ot->idname= _PyUnicode_AsString(item);
	

	item= PyObject_GetAttrString(py_class, PYOP_ATTR_UINAME);
	if (item) {
		Py_DECREF(item);
		ot->name= _PyUnicode_AsString(item);
	}
	else {
		ot->name= ot->idname;
		PyErr_Clear();
	}

	item= PyObject_GetAttrString(py_class, PYOP_ATTR_DESCRIPTION);
	Py_DECREF(item);
	ot->description= (item && PyUnicode_Check(item)) ? _PyUnicode_AsString(item):"";
	
	/* api callbacks, detailed checks dont on adding */ 
	if (PyObject_HasAttrString(py_class, "invoke"))
		ot->invoke= PYTHON_OT_invoke;
	if (PyObject_HasAttrString(py_class, "exec"))
		ot->exec= PYTHON_OT_exec;
	if (PyObject_HasAttrString(py_class, "poll"))
		ot->poll= PYTHON_OT_poll;
	
	ot->pyop_data= userdata;
	
	props= PyObject_GetAttrString(py_class, PYOP_ATTR_PROP);
	
	if (props) {
		PyObject *dummy_args = PyTuple_New(0);
		int i;
		
		Py_DECREF(props);

		for(i=0; i<PyList_Size(props); i++) {
			PyObject *py_func_ptr, *py_kw, *py_srna_cobject, *py_ret;
			item = PyList_GET_ITEM(props, i);
			
			if (PyArg_ParseTuple(item, "O!O!", &PyCObject_Type, &py_func_ptr, &PyDict_Type, &py_kw)) {
				
				PyObject *(*pyfunc)(PyObject *, PyObject *, PyObject *);
				pyfunc = PyCObject_AsVoidPtr(py_func_ptr);
				py_srna_cobject = PyCObject_FromVoidPtr(ot->srna, NULL);
				
				py_ret = pyfunc(py_srna_cobject, dummy_args, py_kw);
				if (py_ret) {
					Py_DECREF(py_ret);
				} else {
					PyErr_Print();
					PyErr_Clear();
				}
				Py_DECREF(py_srna_cobject);
				
			} else {
				/* cant return NULL from here */ // XXX a bit ugly
				PyErr_Print();
				PyErr_Clear();
			}
			
			// expect a tuple with a CObject and a dict
		}
		Py_DECREF(dummy_args);
	} else {
		PyErr_Clear();
	}
}


/* pyOperators - Operators defined IN Python */
PyObject *PYOP_wrap_add(PyObject *self, PyObject *py_class)
{	
	PyObject *base_class, *item;
	
	
	char *idname= NULL;
	int i;

	static struct BPY_class_attr_check pyop_class_attr_values[]= {
		{PYOP_ATTR_IDNAME,		's', 0,	0},
		{PYOP_ATTR_UINAME,		's', 0,	BPY_CLASS_ATTR_OPTIONAL},
		{PYOP_ATTR_PROP,		'l', 0,	BPY_CLASS_ATTR_OPTIONAL},
		{PYOP_ATTR_DESCRIPTION,	's', 0,	BPY_CLASS_ATTR_NONE_OK},
		{"exec",	'f', 1,	BPY_CLASS_ATTR_OPTIONAL},
		{"invoke",	'f', 2,	BPY_CLASS_ATTR_OPTIONAL},
		{"poll",	'f', 2,	BPY_CLASS_ATTR_OPTIONAL},
		{NULL, 0, 0, 0}
	};

	// in python would be...
	//PyObject *optype = PyObject_GetAttrString(PyObject_GetAttrString(PyDict_GetItemString(PyEval_GetGlobals(), "bpy"), "types"), "Operator");
	base_class = PyObject_GetAttrStringArgs(PyDict_GetItemString(PyEval_GetGlobals(), "bpy"), 2, "types", "Operator");
	Py_DECREF(base_class);

	if(BPY_class_validate("Operator", py_class, base_class, pyop_class_attr_values, NULL) < 0) {
		return NULL; /* BPY_class_validate sets the error */
	}

	/* class name is used for operator ID - this can be changed later if we want */
	item= PyObject_GetAttrString(py_class, PYOP_ATTR_IDNAME);
	Py_DECREF(item);
	idname =  _PyUnicode_AsString(item);
	
	if (WM_operatortype_find(idname)) {
		PyErr_Format( PyExc_AttributeError, "Operator alredy exists with this name \"%s\"", idname);
		return NULL;
	}
	
	/* If we have properties set, check its a list of dicts */
	item= PyObject_GetAttrString(py_class, PYOP_ATTR_PROP);
	if (item) {
		Py_DECREF(item);
		for(i=0; i<PyList_Size(item); i++) {
			PyObject *py_args = PyList_GET_ITEM(item, i);
			PyObject *py_func_ptr, *py_kw; /* place holders */
			
			if (!PyArg_ParseTuple(py_args, "O!O!", &PyCObject_Type, &py_func_ptr, &PyDict_Type, &py_kw)) {
				PyErr_Format(PyExc_ValueError, "Cant register operator class - %s.properties must contain values from FloatProperty", idname);
				return NULL;				
			}
		}
	}
	else {
		PyErr_Clear();
	}
	
	Py_INCREF(py_class);
	WM_operatortype_append_ptr(PYTHON_OT_wrapper, py_class);

	Py_RETURN_NONE;
}

PyObject *PYOP_wrap_remove(PyObject *self, PyObject *value)
{
	PyObject *py_class;
	char *idname= NULL;
	wmOperatorType *ot;
	

	if (PyUnicode_Check(value))
		idname = _PyUnicode_AsString(value);
	else if (PyCFunction_Check(value)) {
		PyObject *cfunc_self = PyCFunction_GetSelf(value);
		if (cfunc_self)
			idname = _PyUnicode_AsString(cfunc_self);
	}
	
	if (idname==NULL) {
		PyErr_SetString( PyExc_ValueError, "Expected the operator name as a string or the operator function");
		return NULL;
	}

	if (!(ot= WM_operatortype_find(idname))) {
		PyErr_Format( PyExc_AttributeError, "Operator \"%s\" does not exists, cant remove", idname);
		return NULL;
	}
	
	if (!(py_class= (PyObject *)ot->pyop_data)) {
		PyErr_Format( PyExc_AttributeError, "Operator \"%s\" was not created by python", idname);
		return NULL;
	}
	
	Py_XDECREF(py_class);

	WM_operatortype_remove(idname);

	Py_RETURN_NONE;
}



