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
 * This file adds some helpers related to ID/Main handling, that cannot fit well in RNA itself.
 */

#include <Python.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_bitmap.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_main.h"

#include "DNA_ID.h"
/* Those following are only to support hack of not listing some internal
 * 'backward' pointers in generated user_map. */
#include "DNA_object_types.h"
#include "DNA_key_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "bpy_capi_utils.h"
#include "bpy_rna_id_collection.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"
#include "RNA_enum_types.h"

#include "bpy_rna.h"

typedef struct IDUserMapData {
  /** Place-holder key only used for lookups to avoid creating new data only for lookups
   * (never return its contents) */
  PyObject *py_id_key_lookup_only;

  /** We loop over data-blocks that this ID points to (do build a reverse lookup table) */
  PyObject *py_id_curr;
  ID *id_curr;

  /** Filter the values we add into the set. */
  BLI_bitmap *types_bitmap;

  /** Set to fill in as we iterate. */
  PyObject *user_map;
  /** true when we're only mapping a subset of all the ID's (subset arg is passed). */
  bool is_subset;
} IDUserMapData;

static int id_code_as_index(const short idcode)
{
  return (int)*((unsigned short *)&idcode);
}

static bool id_check_type(const ID *id, const BLI_bitmap *types_bitmap)
{
  return BLI_BITMAP_TEST_BOOL(types_bitmap, id_code_as_index(GS(id->name)));
}

static int foreach_libblock_id_user_map_callback(void *user_data,
                                                 ID *self_id,
                                                 ID **id_p,
                                                 int UNUSED(cb_flag))
{
  IDUserMapData *data = user_data;

  if (*id_p) {

    if (data->types_bitmap) {
      if (!id_check_type(*id_p, data->types_bitmap)) {
        return IDWALK_RET_NOP;
      }
    }

    if ((GS(self_id->name) == ID_OB) && (id_p == (ID **)&((Object *)self_id)->proxy_from)) {
      /* We skip proxy_from here,
       * since it's some internal pointer which is not relevant info for py/API level. */
      return IDWALK_RET_NOP;
    }
    else if ((GS(self_id->name) == ID_KE) && (id_p == (ID **)&((Key *)self_id)->from)) {
      /* We skip from here,
       * since it's some internal pointer which is not relevant info for py/API level. */
      return IDWALK_RET_NOP;
    }

    /* pyrna_struct_hash() uses ptr.data only,
     * but pyrna_struct_richcmp() uses also ptr.type,
     * so we need to create a valid PointerRNA here...
     */
    PyObject *key = data->py_id_key_lookup_only;
    RNA_id_pointer_create(*id_p, &((BPy_StructRNA *)key)->ptr);

    PyObject *set;
    if ((set = PyDict_GetItem(data->user_map, key)) == NULL) {

      /* limit to key's added already */
      if (data->is_subset) {
        return IDWALK_RET_NOP;
      }

      /* Cannot use our placeholder key here! */
      key = pyrna_id_CreatePyObject(*id_p);
      set = PySet_New(NULL);
      PyDict_SetItem(data->user_map, key, set);
      Py_DECREF(set);
      Py_DECREF(key);
    }

    if (data->py_id_curr == NULL) {
      data->py_id_curr = pyrna_id_CreatePyObject(data->id_curr);
    }

    PySet_Add(set, data->py_id_curr);
  }

  return IDWALK_RET_NOP;
}

PyDoc_STRVAR(bpy_user_map_doc,
             ".. method:: user_map([subset=(id1, id2, ...)], key_types={..}, value_types={..})\n"
             "\n"
             "   Returns a mapping of all ID datablocks in current ``bpy.data`` to a set of all "
             "datablocks using them.\n"
             "\n"
             "   For list of valid set members for key_types & value_types, see: "
             ":class:`bpy.types.KeyingSetPath.id_type`.\n"
             "\n"
             "   :arg subset: When passed, only these data-blocks and their users will be "
             "included as keys/values in the map.\n"
             "   :type subset: sequence\n"
             "   :arg key_types: Filter the keys mapped by ID types.\n"
             "   :type key_types: set of strings\n"
             "   :arg value_types: Filter the values in the set by ID types.\n"
             "   :type value_types: set of strings\n"
             "   :return: dictionary of :class:`bpy.types.ID` instances, with sets of ID's as "
             "their values.\n"
             "   :rtype: dict\n");
