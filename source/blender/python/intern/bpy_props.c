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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "bpy_props.h"
#include "bpy_rna.h"
#include "bpy_util.h"

#include "RNA_define.h" /* for defining our own rna */
#include "RNA_enum_types.h"

#include "MEM_guardedalloc.h"

EnumPropertyItem property_flag_items[] = {
	{PROP_HIDDEN, "HIDDEN", 0, "Hidden", ""},
	{PROP_ANIMATABLE, "ANIMATABLE", 0, "Animateable", ""},
	{0, NULL, 0, NULL, NULL}};

/* subtypes */
EnumPropertyItem property_subtype_string_items[] = {
	{PROP_FILEPATH, "FILE_PATH", 0, "File Path", ""},
	{PROP_DIRPATH, "DIR_PATH", 0, "Directory Path", ""},
	{PROP_FILENAME, "FILENAME", 0, "Filename", ""},

	{PROP_NONE, "NONE", 0, "None", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem property_subtype_number_items[] = {
	{PROP_UNSIGNED, "UNSIGNED", 0, "Unsigned", ""},
	{PROP_PERCENTAGE, "PERCENTAGE", 0, "Percentage", ""},
	{PROP_FACTOR, "FACTOR", 0, "Factor", ""},
	{PROP_ANGLE, "ANGLE", 0, "Angle", ""},
	{PROP_TIME, "TIME", 0, "Time", ""},
	{PROP_DISTANCE, "DISTANCE", 0, "Distance", ""},

	{PROP_NONE, "NONE", 0, "None", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem property_subtype_array_items[] = {
	{PROP_COLOR, "COLOR", 0, "Color", ""},
	{PROP_TRANSLATION, "TRANSLATION", 0, "Translation", ""},
	{PROP_DIRECTION, "DIRECTION", 0, "Direction", ""},
	{PROP_VELOCITY, "VELOCITY", 0, "Velocity", ""},
	{PROP_ACCELERATION, "ACCELERATION", 0, "Acceleration", ""},
	{PROP_MATRIX, "MATRIX", 0, "Matrix", ""},
	{PROP_EULER, "EULER", 0, "Euler", ""},
	{PROP_QUATERNION, "QUATERNION", 0, "Quaternion", ""},
	{PROP_AXISANGLE, "AXISANGLE", 0, "Axis Angle", ""},
	{PROP_XYZ, "XYZ", 0, "XYZ", ""},
	{PROP_COLOR_GAMMA, "COLOR_GAMMA", 0, "Color Gamma", ""},
	{PROP_LAYER, "LAYER", 0, "Layer", ""},

	{PROP_NONE, "NONE", 0, "None", ""},
	{0, NULL, 0, NULL, NULL}};

/* operators use this so it can store the args given but defer running
 * it until the operator runs where these values are used to setup the
 * default args for that operator instance */
static PyObject *bpy_prop_deferred_return(void *func, PyObject *kw)
{
	PyObject *ret = PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, PyCapsule_New(func, NULL, NULL));
	if(kw==NULL)	kw= PyDict_New();
	else			Py_INCREF(kw);
	PyTuple_SET_ITEM(ret, 1, kw);
	return ret;
}

#if 0
static int bpy_struct_id_used(StructRNA *srna, char *identifier)
{
	PointerRNA ptr;
	RNA_pointer_create(NULL, srna, NULL, &ptr);
	return (RNA_struct_find_property(&ptr, identifier) != NULL);
}
#endif


/* Function that sets RNA, NOTE - self is NULL when called from python, but being abused from C so we can pass the srna allong
 * This isnt incorrect since its a python object - but be careful */
char BPy_BoolProperty_doc[] =
".. function:: BoolProperty(name=\"\", description=\"\", default=False, options={'ANIMATABLE'}, subtype='NONE')\n"
"\n"
"   Returns a new boolean property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg subtype: Enumerator in ['UNSIGNED', 'PERCENTAGE', 'FACTOR', 'ANGLE', 'TIME', 'DISTANCE', 'NONE'].\n"
"   :type subtype: string";

PyObject *BPy_BoolProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "BoolProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "name", "description", "default", "options", "subtype", NULL};
		char *id=NULL, *name="", *description="";
		int def=0;
		PropertyRNA *prop;
		PyObject *pyopts= NULL;
		int opts=0;
		char *pysubtype= NULL;
		int subtype= PROP_NONE;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssiO!s:BoolProperty", (char **)kwlist, &id, &name, &description, &def, &PySet_Type, &pyopts, &pysubtype))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "BoolProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "BoolProperty(options={...}):"))
			return NULL;

		if(pysubtype && RNA_enum_value_from_id(property_subtype_number_items, pysubtype, &subtype)==0) {
			PyErr_Format(PyExc_TypeError, "BoolProperty(subtype='%s'): invalid subtype.", pysubtype);
			return NULL;
		}

		// prop= RNA_def_boolean(srna, id, def, name, description);
		prop= RNA_def_property(srna, id, PROP_BOOLEAN, subtype);
		RNA_def_property_boolean_default(prop, def);
		RNA_def_property_ui_text(prop, name, description);

		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_BoolProperty, kw);
	}
}

