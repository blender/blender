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

/** \file blender/python/intern/bpy_rna.c
 *  \ingroup pythonintern
 *
 * This file is the main interface between python and blenders data api (RNA),
 * exposing RNA to python so blender data can be accessed in a python like way.
 *
 * The two main types are 'BPy_StructRNA' and 'BPy_PropertyRNA' - the base
 * classes for most of the data python accesses in blender.
 */

#include <Python.h>

#include <stddef.h>
#include <float.h> /* FLT_MIN/MAX */

#include "RNA_types.h"

#include "bpy_rna.h"
#include "bpy_rna_anim.h"
#include "bpy_props.h"
#include "bpy_util.h"
#include "bpy_rna_callback.h"
#include "bpy_intern_string.h"

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
#include "MEM_guardedalloc.h"
#endif

#include "BLI_dynstr.h"
#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_utildefines.h"

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
#include "BLI_ghash.h"
#endif

#include "RNA_enum_types.h"
#include "RNA_define.h" /* RNA_def_property_free_identifier */
#include "RNA_access.h"

#include "MEM_guardedalloc.h"

#include "BKE_main.h"
#include "BKE_idcode.h"
#include "BKE_context.h"
#include "BKE_global.h" /* evil G.* */
#include "BKE_report.h"
#include "BKE_idprop.h"

#include "BKE_animsys.h"
#include "BKE_fcurve.h"

#include "../generic/idprop_py_api.h" /* for IDprop lookups */
#include "../generic/py_capi_utils.h"

#ifdef WITH_INTERNATIONAL
#include "BLF_translation.h"
#endif

#define USE_PEDANTIC_WRITE
#define USE_MATHUTILS
#define USE_STRING_COERCE

static PyObject* pyrna_struct_Subtype(PointerRNA *ptr);
static PyObject *pyrna_prop_collection_values(BPy_PropertyRNA *self);

#define BPY_DOC_ID_PROP_TYPE_NOTE                                             \
"   .. note::\n"                                                              \
"\n"                                                                          \
"      Only :class:`bpy.types.ID`, :class:`bpy.types.Bone` and \n"            \
"      :class:`bpy.types.PoseBone` classes support custom properties.\n"


int pyrna_struct_validity_check(BPy_StructRNA *pysrna)
{
	if (pysrna->ptr.type) {
		return 0;
	}
	PyErr_Format(PyExc_ReferenceError,
	             "StructRNA of type %.200s has been removed",
	             Py_TYPE(pysrna)->tp_name);
	return -1;
}

int pyrna_prop_validity_check(BPy_PropertyRNA *self)
{
	if (self->ptr.type) {
		return 0;
	}
	PyErr_Format(PyExc_ReferenceError,
	             "PropertyRNA of type %.200s.%.200s has been removed",
	             Py_TYPE(self)->tp_name, RNA_property_identifier(self->prop));
	return -1;
}

#if defined(USE_PYRNA_INVALIDATE_GC) || defined(USE_PYRNA_INVALIDATE_WEAKREF)
static void pyrna_invalidate(BPy_DummyPointerRNA *self)
{
	self->ptr.type = NULL; /* this is checked for validity */
	self->ptr.id.data = NULL; /* should not be needed but prevent bad pointer access, just incase */
}
#endif

#ifdef USE_PYRNA_INVALIDATE_GC
#define FROM_GC(g) ((PyObject *)(((PyGC_Head *)g) + 1))

/* only for sizeof() */
struct gc_generation {
	PyGC_Head head;
	int threshold;
	int count;
} gc_generation;

static void id_release_gc(struct ID *id)
{
	unsigned int j;
	// unsigned int i = 0;
	for (j = 0; j < 3; j++) {
		/* hack below to get the 2 other lists from _PyGC_generation0 that are normally not exposed */
		PyGC_Head *gen = (PyGC_Head *)(((char *)_PyGC_generation0) + (sizeof(gc_generation) * j));
		PyGC_Head *g = gen->gc.gc_next;
		while ((g = g->gc.gc_next) != gen) {
			PyObject *ob = FROM_GC(g);
			if (PyType_IsSubtype(Py_TYPE(ob), &pyrna_struct_Type) || PyType_IsSubtype(Py_TYPE(ob), &pyrna_prop_Type)) {
				BPy_DummyPointerRNA *ob_ptr = (BPy_DummyPointerRNA *)ob;
				if (ob_ptr->ptr.id.data == id) {
					pyrna_invalidate(ob_ptr);
					// printf("freeing: %p %s, %.200s\n", (void *)ob, id->name, Py_TYPE(ob)->tp_name);
					// i++;
				}
			}
		}
	}
	// printf("id_release_gc freed '%s': %d\n", id->name, i);
}
#endif

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
//#define DEBUG_RNA_WEAKREF

struct GHash *id_weakref_pool = NULL;
static PyObject *id_free_weakref_cb(PyObject *weakinfo_pair, PyObject *weakref);
static PyMethodDef id_free_weakref_cb_def = {"id_free_weakref_cb", (PyCFunction)id_free_weakref_cb, METH_O, NULL};

/* adds a reference to the list, remember to decref */
static GHash *id_weakref_pool_get(ID *id)
{
	GHash *weakinfo_hash = NULL;

	if (id_weakref_pool) {
		weakinfo_hash = BLI_ghash_lookup(id_weakref_pool, (void *)id);
	}
	else {
		/* first time, allocate pool */
		id_weakref_pool = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "rna_global_pool");
		weakinfo_hash = NULL;
	}

	if (weakinfo_hash == NULL) {
		/* we're using a ghash as a set, could use libHX's HXMAP_SINGULAR but would be an extra dep. */
		weakinfo_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "rna_id");
		BLI_ghash_insert(id_weakref_pool, (void *)id, weakinfo_hash);
	}

	return weakinfo_hash;
}

/* called from pyrna_struct_CreatePyObject() and pyrna_prop_CreatePyObject() */
void id_weakref_pool_add(ID *id, BPy_DummyPointerRNA *pyrna)
{
	PyObject *weakref;
	PyObject *weakref_capsule;
	PyObject *weakref_cb_py;

	/* create a new function instance and insert the list as 'self' so we can remove ourself from it */
	GHash *weakinfo_hash = id_weakref_pool_get(id); /* new or existing */

	weakref_capsule = PyCapsule_New(weakinfo_hash, NULL, NULL);
	weakref_cb_py = PyCFunction_New(&id_free_weakref_cb_def, weakref_capsule);
	Py_DECREF(weakref_capsule);

	/* add weakref to weakinfo_hash list */
	weakref = PyWeakref_NewRef((PyObject *)pyrna, weakref_cb_py);

	Py_DECREF(weakref_cb_py); /* function owned by the weakref now */

	/* important to add at the end, since first removal looks at the end */
	BLI_ghash_insert(weakinfo_hash, (void *)weakref, id); /* using a hash table as a set, all 'id's are the same */
	/* weakinfo_hash owns the weakref */

}

/* workaround to get the last id without a lookup */
static ID *_id_tmp_ptr;
static void value_id_set(void *id)
{
	_id_tmp_ptr = (ID *)id;
}

static void id_release_weakref_list(struct ID *id, GHash *weakinfo_hash);
static PyObject *id_free_weakref_cb(PyObject *weakinfo_capsule, PyObject *weakref)
{
	/* important to search backwards */
	GHash *weakinfo_hash = PyCapsule_GetPointer(weakinfo_capsule, NULL);


	if (BLI_ghash_size(weakinfo_hash) > 1) {
		BLI_ghash_remove(weakinfo_hash, weakref, NULL, NULL);
	}
	else { /* get the last id and free it */
		BLI_ghash_remove(weakinfo_hash, weakref, NULL, value_id_set);
		id_release_weakref_list(_id_tmp_ptr, weakinfo_hash);
	}

	Py_DECREF(weakref);

	Py_RETURN_NONE;
}

static void id_release_weakref_list(struct ID *id, GHash *weakinfo_hash)
{
	GHashIterator weakinfo_hash_iter;

	BLI_ghashIterator_init(&weakinfo_hash_iter, weakinfo_hash);

#ifdef DEBUG_RNA_WEAKREF
	fprintf(stdout, "id_release_weakref: '%s', %d items\n", id->name, BLI_ghash_size(weakinfo_hash));
#endif

	while (!BLI_ghashIterator_isDone(&weakinfo_hash_iter)) {
		PyObject *weakref = (PyObject *)BLI_ghashIterator_getKey(&weakinfo_hash_iter);
		PyObject *item = PyWeakref_GET_OBJECT(weakref);
		if (item != Py_None) {

#ifdef DEBUG_RNA_WEAKREF
			PyC_ObSpit("id_release_weakref item ", item);
#endif

			pyrna_invalidate((BPy_DummyPointerRNA *)item);
		}

		Py_DECREF(weakref);

		BLI_ghashIterator_step(&weakinfo_hash_iter);
	}

	BLI_ghash_remove(id_weakref_pool, (void *)id, NULL, NULL);
	BLI_ghash_free(weakinfo_hash, NULL, NULL);

	if (BLI_ghash_size(id_weakref_pool) == 0) {
		BLI_ghash_free(id_weakref_pool, NULL, NULL);
		id_weakref_pool = NULL;
#ifdef DEBUG_RNA_WEAKREF
		printf("id_release_weakref freeing pool\n");
#endif
	}
}

static void id_release_weakref(struct ID *id)
{
	GHash *weakinfo_hash = BLI_ghash_lookup(id_weakref_pool, (void *)id);
	if (weakinfo_hash) {
		id_release_weakref_list(id, weakinfo_hash);
	}
}

#endif /* USE_PYRNA_INVALIDATE_WEAKREF */

void BPY_id_release(struct ID *id)
{
#ifdef USE_PYRNA_INVALIDATE_GC
	id_release_gc(id);
#endif

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
	if (id_weakref_pool) {
		PyGILState_STATE gilstate = PyGILState_Ensure();

		id_release_weakref(id);

		PyGILState_Release(gilstate);
	}
#endif /* USE_PYRNA_INVALIDATE_WEAKREF */

	(void)id;
}

#ifdef USE_PEDANTIC_WRITE
static short rna_disallow_writes = FALSE;

static int rna_id_write_error(PointerRNA *ptr, PyObject *key)
{
	ID *id = ptr->id.data;
	if (id) {
		const short idcode = GS(id->name);
		if (!ELEM(idcode, ID_WM, ID_SCR)) { /* may need more added here */
			const char *idtype = BKE_idcode_to_name(idcode);
			const char *pyname;
			if (key && PyUnicode_Check(key)) pyname = _PyUnicode_AsString(key);
			else                             pyname = "<UNKNOWN>";

			/* make a nice string error */
			BLI_assert(idtype != NULL);
			PyErr_Format(PyExc_AttributeError,
			             "Writing to ID classes in this context is not allowed: "
			             "%.200s, %.200s datablock, error setting %.200s.%.200s",
			             id->name + 2, idtype, RNA_struct_identifier(ptr->type), pyname);

			return TRUE;
		}
	}
	return FALSE;
}
#endif // USE_PEDANTIC_WRITE


#ifdef USE_PEDANTIC_WRITE
int pyrna_write_check(void)
{
	return !rna_disallow_writes;
}

void pyrna_write_set(int val)
{
	rna_disallow_writes = !val;
}
#else // USE_PEDANTIC_WRITE
int pyrna_write_check(void)
{
	return TRUE;
}
void pyrna_write_set(int UNUSED(val))
{
	/* nothing */
}
#endif // USE_PEDANTIC_WRITE

static Py_ssize_t pyrna_prop_collection_length(BPy_PropertyRNA *self);
static Py_ssize_t pyrna_prop_array_length(BPy_PropertyArrayRNA *self);
static int pyrna_py_to_prop(PointerRNA *ptr, PropertyRNA *prop, void *data, PyObject *value, const char *error_prefix);
static int deferred_register_prop(StructRNA *srna, PyObject *key, PyObject *item);

#ifdef USE_MATHUTILS
#include "../mathutils/mathutils.h" /* so we can have mathutils callbacks */

static PyObject *pyrna_prop_array_subscript_slice(BPy_PropertyArrayRNA *self, PointerRNA *ptr, PropertyRNA *prop,
                                                  Py_ssize_t start, Py_ssize_t stop, Py_ssize_t length);
static short pyrna_rotation_euler_order_get(PointerRNA *ptr, PropertyRNA **prop_eul_order, short order_fallback);

/* bpyrna vector/euler/quat callbacks */
static int mathutils_rna_array_cb_index = -1; /* index for our callbacks */

/* subtype not used much yet */
#define MATHUTILS_CB_SUBTYPE_EUL 0
#define MATHUTILS_CB_SUBTYPE_VEC 1
#define MATHUTILS_CB_SUBTYPE_QUAT 2
#define MATHUTILS_CB_SUBTYPE_COLOR 3

static int mathutils_rna_generic_check(BaseMathObject *bmo)
{
	BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

	PYRNA_PROP_CHECK_INT(self);

	return self->prop ? 0 : -1;
}

static int mathutils_rna_vector_get(BaseMathObject *bmo, int subtype)
{
	BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

	PYRNA_PROP_CHECK_INT(self);

	if (self->prop == NULL)
		return -1;

	RNA_property_float_get_array(&self->ptr, self->prop, bmo->data);

	/* Euler order exception */
	if (subtype == MATHUTILS_CB_SUBTYPE_EUL) {
		EulerObject *eul = (EulerObject *)bmo;
		PropertyRNA *prop_eul_order = NULL;
		eul->order = pyrna_rotation_euler_order_get(&self->ptr, &prop_eul_order, eul->order);
	}

	return 0;
}

static int mathutils_rna_vector_set(BaseMathObject *bmo, int subtype)
{
	BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;
	float min, max;

	PYRNA_PROP_CHECK_INT(self);

	if (self->prop == NULL)
		return -1;

#ifdef USE_PEDANTIC_WRITE
	if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
		return -1;
	}
#endif // USE_PEDANTIC_WRITE

	if (!RNA_property_editable_flag(&self->ptr, self->prop)) {
		PyErr_Format(PyExc_AttributeError,
		             "bpy_prop \"%.200s.%.200s\" is read-only",
		             RNA_struct_identifier(self->ptr.type), RNA_property_identifier(self->prop));
		return -1;
	}

	RNA_property_float_range(&self->ptr, self->prop, &min, &max);

	if (min != FLT_MIN || max != FLT_MAX) {
		int i, len = RNA_property_array_length(&self->ptr, self->prop);
		for (i = 0; i < len; i++) {
			CLAMP(bmo->data[i], min, max);
		}
	}

	RNA_property_float_set_array(&self->ptr, self->prop, bmo->data);
	if (RNA_property_update_check(self->prop)) {
		RNA_property_update(BPy_GetContext(), &self->ptr, self->prop);
	}

	/* Euler order exception */
	if (subtype == MATHUTILS_CB_SUBTYPE_EUL) {
		EulerObject *eul = (EulerObject *)bmo;
		PropertyRNA *prop_eul_order = NULL;
		short order = pyrna_rotation_euler_order_get(&self->ptr, &prop_eul_order, eul->order);
		if (order != eul->order) {
			RNA_property_enum_set(&self->ptr, prop_eul_order, eul->order);
			if (RNA_property_update_check(prop_eul_order)) {
				RNA_property_update(BPy_GetContext(), &self->ptr, prop_eul_order);
			}
		}
	}
	return 0;
}

static int mathutils_rna_vector_get_index(BaseMathObject *bmo, int UNUSED(subtype), int index)
{
	BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

	PYRNA_PROP_CHECK_INT(self);

	if (self->prop == NULL)
		return -1;

	bmo->data[index] = RNA_property_float_get_index(&self->ptr, self->prop, index);
	return 0;
}

static int mathutils_rna_vector_set_index(BaseMathObject *bmo, int UNUSED(subtype), int index)
{
	BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

	PYRNA_PROP_CHECK_INT(self);

	if (self->prop == NULL)
		return -1;

#ifdef USE_PEDANTIC_WRITE
	if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
		return -1;
	}
#endif // USE_PEDANTIC_WRITE

	if (!RNA_property_editable_flag(&self->ptr, self->prop)) {
		PyErr_Format(PyExc_AttributeError,
		             "bpy_prop \"%.200s.%.200s\" is read-only",
		             RNA_struct_identifier(self->ptr.type), RNA_property_identifier(self->prop));
		return -1;
	}

	RNA_property_float_clamp(&self->ptr, self->prop, &bmo->data[index]);
	RNA_property_float_set_index(&self->ptr, self->prop, index, bmo->data[index]);

	if (RNA_property_update_check(self->prop)) {
		RNA_property_update(BPy_GetContext(), &self->ptr, self->prop);
	}

	return 0;
}

static Mathutils_Callback mathutils_rna_array_cb = {
	(BaseMathCheckFunc)		mathutils_rna_generic_check,
	(BaseMathGetFunc)		mathutils_rna_vector_get,
	(BaseMathSetFunc)		mathutils_rna_vector_set,
	(BaseMathGetIndexFunc)	mathutils_rna_vector_get_index,
	(BaseMathSetIndexFunc)	mathutils_rna_vector_set_index
};


/* bpyrna matrix callbacks */
static int mathutils_rna_matrix_cb_index = -1; /* index for our callbacks */

static int mathutils_rna_matrix_get(BaseMathObject *bmo, int UNUSED(subtype))
{
	BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

	PYRNA_PROP_CHECK_INT(self);

	if (self->prop == NULL)
		return -1;

	RNA_property_float_get_array(&self->ptr, self->prop, bmo->data);
	return 0;
}

static int mathutils_rna_matrix_set(BaseMathObject *bmo, int UNUSED(subtype))
{
	BPy_PropertyRNA *self = (BPy_PropertyRNA *)bmo->cb_user;

	PYRNA_PROP_CHECK_INT(self);

	if (self->prop == NULL)
		return -1;

#ifdef USE_PEDANTIC_WRITE
	if (rna_disallow_writes && rna_id_write_error(&self->ptr, NULL)) {
		return -1;
	}
#endif // USE_PEDANTIC_WRITE

	if (!RNA_property_editable_flag(&self->ptr, self->prop)) {
		PyErr_Format(PyExc_AttributeError,
		             "bpy_prop \"%.200s.%.200s\" is read-only",
		             RNA_struct_identifier(self->ptr.type), RNA_property_identifier(self->prop));
		return -1;
	}

	/* can ignore clamping here */
	RNA_property_float_set_array(&self->ptr, self->prop, bmo->data);

	if (RNA_property_update_check(self->prop)) {
		RNA_property_update(BPy_GetContext(), &self->ptr, self->prop);
	}
	return 0;
}

static Mathutils_Callback mathutils_rna_matrix_cb = {
	mathutils_rna_generic_check,
	mathutils_rna_matrix_get,
	mathutils_rna_matrix_set,
	NULL,
	NULL
};

static short pyrna_rotation_euler_order_get(PointerRNA *ptr, PropertyRNA **prop_eul_order, short order_fallback)
{
	/* attempt to get order */
	if (*prop_eul_order == NULL)
		*prop_eul_order = RNA_struct_find_property(ptr, "rotation_mode");

	if (*prop_eul_order) {
		short order = RNA_property_enum_get(ptr, *prop_eul_order);
		if (order >= EULER_ORDER_XYZ && order <= EULER_ORDER_ZYX) /* could be quat or axisangle */
			return order;
	}

	return order_fallback;
}

#endif // USE_MATHUTILS

/* note that PROP_NONE is included as a vector subtype. this is because its handy to
 * have x/y access to fcurve keyframes and other fixed size float arrays of length 2-4. */
#define PROP_ALL_VECTOR_SUBTYPES                                              \
		 PROP_COORDS:                                                         \
	case PROP_TRANSLATION:                                                    \
	case PROP_DIRECTION:                                                      \
	case PROP_VELOCITY:                                                       \
	case PROP_ACCELERATION:                                                   \
	case PROP_XYZ:                                                            \
	case PROP_XYZ_LENGTH                                                      \


PyObject *pyrna_math_object_from_array(PointerRNA *ptr, PropertyRNA *prop)
{
	PyObject *ret = NULL;

#ifdef USE_MATHUTILS
	int subtype, totdim;
	int len;
	int is_thick;
	const int flag = RNA_property_flag(prop);

	/* disallow dynamic sized arrays to be wrapped since the size could change
	 * to a size mathutils does not support */
	if ((RNA_property_type(prop) != PROP_FLOAT) || (flag & PROP_DYNAMIC))
		return NULL;

	len = RNA_property_array_length(ptr, prop);
	subtype = RNA_property_subtype(prop);
	totdim = RNA_property_array_dimension(ptr, prop, NULL);
	is_thick = (flag & PROP_THICK_WRAP);

	if (totdim == 1 || (totdim == 2 && subtype == PROP_MATRIX)) {
		if (!is_thick)
			ret = pyrna_prop_CreatePyObject(ptr, prop); /* owned by the mathutils PyObject */

		switch (subtype) {
		case PROP_ALL_VECTOR_SUBTYPES:
			if (len >= 2 && len <= 4) {
				if (is_thick) {
					ret = Vector_CreatePyObject(NULL, len, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((VectorObject *)ret)->vec);
				}
				else {
					PyObject *vec_cb = Vector_CreatePyObject_cb(ret, len, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_VEC);
					Py_DECREF(ret); /* the vector owns now */
					ret = vec_cb; /* return the vector instead */
				}
			}
			break;
		case PROP_MATRIX:
			if (len == 16) {
				if (is_thick) {
					ret = Matrix_CreatePyObject(NULL, 4, 4, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((MatrixObject *)ret)->matrix);
				}
				else {
					PyObject *mat_cb = Matrix_CreatePyObject_cb(ret, 4,4, mathutils_rna_matrix_cb_index, FALSE);
					Py_DECREF(ret); /* the matrix owns now */
					ret = mat_cb; /* return the matrix instead */
				}
			}
			else if (len == 9) {
				if (is_thick) {
					ret = Matrix_CreatePyObject(NULL, 3, 3, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((MatrixObject *)ret)->matrix);
				}
				else {
					PyObject *mat_cb = Matrix_CreatePyObject_cb(ret, 3,3, mathutils_rna_matrix_cb_index, FALSE);
					Py_DECREF(ret); /* the matrix owns now */
					ret = mat_cb; /* return the matrix instead */
				}
			}
			break;
		case PROP_EULER:
		case PROP_QUATERNION:
			if (len == 3) { /* euler */
				if (is_thick) {
					/* attempt to get order, only needed for thick types since wrapped with update via callbacks */
					PropertyRNA *prop_eul_order = NULL;
					short order = pyrna_rotation_euler_order_get(ptr, &prop_eul_order, EULER_ORDER_XYZ);

					ret = Euler_CreatePyObject(NULL, order, Py_NEW, NULL); // TODO, get order from RNA
					RNA_property_float_get_array(ptr, prop, ((EulerObject *)ret)->eul);
				}
				else {
					/* order will be updated from callback on use */
					PyObject *eul_cb = Euler_CreatePyObject_cb(ret, EULER_ORDER_XYZ, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_EUL); // TODO, get order from RNA
					Py_DECREF(ret); /* the euler owns now */
					ret = eul_cb; /* return the euler instead */
				}
			}
			else if (len == 4) {
				if (is_thick) {
					ret = Quaternion_CreatePyObject(NULL, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((QuaternionObject *)ret)->quat);
				}
				else {
					PyObject *quat_cb = Quaternion_CreatePyObject_cb(ret, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_QUAT);
					Py_DECREF(ret); /* the quat owns now */
					ret = quat_cb; /* return the quat instead */
				}
			}
			break;
		case PROP_COLOR:
		case PROP_COLOR_GAMMA:
			if (len == 3) { /* color */
				if (is_thick) {
					ret = Color_CreatePyObject(NULL, Py_NEW, NULL); // TODO, get order from RNA
					RNA_property_float_get_array(ptr, prop, ((ColorObject *)ret)->col);
				}
				else {
					PyObject *col_cb = Color_CreatePyObject_cb(ret, mathutils_rna_array_cb_index, MATHUTILS_CB_SUBTYPE_COLOR);
					Py_DECREF(ret); /* the color owns now */
					ret = col_cb; /* return the color instead */
				}
			}
		default:
			break;
		}
	}

	if (ret == NULL) {
		if (is_thick) {
			/* this is an array we cant reference (since its not thin wrappable)
			 * and cannot be coerced into a mathutils type, so return as a list */
			ret = pyrna_prop_array_subscript_slice(NULL, ptr, prop, 0, len, len);
		}
		else {
			ret = pyrna_prop_CreatePyObject(ptr, prop); /* owned by the mathutils PyObject */
		}
	}
#else // USE_MATHUTILS
	(void)ptr;
	(void)prop;
#endif // USE_MATHUTILS

	return ret;
}

/* same as RNA_enum_value_from_id but raises an exception */
int pyrna_enum_value_from_id(EnumPropertyItem *item, const char *identifier, int *value, const char *error_prefix)
{
	if (RNA_enum_value_from_id(item, identifier, value) == 0) {
		const char *enum_str = BPy_enum_as_string(item);
		PyErr_Format(PyExc_ValueError,
		             "%s: '%.200s' not found in (%s)",
		             error_prefix, identifier, enum_str);
		MEM_freeN((void *)enum_str);
		return -1;
	}

	return 0;
}

/* note on __cmp__:
 * checking the 'ptr->data' matches works in almost all cases,
 * however there are a few RNA properties that are fake sub-structs and
 * share the pointer with the parent, in those cases this happens 'a.b == a'
 * see: r43352 for example.
 *
 * So compare the 'ptr->type' as well to avoid this problem.
 * It's highly unlikely this would happen that 'ptr->data' and 'ptr->prop' would match,
 * but _not_ 'ptr->type' but include this check for completeness.
 * - campbell */

static int pyrna_struct_compare(BPy_StructRNA *a, BPy_StructRNA *b)
{
	return ( (a->ptr.data == b->ptr.data) &&
	         (a->ptr.type == b->ptr.type)) ? 0 : -1;
}

static int pyrna_prop_compare(BPy_PropertyRNA *a, BPy_PropertyRNA *b)
{
	return ( (a->prop == b->prop) &&
	         (a->ptr.data == b->ptr.data) &&
	         (a->ptr.type == b->ptr.type) ) ? 0 : -1;
}

static PyObject *pyrna_struct_richcmp(PyObject *a, PyObject *b, int op)
{
	PyObject *res;
	int ok = -1; /* zero is true */

	if (BPy_StructRNA_Check(a) && BPy_StructRNA_Check(b))
		ok = pyrna_struct_compare((BPy_StructRNA *)a, (BPy_StructRNA *)b);

	switch (op) {
	case Py_NE:
		ok = !ok; /* pass through */
	case Py_EQ:
		res = ok ? Py_False : Py_True;
		break;

	case Py_LT:
	case Py_LE:
	case Py_GT:
	case Py_GE:
		res = Py_NotImplemented;
		break;
	default:
		PyErr_BadArgument();
		return NULL;
	}

	return Py_INCREF(res), res;
}

static PyObject *pyrna_prop_richcmp(PyObject *a, PyObject *b, int op)
{
	PyObject *res;
	int ok = -1; /* zero is true */

	if (BPy_PropertyRNA_Check(a) && BPy_PropertyRNA_Check(b))
		ok = pyrna_prop_compare((BPy_PropertyRNA *)a, (BPy_PropertyRNA *)b);

	switch (op) {
	case Py_NE:
		ok = !ok; /* pass through */
	case Py_EQ:
		res = ok ? Py_False : Py_True;
		break;

	case Py_LT:
	case Py_LE:
	case Py_GT:
	case Py_GE:
		res = Py_NotImplemented;
		break;
	default:
		PyErr_BadArgument();
		return NULL;
	}

	return Py_INCREF(res), res;
}

/*----------------------repr--------------------------------------------*/
static PyObject *pyrna_struct_str(BPy_StructRNA *self)
{
	PyObject *ret;
	const char *name;

	if (!PYRNA_STRUCT_IS_VALID(self)) {
		return PyUnicode_FromFormat("<bpy_struct, %.200s dead>",
		                            Py_TYPE(self)->tp_name);
	}

	/* print name if available */
	name = RNA_struct_name_get_alloc(&self->ptr, NULL, 0, NULL);
	if (name) {
		ret = PyUnicode_FromFormat("<bpy_struct, %.200s(\"%.200s\")>",
		                          RNA_struct_identifier(self->ptr.type),
		                          name);
		MEM_freeN((void *)name);
		return ret;
	}

	return PyUnicode_FromFormat("<bpy_struct, %.200s at %p>",
	                            RNA_struct_identifier(self->ptr.type),
	                            self->ptr.data);
}

