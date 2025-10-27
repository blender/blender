/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file adds some helpers related to ID/Main handling, that cannot fit well in RNA itself.
 */

#include <Python.h>
#include <cstddef>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_string.h"

#include "BKE_bpath.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"

#include "DNA_ID.h"
/* Those following are only to support hack of not listing some internal
 * 'backward' pointers in generated user_map. */

#include "WM_api.hh"
#include "WM_types.hh"

#include "bpy_rna_id_collection.hh"

#include "../generic/py_capi_rna.hh"
#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */
#include "../generic/python_utildefines.hh"

#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "bpy_rna.hh"

static Main *pyrna_bmain_FromPyObject(PyObject *obj)
{
  if (!BPy_StructRNA_Check(obj)) {
    PyErr_Format(PyExc_TypeError,
                 "Expected a StructRNA of type BlendData, not %.200s",
                 Py_TYPE(obj)->tp_name);
    return nullptr;
  }
  BPy_StructRNA *pyrna = reinterpret_cast<BPy_StructRNA *>(obj);
  PYRNA_STRUCT_CHECK_OBJ(pyrna);
  if (!(pyrna->ptr && pyrna->ptr->type == &RNA_BlendData && pyrna->ptr->data)) {
    PyErr_Format(PyExc_TypeError,
                 "Expected a StructRNA of type BlendData, not %.200s",
                 Py_TYPE(pyrna)->tp_name);
    return nullptr;
  }
  return static_cast<Main *>(pyrna->ptr->data);
}

struct IDUserMapData {
  /** We loop over data-blocks that this ID points to (do build a reverse lookup table) */
  PyObject *py_id_curr;
  ID *id_curr;

  /** Filter the values we add into the set. */
  BLI_bitmap *types_bitmap;

  /** Set to fill in as we iterate. */
  PyObject *user_map;
  /** true when we're only mapping a subset of all the ID's (subset arg is passed). */
  bool is_subset;
};

static int id_code_as_index(const short idcode)
{
  return int(*((ushort *)&idcode));
}

static bool id_check_type(const ID *id, const BLI_bitmap *types_bitmap)
{
  return BLI_BITMAP_TEST_BOOL(types_bitmap, id_code_as_index(GS(id->name)));
}

static int foreach_libblock_id_user_map_callback(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;

  if (*id_p) {
    IDUserMapData *data = static_cast<IDUserMapData *>(cb_data->user_data);
    const LibraryForeachIDCallbackFlag cb_flag = cb_data->cb_flag;

    if (data->types_bitmap) {
      if (!id_check_type(*id_p, data->types_bitmap)) {
        return IDWALK_RET_NOP;
      }
    }

    if (cb_flag & IDWALK_CB_LOOPBACK) {
      /* We skip loop-back pointers like Key.from here,
       * since it's some internal pointer which is not relevant info for py/API level. */
      return IDWALK_RET_NOP;
    }

    if (cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
      /* We skip private pointers themselves, like root node trees, we'll 'link' their own ID
       * pointers to their 'ID owner' instead. */
      return IDWALK_RET_NOP;
    }

    PyObject *key = pyrna_id_CreatePyObject(*id_p);

    PyObject *set;
    if ((set = PyDict_GetItem(data->user_map, key)) == nullptr) {
      /* limit to key's added already */
      if (data->is_subset) {
        return IDWALK_RET_NOP;
      }

      set = PySet_New(nullptr);
      PyDict_SetItem(data->user_map, key, set);
      Py_DECREF(set);
    }
    Py_DECREF(key);

    if (data->py_id_curr == nullptr) {
      data->py_id_curr = pyrna_id_CreatePyObject(data->id_curr);
    }

    PySet_Add(set, data->py_id_curr);
  }

  return IDWALK_RET_NOP;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_user_map_doc,
    /* NOTE: These documented default values (None) are here just to signal that these parameters
     * are optional. Explicitly passing None is not valid, and will raise a TypeError. */
    ".. method:: user_map(*, subset=None, key_types=None, value_types=None)\n"
    "\n"
    "   Returns a mapping of all ID data-blocks in current ``bpy.data`` to a set of all "
    "data-blocks using them.\n"
    "\n"
    "   For list of valid set members for key_types & value_types, see: "
    ":class:`bpy.types.KeyingSetPath.id_type`.\n"
    "\n"
    "   :arg subset: When passed, only these data-blocks and their users will be "
    "included as keys/values in the map.\n"
    "   :type subset: Sequence[:class:`bpy.types.ID`]\n"
    "   :arg key_types: Filter the keys mapped by ID types.\n"
    "   :type key_types: set[str]\n"
    "   :arg value_types: Filter the values in the set by ID types.\n"
    "   :type value_types: set[str]\n"
    "   :return: dictionary that maps data-blocks ID's to their users.\n"
    "   :rtype: dict[:class:`bpy.types.ID`, set[:class:`bpy.types.ID`]]\n");
