
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
#include "bpy_util.h"

#include "../generic/bpy_internal_import.h" // our own imports

#define PYOP_ATTR_UINAME		"bl_label"
#define PYOP_ATTR_IDNAME		"bl_idname"		/* the name given by python */
#define PYOP_ATTR_IDNAME_BL	"_bl_idname"	/* our own name converted into blender syntax, users wont see this */
#define PYOP_ATTR_DESCRIPTION	"__doc__"		/* use pythons docstring */
#define PYOP_ATTR_REGISTER		"bl_register"	/* True/False. if this python operator should be registered */
#define PYOP_ATTR_UNDO			"bl_undo"		/* True/False. if this python operator should be undone */

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

static int PYTHON_OT_generic(int mode, bContext *C, wmOperatorType *ot, wmOperator *op, wmEvent *event)
{
	PyObject *py_class = ot->pyop_data;
	PyObject *args;
	PyObject *ret= NULL, *py_class_instance, *item= NULL;
	int ret_flag= (mode==PYOP_POLL ? 0:OPERATOR_CANCELLED);
	PointerRNA ptr_context;
	PointerRNA ptr_operator;
	PointerRNA ptr_event;

	PyGILState_STATE gilstate;

	bpy_context_set(C, &gilstate);

	args = PyTuple_New(1);

	/* poll has no 'op', should be ok still */
	/* use an rna instance as the first arg */
	RNA_pointer_create(NULL, &RNA_Operator, op, &ptr_operator);
	PyTuple_SET_ITEM(args, 0, pyrna_struct_CreatePyObject(&ptr_operator));

	py_class_instance = PyObject_Call(py_class, args, NULL);
	Py_DECREF(args);
	
	if (py_class_instance) { /* Initializing the class worked, now run its invoke function */
		PyObject *class_dict= PyObject_GetAttrString(py_class_instance, "__dict__");
		
		/* Assign instance attributes from operator properties */
		if(op) {
			const char *arg_name;

			RNA_STRUCT_BEGIN(op->ptr, prop) {
				arg_name= RNA_property_identifier(prop);

				if (strcmp(arg_name, "rna_type")==0) continue;

				item = pyrna_prop_to_py(op->ptr, prop);
				PyDict_SetItemString(class_dict, arg_name, item);
				Py_DECREF(item);
			}
			RNA_STRUCT_END;
		}

		RNA_pointer_create(NULL, &RNA_Context, C, &ptr_context);
		
		if (mode==PYOP_INVOKE) {
			item= PyObject_GetAttrString(py_class, "invoke");
			args = PyTuple_New(3);
			
			RNA_pointer_create(NULL, &RNA_Event, event, &ptr_event);

			// PyTuple_SET_ITEM "steals" object reference, it is
			// an object passed shouldn't be DECREF'ed
			PyTuple_SET_ITEM(args, 1, pyrna_struct_CreatePyObject(&ptr_context));
			PyTuple_SET_ITEM(args, 2, pyrna_struct_CreatePyObject(&ptr_event));
		}
		else if (mode==PYOP_EXEC) {
			item= PyObject_GetAttrString(py_class, "execute");
			args = PyTuple_New(2);
			
			PyTuple_SET_ITEM(args, 1, pyrna_struct_CreatePyObject(&ptr_context));
		}
		else if (mode==PYOP_POLL) {
			item= PyObject_GetAttrString(py_class, "poll");
			args = PyTuple_New(2);
			PyTuple_SET_ITEM(args, 1, pyrna_struct_CreatePyObject(&ptr_context));
		}
		PyTuple_SET_ITEM(args, 0, py_class_instance);
	
		ret = PyObject_Call(item, args, NULL);
		
		Py_DECREF(args);
		Py_DECREF(item);
		Py_DECREF(class_dict);
	}
	else {
		PyErr_Print();
		PyErr_Clear();
	}
	
	if (ret == NULL) { /* covers py_class_instance failing too */
		if(op)
			BPy_errors_to_report(op->reports);
	}
	else {
		if (mode==PYOP_POLL) {
			if (PyBool_Check(ret) == 0) {
				PyErr_SetString(PyExc_ValueError, "Python poll function return value ");
				if(op)
					BPy_errors_to_report(op->reports);
			}
			else {
				ret_flag= ret==Py_True ? 1:0;
			}
			
		} else if (BPY_flag_from_seq(pyop_ret_flags, ret, &ret_flag) == -1) {
			/* the returned value could not be converted into a flag */
			if(op) {
				fprintf(stderr, "error using return value from \"%s\"\n", op->idname); // for some reason the error raised doesnt include file:line... this helps
				BPy_errors_to_report(op->reports);
			}

			ret_flag = OPERATOR_CANCELLED;
		}
		/* there is no need to copy the py keyword dict modified by
		 * pyot->py_invoke(), back to the operator props since they are just
		 * thrown away anyway
		 *
		 * If we ever want to do this and use the props again,
		 * it can be done with - pyrna_pydict_to_props(op->ptr, kw, "")
		 */
		
		Py_DECREF(ret);
	}

#if 0 /* only for testing */

	/* print operator return value */
	if (mode != PYOP_POLL) {
		char flag_str[100];
		char class_name[100];
		BPY_flag_def *flag_def = pyop_ret_flags;

		strcpy(flag_str, "");
		
		while(flag_def->name) {
			if (ret_flag & flag_def->flag) {
				if(flag_str[1])
					sprintf(flag_str, "%s | %s", flag_str, flag_def->name);
				else
					strcpy(flag_str, flag_def->name);
			}
			flag_def++;
		}

		/* get class name */
		item= PyObject_GetAttrString(py_class, PYOP_ATTR_IDNAME);
		strcpy(class_name, _PyUnicode_AsString(item));
		Py_DECREF(item);

		fprintf(stderr, "%s's %s returned %s\n", class_name, mode == PYOP_EXEC ? "execute" : "invoke", flag_str);
	}
#endif

	bpy_context_clear(C, &gilstate);

	return ret_flag;
}

