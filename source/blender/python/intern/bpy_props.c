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

#include "bpy_props.h"
#include "bpy_rna.h"

#include "RNA_access.h"
#include "RNA_define.h" /* for defining our own rna */

#include "MEM_guardedalloc.h"

#include "float.h" /* FLT_MIN/MAX */

/* operators use this so it can store the args given but defer running
 * it until the operator runs where these values are used to setup the
 * default args for that operator instance */
static PyObject *bpy_prop_deferred_return(void *func, PyObject *kw)
{
	PyObject *ret = PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, PyCObject_FromVoidPtr(func, NULL));
	PyTuple_SET_ITEM(ret, 1, kw);
	Py_INCREF(kw);
	return ret;
}

/* Function that sets RNA, NOTE - self is NULL when called from python, but being abused from C so we can pass the srna allong
 * This isnt incorrect since its a python object - but be careful */
static char BPy_BoolProperty_doc[] =
".. function:: BoolProperty(name=\"\", description=\"\", default=False, hidden=False)\n"
"\n"
"   Returns a new boolean property definition..";

PyObject *BPy_BoolProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_Size(args) > 0) {
	 	PyErr_SetString(PyExc_ValueError, "all args must be keywors"); // TODO - py3 can enforce this.
		return NULL;
	}

	srna= srna_from_self(self);
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "name", "description", "default", "hidden", NULL};
		char *id=NULL, *name="", *description="";
		int def=0, hidden=0;
		PropertyRNA *prop;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssii:BoolProperty", kwlist, &id, &name, &description, &def, &hidden))
			return NULL;

		prop= RNA_def_boolean(srna, id, def, name, description);
		if(hidden) RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_BoolProperty, kw);
	}
}

static char BPy_IntProperty_doc[] =
".. function:: IntProperty(name=\"\", description=\"\", default=0, min=-sys.maxint, max=sys.maxint, soft_min=-sys.maxint, soft_max=sys.maxint, step=1, hidden=False)\n"
"\n"
"   Returns a new int property definition.";
PyObject *BPy_IntProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_Size(args) > 0) {
	 	PyErr_SetString(PyExc_ValueError, "all args must be keywors"); // TODO - py3 can enforce this.
		return NULL;
	}

	srna= srna_from_self(self);
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "name", "description", "default", "min", "max", "soft_min", "soft_max", "step", "hidden", NULL};
		char *id=NULL, *name="", *description="";
		int min=INT_MIN, max=INT_MAX, soft_min=INT_MIN, soft_max=INT_MAX, step=1, def=0;
		int hidden=0;
		PropertyRNA *prop;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssiiiiiii:IntProperty", kwlist, &id, &name, &description, &def, &min, &max, &soft_min, &soft_max, &step, &hidden))
			return NULL;

		prop= RNA_def_int(srna, id, def, min, max, name, description, soft_min, soft_max);
		RNA_def_property_ui_range(prop, min, max, step, 0);
		if(hidden) RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_IntProperty, kw);
	}
}

static char BPy_FloatProperty_doc[] =
".. function:: FloatProperty(name=\"\", description=\"\", default=0.0, min=sys.float_info.min, max=sys.float_info.max, soft_min=sys.float_info.min, soft_max=sys.float_info.max, step=3, precision=2, hidden=False)\n"
"\n"
"   Returns a new float property definition.";
PyObject *BPy_FloatProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_Size(args) > 0) {
	 	PyErr_SetString(PyExc_ValueError, "all args must be keywors"); // TODO - py3 can enforce this.
		return NULL;
	}

	srna= srna_from_self(self);
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "name", "description", "default", "min", "max", "soft_min", "soft_max", "step", "precision", "hidden", NULL};
		char *id=NULL, *name="", *description="";
		float min=-FLT_MAX, max=FLT_MAX, soft_min=-FLT_MAX, soft_max=FLT_MAX, step=3, def=0.0f;
		int precision= 2, hidden=0;
		PropertyRNA *prop;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssffffffii:FloatProperty", kwlist, &id, &name, &description, &def, &min, &max, &soft_min, &soft_max, &step, &precision, &hidden))
			return NULL;

		prop= RNA_def_float(srna, id, def, min, max, name, description, soft_min, soft_max);
		RNA_def_property_ui_range(prop, min, max, step, precision);
		if(hidden) RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_FloatProperty, kw);
	}
}