static PyObject *bpy_user_map(PyObject *self, PyObject *args, PyObject *kwds)
{
  Main *bmain = pyrna_bmain_FromPyObject(self);
  if (!bmain) {
    return nullptr;
  }

  ListBase *lb;
  ID *id;

  PyObject *subset = nullptr;

  PyObject *key_types = nullptr;
  PyObject *val_types = nullptr;
  BLI_bitmap *key_types_bitmap = nullptr;
  BLI_bitmap *val_types_bitmap = nullptr;

  PyObject *ret = nullptr;

  IDUserMapData data_cb = {nullptr};

  static const char *_keywords[] = {"subset", "key_types", "value_types", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "|$" /* Optional keyword only arguments. */
      "O"  /* `subset` */
      "O!" /* `key_types` */
      "O!" /* `value_types` */
      ":user_map",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kwds, &_parser, &subset, &PySet_Type, &key_types, &PySet_Type, &val_types))
  {
    return nullptr;
  }

  if (key_types) {
    key_types_bitmap = pyrna_enum_bitmap_from_set(
        rna_enum_id_type_items, key_types, sizeof(short), true, USHRT_MAX, "key types");
    if (key_types_bitmap == nullptr) {
      goto error;
    }
  }

  if (val_types) {
    val_types_bitmap = pyrna_enum_bitmap_from_set(
        rna_enum_id_type_items, val_types, sizeof(short), true, USHRT_MAX, "value types");
    if (val_types_bitmap == nullptr) {
      goto error;
    }
  }

  if (subset) {
    PyObject *subset_fast = PySequence_Fast(subset, "user_map");
    if (subset_fast == nullptr) {
      goto error;
    }

    PyObject **subset_array = PySequence_Fast_ITEMS(subset_fast);
    Py_ssize_t subset_len = PySequence_Fast_GET_SIZE(subset_fast);

    data_cb.user_map = _PyDict_NewPresized(subset_len);
    data_cb.is_subset = true;
    for (; subset_len; subset_array++, subset_len--) {
      ID *id;
      if (!pyrna_id_FromPyObject(*subset_array, &id)) {
        PyErr_Format(PyExc_TypeError,
                     "Expected an ID type in `subset` iterable, not %.200s",
                     Py_TYPE(*subset_array)->tp_name);
        Py_DECREF(subset_fast);
        Py_DECREF(data_cb.user_map);
        goto error;
      }

      if (!PyDict_Contains(data_cb.user_map, *subset_array)) {
        PyObject *set = PySet_New(nullptr);
        PyDict_SetItem(data_cb.user_map, *subset_array, set);
        Py_DECREF(set);
      }
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
      if (key_types_bitmap == nullptr && val_types_bitmap != nullptr) {
        if (!id_check_type(id, val_types_bitmap)) {
          break;
        }
      }

      if (!data_cb.is_subset &&
          /* We do not want to pre-add keys of filtered out types. */
          (key_types_bitmap == nullptr || id_check_type(id, key_types_bitmap)) &&
          /* We do not want to pre-add keys when we have filter on value types,
           * but not on key types. */
          (val_types_bitmap == nullptr || key_types_bitmap != nullptr))
      {
        PyObject *key = pyrna_id_CreatePyObject(id);
        PyObject *set;

        /* We have to insert the key now,
         * otherwise ID unused would be missing from final dict... */
        if ((set = PyDict_GetItem(data_cb.user_map, key)) == nullptr) {
          set = PySet_New(nullptr);
          PyDict_SetItem(data_cb.user_map, key, set);
          Py_DECREF(set);
        }
        Py_DECREF(key);
      }

      if (val_types_bitmap != nullptr && !id_check_type(id, val_types_bitmap)) {
        continue;
      }

      data_cb.id_curr = id;
      BKE_library_foreach_ID_link(
          nullptr, id, foreach_libblock_id_user_map_callback, &data_cb, IDWALK_NOP);

      if (data_cb.py_id_curr) {
        Py_DECREF(data_cb.py_id_curr);
        data_cb.py_id_curr = nullptr;
      }
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;

  ret = data_cb.user_map;

error:
  if (key_types_bitmap != nullptr) {
    MEM_freeN(key_types_bitmap);
  }
  if (val_types_bitmap != nullptr) {
    MEM_freeN(val_types_bitmap);
  }

  return ret;
}

struct IDFilePathMapData {
  /* Data unchanged for the whole process. */

  /** Set to fill in as we iterate. */
  PyObject *file_path_map;

  /** Whether to include library filepath of linked IDs or not. */
  bool include_libraries;

  /* Data modified for each processed ID. */

  /** The processed ID. */
  ID *id;
  /** The set of file paths for the processed ID. */
  PyObject *id_file_path_set;
};

static bool foreach_id_file_path_map_callback(BPathForeachPathData *bpath_data,
                                              char * /*path_dst*/,
                                              size_t /*path_dst_maxncpy*/,
                                              const char *path_src)
{
  IDFilePathMapData &data = *static_cast<IDFilePathMapData *>(bpath_data->user_data);
  PyObject *id_file_path_set = data.id_file_path_set;

  BLI_assert(data.id == bpath_data->owner_id);

  if (path_src && *path_src) {
    PyObject *path = PyC_UnicodeFromBytes(path_src);
    PySet_Add(id_file_path_set, path);
    Py_DECREF(path);
  }
  return false;
}

static void foreach_id_file_path_map(BPathForeachPathData &bpath_data)
{
  IDFilePathMapData &data = *static_cast<IDFilePathMapData *>(bpath_data.user_data);
  ID *id = data.id;
  PyObject *id_file_path_set = data.id_file_path_set;

  if (data.include_libraries && ID_IS_LINKED(id)) {
    PyObject *path = PyC_UnicodeFromBytes(id->lib->filepath);
    PySet_Add(id_file_path_set, path);
    Py_DECREF(path);
  }

  BKE_bpath_foreach_path_id(&bpath_data, id);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_file_path_map_doc,
    ".. method:: file_path_map(*, subset=None, key_types=None, include_libraries=False)\n"
    "\n"
    "   Returns a mapping of all ID data-blocks in current ``bpy.data`` to a set of all "
    "file paths used by them.\n"
    "\n"
    "   For list of valid set members for key_types, see: "
    ":class:`bpy.types.KeyingSetPath.id_type`.\n"
    "\n"
    "   :arg subset: When given, only these data-blocks and their used file paths "
    "will be included as keys/values in the map.\n"
    "   :type subset: sequence\n"
    "   :arg key_types: When given, filter the keys mapped by ID types. Ignored if ``subset`` is "
    "also given.\n"
    "   :type key_types: set[str]\n"
    "   :arg include_libraries: Include library file paths of linked data. False by default.\n"
    "   :type include_libraries: bool\n"
    "   :return: dictionary of :class:`bpy.types.ID` instances, with sets of file path "
    "strings as their values.\n"
    "   :rtype: dict\n");
static PyObject *bpy_file_path_map(PyObject *self, PyObject *args, PyObject *kwds)
{
  Main *bmain = pyrna_bmain_FromPyObject(self);
  if (!bmain) {
    return nullptr;
  }

  PyObject *subset = nullptr;

  PyObject *key_types = nullptr;
  PyObject *include_libraries = nullptr;
  BLI_bitmap *key_types_bitmap = nullptr;

  PyObject *ret = nullptr;

  IDFilePathMapData filepathmap_data{};
  BPathForeachPathData bpath_data{};

  static const char *_keywords[] = {"subset", "key_types", "include_libraries", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "|$" /* Optional keyword only arguments. */
      "O"  /* `subset` */
      "O!" /* `key_types` */
      "O!" /* `include_libraries` */
      ":file_path_map",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &subset,
                                        &PySet_Type,
                                        &key_types,
                                        &PyBool_Type,
                                        &include_libraries))
  {
    return nullptr;
  }

  if (key_types) {
    key_types_bitmap = pyrna_enum_bitmap_from_set(
        rna_enum_id_type_items, key_types, sizeof(short), true, USHRT_MAX, "key types");
    if (key_types_bitmap == nullptr) {
      goto error;
    }
  }

  bpath_data.bmain = bmain;
  bpath_data.callback_function = foreach_id_file_path_map_callback;
  /* TODO: needs to be controllable from caller (add more options to the API). */
  bpath_data.flag = BKE_BPATH_FOREACH_PATH_SKIP_PACKED | BKE_BPATH_TRAVERSE_SKIP_WEAK_REFERENCES;
  bpath_data.user_data = &filepathmap_data;

  filepathmap_data.include_libraries = (include_libraries == Py_True);

  if (subset) {
    PyObject *subset_fast = PySequence_Fast(subset, "subset");
    if (subset_fast == nullptr) {
      goto error;
    }

    PyObject **subset_array = PySequence_Fast_ITEMS(subset_fast);
    Py_ssize_t subset_len = PySequence_Fast_GET_SIZE(subset_fast);

    filepathmap_data.file_path_map = _PyDict_NewPresized(subset_len);
    for (; subset_len; subset_array++, subset_len--) {
      if (PyDict_Contains(filepathmap_data.file_path_map, *subset_array)) {
        continue;
      }

      ID *id;
      if (!pyrna_id_FromPyObject(*subset_array, &id)) {
        PyErr_Format(PyExc_TypeError,
                     "Expected an ID type in `subset` iterable, not %.200s",
                     Py_TYPE(*subset_array)->tp_name);
        Py_DECREF(subset_fast);
        Py_DECREF(filepathmap_data.file_path_map);
        goto error;
      }

      filepathmap_data.id_file_path_set = PySet_New(nullptr);
      PyDict_SetItem(
          filepathmap_data.file_path_map, *subset_array, filepathmap_data.id_file_path_set);
      Py_DECREF(filepathmap_data.id_file_path_set);

      filepathmap_data.id = id;
      foreach_id_file_path_map(bpath_data);
    }
    Py_DECREF(subset_fast);
  }
  else {
    ListBase *lb;
    ID *id;
    filepathmap_data.file_path_map = PyDict_New();

    FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
      FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
        /* We can skip here in case we have some filter on key types. */
        if (key_types_bitmap && !id_check_type(id, key_types_bitmap)) {
          break;
        }

        PyObject *key = pyrna_id_CreatePyObject(id);
        filepathmap_data.id_file_path_set = PySet_New(nullptr);
        PyDict_SetItem(filepathmap_data.file_path_map, key, filepathmap_data.id_file_path_set);
        Py_DECREF(filepathmap_data.id_file_path_set);
        Py_DECREF(key);

        filepathmap_data.id = id;
        foreach_id_file_path_map(bpath_data);
      }
      FOREACH_MAIN_LISTBASE_ID_END;
    }
    FOREACH_MAIN_LISTBASE_END;
  }

  ret = filepathmap_data.file_path_map;