static int PYTHON_OT_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return PYTHON_OT_generic(PYOP_INVOKE, C, op->type, op, event);	
}

static int PYTHON_OT_execute(bContext *C, wmOperator *op)
{
	return PYTHON_OT_generic(PYOP_EXEC, C, op->type, op, NULL);
}

static int PYTHON_OT_poll(bContext *C, wmOperatorType *ot)
{
	return PYTHON_OT_generic(PYOP_POLL, C, ot, NULL, NULL);
}

void PYTHON_OT_wrapper(wmOperatorType *ot, void *userdata)
{
	PyObject *py_class = (PyObject *)userdata;
	PyObject *item;

	/* identifiers */
	item= PyObject_GetAttrString(py_class, PYOP_ATTR_IDNAME_BL);
	ot->idname= _PyUnicode_AsString(item);
	Py_DECREF(item);

	item= PyObject_GetAttrString(py_class, PYOP_ATTR_UINAME);
	if (item) {
		ot->name= _PyUnicode_AsString(item);
		Py_DECREF(item);
	}
	else {
		ot->name= ot->idname;
		PyErr_Clear();
	}

	item= PyObject_GetAttrString(py_class, PYOP_ATTR_DESCRIPTION);
	ot->description= (item && PyUnicode_Check(item)) ? _PyUnicode_AsString(item):"undocumented python operator";
	Py_XDECREF(item);
	
	/* api callbacks, detailed checks dont on adding */ 
	if (PyObject_HasAttrString(py_class, "invoke"))
		ot->invoke= PYTHON_OT_invoke;
	//else
	//	ot->invoke= WM_operator_props_popup; /* could have an option for standard invokes */

	if (PyObject_HasAttrString(py_class, "execute"))
		ot->exec= PYTHON_OT_execute;
	if (PyObject_HasAttrString(py_class, "poll"))
		ot->pyop_poll= PYTHON_OT_poll;
	
	ot->pyop_data= userdata;
	
	/* flags */
	ot->flag= 0;

	item= PyObject_GetAttrString(py_class, PYOP_ATTR_REGISTER);
	if (item) {
		ot->flag |= PyObject_IsTrue(item)!=0 ? OPTYPE_REGISTER:0;
		Py_DECREF(item);
	}
	else {
		PyErr_Clear();
	}
	item= PyObject_GetAttrString(py_class, PYOP_ATTR_UNDO);
	if (item) {
		ot->flag |= PyObject_IsTrue(item)!=0 ? OPTYPE_UNDO:0;
		Py_DECREF(item);
	}
	else {
		PyErr_Clear();
	}

	/* Can't use this because it returns a dict proxy
	 *
	 * item= PyObject_GetAttrString(py_class, "__dict__");
	 */
	item= ((PyTypeObject*)py_class)->tp_dict;
	if(item) {
		if(pyrna_deferred_register_props(ot->srna, item)!=0) {
			PyErr_Print();
			PyErr_Clear();
		}
	}
	else {
		PyErr_Clear();
	}
}