static PyObject *pyrna_struct_repr(BPy_StructRNA *self)
{
	ID *id = self->ptr.id.data;
	PyObject *tmp_str;
	PyObject *ret;

	if (id == NULL || !PYRNA_STRUCT_IS_VALID(self))
		return pyrna_struct_str(self); /* fallback */

	tmp_str = PyUnicode_FromString(id->name + 2);

	if (RNA_struct_is_ID(self->ptr.type)) {
		ret = PyUnicode_FromFormat("bpy.data.%s[%R]",
		                            BKE_idcode_to_name_plural(GS(id->name)),
		                            tmp_str);
	}
	else {
		const char *path;
		path = RNA_path_from_ID_to_struct(&self->ptr);
		if (path) {
			ret = PyUnicode_FromFormat("bpy.data.%s[%R].%s",
			                          BKE_idcode_to_name_plural(GS(id->name)),
			                          tmp_str,
			                          path);
			MEM_freeN((void *)path);
		}
		else { /* cant find, print something sane */
			ret = PyUnicode_FromFormat("bpy.data.%s[%R]...%s",
			                          BKE_idcode_to_name_plural(GS(id->name)),
			                          tmp_str,
			                          RNA_struct_identifier(self->ptr.type));
		}
	}

	Py_DECREF(tmp_str);

	return ret;
}

static PyObject *pyrna_prop_str(BPy_PropertyRNA *self)
{
	PyObject *ret;
	PointerRNA ptr;
	const char *name;
	const char *type_id = NULL;
	char type_fmt[64] = "";
	int type;

	PYRNA_PROP_CHECK_OBJ(self);

	type = RNA_property_type(self->prop);

	if (RNA_enum_id_from_value(property_type_items, type, &type_id) == 0) {
		PyErr_SetString(PyExc_RuntimeError, "could not use property type, internal error"); /* should never happen */
		return NULL;
	}
	else {
		/* this should never fail */
		int len = -1;
		char *c = type_fmt;

		while ((*c++= tolower(*type_id++))) {}

		if (type == PROP_COLLECTION) {
			len = pyrna_prop_collection_length(self);
		}
		else if (RNA_property_array_check(self->prop)) {
			len = pyrna_prop_array_length((BPy_PropertyArrayRNA *)self);
		}

		if (len != -1)
			sprintf(--c, "[%d]", len);
	}

	/* if a pointer, try to print name of pointer target too */
	if (type == PROP_POINTER) {
		ptr = RNA_property_pointer_get(&self->ptr, self->prop);
		name = RNA_struct_name_get_alloc(&ptr, NULL, 0, NULL);

		if (name) {
			ret = PyUnicode_FromFormat("<bpy_%.200s, %.200s.%.200s(\"%.200s\")>",
			                          type_fmt,
			                          RNA_struct_identifier(self->ptr.type),
			                          RNA_property_identifier(self->prop),
			                          name);
			MEM_freeN((void *)name);
			return ret;
		}
	}
	if (type == PROP_COLLECTION) {
		PointerRNA r_ptr;
		if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
			return PyUnicode_FromFormat("<bpy_%.200s, %.200s>",
			                            type_fmt,
			                            RNA_struct_identifier(r_ptr.type));
		}
	}

	return PyUnicode_FromFormat("<bpy_%.200s, %.200s.%.200s>",
	                            type_fmt,
	                            RNA_struct_identifier(self->ptr.type),
	                            RNA_property_identifier(self->prop));
}

static PyObject *pyrna_prop_repr(BPy_PropertyRNA *self)
{
	ID *id = self->ptr.id.data;
	PyObject *tmp_str;
	PyObject *ret;
	const char *path;

	PYRNA_PROP_CHECK_OBJ(self);

	if (id == NULL)
		return pyrna_prop_str(self); /* fallback */

	tmp_str = PyUnicode_FromString(id->name + 2);

	path = RNA_path_from_ID_to_property(&self->ptr, self->prop);
	if (path) {
		ret = PyUnicode_FromFormat("bpy.data.%s[%R].%s",
		                          BKE_idcode_to_name_plural(GS(id->name)),
		                          tmp_str,
		                          path);
		MEM_freeN((void *)path);
	}
	else { /* cant find, print something sane */
		ret = PyUnicode_FromFormat("bpy.data.%s[%R]...%s",
		                          BKE_idcode_to_name_plural(GS(id->name)),
		                          tmp_str,
		                          RNA_property_identifier(self->prop));
	}

	Py_DECREF(tmp_str);

	return ret;
}


static PyObject *pyrna_func_repr(BPy_FunctionRNA *self)
{
	return PyUnicode_FromFormat("<%.200s %.200s.%.200s()>",
	                            Py_TYPE(self)->tp_name,
	                            RNA_struct_identifier(self->ptr.type),
	                            RNA_function_identifier(self->func));
}


static Py_hash_t pyrna_struct_hash(BPy_StructRNA *self)
{
	return _Py_HashPointer(self->ptr.data);
}

/* from python's meth_hash v3.1.2 */
static long pyrna_prop_hash(BPy_PropertyRNA *self)
{
	long x, y;
	if (self->ptr.data == NULL)
		x = 0;
	else {
		x = _Py_HashPointer(self->ptr.data);
		if (x == -1)
			return -1;
	}
	y = _Py_HashPointer((void *)(self->prop));
	if (y == -1)
		return -1;
	x ^= y;
	if (x == -1)
		x = -2;
	return x;
}

#ifdef USE_PYRNA_STRUCT_REFERENCE
static int pyrna_struct_traverse(BPy_StructRNA *self, visitproc visit, void *arg)
{
	Py_VISIT(self->reference);
	return 0;
}

static int pyrna_struct_clear(BPy_StructRNA *self)
{
	Py_CLEAR(self->reference);
	return 0;
}
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

/* use our own dealloc so we can free a property if we use one */
static void pyrna_struct_dealloc(BPy_StructRNA *self)
{
#ifdef PYRNA_FREE_SUPPORT
	if (self->freeptr && self->ptr.data) {
		IDP_FreeProperty(self->ptr.data);
		MEM_freeN(self->ptr.data);
		self->ptr.data = NULL;
	}
#endif /* PYRNA_FREE_SUPPORT */

#ifdef USE_WEAKREFS
	if (self->in_weakreflist != NULL) {
		PyObject_ClearWeakRefs((PyObject *)self);
	}
#endif

#ifdef USE_PYRNA_STRUCT_REFERENCE
	if (self->reference) {
		PyObject_GC_UnTrack(self);
		pyrna_struct_clear(self);
	}
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

	/* Note, for subclassed PyObjects we cant just call PyObject_DEL() directly or it will crash */
	Py_TYPE(self)->tp_free(self);
}

#ifdef USE_PYRNA_STRUCT_REFERENCE
static void pyrna_struct_reference_set(BPy_StructRNA *self, PyObject *reference)
{
	if (self->reference) {
//		PyObject_GC_UnTrack(self); /* INITIALIZED TRACKED ? */
		pyrna_struct_clear(self);
	}
	/* reference is now NULL */

	if (reference) {
		self->reference = reference;
		Py_INCREF(reference);
//		PyObject_GC_Track(self);  /* INITIALIZED TRACKED ? */
	}
}
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

/* use our own dealloc so we can free a property if we use one */
static void pyrna_prop_dealloc(BPy_PropertyRNA *self)
{
#ifdef USE_WEAKREFS
	if (self->in_weakreflist != NULL) {
		PyObject_ClearWeakRefs((PyObject *)self);
	}
#endif
	/* Note, for subclassed PyObjects we cant just call PyObject_DEL() directly or it will crash */
	Py_TYPE(self)->tp_free(self);
}

static void pyrna_prop_array_dealloc(BPy_PropertyRNA *self)
{
#ifdef USE_WEAKREFS
	if (self->in_weakreflist != NULL) {
		PyObject_ClearWeakRefs((PyObject *)self);
	}
#endif
	/* Note, for subclassed PyObjects we cant just call PyObject_DEL() directly or it will crash */
	Py_TYPE(self)->tp_free(self);
}

static const char *pyrna_enum_as_string(PointerRNA *ptr, PropertyRNA *prop)
{
	EnumPropertyItem *item;
	const char *result;
	int free = FALSE;

	RNA_property_enum_items(BPy_GetContext(), ptr, prop, &item, NULL, &free);
	if (item) {
		result = BPy_enum_as_string(item);
	}
	else {
		result = "";
	}

	if (free)
		MEM_freeN(item);

	return result;
}


static int pyrna_string_to_enum(PyObject *item, PointerRNA *ptr, PropertyRNA *prop, int *val, const char *error_prefix)
{
	const char *param = _PyUnicode_AsString(item);

	if (param == NULL) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s expected a string enum, not %.200s",
		             error_prefix, Py_TYPE(item)->tp_name);
		return -1;
	}
	else {
		/* hack so that dynamic enums used for operator properties will be able to be built (i.e. context will be supplied to itemf)
		 * and thus running defining operator buttons for such operators in UI will work */
		RNA_def_property_clear_flag(prop, PROP_ENUM_NO_CONTEXT);

		if (!RNA_property_enum_value(BPy_GetContext(), ptr, prop, param, val)) {
			const char *enum_str = pyrna_enum_as_string(ptr, prop);
			PyErr_Format(PyExc_TypeError,
			             "%.200s enum \"%.200s\" not found in (%.200s)",
			             error_prefix, param, enum_str);
			MEM_freeN((void *)enum_str);
			return -1;
		}
	}

	return 0;
}

/* 'value' _must_ be a set type, error check before calling */
int pyrna_set_to_enum_bitfield(EnumPropertyItem *items, PyObject *value, int *r_value, const char *error_prefix)
{
	/* set of enum items, concatenate all values with OR */
	int ret, flag = 0;

	/* set looping */
	Py_ssize_t pos = 0;
	Py_ssize_t hash = 0;
	PyObject *key;

	*r_value = 0;

	while (_PySet_NextEntry(value, &pos, &key, &hash)) {
		const char *param = _PyUnicode_AsString(key);

		if (param == NULL) {
			PyErr_Format(PyExc_TypeError,
			             "%.200s expected a string, not %.200s",
			             error_prefix, Py_TYPE(key)->tp_name);
			return -1;
		}

		if (pyrna_enum_value_from_id(items, param, &ret, error_prefix) < 0) {
			return -1;
		}

		flag |= ret;
	}

	*r_value = flag;
	return 0;
}

static int pyrna_prop_to_enum_bitfield(PointerRNA *ptr, PropertyRNA *prop, PyObject *value, int *r_value, const char *error_prefix)
{
	EnumPropertyItem *item;
	int ret;
	int free = FALSE;

	*r_value = 0;

	if (!PyAnySet_Check(value)) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s, %.200s.%.200s expected a set, not a %.200s",
		             error_prefix, RNA_struct_identifier(ptr->type),
		             RNA_property_identifier(prop), Py_TYPE(value)->tp_name);
		return -1;
	}

	RNA_property_enum_items(BPy_GetContext(), ptr, prop, &item, NULL, &free);

	if (item) {
		ret = pyrna_set_to_enum_bitfield(item, value, r_value, error_prefix);
	}
	else {
		if (PySet_GET_SIZE(value)) {
			PyErr_Format(PyExc_TypeError,
			             "%.200s: empty enum \"%.200s\" could not have any values assigned",
			             error_prefix, RNA_property_identifier(prop));
			ret = -1;
		}
		else {
			ret = 0;
		}
	}

	if (free)
		MEM_freeN(item);

	return ret;
}

PyObject *pyrna_enum_bitfield_to_py(EnumPropertyItem *items, int value)
{
	PyObject *ret = PySet_New(NULL);
	const char *identifier[RNA_ENUM_BITFLAG_SIZE + 1];

	if (RNA_enum_bitflag_identifiers(items, value, identifier)) {
		PyObject *item;
		int index;
		for (index = 0; identifier[index]; index++) {
			item = PyUnicode_FromString(identifier[index]);
			PySet_Add(ret, item);
			Py_DECREF(item);
		}
	}

	return ret;
}

static PyObject *pyrna_enum_to_py(PointerRNA *ptr, PropertyRNA *prop, int val)
{
	PyObject *item, *ret = NULL;

	if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
		const char *identifier[RNA_ENUM_BITFLAG_SIZE + 1];

		ret = PySet_New(NULL);

		if (RNA_property_enum_bitflag_identifiers(BPy_GetContext(), ptr, prop, val, identifier)) {
			int index;

			for (index = 0; identifier[index]; index++) {
				item = PyUnicode_FromString(identifier[index]);
				PySet_Add(ret, item);
				Py_DECREF(item);
			}

		}
	}
	else {
		const char *identifier;
		if (RNA_property_enum_identifier(BPy_GetContext(), ptr, prop, val, &identifier)) {
			ret = PyUnicode_FromString(identifier);
		}
		else {
			EnumPropertyItem *enum_item;
			int free = FALSE;

			/* don't throw error here, can't trust blender 100% to give the
			 * right values, python code should not generate error for that */
			RNA_property_enum_items(BPy_GetContext(), ptr, prop, &enum_item, NULL, &free);
			if (enum_item && enum_item->identifier) {
				ret = PyUnicode_FromString(enum_item->identifier);
			}
			else {
				const char *ptr_name = RNA_struct_name_get_alloc(ptr, NULL, 0, NULL);

				/* prefer not fail silently incase of api errors, maybe disable it later */
				printf("RNA Warning: Current value \"%d\" "
					   "matches no enum in '%s', '%s', '%s'\n",
					   val, RNA_struct_identifier(ptr->type),
					   ptr_name, RNA_property_identifier(prop));

#if 0			// gives python decoding errors while generating docs :(
				char error_str[256];
				BLI_snprintf(error_str, sizeof(error_str),
							 "RNA Warning: Current value \"%d\" "
							 "matches no enum in '%s', '%s', '%s'",
							 val, RNA_struct_identifier(ptr->type),
							 ptr_name, RNA_property_identifier(prop));

				PyErr_Warn(PyExc_RuntimeWarning, error_str);
#endif

				if (ptr_name)
					MEM_freeN((void *)ptr_name);

				ret = PyUnicode_FromString("");
			}

			if (free)
				MEM_freeN(enum_item);
			/*
			PyErr_Format(PyExc_AttributeError,
			             "RNA Error: Current value \"%d\" matches no enum", val);
			ret = NULL;
			*/
		}
	}

	return ret;
}

PyObject *pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop)
{
	PyObject *ret;
	const int type = RNA_property_type(prop);

	if (RNA_property_array_check(prop)) {
		return pyrna_py_from_array(ptr, prop);
	}

	/* see if we can coorce into a python type - PropertyType */
	switch (type) {
	case PROP_BOOLEAN:
		ret = PyBool_FromLong(RNA_property_boolean_get(ptr, prop));
		break;
	case PROP_INT:
		ret = PyLong_FromSsize_t((Py_ssize_t)RNA_property_int_get(ptr, prop));
		break;
	case PROP_FLOAT:
		ret = PyFloat_FromDouble(RNA_property_float_get(ptr, prop));
		break;
	case PROP_STRING:
	{
		const int subtype = RNA_property_subtype(prop);
		const char *buf;
		int buf_len;
		char buf_fixed[32];

		buf = RNA_property_string_get_alloc(ptr, prop, buf_fixed, sizeof(buf_fixed), &buf_len);
#ifdef USE_STRING_COERCE
		/* only file paths get special treatment, they may contain non utf-8 chars */
		if (subtype == PROP_BYTESTRING) {
			ret = PyBytes_FromStringAndSize(buf, buf_len);
		}
		else if (ELEM3(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
			ret = PyC_UnicodeFromByteAndSize(buf, buf_len);
		}
		else {
			ret = PyUnicode_FromStringAndSize(buf, buf_len);
		}
#else // USE_STRING_COERCE
		if (subtype == PROP_BYTESTRING) {
			ret = PyBytes_FromStringAndSize(buf, buf_len);
		}
		else {
			ret = PyUnicode_FromStringAndSize(buf, buf_len);
		}
#endif // USE_STRING_COERCE
		if (buf_fixed != buf) {
			MEM_freeN((void *)buf);
		}
		break;
	}
	case PROP_ENUM:
	{
		ret = pyrna_enum_to_py(ptr, prop, RNA_property_enum_get(ptr, prop));
		break;
	}
	case PROP_POINTER:
	{
		PointerRNA newptr;
		newptr = RNA_property_pointer_get(ptr, prop);
		if (newptr.data) {
			ret = pyrna_struct_CreatePyObject(&newptr);
		}
		else {
			ret = Py_None;
			Py_INCREF(ret);
		}
		break;
	}
	case PROP_COLLECTION:
		ret = pyrna_prop_CreatePyObject(ptr, prop);
		break;
	default:
		PyErr_Format(PyExc_TypeError,
		             "bpy_struct internal error: unknown type '%d' (pyrna_prop_to_py)", type);
		ret = NULL;
		break;
	}

	return ret;
}

/* This function is used by operators and converting dicts into collections.
 * Its takes keyword args and fills them with property values */
int pyrna_pydict_to_props(PointerRNA *ptr, PyObject *kw, int all_args, const char *error_prefix)
{
	int error_val = 0;
	int totkw;
	const char *arg_name = NULL;
	PyObject *item;

	totkw = kw ? PyDict_Size(kw):0;

	RNA_STRUCT_BEGIN(ptr, prop) {
		arg_name = RNA_property_identifier(prop);

		if (strcmp(arg_name, "rna_type") == 0) continue;

		if (kw == NULL) {
			PyErr_Format(PyExc_TypeError,
			             "%.200s: no keywords, expected \"%.200s\"",
			             error_prefix, arg_name ? arg_name : "<UNKNOWN>");
			error_val = -1;
			break;
		}

		item = PyDict_GetItemString(kw, arg_name); /* wont set an error */

		if (item == NULL) {
			if (all_args) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s: keyword \"%.200s\" missing",
				             error_prefix, arg_name ? arg_name : "<UNKNOWN>");
				error_val = -1; /* pyrna_py_to_prop sets the error */
				break;
			}
		}
		else {
			if (pyrna_py_to_prop(ptr, prop, NULL, item, error_prefix)) {
				error_val = -1;
				break;
			}
			totkw--;
		}
	}
	RNA_STRUCT_END;

	if (error_val == 0 && totkw > 0) { /* some keywords were given that were not used :/ */
		PyObject *key, *value;
		Py_ssize_t pos = 0;

		while (PyDict_Next(kw, &pos, &key, &value)) {
			arg_name = _PyUnicode_AsString(key);
			if (RNA_struct_find_property(ptr, arg_name) == NULL) break;
			arg_name = NULL;
		}

		PyErr_Format(PyExc_TypeError,
		             "%.200s: keyword \"%.200s\" unrecognized",
		             error_prefix, arg_name ? arg_name : "<UNKNOWN>");
		error_val = -1;
	}

	return error_val;
}


static PyObject *pyrna_func_to_py(PointerRNA *ptr, FunctionRNA *func)
{
	BPy_FunctionRNA* pyfunc = (BPy_FunctionRNA *) PyObject_NEW(BPy_FunctionRNA, &pyrna_func_Type);
	pyfunc->ptr = *ptr;
	pyfunc->func = func;
	return (PyObject *)pyfunc;
}


static int pyrna_py_to_prop(PointerRNA *ptr, PropertyRNA *prop, void *data, PyObject *value, const char *error_prefix)
{
	/* XXX hard limits should be checked here */
	const int type = RNA_property_type(prop);


	if (RNA_property_array_check(prop)) {
		/* done getting the length */
		if (pyrna_py_to_array(ptr, prop, data, value, error_prefix) == -1) {
			return -1;
		}
	}
	else {
		/* Normal Property (not an array) */

		/* see if we can coorce into a python type - PropertyType */
		switch (type) {
		case PROP_BOOLEAN:
		{
			int param;
			/* prefer not to have an exception here
			 * however so many poll functions return None or a valid Object.
			 * its a hassle to convert these into a bool before returning, */
			if (RNA_property_flag(prop) & PROP_OUTPUT)
				param = PyObject_IsTrue(value);
			else
				param = PyLong_AsLong(value);

			if (param < 0) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s %.200s.%.200s expected True/False or 0/1, not %.200s",
				             error_prefix, RNA_struct_identifier(ptr->type),
				             RNA_property_identifier(prop), Py_TYPE(value)->tp_name);
				return -1;
			}
			else {
				if (data)  *((int *)data)= param;
				else       RNA_property_boolean_set(ptr, prop, param);
			}
			break;
		}
		case PROP_INT:
		{
			int overflow;
			long param = PyLong_AsLongAndOverflow(value, &overflow);
			if (overflow || (param > INT_MAX) || (param < INT_MIN)) {
				PyErr_Format(PyExc_ValueError,
				             "%.200s %.200s.%.200s value not in 'int' range "
				             "(" STRINGIFY(INT_MIN) ", " STRINGIFY(INT_MAX) ")",
				             error_prefix, RNA_struct_identifier(ptr->type),
				             RNA_property_identifier(prop));
				return -1;
			}
			else if (param == -1 && PyErr_Occurred()) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s %.200s.%.200s expected an int type, not %.200s",
				             error_prefix, RNA_struct_identifier(ptr->type),
				             RNA_property_identifier(prop), Py_TYPE(value)->tp_name);
				return -1;
			}
			else {
				int param_i = (int)param;
				RNA_property_int_clamp(ptr, prop, &param_i);
				if (data)  *((int *)data)= param_i;
				else       RNA_property_int_set(ptr, prop, param_i);
			}
			break;
		}
		case PROP_FLOAT:
		{
			float param = PyFloat_AsDouble(value);
			if (PyErr_Occurred()) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s %.200s.%.200s expected a float type, not %.200s",
				             error_prefix, RNA_struct_identifier(ptr->type),
				             RNA_property_identifier(prop), Py_TYPE(value)->tp_name);
				return -1;
			}
			else {
				RNA_property_float_clamp(ptr, prop, (float *)&param);
				if (data)   *((float *)data)= param;
				else        RNA_property_float_set(ptr, prop, param);
			}
			break;
		}
		case PROP_STRING:
		{
			const int subtype = RNA_property_subtype(prop);
			const char *param;

			if (subtype == PROP_BYTESTRING) {

				/* Byte String */

				param = PyBytes_AsString(value);

				if (param == NULL) {
					if (PyBytes_Check(value)) {
						/* there was an error assigning a string type,
						 * rather than setting a new error, prefix the existing one
						 */
						PyC_Err_Format_Prefix(PyExc_TypeError,
						                      "%.200s %.200s.%.200s error assigning bytes",
						                      error_prefix, RNA_struct_identifier(ptr->type),
						                      RNA_property_identifier(prop));
					}
					else {
						PyErr_Format(PyExc_TypeError,
						             "%.200s %.200s.%.200s expected a bytes type, not %.200s",
						             error_prefix, RNA_struct_identifier(ptr->type),
						             RNA_property_identifier(prop), Py_TYPE(value)->tp_name);
					}

					return -1;
				}
				else {
					/* same as unicode */
					if (data)   *((char **)data)= (char *)param; /*XXX, this is suspect but needed for function calls, need to see if theres a better way */
					else        RNA_property_string_set(ptr, prop, param);
				}
			}
			else {

				/* Unicode String */

#ifdef USE_STRING_COERCE
				PyObject *value_coerce = NULL;
				if (ELEM3(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
					/* TODO, get size */
					param = PyC_UnicodeAsByte(value, &value_coerce);
				}
				else {
					param = _PyUnicode_AsString(value);
#ifdef WITH_INTERNATIONAL
					if (subtype == PROP_TRANSLATE) {
						param = IFACE_(param);
					}
#endif // WITH_INTERNATIONAL

				}
#else // USE_STRING_COERCE
				param = _PyUnicode_AsString(value);
#endif // USE_STRING_COERCE

				if (param == NULL) {
					if (PyUnicode_Check(value)) {
						/* there was an error assigning a string type,
						 * rather than setting a new error, prefix the existing one
						 */
						PyC_Err_Format_Prefix(PyExc_TypeError,
						                      "%.200s %.200s.%.200s error assigning string",
						                      error_prefix, RNA_struct_identifier(ptr->type),
						                      RNA_property_identifier(prop));
					}
					else {
						PyErr_Format(PyExc_TypeError,
						             "%.200s %.200s.%.200s expected a string type, not %.200s",
						             error_prefix, RNA_struct_identifier(ptr->type),
						             RNA_property_identifier(prop), Py_TYPE(value)->tp_name);
					}

					return -1;
				}
				else {
					/* same as bytes */
					if (data)   *((char **)data)= (char *)param; /*XXX, this is suspect but needed for function calls, need to see if theres a better way */
					else        RNA_property_string_set(ptr, prop, param);
				}
#ifdef USE_STRING_COERCE
				Py_XDECREF(value_coerce);
#endif // USE_STRING_COERCE
			}
			break;
		}
		case PROP_ENUM:
		{
			int val = 0;

			/* type checkins is done by each function */
			if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
				/* set of enum items, concatenate all values with OR */
				if (pyrna_prop_to_enum_bitfield(ptr, prop, value, &val, error_prefix) < 0) {
					return -1;
				}
			}
			else {
				/* simple enum string */
				if (pyrna_string_to_enum(value, ptr, prop, &val, error_prefix) < 0) {
					return -1;
				}
			}

			if (data)  *((int *)data)= val;
			else       RNA_property_enum_set(ptr, prop, val);

			break;
		}
		case PROP_POINTER:
		{
			PyObject *value_new = NULL;

			StructRNA *ptr_type = RNA_property_pointer_type(ptr, prop);
			int flag = RNA_property_flag(prop);

			/* this is really nasty!, so we can fake the operator having direct properties eg:
			 * layout.prop(self, "filepath")
			 * ... which infact should be
			 * layout.prop(self.properties, "filepath")
			 *
			 * we need to do this trick.
			 * if the prop is not an operator type and the pyobject is an operator,
			 * use its properties in place of its self.
			 *
			 * this is so bad that its almost a good reason to do away with fake 'self.properties -> self' class mixing
			 * if this causes problems in the future it should be removed.
			 */
			if ((ptr_type == &RNA_AnyType) &&
				(BPy_StructRNA_Check(value)) &&
				(RNA_struct_is_a(((BPy_StructRNA *)value)->ptr.type, &RNA_Operator))
			) {
				value = PyObject_GetAttrString(value, "properties");
				value_new = value;
			}


			/* if property is an OperatorProperties pointer and value is a map,
			 * forward back to pyrna_pydict_to_props */
			if (RNA_struct_is_a(ptr_type, &RNA_OperatorProperties) && PyDict_Check(value)) {
				PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
				return pyrna_pydict_to_props(&opptr, value, 0, error_prefix);
			}

			/* another exception, allow to pass a collection as an RNA property */
			if (Py_TYPE(value) == &pyrna_prop_collection_Type) { /* ok to ignore idprop collections */
				PointerRNA c_ptr;
				BPy_PropertyRNA *value_prop = (BPy_PropertyRNA *)value;
				if (RNA_property_collection_type_get(&value_prop->ptr, value_prop->prop, &c_ptr)) {
					value = pyrna_struct_CreatePyObject(&c_ptr);
					value_new = value;
				}
				else {
					PyErr_Format(PyExc_TypeError,
					             "%.200s %.200s.%.200s collection has no type, "
					             "cant be used as a %.200s type",
					             error_prefix, RNA_struct_identifier(ptr->type),
					             RNA_property_identifier(prop), RNA_struct_identifier(ptr_type));
					return -1;
				}
			}

			if (!BPy_StructRNA_Check(value) && value != Py_None) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s %.200s.%.200s expected a %.200s type, not %.200s",
				             error_prefix, RNA_struct_identifier(ptr->type),
				             RNA_property_identifier(prop), RNA_struct_identifier(ptr_type),
				             Py_TYPE(value)->tp_name);
				Py_XDECREF(value_new); return -1;
			}
			else if ((flag & PROP_NEVER_NULL) && value == Py_None) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s %.200s.%.200s does not support a 'None' assignment %.200s type",
				             error_prefix, RNA_struct_identifier(ptr->type),
				             RNA_property_identifier(prop), RNA_struct_identifier(ptr_type));
				Py_XDECREF(value_new); return -1;
			}
			else if ((value != Py_None) &&
			         ((flag & PROP_ID_SELF_CHECK) && ptr->id.data == ((BPy_StructRNA *)value)->ptr.id.data))
			{
				PyErr_Format(PyExc_TypeError,
				             "%.200s %.200s.%.200s ID type does not support assignment to its self",
				             error_prefix, RNA_struct_identifier(ptr->type),
				             RNA_property_identifier(prop));
				Py_XDECREF(value_new); return -1;
			}
			else {
				BPy_StructRNA *param = (BPy_StructRNA *)value;
				int raise_error = FALSE;
				if (data) {

					if (flag & PROP_RNAPTR) {
						if (value == Py_None)
							memset(data, 0, sizeof(PointerRNA));
						else
							*((PointerRNA *)data)= param->ptr;
					}
					else if (value == Py_None) {
						*((void **)data)= NULL;
					}
					else if (RNA_struct_is_a(param->ptr.type, ptr_type)) {
						*((void **)data)= param->ptr.data;
					}
					else {
						raise_error = TRUE;
					}
				}
				else {
					/* data == NULL, assign to RNA */
					if (value == Py_None) {
						PointerRNA valueptr = {{NULL}};
						RNA_property_pointer_set(ptr, prop, valueptr);
					}
					else if (RNA_struct_is_a(param->ptr.type, ptr_type)) {
						RNA_property_pointer_set(ptr, prop, param->ptr);
					}
					else {
						PointerRNA tmp;
						RNA_pointer_create(NULL, ptr_type, NULL, &tmp);
						PyErr_Format(PyExc_TypeError,
						             "%.200s %.200s.%.200s expected a %.200s type. not %.200s",
						             error_prefix, RNA_struct_identifier(ptr->type),
						             RNA_property_identifier(prop), RNA_struct_identifier(tmp.type),
						             RNA_struct_identifier(param->ptr.type));
						Py_XDECREF(value_new); return -1;
					}
				}

				if (raise_error) {
					PointerRNA tmp;
					RNA_pointer_create(NULL, ptr_type, NULL, &tmp);
					PyErr_Format(PyExc_TypeError,
					             "%.200s %.200s.%.200s expected a %.200s type, not %.200s",
					             error_prefix, RNA_struct_identifier(ptr->type),
					             RNA_property_identifier(prop), RNA_struct_identifier(tmp.type),
					             RNA_struct_identifier(param->ptr.type));
					Py_XDECREF(value_new); return -1;
				}
			}

			Py_XDECREF(value_new);

			break;
		}
		case PROP_COLLECTION:
		{
			Py_ssize_t seq_len, i;
			PyObject *item;
			PointerRNA itemptr;
			ListBase *lb;
			CollectionPointerLink *link;

			lb = (data) ? (ListBase *)data : NULL;

			/* convert a sequence of dict's into a collection */
			if (!PySequence_Check(value)) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s %.200s.%.200s expected a sequence for an RNA collection, not %.200s",
				             error_prefix, RNA_struct_identifier(ptr->type),
				             RNA_property_identifier(prop), Py_TYPE(value)->tp_name);
				return -1;
			}

			seq_len = PySequence_Size(value);
			for (i = 0; i < seq_len; i++) {
				item = PySequence_GetItem(value, i);

				if (item == NULL) {
					PyErr_Format(PyExc_TypeError,
					             "%.200s %.200s.%.200s failed to get sequence index '%d' for an RNA collection",
					             error_prefix, RNA_struct_identifier(ptr->type),
					             RNA_property_identifier(prop), i);
					Py_XDECREF(item);
					return -1;
				}

				if (PyDict_Check(item) == 0) {
					PyErr_Format(PyExc_TypeError,
					             "%.200s %.200s.%.200s expected a each sequence "
					             "member to be a dict for an RNA collection, not %.200s",
					             error_prefix, RNA_struct_identifier(ptr->type),
					             RNA_property_identifier(prop), Py_TYPE(item)->tp_name);
					Py_XDECREF(item);
					return -1;
				}

				if (lb) {
					link = MEM_callocN(sizeof(CollectionPointerLink), "PyCollectionPointerLink");
					link->ptr = itemptr;
					BLI_addtail(lb, link);
				}
				else
					RNA_property_collection_add(ptr, prop, &itemptr);

				if (pyrna_pydict_to_props(&itemptr, item, 1, "Converting a python list to an RNA collection") == -1) {
					PyObject *msg = PyC_ExceptionBuffer();
					const char *msg_char = _PyUnicode_AsString(msg);

					PyErr_Format(PyExc_TypeError,
					             "%.200s %.200s.%.200s error converting a member of a collection "
					             "from a dicts into an RNA collection, failed with: %s",
					             error_prefix, RNA_struct_identifier(ptr->type),
					             RNA_property_identifier(prop), msg_char);

					Py_DECREF(item);
					Py_DECREF(msg);
					return -1;
				}
				Py_DECREF(item);
			}

			break;
		}
		default:
			PyErr_Format(PyExc_AttributeError,
			             "%.200s %.200s.%.200s unknown property type (pyrna_py_to_prop)",
			             error_prefix, RNA_struct_identifier(ptr->type),
			             RNA_property_identifier(prop));
			return -1;
			break;
		}
	}

	/* Run rna property functions */
	if (RNA_property_update_check(prop)) {
		RNA_property_update(BPy_GetContext(), ptr, prop);
	}

	return 0;
}