error:
  if (key_types_bitmap != nullptr) {
    MEM_freeN(key_types_bitmap);
  }

  return ret;
}

struct IDFilePathForeachData {
  /**
   * Python callback function for visiting each path.
   *
   * `def visit_path_fn(owner_id: bpy.types.ID, path: str) -> str | None`
   *
   * If the function returns a string, the path is replaced with the return
   * value.
   */
  PyObject *visit_path_fn;

  /**
   * Set to `true` when there was an exception in the callback function. Once this is set, no
   * Python API function should be called any more (apart from reference counting), so that the
   * error state is maintained correctly.
   */
  bool seen_error;
};

/**
 * Wraps #eBPathForeachFlag from BKE_path.hh.
 *
 * This is exposed publicly (as in, not inline in a function) for the purpose of
 * being included in documentation.
 */
const EnumPropertyItem rna_enum_file_path_foreach_flag_items[] = {
    /* BKE_BPATH_FOREACH_PATH_ABSOLUTE is not included here, as its only use is to initialize a
     * field in BPathForeachPathData that is not used by the callback. */
    {BKE_BPATH_FOREACH_PATH_SKIP_LINKED,
     "SKIP_LINKED",
     0,
     "Skip Linked",
     "Skip paths of linked IDs"},
    {BKE_BPATH_FOREACH_PATH_SKIP_PACKED,
     "SKIP_PACKED",
     0,
     "Skip Packed",
     "Skip paths when their matching data is packed"},
    {BKE_BPATH_FOREACH_PATH_RESOLVE_TOKEN,
     "RESOLVE_TOKEN",
     0,
     "Resolve Token",
     "Resolve tokens within a virtual filepath to a single, concrete, filepath. Currently only "
     "used for UDIM tiles"},
    {BKE_BPATH_TRAVERSE_SKIP_WEAK_REFERENCES,
     "SKIP_WEAK_REFERENCES",
     0,
     "Skip Weak References",
     "Skip weak reference paths. Those paths are typically 'nice to have' extra information, but "
     "are not used as actual source of data by the current .blend file"},
    {BKE_BPATH_FOREACH_PATH_SKIP_MULTIFILE,
     "SKIP_MULTIFILE",
     0,
     "Skip Multi-file",
     "Skip paths where a single dir is used with an array of files, eg. sequence strip images or "
     "point-caches. In this case only the first file path is processed. This is needed for "
     "directory manipulation callbacks which might otherwise modify the same directory multiple "
     "times"},
    {BKE_BPATH_FOREACH_PATH_RELOAD_EDITED,
     "RELOAD_EDITED",
     0,
     "Reload Edited",
     "Reload data when the path is edited"},
    {0, nullptr, 0, nullptr, nullptr},
};