char BPy_BoolVectorProperty_doc[] =
".. function:: BoolVectorProperty(name=\"\", description=\"\", default=(False, False, False), options={'ANIMATABLE'}, subtype='NONE', size=3)\n"
"\n"
"   Returns a new vector boolean property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg subtype: Enumerator in ['COLOR', 'TRANSLATION', 'DIRECTION', 'VELOCITY', 'ACCELERATION', 'MATRIX', 'EULER', 'QUATERNION', 'AXISANGLE', 'XYZ', 'COLOR_GAMMA', 'LAYER', 'NONE'].\n"
"   :type subtype: string";
PyObject *BPy_BoolVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "BoolVectorProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default", "options", "subtype", "size", NULL};
		char *id=NULL, *name="", *description="";
		int def[PYRNA_STACK_ARRAY]={0};
		int size=3;
		PropertyRNA *prop;
		PyObject *pydef= NULL;
		PyObject *pyopts= NULL;
		int opts=0;
		char *pysubtype= NULL;
		int subtype= PROP_NONE;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssOO!si:BoolVectorProperty", (char **)kwlist, &id, &name, &description, &pydef, &PySet_Type, &pyopts, &pysubtype, &size))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "BoolVectorProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "BoolVectorProperty(options={...}):"))
			return NULL;

		if(pysubtype && RNA_enum_value_from_id(property_subtype_array_items, pysubtype, &subtype)==0) {
			PyErr_Format(PyExc_TypeError, "BoolVectorProperty(subtype='%s'): invalid subtype.", pysubtype);
			return NULL;
		}

		if(size < 1 || size > PYRNA_STACK_ARRAY) {
			PyErr_Format(PyExc_TypeError, "BoolVectorProperty(size=%d): size must be between 0 and %d.", size, PYRNA_STACK_ARRAY);
			return NULL;
		}

		if(pydef && BPyAsPrimitiveArray(def, pydef, size, &PyBool_Type, "BoolVectorProperty(default=sequence)") < 0)
			return NULL;

		// prop= RNA_def_boolean_array(srna, id, size, pydef ? def:NULL, name, description);
		prop= RNA_def_property(srna, id, PROP_BOOLEAN, subtype);
		RNA_def_property_array(prop, size);
		if(pydef) RNA_def_property_boolean_array_default(prop, def);
		RNA_def_property_ui_text(prop, name, description);

		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_BoolVectorProperty, kw);
	}
}