static PyObject *bpy_user_map(PyObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
#if 0 /* If someone knows how to get a proper 'self' in that case... */
  BPy_StructRNA *pyrna = (BPy_StructRNA *)self;
  Main *bmain = pyrna->ptr.data;
#else
  Main *bmain = G_MAIN; /* XXX Ugly, but should work! */
#endif
  ListBase *lb;
  ID *id;

  PyObject *subset = NULL;

  PyObject *key_types = NULL;
  PyObject *val_types = NULL;
  BLI_bitmap *key_types_bitmap = NULL;
  BLI_bitmap *val_types_bitmap = NULL;

  PyObject *ret = NULL;

  IDUserMapData data_cb = {NULL};

  static const char *_keywords[] = {"subset", "key_types", "value_types", NULL};
  static _PyArg_Parser _parser = {"|O$O!O!:user_map", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &subset, &PySet_Type, &key_types, &PySet_Type, &val_types)) {
    return NULL;
  }

  if (key_types) {
    key_types_bitmap = pyrna_set_to_enum_bitmap(
        rna_enum_id_type_items, key_types, sizeof(short), true, USHRT_MAX, "key types");
    if (key_types_bitmap == NULL) {
      goto error;
    }
  }

  if (val_types) {
    val_types_bitmap = pyrna_set_to_enum_bitmap(
        rna_enum_id_type_items, val_types, sizeof(short), true, USHRT_MAX, "value types");
    if (val_types_bitmap == NULL) {
      goto error;
    }
  }

  if (subset) {
    PyObject *subset_fast = PySequence_Fast(subset, "user_map");
    if (subset_fast == NULL) {
      goto error;
    }

    PyObject **subset_array = PySequence_Fast_ITEMS(subset_fast);
    Py_ssize_t subset_len = PySequence_Fast_GET_SIZE(subset_fast);

    data_cb.user_map = _PyDict_NewPresized(subset_len);
    data_cb.is_subset = true;
    for (; subset_len; subset_array++, subset_len--) {
      PyObject *set = PySet_New(NULL);
      PyDict_SetItem(data_cb.user_map, *subset_array, set);
      Py_DECREF(set);
    }
    Py_DECREF(subset_fast);
  }
  else {
    data_cb.user_map = PyDict_New();
  }

  data_cb.types_bitmap = key_types_bitmap;

  FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
    FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
      /* We cannot skip here in case we have some filter on key types... */
      if (key_types_bitmap == NULL && val_types_bitmap != NULL) {
        if (!id_check_type(id, val_types_bitmap)) {
          break;
        }
      }

      /* One-time init, ID is just used as placeholder here, we abuse this in iterator callback
       * to avoid having to rebuild a complete bpyrna object each time for the key searching
       * (where only ID pointer value is used). */
      if (data_cb.py_id_key_lookup_only == NULL) {
        data_cb.py_id_key_lookup_only = pyrna_id_CreatePyObject(id);
      }

      if (!data_cb.is_subset &&
          /* We do not want to pre-add keys of flitered out types. */
          (key_types_bitmap == NULL || id_check_type(id, key_types_bitmap)) &&
          /* We do not want to pre-add keys when we have filter on value types,
           * but not on key types. */
          (val_types_bitmap == NULL || key_types_bitmap != NULL)) {
        PyObject *key = data_cb.py_id_key_lookup_only;
        PyObject *set;

        RNA_id_pointer_create(id, &((BPy_StructRNA *)key)->ptr);

        /* We have to insert the key now,
         * otherwise ID unused would be missing from final dict... */
        if ((set = PyDict_GetItem(data_cb.user_map, key)) == NULL) {
          /* Cannot use our placeholder key here! */
          key = pyrna_id_CreatePyObject(id);
          set = PySet_New(NULL);
          PyDict_SetItem(data_cb.user_map, key, set);
          Py_DECREF(set);
          Py_DECREF(key);
        }
      }

      if (val_types_bitmap != NULL && !id_check_type(id, val_types_bitmap)) {
        continue;
      }

      data_cb.id_curr = id;
      BKE_library_foreach_ID_link(
          NULL, id, foreach_libblock_id_user_map_callback, &data_cb, IDWALK_CB_NOP);

      if (data_cb.py_id_curr) {
        Py_DECREF(data_cb.py_id_curr);
        data_cb.py_id_curr = NULL;
      }
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_ID_END;

  ret = data_cb.user_map;