static bool foreach_id_file_path_foreach_callback(BPathForeachPathData *bpath_data,
                                                  char *path_dst,
                                                  const size_t path_dst_maxncpy,
                                                  const char *path_src)
{
  IDFilePathForeachData &data = *static_cast<IDFilePathForeachData *>(bpath_data->user_data);

  if (data.seen_error) {
    /* The Python interpreter is already set up for reporting an exception, so don't touch it. */
    return false;
  }

  if (!path_src || !path_src[0]) {
    return false;
  }
  BLI_assert(path_dst);

  /* Construct the callback function parameters. */
  PointerRNA id_ptr = RNA_id_pointer_create(bpath_data->owner_id);
  PyObject *args = PyTuple_New(3);
  /* args[0]: */
  PyObject *py_owner_id = pyrna_struct_CreatePyObject(&id_ptr);
  /* args[1]: */
  PyObject *py_path_src = PyUnicode_FromString(path_src);
  /* args[2]: currently-unused parameter for passing metadata of the path to the Python function.
   * This is intended pass info like:
   *  - Is the path intended to reference a directory or a file.
   *  - Does the path support templates.
   *  - Is the path referring to input or output (the render output, or file output nodes).
   * Even though this is not implemented currently, the parameter is already added so that the
   * eventual implementation is not an API-breaking change. */
  PyObject *py_path_meta = Py_NewRef(Py_None);
  PyTuple_SET_ITEMS(args, py_owner_id, py_path_src, py_path_meta);

  /* Call the Python callback function. */
  PyObject *result = PyObject_CallObject(data.visit_path_fn, args);

  /* Done with the function arguments. */
  Py_DECREF(args);
  args = nullptr;

  if (result == nullptr) {
    data.seen_error = true;
    return false;
  }

  if (result == Py_None) {
    /* Nothing to do. */
    Py_DECREF(result);
    return false;
  }

  if (!PyUnicode_Check(result)) {
    PyErr_Format(PyExc_TypeError,
                 "visit_path_fn() should return a string or None, but returned %s for "
                 "owner_id=\"%s\" and file_path=\"%s\"",
                 Py_TYPE(result)->tp_name,
                 bpath_data->owner_id->name,
                 path_src);
    data.seen_error = true;
    Py_DECREF(result);
    return false;
  }

  /* Copy the returned string back into the path. */
  Py_ssize_t replacement_path_length = 0;
  PyObject *value_coerce = nullptr;
  const char *replacement_path = PyC_UnicodeAsBytesAndSize(
      result, &replacement_path_length, &value_coerce);

  /* BLI_strncpy wants buffer size, but PyC_UnicodeAsBytesAndSize reports string
   * length, hence the +1. */
  BLI_strncpy(
      path_dst, replacement_path, std::min(path_dst_maxncpy, size_t(replacement_path_length + 1)));

  Py_XDECREF(value_coerce);
  Py_DECREF(result);
  return true;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_file_path_foreach_doc,
    ".. method:: file_path_foreach(visit_path_fn, *, subset=None, visit_types=None, "
    "flags={'SKIP_PACKED', 'SKIP_WEAK_REFERENCES'})\n"
    "\n"
    "   Call ``visit_path_fn`` for the file paths used by all ID data-blocks in current "
    "``bpy.data``.\n"
    "\n"
    "   For list of valid set members for visit_types, see: "
    ":class:`bpy.types.KeyingSetPath.id_type`.\n"
    "\n"
    "   :arg visit_path_fn: function that takes three parameters: the data-block, a file path, "
    "and a placeholder for future use. The function should return either ``None`` or a ``str``. "
    "In the latter case, the visited file path will be replaced with the returned string.\n"
    "   :type visit_path_fn: Callable[[:class:`bpy.types.ID`, str, Any], str|None]\n"
    "   :arg subset: When given, only these data-blocks and their used file paths "
    "will be visited.\n"
    "   :type subset: set[str]\n"
    "   :arg visit_types: When given, only visit data-blocks of these types. Ignored if "
    "``subset`` is also given.\n"
    "   :type visit_types: set[str]\n"
    "   :type flags: set[str]\n"
    "   :arg flags: Set of flags that influence which data-blocks are visited. See "
    ":ref:`rna_enum_file_path_foreach_flag_items`.\n");