static PyObject *pyrna_prop_array_to_py_index(BPy_PropertyArrayRNA *self, int index)
{
	PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);
	return pyrna_py_from_array_index(self, &self->ptr, self->prop, index);
}

static int pyrna_py_to_prop_array_index(BPy_PropertyArrayRNA *self, int index, PyObject *value)
{
	int ret = 0;
	PointerRNA *ptr = &self->ptr;
	PropertyRNA *prop = self->prop;

	const int totdim = RNA_property_array_dimension(ptr, prop, NULL);

	if (totdim > 1) {
		/* char error_str[512]; */
		if (pyrna_py_to_array_index(&self->ptr, self->prop, self->arraydim, self->arrayoffset, index, value, "") == -1) {
			/* error is set */
			ret = -1;
		}
	}
	else {
		/* see if we can coerce into a python type - PropertyType */
		switch (RNA_property_type(prop)) {
		case PROP_BOOLEAN:
			{
				int param = PyLong_AsLong(value);

				if (param < 0 || param > 1) {
					PyErr_SetString(PyExc_TypeError, "expected True/False or 0/1");
					ret = -1;
				}
				else {
					RNA_property_boolean_set_index(ptr, prop, index, param);
				}
				break;
			}
		case PROP_INT:
			{
				int param = PyLong_AsLong(value);
				if (param == -1 && PyErr_Occurred()) {
					PyErr_SetString(PyExc_TypeError, "expected an int type");
					ret = -1;
				}
				else {
					RNA_property_int_clamp(ptr, prop, &param);
					RNA_property_int_set_index(ptr, prop, index, param);
				}
				break;
			}
		case PROP_FLOAT:
			{
				float param = PyFloat_AsDouble(value);
				if (PyErr_Occurred()) {
					PyErr_SetString(PyExc_TypeError, "expected a float type");
					ret = -1;
				}
				else {
					RNA_property_float_clamp(ptr, prop, &param);
					RNA_property_float_set_index(ptr, prop, index, param);
				}
				break;
			}
		default:
			PyErr_SetString(PyExc_AttributeError, "not an array type");
			ret = -1;
			break;
		}
	}

	/* Run rna property functions */
	if (RNA_property_update_check(prop)) {
		RNA_property_update(BPy_GetContext(), ptr, prop);
	}

	return ret;
}

//---------------sequence-------------------------------------------
static Py_ssize_t pyrna_prop_array_length(BPy_PropertyArrayRNA *self)
{
	PYRNA_PROP_CHECK_INT((BPy_PropertyRNA *)self);

	if (RNA_property_array_dimension(&self->ptr, self->prop, NULL) > 1)
		return RNA_property_multi_array_length(&self->ptr, self->prop, self->arraydim);
	else
		return RNA_property_array_length(&self->ptr, self->prop);
}

static Py_ssize_t pyrna_prop_collection_length(BPy_PropertyRNA *self)
{
	PYRNA_PROP_CHECK_INT(self);

	return RNA_property_collection_length(&self->ptr, self->prop);
}

/* bool functions are for speed, so we can avoid getting the length
 * of 1000's of items in a linked list for eg. */
static int pyrna_prop_array_bool(BPy_PropertyRNA *self)
{
	PYRNA_PROP_CHECK_INT(self);

	return RNA_property_array_length(&self->ptr, self->prop) ? 1 : 0;
}

static int pyrna_prop_collection_bool(BPy_PropertyRNA *self)
{
	/* no callback defined, just iterate and find the nth item */
	CollectionPropertyIterator iter;
	int test;

	PYRNA_PROP_CHECK_INT(self);

	RNA_property_collection_begin(&self->ptr, self->prop, &iter);
	test = iter.valid;
	RNA_property_collection_end(&iter);
	return test;
}


/* notice getting the length of the collection is avoided unless negative
 * index is used or to detect internal error with a valid index.
 * This is done for faster lookups. */
#define PYRNA_PROP_COLLECTION_ABS_INDEX(ret_err)                              \
	if (keynum < 0) {                                                         \
		keynum_abs += RNA_property_collection_length(&self->ptr, self->prop); \
		if (keynum_abs < 0) {                                                 \
			PyErr_Format(PyExc_IndexError,                                    \
			             "bpy_prop_collection[%d]: out of range.", keynum);   \
			return ret_err;                                                   \
		}                                                                     \
	}                                                                         \


/* internal use only */
static PyObject *pyrna_prop_collection_subscript_int(BPy_PropertyRNA *self, Py_ssize_t keynum)
{
	PointerRNA newptr;
	Py_ssize_t keynum_abs = keynum;

	PYRNA_PROP_CHECK_OBJ(self);

	PYRNA_PROP_COLLECTION_ABS_INDEX(NULL);

	if (RNA_property_collection_lookup_int(&self->ptr, self->prop, keynum_abs, &newptr)) {
		return pyrna_struct_CreatePyObject(&newptr);
	}
	else {
		const int len = RNA_property_collection_length(&self->ptr, self->prop);
		if (keynum_abs >= len) {
			PyErr_Format(PyExc_IndexError,
			             "bpy_prop_collection[index]: "
			             "index %d out of range, size %d", keynum, len);
		}
		else {
			PyErr_Format(PyExc_RuntimeError,
			             "bpy_prop_collection[index]: internal error, "
			             "valid index %d given in %d sized collection but value not found",
			             keynum_abs, len);
		}

		return NULL;
	}
}

/* values type must have been already checked */
static int pyrna_prop_collection_ass_subscript_int(BPy_PropertyRNA *self, Py_ssize_t keynum, PyObject *value)
{
	Py_ssize_t keynum_abs = keynum;
	const PointerRNA *ptr = (value == Py_None) ? (&PointerRNA_NULL) : &((BPy_StructRNA *)value)->ptr;

	PYRNA_PROP_CHECK_INT(self);

	PYRNA_PROP_COLLECTION_ABS_INDEX(-1);

	if (RNA_property_collection_assign_int(&self->ptr, self->prop, keynum_abs, ptr) == 0) {
		const int len = RNA_property_collection_length(&self->ptr, self->prop);
		if (keynum_abs >= len) {
			PyErr_Format(PyExc_IndexError,
			             "bpy_prop_collection[index] = value: "
			             "index %d out of range, size %d", keynum, len);
		}
		else {

			PyErr_Format(PyExc_IndexError,
			             "bpy_prop_collection[index] = value: "
			             "failed assignment (unknown reason)", keynum);
		}
		return -1;
	}

	return 0;
}

static PyObject *pyrna_prop_array_subscript_int(BPy_PropertyArrayRNA *self, int keynum)
{
	int len;

	PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

	len = pyrna_prop_array_length(self);

	if (keynum < 0) keynum += len;

	if (keynum >= 0 && keynum < len)
		return pyrna_prop_array_to_py_index(self, keynum);

	PyErr_Format(PyExc_IndexError,
	             "bpy_prop_array[index]: index %d out of range", keynum);
	return NULL;
}

static PyObject *pyrna_prop_collection_subscript_str(BPy_PropertyRNA *self, const char *keyname)
{
	PointerRNA newptr;

	PYRNA_PROP_CHECK_OBJ(self);

	if (RNA_property_collection_lookup_string(&self->ptr, self->prop, keyname, &newptr))
		return pyrna_struct_CreatePyObject(&newptr);

	PyErr_Format(PyExc_KeyError, "bpy_prop_collection[key]: key \"%.200s\" not found", keyname);
	return NULL;
}
/* static PyObject *pyrna_prop_array_subscript_str(BPy_PropertyRNA *self, char *keyname) */

/* special case: bpy.data.objects["some_id_name", "//some_lib_name.blend"]
 * also for:     bpy.data.objects.get(("some_id_name", "//some_lib_name.blend"), fallback)
 *
 * note:
 * error codes since this is not to be called directly from python,
 * this matches pythons __contains__ values capi.
 * -1: exception set
 *  0: not found
 *  1: found */
int pyrna_prop_collection_subscript_str_lib_pair_ptr(BPy_PropertyRNA *self, PyObject *key,
                                                     const char *err_prefix, const short err_not_found,
                                                     PointerRNA *r_ptr
                                                     )
{
	char *keyname;

	/* first validate the args, all we know is that they are a tuple */
	if (PyTuple_GET_SIZE(key) != 2) {
		PyErr_Format(PyExc_KeyError,
		             "%s: tuple key must be a pair, not size %d",
		             err_prefix, PyTuple_GET_SIZE(key));
		return -1;
	}
	else if (self->ptr.type != &RNA_BlendData) {
		PyErr_Format(PyExc_KeyError,
		             "%s: is only valid for bpy.data collections, not %.200s",
		             err_prefix, RNA_struct_identifier(self->ptr.type));
		return -1;
	}
	else if ((keyname = _PyUnicode_AsString(PyTuple_GET_ITEM(key, 0))) == NULL) {
		PyErr_Format(PyExc_KeyError,
		             "%s: id must be a string, not %.200s",
		             err_prefix, Py_TYPE(PyTuple_GET_ITEM(key, 0))->tp_name);
		return -1;
	}
	else {
		PyObject *keylib = PyTuple_GET_ITEM(key, 1);
		Library *lib;
		int found = FALSE;

		if (keylib == Py_None) {
			lib = NULL;
		}
		else if (PyUnicode_Check(keylib)) {
			Main *bmain = self->ptr.data;
			const char *keylib_str = _PyUnicode_AsString(keylib);
			lib = BLI_findstring(&bmain->library, keylib_str, offsetof(Library, name));
			if (lib == NULL) {
				if (err_not_found) {
					PyErr_Format(PyExc_KeyError,
								 "%s: lib name '%.240s' "
								 "does not reference a valid library",
								 err_prefix, keylib_str);
					return -1;
				}
				else {
					return 0;
				}

			}
		}
		else {
			PyErr_Format(PyExc_KeyError,
			             "%s: lib must be a sting or None, not %.200s",
			             err_prefix, Py_TYPE(keylib)->tp_name);
			return -1;
		}

		/* lib is either a valid poniter or NULL,
		 * either way can do direct comparison with id.lib */

		RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
			ID *id = itemptr.data; /* always an ID */
			if (id->lib == lib && (strncmp(keyname, id->name + 2, sizeof(id->name) - 2) == 0)) {
				found = TRUE;
				if (r_ptr) {
					*r_ptr = itemptr;
				}
				break;
			}
		}
		RNA_PROP_END;

		/* we may want to fail silently as with collection.get() */
		if ((found == FALSE) && err_not_found) {
			/* only runs for getitem access so use fixed string */
			PyErr_SetString(PyExc_KeyError,
			                "bpy_prop_collection[key, lib]: not found");
			return -1;
		}
		else {
			return found; /* 1 / 0, no exception */
		}
	}
}

static PyObject *pyrna_prop_collection_subscript_str_lib_pair(BPy_PropertyRNA *self, PyObject *key,
                                                              const char *err_prefix, const short err_not_found)
{
	PointerRNA ptr;
	const int contains = pyrna_prop_collection_subscript_str_lib_pair_ptr(self, key, err_prefix, err_not_found, &ptr);

	if (contains == 1) {
		return pyrna_struct_CreatePyObject(&ptr);
	}
	else {
		return NULL;
	}
}


static PyObject *pyrna_prop_collection_subscript_slice(BPy_PropertyRNA *self, Py_ssize_t start, Py_ssize_t stop)
{
	CollectionPropertyIterator rna_macro_iter;
	int count = 0;

	PyObject *list;
	PyObject *item;

	PYRNA_PROP_CHECK_OBJ(self);

	list = PyList_New(0);

	/* first loop up-until the start */
	for (RNA_property_collection_begin(&self->ptr, self->prop, &rna_macro_iter);
	     rna_macro_iter.valid;
	     RNA_property_collection_next(&rna_macro_iter))
	{
		/* PointerRNA itemptr = rna_macro_iter.ptr; */
		if (count == start) {
			break;
		}
		count++;
	}

	/* add items until stop */
	for (; rna_macro_iter.valid;
	     RNA_property_collection_next(&rna_macro_iter))
	{
		item = pyrna_struct_CreatePyObject(&rna_macro_iter.ptr);
		PyList_Append(list, item);
		Py_DECREF(item);

		count++;
		if (count == stop) {
			break;
		}
	}

	RNA_property_collection_end(&rna_macro_iter);

	return list;
}

/* TODO - dimensions
 * note: could also use pyrna_prop_array_to_py_index(self, count) in a loop but its a lot slower
 * since at the moment it reads (and even allocates) the entire array for each index.
 */
static PyObject *pyrna_prop_array_subscript_slice(BPy_PropertyArrayRNA *self, PointerRNA *ptr, PropertyRNA *prop,
                                                  Py_ssize_t start, Py_ssize_t stop, Py_ssize_t length)
{
	int count, totdim;
	PyObject *tuple;

	PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

	tuple = PyTuple_New(stop - start);

	/* PYRNA_PROP_CHECK_OBJ(self); isn't needed, internal use only */

	totdim = RNA_property_array_dimension(ptr, prop, NULL);

	if (totdim > 1) {
		for (count = start; count < stop; count++)
			PyTuple_SET_ITEM(tuple, count - start, pyrna_prop_array_to_py_index(self, count));
	}
	else {
		switch (RNA_property_type(prop)) {
		case PROP_FLOAT:
			{
				float values_stack[PYRNA_STACK_ARRAY];
				float *values;
				if (length > PYRNA_STACK_ARRAY) { values = PyMem_MALLOC(sizeof(float) * length); }
				else                            { values = values_stack; }
				RNA_property_float_get_array(ptr, prop, values);

				for (count = start; count < stop; count++)
					PyTuple_SET_ITEM(tuple, count-start, PyFloat_FromDouble(values[count]));

				if (values != values_stack) {
					PyMem_FREE(values);
				}
				break;
			}
		case PROP_BOOLEAN:
			{
				int values_stack[PYRNA_STACK_ARRAY];
				int *values;
				if (length > PYRNA_STACK_ARRAY)	{ values = PyMem_MALLOC(sizeof(int) * length); }
				else                            { values = values_stack; }

				RNA_property_boolean_get_array(ptr, prop, values);
				for (count = start; count < stop; count++)
					PyTuple_SET_ITEM(tuple, count-start, PyBool_FromLong(values[count]));

				if (values != values_stack) {
					PyMem_FREE(values);
				}
				break;
			}
		case PROP_INT:
			{
				int values_stack[PYRNA_STACK_ARRAY];
				int *values;
				if (length > PYRNA_STACK_ARRAY) { values = PyMem_MALLOC(sizeof(int) * length); }
				else                            { values = values_stack; }

				RNA_property_int_get_array(ptr, prop, values);
				for (count = start; count < stop; count++)
					PyTuple_SET_ITEM(tuple, count-start, PyLong_FromSsize_t(values[count]));

				if (values != values_stack) {
					PyMem_FREE(values);
				}
				break;
			}
		default:
			BLI_assert(!"Invalid array type");

			PyErr_SetString(PyExc_TypeError, "not an array type");
			Py_DECREF(tuple);
			tuple = NULL;
		}
	}
	return tuple;
}

static PyObject *pyrna_prop_collection_subscript(BPy_PropertyRNA *self, PyObject *key)
{
	PYRNA_PROP_CHECK_OBJ(self);

	if (PyUnicode_Check(key)) {
		return pyrna_prop_collection_subscript_str(self, _PyUnicode_AsString(key));
	}
	else if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;

		return pyrna_prop_collection_subscript_int(self, i);
	}
	else if (PySlice_Check(key)) {
		PySliceObject *key_slice = (PySliceObject *)key;
		Py_ssize_t step = 1;

		if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
			return NULL;
		}
		else if (step != 1) {
			PyErr_SetString(PyExc_TypeError, "bpy_prop_collection[slice]: slice steps not supported");
			return NULL;
		}
		else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
			return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
		}
		else {
			Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

			/* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
			if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start)) return NULL;
			if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop))    return NULL;

			if (start < 0 || stop < 0) {
				/* only get the length for negative values */
				Py_ssize_t len = (Py_ssize_t)RNA_property_collection_length(&self->ptr, self->prop);
				if (start < 0) start += len;
				if (stop < 0) start += len;
			}

			if (stop - start <= 0) {
				return PyList_New(0);
			}
			else {
				return pyrna_prop_collection_subscript_slice(self, start, stop);
			}
		}
	}
	else if (PyTuple_Check(key)) {
		/* special case, for ID datablocks we */
		return pyrna_prop_collection_subscript_str_lib_pair(self, key,
		                                                    "bpy_prop_collection[id, lib]", TRUE);
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "bpy_prop_collection[key]: invalid key, "
		             "must be a string or an int, not %.200s",
		             Py_TYPE(key)->tp_name);
		return NULL;
	}
}

/* generic check to see if a PyObject is compatible with a collection
 * -1 on failure, 0 on success, sets the error */
static int pyrna_prop_collection_type_check(BPy_PropertyRNA *self, PyObject *value)
{
	StructRNA *prop_srna;

	if (value == Py_None) {
		if (RNA_property_flag(self->prop) & PROP_NEVER_NULL) {
			PyErr_Format(PyExc_TypeError,
			             "bpy_prop_collection[key] = value: invalid, "
			             "this collection doesnt support None assignment");
			return -1;
		}
		else {
			return 0; /* None is OK */
		}
	}
	else if (BPy_StructRNA_Check(value) == 0) {
		PyErr_Format(PyExc_TypeError,
		             "bpy_prop_collection[key] = value: invalid, "
		             "expected a StructRNA type or None, not a %.200s",
		             Py_TYPE(value)->tp_name);
		return -1;
	}
	else if ((prop_srna = RNA_property_pointer_type(&self->ptr, self->prop))) {
		StructRNA *value_srna = ((BPy_StructRNA *)value)->ptr.type;
		if (RNA_struct_is_a(value_srna, prop_srna) == 0) {
			PyErr_Format(PyExc_TypeError,
			             "bpy_prop_collection[key] = value: invalid, "
			             "expected a '%.200s' type or None, not a '%.200s'",
			             RNA_struct_identifier(prop_srna),
			             RNA_struct_identifier(value_srna)
			             );
			return -1;
		}
		else {
			return 0; /* OK, this is the correct type!*/
		}
	}

	PyErr_Format(PyExc_TypeError,
	             "bpy_prop_collection[key] = value: internal error, "
	             "failed to get the collection type");
	return -1;
}

/* note: currently this is a copy of 'pyrna_prop_collection_subscript' with
 * large blocks commented, we may support slice/key indices later */
static int pyrna_prop_collection_ass_subscript(BPy_PropertyRNA *self, PyObject *key, PyObject *value)
{
	PYRNA_PROP_CHECK_INT(self);

	/* validate the assigned value */
	if (value == NULL) {
		PyErr_SetString(PyExc_TypeError,
		                "del bpy_prop_collection[key]: not supported");
		return -1;
	}
	else if (pyrna_prop_collection_type_check(self, value) == -1) {
		return -1; /* exception is set */
	}

#if 0
	if (PyUnicode_Check(key)) {
		return pyrna_prop_collection_subscript_str(self, _PyUnicode_AsString(key));
	}
	else
#endif
	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;

		return pyrna_prop_collection_ass_subscript_int(self, i, value);
	}
#if 0 /* TODO, fake slice assignment */
	else if (PySlice_Check(key)) {
		PySliceObject *key_slice = (PySliceObject *)key;
		Py_ssize_t step = 1;

		if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
			return NULL;
		}
		else if (step != 1) {
			PyErr_SetString(PyExc_TypeError, "bpy_prop_collection[slice]: slice steps not supported");
			return NULL;
		}
		else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
			return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
		}
		else {
			Py_ssize_t start = 0, stop = PY_SSIZE_T_MAX;

			/* avoid PySlice_GetIndicesEx because it needs to know the length ahead of time. */
			if (key_slice->start != Py_None && !_PyEval_SliceIndex(key_slice->start, &start))	return NULL;
			if (key_slice->stop != Py_None && !_PyEval_SliceIndex(key_slice->stop, &stop))		return NULL;

			if (start < 0 || stop < 0) {
				/* only get the length for negative values */
				Py_ssize_t len = (Py_ssize_t)RNA_property_collection_length(&self->ptr, self->prop);
				if (start < 0) start += len;
				if (stop < 0) start += len;
			}

			if (stop - start <= 0) {
				return PyList_New(0);
			}
			else {
				return pyrna_prop_collection_subscript_slice(self, start, stop);
			}
		}
	}
#endif
	else {
		PyErr_Format(PyExc_TypeError,
		             "bpy_prop_collection[key]: invalid key, "
		             "must be a string or an int, not %.200s",
		             Py_TYPE(key)->tp_name);
		return -1;
	}
}

static PyObject *pyrna_prop_array_subscript(BPy_PropertyArrayRNA *self, PyObject *key)
{
	PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

	/*if (PyUnicode_Check(key)) {
		return pyrna_prop_array_subscript_str(self, _PyUnicode_AsString(key));
	}
	else */
	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return pyrna_prop_array_subscript_int(self, PyLong_AsLong(key));
	}
	else if (PySlice_Check(key)) {
		Py_ssize_t step = 1;
		PySliceObject *key_slice = (PySliceObject *)key;

		if (key_slice->step != Py_None && !_PyEval_SliceIndex(key, &step)) {
			return NULL;
		}
		else if (step != 1) {
			PyErr_SetString(PyExc_TypeError, "bpy_prop_array[slice]: slice steps not supported");
			return NULL;
		}
		else if (key_slice->start == Py_None && key_slice->stop == Py_None) {
			/* note, no significant advantage with optimizing [:] slice as with collections
			 * but include here for consistency with collection slice func */
			Py_ssize_t len = (Py_ssize_t)pyrna_prop_array_length(self);
			return pyrna_prop_array_subscript_slice(self, &self->ptr, self->prop, 0, len, len);
		}
		else {
			int len = pyrna_prop_array_length(self);
			Py_ssize_t start, stop, slicelength;

			if (PySlice_GetIndicesEx((void *)key, len, &start, &stop, &step, &slicelength) < 0)
				return NULL;

			if (slicelength <= 0) {
				return PyTuple_New(0);
			}
			else {
				return pyrna_prop_array_subscript_slice(self, &self->ptr, self->prop, start, stop, len);
			}
		}
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "bpy_prop_array[key]: invalid key, key must be an int");
		return NULL;
	}
}