static char BPy_FloatVectorProperty_doc[] =
".. function:: FloatVectorProperty(name=\"\", description=\"\", default=(0.0, 0.0, 0.0), min=sys.float_info.min, max=sys.float_info.max, soft_min=sys.float_info.min, soft_max=sys.float_info.max, step=3, precision=2, hidden=False, size=3)\n"
"\n"
"   Returns a new vector float property definition.";
PyObject *BPy_FloatVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_Size(args) > 0) {
	 	PyErr_SetString(PyExc_ValueError, "all args must be keywors"); // TODO - py3 can enforce this.
		return NULL;
	}

	srna= srna_from_self(self);
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "name", "description", "default", "min", "max", "soft_min", "soft_max", "step", "precision", "hidden", "size", NULL};
		char *id=NULL, *name="", *description="";
		float min=-FLT_MAX, max=FLT_MAX, soft_min=-FLT_MAX, soft_max=FLT_MAX, step=3, def[PYRNA_STACK_ARRAY]={0.0f};
		int precision= 2, hidden=0, size=3;
		PropertyRNA *prop;
		PyObject *pydef= NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|ssOfffffiii:FloatVectorProperty", kwlist, &id, &name, &description, &pydef, &min, &max, &soft_min, &soft_max, &step, &precision, &hidden, &size))
			return NULL;

		if(size < 0 || size > PYRNA_STACK_ARRAY) {
			PyErr_Format(PyExc_TypeError, "FloatVectorProperty(): size must be between 0 and %d, given %d.", PYRNA_STACK_ARRAY, size);
			return NULL;
		}

		if(pydef) {
			int i;

			if(!PySequence_Check(pydef)) {
				PyErr_Format(PyExc_TypeError, "FloatVectorProperty(): default value is not a sequence of size: %d.", size);
				return NULL;
			}

			if(size != PySequence_Size(pydef)) {
				PyErr_Format(PyExc_TypeError, "FloatVectorProperty(): size: %d, does not default: %d.", size, PySequence_Size(pydef));
				return NULL;
			}

			for(i=0; i<size; i++) {
				PyObject *item= PySequence_GetItem(pydef, i);
				if(item) {
					def[i]= PyFloat_AsDouble(item);
					Py_DECREF(item);
				}
			}

			if(PyErr_Occurred()) { /* error set above */
				return NULL;
			}
		}

		prop= RNA_def_float_vector(srna, id, size, pydef ? def:NULL, min, max, name, description, soft_min, soft_max);
		RNA_def_property_ui_range(prop, min, max, step, precision);
		if(hidden) RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_FloatVectorProperty, kw);
	}
}

static char BPy_StringProperty_doc[] =
".. function:: StringProperty(name=\"\", description=\"\", default=\"\", maxlen=0, hidden=False)\n"
"\n"
"   Returns a new string property definition.";
PyObject *BPy_StringProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_Size(args) > 0) {
	 	PyErr_SetString(PyExc_ValueError, "all args must be keywors"); // TODO - py3 can enforce this.
		return NULL;
	}

	srna= srna_from_self(self);
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "name", "description", "default", "maxlen", "hidden", NULL};
		char *id=NULL, *name="", *description="", *def="";
		int maxlen=0, hidden=0;
		PropertyRNA *prop;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "s|sssii:StringProperty", kwlist, &id, &name, &description, &def, &maxlen, &hidden))
			return NULL;

		prop= RNA_def_string(srna, id, def, maxlen, name, description);
		if(hidden) RNA_def_property_flag(prop, PROP_HIDDEN);
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

static char BPy_EnumProperty_doc[] =
".. function:: EnumProperty(items, name=\"\", description=\"\", default=\"\", hidden=False)\n"
"\n"
"   Returns a new enumerator property definition.\n"
"\n"
"   :arg items: The items that make up this enumerator.\n"
"   :type items: sequence of string triplets";
PyObject *BPy_EnumProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_Size(args) > 0) {
	 	PyErr_SetString(PyExc_ValueError, "all args must be keywors"); // TODO - py3 can enforce this.
		return NULL;
	}

	srna= srna_from_self(self);
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "items", "name", "description", "default", "hidden", NULL};
		char *id=NULL, *name="", *description="", *def="";
		int defvalue=0, hidden=0;
		PyObject *items= Py_None;
		EnumPropertyItem *eitems;
		PropertyRNA *prop;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|sssi:EnumProperty", kwlist, &id, &items, &name, &description, &def, &hidden))
			return NULL;

		eitems= enum_items_from_py(items, def, &defvalue);
		if(!eitems)
			return NULL;

		prop= RNA_def_enum(srna, id, eitems, defvalue, name, description);
		if(hidden) RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_duplicate_pointers(prop);
		MEM_freeN(eitems);

		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_EnumProperty, kw);
	}
}