char BPy_IntProperty_doc[] =
".. function:: IntProperty(name=\"\", description=\"\", default=0, min=-sys.maxint, max=sys.maxint, soft_min=-sys.maxint, soft_max=sys.maxint, step=1, options={'ANIMATABLE'}, subtype='NONE')\n"
"\n"
"   Returns a new int property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg subtype: Enumerator in ['UNSIGNED', 'PERCENTAGE', 'FACTOR', 'ANGLE', 'TIME', 'DISTANCE', 'NONE'].\n"
"   :type subtype: string";
PyObject *BPy_IntProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "IntProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default", "min", "max", "soft_min", "soft_max", "step", "options", "subtype", NULL};
		char *id=NULL, *name="", *description="";
		int min=INT_MIN, max=INT_MAX, soft_min=INT_MIN, soft_max=INT_MAX, step=1, def=0;
		PropertyRNA *prop;
		PyObject *pyopts= NULL;
		int opts=0;
		char *pysubtype= NULL;
		int subtype= PROP_NONE;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssiiiiiiO!s:IntProperty", (char **)kwlist, &id, &name, &description, &def, &min, &max, &soft_min, &soft_max, &step, &PySet_Type, &pyopts, &pysubtype))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "IntProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "IntProperty(options={...}):"))
			return NULL;

		if(pysubtype && RNA_enum_value_from_id(property_subtype_number_items, pysubtype, &subtype)==0) {
			PyErr_Format(PyExc_TypeError, "IntProperty(subtype='%s'): invalid subtype.", pysubtype);
			return NULL;
		}

		prop= RNA_def_property(srna, id, PROP_INT, subtype);
		RNA_def_property_int_default(prop, def);
		RNA_def_property_range(prop, min, max);
		RNA_def_property_ui_text(prop, name, description);
		RNA_def_property_ui_range(prop, soft_min, soft_max, step, 3);

		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_IntProperty, kw);
	}
}

char BPy_IntVectorProperty_doc[] =
".. function:: IntVectorProperty(name=\"\", description=\"\", default=(0, 0, 0), min=-sys.maxint, max=sys.maxint, soft_min=-sys.maxint, soft_max=sys.maxint, options={'ANIMATABLE'}, subtype='NONE', size=3)\n"
"\n"
"   Returns a new vector int property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg subtype: Enumerator in ['COLOR', 'TRANSLATION', 'DIRECTION', 'VELOCITY', 'ACCELERATION', 'MATRIX', 'EULER', 'QUATERNION', 'AXISANGLE', 'XYZ', 'COLOR_GAMMA', 'LAYER', 'NONE'].\n"
"   :type subtype: string";
PyObject *BPy_IntVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "IntVectorProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default", "min", "max", "soft_min", "soft_max", "step", "options", "subtype", "size", NULL};
		char *id=NULL, *name="", *description="";
		int min=INT_MIN, max=INT_MAX, soft_min=INT_MIN, soft_max=INT_MAX, step=1, def[PYRNA_STACK_ARRAY]={0};
		int size=3;
		PropertyRNA *prop;
		PyObject *pydef= NULL;
		PyObject *pyopts= NULL;
		int opts=0;
		char *pysubtype= NULL;
		int subtype= PROP_NONE;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssOiiiiO!si:IntVectorProperty", (char **)kwlist, &id, &name, &description, &pydef, &min, &max, &soft_min, &soft_max, &PySet_Type, &pyopts, &pysubtype, &size))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "IntVectorProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "IntVectorProperty(options={...}):"))
			return NULL;

		if(pysubtype && RNA_enum_value_from_id(property_subtype_array_items, pysubtype, &subtype)==0) {
			PyErr_Format(PyExc_TypeError, "IntVectorProperty(subtype='%s'): invalid subtype.", pysubtype);
			return NULL;
		}

		if(size < 1 || size > PYRNA_STACK_ARRAY) {
			PyErr_Format(PyExc_TypeError, "IntVectorProperty(size=%d): size must be between 0 and %d.", size, PYRNA_STACK_ARRAY);
			return NULL;
		}

		if(pydef && BPyAsPrimitiveArray(def, pydef, size, &PyLong_Type, "IntVectorProperty(default=sequence)") < 0)
			return NULL;

		prop= RNA_def_property(srna, id, PROP_INT, subtype);
		RNA_def_property_array(prop, size);
		if(pydef) RNA_def_property_int_array_default(prop, def);
		RNA_def_property_range(prop, min, max);
		RNA_def_property_ui_text(prop, name, description);
		RNA_def_property_ui_range(prop, soft_min, soft_max, step, 3);

		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_IntVectorProperty, kw);
	}
}