/* could call (pyrna_py_to_prop_array_index(self, i, value) in a loop but it is slow */
static int prop_subscript_ass_array_slice(PointerRNA *ptr, PropertyRNA *prop,
                                          int start, int stop, int length, PyObject *value_orig)
{
	PyObject *value;
	int count;
	void *values_alloc = NULL;
	int ret = 0;

	if (value_orig == NULL) {
		PyErr_SetString(PyExc_TypeError,
		                "bpy_prop_array[slice] = value: deleting with list types is not supported by bpy_struct");
		return -1;
	}

	if (!(value = PySequence_Fast(value_orig, "bpy_prop_array[slice] = value: assignment is not a sequence type"))) {
		return -1;
	}

	if (PySequence_Fast_GET_SIZE(value) != stop-start) {
		Py_DECREF(value);
		PyErr_SetString(PyExc_TypeError,
		                "bpy_prop_array[slice] = value: resizing bpy_struct arrays isn't supported");
		return -1;
	}

	switch (RNA_property_type(prop)) {
		case PROP_FLOAT:
		{
			float values_stack[PYRNA_STACK_ARRAY];
			float *values, fval;

			float min, max;
			RNA_property_float_range(ptr, prop, &min, &max);

			if (length > PYRNA_STACK_ARRAY) { values = values_alloc = PyMem_MALLOC(sizeof(float) * length); }
			else                            { values = values_stack; }
			if (start != 0 || stop != length) /* partial assignment? - need to get the array */
				RNA_property_float_get_array(ptr, prop, values);

			for (count = start; count < stop; count++) {
				fval = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, count-start));
				CLAMP(fval, min, max);
				values[count] = fval;
			}

			if (PyErr_Occurred()) ret = -1;
			else                  RNA_property_float_set_array(ptr, prop, values);
			break;
		}
		case PROP_BOOLEAN:
		{
			int values_stack[PYRNA_STACK_ARRAY];
			int *values;
			if (length > PYRNA_STACK_ARRAY) { values = values_alloc = PyMem_MALLOC(sizeof(int) * length); }
			else                            { values = values_stack; }

			if (start != 0 || stop != length) /* partial assignment? - need to get the array */
				RNA_property_boolean_get_array(ptr, prop, values);

			for (count = start; count < stop; count++)
				values[count] = PyLong_AsLong(PySequence_Fast_GET_ITEM(value, count-start));

			if (PyErr_Occurred()) ret = -1;
			else                  RNA_property_boolean_set_array(ptr, prop, values);
			break;
		}
		case PROP_INT:
		{
			int values_stack[PYRNA_STACK_ARRAY];
			int *values, ival;

			int min, max;
			RNA_property_int_range(ptr, prop, &min, &max);

			if (length > PYRNA_STACK_ARRAY)	{ values = values_alloc = PyMem_MALLOC(sizeof(int) * length); }
			else                            { values = values_stack; }

			if (start != 0 || stop != length) /* partial assignment? - need to get the array */
				RNA_property_int_get_array(ptr, prop, values);

			for (count = start; count < stop; count++) {
				ival = PyLong_AsLong(PySequence_Fast_GET_ITEM(value, count-start));
				CLAMP(ival, min, max);
				values[count] = ival;
			}

			if (PyErr_Occurred()) ret = -1;
			else                  RNA_property_int_set_array(ptr, prop, values);
			break;
		}
		default:
			PyErr_SetString(PyExc_TypeError, "not an array type");
			ret = -1;
	}

	Py_DECREF(value);

	if (values_alloc) {
		PyMem_FREE(values_alloc);
	}

	return ret;

}

static int prop_subscript_ass_array_int(BPy_PropertyArrayRNA *self, Py_ssize_t keynum, PyObject *value)
{
	int len;

	PYRNA_PROP_CHECK_INT((BPy_PropertyRNA *)self);

	len = pyrna_prop_array_length(self);

	if (keynum < 0) keynum += len;

	if (keynum >= 0 && keynum < len)
		return pyrna_py_to_prop_array_index(self, keynum, value);

	PyErr_SetString(PyExc_IndexError,
	                "bpy_prop_array[index] = value: index out of range");
	return -1;
}

static int pyrna_prop_array_ass_subscript(BPy_PropertyArrayRNA *self, PyObject *key, PyObject *value)
{
	/* char *keyname = NULL; */ /* not supported yet */
	int ret = -1;

	PYRNA_PROP_CHECK_INT((BPy_PropertyRNA *)self);

	if (!RNA_property_editable_flag(&self->ptr, self->prop)) {
		PyErr_Format(PyExc_AttributeError,
		             "bpy_prop_collection: attribute \"%.200s\" from \"%.200s\" is read-only",
		             RNA_property_identifier(self->prop), RNA_struct_identifier(self->ptr.type));
		ret = -1;
	}

	else if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred()) {
			ret = -1;
		}
		else {
			ret = prop_subscript_ass_array_int(self, i, value);
		}
	}
	else if (PySlice_Check(key)) {
		int len = RNA_property_array_length(&self->ptr, self->prop);
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((void *)key, len, &start, &stop, &step, &slicelength) < 0) {
			ret = -1;
		}
		else if (slicelength <= 0) {
			ret = 0; /* do nothing */
		}
		else if (step == 1) {
			ret = prop_subscript_ass_array_slice(&self->ptr, self->prop, start, stop, len, value);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with rna");
			ret = -1;
		}
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "invalid key, key must be an int");
		ret = -1;
	}

	if (ret != -1) {
		if (RNA_property_update_check(self->prop)) {
			RNA_property_update(BPy_GetContext(), &self->ptr, self->prop);
		}
	}

	return ret;
}

/* for slice only */
static PyMappingMethods pyrna_prop_array_as_mapping = {
	(lenfunc) pyrna_prop_array_length,	/* mp_length */
	(binaryfunc) pyrna_prop_array_subscript,	/* mp_subscript */
	(objobjargproc) pyrna_prop_array_ass_subscript,	/* mp_ass_subscript */
};

static PyMappingMethods pyrna_prop_collection_as_mapping = {
	(lenfunc) pyrna_prop_collection_length,	/* mp_length */
	(binaryfunc) pyrna_prop_collection_subscript,	/* mp_subscript */
	(objobjargproc) pyrna_prop_collection_ass_subscript,	/* mp_ass_subscript */
};

/* only for fast bool's, large structs, assign nb_bool on init */
static PyNumberMethods pyrna_prop_array_as_number = {
	NULL, /* nb_add */
	NULL, /* nb_subtract */
	NULL, /* nb_multiply */
	NULL, /* nb_remainder */
	NULL, /* nb_divmod */
	NULL, /* nb_power */
	NULL, /* nb_negative */
	NULL, /* nb_positive */
	NULL, /* nb_absolute */
	(inquiry) pyrna_prop_array_bool, /* nb_bool */
};
static PyNumberMethods pyrna_prop_collection_as_number = {
	NULL, /* nb_add */
	NULL, /* nb_subtract */
	NULL, /* nb_multiply */
	NULL, /* nb_remainder */
	NULL, /* nb_divmod */
	NULL, /* nb_power */
	NULL, /* nb_negative */
	NULL, /* nb_positive */
	NULL, /* nb_absolute */
	(inquiry) pyrna_prop_collection_bool, /* nb_bool */
};

static int pyrna_prop_array_contains(BPy_PropertyRNA *self, PyObject *value)
{
	return pyrna_array_contains_py(&self->ptr, self->prop, value);
}

static int pyrna_prop_collection_contains(BPy_PropertyRNA *self, PyObject *key)
{
	PointerRNA newptr; /* not used, just so RNA_property_collection_lookup_string runs */

	if (PyTuple_Check(key)) {
		/* special case, for ID datablocks we */
		return pyrna_prop_collection_subscript_str_lib_pair_ptr(self, key,
		                                                        "(id, lib) in bpy_prop_collection", FALSE, NULL);
	}
	else {

		/* key in dict style check */
		const char *keyname = _PyUnicode_AsString(key);

		if (keyname == NULL) {
			PyErr_SetString(PyExc_TypeError,
			                "bpy_prop_collection.__contains__: expected a string or a tuple of strings");
			return -1;
		}

		if (RNA_property_collection_lookup_string(&self->ptr, self->prop, keyname, &newptr))
			return 1;

		return 0;
	}
}

static int pyrna_struct_contains(BPy_StructRNA *self, PyObject *value)
{
	IDProperty *group;
	const char *name = _PyUnicode_AsString(value);

	PYRNA_STRUCT_CHECK_INT(self);

	if (!name) {
		PyErr_SetString(PyExc_TypeError, "bpy_struct.__contains__: expected a string");
		return -1;
	}

	if (RNA_struct_idprops_check(self->ptr.type) == 0) {
		PyErr_SetString(PyExc_TypeError, "bpy_struct: this type doesn't support IDProperties");
		return -1;
	}

	group = RNA_struct_idprops(&self->ptr, 0);

	if (!group)
		return 0;

	return IDP_GetPropertyFromGroup(group, name) ? 1:0;
}

static PySequenceMethods pyrna_prop_array_as_sequence = {
	(lenfunc)pyrna_prop_array_length,		/* Cant set the len otherwise it can evaluate as false */
	NULL,		/* sq_concat */
	NULL,		/* sq_repeat */
	(ssizeargfunc)pyrna_prop_array_subscript_int, /* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,		/* sq_slice */
	(ssizeobjargproc)prop_subscript_ass_array_int,		/* sq_ass_item */
	NULL,		/* *was* sq_ass_slice */
	(objobjproc)pyrna_prop_array_contains,	/* sq_contains */
	(binaryfunc) NULL, /* sq_inplace_concat */
	(ssizeargfunc) NULL, /* sq_inplace_repeat */
};

static PySequenceMethods pyrna_prop_collection_as_sequence = {
	(lenfunc)pyrna_prop_collection_length,		/* Cant set the len otherwise it can evaluate as false */
	NULL,		/* sq_concat */
	NULL,		/* sq_repeat */
	(ssizeargfunc)pyrna_prop_collection_subscript_int, /* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,		/* *was* sq_slice */
	(ssizeobjargproc)/* pyrna_prop_collection_ass_subscript_int */ NULL /* let mapping take this one */, /* sq_ass_item */
	NULL,		/* *was* sq_ass_slice */
	(objobjproc)pyrna_prop_collection_contains,	/* sq_contains */
	(binaryfunc) NULL, /* sq_inplace_concat */
	(ssizeargfunc) NULL, /* sq_inplace_repeat */
};

static PySequenceMethods pyrna_struct_as_sequence = {
	NULL,		/* Cant set the len otherwise it can evaluate as false */
	NULL,		/* sq_concat */
	NULL,		/* sq_repeat */
	NULL,		/* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,		/* *was* sq_slice */
	NULL,		/* sq_ass_item */
	NULL,		/* *was* sq_ass_slice */
	(objobjproc)pyrna_struct_contains,	/* sq_contains */
	(binaryfunc) NULL, /* sq_inplace_concat */
	(ssizeargfunc) NULL, /* sq_inplace_repeat */
};

static PyObject *pyrna_struct_subscript(BPy_StructRNA *self, PyObject *key)
{
	/* mostly copied from BPy_IDGroup_Map_GetItem */
	IDProperty *group, *idprop;
	const char *name = _PyUnicode_AsString(key);

	PYRNA_STRUCT_CHECK_OBJ(self);

	if (RNA_struct_idprops_check(self->ptr.type) == 0) {
		PyErr_SetString(PyExc_TypeError, "this type doesn't support IDProperties");
		return NULL;
	}

	if (name == NULL) {
		PyErr_SetString(PyExc_TypeError, "bpy_struct[key]: only strings are allowed as keys of ID properties");
		return NULL;
	}

	group = RNA_struct_idprops(&self->ptr, 0);

	if (group == NULL) {
		PyErr_Format(PyExc_KeyError, "bpy_struct[key]: key \"%s\" not found", name);
		return NULL;
	}

	idprop = IDP_GetPropertyFromGroup(group, name);

	if (idprop == NULL) {
		PyErr_Format(PyExc_KeyError, "bpy_struct[key]: key \"%s\" not found", name);
		return NULL;
	}

	return BPy_IDGroup_WrapData(self->ptr.id.data, idprop, group);
}

static int pyrna_struct_ass_subscript(BPy_StructRNA *self, PyObject *key, PyObject *value)
{
	IDProperty *group;

	PYRNA_STRUCT_CHECK_INT(self);

	group = RNA_struct_idprops(&self->ptr, 1);

#ifdef USE_PEDANTIC_WRITE
	if (rna_disallow_writes && rna_id_write_error(&self->ptr, key)) {
		return -1;
	}
#endif // USE_PEDANTIC_WRITE

	if (group == NULL) {
		PyErr_SetString(PyExc_TypeError, "bpy_struct[key] = val: id properties not supported for this type");
		return -1;
	}

	return BPy_Wrap_SetMapItem(group, key, value);
}

static PyMappingMethods pyrna_struct_as_mapping = {
	(lenfunc) NULL,	/* mp_length */
	(binaryfunc) pyrna_struct_subscript,	/* mp_subscript */
	(objobjargproc) pyrna_struct_ass_subscript,	/* mp_ass_subscript */
};

PyDoc_STRVAR(pyrna_struct_keys_doc,
".. method:: keys()\n"
"\n"
"   Returns the keys of this objects custom properties (matches pythons\n"
"   dictionary function of the same name).\n"
"\n"
"   :return: custom property keys.\n"
"   :rtype: list of strings\n"
"\n"
BPY_DOC_ID_PROP_TYPE_NOTE
);
static PyObject *pyrna_struct_keys(BPy_PropertyRNA *self)
{
	IDProperty *group;

	if (RNA_struct_idprops_check(self->ptr.type) == 0) {
		PyErr_SetString(PyExc_TypeError, "bpy_struct.keys(): this type doesn't support IDProperties");
		return NULL;
	}

	group = RNA_struct_idprops(&self->ptr, 0);

	if (group == NULL)
		return PyList_New(0);

	return BPy_Wrap_GetKeys(group);
}

PyDoc_STRVAR(pyrna_struct_items_doc,
".. method:: items()\n"
"\n"
"   Returns the items of this objects custom properties (matches pythons\n"
"   dictionary function of the same name).\n"
"\n"
"   :return: custom property key, value pairs.\n"
"   :rtype: list of key, value tuples\n"
"\n"
BPY_DOC_ID_PROP_TYPE_NOTE
);
static PyObject *pyrna_struct_items(BPy_PropertyRNA *self)
{
	IDProperty *group;

	if (RNA_struct_idprops_check(self->ptr.type) == 0) {
		PyErr_SetString(PyExc_TypeError, "bpy_struct.items(): this type doesn't support IDProperties");
		return NULL;
	}

	group = RNA_struct_idprops(&self->ptr, 0);

	if (group == NULL)
		return PyList_New(0);

	return BPy_Wrap_GetItems(self->ptr.id.data, group);
}

PyDoc_STRVAR(pyrna_struct_values_doc,
".. method:: values()\n"
"\n"
"   Returns the values of this objects custom properties (matches pythons\n"
"   dictionary function of the same name).\n"
"\n"
"   :return: custom property values.\n"
"   :rtype: list\n"
"\n"
BPY_DOC_ID_PROP_TYPE_NOTE
);
static PyObject *pyrna_struct_values(BPy_PropertyRNA *self)
{
	IDProperty *group;

	if (RNA_struct_idprops_check(self->ptr.type) == 0) {
		PyErr_SetString(PyExc_TypeError, "bpy_struct.values(): this type doesn't support IDProperties");
		return NULL;
	}

	group = RNA_struct_idprops(&self->ptr, 0);

	if (group == NULL)
		return PyList_New(0);

	return BPy_Wrap_GetValues(self->ptr.id.data, group);
}


PyDoc_STRVAR(pyrna_struct_is_property_set_doc,
".. method:: is_property_set(property)\n"
"\n"
"   Check if a property is set, use for testing operator properties.\n"
"\n"
"   :return: True when the property has been set.\n"
"   :rtype: boolean\n"
);
static PyObject *pyrna_struct_is_property_set(BPy_StructRNA *self, PyObject *args)
{
	PropertyRNA *prop;
	const char *name;

	PYRNA_STRUCT_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "s:is_property_set", &name))
		return NULL;

	if ((prop = RNA_struct_find_property(&self->ptr, name)) == NULL) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s.is_property_set(\"%.200s\") not found",
		             RNA_struct_identifier(self->ptr.type), name);
		return NULL;
	}

	return PyBool_FromLong(RNA_property_is_set(&self->ptr, prop));
}

PyDoc_STRVAR(pyrna_struct_is_property_hidden_doc,
".. method:: is_property_hidden(property)\n"
"\n"
"   Check if a property is hidden.\n"
"\n"
"   :return: True when the property is hidden.\n"
"   :rtype: boolean\n"
);
static PyObject *pyrna_struct_is_property_hidden(BPy_StructRNA *self, PyObject *args)
{
	PropertyRNA *prop;
	const char *name;

	PYRNA_STRUCT_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "s:is_property_hidden", &name))
		return NULL;

	if ((prop = RNA_struct_find_property(&self->ptr, name)) == NULL) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s.is_property_hidden(\"%.200s\") not found",
		             RNA_struct_identifier(self->ptr.type), name);
		return NULL;
	}

	return PyBool_FromLong(RNA_property_flag(prop) & PROP_HIDDEN);
}

PyDoc_STRVAR(pyrna_struct_path_resolve_doc,
".. method:: path_resolve(path, coerce=True)\n"
"\n"
"   Returns the property from the path, raise an exception when not found.\n"
"\n"
"   :arg path: path which this property resolves.\n"
"   :type path: string\n"
"   :arg coerce: optional argument, when True, the property will be converted\n"
"      into its python representation.\n"
"   :type coerce: boolean\n"
);
static PyObject *pyrna_struct_path_resolve(BPy_StructRNA *self, PyObject *args)
{
	const char *path;
	PyObject *coerce = Py_True;
	PointerRNA r_ptr;
	PropertyRNA *r_prop;
	int index = -1;

	PYRNA_STRUCT_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "s|O!:path_resolve", &path, &PyBool_Type, &coerce))
		return NULL;

	if (RNA_path_resolve_full(&self->ptr, path, &r_ptr, &r_prop, &index)) {
		if (r_prop) {
			if (index != -1) {
				if (index >= RNA_property_array_length(&r_ptr, r_prop) || index < 0) {
					PyErr_Format(PyExc_IndexError,
					             "%.200s.path_resolve(\"%.200s\") index out of range",
					             RNA_struct_identifier(self->ptr.type), path);
					return NULL;
				}
				else {
					return pyrna_array_index(&r_ptr, r_prop, index);
				}
			}
			else {
				if (coerce == Py_False) {
					return pyrna_prop_CreatePyObject(&r_ptr, r_prop);
				}
				else {
					return pyrna_prop_to_py(&r_ptr, r_prop);
				}
			}
		}
		else {
			return pyrna_struct_CreatePyObject(&r_ptr);
		}
	}
	else {
		PyErr_Format(PyExc_ValueError,
		             "%.200s.path_resolve(\"%.200s\") could not be resolved",
		             RNA_struct_identifier(self->ptr.type), path);
		return NULL;
	}
}

PyDoc_STRVAR(pyrna_struct_path_from_id_doc,
".. method:: path_from_id(property=\"\")\n"
"\n"
"   Returns the data path from the ID to this object (string).\n"
"\n"
"   :arg property: Optional property name which can be used if the path is\n"
"      to a property of this object.\n"
"   :type property: string\n"
"   :return: The path from :class:`bpy.types.bpy_struct.id_data`\n"
"      to this struct and property (when given).\n"
"   :rtype: str\n"
);
static PyObject *pyrna_struct_path_from_id(BPy_StructRNA *self, PyObject *args)
{
	const char *name = NULL;
	const char *path;
	PropertyRNA *prop;
	PyObject *ret;

	PYRNA_STRUCT_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "|s:path_from_id", &name))
		return NULL;

	if (name) {
		prop = RNA_struct_find_property(&self->ptr, name);
		if (prop == NULL) {
			PyErr_Format(PyExc_AttributeError,
			             "%.200s.path_from_id(\"%.200s\") not found",
			             RNA_struct_identifier(self->ptr.type), name);
			return NULL;
		}

		path = RNA_path_from_ID_to_property(&self->ptr, prop);
	}
	else {
		path = RNA_path_from_ID_to_struct(&self->ptr);
	}

	if (path == NULL) {
		if (name) {
			PyErr_Format(PyExc_ValueError,
			             "%.200s.path_from_id(\"%s\") found but does not support path creation",
			             RNA_struct_identifier(self->ptr.type), name);
		}
		else {
			PyErr_Format(PyExc_ValueError,
			             "%.200s.path_from_id() does not support path creation for this type",
			             RNA_struct_identifier(self->ptr.type));
		}
		return NULL;
	}

	ret = PyUnicode_FromString(path);
	MEM_freeN((void *)path);

	return ret;
}

PyDoc_STRVAR(pyrna_prop_path_from_id_doc,
".. method:: path_from_id()\n"
"\n"
"   Returns the data path from the ID to this property (string).\n"
"\n"
"   :return: The path from :class:`bpy.types.bpy_struct.id_data` to this property.\n"
"   :rtype: str\n"
);
static PyObject *pyrna_prop_path_from_id(BPy_PropertyRNA *self)
{
	const char *path;
	PropertyRNA *prop = self->prop;
	PyObject *ret;

	path = RNA_path_from_ID_to_property(&self->ptr, self->prop);

	if (path == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "%.200s.%.200s.path_from_id() does not support path creation for this type",
		             RNA_struct_identifier(self->ptr.type), RNA_property_identifier(prop));
		return NULL;
	}

	ret = PyUnicode_FromString(path);
	MEM_freeN((void *)path);

	return ret;
}

PyDoc_STRVAR(pyrna_struct_type_recast_doc,
".. method:: type_recast()\n"
"\n"
"   Return a new instance, this is needed because types\n"
"   such as textures can be changed at runtime.\n"
"\n"
"   :return: a new instance of this object with the type initialized again.\n"
"   :rtype: subclass of :class:`bpy.types.bpy_struct`\n"
);
static PyObject *pyrna_struct_type_recast(BPy_StructRNA *self)
{
	PointerRNA r_ptr;

	PYRNA_STRUCT_CHECK_OBJ(self);

	RNA_pointer_recast(&self->ptr, &r_ptr);
	return pyrna_struct_CreatePyObject(&r_ptr);
}

static void pyrna_dir_members_py(PyObject *list, PyObject *self)
{
	PyObject *dict;
	PyObject **dict_ptr;
	PyObject *list_tmp;

	dict_ptr = _PyObject_GetDictPtr((PyObject *)self);

	if (dict_ptr && (dict = *dict_ptr)) {
		list_tmp = PyDict_Keys(dict);
		PyList_SetSlice(list, INT_MAX, INT_MAX, list_tmp);
		Py_DECREF(list_tmp);
	}

	dict = ((PyTypeObject *)Py_TYPE(self))->tp_dict;
	if (dict) {
		list_tmp = PyDict_Keys(dict);
		PyList_SetSlice(list, INT_MAX, INT_MAX, list_tmp);
		Py_DECREF(list_tmp);
	}
}

static void pyrna_dir_members_rna(PyObject *list, PointerRNA *ptr)
{
	PyObject *pystring;
	const char *idname;

	/* for looping over attrs and funcs */
	PointerRNA tptr;
	PropertyRNA *iterprop;

	{
		RNA_pointer_create(NULL, &RNA_Struct, ptr->type, &tptr);
		iterprop = RNA_struct_find_property(&tptr, "functions");

		RNA_PROP_BEGIN(&tptr, itemptr, iterprop) {
			idname = RNA_function_identifier(itemptr.data);

			pystring = PyUnicode_FromString(idname);
			PyList_Append(list, pystring);
			Py_DECREF(pystring);
		}
		RNA_PROP_END;
	}

	{
		/*
		 * Collect RNA attributes
		 */
		char name[256], *nameptr;
		int namelen;

		iterprop = RNA_struct_iterator_property(ptr->type);

		RNA_PROP_BEGIN(ptr, itemptr, iterprop) {
			nameptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &namelen);

			if (nameptr) {
				pystring = PyUnicode_FromStringAndSize(nameptr, namelen);
				PyList_Append(list, pystring);
				Py_DECREF(pystring);

				if (name != nameptr) {
					MEM_freeN(nameptr);
				}
			}
		}
		RNA_PROP_END;
	}
}


static PyObject *pyrna_struct_dir(BPy_StructRNA *self)
{
	PyObject *ret;
	PyObject *pystring;

	PYRNA_STRUCT_CHECK_OBJ(self);

	/* Include this incase this instance is a subtype of a python class
	 * In these instances we may want to return a function or variable provided by the subtype
	 * */
	ret = PyList_New(0);

	if (!BPy_StructRNA_CheckExact(self))
		pyrna_dir_members_py(ret, (PyObject *)self);

	pyrna_dir_members_rna(ret, &self->ptr);

	if (self->ptr.type == &RNA_Context) {
		ListBase lb = CTX_data_dir_get(self->ptr.data);
		LinkData *link;

		for (link = lb.first; link; link = link->next) {
			pystring = PyUnicode_FromString(link->data);
			PyList_Append(ret, pystring);
			Py_DECREF(pystring);
		}

		BLI_freelistN(&lb);
	}

	{
		/* set(), this is needed to remove-doubles because the deferred
		 * register-props will be in both the python __dict__ and accessed as RNA */

		PyObject *set = PySet_New(ret);

		Py_DECREF(ret);
		ret = PySequence_List(set);
		Py_DECREF(set);
	}

	return ret;
}

//---------------getattr--------------------------------------------
static PyObject *pyrna_struct_getattro(BPy_StructRNA *self, PyObject *pyname)
{
	const char *name = _PyUnicode_AsString(pyname);
	PyObject *ret;
	PropertyRNA *prop;
	FunctionRNA *func;

	PYRNA_STRUCT_CHECK_OBJ(self);

	if (name == NULL) {
		PyErr_SetString(PyExc_AttributeError, "bpy_struct: __getattr__ must be a string");
		ret = NULL;
	}
	else if (name[0] == '_') { // rna can't start with a "_", so for __dict__ and similar we can skip using rna lookups
		/* annoying exception, maybe we need to have different types for this... */
		if ((strcmp(name, "__getitem__") == 0 || strcmp(name, "__setitem__") == 0) && !RNA_struct_idprops_check(self->ptr.type)) {
			PyErr_SetString(PyExc_AttributeError, "bpy_struct: no __getitem__ support for this type");
			ret = NULL;
		}
		else {
			ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
		}
	}
	else if ((prop = RNA_struct_find_property(&self->ptr, name))) {
		ret = pyrna_prop_to_py(&self->ptr, prop);
	}
	/* RNA function only if callback is declared (no optional functions) */
	else if ((func = RNA_struct_find_function(&self->ptr, name)) && RNA_function_defined(func)) {
		ret = pyrna_func_to_py(&self->ptr, func);
	}
	else if (self->ptr.type == &RNA_Context) {
		bContext *C = self->ptr.data;
		if (C == NULL) {
			PyErr_Format(PyExc_AttributeError,
			             "bpy_struct: Context is 'NULL', can't get \"%.200s\" from context",
			             name);
			ret = NULL;
		}
		else {
			PointerRNA newptr;
			ListBase newlb;
			short newtype;

			int done = CTX_data_get(C, name, &newptr, &newlb, &newtype);

			if (done == 1) { /* found */
				switch (newtype) {
				case CTX_DATA_TYPE_POINTER:
					if (newptr.data == NULL) {
						ret = Py_None;
						Py_INCREF(ret);
					}
					else {
						ret = pyrna_struct_CreatePyObject(&newptr);
					}
					break;
				case CTX_DATA_TYPE_COLLECTION:
					{
						CollectionPointerLink *link;
						PyObject *linkptr;

						ret = PyList_New(0);

						for (link = newlb.first; link; link = link->next) {
							linkptr = pyrna_struct_CreatePyObject(&link->ptr);
							PyList_Append(ret, linkptr);
							Py_DECREF(linkptr);
						}
					}
					break;
				default:
					/* should never happen */
					BLI_assert(!"Invalid context type");

					PyErr_Format(PyExc_AttributeError,
					             "bpy_struct: Context type invalid %d, can't get \"%.200s\" from context",
					             newtype, name);
					ret = NULL;
				}
			}
			else if (done == -1) { /* found but not set */
				ret = Py_None;
				Py_INCREF(ret);
			}
			else { /* not found in the context */
				/* lookup the subclass. raise an error if its not found */
				ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
			}

			BLI_freelistN(&newlb);
		}
	}
	else {
#if 0
		PyErr_Format(PyExc_AttributeError,
		             "bpy_struct: attribute \"%.200s\" not found",
		             name);
		ret = NULL;
#endif
		/* Include this incase this instance is a subtype of a python class
		 * In these instances we may want to return a function or variable provided by the subtype
		 *
		 * Also needed to return methods when its not a subtype
		 * */

		/* The error raised here will be displayed */
		ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
	}

	return ret;
}

#if 0
static int pyrna_struct_pydict_contains(PyObject *self, PyObject *pyname)
{
	PyObject *dict = *(_PyObject_GetDictPtr((PyObject *)self));
	if (dict == NULL) /* unlikely */
		return 0;

	return PyDict_Contains(dict, pyname);
}
#endif

//--------------- setattr-------------------------------------------
static int pyrna_is_deferred_prop(const PyObject *value)
{
	return  PyTuple_CheckExact(value) &&
	        PyTuple_GET_SIZE(value) == 2 &&
	        PyCFunction_Check(PyTuple_GET_ITEM(value, 0)) &&
	        PyDict_CheckExact(PyTuple_GET_ITEM(value, 1));
}