/* pyOperators - Operators defined IN Python */
PyObject *PYOP_wrap_add(PyObject *self, PyObject *py_class)
{	
	PyObject *base_class, *item;
	wmOperatorType *ot;
	
	
	char *idname= NULL;
	char idname_bl[OP_MAX_TYPENAME]; /* converted to blender syntax */

	static struct BPY_class_attr_check pyop_class_attr_values[]= {
		{PYOP_ATTR_IDNAME,		's', -1, OP_MAX_TYPENAME-3,	0}, /* -3 because a.b -> A_OT_b */
		{PYOP_ATTR_UINAME,		's', -1,-1,	BPY_CLASS_ATTR_OPTIONAL},
		{PYOP_ATTR_DESCRIPTION,	's', -1,-1,	BPY_CLASS_ATTR_NONE_OK},
		{"execute",				'f', 2,	-1, BPY_CLASS_ATTR_OPTIONAL},
		{"invoke",				'f', 3,	-1, BPY_CLASS_ATTR_OPTIONAL},
		{"poll",				'f', 2,	-1, BPY_CLASS_ATTR_OPTIONAL},
		{NULL, 0, 0, 0}
	};

	// in python would be...
	//PyObject *optype = PyObject_GetAttrString(PyObject_GetAttrString(PyDict_GetItemString(PyEval_GetGlobals(), "bpy"), "types"), "Operator");
	base_class = PyObject_GetAttrStringArgs(PyDict_GetItemString(PyEval_GetGlobals(), "bpy"), 2, "types", "Operator");

	if(BPY_class_validate("Operator", py_class, base_class, pyop_class_attr_values, NULL) < 0) {
		return NULL; /* BPY_class_validate sets the error */
	}
	Py_DECREF(base_class);

	/* class name is used for operator ID - this can be changed later if we want */
	item= PyObject_GetAttrString(py_class, PYOP_ATTR_IDNAME);
	idname =  _PyUnicode_AsString(item);


	/* annoying conversion! */
	WM_operator_bl_idname(idname_bl, idname);
	Py_DECREF(item);

	item= PyUnicode_FromString(idname_bl);
	PyObject_SetAttrString(py_class, PYOP_ATTR_IDNAME_BL, item);
	idname =  _PyUnicode_AsString(item);
	Py_DECREF(item);
	/* end annoying conversion! */

	
	/* remove if it already exists */
	if ((ot=WM_operatortype_exists(idname))) {
		if(ot->pyop_data) {
			Py_XDECREF((PyObject*)ot->pyop_data);
		}
		WM_operatortype_remove(idname);
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

	if (!(ot= WM_operatortype_exists(idname))) {
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