static PyObject *bpy_file_path_foreach(PyObject *self, PyObject *args, PyObject *kwds)
{
  Main *bmain = pyrna_bmain_FromPyObject(self);
  if (!bmain) {
    return nullptr;
  }

  PyObject *visit_path_fn = nullptr;
  PyObject *subset = nullptr;
  PyObject *visit_types = nullptr;
  std::unique_ptr<BLI_bitmap, MEM_freeN_smart_ptr_deleter> visit_types_bitmap;
  PyObject *py_flags = nullptr;

  IDFilePathForeachData filepathforeach_data{};
  BPathForeachPathData bpath_data{};

  static const char *_keywords[] = {"visit_path_fn", "subset", "visit_types", "flags", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O!" /* `visit_path_fn` */
      "|$" /* Optional keyword only arguments. */
      "O"  /* `subset` */
      "O!" /* `visit_types` */
      "O!" /* `flags` */
      ":file_path_foreach",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        &PyFunction_Type,
                                        &visit_path_fn,
                                        &subset,
                                        &PySet_Type,
                                        &visit_types,
                                        &PySet_Type,
                                        &py_flags))
  {
    return nullptr;
  }

  if (visit_types) {
    BLI_bitmap *visit_types_bitmap_rawptr = pyrna_enum_bitmap_from_set(
        rna_enum_id_type_items, visit_types, sizeof(short), true, USHRT_MAX, "visit_types");
    if (visit_types_bitmap_rawptr == nullptr) {
      return nullptr;
    }
    visit_types_bitmap.reset(visit_types_bitmap_rawptr);
  }

  /* Parse the flags, start with sensible defaults. */
  bpath_data.flag = BKE_BPATH_FOREACH_PATH_SKIP_PACKED | BKE_BPATH_TRAVERSE_SKIP_WEAK_REFERENCES;
  if (py_flags) {
    if (pyrna_enum_bitfield_from_set(rna_enum_file_path_foreach_flag_items,
                                     py_flags,
                                     reinterpret_cast<int *>(&bpath_data.flag),
                                     "flags") == -1)
    {
      return nullptr;
    }
  }

  bpath_data.bmain = bmain;
  bpath_data.callback_function = foreach_id_file_path_foreach_callback;
  bpath_data.user_data = &filepathforeach_data;

  filepathforeach_data.visit_path_fn = visit_path_fn;
  filepathforeach_data.seen_error = false;

  if (subset) {
    /* Visit the given subset of IDs. */
    PyObject *subset_fast = PySequence_Fast(subset, "subset");
    if (!subset_fast) {
      return nullptr;
    }

    PyObject **subset_array = PySequence_Fast_ITEMS(subset_fast);
    const Py_ssize_t subset_len = PySequence_Fast_GET_SIZE(subset_fast);
    for (Py_ssize_t index = 0; index < subset_len; index++) {
      PyObject *subset_item = subset_array[index];

      ID *id;
      if (!pyrna_id_FromPyObject(subset_item, &id)) {
        PyErr_Format(PyExc_TypeError,
                     "Expected an ID type in `subset` iterable, not %.200s",
                     Py_TYPE(subset_item)->tp_name);
        Py_DECREF(subset_fast);
        return nullptr;
      }

      BKE_bpath_foreach_path_id(&bpath_data, id);
      if (filepathforeach_data.seen_error) {
        /* Whatever triggered this error should have already set up the Python
         * interpreter for producing an exception. */
        Py_DECREF(subset_fast);
        return nullptr;
      }
    }
    Py_DECREF(subset_fast);
  }
  else {
    /* Visit all IDs, filtered by type if necessary. */
    ListBase *lb;
    FOREACH_MAIN_LISTBASE_BEGIN (bmain, lb) {
      ID *id;
      FOREACH_MAIN_LISTBASE_ID_BEGIN (lb, id) {
        if (visit_types_bitmap && !id_check_type(id, visit_types_bitmap.get())) {
          break;
        }

        BKE_bpath_foreach_path_id(&bpath_data, id);
        if (filepathforeach_data.seen_error) {
          /* Whatever triggered this error should have already set up the Python
           * interpreter for producing an exception. */
          return nullptr;
        }
      }
      FOREACH_MAIN_LISTBASE_ID_END;
    }
    FOREACH_MAIN_LISTBASE_END;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_batch_remove_doc,
    ".. method:: batch_remove(ids)\n"
    "\n"
    "   Remove (delete) several IDs at once.\n"
    "\n"
    "   Note that this function is quicker than individual calls to :func:`remove()` "
    "(from :class:`bpy.types.BlendData`\n"
    "   ID collections), but less safe/versatile (it can break Blender, e.g. by removing "
    "all scenes...).\n"
    "\n"
    "   :arg ids: Sequence of IDs (types can be mixed).\n"
    "   :type ids: Sequence[:class:`bpy.types.ID`]\n");