#if 0
static PyObject *pyrna_struct_meta_idprop_getattro(PyObject *cls, PyObject *attr)
{
	PyObject *ret = PyType_Type.tp_getattro(cls, attr);

	/* Allows:
	 * >>> bpy.types.Scene.foo = BoolProperty()
	 * >>> bpy.types.Scene.foo
	 * <bpy_struct, BoolProperty("foo")>
	 * ...rather than returning the deferred class register tuple as checked by pyrna_is_deferred_prop()
	 *
	 * Disable for now, this is faking internal behavior in a way thats too tricky to maintain well. */
#if 0
	if (ret == NULL) { // || pyrna_is_deferred_prop(ret)
		StructRNA *srna = srna_from_self(cls, "StructRNA.__getattr__");
		if (srna) {
			PropertyRNA *prop = RNA_struct_type_find_property(srna, _PyUnicode_AsString(attr));
			if (prop) {
				PointerRNA tptr;
				PyErr_Clear(); /* clear error from tp_getattro */
				RNA_pointer_create(NULL, &RNA_Property, prop, &tptr);
				ret = pyrna_struct_CreatePyObject(&tptr);
			}
		}
	}
#endif

	return ret;
}
#endif

static int pyrna_struct_meta_idprop_setattro(PyObject *cls, PyObject *attr, PyObject *value)
{
	StructRNA *srna = srna_from_self(cls, "StructRNA.__setattr__");
	const int is_deferred_prop = (value && pyrna_is_deferred_prop(value));
	const char *attr_str = _PyUnicode_AsString(attr);

	if (srna && !pyrna_write_check() && (is_deferred_prop || RNA_struct_type_find_property(srna, attr_str))) {
		PyErr_Format(PyExc_AttributeError,
		             "pyrna_struct_meta_idprop_setattro() "
		             "can't set in readonly state '%.200s.%S'",
		             ((PyTypeObject *)cls)->tp_name, attr);
		return -1;
	}

	if (srna == NULL) {
		/* allow setting on unregistered classes which can be registered later on */
		/*
		if (value && is_deferred_prop) {
			PyErr_Format(PyExc_AttributeError,
			             "pyrna_struct_meta_idprop_setattro() unable to get srna from class '%.200s'",
			             ((PyTypeObject *)cls)->tp_name);
			return -1;
		}
		*/
		/* srna_from_self may set an error */
		PyErr_Clear();
		return PyType_Type.tp_setattro(cls, attr, value);
	}

	if (value) {
		/* check if the value is a property */
		if (is_deferred_prop) {
			int ret = deferred_register_prop(srna, attr, value);
			if (ret == -1) {
				/* error set */
				return ret;
			}

			/* pass through and assign to the classes __dict__ as well
			 * when the value isn't assigned it still creates the RNA property
			 * but gets confusing from script writers POV if the assigned value cant be read back. */
		}
		else {
			/* remove existing property if its set or we also end up with confusion */
			RNA_def_property_free_identifier(srna, attr_str); /* ignore on failure */
		}
	}
	else { /* __delattr__ */
		/* first find if this is a registered property */
		const int ret = RNA_def_property_free_identifier(srna, attr_str);
		if (ret == -1) {
			PyErr_Format(PyExc_TypeError,
			             "struct_meta_idprop.detattr(): '%s' not a dynamic property",
			             attr_str);
			return -1;
		}
	}

	/* fallback to standard py, delattr/setattr */
	return PyType_Type.tp_setattro(cls, attr, value);
}

static int pyrna_struct_setattro(BPy_StructRNA *self, PyObject *pyname, PyObject *value)
{
	const char *name = _PyUnicode_AsString(pyname);
	PropertyRNA *prop = NULL;

	PYRNA_STRUCT_CHECK_INT(self);

#ifdef USE_PEDANTIC_WRITE
	if (rna_disallow_writes && rna_id_write_error(&self->ptr, pyname)) {
		return -1;
	}
#endif // USE_PEDANTIC_WRITE

	if (name == NULL) {
		PyErr_SetString(PyExc_AttributeError, "bpy_struct: __setattr__ must be a string");
		return -1;
	}
	else if (name[0] != '_' && (prop = RNA_struct_find_property(&self->ptr, name))) {
		if (!RNA_property_editable_flag(&self->ptr, prop)) {
			PyErr_Format(PyExc_AttributeError,
			             "bpy_struct: attribute \"%.200s\" from \"%.200s\" is read-only",
			             RNA_property_identifier(prop), RNA_struct_identifier(self->ptr.type));
			return -1;
		}
	}
	else if (self->ptr.type == &RNA_Context) {
		/* code just raises correct error, context prop's cant be set, unless its apart of the py class */
		bContext *C = self->ptr.data;
		if (C == NULL) {
			PyErr_Format(PyExc_AttributeError,
			             "bpy_struct: Context is 'NULL', can't set \"%.200s\" from context",
			             name);
			return -1;
		}
		else {
			PointerRNA newptr;
			ListBase newlb;
			short newtype;

			int done = CTX_data_get(C, name, &newptr, &newlb, &newtype);

			if (done == 1) {
				PyErr_Format(PyExc_AttributeError,
				             "bpy_struct: Context property \"%.200s\" is read-only",
				             name);
				BLI_freelistN(&newlb);
				return -1;
			}

			BLI_freelistN(&newlb);
		}
	}

	/* pyrna_py_to_prop sets its own exceptions */
	if (prop) {
		if (value == NULL) {
			PyErr_SetString(PyExc_AttributeError, "bpy_struct: del not supported");
			return -1;
		}
		return pyrna_py_to_prop(&self->ptr, prop, NULL, value, "bpy_struct: item.attr = val:");
	}
	else {
		return PyObject_GenericSetAttr((PyObject *)self, pyname, value);
	}
}

static PyObject *pyrna_prop_dir(BPy_PropertyRNA *self)
{
	PyObject *ret;
	PointerRNA r_ptr;

	/* Include this incase this instance is a subtype of a python class
	 * In these instances we may want to return a function or variable provided by the subtype
	 * */
	ret = PyList_New(0);

	if (!BPy_PropertyRNA_CheckExact(self)) {
		pyrna_dir_members_py(ret, (PyObject *)self);
	}

	if (RNA_property_type(self->prop) == PROP_COLLECTION) {
		if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
			pyrna_dir_members_rna(ret, &r_ptr);
		}
	}

	return ret;
}


static PyObject *pyrna_prop_array_getattro(BPy_PropertyRNA *self, PyObject *pyname)
{
	return PyObject_GenericGetAttr((PyObject *)self, pyname);
}

static PyObject *pyrna_prop_collection_getattro(BPy_PropertyRNA *self, PyObject *pyname)
{
	const char *name = _PyUnicode_AsString(pyname);

	if (name == NULL) {
		PyErr_SetString(PyExc_AttributeError, "bpy_prop_collection: __getattr__ must be a string");
		return NULL;
	}
	else if (name[0] != '_') {
		PyObject *ret;
		PropertyRNA *prop;
		FunctionRNA *func;

		PointerRNA r_ptr;
		if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
			if ((prop = RNA_struct_find_property(&r_ptr, name))) {
				ret = pyrna_prop_to_py(&r_ptr, prop);

				return ret;
			}
			else if ((func = RNA_struct_find_function(&r_ptr, name))) {
				PyObject *self_collection = pyrna_struct_CreatePyObject(&r_ptr);
				ret = pyrna_func_to_py(&((BPy_DummyPointerRNA *)self_collection)->ptr, func);
				Py_DECREF(self_collection);

				return ret;
			}
		}
	}

#if 0
	return PyObject_GenericGetAttr((PyObject *)self, pyname);
#else
	{
		/* Could just do this except for 1 awkward case.
		 * PyObject_GenericGetAttr((PyObject *)self, pyname);
		 * so as to support 'bpy.data.library.load()'
		 * note, this _only_ supports static methods */

		PyObject *ret = PyObject_GenericGetAttr((PyObject *)self, pyname);

		if (ret == NULL && name[0] != '_') { /* avoid inheriting __call__ and similar */
			/* since this is least common case, handle it last */
			PointerRNA r_ptr;
			if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
				PyObject *cls;

				PyObject *error_type, *error_value, *error_traceback;
				PyErr_Fetch(&error_type, &error_value, &error_traceback);
				PyErr_Clear();

				cls = pyrna_struct_Subtype(&r_ptr); /* borrows */
				ret = PyObject_GenericGetAttr(cls, pyname);
				/* restore the original error */
				if (ret == NULL) {
					PyErr_Restore(error_type, error_value, error_traceback);
				}
			}
		}

		return ret;
	}
#endif
}

//--------------- setattr-------------------------------------------
static int pyrna_prop_collection_setattro(BPy_PropertyRNA *self, PyObject *pyname, PyObject *value)
{
	const char *name = _PyUnicode_AsString(pyname);
	PropertyRNA *prop;
	PointerRNA r_ptr;

#ifdef USE_PEDANTIC_WRITE
	if (rna_disallow_writes && rna_id_write_error(&self->ptr, pyname)) {
		return -1;
	}
#endif // USE_PEDANTIC_WRITE

	if (name == NULL) {
		PyErr_SetString(PyExc_AttributeError, "bpy_prop: __setattr__ must be a string");
		return -1;
	}
	else if (value == NULL) {
		PyErr_SetString(PyExc_AttributeError, "bpy_prop: del not supported");
		return -1;
	}
	else if (RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
		if ((prop = RNA_struct_find_property(&r_ptr, name))) {
			/* pyrna_py_to_prop sets its own exceptions */
			return pyrna_py_to_prop(&r_ptr, prop, NULL, value, "BPy_PropertyRNA - Attribute (setattr):");
		}
	}

	PyErr_Format(PyExc_AttributeError,
	             "bpy_prop_collection: attribute \"%.200s\" not found",
	             name);
	return -1;
}

/* odd case, we need to be able return a python method from a tp_getset */
static PyObject *pyrna_prop_collection_idprop_add(BPy_PropertyRNA *self)
{
	PointerRNA r_ptr;

	RNA_property_collection_add(&self->ptr, self->prop, &r_ptr);
	if (!r_ptr.data) {
		PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.add(): not supported for this collection");
		return NULL;
	}
	else {
		return pyrna_struct_CreatePyObject(&r_ptr);
	}
}

static PyObject *pyrna_prop_collection_idprop_remove(BPy_PropertyRNA *self, PyObject *value)
{
	int key = PyLong_AsLong(value);

	if (key == -1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.remove(): expected one int argument");
		return NULL;
	}

	if (!RNA_property_collection_remove(&self->ptr, self->prop, key)) {
		PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.remove() not supported for this collection");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *pyrna_prop_collection_idprop_move(BPy_PropertyRNA *self, PyObject *args)
{
	int key = 0, pos = 0;

	if (!PyArg_ParseTuple(args, "ii", &key, &pos)) {
		PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.move(): expected two ints as arguments");
		return NULL;
	}

	if (!RNA_property_collection_move(&self->ptr, self->prop, key, pos)) {
		PyErr_SetString(PyExc_TypeError, "bpy_prop_collection.move() not supported for this collection");
		return NULL;
	}

	Py_RETURN_NONE;
}


PyDoc_STRVAR(pyrna_struct_get_id_data_doc,
"The :class:`bpy.types.ID` object this datablock is from or None, (not available for all data types)"
);
static PyObject *pyrna_struct_get_id_data(BPy_DummyPointerRNA *self)
{
	/* used for struct and pointer since both have a ptr */
	if (self->ptr.id.data) {
		PointerRNA id_ptr;
		RNA_id_pointer_create((ID *)self->ptr.id.data, &id_ptr);
		return pyrna_struct_CreatePyObject(&id_ptr);
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(pyrna_struct_get_data_doc,
"The data this property is using, *type* :class:`bpy.types.bpy_struct`"
);
static PyObject *pyrna_struct_get_data(BPy_DummyPointerRNA *self)
{
	return pyrna_struct_CreatePyObject(&self->ptr);
}

PyDoc_STRVAR(pyrna_struct_get_rna_type_doc,
"The property type for introspection"
);
static PyObject *pyrna_struct_get_rna_type(BPy_PropertyRNA *self)
{
	PointerRNA tptr;
	RNA_pointer_create(NULL, &RNA_Property, self->prop, &tptr);
	return pyrna_struct_Subtype(&tptr);
}



/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/

static PyGetSetDef pyrna_prop_getseters[] = {
	{(char *)"id_data", (getter)pyrna_struct_get_id_data, (setter)NULL, (char *)pyrna_struct_get_id_data_doc, NULL},
	{(char *)"data", (getter)pyrna_struct_get_data, (setter)NULL, (char *)pyrna_struct_get_data_doc, NULL},
	{(char *)"rna_type", (getter)pyrna_struct_get_rna_type, (setter)NULL, (char *)pyrna_struct_get_rna_type_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

static PyGetSetDef pyrna_struct_getseters[] = {
	{(char *)"id_data", (getter)pyrna_struct_get_id_data, (setter)NULL, (char *)pyrna_struct_get_id_data_doc, NULL},
	{NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};


PyDoc_STRVAR(pyrna_prop_collection_keys_doc,
".. method:: keys()\n"
"\n"
"   Return the identifiers of collection members\n"
"   (matching pythons dict.keys() functionality).\n"
"\n"
"   :return: the identifiers for each member of this collection.\n"
"   :rtype: list of stings\n"
);
static PyObject *pyrna_prop_collection_keys(BPy_PropertyRNA *self)
{
	PyObject *ret = PyList_New(0);
	PyObject *item;
	char name[256], *nameptr;
	int namelen;

	RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
		nameptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &namelen);

		if (nameptr) {
			/* add to python list */
			item = PyUnicode_FromStringAndSize(nameptr, namelen);
			PyList_Append(ret, item);
			Py_DECREF(item);
			/* done */

			if (name != nameptr) {
				MEM_freeN(nameptr);
			}
		}
	}
	RNA_PROP_END;

	return ret;
}

PyDoc_STRVAR(pyrna_prop_collection_items_doc,
".. method:: items()\n"
"\n"
"   Return the identifiers of collection members\n"
"   (matching pythons dict.items() functionality).\n"
"\n"
"   :return: (key, value) pairs for each member of this collection.\n"
"   :rtype: list of tuples\n"
);
static PyObject *pyrna_prop_collection_items(BPy_PropertyRNA *self)
{
	PyObject *ret = PyList_New(0);
	PyObject *item;
	char name[256], *nameptr;
	int namelen;
	int i = 0;

	RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
		if (itemptr.data) {
			/* add to python list */
			item = PyTuple_New(2);
			nameptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &namelen);
			if (nameptr) {
				PyTuple_SET_ITEM(item, 0, PyUnicode_FromStringAndSize(nameptr, namelen));
				if (name != nameptr)
					MEM_freeN(nameptr);
			}
			else {
				/* a bit strange but better then returning an empty list */
				PyTuple_SET_ITEM(item, 0, PyLong_FromSsize_t(i));
			}
			PyTuple_SET_ITEM(item, 1, pyrna_struct_CreatePyObject(&itemptr));

			PyList_Append(ret, item);
			Py_DECREF(item);

			i++;
		}
	}
	RNA_PROP_END;

	return ret;
}

PyDoc_STRVAR(pyrna_prop_collection_values_doc,
".. method:: values()\n"
"\n"
"   Return the values of collection\n"
"   (matching pythons dict.values() functionality).\n"
"\n"
"   :return: the members of this collection.\n"
"   :rtype: list\n"
);
static PyObject *pyrna_prop_collection_values(BPy_PropertyRNA *self)
{
	/* re-use slice*/
	return pyrna_prop_collection_subscript_slice(self, 0, PY_SSIZE_T_MAX);
}

PyDoc_STRVAR(pyrna_struct_get_doc,
".. method:: get(key, default=None)\n"
"\n"
"   Returns the value of the custom property assigned to key or default\n"
"   when not found (matches pythons dictionary function of the same name).\n"
"\n"
"   :arg key: The key associated with the custom property.\n"
"   :type key: string\n"
"   :arg default: Optional argument for the value to return if\n"
"      *key* is not found.\n"
"   :type default: Undefined\n"
"\n"
BPY_DOC_ID_PROP_TYPE_NOTE
);
static PyObject *pyrna_struct_get(BPy_StructRNA *self, PyObject *args)
{
	IDProperty *group, *idprop;

	const char *key;
	PyObject* def = Py_None;

	PYRNA_STRUCT_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "s|O:get", &key, &def))
		return NULL;

	/* mostly copied from BPy_IDGroup_Map_GetItem */
	if (RNA_struct_idprops_check(self->ptr.type) == 0) {
		PyErr_SetString(PyExc_TypeError, "this type doesn't support IDProperties");
		return NULL;
	}

	group = RNA_struct_idprops(&self->ptr, 0);
	if (group) {
		idprop = IDP_GetPropertyFromGroup(group, key);

		if (idprop) {
			return BPy_IDGroup_WrapData(self->ptr.id.data, idprop, group);
		}
	}

	return Py_INCREF(def), def;
}

PyDoc_STRVAR(pyrna_struct_as_pointer_doc,
".. method:: as_pointer()\n"
"\n"
"   Returns the memory address which holds a pointer to blenders internal data\n"
"\n"
"   :return: int (memory address).\n"
"   :rtype: int\n"
"\n"
"   .. note:: This is intended only for advanced script writers who need to\n"
"      pass blender data to their own C/Python modules.\n"
);
static PyObject *pyrna_struct_as_pointer(BPy_StructRNA *self)
{
	return PyLong_FromVoidPtr(self->ptr.data);
}

PyDoc_STRVAR(pyrna_prop_collection_get_doc,
".. method:: get(key, default=None)\n"
"\n"
"   Returns the value of the item assigned to key or default when not found\n"
"   (matches pythons dictionary function of the same name).\n"
"\n"
"   :arg key: The identifier for the collection member.\n"
"   :type key: string\n"
"   :arg default: Optional argument for the value to return if\n"
"      *key* is not found.\n"
"   :type default: Undefined\n"
);
static PyObject *pyrna_prop_collection_get(BPy_PropertyRNA *self, PyObject *args)
{
	PointerRNA newptr;

	PyObject *key_ob;
	PyObject* def = Py_None;

	PYRNA_PROP_CHECK_OBJ(self);

	if (!PyArg_ParseTuple(args, "O|O:get", &key_ob, &def))
		return NULL;

	if (PyUnicode_Check(key_ob)) {
		const char *key = _PyUnicode_AsString(key_ob);

		if (RNA_property_collection_lookup_string(&self->ptr, self->prop, key, &newptr))
			return pyrna_struct_CreatePyObject(&newptr);
	}
	else if (PyTuple_Check(key_ob)) {
		PyObject *ret = pyrna_prop_collection_subscript_str_lib_pair(self, key_ob,
		                                                            "bpy_prop_collection.get((id, lib))", FALSE);
		if (ret) {
			return ret;
		}
	}
	else {
		PyErr_Format(PyExc_KeyError,
		             "bpy_prop_collection.get(key, ...): key must be a string or tuple, not %.200s",
		             Py_TYPE(key_ob)->tp_name);
	}

	return Py_INCREF(def), def;
}

PyDoc_STRVAR(pyrna_prop_collection_find_doc,
".. method:: find(key)\n"
"\n"
"   Returns the index of a key in a collection or -1 when not found\n"
"   (matches pythons string find function of the same name).\n"
"\n"
"   :arg key: The identifier for the collection member.\n"
"   :type key: string\n"
"   :return: index of the key.\n"
"   :rtype: int\n"
);
static PyObject *pyrna_prop_collection_find(BPy_PropertyRNA *self, PyObject *key_ob)
{
	Py_ssize_t key_len_ssize_t;
	const char *key = _PyUnicode_AsStringAndSize(key_ob, &key_len_ssize_t);
	const int key_len = (int)key_len_ssize_t; /* comare with same type */

	char name[256], *nameptr;
	int namelen;
	int i = 0;
	int index = -1;

	PYRNA_PROP_CHECK_OBJ(self);

	RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
		nameptr = RNA_struct_name_get_alloc(&itemptr, name, sizeof(name), &namelen);

		if (nameptr) {
			if ((key_len == namelen) && memcmp(nameptr, key, key_len) == 0) {
				index = i;
				break;
			}

			if (name != nameptr) {
				MEM_freeN(nameptr);
			}
		}

		i++;
	}
	RNA_PROP_END;

	return PyLong_FromSsize_t(index);
}

static void foreach_attr_type(	BPy_PropertyRNA *self, const char *attr,
									/* values to assign */
									RawPropertyType *raw_type, int *attr_tot, int *attr_signed)
{
	PropertyRNA *prop;
	*raw_type = PROP_RAW_UNSET;
	*attr_tot = 0;
	*attr_signed = FALSE;

	/* note: this is fail with zero length lists, so dont let this get caled in that case */
	RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
		prop = RNA_struct_find_property(&itemptr, attr);
		*raw_type = RNA_property_raw_type(prop);
		*attr_tot = RNA_property_array_length(&itemptr, prop);
		*attr_signed = (RNA_property_subtype(prop) == PROP_UNSIGNED) ? FALSE:TRUE;
		break;
	}
	RNA_PROP_END;
}

/* pyrna_prop_collection_foreach_get/set both use this */
static int foreach_parse_args(
        BPy_PropertyRNA *self, PyObject *args,

		/*values to assign */
		const char **attr, PyObject **seq, int *tot, int *size,
        RawPropertyType *raw_type, int *attr_tot, int *attr_signed
        )
{
#if 0
	int array_tot;
	int target_tot;
#endif

	*size = *attr_tot = *attr_signed = FALSE;
	*raw_type = PROP_RAW_UNSET;

	if (!PyArg_ParseTuple(args, "sO", attr, seq) || (!PySequence_Check(*seq) && PyObject_CheckBuffer(*seq))) {
		PyErr_SetString(PyExc_TypeError, "foreach_get(attr, sequence) expects a string and a sequence");
		return -1;
	}

	*tot = PySequence_Size(*seq); // TODO - buffer may not be a sequence! array.array() is tho.

	if (*tot > 0) {
		foreach_attr_type(self, *attr, raw_type, attr_tot, attr_signed);
		*size = RNA_raw_type_sizeof(*raw_type);

#if 0	// works fine but not strictly needed, we could allow RNA_property_collection_raw_* to do the checks
		if ((*attr_tot) < 1)
			*attr_tot = 1;

		if (RNA_property_type(self->prop) == PROP_COLLECTION)
			array_tot = RNA_property_collection_length(&self->ptr, self->prop);
		else
			array_tot = RNA_property_array_length(&self->ptr, self->prop);


		target_tot = array_tot * (*attr_tot);

		/* rna_access.c - rna_raw_access(...) uses this same method */
		if (target_tot != (*tot)) {
			PyErr_Format(PyExc_TypeError,
			             "foreach_get(attr, sequence) sequence length mismatch given %d, needed %d",
			             *tot, target_tot);
			return -1;
		}
#endif
	}

	/* check 'attr_tot' otherwise we dont know if any values were set
	 * this isn't ideal because it means running on an empty list may fail silently when its not compatible. */
	if (*size == 0 && *attr_tot != 0) {
		PyErr_SetString(PyExc_AttributeError, "attribute does not support foreach method");
		return -1;
	}
	return 0;
}

static int foreach_compat_buffer(RawPropertyType raw_type, int attr_signed, const char *format)
{
	char f = format ? *format:'B'; /* B is assumed when not set */

	switch (raw_type) {
	case PROP_RAW_CHAR:
		if (attr_signed)  return (f == 'b') ? 1:0;
		else              return (f == 'B') ? 1:0;
	case PROP_RAW_SHORT:
		if (attr_signed)  return (f == 'h') ? 1:0;
		else              return (f == 'H') ? 1:0;
	case PROP_RAW_INT:
		if (attr_signed)  return (f == 'i') ? 1:0;
		else              return (f == 'I') ? 1:0;
	case PROP_RAW_FLOAT:
		return (f == 'f') ? 1:0;
	case PROP_RAW_DOUBLE:
		return (f == 'd') ? 1:0;
	case PROP_RAW_UNSET:
		return 0;
	}

	return 0;
}