static StructRNA *pointer_type_from_py(PyObject *value)
{
	StructRNA *srna;

	srna= srna_from_self(value);
	if(!srna) {
	 	PyErr_SetString(PyExc_SystemError, "expected an RNA type derived from IDPropertyGroup");
		return NULL;
	}

	if(!RNA_struct_is_a(srna, &RNA_IDPropertyGroup)) {
	 	PyErr_SetString(PyExc_SystemError, "expected an RNA type derived from IDPropertyGroup");
		return NULL;
	}

	return srna;
}

static char BPy_PointerProperty_doc[] =
".. function:: PointerProperty(items, type=\"\", description=\"\", default=\"\", hidden=False)\n"
"\n"
"   Returns a new pointer property definition.\n"
"\n"
"   :arg type: Dynamic type from :mod:`bpy.types`.\n"
"   :type type: class";
PyObject *BPy_PointerProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_Size(args) > 0) {
	 	PyErr_SetString(PyExc_ValueError, "all args must be keywors"); // TODO - py3 can enforce this.
		return NULL;
	}

	srna= srna_from_self(self);
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "type", "name", "description", "hidden", NULL};
		char *id=NULL, *name="", *description="";
		int hidden= 0;
		PropertyRNA *prop;
		StructRNA *ptype;
		PyObject *type= Py_None;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|ssi:PointerProperty", kwlist, &id, &type, &name, &description, &hidden))
			return NULL;

		ptype= pointer_type_from_py(type);
		if(!ptype)
			return NULL;

		prop= RNA_def_pointer_runtime(srna, id, ptype, name, description);
		if(hidden) RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_PointerProperty, kw);
	}
	return NULL;
}

static char BPy_CollectionProperty_doc[] =
".. function:: CollectionProperty(items, type=\"\", description=\"\", default=\"\", hidden=False)\n"
"\n"
"   Returns a new collection property definition.\n"
"\n"
"   :arg type: Dynamic type from :mod:`bpy.types`.\n"
"   :type type: class";
PyObject *BPy_CollectionProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_Size(args) > 0) {
	 	PyErr_SetString(PyExc_ValueError, "all args must be keywors"); // TODO - py3 can enforce this.
		return NULL;
	}

	srna= srna_from_self(self);
	if(srna==NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if(srna) {
		static char *kwlist[] = {"attr", "type", "name", "description", "hidden", NULL};
		char *id=NULL, *name="", *description="";
		int hidden= 0;
		PropertyRNA *prop;
		StructRNA *ptype;
		PyObject *type= Py_None;

		if (!PyArg_ParseTupleAndKeywords(args, kw, "sO|ssi:CollectionProperty", kwlist, &id, &type, &name, &description, &hidden))
			return NULL;

		ptype= pointer_type_from_py(type);
		if(!ptype)
			return NULL;

		prop= RNA_def_collection_runtime(srna, id, ptype, name, description);
		if(hidden) RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_duplicate_pointers(prop);
		Py_RETURN_NONE;
	}
	else { /* operators defer running this function */
		return bpy_prop_deferred_return((void *)BPy_CollectionProperty, kw);
	}
	return NULL;
}

static struct PyMethodDef props_methods[] = {
	{"BoolProperty", (PyCFunction)BPy_BoolProperty, METH_VARARGS|METH_KEYWORDS, BPy_BoolProperty_doc},
	{"IntProperty", (PyCFunction)BPy_IntProperty, METH_VARARGS|METH_KEYWORDS, BPy_IntProperty_doc},
	{"FloatProperty", (PyCFunction)BPy_FloatProperty, METH_VARARGS|METH_KEYWORDS, BPy_FloatProperty_doc},
	{"FloatVectorProperty", (PyCFunction)BPy_FloatVectorProperty, METH_VARARGS|METH_KEYWORDS, BPy_FloatVectorProperty_doc},
	{"StringProperty", (PyCFunction)BPy_StringProperty, METH_VARARGS|METH_KEYWORDS, BPy_StringProperty_doc},
	{"EnumProperty", (PyCFunction)BPy_EnumProperty, METH_VARARGS|METH_KEYWORDS, BPy_EnumProperty_doc},
	{"PointerProperty", (PyCFunction)BPy_PointerProperty, METH_VARARGS|METH_KEYWORDS, BPy_PointerProperty_doc},
	{"CollectionProperty", (PyCFunction)BPy_CollectionProperty, METH_VARARGS|METH_KEYWORDS, BPy_CollectionProperty_doc},
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
