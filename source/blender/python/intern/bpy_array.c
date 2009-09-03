/**
 * 
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
 * Contributor(s): Arystanbek Dyussenov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "Python.h"

#include "bpy_rna.h"

#include "RNA_access.h"

#include "BLI_string.h"

#include "MEM_guardedalloc.h"

typedef void (*ItemConvertFunc)(PyObject *, char *);
typedef int (*ItemTypeCheckFunc)(PyObject *);
typedef void (*RNA_SetArrayFunc)(PointerRNA *, PropertyRNA *, const char *);

/* Ensures that a python sequence has an expected number of items/sub-items and items are of expected type. */
static int pyrna_validate_array(PyObject *seq, unsigned short dim, unsigned short totdim, unsigned short dim_size[],
								ItemTypeCheckFunc check_item_type, const char *item_type_str, char *error_str, int error_str_size)
{
	int i;
	if (dim < totdim) {
		for (i= 0; i < PySequence_Length(seq); i++) {
			PyObject *item;
			int ok= 1;
			item= PySequence_GetItem(seq, i);

			if (!PySequence_Check(item)) {
				BLI_snprintf(error_str, error_str_size, "expected a %d-dimensional sequence of %s", (int)totdim, item_type_str);
				ok= 0;
			}
			else if (PySequence_Length(item) != dim_size[dim - 1]) {
				BLI_snprintf(error_str, error_str_size, "dimension %d should contain %d items", (int)dim, (int)dim_size[dim - 1]);
				ok= 0;
			}

			if (!pyrna_validate_array(item, dim + 1, totdim, dim_size, check_item_type, item_type_str, error_str, error_str_size)) {
				ok= 0;
			}

			Py_DECREF(item);

			if (!ok)
				return 0;
		}
	}
	else {
		for (i= 0; i < PySequence_Length(seq); i++) {
			PyObject *item= PySequence_GetItem(seq, i);

			if (!check_item_type(item)) {
				Py_DECREF(item);
							
				BLI_snprintf(error_str, error_str_size, "sequence items should be of type %s", item_type_str);
				return 0;
			}

			Py_DECREF(item);
		}
	}

	return 1;
}

/* Returns the number of items in a single- or multi-dimensional sequence. */
static int pyrna_count_items(PyObject *seq)
{
	int totitem= 0;

	if (PySequence_Check(seq)) {
		int i;
		for (i= 0; i < PySequence_Length(seq); i++) {
			PyObject *item= PySequence_GetItem(seq, i);
			totitem += pyrna_count_items(item);
			Py_DECREF(item);
		}
	}
	else
		totitem= 1;

	return totitem;
}

static int pyrna_apply_array_length(PointerRNA *ptr, PropertyRNA *prop, int totitem, char *error_str, int error_str_size)
{
	if (RNA_property_flag(prop) & PROP_DYNAMIC) {
		/* length can be flexible */
		if (RNA_property_array_length(ptr, prop) != totitem) {
			if (!RNA_property_dynamic_array_set_length(ptr, prop, totitem)) {
				BLI_snprintf(error_str, error_str_size, "%s.%s: array length cannot be changed to %d", RNA_struct_identifier(ptr->type), RNA_property_identifier(prop), totitem);
				return 0;
			}
		}
	}
	else {
		/* length is a constraint */
		int len= RNA_property_array_length(ptr, prop);
		if (totitem != len) {
			BLI_snprintf(error_str, error_str_size, "sequence must have length of %d", len);
			return 0;
		}
	}
	return 1;
}

static char *pyrna_py_to_array(PyObject *seq, unsigned short dim, unsigned short totdim, char *data, unsigned int item_size, ItemConvertFunc convert_item)
{
	unsigned int i;
	for (i= 0; i < PySequence_Length(seq); i++) {
		PyObject *item= PySequence_GetItem(seq, i);

		if (dim < totdim) {
			data= pyrna_py_to_array(item, dim + 1, totdim, data, item_size, convert_item);
		}
		else {
			convert_item(item, data);
			data += item_size;
		}
			
		Py_DECREF(item);
	}

	return data;
}

static int pyrna_py_to_array_generic(PyObject *py, PointerRNA *ptr, PropertyRNA *prop, char *param_data, char *error_str, int error_str_size,
									 ItemTypeCheckFunc check_item_type, const char *item_type_str, int item_size, ItemConvertFunc convert_item, RNA_SetArrayFunc rna_set_array)
{
	unsigned short totdim, dim_size[100];
	int totitem;
	char *data= NULL;

	totdim= RNA_property_array_dimension(prop, dim_size);

	if (!pyrna_validate_array(py, 1, totdim, dim_size, check_item_type, item_type_str, error_str, error_str_size))
		return 0;

	totitem= pyrna_count_items(py);

	if (!pyrna_apply_array_length(ptr, prop, totitem, error_str, error_str_size))
		return 0;

	if (totitem) {
		if (!param_data || RNA_property_flag(prop) & PROP_DYNAMIC)
			data= MEM_callocN(item_size * totitem, "pyrna primitive type array");
		else
			data= param_data;

		pyrna_py_to_array(py, 1, totdim, data, item_size, convert_item);

		if (param_data) {
			if (RNA_property_flag(prop) & PROP_DYNAMIC) {
				/* not freeing allocated mem, RNA_parameter_list_free will do this */
				*(char**)param_data= data;
			}
		}
		else {
			/* NULL can only pass through in case RNA property arraylength is 0 (impossible?) */
			rna_set_array(ptr, prop, data);
			MEM_freeN(data);
		}
	}

	return 1;
}

static void pyrna_py_to_float(PyObject *py, char *data)
{
	*(float*)data= (float)PyFloat_AsDouble(py);
}

static void pyrna_py_to_int(PyObject *py, char *data)
{
	*(int*)data= (int)PyLong_AsSsize_t(py);
}

static void pyrna_py_to_boolean(PyObject *py, char *data)
{
	*(int*)data= (int)PyObject_IsTrue(py);
}

static int py_float_check(PyObject *py)
{
	return PyFloat_Check(py) || (PyIndex_Check(py));
}

static int py_int_check(PyObject *py)
{
	return PyLong_Check(py) || (PyIndex_Check(py));
}

static int py_bool_check(PyObject *py)
{
	return PyBool_Check(py) || (PyIndex_Check(py));
}

int pyrna_py_to_float_array(PyObject *py, PointerRNA *ptr, PropertyRNA *prop, char *param_data, char *error_str, int error_str_size)
{
	return pyrna_py_to_array_generic(py, ptr, prop, param_data, error_str, error_str_size,
									 py_float_check, "float", sizeof(float), pyrna_py_to_float, (RNA_SetArrayFunc)RNA_property_float_set_array);
}

int pyrna_py_to_int_array(PyObject *py, PointerRNA *ptr, PropertyRNA *prop, char *param_data, char *error_str, int error_str_size)
{
	return pyrna_py_to_array_generic(py, ptr, prop, param_data, error_str, error_str_size,
									 py_int_check, "int", sizeof(int), pyrna_py_to_int, (RNA_SetArrayFunc)RNA_property_int_set_array);
}

int pyrna_py_to_boolean_array(PyObject *py, PointerRNA *ptr, PropertyRNA *prop, char *param_data, char *error_str, int error_str_size)
{
	return pyrna_py_to_array_generic(py, ptr, prop, param_data, error_str, error_str_size,
									 py_bool_check, "boolean", sizeof(int), pyrna_py_to_boolean, (RNA_SetArrayFunc)RNA_property_boolean_set_array);
}