char BPy_FloatProperty_doc[] =
".. function:: FloatProperty(name=\"\", description=\"\", default=0.0, min=sys.float_info.min, max=sys.float_info.max, soft_min=sys.float_info.min, soft_max=sys.float_info.max, step=3, precision=2, options={'ANIMATABLE'}, subtype='NONE', unit='NONE')\n"
"\n"
"   Returns a new float property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg subtype: Enumerator in ['UNSIGNED', 'PERCENTAGE', 'FACTOR', 'ANGLE', 'TIME', 'DISTANCE', 'NONE'].\n"
"   :type subtype: string\n"
"   :arg unit: Enumerator in ['NONE', 'LENGTH', 'AREA', 'VOLUME', 'ROTATION', 'TIME', 'VELOCITY', 'ACCELERATION'].\n"
"   :type unit: string\n";
PyObject *BPy_FloatProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "FloatProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default", "min", "max", "soft_min", "soft_max", "step", "precision", "options", "subtype", "unit", NULL};
		char *id=NULL, *name="", *description="";
		float min=-FLT_MAX, max=FLT_MAX, soft_min=-FLT_MAX, soft_max=FLT_MAX, step=3, def=0.0f;
		int precision= 2;
		PropertyRNA *prop;
		PyObject *pyopts= NULL;
		int opts=0;
		char *pysubtype= NULL;
		int subtype= PROP_NONE;
		char *pyunit= NULL;
		int unit= PROP_UNIT_NONE;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssffffffiO!ss:FloatProperty", (char **)kwlist, &id, &name, &description, &def, &min, &max, &soft_min, &soft_max, &step, &precision, &PySet_Type, &pyopts, &pysubtype, &pyunit))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "FloatProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "FloatProperty(options={...}):"))
			return NULL;

		if(pysubtype && RNA_enum_value_from_id(property_subtype_number_items, pysubtype, &subtype)==0) {
			PyErr_Format(PyExc_TypeError, "FloatProperty(subtype='%s'): invalid subtype.", pysubtype);
			return NULL;
		}

		if(pyunit && RNA_enum_value_from_id(property_unit_items, pyunit, &unit)==0) {
			PyErr_Format(PyExc_TypeError, "FloatProperty(unit='%s'): invalid unit.");
			return NULL;
		}

		prop= RNA_def_property(srna, id, PROP_FLOAT, subtype | unit);
		RNA_def_property_float_default(prop, def);
		RNA_def_property_range(prop, min, max);
		RNA_def_property_ui_text(prop, name, description);
		RNA_def_property_ui_range(prop, soft_min, soft_max, step, precision);

		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_FloatProperty, kw);
	}
}

char BPy_FloatVectorProperty_doc[] =
".. function:: FloatVectorProperty(name=\"\", description=\"\", default=(0.0, 0.0, 0.0), min=sys.float_info.min, max=sys.float_info.max, soft_min=sys.float_info.min, soft_max=sys.float_info.max, step=3, precision=2, options={'ANIMATABLE'}, subtype='NONE', size=3)\n"
"\n"
"   Returns a new vector float property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg subtype: Enumerator in ['COLOR', 'TRANSLATION', 'DIRECTION', 'VELOCITY', 'ACCELERATION', 'MATRIX', 'EULER', 'QUATERNION', 'AXISANGLE', 'XYZ', 'COLOR_GAMMA', 'LAYER', 'NONE'].\n"
"   :type subtype: string";
PyObject *BPy_FloatVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "FloatVectorProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default", "min", "max", "soft_min", "soft_max", "step", "precision", "options", "subtype", "size", NULL};
		char *id=NULL, *name="", *description="";
		float min=-FLT_MAX, max=FLT_MAX, soft_min=-FLT_MAX, soft_max=FLT_MAX, step=3, def[PYRNA_STACK_ARRAY]={0.0f};
		int precision= 2, size=3;
		PropertyRNA *prop;
		PyObject *pydef= NULL;
		PyObject *pyopts= NULL;
		int opts=0;
		char *pysubtype= NULL;
		int subtype= PROP_NONE;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssOfffffiO!si:FloatVectorProperty", (char **)kwlist, &id, &name, &description, &pydef, &min, &max, &soft_min, &soft_max, &step, &precision, &PySet_Type, &pyopts, &pysubtype, &size))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "FloatVectorProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "FloatVectorProperty(options={...}):"))
			return NULL;

		if(pysubtype && RNA_enum_value_from_id(property_subtype_array_items, pysubtype, &subtype)==0) {
			PyErr_Format(PyExc_TypeError, "FloatVectorProperty(subtype='%s'): invalid subtype.", pysubtype);
			return NULL;
		}

		if(size < 1 || size > PYRNA_STACK_ARRAY) {
			PyErr_Format(PyExc_TypeError, "FloatVectorProperty(size=%d): size must be between 0 and %d.", size, PYRNA_STACK_ARRAY);
			return NULL;
		}

		if(pydef && BPyAsPrimitiveArray(def, pydef, size, &PyFloat_Type, "FloatVectorProperty(default=sequence)") < 0)
			return NULL;

		prop= RNA_def_property(srna, id, PROP_FLOAT, subtype);
		RNA_def_property_array(prop, size);
		if(pydef) RNA_def_property_float_array_default(prop, def);
		RNA_def_property_range(prop, min, max);
		RNA_def_property_ui_text(prop, name, description);
		RNA_def_property_ui_range(prop, soft_min, soft_max, step, precision);

		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_FloatVectorProperty, kw);
	}
}