error:
  if (data_cb.py_id_key_lookup_only != NULL) {
    Py_XDECREF(data_cb.py_id_key_lookup_only);
  }

  if (key_types_bitmap != NULL) {
    MEM_freeN(key_types_bitmap);
  }

  if (val_types_bitmap != NULL) {
    MEM_freeN(val_types_bitmap);
  }

  return ret;
}

PyDoc_STRVAR(bpy_batch_remove_doc,
             ".. method:: batch_remove(ids=(id1, id2, ...))\n"
             "\n"
             "   Remove (delete) several IDs at once.\n"
             "\n"
             "   WARNING: Considered experimental feature currently.\n"
             "\n"
             "   Note that this function is quicker than individual calls to :func:`remove()` "
             "(from :class:`bpy.types.BlendData`\n"
             "   ID collections), but less safe/versatile (it can break Blender, e.g. by removing "
             "all scenes...).\n"
             "\n"
             "   :arg ids: Iterables of IDs (types can be mixed).\n"
             "   :type subset: sequence\n");
static PyObject *bpy_batch_remove(PyObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
#if 0 /* If someone knows how to get a proper 'self' in that case... */
  BPy_StructRNA *pyrna = (BPy_StructRNA *)self;
  Main *bmain = pyrna->ptr.data;
#else
  Main *bmain = G_MAIN; /* XXX Ugly, but should work! */
#endif

  PyObject *ids = NULL;

  PyObject *ret = NULL;

  static const char *_keywords[] = {"ids", NULL};
  static _PyArg_Parser _parser = {"O:user_map", _keywords, 0};
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &ids)) {
    return ret;
  }

  if (ids) {
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

    PyObject *ids_fast = PySequence_Fast(ids, "batch_remove");
    if (ids_fast == NULL) {
      goto error;
    }

    PyObject **ids_array = PySequence_Fast_ITEMS(ids_fast);
    Py_ssize_t ids_len = PySequence_Fast_GET_SIZE(ids_fast);

    for (; ids_len; ids_array++, ids_len--) {
      ID *id;
      if (!pyrna_id_FromPyObject(*ids_array, &id)) {
        PyErr_Format(
            PyExc_TypeError, "Expected an ID type, not %.200s", Py_TYPE(*ids_array)->tp_name);
        Py_DECREF(ids_fast);
        goto error;
      }

      id->tag |= LIB_TAG_DOIT;
    }
    Py_DECREF(ids_fast);

    BKE_id_multi_tagged_delete(bmain);
    /* Force full redraw, mandatory to avoid crashes when running this from UI... */
    WM_main_add_notifier(NC_WINDOW, NULL);
  }
  else {
    goto error;
  }

  Py_INCREF(Py_None);
  ret = Py_None;

error:
  return ret;
}

int BPY_rna_id_collection_module(PyObject *mod_par)
{
  static PyMethodDef user_map = {
      "user_map", (PyCFunction)bpy_user_map, METH_VARARGS | METH_KEYWORDS, bpy_user_map_doc};

  PyModule_AddObject(mod_par, "_rna_id_collection_user_map", PyCFunction_New(&user_map, NULL));

  static PyMethodDef batch_remove = {
      "batch_remove",
      (PyCFunction)bpy_batch_remove,
      METH_VARARGS | METH_KEYWORDS,
      bpy_batch_remove_doc,
  };

  PyModule_AddObject(
      mod_par, "_rna_id_collection_batch_remove", PyCFunction_New(&batch_remove, NULL));

  return 0;
}