static PyObject *bpy_batch_remove(PyObject *self, PyObject *args, PyObject *kwds)
{
  Main *bmain = pyrna_bmain_FromPyObject(self);
  if (!bmain) {
    return nullptr;
  }

  PyObject *ids = nullptr;

  static const char *_keywords[] = {"ids", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O" /* `ids` */
      ":batch_remove",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kwds, &_parser, &ids)) {
    return nullptr;
  }

  if (!ids) {
    return nullptr;
  }

  PyObject *ids_fast = PySequence_Fast(ids, "batch_remove");
  if (ids_fast == nullptr) {
    return nullptr;
  }

  PyObject **ids_array = PySequence_Fast_ITEMS(ids_fast);
  Py_ssize_t ids_len = PySequence_Fast_GET_SIZE(ids_fast);
  blender::Set<ID *> ids_to_delete;
  for (; ids_len; ids_array++, ids_len--) {
    ID *id;
    if (!pyrna_id_FromPyObject(*ids_array, &id)) {
      PyErr_Format(
          PyExc_TypeError, "Expected an ID type, not %.200s", Py_TYPE(*ids_array)->tp_name);
      Py_DECREF(ids_fast);
      return nullptr;
    }

    ids_to_delete.add(id);
  }
  Py_DECREF(ids_fast);

  BKE_id_multi_delete(bmain, ids_to_delete);
  /* Force full redraw, mandatory to avoid crashes when running this from UI... */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_orphans_purge_doc,
    ".. method:: orphans_purge()\n"
    "\n"
    "   Remove (delete) all IDs with no user.\n"
    "\n"
    "   :arg do_local_ids: Include unused local IDs in the deletion, defaults to True\n"
    "   :type do_local_ids: bool, optional\n"
    "   :arg do_linked_ids: Include unused linked IDs in the deletion, defaults to True\n"
    "   :type do_linked_ids: bool, optional\n"
    "   :arg do_recursive: Recursively check for unused IDs, ensuring no orphaned one "
    "remain after a single run of that function, defaults to False\n"
    "   :type do_recursive: bool, optional\n"
    "   :return: The number of deleted IDs.\n");