char BPy_StringProperty_doc[] =
".. function:: StringProperty(name=\"\", description=\"\", default=\"\", maxlen=0, options={'ANIMATABLE'}, subtype='NONE')\n"
"\n"
"   Returns a new string property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg subtype: Enumerator in ['FILE_PATH', 'DIR_PATH', 'FILENAME', 'NONE'].\n"
"   :type subtype: string";
PyObject *BPy_StringProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "StringProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default", "maxlen", "options", "subtype", NULL};
		char *id=NULL, *name="", *description="", *def="";
		int maxlen=0;
		PropertyRNA *prop;
		PyObject *pyopts= NULL;
		int opts=0;
		char *pysubtype= NULL;
		int subtype= PROP_NONE;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|sssiO!s:StringProperty", (char **)kwlist, &id, &name, &description, &def, &maxlen, &PySet_Type, &pyopts, &pysubtype))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "StringProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "StringProperty(options={...}):"))
			return NULL;

		if(pysubtype && RNA_enum_value_from_id(property_subtype_string_items, pysubtype, &subtype)==0) {
			PyErr_Format(PyExc_TypeError, "StringProperty(subtype='%s'): invalid subtype.", pysubtype);
			return NULL;
		}

		prop= RNA_def_property(srna, id, PROP_STRING, subtype);
		if(maxlen != 0) RNA_def_property_string_maxlength(prop, maxlen);
		if(def) RNA_def_property_string_default(prop, def);
		RNA_def_property_ui_text(prop, name, description);

		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_StringProperty, kw);
	}
}

static EnumPropertyItem *enum_items_from_py(PyObject *value, const char *def, int *defvalue)
{
	EnumPropertyItem *items= NULL;
	PyObject *item;
	int seq_len, i, totitem= 0;

	if(!PySequence_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "expected a sequence of tuples for the enum items");
		return NULL;
	}

	seq_len = PySequence_Length(value);
	for(i=0; i<seq_len; i++) {
		EnumPropertyItem tmp= {0, "", 0, "", ""};

		item= PySequence_GetItem(value, i);
		if(item==NULL || PyTuple_Check(item)==0) {
			PyErr_SetString(PyExc_TypeError, "expected a sequence of tuples for the enum items");
			if(items) MEM_freeN(items);
			Py_XDECREF(item);
			return NULL;
		}

		if(!PyArg_ParseTuple(item, "sss", &tmp.identifier, &tmp.name, &tmp.description)) {
			PyErr_SetString(PyExc_TypeError, "expected an identifier, name and description in the tuple");
			Py_DECREF(item);
			return NULL;
		}

		tmp.value= i;
		RNA_enum_item_add(&items, &totitem, &tmp);

		if(def[0] && strcmp(def, tmp.identifier) == 0)
			*defvalue= tmp.value;

		Py_DECREF(item);
	}

	if(!def[0])
		*defvalue= 0;

	RNA_enum_item_end(&items, &totitem);

	return items;
}

