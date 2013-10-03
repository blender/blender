/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_operator_wrap.c
 *  \ingroup pythonintern
 *
 * This file is so python can define operators that C can call into.
 * The generic callback functions for python operators are defines in
 * 'rna_wm.c', some calling into functions here to do python specific
 * functionality.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "bpy_rna.h"
#include "bpy_intern_string.h"
#include "bpy_operator_wrap.h"  /* own include */

static void operator_properties_init(wmOperatorType *ot)
{
	PyTypeObject *py_class = ot->ext.data;
	RNA_struct_blender_type_set(ot->ext.srna, ot);

	/* only call this so pyrna_deferred_register_class gives a useful error
	 * WM_operatortype_append_ptr will call RNA_def_struct_identifier
	 * later */
	RNA_def_struct_identifier(ot->srna, ot->idname);

	if (pyrna_deferred_register_class(ot->srna, py_class) != 0) {
		PyErr_Print(); /* failed to register operator props */
		PyErr_Clear();
	}

	/* set the default property: ot->prop */
	{
		/* picky developers will notice that 'bl_property' won't work with inheritance
		 * get direct from the dict to avoid raising a load of attribute errors (yes this isnt ideal) - campbell */
		PyObject *py_class_dict = py_class->tp_dict;
		PyObject *bl_property = PyDict_GetItem(py_class_dict, bpy_intern_str_bl_property);
		const char *prop_id;
		bool prop_raise_error;

		if (bl_property) {
			if (PyUnicode_Check(bl_property)) {
				/* since the property is explicitly given, raise an error if its not found */
				prop_id = _PyUnicode_AsString(bl_property);
				prop_raise_error = true;
			}
			else {
				PyErr_Format(PyExc_ValueError,
				             "%.200s.bl_property should be a string, not %.200s",
				             ot->idname, Py_TYPE(bl_property)->tp_name);

				/* this could be done cleaner, for now its OK */
				PyErr_Print();
				PyErr_Clear();

				prop_id = NULL;
				prop_raise_error = false;
			}
		}
		else {
			/* fallback to hard-coded string (pre 2.66, could be deprecated) */
			prop_id = "type";
			prop_raise_error = false;
		}

		if (prop_id) {
			PointerRNA ptr;
			PropertyRNA *prop;

			RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
			prop = RNA_struct_find_property(&ptr, prop_id);
			if (prop) {
				ot->prop = prop;
			}
			else {
				if (prop_raise_error) {
					PyErr_Format(PyExc_ValueError,
					             "%.200s.bl_property '%.200s' not found",
					             ot->idname, prop_id);

					/* this could be done cleaner, for now its OK */
					PyErr_Print();
					PyErr_Clear();
				}
			}
		}
	}
	/* end 'ot->prop' assignment */

}

void operator_wrapper(wmOperatorType *ot, void *userdata)
{
	/* take care not to overwrite anything set in
	 * WM_operatortype_append_ptr before opfunc() is called */
	StructRNA *srna = ot->srna;
	*ot = *((wmOperatorType *)userdata);
	ot->srna = srna; /* restore */

	/* Use i18n context from ext.srna if possible (py operators). */
	if (ot->ext.srna) {
		RNA_def_struct_translation_context(ot->srna, RNA_struct_translation_context(ot->ext.srna));
	}

	operator_properties_init(ot);
}

void macro_wrapper(wmOperatorType *ot, void *userdata)
{
	wmOperatorType *data = (wmOperatorType *)userdata;

	/* only copy a couple of things, the rest is set by the macro registration */
	ot->name = data->name;
	ot->idname = data->idname;
	ot->description = data->description;
	ot->flag |= data->flag; /* append flags to the one set by registration */
	ot->pyop_poll = data->pyop_poll;
	ot->ui = data->ui;
	ot->ext = data->ext;

	/* Use i18n context from ext.srna if possible (py operators). */
	if (ot->ext.srna) {
		RNA_def_struct_translation_context(ot->srna, RNA_struct_translation_context(ot->ext.srna));
	}

	operator_properties_init(ot);
}

PyObject *PYOP_wrap_macro_define(PyObject *UNUSED(self), PyObject *args)
{
	wmOperatorType *ot;
	wmOperatorTypeMacro *otmacro;
	PyObject *macro;
	PointerRNA ptr_otmacro;
	StructRNA *srna;

	char *opname;
	const char *macroname;

	if (!PyArg_ParseTuple(args, "Os:_bpy.ops.macro_define", &macro, &opname))
		return NULL;

	if (WM_operatortype_find(opname, true) == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "Macro Define: '%s' is not a valid operator id",
		             opname);
		return NULL;
	}

	/* identifiers */
	srna = pyrna_struct_as_srna((PyObject *)macro, false, "Macro Define:");
	if (srna == NULL) {
		return NULL;
	}

	macroname = RNA_struct_identifier(srna);
	ot = WM_operatortype_find(macroname, true);

	if (!ot) {
		PyErr_Format(PyExc_ValueError,
		             "Macro Define: '%s' is not a valid macro",
		             macroname);
		return NULL;
	}

	otmacro = WM_operatortype_macro_define(ot, opname);

	RNA_pointer_create(NULL, &RNA_OperatorMacro, otmacro, &ptr_otmacro);

	return pyrna_struct_CreatePyObject(&ptr_otmacro);
}