static PyObject *bpy_orphans_purge(PyObject *self, PyObject *args, PyObject *kwds)
{
  Main *bmain = pyrna_bmain_FromPyObject(self);
  if (!bmain) {
    return nullptr;
  }

  LibQueryUnusedIDsData unused_ids_data;
  unused_ids_data.do_local_ids = true;
  unused_ids_data.do_linked_ids = true;
  unused_ids_data.do_recursive = false;

  static const char *_keywords[] = {"do_local_ids", "do_linked_ids", "do_recursive", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "|"  /* Optional arguments. */
      "O&" /* `do_local_ids` */
      "O&" /* `do_linked_ids` */
      "O&" /* `do_recursive` */
      ":orphans_purge",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kwds,
                                        &_parser,
                                        PyC_ParseBool,
                                        &unused_ids_data.do_local_ids,
                                        PyC_ParseBool,
                                        &unused_ids_data.do_linked_ids,
                                        PyC_ParseBool,
                                        &unused_ids_data.do_recursive))
  {
    return nullptr;
  }

  /* Tag all IDs to delete. */
  BKE_lib_query_unused_ids_tag(bmain, ID_TAG_DOIT, unused_ids_data);

  if (unused_ids_data.num_total[INDEX_ID_NULL] == 0) {
    return PyLong_FromSize_t(0);
  }

  const size_t num_datablocks_deleted = BKE_id_multi_tagged_delete(bmain);
  /* Force full redraw, mandatory to avoid crashes when running this from UI... */
  WM_main_add_notifier(NC_WINDOW, nullptr);

  return PyLong_FromSize_t(num_datablocks_deleted);
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

PyMethodDef BPY_rna_id_collection_user_map_method_def = {
    "user_map",
    (PyCFunction)bpy_user_map,
    METH_VARARGS | METH_KEYWORDS,
    bpy_user_map_doc,
};
PyMethodDef BPY_rna_id_collection_file_path_map_method_def = {
    "file_path_map",
    (PyCFunction)bpy_file_path_map,
    METH_VARARGS | METH_KEYWORDS,
    bpy_file_path_map_doc,
};
PyMethodDef BPY_rna_id_collection_file_path_foreach_method_def = {
    "file_path_foreach",
    (PyCFunction)bpy_file_path_foreach,
    METH_VARARGS | METH_KEYWORDS,
    bpy_file_path_foreach_doc,
};
PyMethodDef BPY_rna_id_collection_batch_remove_method_def = {
    "batch_remove",
    (PyCFunction)bpy_batch_remove,
    METH_VARARGS | METH_KEYWORDS,
    bpy_batch_remove_doc,
};
PyMethodDef BPY_rna_id_collection_orphans_purge_method_def = {
    "orphans_purge",
    (PyCFunction)bpy_orphans_purge,
    METH_VARARGS | METH_KEYWORDS,
    bpy_orphans_purge_doc,
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif
