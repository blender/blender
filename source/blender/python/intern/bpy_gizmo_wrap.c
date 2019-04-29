/*
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
 */

/** \file
 * \ingroup pythonintern
 *
 * This file is so Python can define widget-group's that C can call into.
 * The generic callback functions for Python widget-group are defines in
 * 'rna_wm.c', some calling into functions here to do python specific
 * functionality.
 *
 * \note This follows 'bpy_operator_wrap.c' very closely.
 * Keep in sync unless there is good reason not to!
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "bpy_rna.h"
#include "bpy_intern_string.h"
#include "bpy_gizmo_wrap.h" /* own include */

/* we may want to add, but not now */

/* -------------------------------------------------------------------- */
/** \name Gizmo
 * \{ */

static bool bpy_gizmotype_target_property_def(wmGizmoType *gzt, PyObject *item)
{
  /* Note: names based on 'rna_rna.c' */
  PyObject *empty_tuple = PyTuple_New(0);

  struct {
    char *id;
    char *type_id;
    int type;
    int array_length;
  } params = {
      .id = NULL, /* not optional */
      .type = PROP_FLOAT,
      .type_id = NULL,
      .array_length = 1,
  };

  static const char *const _keywords[] = {"id", "type", "array_length", NULL};
  static _PyArg_Parser _parser = {"|$ssi:register_class", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(
          empty_tuple, item, &_parser, &params.id, &params.type_id, &params.array_length)) {
    goto fail;
  }

  if (params.id == NULL) {
    PyErr_SetString(PyExc_ValueError, "'id' argument not given");
    goto fail;
  }

  if ((params.type_id != NULL) &&
      pyrna_enum_value_from_id(
          rna_enum_property_type_items, params.type_id, &params.type, "'type' enum value") == -1) {
    goto fail;
  }
  else {
    params.type = rna_enum_property_type_items[params.type].value;
  }

  if ((params.array_length < 1 || params.array_length > RNA_MAX_ARRAY_LENGTH)) {
    PyErr_SetString(PyExc_ValueError, "'array_length' out of range");
    goto fail;
  }

  WM_gizmotype_target_property_def(gzt, params.id, params.type, params.array_length);
  Py_DECREF(empty_tuple);
  return true;

fail:
  Py_DECREF(empty_tuple);
  return false;
}

static void gizmo_properties_init(wmGizmoType *gzt)
{
  PyTypeObject *py_class = gzt->ext.data;
  RNA_struct_blender_type_set(gzt->ext.srna, gzt);

  /* only call this so pyrna_deferred_register_class gives a useful error
   * WM_operatortype_append_ptr will call RNA_def_struct_identifier
   * later */
  RNA_def_struct_identifier_no_struct_map(gzt->srna, gzt->idname);

  if (pyrna_deferred_register_class(gzt->srna, py_class) != 0) {
    PyErr_Print(); /* failed to register operator props */
    PyErr_Clear();
  }

  /* Extract target property definitions from 'bl_target_properties' */
  {
    /* Picky developers will notice that 'bl_targets' won't work with inheritance
     * get direct from the dict to avoid raising a load of attribute errors
     * (yes this isnt ideal) - campbell. */
    PyObject *py_class_dict = py_class->tp_dict;
    PyObject *bl_target_properties = PyDict_GetItem(py_class_dict,
                                                    bpy_intern_str_bl_target_properties);

    /* Some widgets may only exist to activate operators. */
    if (bl_target_properties != NULL) {
      PyObject *bl_target_properties_fast;
      if (!(bl_target_properties_fast = PySequence_Fast(bl_target_properties,
                                                        "bl_target_properties sequence"))) {
        /* PySequence_Fast sets the error */
        PyErr_Print();
        PyErr_Clear();
        return;
      }

      const uint items_len = PySequence_Fast_GET_SIZE(bl_target_properties_fast);
      PyObject **items = PySequence_Fast_ITEMS(bl_target_properties_fast);

      for (uint i = 0; i < items_len; i++) {
        if (!bpy_gizmotype_target_property_def(gzt, items[i])) {
          PyErr_Print();
          PyErr_Clear();
          break;
        }
      }

      Py_DECREF(bl_target_properties_fast);
    }
  }
}

void BPY_RNA_gizmo_wrapper(wmGizmoType *gzt, void *userdata)
{
  /* take care not to overwrite anything set in
   * WM_gizmomaptype_group_link_ptr before opfunc() is called */
  StructRNA *srna = gzt->srna;
  *gzt = *((wmGizmoType *)userdata);
  gzt->srna = srna; /* restore */

  /* don't do translations here yet */
#if 0
  /* Use i18n context from ext.srna if possible (py gizmogroups). */
  if (gt->ext.srna) {
    RNA_def_struct_translation_context(gt->srna, RNA_struct_translation_context(gt->ext.srna));
  }
#endif

  gzt->struct_size = sizeof(wmGizmo);

  gizmo_properties_init(gzt);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Group
 * \{ */

static void gizmogroup_properties_init(wmGizmoGroupType *gzgt)
{
  PyTypeObject *py_class = gzgt->ext.data;
  RNA_struct_blender_type_set(gzgt->ext.srna, gzgt);

  /* only call this so pyrna_deferred_register_class gives a useful error
   * WM_operatortype_append_ptr will call RNA_def_struct_identifier
   * later */
  RNA_def_struct_identifier_no_struct_map(gzgt->srna, gzgt->idname);

  if (pyrna_deferred_register_class(gzgt->srna, py_class) != 0) {
    PyErr_Print(); /* failed to register operator props */
    PyErr_Clear();
  }
}

void BPY_RNA_gizmogroup_wrapper(wmGizmoGroupType *gzgt, void *userdata)
{
  /* take care not to overwrite anything set in
   * WM_gizmomaptype_group_link_ptr before opfunc() is called */
  StructRNA *srna = gzgt->srna;
  *gzgt = *((wmGizmoGroupType *)userdata);
  gzgt->srna = srna; /* restore */

  /* don't do translations here yet */
#if 0
  /* Use i18n context from ext.srna if possible (py gizmogroups). */
  if (gzgt->ext.srna) {
    RNA_def_struct_translation_context(gzgt->srna, RNA_struct_translation_context(gzgt->ext.srna));
  }
#endif

  gizmogroup_properties_init(gzgt);
}

/** \} */