static PyObject *foreach_getset(BPy_PropertyRNA *self, PyObject *args, int set)
{
	PyObject *item = NULL;
	int i = 0, ok = 0, buffer_is_compat;
	void *array = NULL;

	/* get/set both take the same args currently */
	const char *attr;
	PyObject *seq;
	int tot, size, attr_tot, attr_signed;
	RawPropertyType raw_type;

	if (foreach_parse_args(self, args, &attr, &seq, &tot, &size, &raw_type, &attr_tot, &attr_signed) < 0)
		return NULL;

	if (tot == 0)
		Py_RETURN_NONE;



	if (set) { /* get the array from python */
		buffer_is_compat = FALSE;
		if (PyObject_CheckBuffer(seq)) {
			Py_buffer buf;
			PyObject_GetBuffer(seq, &buf, PyBUF_SIMPLE | PyBUF_FORMAT);

			/* check if the buffer matches */

			buffer_is_compat = foreach_compat_buffer(raw_type, attr_signed, buf.format);

			if (buffer_is_compat) {
				ok = RNA_property_collection_raw_set(NULL, &self->ptr, self->prop, attr, buf.buf, raw_type, tot);
			}

			PyBuffer_Release(&buf);
		}

		/* could not use the buffer, fallback to sequence */
		if (!buffer_is_compat) {
			array = PyMem_Malloc(size * tot);

			for ( ; i < tot; i++) {
				item = PySequence_GetItem(seq, i);
				switch (raw_type) {
				case PROP_RAW_CHAR:
					((char *)array)[i] = (char)PyLong_AsLong(item);
					break;
				case PROP_RAW_SHORT:
					((short *)array)[i] = (short)PyLong_AsLong(item);
					break;
				case PROP_RAW_INT:
					((int *)array)[i] = (int)PyLong_AsLong(item);
					break;
				case PROP_RAW_FLOAT:
					((float *)array)[i] = (float)PyFloat_AsDouble(item);
					break;
				case PROP_RAW_DOUBLE:
					((double *)array)[i] = (double)PyFloat_AsDouble(item);
					break;
				case PROP_RAW_UNSET:
					/* should never happen */
					BLI_assert(!"Invalid array type - set");
					break;
				}

				Py_DECREF(item);
			}

			ok = RNA_property_collection_raw_set(NULL, &self->ptr, self->prop, attr, array, raw_type, tot);
		}
	}
	else {
		buffer_is_compat = FALSE;
		if (PyObject_CheckBuffer(seq)) {
			Py_buffer buf;
			PyObject_GetBuffer(seq, &buf, PyBUF_SIMPLE | PyBUF_FORMAT);

			/* check if the buffer matches, TODO - signed/unsigned types */

			buffer_is_compat = foreach_compat_buffer(raw_type, attr_signed, buf.format);

			if (buffer_is_compat) {
				ok = RNA_property_collection_raw_get(NULL, &self->ptr, self->prop, attr, buf.buf, raw_type, tot);
			}

			PyBuffer_Release(&buf);
		}

		/* could not use the buffer, fallback to sequence */
		if (!buffer_is_compat) {
			array = PyMem_Malloc(size * tot);

			ok = RNA_property_collection_raw_get(NULL, &self->ptr, self->prop, attr, array, raw_type, tot);

			if (!ok) i = tot; /* skip the loop */

			for ( ; i < tot; i++) {

				switch (raw_type) {
				case PROP_RAW_CHAR:
					item = PyLong_FromSsize_t((Py_ssize_t) ((char *)array)[i]);
					break;
				case PROP_RAW_SHORT:
					item = PyLong_FromSsize_t((Py_ssize_t) ((short *)array)[i]);
					break;
				case PROP_RAW_INT:
					item = PyLong_FromSsize_t((Py_ssize_t) ((int *)array)[i]);
					break;
				case PROP_RAW_FLOAT:
					item = PyFloat_FromDouble((double) ((float *)array)[i]);
					break;
				case PROP_RAW_DOUBLE:
					item = PyFloat_FromDouble((double) ((double *)array)[i]);
					break;
				default: /* PROP_RAW_UNSET */
					/* should never happen */
					BLI_assert(!"Invalid array type - get");
					item = Py_None;
					Py_INCREF(item);
					break;
				}

				PySequence_SetItem(seq, i, item);
				Py_DECREF(item);
			}
		}
	}

	if (array)
		PyMem_Free(array);

	if (PyErr_Occurred()) {
		/* Maybe we could make our own error */
		PyErr_Print();
		PyErr_SetString(PyExc_TypeError, "couldn't access the py sequence");
		return NULL;
	}
	if (!ok) {
		PyErr_SetString(PyExc_RuntimeError, "internal error setting the array");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(pyrna_prop_collection_foreach_get_doc,
".. method:: foreach_get(attr, seq)\n"
"\n"
"   This is a function to give fast access to attributes within a collection.\n"
"\n"
"   .. code-block:: python\n"
"\n"
"      collection.foreach_get(someseq, attr)\n"
"\n"
"      # Python equivalent\n"
"      for i in range(len(seq)): someseq[i] = getattr(collection, attr)\n"
"\n"
);
static PyObject *pyrna_prop_collection_foreach_get(BPy_PropertyRNA *self, PyObject *args)
{
	PYRNA_PROP_CHECK_OBJ(self);

	return foreach_getset(self, args, 0);
}

PyDoc_STRVAR(pyrna_prop_collection_foreach_set_doc,
".. method:: foreach_set(attr, seq)\n"
"\n"
"   This is a function to give fast access to attributes within a collection.\n"
"\n"
"   .. code-block:: python\n"
"\n"
"      collection.foreach_set(seq, attr)\n"
"\n"
"      # Python equivalent\n"
"      for i in range(len(seq)): setattr(collection[i], attr, seq[i])\n"
"\n"
);
static PyObject *pyrna_prop_collection_foreach_set(BPy_PropertyRNA *self, PyObject *args)
{
	PYRNA_PROP_CHECK_OBJ(self);

	return foreach_getset(self, args, 1);
}

/* A bit of a kludge, make a list out of a collection or array,
 * then return the lists iter function, not especially fast but convenient for now */
static PyObject *pyrna_prop_array_iter(BPy_PropertyArrayRNA *self)
{
	/* Try get values from a collection */
	PyObject *ret;
	PyObject *iter = NULL;
	int len;

	PYRNA_PROP_CHECK_OBJ((BPy_PropertyRNA *)self);

	len = pyrna_prop_array_length(self);
	ret = pyrna_prop_array_subscript_slice(self, &self->ptr, self->prop, 0, len, len);

	/* we know this is a list so no need to PyIter_Check
	 * otherwise it could be NULL (unlikely) if conversion failed */
	if (ret) {
		iter = PyObject_GetIter(ret);
		Py_DECREF(ret);
	}

	return iter;
}

static PyObject *pyrna_prop_collection_iter(BPy_PropertyRNA *self);

#ifndef USE_PYRNA_ITER
static PyObject *pyrna_prop_collection_iter(BPy_PropertyRNA *self)
{
	/* Try get values from a collection */
	PyObject *ret;
	PyObject *iter = NULL;
	ret = pyrna_prop_collection_values(self);

	/* we know this is a list so no need to PyIter_Check
	 * otherwise it could be NULL (unlikely) if conversion failed */
	if (ret) {
		iter = PyObject_GetIter(ret);
		Py_DECREF(ret);
	}

	return iter;
}
#endif /* # !USE_PYRNA_ITER */

static struct PyMethodDef pyrna_struct_methods[] = {

	/* only for PointerRNA's with ID'props */
	{"keys", (PyCFunction)pyrna_struct_keys, METH_NOARGS, pyrna_struct_keys_doc},
	{"values", (PyCFunction)pyrna_struct_values, METH_NOARGS, pyrna_struct_values_doc},
	{"items", (PyCFunction)pyrna_struct_items, METH_NOARGS, pyrna_struct_items_doc},

	{"get", (PyCFunction)pyrna_struct_get, METH_VARARGS, pyrna_struct_get_doc},

	{"as_pointer", (PyCFunction)pyrna_struct_as_pointer, METH_NOARGS, pyrna_struct_as_pointer_doc},

	/* bpy_rna_anim.c */
	{"keyframe_insert", (PyCFunction)pyrna_struct_keyframe_insert, METH_VARARGS|METH_KEYWORDS, pyrna_struct_keyframe_insert_doc},
	{"keyframe_delete", (PyCFunction)pyrna_struct_keyframe_delete, METH_VARARGS|METH_KEYWORDS, pyrna_struct_keyframe_delete_doc},
	{"driver_add", (PyCFunction)pyrna_struct_driver_add, METH_VARARGS, pyrna_struct_driver_add_doc},
	{"driver_remove", (PyCFunction)pyrna_struct_driver_remove, METH_VARARGS, pyrna_struct_driver_remove_doc},

	{"is_property_set", (PyCFunction)pyrna_struct_is_property_set, METH_VARARGS, pyrna_struct_is_property_set_doc},
	{"is_property_hidden", (PyCFunction)pyrna_struct_is_property_hidden, METH_VARARGS, pyrna_struct_is_property_hidden_doc},
	{"path_resolve", (PyCFunction)pyrna_struct_path_resolve, METH_VARARGS, pyrna_struct_path_resolve_doc},
	{"path_from_id", (PyCFunction)pyrna_struct_path_from_id, METH_VARARGS, pyrna_struct_path_from_id_doc},
	{"type_recast", (PyCFunction)pyrna_struct_type_recast, METH_NOARGS, pyrna_struct_type_recast_doc},
	{"__dir__", (PyCFunction)pyrna_struct_dir, METH_NOARGS, NULL},

	/* experemental */
	{"callback_add", (PyCFunction)pyrna_callback_add, METH_VARARGS, NULL},
	{"callback_remove", (PyCFunction)pyrna_callback_remove, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef pyrna_prop_methods[] = {
	{"path_from_id", (PyCFunction)pyrna_prop_path_from_id, METH_NOARGS, pyrna_prop_path_from_id_doc},
	{"__dir__", (PyCFunction)pyrna_prop_dir, METH_NOARGS, NULL},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef pyrna_prop_array_methods[] = {
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef pyrna_prop_collection_methods[] = {
	{"foreach_get", (PyCFunction)pyrna_prop_collection_foreach_get, METH_VARARGS, pyrna_prop_collection_foreach_get_doc},
	{"foreach_set", (PyCFunction)pyrna_prop_collection_foreach_set, METH_VARARGS, pyrna_prop_collection_foreach_set_doc},

	{"keys", (PyCFunction)pyrna_prop_collection_keys, METH_NOARGS, pyrna_prop_collection_keys_doc},
	{"items", (PyCFunction)pyrna_prop_collection_items, METH_NOARGS, pyrna_prop_collection_items_doc},
	{"values", (PyCFunction)pyrna_prop_collection_values, METH_NOARGS, pyrna_prop_collection_values_doc},

	{"get", (PyCFunction)pyrna_prop_collection_get, METH_VARARGS, pyrna_prop_collection_get_doc},
	{"find", (PyCFunction)pyrna_prop_collection_find, METH_O, pyrna_prop_collection_find_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef pyrna_prop_collection_idprop_methods[] = {
	{"add", (PyCFunction)pyrna_prop_collection_idprop_add, METH_NOARGS, NULL},
	{"remove", (PyCFunction)pyrna_prop_collection_idprop_remove, METH_O, NULL},
	{"move", (PyCFunction)pyrna_prop_collection_idprop_move, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL}
};

/* only needed for subtyping, so a new class gets a valid BPy_StructRNA
 * todo - also accept useful args */
static PyObject *pyrna_struct_new(PyTypeObject *type, PyObject *args, PyObject *UNUSED(kwds))
{
	if (PyTuple_GET_SIZE(args) == 1) {
		BPy_StructRNA *base = (BPy_StructRNA *)PyTuple_GET_ITEM(args, 0);
		if (Py_TYPE(base) == type) {
			Py_INCREF(base);
			return (PyObject *)base;
		}
		else if (PyType_IsSubtype(Py_TYPE(base), &pyrna_struct_Type)) {
			/* this almost never runs, only when using user defined subclasses of built-in object.
			 * this isn't common since its NOT related to registerable subclasses. eg:

				>>> class MyObSubclass(bpy.types.Object):
				...     def test_func(self):
				...         print(100)
				...
				>>> myob = MyObSubclass(bpy.context.object)
				>>> myob.test_func()
				100
			 *
			 * Keep this since it could be useful.
			 */
			BPy_StructRNA *ret;
			if ((ret = (BPy_StructRNA *)type->tp_alloc(type, 0))) {
				ret->ptr = base->ptr;
			}
			/* pass on exception & NULL if tp_alloc fails */
			return (PyObject *)ret;
		}

		/* error, invalid type given */
		PyErr_Format(PyExc_TypeError,
		             "bpy_struct.__new__(type): type '%.200s' is not a subtype of bpy_struct",
		             type->tp_name);
		return NULL;
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "bpy_struct.__new__(type): expected a single argument");
		return NULL;
	}
}

/* only needed for subtyping, so a new class gets a valid BPy_StructRNA
 * todo - also accept useful args */
static PyObject *pyrna_prop_new(PyTypeObject *type, PyObject *args, PyObject *UNUSED(kwds))
{
	BPy_PropertyRNA *base;

	if (!PyArg_ParseTuple(args, "O!:bpy_prop.__new__", &pyrna_prop_Type, &base))
		return NULL;

	if (type == Py_TYPE(base)) {
		Py_INCREF(base);
		return (PyObject *)base;
	}
	else if (PyType_IsSubtype(type, &pyrna_prop_Type)) {
		BPy_PropertyRNA *ret = (BPy_PropertyRNA *) type->tp_alloc(type, 0);
		ret->ptr = base->ptr;
		ret->prop = base->prop;
		return (PyObject *)ret;
	}
	else {
		PyErr_Format(PyExc_TypeError,
		             "bpy_prop.__new__(type): type '%.200s' is not a subtype of bpy_prop",
		             type->tp_name);
		return NULL;
	}
}

static PyObject *pyrna_param_to_py(PointerRNA *ptr, PropertyRNA *prop, void *data)
{
	PyObject *ret;
	const int type = RNA_property_type(prop);
	const int flag = RNA_property_flag(prop);

	if (RNA_property_array_check(prop)) {
		int a, len;

		if (flag & PROP_DYNAMIC) {
			ParameterDynAlloc *data_alloc = data;
			len = data_alloc->array_tot;
			data = data_alloc->array;
		}
		else
			len = RNA_property_array_length(ptr, prop);

		/* resolve the array from a new pytype */

		/* kazanbas: TODO make multidim sequences here */

		switch (type) {
		case PROP_BOOLEAN:
			ret = PyTuple_New(len);
			for (a = 0; a < len; a++)
				PyTuple_SET_ITEM(ret, a, PyBool_FromLong(((int *)data)[a]));
			break;
		case PROP_INT:
			ret = PyTuple_New(len);
			for (a = 0; a < len; a++)
				PyTuple_SET_ITEM(ret, a, PyLong_FromSsize_t((Py_ssize_t)((int *)data)[a]));
			break;
		case PROP_FLOAT:
			switch (RNA_property_subtype(prop)) {
#ifdef USE_MATHUTILS
				case PROP_ALL_VECTOR_SUBTYPES:
					ret = Vector_CreatePyObject(data, len, Py_NEW, NULL);
					break;
				case PROP_MATRIX:
					if (len == 16) {
						ret = Matrix_CreatePyObject(data, 4, 4, Py_NEW, NULL);
						break;
					}
					else if (len == 9) {
						ret = Matrix_CreatePyObject(data, 3, 3, Py_NEW, NULL);
						break;
					}
					/* pass through */
#endif
				default:
					ret = PyTuple_New(len);
					for (a = 0; a < len; a++)
						PyTuple_SET_ITEM(ret, a, PyFloat_FromDouble(((float *)data)[a]));

			}
			break;
		default:
			PyErr_Format(PyExc_TypeError,
			             "RNA Error: unknown array type \"%d\" (pyrna_param_to_py)",
			             type);
			ret = NULL;
			break;
		}
	}
	else {
		/* see if we can coorce into a python type - PropertyType */
		switch (type) {
		case PROP_BOOLEAN:
			ret = PyBool_FromLong(*(int *)data);
			break;
		case PROP_INT:
			ret = PyLong_FromSsize_t((Py_ssize_t)*(int *)data);
			break;
		case PROP_FLOAT:
			ret = PyFloat_FromDouble(*(float *)data);
			break;
		case PROP_STRING:
		{
			char *data_ch;
			PyObject *value_coerce = NULL;
			const int subtype = RNA_property_subtype(prop);

			if (flag & PROP_THICK_WRAP)
				data_ch = (char *)data;
			else
				data_ch = *(char **)data;

#ifdef USE_STRING_COERCE
			if (subtype == PROP_BYTESTRING) {
				ret = PyBytes_FromString(data_ch);
			}
			else if (ELEM3(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME)) {
				ret = PyC_UnicodeFromByte(data_ch);
			}
			else {
				ret = PyUnicode_FromString(data_ch);
			}
#else
			if (subtype == PROP_BYTESTRING) {
				ret = PyBytes_FromString(buf);
			}
			else {
				ret = PyUnicode_FromString(data_ch);
			}
#endif

#ifdef USE_STRING_COERCE
			Py_XDECREF(value_coerce);
#endif

			break;
		}
		case PROP_ENUM:
		{
			ret = pyrna_enum_to_py(ptr, prop, *(int *)data);
			break;
		}
		case PROP_POINTER:
		{
			PointerRNA newptr;
			StructRNA *ptype = RNA_property_pointer_type(ptr, prop);

			if (flag & PROP_RNAPTR) {
				/* in this case we get the full ptr */
				newptr = *(PointerRNA *)data;
			}
			else {
				if (RNA_struct_is_ID(ptype)) {
					RNA_id_pointer_create(*(void **)data, &newptr);
				}
				else {
					/* note: this is taken from the function's ID pointer
					 * and will break if a function returns a pointer from
					 * another ID block, watch this! - it should at least be
					 * easy to debug since they are all ID's */
					RNA_pointer_create(ptr->id.data, ptype, *(void **)data, &newptr);
				}
			}

			if (newptr.data) {
				ret = pyrna_struct_CreatePyObject(&newptr);
			}
			else {
				ret = Py_None;
				Py_INCREF(ret);
			}
			break;
		}
		case PROP_COLLECTION:
		{
			ListBase *lb = (ListBase *)data;
			CollectionPointerLink *link;
			PyObject *linkptr;

			ret = PyList_New(0);

			for (link = lb->first; link; link = link->next) {
				linkptr = pyrna_struct_CreatePyObject(&link->ptr);
				PyList_Append(ret, linkptr);
				Py_DECREF(linkptr);
			}

			break;
		}
		default:
			PyErr_Format(PyExc_TypeError,
			             "RNA Error: unknown type \"%d\" (pyrna_param_to_py)",
			             type);
			ret = NULL;
			break;
		}
	}

	return ret;
}

/* Use to replace PyDict_GetItemString() when the overhead of converting a
 * string into a python unicode is higher than a non hash lookup.
 * works on small dict's such as keyword args. */
static PyObject *small_dict_get_item_string(PyObject *dict, const char *key_lookup)
{
	PyObject *key = NULL;
	Py_ssize_t pos = 0;
	PyObject *value = NULL;

	while (PyDict_Next(dict, &pos, &key, &value)) {
		if (PyUnicode_Check(key)) {
			if (strcmp(key_lookup, _PyUnicode_AsString(key)) == 0) {
				return value;
			}
		}
	}

	return NULL;
}

static PyObject *pyrna_func_call(BPy_FunctionRNA *self, PyObject *args, PyObject *kw)
{
	/* Note, both BPy_StructRNA and BPy_PropertyRNA can be used here */
	PointerRNA *self_ptr = &self->ptr;
	FunctionRNA *self_func = self->func;

	PointerRNA funcptr;
	ParameterList parms;
	ParameterIterator iter;
	PropertyRNA *parm;
	PyObject *ret, *item;
	int i, pyargs_len, pykw_len, parms_len, ret_len, flag, err = 0, kw_tot = 0, kw_arg;

	PropertyRNA *pret_single = NULL;
	void *retdata_single = NULL;

	/* enable this so all strings are copied and freed after calling.
	 * this exposes bugs where the pointer to the string is held and re-used */
// #define DEBUG_STRING_FREE

#ifdef DEBUG_STRING_FREE
	PyObject *string_free_ls = PyList_New(0);
#endif

	/* Should never happen but it does in rare cases */
	BLI_assert(self_ptr != NULL);

	if (self_ptr == NULL) {
		PyErr_SetString(PyExc_RuntimeError, "rna functions internal rna pointer is NULL, this is a bug. aborting");
		return NULL;
	}

	if (self_func == NULL) {
		PyErr_Format(PyExc_RuntimeError,
		             "%.200s.<unknown>(): rna function internal function is NULL, this is a bug. aborting",
		             RNA_struct_identifier(self_ptr->type));
		return NULL;
	}

	/* for testing */
	/*
	{
		const char *fn;
		int lineno;
		PyC_FileAndNum(&fn, &lineno);
		printf("pyrna_func_call > %.200s.%.200s : %.200s:%d\n",
		       RNA_struct_identifier(self_ptr->type), RNA_function_identifier(self_func), fn, lineno);
	}
	*/

	/* include the ID pointer for pyrna_param_to_py() so we can include the
	 * ID pointer on return values, this only works when returned values have
	 * the same ID as the functions. */
	RNA_pointer_create(self_ptr->id.data, &RNA_Function, self_func, &funcptr);

	pyargs_len = PyTuple_GET_SIZE(args);
	pykw_len = kw ? PyDict_Size(kw) : 0;

	RNA_parameter_list_create(&parms, self_ptr, self_func);
	RNA_parameter_list_begin(&parms, &iter);
	parms_len = RNA_parameter_list_arg_count(&parms);
	ret_len = 0;

	if (pyargs_len + pykw_len > parms_len) {
		RNA_parameter_list_end(&iter);
		PyErr_Format(PyExc_TypeError,
		             "%.200s.%.200s(): takes at most %d arguments, got %d",
		             RNA_struct_identifier(self_ptr->type), RNA_function_identifier(self_func),
		             parms_len, pyargs_len + pykw_len);
		err = -1;
	}

	/* parse function parameters */
	for (i = 0; iter.valid && err == 0; RNA_parameter_list_next(&iter)) {
		parm = iter.parm;
		flag = RNA_property_flag(parm);

		/* only useful for single argument returns, we'll need another list loop for multiple */
		if (flag & PROP_OUTPUT) {
			ret_len++;
			if (pret_single == NULL) {
				pret_single = parm;
				retdata_single = iter.data;
			}

			continue;
		}

		item = NULL;

		if (i < pyargs_len) {
			item = PyTuple_GET_ITEM(args, i);
			kw_arg = FALSE;
		}
		else if (kw != NULL) {
#if 0
			item = PyDict_GetItemString(kw, RNA_property_identifier(parm)); /* borrow ref */
#else
			item = small_dict_get_item_string(kw, RNA_property_identifier(parm)); /* borrow ref */
#endif
			if (item)
				kw_tot++; /* make sure invalid keywords are not given */

			kw_arg = TRUE;
		}

		i++; /* current argument */

		if (item == NULL) {
			if (flag & PROP_REQUIRED) {
				PyErr_Format(PyExc_TypeError,
				             "%.200s.%.200s(): required parameter \"%.200s\" not specified",
				             RNA_struct_identifier(self_ptr->type),
				             RNA_function_identifier(self_func),
				             RNA_property_identifier(parm));
				err = -1;
				break;
			}
			else { /* PyDict_GetItemString wont raise an error */
				continue;
			}
		}

#ifdef DEBUG_STRING_FREE
		if (item) {
			if (PyUnicode_Check(item)) {
				item = PyUnicode_FromString(_PyUnicode_AsString(item));
				PyList_Append(string_free_ls, item);
				Py_DECREF(item);
			}
		}
#endif
		err = pyrna_py_to_prop(&funcptr, parm, iter.data, item, "");

		if (err != 0) {
			/* the error generated isn't that useful, so generate it again with a useful prefix
			 * could also write a function to prepend to error messages */
			char error_prefix[512];
			PyErr_Clear(); /* re-raise */

			if (kw_arg == TRUE)
				BLI_snprintf(error_prefix, sizeof(error_prefix),
				             "%.200s.%.200s(): error with keyword argument \"%.200s\" - ",
				             RNA_struct_identifier(self_ptr->type),
				             RNA_function_identifier(self_func),
				             RNA_property_identifier(parm));
			else
				BLI_snprintf(error_prefix, sizeof(error_prefix),
				             "%.200s.%.200s(): error with argument %d, \"%.200s\" - ",
				             RNA_struct_identifier(self_ptr->type),
				             RNA_function_identifier(self_func),
				             i,
				             RNA_property_identifier(parm));

			pyrna_py_to_prop(&funcptr, parm, iter.data, item, error_prefix);

			break;
		}
	}

	RNA_parameter_list_end(&iter);

	/* Check if we gave args that don't exist in the function
	 * printing the error is slow but it should only happen when developing.
	 * the if below is quick, checking if it passed less keyword args then we gave.
	 * (Dont overwrite the error if we have one, otherwise can skip important messages and confuse with args)
	 */
	if (err == 0 && kw && (pykw_len > kw_tot)) {
		PyObject *key, *value;
		Py_ssize_t pos = 0;

		DynStr *bad_args = BLI_dynstr_new();
		DynStr *good_args = BLI_dynstr_new();

		const char *arg_name, *bad_args_str, *good_args_str;
		int found = FALSE, first = TRUE;

		while (PyDict_Next(kw, &pos, &key, &value)) {

			arg_name = _PyUnicode_AsString(key);
			found = FALSE;

			if (arg_name == NULL) { /* unlikely the argname is not a string but ignore if it is*/
				PyErr_Clear();
			}
			else {
				/* Search for arg_name */
				RNA_parameter_list_begin(&parms, &iter);
				for (; iter.valid; RNA_parameter_list_next(&iter)) {
					parm = iter.parm;
					if (strcmp(arg_name, RNA_property_identifier(parm)) == 0) {
						found = TRUE;
						break;
					}
				}

				RNA_parameter_list_end(&iter);

				if (found == FALSE) {
					BLI_dynstr_appendf(bad_args, first ? "%s" : ", %s", arg_name);
					first = FALSE;
				}
			}
		}

		/* list good args */
		first = TRUE;

		RNA_parameter_list_begin(&parms, &iter);
		for (; iter.valid; RNA_parameter_list_next(&iter)) {
			parm = iter.parm;
			if (RNA_property_flag(parm) & PROP_OUTPUT)
				continue;

			BLI_dynstr_appendf(good_args, first ? "%s" : ", %s", RNA_property_identifier(parm));
			first = FALSE;
		}
		RNA_parameter_list_end(&iter);


		bad_args_str = BLI_dynstr_get_cstring(bad_args);
		good_args_str = BLI_dynstr_get_cstring(good_args);

		PyErr_Format(PyExc_TypeError,
		             "%.200s.%.200s(): was called with invalid keyword arguments(s) (%s), expected (%s)",
		             RNA_struct_identifier(self_ptr->type), RNA_function_identifier(self_func),
		             bad_args_str, good_args_str);

		BLI_dynstr_free(bad_args);
		BLI_dynstr_free(good_args);
		MEM_freeN((void *)bad_args_str);
		MEM_freeN((void *)good_args_str);

		err = -1;
	}

	ret = NULL;
	if (err == 0) {
		/* call function */
		ReportList reports;
		bContext *C = BPy_GetContext();

		BKE_reports_init(&reports, RPT_STORE);
		RNA_function_call(C, &reports, self_ptr, self_func, &parms);

		err = (BPy_reports_to_error(&reports, PyExc_RuntimeError, TRUE));

		/* return value */
		if (err != -1) {
			if (ret_len > 0) {
				if (ret_len > 1) {
					ret = PyTuple_New(ret_len);
					i = 0; /* arg index */

					RNA_parameter_list_begin(&parms, &iter);

					for (; iter.valid; RNA_parameter_list_next(&iter)) {
						parm = iter.parm;
						flag = RNA_property_flag(parm);

						if (flag & PROP_OUTPUT)
							PyTuple_SET_ITEM(ret, i++, pyrna_param_to_py(&funcptr, parm, iter.data));
					}

					RNA_parameter_list_end(&iter);
				}
				else
					ret = pyrna_param_to_py(&funcptr, pret_single, retdata_single);

				/* possible there is an error in conversion */
				if (ret == NULL)
					err = -1;
			}
		}
	}


#ifdef DEBUG_STRING_FREE
	/*
	if (PyList_GET_SIZE(string_free_ls)) {
		printf("%.200s.%.200s():  has %d strings\n",
		       RNA_struct_identifier(self_ptr->type),
		       RNA_function_identifier(self_func),
		       (int)PyList_GET_SIZE(string_free_ls));
	}
	 */
	Py_DECREF(string_free_ls);
#undef DEBUG_STRING_FREE
#endif

	/* cleanup */
	RNA_parameter_list_end(&iter);
	RNA_parameter_list_free(&parms);

	if (ret)
		return ret;

	if (err == -1)
		return NULL;

	Py_RETURN_NONE;
}


/* subclasses of pyrna_struct_Type which support idprop definitions use this as a metaclass */
/* note: tp_base member is set to &PyType_Type on init */
PyTypeObject pyrna_struct_meta_idprop_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_struct_meta_idprop",   /* tp_name */

	/* NOTE! would be PyTypeObject, but subtypes of Type must be PyHeapTypeObject's */
	sizeof(PyHeapTypeObject),   /* tp_basicsize */

	0,                          /* tp_itemsize */
	/* methods */
	NULL,                       /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* deprecated in python 3.0! */
	NULL,                       /* tp_repr */

	/* Method suites for standard classes */
	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */
	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL /*(getattrofunc) pyrna_struct_meta_idprop_getattro*/, /* getattrofunc tp_getattro; */
	(setattrofunc) pyrna_struct_meta_idprop_setattro, /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* long tp_flags; */

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
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,                       /* struct PyGetSetDef *tp_getset; */
#if defined(_MSC_VER) || defined(FREE_WINDOWS)
	NULL, /* defer assignment */
#else
	&PyType_Type,                       /* struct _typeobject *tp_base; */
#endif
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


/*-----------------------BPy_StructRNA method def------------------------------*/
PyTypeObject pyrna_struct_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_struct",               /* tp_name */
	sizeof(BPy_StructRNA),      /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	(destructor) pyrna_struct_dealloc,/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	(reprfunc) pyrna_struct_repr, /* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&pyrna_struct_as_sequence,  /* PySequenceMethods *tp_as_sequence; */
	&pyrna_struct_as_mapping,   /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	(hashfunc) pyrna_struct_hash, /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	(reprfunc) pyrna_struct_str, /* reprfunc tp_str; */
	(getattrofunc) pyrna_struct_getattro, /* getattrofunc tp_getattro; */
	(setattrofunc) pyrna_struct_setattro, /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC, /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
#ifdef USE_PYRNA_STRUCT_REFERENCE
	(traverseproc) pyrna_struct_traverse, /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	(inquiry)pyrna_struct_clear, /* inquiry tp_clear; */
#else
	NULL,                       /* traverseproc tp_traverse; */

/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	(richcmpfunc)pyrna_struct_richcmp, /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
#ifdef USE_WEAKREFS
	offsetof(BPy_StructRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
	0,
#endif
  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	pyrna_struct_methods,       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	pyrna_struct_getseters,     /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	pyrna_struct_new,           /* newfunc tp_new; */
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

/*-----------------------BPy_PropertyRNA method def------------------------------*/
PyTypeObject pyrna_prop_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_prop",                 /* tp_name */
	sizeof(BPy_PropertyRNA),    /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	(destructor) pyrna_prop_dealloc, /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	(reprfunc) pyrna_prop_repr, /* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	(hashfunc) pyrna_prop_hash, /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	(reprfunc) pyrna_prop_str,  /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	(richcmpfunc)pyrna_prop_richcmp,	/* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
#ifdef USE_WEAKREFS
	offsetof(BPy_PropertyRNA, in_weakreflist),	/* long tp_weaklistoffset; */
#else
	0,
#endif

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	pyrna_prop_methods,         /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	pyrna_prop_getseters,      	/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	pyrna_prop_new,             /* newfunc tp_new; */
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

PyTypeObject pyrna_prop_array_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_prop_array",           /* tp_name */
	sizeof(BPy_PropertyArrayRNA),			/* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	(destructor)pyrna_prop_array_dealloc, /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	NULL,/* subclassed */       /* tp_repr */

	/* Method suites for standard classes */

	&pyrna_prop_array_as_number,   /* PyNumberMethods *tp_as_number; */
	&pyrna_prop_array_as_sequence, /* PySequenceMethods *tp_as_sequence; */
	&pyrna_prop_array_as_mapping,  /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	(getattrofunc) pyrna_prop_array_getattro, /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL, /* subclassed */ /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
#ifdef USE_WEAKREFS
	offsetof(BPy_PropertyArrayRNA, in_weakreflist),	/* long tp_weaklistoffset; */
#else
	0,
#endif
  /*** Added in release 2.2 ***/
	/*   Iterators */
	(getiterfunc)pyrna_prop_array_iter,	/* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	pyrna_prop_array_methods,   /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL /*pyrna_prop_getseters*/, /* struct PyGetSetDef *tp_getset; */
	&pyrna_prop_Type,           /* struct _typeobject *tp_base; */
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

PyTypeObject pyrna_prop_collection_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_prop_collection",      /* tp_name */
	sizeof(BPy_PropertyRNA),    /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	(destructor)pyrna_prop_dealloc, /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	NULL, /* subclassed */		/* tp_repr */

	/* Method suites for standard classes */

	&pyrna_prop_collection_as_number,   /* PyNumberMethods *tp_as_number; */
	&pyrna_prop_collection_as_sequence, /* PySequenceMethods *tp_as_sequence; */
	&pyrna_prop_collection_as_mapping,  /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	(getattrofunc) pyrna_prop_collection_getattro, /* getattrofunc tp_getattro; */
	(setattrofunc) pyrna_prop_collection_setattro, /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL, /* subclassed */		/* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
#ifdef USE_WEAKREFS
	offsetof(BPy_PropertyRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
	0,
#endif

  /*** Added in release 2.2 ***/
	/*   Iterators */
	(getiterfunc)pyrna_prop_collection_iter, /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	pyrna_prop_collection_methods, /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL /*pyrna_prop_getseters*/, /* struct PyGetSetDef *tp_getset; */
	&pyrna_prop_Type,           /* struct _typeobject *tp_base; */
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

/* only for add/remove/move methods */
static PyTypeObject pyrna_prop_collection_idprop_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_prop_collection_idprop", /* tp_name */
	sizeof(BPy_PropertyRNA),    /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	(destructor)pyrna_prop_dealloc, /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	NULL, /* subclassed */      /* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL, /* subclassed */		/* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
#ifdef USE_WEAKREFS
	offsetof(BPy_PropertyRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
	0,
#endif

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	pyrna_prop_collection_idprop_methods, /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL /*pyrna_prop_getseters*/, /* struct PyGetSetDef *tp_getset; */
	&pyrna_prop_collection_Type,/* struct _typeobject *tp_base; */
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

/*-----------------------BPy_PropertyRNA method def------------------------------*/
PyTypeObject pyrna_func_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_func",                 /* tp_name */
	sizeof(BPy_FunctionRNA),    /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	NULL,                       /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	(reprfunc) pyrna_func_repr, /* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	(ternaryfunc)pyrna_func_call, /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
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
#ifdef USE_WEAKREFS
	offsetof(BPy_PropertyRNA, in_weakreflist),	/* long tp_weaklistoffset; */
#else
	0,
#endif

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
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

#ifdef USE_PYRNA_ITER
/* --- collection iterator: start --- */
/* wrap rna collection iterator functions */
/*
 * RNA_property_collection_begin(...)
 * RNA_property_collection_next(...)
 * RNA_property_collection_end(...)
 */

static void pyrna_prop_collection_iter_dealloc(BPy_PropertyCollectionIterRNA *self);
static PyObject *pyrna_prop_collection_iter_next(BPy_PropertyCollectionIterRNA *self);

PyTypeObject pyrna_prop_collection_iter_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"bpy_prop_collection_iter", /* tp_name */
	sizeof(BPy_PropertyCollectionIterRNA), /* tp_basicsize */
	0,                          /* tp_itemsize */
	/* methods */
	(destructor)pyrna_prop_collection_iter_dealloc, /* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,                       /* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,                       /* tp_compare */ /* DEPRECATED in python 3.0! */
	NULL,/* subclassed */		/* tp_repr */

	/* Method suites for standard classes */

	NULL,    /* PyNumberMethods *tp_as_number; */
	NULL,                       /* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
#if defined(_MSC_VER) || defined(FREE_WINDOWS)
	NULL, /* defer assignment */
#else
	PyObject_GenericGetAttr,    /* getattrofunc tp_getattro; */
#endif
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
	NULL, /* subclassed */		/* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
#ifdef USE_WEAKREFS
	offsetof(BPy_PropertyCollectionIterRNA, in_weakreflist), /* long tp_weaklistoffset; */
#else
	0,
#endif
  /*** Added in release 2.2 ***/
	/*   Iterators */
#if defined(_MSC_VER) || defined(FREE_WINDOWS)
	NULL, /* defer assignment */
#else
	PyObject_SelfIter,          /* getiterfunc tp_iter; */
#endif
	(iternextfunc) pyrna_prop_collection_iter_next, /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
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

PyObject *pyrna_prop_collection_iter_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop)
{
	BPy_PropertyCollectionIterRNA *self = PyObject_New(BPy_PropertyCollectionIterRNA, &pyrna_prop_collection_iter_Type);

#ifdef USE_WEAKREFS
	self->in_weakreflist = NULL;
#endif

	RNA_property_collection_begin(ptr, prop, &self->iter);

	return (PyObject *)self;
}

static PyObject *pyrna_prop_collection_iter(BPy_PropertyRNA *self)
{
	return pyrna_prop_collection_iter_CreatePyObject(&self->ptr, self->prop);
}

static PyObject *pyrna_prop_collection_iter_next(BPy_PropertyCollectionIterRNA *self)
{
	if (self->iter.valid == FALSE) {
		PyErr_SetString(PyExc_StopIteration, "pyrna_prop_collection_iter stop");
		return NULL;
	}
	else {
		BPy_StructRNA *pyrna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&self->iter.ptr);

#ifdef USE_PYRNA_STRUCT_REFERENCE
		if (pyrna) { /* unlikely but may fail */
			if ((PyObject *)pyrna != Py_None) {
				/* hold a reference to the iterator since it may have
				 * allocated memory 'pyrna' needs. eg: introspecting dynamic enum's  */
				/* TODO, we could have an api call to know if this is needed since most collections don't */
				pyrna_struct_reference_set(pyrna, (PyObject *)self);
			}
		}
#endif /* !USE_PYRNA_STRUCT_REFERENCE */

		RNA_property_collection_next(&self->iter);

		return (PyObject *)pyrna;
	}
}


static void pyrna_prop_collection_iter_dealloc(BPy_PropertyCollectionIterRNA *self)
{
#ifdef USE_WEAKREFS
	if (self->in_weakreflist != NULL) {
		PyObject_ClearWeakRefs((PyObject *)self);
	}
#endif

	RNA_property_collection_end(&self->iter);

	PyObject_DEL(self);
}

/* --- collection iterator: end --- */
#endif /* !USE_PYRNA_ITER */


static void pyrna_subtype_set_rna(PyObject *newclass, StructRNA *srna)
{
	PointerRNA ptr;
	PyObject *item;

	Py_INCREF(newclass);

	if (RNA_struct_py_type_get(srna))
		PyC_ObSpit("RNA WAS SET - ", RNA_struct_py_type_get(srna));

	Py_XDECREF(((PyObject *)RNA_struct_py_type_get(srna)));

	RNA_struct_py_type_set(srna, (void *)newclass); /* Store for later use */

	/* Not 100% needed but useful,
	 * having an instance within a type looks wrong however this instance IS an rna type */

	/* python deals with the circular ref */
	RNA_pointer_create(NULL, &RNA_Struct, srna, &ptr);
	item = pyrna_struct_CreatePyObject(&ptr);

	/* note, must set the class not the __dict__ else the internal slots are not updated correctly */
	PyObject_SetAttr(newclass, bpy_intern_str_bl_rna, item);
	Py_DECREF(item);

	/* done with rna instance */
}

static PyObject* pyrna_srna_Subtype(StructRNA *srna);

/* return a borrowed reference */
static PyObject* pyrna_srna_PyBase(StructRNA *srna) //, PyObject *bpy_types_dict)
{
	/* Assume RNA_struct_py_type_get(srna) was already checked */
	StructRNA *base;

	PyObject *py_base = NULL;

	/* get the base type */
	base = RNA_struct_base(srna);

	if (base && base != srna) {
		/*/printf("debug subtype %s %p\n", RNA_struct_identifier(srna), srna); */
		py_base = pyrna_srna_Subtype(base); //, bpy_types_dict);
		Py_DECREF(py_base); /* srna owns, this is only to pass as an arg */
	}

	if (py_base == NULL) {
		py_base = (PyObject *)&pyrna_struct_Type;
	}

	return py_base;
}

/* check if we have a native python subclass, use it when it exists
 * return a borrowed reference */
static PyObject *bpy_types_dict = NULL;

static PyObject* pyrna_srna_ExternalType(StructRNA *srna)
{
	const char *idname = RNA_struct_identifier(srna);
	PyObject *newclass;

	if (bpy_types_dict == NULL) {
		PyObject *bpy_types = PyImport_ImportModuleLevel((char *)"bpy_types", NULL, NULL, NULL, 0);

		if (bpy_types == NULL) {
			PyErr_Print();
			PyErr_Clear();
			fprintf(stderr, "%s: failed to find 'bpy_types' module\n", __func__);
			return NULL;
		}
		bpy_types_dict = PyModule_GetDict(bpy_types); // borrow
		Py_DECREF(bpy_types); // fairly safe to assume the dict is kept
	}

	newclass = PyDict_GetItemString(bpy_types_dict, idname);

	/* sanity check, could skip this unless in debug mode */
	if (newclass) {
		PyObject *base_compare = pyrna_srna_PyBase(srna);
		//PyObject *slots = PyObject_GetAttrString(newclass, "__slots__"); // cant do this because it gets superclasses values!
		//PyObject *bases = PyObject_GetAttrString(newclass, "__bases__"); // can do this but faster not to.
		PyObject *tp_bases = ((PyTypeObject *)newclass)->tp_bases;
		PyObject *tp_slots = PyDict_GetItem(((PyTypeObject *)newclass)->tp_dict, bpy_intern_str___slots__);

		if (tp_slots == NULL) {
			fprintf(stderr, "%s: expected class '%s' to have __slots__ defined\n\nSee bpy_types.py\n", __func__, idname);
			newclass = NULL;
		}
		else if (PyTuple_GET_SIZE(tp_bases)) {
			PyObject *base = PyTuple_GET_ITEM(tp_bases, 0);

			if (base_compare != base) {
				fprintf(stderr, "%s: incorrect subclassing of SRNA '%s'\nSee bpy_types.py\n", __func__, idname);
				PyC_ObSpit("Expected! ", base_compare);
				newclass = NULL;
			}
			else {
				if (G.f & G_DEBUG)
					fprintf(stderr, "SRNA Subclassed: '%s'\n", idname);
			}
		}
	}

	return newclass;
}

static PyObject* pyrna_srna_Subtype(StructRNA *srna)
{
	PyObject *newclass = NULL;

		/* stupid/simple case */
	if (srna == NULL) {
		newclass = NULL; /* Nothing to do */
	}	/* the class may have already been declared & allocated */
	else if ((newclass = RNA_struct_py_type_get(srna))) {
		Py_INCREF(newclass);
	}	/* check if bpy_types.py module has the class defined in it */
	else if ((newclass = pyrna_srna_ExternalType(srna))) {
		pyrna_subtype_set_rna(newclass, srna);
		Py_INCREF(newclass);
	}	/* create a new class instance with the C api
		 * mainly for the purposing of matching the C/rna type hierarchy */
	else {
		/* subclass equivalents
		- class myClass(myBase):
			some = 'value' # or ...
		- myClass = type(name='myClass', bases=(myBase,), dict={'__module__':'bpy.types'})
		*/

		/* Assume RNA_struct_py_type_get(srna) was already checked */
		PyObject *py_base = pyrna_srna_PyBase(srna);
		PyObject *metaclass;
		const char *idname = RNA_struct_identifier(srna);

		/* remove __doc__ for now */
		// const char *descr = RNA_struct_ui_description(srna);
		// if (!descr) descr = "(no docs)";
		// "__doc__", descr

		if ( RNA_struct_idprops_check(srna) &&
		     !PyObject_IsSubclass(py_base, (PyObject *)&pyrna_struct_meta_idprop_Type))
		{
			metaclass = (PyObject *)&pyrna_struct_meta_idprop_Type;
		}
		else {
			metaclass = (PyObject *)&PyType_Type;
		}

		/* always use O not N when calling, N causes refcount errors */
		newclass = PyObject_CallFunction(metaclass, (char *)"s(O){sss()}",
		                                 idname, py_base, "__module__","bpy.types", "__slots__");

		/* newclass will now have 2 ref's, ???, probably 1 is internal since decrefing here segfaults */

		/* PyC_ObSpit("new class ref", newclass); */

		if (newclass) {
			/* srna owns one, and the other is owned by the caller */
			pyrna_subtype_set_rna(newclass, srna);

			// XXX, adding this back segfaults blender on load.
			// Py_DECREF(newclass); /* let srna own */
		}
		else {
			/* this should not happen */
			printf("%s: error registering '%s'\n", __func__, idname);
			PyErr_Print();
			PyErr_Clear();
		}
	}

	return newclass;
}

/* use for subtyping so we know which srna is used for a PointerRNA */
static StructRNA *srna_from_ptr(PointerRNA *ptr)
{
	if (ptr->type == &RNA_Struct) {
		return ptr->data;
	}
	else {
		return ptr->type;
	}
}

/* always returns a new ref, be sure to decref when done */
static PyObject* pyrna_struct_Subtype(PointerRNA *ptr)
{
	return pyrna_srna_Subtype(srna_from_ptr(ptr));
}

/*-----------------------CreatePyObject---------------------------------*/
PyObject *pyrna_struct_CreatePyObject(PointerRNA *ptr)
{
	BPy_StructRNA *pyrna = NULL;

	/* note: don't rely on this to return None since NULL data with a valid type can often crash */
	if (ptr->data == NULL && ptr->type == NULL) { /* Operator RNA has NULL data */
		Py_RETURN_NONE;
	}
	else {
		PyTypeObject *tp = (PyTypeObject *)pyrna_struct_Subtype(ptr);

		if (tp) {
			pyrna = (BPy_StructRNA *) tp->tp_alloc(tp, 0);
			Py_DECREF(tp); /* srna owns, cant hold a ref */
		}
		else {
			fprintf(stderr, "%s: could not make type\n", __func__);
			pyrna = (BPy_StructRNA *) PyObject_GC_New(BPy_StructRNA, &pyrna_struct_Type);
#ifdef USE_WEAKREFS
			pyrna->in_weakreflist = NULL;
#endif
		}
	}

	if (pyrna == NULL) {
		PyErr_SetString(PyExc_MemoryError, "couldn't create bpy_struct object");
		return NULL;
	}

	pyrna->ptr = *ptr;
#ifdef PYRNA_FREE_SUPPORT
	pyrna->freeptr = FALSE;
#endif

#ifdef USE_PYRNA_STRUCT_REFERENCE
	pyrna->reference = NULL;
#endif

	// PyC_ObSpit("NewStructRNA: ", (PyObject *)pyrna);

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
	if (ptr->id.data) {
		id_weakref_pool_add(ptr->id.data, (BPy_DummyPointerRNA *)pyrna);
	}
#endif
	return (PyObject *)pyrna;
}

PyObject *pyrna_prop_CreatePyObject(PointerRNA *ptr, PropertyRNA *prop)
{
	BPy_PropertyRNA *pyrna;

	if (RNA_property_array_check(prop) == 0) {
		PyTypeObject *type;

		if (RNA_property_type(prop) != PROP_COLLECTION) {
			type = &pyrna_prop_Type;
		}
		else {
			if ((RNA_property_flag(prop) & PROP_IDPROPERTY) == 0) {
				type = &pyrna_prop_collection_Type;
			}
			else {
				type = &pyrna_prop_collection_idprop_Type;
			}
		}

		pyrna = (BPy_PropertyRNA *) PyObject_NEW(BPy_PropertyRNA, type);
#ifdef USE_WEAKREFS
		pyrna->in_weakreflist = NULL;
#endif
	}
	else {
		pyrna = (BPy_PropertyRNA *) PyObject_NEW(BPy_PropertyArrayRNA, &pyrna_prop_array_Type);
		((BPy_PropertyArrayRNA *)pyrna)->arraydim = 0;
		((BPy_PropertyArrayRNA *)pyrna)->arrayoffset = 0;
#ifdef USE_WEAKREFS
		((BPy_PropertyArrayRNA *)pyrna)->in_weakreflist = NULL;
#endif
	}

	if (pyrna == NULL) {
		PyErr_SetString(PyExc_MemoryError, "couldn't create BPy_rna object");
		return NULL;
	}

	pyrna->ptr = *ptr;
	pyrna->prop = prop;

#ifdef USE_PYRNA_INVALIDATE_WEAKREF
	if (ptr->id.data) {
		id_weakref_pool_add(ptr->id.data, (BPy_DummyPointerRNA *)pyrna);
	}
#endif

	return (PyObject *)pyrna;
}

void BPY_rna_init(void)
{
#ifdef USE_MATHUTILS // register mathutils callbacks, ok to run more then once.
	mathutils_rna_array_cb_index = Mathutils_RegisterCallback(&mathutils_rna_array_cb);
	mathutils_rna_matrix_cb_index = Mathutils_RegisterCallback(&mathutils_rna_matrix_cb);
#endif

	/* for some reason MSVC complains of these */
#if defined(_MSC_VER) || defined(FREE_WINDOWS)
	pyrna_struct_meta_idprop_Type.tp_base = &PyType_Type;

	pyrna_prop_collection_iter_Type.tp_iter = PyObject_SelfIter;
	pyrna_prop_collection_iter_Type.tp_getattro = PyObject_GenericGetAttr;
#endif

	/* metaclass */
	if (PyType_Ready(&pyrna_struct_meta_idprop_Type) < 0)
		return;

	if (PyType_Ready(&pyrna_struct_Type) < 0)
		return;

	if (PyType_Ready(&pyrna_prop_Type) < 0)
		return;

	if (PyType_Ready(&pyrna_prop_array_Type) < 0)
		return;

	if (PyType_Ready(&pyrna_prop_collection_Type) < 0)
		return;

	if (PyType_Ready(&pyrna_prop_collection_idprop_Type) < 0)
		return;

	if (PyType_Ready(&pyrna_func_Type) < 0)
		return;

#ifdef USE_PYRNA_ITER
	if (PyType_Ready(&pyrna_prop_collection_iter_Type) < 0)
		return;
#endif
}

/* bpy.data from python */
static PointerRNA *rna_module_ptr = NULL;
PyObject *BPY_rna_module(void)
{
	BPy_StructRNA *pyrna;
	PointerRNA ptr;

	/* for now, return the base RNA type rather than a real module */
	RNA_main_pointer_create(G.main, &ptr);
	pyrna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);

	rna_module_ptr = &pyrna->ptr;
	return (PyObject *)pyrna;
}

void BPY_update_rna_module(void)
{
#if 0
	RNA_main_pointer_create(G.main, rna_module_ptr);
#else
	rna_module_ptr->data = G.main; /* just set data is enough */
#endif
}

#if 0
/* This is a way we can access docstrings for RNA types
 * without having the datatypes in blender */
PyObject *BPY_rna_doc(void)
{
	PointerRNA ptr;

	/* for now, return the base RNA type rather than a real module */
	RNA_blender_rna_pointer_create(&ptr);

	return pyrna_struct_CreatePyObject(&ptr);
}
#endif


/* pyrna_basetype_* - BPy_BaseTypeRNA is just a BPy_PropertyRNA struct with a different type
 * the self->ptr and self->prop are always set to the "structs" collection */
//---------------getattr--------------------------------------------
static PyObject *pyrna_basetype_getattro(BPy_BaseTypeRNA *self, PyObject *pyname)
{
	PointerRNA newptr;
	PyObject *ret;
	const char *name = _PyUnicode_AsString(pyname);

	if (name == NULL) {
		PyErr_SetString(PyExc_AttributeError, "bpy.types: __getattr__ must be a string");
		ret = NULL;
	}
	else if (RNA_property_collection_lookup_string(&self->ptr, self->prop, name, &newptr)) {
		ret = pyrna_struct_Subtype(&newptr);
		if (ret == NULL) {
			PyErr_Format(PyExc_RuntimeError,
			             "bpy.types.%.200s subtype could not be generated, this is a bug!",
			             _PyUnicode_AsString(pyname));
		}
	}
	else {
#if 0
		PyErr_Format(PyExc_AttributeError,
		             "bpy.types.%.200s RNA_Struct does not exist",
		             _PyUnicode_AsString(pyname));
		return NULL;
#endif
		/* The error raised here will be displayed */
		ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
	}

	return ret;
}

static PyObject *pyrna_basetype_dir(BPy_BaseTypeRNA *self);
static PyObject *pyrna_register_class(PyObject *self, PyObject *py_class);
static PyObject *pyrna_unregister_class(PyObject *self, PyObject *py_class);

static struct PyMethodDef pyrna_basetype_methods[] = {
	{"__dir__", (PyCFunction)pyrna_basetype_dir, METH_NOARGS, ""},
	{NULL, NULL, 0, NULL}
};

/* used to call ..._keys() direct, but we need to filter out operator subclasses */
#if 0
static PyObject *pyrna_basetype_dir(BPy_BaseTypeRNA *self)
{
	PyObject *list;
#if 0
	PyObject *name;
	PyMethodDef *meth;
#endif

	list = pyrna_prop_collection_keys(self); /* like calling structs.keys(), avoids looping here */

#if 0 /* for now only contains __dir__ */
	for (meth = pyrna_basetype_methods; meth->ml_name; meth++) {
		name = PyUnicode_FromString(meth->ml_name);
		PyList_Append(list, name);
		Py_DECREF(name);
	}
#endif
	return list;
}

#else

static PyObject *pyrna_basetype_dir(BPy_BaseTypeRNA *self)
{
	PyObject *ret = PyList_New(0);
	PyObject *item;

	RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
		StructRNA *srna = itemptr.data;
		StructRNA *srna_base = RNA_struct_base(itemptr.data);
		/* skip own operators, these double up [#29666] */
		if (srna_base == &RNA_Operator) {
			/* do nothing */
		}
		else {
			/* add to python list */
			item = PyUnicode_FromString(RNA_struct_identifier(srna));
			PyList_Append(ret, item);
			Py_DECREF(item);
		}
	}
	RNA_PROP_END;

	return ret;
}

#endif

static PyTypeObject pyrna_basetype_Type = BLANK_PYTHON_TYPE;

PyObject *BPY_rna_types(void)
{
	BPy_BaseTypeRNA *self;

	if ((pyrna_basetype_Type.tp_flags & Py_TPFLAGS_READY) == 0) {
		pyrna_basetype_Type.tp_name = "RNA_Types";
		pyrna_basetype_Type.tp_basicsize = sizeof(BPy_BaseTypeRNA);
		pyrna_basetype_Type.tp_getattro = (getattrofunc) pyrna_basetype_getattro;
		pyrna_basetype_Type.tp_flags = Py_TPFLAGS_DEFAULT;
		pyrna_basetype_Type.tp_methods = pyrna_basetype_methods;

		if (PyType_Ready(&pyrna_basetype_Type) < 0)
			return NULL;
	}

	self = (BPy_BaseTypeRNA *)PyObject_NEW(BPy_BaseTypeRNA, &pyrna_basetype_Type);

	/* avoid doing this lookup for every getattr */
	RNA_blender_rna_pointer_create(&self->ptr);
	self->prop = RNA_struct_find_property(&self->ptr, "structs");
#ifdef USE_WEAKREFS
	self->in_weakreflist = NULL;
#endif
	return (PyObject *)self;
}

StructRNA *pyrna_struct_as_srna(PyObject *self, int parent, const char *error_prefix)
{
	BPy_StructRNA *py_srna = NULL;
	StructRNA *srna;

	/* ack, PyObject_GetAttrString wont look up this types tp_dict first :/ */
	if (PyType_Check(self)) {
		py_srna = (BPy_StructRNA *)PyDict_GetItem(((PyTypeObject *)self)->tp_dict, bpy_intern_str_bl_rna);
		Py_XINCREF(py_srna);
	}

	if (parent) {
		/* be very careful with this since it will return a parent classes srna.
		 * modifying this will do confusing stuff! */
		if (py_srna == NULL)
			py_srna = (BPy_StructRNA *)PyObject_GetAttr(self, bpy_intern_str_bl_rna);
	}

	if (py_srna == NULL) {
		PyErr_Format(PyExc_RuntimeError,
		             "%.200s, missing bl_rna attribute from '%.200s' instance (may not be registered)",
		             error_prefix, Py_TYPE(self)->tp_name);
		return NULL;
	}

	if (!BPy_StructRNA_Check(py_srna)) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s, bl_rna attribute wrong type '%.200s' on '%.200s'' instance",
		             error_prefix, Py_TYPE(py_srna)->tp_name,
		             Py_TYPE(self)->tp_name);
		Py_DECREF(py_srna);
		return NULL;
	}

	if (py_srna->ptr.type != &RNA_Struct) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s, bl_rna attribute not a RNA_Struct, on '%.200s'' instance",
		             error_prefix, Py_TYPE(self)->tp_name);
		Py_DECREF(py_srna);
		return NULL;
	}

	srna = py_srna->ptr.data;
	Py_DECREF(py_srna);

	return srna;
}