char BPy_EnumProperty_doc[] =
".. function:: EnumProperty(items, name=\"\", description=\"\", default=\"\", options={'ANIMATABLE'})\n"
"\n"
"   Returns a new enumerator property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg items: The items that make up this enumerator.\n"
"   :type items: sequence of string triplets";
PyObject *BPy_EnumProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "EnumProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "items", "name", "description", "default", "options", NULL};
		char *id=NULL, *name="", *description="", *def="";
		int defvalue=0;
		PyObject *items= Py_None;
		EnumPropertyItem *eitems;
		PropertyRNA *prop;
		PyObject *pyopts= NULL;
		int opts=0;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|sssO!:EnumProperty", (char **)kwlist, &id, &items, &name, &description, &def, &PySet_Type, &pyopts))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "EnumProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "EnumProperty(options={...}):"))
			return NULL;

		eitems= enum_items_from_py(items, def, &defvalue);
		if(!eitems)
			return NULL;

		prop= RNA_def_enum(srna, id, eitems, defvalue, name, description);
		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		MEM_freeN(eitems);

		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_EnumProperty, kw);
	}
}

static StructRNA *pointer_type_from_py(PyObject *value, const char *error_prefix)
{
	StructRNA *srna;

	srna= srna_from_self(value, "BoolProperty(...):");
	if(!srna) {

		PyObject *msg= BPY_exception_buffer();
		char *msg_char= _PyUnicode_AsString(msg);
		PyErr_Format(PyExc_TypeError, "%.200s expected an RNA type derived from IDPropertyGroup, failed with: %s", error_prefix, msg_char);
		Py_DECREF(msg);
		return NULL;
	}

	if(!RNA_struct_is_a(srna, &RNA_IDPropertyGroup)) {
		 PyErr_Format(PyExc_SystemError, "%.200s expected an RNA type derived from IDPropertyGroup", error_prefix);
		return NULL;
	}

	return srna;
}

char BPy_PointerProperty_doc[] =
".. function:: PointerProperty(items, type=\"\", description=\"\", default=\"\", options={'ANIMATABLE'})\n"
"\n"
"   Returns a new pointer property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg type: Dynamic type from :mod:`bpy.types`.\n"
"   :type type: class";
PyObject *BPy_PointerProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "PointerProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "type", "name", "description", "options", NULL};
		char *id=NULL, *name="", *description="";
		PropertyRNA *prop;
		StructRNA *ptype;
		PyObject *type= Py_None;
		PyObject *pyopts= NULL;
		int opts=0;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|ssO!:PointerProperty", (char **)kwlist, &id, &type, &name, &description, &PySet_Type, &pyopts))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "PointerProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "PointerProperty(options={...}):"))
			return NULL;

		ptype= pointer_type_from_py(type, "PointerProperty(...):");
		if(!ptype)
			return NULL;

		prop= RNA_def_pointer_runtime(srna, id, ptype, name, description);
		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_PointerProperty, kw);
	}
	return NULL;
}

char BPy_CollectionProperty_doc[] =
".. function:: CollectionProperty(items, type=\"\", description=\"\", default=\"\", options={'ANIMATABLE'})\n"
"\n"
"   Returns a new collection property definition.\n"
"\n"
"   :arg options: Enumerator in ['HIDDEN', 'ANIMATABLE'].\n"
"   :type options: set\n"
"   :arg type: Dynamic type from :mod:`bpy.types`.\n"
"   :type type: class";
PyObject *BPy_CollectionProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) > 0) {
		 PyErr_SetString(PyExc_ValueError, "all args must be keywords");
		return NULL;
	}

	srna= srna_from_self(self, "CollectionProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static const char *kwlist[] = {"attr", "type", "name", "description", "options", NULL};
		char *id=NULL, *name="", *description="";
		PropertyRNA *prop;
		StructRNA *ptype;
		PyObject *type= Py_None;
		PyObject *pyopts= NULL;
		int opts=0;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|ssO!:CollectionProperty", (char **)kwlist, &id, &type, &name, &description, &PySet_Type, &pyopts))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) == -1) {
			PyErr_Format(PyExc_TypeError, "CollectionProperty(): '%s' is defined as a non-dynamic type.", id);
			return NULL;
		}

		if(pyopts && pyrna_set_to_enum_bitfield(property_flag_items, pyopts, &opts, "CollectionProperty(options={...}):"))
			return NULL;

		ptype= pointer_type_from_py(type, "CollectionProperty(...):");
		if(!ptype)
			return NULL;

		prop= RNA_def_collection_runtime(srna, id, ptype, name, description);
		if(pyopts) {
			if(opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if((opts & PROP_ANIMATABLE)==0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
		}
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_CollectionProperty, kw);
	}
	return NULL;
}

char BPy_RemoveProperty_doc[] =
".. function:: RemoveProperty(attr)\n"
"\n"
"   Removes a dynamically defined property.\n"
"\n"
"   :arg attr: Property name.\n"
"   :type attr: string";
PyObject *BPy_RemoveProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	srna= srna_from_self(self, "RemoveProperty(...):");
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna==NULL) {
		PyErr_SetString(PyExc_TypeError, "RemoveProperty(): struct rna not available for this type.");
		return NULL;
	}
	else {
		static const char *kwlist[] = {"attr", NULL};
		
		char *id=NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s:RemoveProperty", (char **)kwlist, &id))
			return NULL;

		if(RNA_def_property_free_identifier(srna, id) != 1) {
			PyErr_Format(PyExc_TypeError, "RemoveProperty(): '%s' not a defined dynamic property.", id);
			return NULL;
		}
		
		Py_RETURN_NONE;
	}
}

static struct PyMethodDef props_methods[] = {
	{"BoolProperty", (PyCFunction)BPy_BoolProperty, METH_VARARGS|METH_KEYWORDS, BPy_BoolProperty_doc},
	{"BoolVectorProperty", (PyCFunction)BPy_BoolVectorProperty, METH_VARARGS|METH_KEYWORDS, BPy_BoolVectorProperty_doc},
	{"IntProperty", (PyCFunction)BPy_IntProperty, METH_VARARGS|METH_KEYWORDS, BPy_IntProperty_doc},
	{"IntVectorProperty", (PyCFunction)BPy_IntVectorProperty, METH_VARARGS|METH_KEYWORDS, BPy_IntVectorProperty_doc},
	{"FloatProperty", (PyCFunction)BPy_FloatProperty, METH_VARARGS|METH_KEYWORDS, BPy_FloatProperty_doc},
	{"FloatVectorProperty", (PyCFunction)BPy_FloatVectorProperty, METH_VARARGS|METH_KEYWORDS, BPy_FloatVectorProperty_doc},
	{"StringProperty", (PyCFunction)BPy_StringProperty, METH_VARARGS|METH_KEYWORDS, BPy_StringProperty_doc},
	{"EnumProperty", (PyCFunction)BPy_EnumProperty, METH_VARARGS|METH_KEYWORDS, BPy_EnumProperty_doc},
	{"PointerProperty", (PyCFunction)BPy_PointerProperty, METH_VARARGS|METH_KEYWORDS, BPy_PointerProperty_doc},
	{"CollectionProperty", (PyCFunction)BPy_CollectionProperty, METH_VARARGS|METH_KEYWORDS, BPy_CollectionProperty_doc},

	/* only useful as a bpy_struct method */
	/* {"RemoveProperty", (PyCFunction)BPy_RemoveProperty, METH_VARARGS|METH_KEYWORDS, BPy_RemoveProperty_doc}, */
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef props_module = {
	PyModuleDef_HEAD_INIT,
	"bpy.props",
	"This module defines properties to extend blenders internal data, the result of these functions"
	" is used to assign properties to classes registered with blender and can't be used directly.",
	-1,/* multiple "initialization" just copies the module dict. */
	props_methods,
	NULL, NULL, NULL, NULL
};

PyObject *BPY_rna_props( void )
{
	PyObject *submodule;
	submodule= PyModule_Create(&props_module);
	PyDict_SetItemString(PySys_GetObject("modules"), props_module.m_name, submodule);

	/* INCREF since its its assumed that all these functions return the
	 * module with a new ref like PyDict_New, since they are passed to
	  * PyModule_AddObject which steals a ref */
	Py_INCREF(submodule);

	return submodule;
}
