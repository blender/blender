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

#include "bpy_operator_wrap.h"
#include "WM_api.h"
#include "WM_types.h"

#include "BLI_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "bpy_rna.h"

static void operator_properties_init(wmOperatorType *ot)
{
	PyObject *py_class = ot->ext.data;
	RNA_struct_blender_type_set(ot->ext.srna, ot);

	/* only call this so pyrna_deferred_register_class gives a useful error
	 * WM_operatortype_append_ptr will call RNA_def_struct_identifier
	 * later */
	RNA_def_struct_identifier(ot->srna, ot->idname);

	if (pyrna_deferred_register_class(ot->srna, py_class) != 0) {
		PyErr_Print(); /* failed to register operator props */
		PyErr_Clear();
	}
}

void operator_wrapper(wmOperatorType *ot, void *userdata)
{
	/* take care not to overwrite anything set in
	 * WM_operatortype_append_ptr before opfunc() is called */
	StructRNA *srna = ot->srna;
	*ot = *((wmOperatorType *)userdata);
	ot->srna = srna; /* restore */

	operator_properties_init(ot);

	/* XXX - not nice, set the first enum as searchable, should have a way for python to set */
	{
		PointerRNA ptr;
		PropertyRNA *prop;

		RNA_pointer_create(NULL, ot->srna, NULL, &ptr);
		prop = RNA_struct_find_property(&ptr, "type");
		if (prop) {
			ot->prop = prop;
		}
	}
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

	if (WM_operatortype_find(opname, TRUE) == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "Macro Define: '%s' is not a valid operator id",
		             opname);
		return NULL;
	}

	/* identifiers */
	srna = srna_from_self(macro, "Macro Define:");
	macroname = RNA_struct_identifier(srna);

	ot = WM_operatortype_find(macroname, TRUE);

	if (!ot) {
		PyErr_Format(PyExc_ValueError,
		             "Macro Define: '%s' is not a valid macro or hasn't been registered yet",
		             macroname);
		return NULL;
	}

	otmacro = WM_operatortype_macro_define(ot, opname);

	RNA_pointer_create(NULL, &RNA_OperatorMacro, otmacro, &ptr_otmacro);

	return pyrna_struct_CreatePyObject(&ptr_otmacro);
}