/* Orphan functions, not sure where they should go */
/* get the srna for methods attached to types */
/*
 * Caller needs to raise error.*/
StructRNA *srna_from_self(PyObject *self, const char *error_prefix)
{

	if (self == NULL) {
		return NULL;
	}
	else if (PyCapsule_CheckExact(self)) {
		return PyCapsule_GetPointer(self, NULL);
	}
	else if (PyType_Check(self) == 0) {
		return NULL;
	}
	else {
		/* These cases above not errors, they just mean the type was not compatible
		 * After this any errors will be raised in the script */

		PyObject *error_type, *error_value, *error_traceback;
		StructRNA *srna;

		PyErr_Fetch(&error_type, &error_value, &error_traceback);
		PyErr_Clear();

		srna = pyrna_struct_as_srna(self, 0, error_prefix);

		if (!PyErr_Occurred()) {
			PyErr_Restore(error_type, error_value, error_traceback);
		}

		return srna;
	}
}

static int deferred_register_prop(StructRNA *srna, PyObject *key, PyObject *item)
{
	/* We only care about results from C which
	 * are for sure types, save some time with error */
	if (pyrna_is_deferred_prop(item)) {

		PyObject *py_func, *py_kw, *py_srna_cobject, *py_ret;

		if (PyArg_ParseTuple(item, "OO!", &py_func, &PyDict_Type, &py_kw)) {
			PyObject *args_fake;

			if (*_PyUnicode_AsString(key) == '_') {
				PyErr_Format(PyExc_ValueError,
				             "bpy_struct \"%.200s\" registration error: "
				             "%.200s could not register because the property starts with an '_'\n",
				             RNA_struct_identifier(srna), _PyUnicode_AsString(key));
				return -1;
			}
			py_srna_cobject = PyCapsule_New(srna, NULL, NULL);

			/* not 100% nice :/, modifies the dict passed, should be ok */
			PyDict_SetItem(py_kw, bpy_intern_str_attr, key);

			args_fake = PyTuple_New(1);
			PyTuple_SET_ITEM(args_fake, 0, py_srna_cobject);

			py_ret = PyObject_Call(py_func, args_fake, py_kw);

			Py_DECREF(args_fake); /* free's py_srna_cobject too */

			if (py_ret) {
				Py_DECREF(py_ret);
			}
			else {
				PyErr_Print();
				PyErr_Clear();

				// PyC_LineSpit();
				PyErr_Format(PyExc_ValueError,
				             "bpy_struct \"%.200s\" registration error: "
				             "%.200s could not register\n",
				             RNA_struct_identifier(srna), _PyUnicode_AsString(key));
				return -1;
			}
		}
		else {
			/* Since this is a class dict, ignore args that can't be passed */

			/* for testing only */
			/* PyC_ObSpit("Why doesn't this work??", item);
			PyErr_Print(); */
			PyErr_Clear();
		}
	}

	return 0;
}

static int pyrna_deferred_register_props(StructRNA *srna, PyObject *class_dict)
{
	PyObject *item, *key;
	PyObject *order;
	Py_ssize_t pos = 0;
	int ret = 0;

	/* in both cases PyDict_CheckExact(class_dict) will be true even
	 * though Operators have a metaclass dict namespace */

	if ((order = PyDict_GetItem(class_dict, bpy_intern_str_order)) && PyList_CheckExact(order)) {
		for (pos = 0; pos < PyList_GET_SIZE(order); pos++) {
			key = PyList_GET_ITEM(order, pos);
			/* however unlikely its possible
			 * fails in py 3.3 beta with __qualname__ */
			if ((item = PyDict_GetItem(class_dict, key))) {
				ret = deferred_register_prop(srna, key, item);
				if (ret != 0) {
					break;
				}
			}
		}
	}
	else {
		while (PyDict_Next(class_dict, &pos, &key, &item)) {
			ret = deferred_register_prop(srna, key, item);

			if (ret != 0)
				break;
		}
	}

	return ret;
}

static int pyrna_deferred_register_class_recursive(StructRNA *srna, PyTypeObject *py_class)
{
	const int len = PyTuple_GET_SIZE(py_class->tp_bases);
	int i, ret;

	/* first scan base classes for registerable properties */
	for (i = 0; i < len; i++) {
		PyTypeObject *py_superclass = (PyTypeObject *)PyTuple_GET_ITEM(py_class->tp_bases, i);

		/* the rules for using these base classes are not clear,
		 * 'object' is of course not worth looking into and
		 * existing subclasses of RNA would cause a lot more dictionary
		 * looping then is needed (SomeOperator would scan Operator.__dict__)
		 * which is harmless but not at all useful.
		 *
		 * So only scan base classes which are not subclasses if blender types.
		 * This best fits having 'mix-in' classes for operators and render engines.
		 * */
		if (py_superclass != &PyBaseObject_Type &&
			!PyObject_IsSubclass((PyObject *)py_superclass, (PyObject *)&pyrna_struct_Type)
		) {
			ret = pyrna_deferred_register_class_recursive(srna, py_superclass);

			if (ret != 0) {
				return ret;
			}
		}
	}

	/* not register out own properties */
	return pyrna_deferred_register_props(srna, py_class->tp_dict); /* getattr(..., "__dict__") returns a proxy */
}

int pyrna_deferred_register_class(StructRNA *srna, PyObject *py_class)
{
	/* Panels and Menus dont need this
	 * save some time and skip the checks here */
	if (!RNA_struct_idprops_register_check(srna))
		return 0;

	return pyrna_deferred_register_class_recursive(srna, (PyTypeObject *)py_class);
}

/*-------------------- Type Registration ------------------------*/

static int rna_function_arg_count(FunctionRNA *func)
{
	const ListBase *lb = RNA_function_defined_parameters(func);
	PropertyRNA *parm;
	Link *link;
	int count = (RNA_function_flag(func) & FUNC_NO_SELF) ? 0 : 1;

	for (link = lb->first; link; link = link->next) {
		parm = (PropertyRNA *)link;
		if (!(RNA_property_flag(parm) & PROP_OUTPUT))
			count++;
	}

	return count;
}

static int bpy_class_validate(PointerRNA *dummyptr, void *py_data, int *have_function)
{
	const ListBase *lb;
	Link *link;
	FunctionRNA *func;
	PropertyRNA *prop;
	StructRNA *srna = dummyptr->type;
	const char *class_type = RNA_struct_identifier(srna);
	PyObject *py_class = (PyObject *)py_data;
	PyObject *base_class = RNA_struct_py_type_get(srna);
	PyObject *item;
	int i, flag, arg_count, func_arg_count;
	const char *py_class_name = ((PyTypeObject *)py_class)->tp_name; // __name__


	if (base_class) {
		if (!PyObject_IsSubclass(py_class, base_class)) {
			PyErr_Format(PyExc_TypeError,
			             "expected %.200s subclass of class \"%.200s\"",
			             class_type, py_class_name);
			return -1;
		}
	}

	/* verify callback functions */
	lb = RNA_struct_type_functions(srna);
	i = 0;
	for (link = lb->first; link; link = link->next) {
		func = (FunctionRNA *)link;
		flag = RNA_function_flag(func);

		if (!(flag & FUNC_REGISTER))
			continue;

		item = PyObject_GetAttrString(py_class, RNA_function_identifier(func));

		have_function[i] = (item != NULL);
		i++;

		if (item == NULL) {
			if ((flag & FUNC_REGISTER_OPTIONAL) == 0) {
				PyErr_Format(PyExc_AttributeError,
				             "expected %.200s, %.200s class to have an \"%.200s\" attribute",
				             class_type, py_class_name,
				             RNA_function_identifier(func));
				return -1;
			}

			PyErr_Clear();
		}
		else {
			Py_DECREF(item); /* no need to keep a ref, the class owns it (technically we should keep a ref but...) */
			if (flag & FUNC_NO_SELF) {
				if (PyMethod_Check(item) == 0) {
					PyErr_Format(PyExc_TypeError,
					             "expected %.200s, %.200s class \"%.200s\" attribute to be a method, not a %.200s",
					             class_type, py_class_name, RNA_function_identifier(func), Py_TYPE(item)->tp_name);
					return -1;
				}
				item = ((PyMethodObject *)item)->im_func;
			}
			else {
				if (PyFunction_Check(item) == 0) {
					PyErr_Format(PyExc_TypeError,
					             "expected %.200s, %.200s class \"%.200s\" attribute to be a function, not a %.200s",
					             class_type, py_class_name, RNA_function_identifier(func), Py_TYPE(item)->tp_name);
					return -1;
				}
			}

			func_arg_count = rna_function_arg_count(func);

			if (func_arg_count >= 0) { /* -1 if we dont care*/
				arg_count = ((PyCodeObject *)PyFunction_GET_CODE(item))->co_argcount;

				/* note, the number of args we check for and the number of args we give to
				 * @classmethods are different (quirk of python),
				 * this is why rna_function_arg_count() doesn't return the value -1*/
				if (flag & FUNC_NO_SELF)
					func_arg_count++;

				if (arg_count != func_arg_count) {
					PyErr_Format(PyExc_ValueError,
					             "expected %.200s, %.200s class \"%.200s\" function to have %d args, found %d",
					             class_type, py_class_name, RNA_function_identifier(func),
					             func_arg_count, arg_count);
					return -1;
				}
			}
		}
	}

	/* verify properties */
	lb = RNA_struct_type_properties(srna);
	for (link = lb->first; link; link = link->next) {
		const char *identifier;
		prop = (PropertyRNA *)link;
		flag = RNA_property_flag(prop);

		if (!(flag & PROP_REGISTER))
			continue;

		identifier = RNA_property_identifier(prop);
		item = PyObject_GetAttrString(py_class, identifier);

		if (item == NULL) {
			/* Sneaky workaround to use the class name as the bl_idname */

#define     BPY_REPLACEMENT_STRING(rna_attr, py_attr)                         \
			if (strcmp(identifier, rna_attr) == 0) {                          \
				item = PyObject_GetAttrString(py_class, py_attr);              \
				if (item && item != Py_None) {                                \
					if (pyrna_py_to_prop(dummyptr, prop, NULL,                \
					                     item, "validating class:") != 0)     \
					{                                                         \
						Py_DECREF(item);                                      \
						return -1;                                            \
					}                                                         \
				}                                                             \
				Py_XDECREF(item);                                             \
			}                                                                 \


			BPY_REPLACEMENT_STRING("bl_idname", "__name__");
			BPY_REPLACEMENT_STRING("bl_description", "__doc__");

#undef		BPY_REPLACEMENT_STRING

			if (item == NULL && (((flag & PROP_REGISTER_OPTIONAL) != PROP_REGISTER_OPTIONAL))) {
				PyErr_Format(PyExc_AttributeError,
				             "expected %.200s, %.200s class to have an \"%.200s\" attribute",
				             class_type, py_class_name, identifier);
				return -1;
			}

			PyErr_Clear();
		}
		else {
			Py_DECREF(item); /* no need to keep a ref, the class owns it */

			if (pyrna_py_to_prop(dummyptr, prop, NULL, item, "validating class:") != 0)
				return -1;
		}
	}

	return 0;
}

/* TODO - multiple return values like with rna functions */
static int bpy_class_call(bContext *C, PointerRNA *ptr, FunctionRNA *func, ParameterList *parms)
{
	PyObject *args;
	PyObject *ret = NULL, *py_srna = NULL, *py_class_instance = NULL, *parmitem;
	PyTypeObject *py_class;
	void **py_class_instance_store = NULL;
	PropertyRNA *parm;
	ParameterIterator iter;
	PointerRNA funcptr;
	int err = 0, i, flag, ret_len = 0;
	const char is_static = (RNA_function_flag(func) & FUNC_NO_SELF) != 0;

	/* annoying!, need to check if the screen gets set to NULL which is a
	 * hint that the file was actually re-loaded. */
	char is_valid_wm;

	PropertyRNA *pret_single = NULL;
	void *retdata_single = NULL;

	PyGILState_STATE gilstate;

#ifdef USE_PEDANTIC_WRITE
	const int is_operator = RNA_struct_is_a(ptr->type, &RNA_Operator);
	const char *func_id = RNA_function_identifier(func);
	/* testing, for correctness, not operator and not draw function */
	const short is_readonly = ((strncmp("draw", func_id, 4) == 0) || /* draw or draw_header */
							 /*strstr("render", func_id) ||*/
							 !is_operator);
#endif

	py_class = RNA_struct_py_type_get(ptr->type);
	/* rare case. can happen when registering subclasses */
	if (py_class == NULL) {
		fprintf(stderr, "%s: unable to get python class for rna struct '%.200s'\n",
		        __func__, RNA_struct_identifier(ptr->type));
		return -1;
	}

	/* XXX, this is needed because render engine calls without a context
	 * this should be supported at some point but at the moment its not! */
	if (C == NULL)
		C = BPy_GetContext();

	is_valid_wm = (CTX_wm_manager(C) != NULL);

	bpy_context_set(C, &gilstate);

	if (!is_static) {
		/* some datatypes (operator, render engine) can store PyObjects for re-use */
		if (ptr->data) {
			void **instance = RNA_struct_instance(ptr);

			if (instance) {
				if (*instance) {
					py_class_instance = *instance;
					Py_INCREF(py_class_instance);
				}
				else {
					/* store the instance here once its created */
					py_class_instance_store = instance;
				}
			}
		}
		/* end exception */

		if (py_class_instance == NULL)
			py_srna = pyrna_struct_CreatePyObject(ptr);

		if (py_class_instance) {
			/* special case, instance is cached */
		}
		else if (py_srna == NULL) {
			py_class_instance = NULL;
		}
		else if (py_srna == Py_None) { /* probably wont ever happen but possible */
			Py_DECREF(py_srna);
			py_class_instance = NULL;
		}
		else {
#if 1
			/* Skip the code below and call init directly on the allocated 'py_srna'
			 * otherwise __init__() always needs to take a second self argument, see pyrna_struct_new().
			 * Although this is annoying to have to implement a part of pythons typeobject.c:type_call().
			 */
			if (py_class->tp_init) {
#ifdef USE_PEDANTIC_WRITE
				const int prev_write = rna_disallow_writes;
				rna_disallow_writes = is_operator ? FALSE : TRUE; /* only operators can write on __init__ */
#endif

				/* true in most cases even when the class its self doesn't define an __init__ function. */
				args = PyTuple_New(0);
				if (py_class->tp_init(py_srna, args, NULL) < 0) {
					Py_DECREF(py_srna);
					py_srna = NULL;
					/* err set below */
				}
				Py_DECREF(args);
#ifdef USE_PEDANTIC_WRITE
				rna_disallow_writes = prev_write;
#endif
			}
			py_class_instance = py_srna;

#else
			const int prev_write = rna_disallow_writes;
			rna_disallow_writes = TRUE;

			/* 'almost' all the time calling the class isn't needed.
			 * We could just do...
			py_class_instance = py_srna;
			Py_INCREF(py_class_instance);
			 * This would work fine but means __init__ functions wouldnt run.
			 * none of blenders default scripts use __init__ but its nice to call it
			 * for general correctness. just to note why this is here when it could be safely removed.
			 */
			args = PyTuple_New(1);
			PyTuple_SET_ITEM(args, 0, py_srna);
			py_class_instance = PyObject_Call(py_class, args, NULL);
			Py_DECREF(args);

			rna_disallow_writes = prev_write;

#endif

			if (py_class_instance == NULL) {
				err = -1; /* so the error is not overridden below */
			}
			else if (py_class_instance_store) {
				*py_class_instance_store = py_class_instance;
				Py_INCREF(py_class_instance);
			}
		}
	}

	if (err != -1 && (is_static || py_class_instance)) { /* Initializing the class worked, now run its invoke function */
		PyObject *item = PyObject_GetAttrString((PyObject *)py_class, RNA_function_identifier(func));
//		flag = RNA_function_flag(func);

		if (item) {
			RNA_pointer_create(NULL, &RNA_Function, func, &funcptr);

			args = PyTuple_New(rna_function_arg_count(func)); /* first arg is included in 'item' */

			if (is_static) {
				i = 0;
			}
			else {
				PyTuple_SET_ITEM(args, 0, py_class_instance);
				i = 1;
			}

			RNA_parameter_list_begin(parms, &iter);

			/* parse function parameters */
			for (; iter.valid; RNA_parameter_list_next(&iter)) {
				parm = iter.parm;
				flag = RNA_property_flag(parm);

				/* only useful for single argument returns, we'll need another list loop for multiple */
				if (flag & PROP_OUTPUT) {
					ret_len++;
					if (pret_single == NULL) {
						pret_single = parm;
						retdata_single = iter.data;
					}

					continue;
				}

				parmitem = pyrna_param_to_py(&funcptr, parm, iter.data);
				PyTuple_SET_ITEM(args, i, parmitem);
				i++;
			}

#ifdef USE_PEDANTIC_WRITE
			rna_disallow_writes = is_readonly ? TRUE:FALSE;
#endif
			/* *** Main Caller *** */

			ret = PyObject_Call(item, args, NULL);

			/* *** Done Calling *** */

#ifdef USE_PEDANTIC_WRITE
			rna_disallow_writes = FALSE;
#endif

			RNA_parameter_list_end(&iter);
			Py_DECREF(item);
			Py_DECREF(args);
		}
		else {
			PyErr_Print();
			PyErr_Clear();
			PyErr_Format(PyExc_TypeError,
			             "could not find function %.200s in %.200s to execute callback",
			             RNA_function_identifier(func), RNA_struct_identifier(ptr->type));
			err = -1;
		}
	}
	else {
		/* the error may be already set if the class instance couldn't be created */
		if (err != -1) {
			PyErr_Format(PyExc_RuntimeError,
			             "could not create instance of %.200s to call callback function %.200s",
			             RNA_struct_identifier(ptr->type), RNA_function_identifier(func));
			err = -1;
		}
	}

	if (ret == NULL) { /* covers py_class_instance failing too */
		err = -1;
	}
	else {
		if (ret_len == 0 && ret != Py_None) {
			PyErr_Format(PyExc_RuntimeError,
			             "expected class %.200s, function %.200s to return None, not %.200s",
			             RNA_struct_identifier(ptr->type), RNA_function_identifier(func),
			             Py_TYPE(ret)->tp_name);
			err = -1;
		}
		else if (ret_len == 1) {
			err = pyrna_py_to_prop(&funcptr, pret_single, retdata_single, ret, "");

			/* when calling operator funcs only gives Function.result with
			 * no line number since the func has finished calling on error,
			 * re-raise the exception with more info since it would be slow to
			 * create prefix on every call (when there are no errors) */
			if (err == -1) {
				PyC_Err_Format_Prefix(PyExc_RuntimeError,
				                      "class %.200s, function %.200s: incompatible return value ",
				                      RNA_struct_identifier(ptr->type), RNA_function_identifier(func)
				                      );
			}
		}
		else if (ret_len > 1) {

			if (PyTuple_Check(ret) == 0) {
				PyErr_Format(PyExc_RuntimeError,
				             "expected class %.200s, function %.200s to return a tuple of size %d, not %.200s",
				             RNA_struct_identifier(ptr->type), RNA_function_identifier(func),
				             ret_len, Py_TYPE(ret)->tp_name);
				err = -1;
			}
			else if (PyTuple_GET_SIZE(ret) != ret_len) {
				PyErr_Format(PyExc_RuntimeError,
				             "class %.200s, function %.200s to returned %d items, expected %d",
				             RNA_struct_identifier(ptr->type), RNA_function_identifier(func),
				             PyTuple_GET_SIZE(ret), ret_len);
				err = -1;
			}
			else {

				RNA_parameter_list_begin(parms, &iter);

				/* parse function parameters */
				for (i = 0; iter.valid; RNA_parameter_list_next(&iter)) {
					parm = iter.parm;
					flag = RNA_property_flag(parm);

					/* only useful for single argument returns, we'll need another list loop for multiple */
					if (flag & PROP_OUTPUT) {
						err = pyrna_py_to_prop(&funcptr, parm, iter.data,
						                      PyTuple_GET_ITEM(ret, i++),
						                      "calling class function:");
						if (err) {
							break;
						}
					}
				}

				RNA_parameter_list_end(&iter);
			}
		}
		Py_DECREF(ret);
	}

	if (err != 0) {
		ReportList *reports;
		/* alert the user, else they wont know unless they see the console. */
		if ( (!is_static) &&
		     (ptr->data) &&
		     (RNA_struct_is_a(ptr->type, &RNA_Operator)) &&
		     (is_valid_wm == (CTX_wm_manager(C) != NULL)))
		{
			wmOperator *op = ptr->data;
			reports = op->reports;
		}
		else {
			/* wont alert users but they can view in 'info' space */
			reports = CTX_wm_reports(C);
		}

		BPy_errors_to_report(reports);

		/* also print in the console for py */
		PyErr_Print();
		PyErr_Clear();
	}

	bpy_context_clear(C, &gilstate);

	return err;
}

static void bpy_class_free(void *pyob_ptr)
{
	PyObject *self = (PyObject *)pyob_ptr;
	PyGILState_STATE gilstate;

	gilstate = PyGILState_Ensure();

	// breaks re-registering classes
	// PyDict_Clear(((PyTypeObject *)self)->tp_dict);
	//
	// remove the rna attribute instead.
	PyDict_DelItem(((PyTypeObject *)self)->tp_dict, bpy_intern_str_bl_rna);
	if (PyErr_Occurred())
		PyErr_Clear();

#if 0 /* needs further investigation, too annoying so quiet for now */
	if (G.f&G_DEBUG) {
		if (self->ob_refcnt > 1) {
			PyC_ObSpit("zombie class - ref should be 1", self);
		}
	}
#endif
	Py_DECREF((PyObject *)pyob_ptr);

	PyGILState_Release(gilstate);
}

void pyrna_alloc_types(void)
{
	PyGILState_STATE gilstate;

	PointerRNA ptr;
	PropertyRNA *prop;

	gilstate = PyGILState_Ensure();

	/* avoid doing this lookup for every getattr */
	RNA_blender_rna_pointer_create(&ptr);
	prop = RNA_struct_find_property(&ptr, "structs");

	RNA_PROP_BEGIN(&ptr, itemptr, prop) {
		PyObject *item = pyrna_struct_Subtype(&itemptr);
		if (item == NULL) {
			if (PyErr_Occurred()) {
				PyErr_Print();
				PyErr_Clear();
			}
		}
		else {
			Py_DECREF(item);
		}
	}
	RNA_PROP_END;

	PyGILState_Release(gilstate);
}


void pyrna_free_types(void)
{
	PointerRNA ptr;
	PropertyRNA *prop;

	/* avoid doing this lookup for every getattr */
	RNA_blender_rna_pointer_create(&ptr);
	prop = RNA_struct_find_property(&ptr, "structs");


	RNA_PROP_BEGIN(&ptr, itemptr, prop) {
		StructRNA *srna = srna_from_ptr(&itemptr);
		void *py_ptr = RNA_struct_py_type_get(srna);

		if (py_ptr) {
#if 0	// XXX - should be able to do this but makes python crash on exit
			bpy_class_free(py_ptr);
#endif
			RNA_struct_py_type_set(srna, NULL);
		}
	}
	RNA_PROP_END;

}

/* Note! MemLeak XXX
 *
 * There is currently a bug where moving registering a python class does
 * not properly manage refcounts from the python class, since the srna owns
 * the python class this should not be so tricky but changing the references as
 * you'd expect when changing ownership crashes blender on exit so I had to comment out
 * the decref. This is not so bad because the leak only happens when re-registering (hold F8)
 * - Should still be fixed - Campbell
 * */
PyDoc_STRVAR(pyrna_register_class_doc,
".. method:: register_class(cls)\n"
"\n"
"   Register a subclass of a blender type in (:class:`bpy.types.Panel`,\n"
"   :class:`bpy.types.Menu`, :class:`bpy.types.Header`, :class:`bpy.types.Operator`,\n"
"   :class:`bpy.types.KeyingSetInfo`, :class:`bpy.types.RenderEngine`).\n"
"\n"
"   If the class has a *register* class method it will be called\n"
"   before registration.\n"
"\n"
"   .. note::\n"
"\n"
"      :exc:`ValueError` exception is raised if the class is not a\n"
"      subclass of a registerable blender class.\n"
"\n"
);
PyMethodDef meth_bpy_register_class = {"register_class", pyrna_register_class, METH_O, pyrna_register_class_doc};
static PyObject *pyrna_register_class(PyObject *UNUSED(self), PyObject *py_class)
{
	bContext *C = NULL;
	ReportList reports;
	StructRegisterFunc reg;
	StructRNA *srna;
	StructRNA *srna_new;
	const char *identifier;
	PyObject *py_cls_meth;

	if (!PyType_Check(py_class)) {
		PyErr_Format(PyExc_ValueError,
		             "register_class(...): "
		             "expected a class argument, not '%.200s'", Py_TYPE(py_class)->tp_name);
		return NULL;
	}

	if (PyDict_GetItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna)) {
		PyErr_SetString(PyExc_ValueError,
		                "register_class(...): "
		                "already registered as a subclass");
		return NULL;
	}

	if (!pyrna_write_check()) {
		PyErr_Format(PyExc_RuntimeError,
		             "register_class(...): "
		             "can't run in readonly state '%.200s'",
		             ((PyTypeObject *)py_class)->tp_name);
		return NULL;
	}

	/* warning: gets parent classes srna, only for the register function */
	srna = pyrna_struct_as_srna(py_class, 1, "register_class(...):");
	if (srna == NULL)
		return NULL;

	/* fails in cases, cant use this check but would like to :| */
	/*
	if (RNA_struct_py_type_get(srna)) {
		PyErr_Format(PyExc_ValueError,
		             "register_class(...): %.200s's parent class %.200s is already registered, this is not allowed",
		             ((PyTypeObject *)py_class)->tp_name, RNA_struct_identifier(srna));
		return NULL;
	}
	*/

	/* check that we have a register callback for this type */
	reg = RNA_struct_register(srna);

	if (!reg) {
		PyErr_Format(PyExc_ValueError,
		             "register_class(...): expected a subclass of a registerable "
		             "rna type (%.200s does not support registration)",
		             RNA_struct_identifier(srna));
		return NULL;
	}

	/* get the context, so register callback can do necessary refreshes */
	C = BPy_GetContext();

	/* call the register callback with reports & identifier */
	BKE_reports_init(&reports, RPT_STORE);

	identifier = ((PyTypeObject *)py_class)->tp_name;

	srna_new = reg(CTX_data_main(C), &reports, py_class, identifier,
	               bpy_class_validate, bpy_class_call, bpy_class_free);

	if (BPy_reports_to_error(&reports, PyExc_RuntimeError, TRUE) == -1)
		return NULL;

	/* python errors validating are not converted into reports so the check above will fail.
	 * the cause for returning NULL will be printed as an error */
	if (srna_new == NULL)
		return NULL;

	pyrna_subtype_set_rna(py_class, srna_new); /* takes a ref to py_class */

	/* old srna still references us, keep the check incase registering somehow can free it */
	if (RNA_struct_py_type_get(srna)) {
		RNA_struct_py_type_set(srna, NULL);
		// Py_DECREF(py_class); // should be able to do this XXX since the old rna adds a new ref.
	}

	/* Can't use this because it returns a dict proxy
	 *
	 * item = PyObject_GetAttrString(py_class, "__dict__");
	 */
	if (pyrna_deferred_register_class(srna_new, py_class) != 0)
		return NULL;

	/* call classed register method () */
	py_cls_meth = PyObject_GetAttr(py_class, bpy_intern_str_register);
	if (py_cls_meth == NULL) {
		PyErr_Clear();
	}
	else {
		PyObject *ret = PyObject_CallObject(py_cls_meth, NULL);
		if (ret) {
			Py_DECREF(ret);
		}
		else {
			return NULL;
		}
	}

	Py_RETURN_NONE;
}


static int pyrna_srna_contains_pointer_prop_srna(StructRNA *srna_props, StructRNA *srna, const char **prop_identifier)
{
	PropertyRNA *prop;
	LinkData *link;

	/* verify properties */
	const ListBase *lb = RNA_struct_type_properties(srna);

	for (link = lb->first; link; link = link->next) {
		prop = (PropertyRNA *)link;
		if (RNA_property_type(prop) == PROP_POINTER && !(RNA_property_flag(prop) & PROP_BUILTIN)) {
			PointerRNA tptr;
			RNA_pointer_create(NULL, &RNA_Struct, srna_props, &tptr);

			if (RNA_property_pointer_type(&tptr, prop) == srna) {
				*prop_identifier = RNA_property_identifier(prop);
				return 1;
			}
		}
	}

	return 0;
}

PyDoc_STRVAR(pyrna_unregister_class_doc,
".. method:: unregister_class(cls)\n"
"\n"
"   Unload the python class from blender.\n"
"\n"
"   If the class has an *unregister* class method it will be called\n"
"   before unregistering.\n"
);
PyMethodDef meth_bpy_unregister_class = {
    "unregister_class", pyrna_unregister_class, METH_O, pyrna_unregister_class_doc
};
static PyObject *pyrna_unregister_class(PyObject *UNUSED(self), PyObject *py_class)
{
	bContext *C = NULL;
	StructUnregisterFunc unreg;
	StructRNA *srna;
	PyObject *py_cls_meth;

	if (!PyType_Check(py_class)) {
		PyErr_Format(PyExc_ValueError,
		             "register_class(...): "
		             "expected a class argument, not '%.200s'", Py_TYPE(py_class)->tp_name);
		return NULL;
	}

	/*if (PyDict_GetItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna) == NULL) {
		PWM_cursor_wait(0);
		PyErr_SetString(PyExc_ValueError, "unregister_class(): not a registered as a subclass");
		return NULL;
	}*/

	if (!pyrna_write_check()) {
		PyErr_Format(PyExc_RuntimeError,
		             "unregister_class(...): "
		             "can't run in readonly state '%.200s'",
		             ((PyTypeObject *)py_class)->tp_name);
		return NULL;
	}

	srna = pyrna_struct_as_srna(py_class, 0, "unregister_class(...):");
	if (srna == NULL)
		return NULL;

	/* check that we have a unregister callback for this type */
	unreg = RNA_struct_unregister(srna);

	if (!unreg) {
		PyErr_SetString(PyExc_ValueError,
		                "unregister_class(...): "
		                "expected a Type subclassed from a registerable rna type (no unregister supported)");
		return NULL;
	}

	/* call classed unregister method */
	py_cls_meth = PyObject_GetAttr(py_class, bpy_intern_str_unregister);
	if (py_cls_meth == NULL) {
		PyErr_Clear();
	}
	else {
		PyObject *ret = PyObject_CallObject(py_cls_meth, NULL);
		if (ret) {
			Py_DECREF(ret);
		}
		else {
			return NULL;
		}
	}

	/* should happen all the time but very slow */
	if (G.f & G_DEBUG) {
		/* remove all properties using this class */
		StructRNA *srna_iter;
		PointerRNA ptr_rna;
		PropertyRNA *prop_rna;
		const char *prop_identifier = NULL;

		RNA_blender_rna_pointer_create(&ptr_rna);
		prop_rna = RNA_struct_find_property(&ptr_rna, "structs");



		/* loop over all structs */
		RNA_PROP_BEGIN(&ptr_rna, itemptr, prop_rna) {
			srna_iter = itemptr.data;
			if (pyrna_srna_contains_pointer_prop_srna(srna_iter, srna, &prop_identifier)) {
				break;
			}
		}
		RNA_PROP_END;

		if (prop_identifier) {
			PyErr_Format(PyExc_RuntimeError,
			             "unregister_class(...): can't unregister %s because %s.%s pointer property is using this",
			             RNA_struct_identifier(srna), RNA_struct_identifier(srna_iter), prop_identifier);
			return NULL;
		}
	}

	/* get the context, so register callback can do necessary refreshes */
	C = BPy_GetContext();

	/* call unregister */
	unreg(CTX_data_main(C), srna); /* calls bpy_class_free, this decref's py_class */

	PyDict_DelItem(((PyTypeObject *)py_class)->tp_dict, bpy_intern_str_bl_rna);
	if (PyErr_Occurred())
		PyErr_Clear(); //return NULL;

	Py_RETURN_NONE;
}
