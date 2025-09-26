/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file exposed blend file library appending/linking to python, typically
 * this would be done via RNA API but in this case a hand written Python API
 * allows us to use Python's context manager (`__enter__` and `__exit__`).
 *
 * Everything here is exposed via `bpy.data.libraries.load(...)` which returns
 * a context manager.
 */

#include <Python.h>
#include <cstddef>

#include "BLI_linklist.h"
#include "BLI_math_vector_types.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_blender_version.h"
#include "BKE_blendfile_link_append.hh"
#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "DNA_space_types.h" /* FILE_LINK, FILE_RELPATH */

#include "BLO_readfile.hh"

#include "bpy_capi_utils.hh"
#include "bpy_library.hh"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */
#include "../generic/python_utildefines.hh"

/* nifty feature. swap out strings for RNA data */
#define USE_RNA_DATABLOCKS

#ifdef USE_RNA_DATABLOCKS
#  include "RNA_access.hh"
#  include "bpy_rna.hh"
#endif

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

struct BoolFlagPair {
  bool value;
  uint32_t flag;
};
static uint32_t bool_flag_pair_as_flag(const BoolFlagPair *bool_flags, int bool_flags_num)
{
  uint32_t flag = 0;
  for (int i = 0; i < bool_flags_num; i++) {
    BLI_assert(bool_flags[i].flag);
    if (bool_flags[i].value) {
      flag |= bool_flags[i].flag;
    }
  }
  return flag;
}

/** \} */

/**
 * The size to pre-allocate #BPy_Library::dict Add +1 for the "version".
 */
static constexpr Py_ssize_t bpy_library_dict_num = INDEX_ID_MAX + 1;

struct BPy_Library {
  /** Required Python macro. */
  PyObject_HEAD

  /** The path supplied by the caller (may be relative). */
  char relpath[FILE_MAX];
  /** The absolute path. */
  char abspath[FILE_MAX];
  BlendHandle *blo_handle;
  /** Referenced by `blo_handle`, so stored here to keep alive for long enough. */
  ReportList reports;
  BlendFileReadReport bf_reports;

  int flag;

  bool create_liboverrides;
  eBKELibLinkOverride liboverride_flags;

  /**
   * A dictionary, accessed via attributes (so keys are strings).
   * - Stores the ID types ("meshes", "objects", etc...).
   * - Also has a "version" attribute to support accessing the blender version.
   *
   * Assigned a pre-sized dictionary using #BPY_LIBRARY_DICT_NUM_INIT item.
   * this will always have these the ID names and some additional slots filled.
   */
  PyObject *dict;
  /**
   * Borrowed reference to the `bmain`, taken from the RNA instance of #RNA_BlendDataLibraries.
   * Defaults to #G.main, Otherwise use a temporary #Main when `bmain_is_temp` is true.
   */
  Main *bmain;
  bool bmain_is_temp;
};

static PyObject *bpy_lib_load(BPy_PropertyRNA *self, PyObject *args, PyObject *kw);
static PyObject *bpy_lib_enter(BPy_Library *self);
static PyObject *bpy_lib_exit(BPy_Library *self, PyObject *args);
static PyObject *bpy_lib_dir(BPy_Library *self);

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef bpy_lib_methods[] = {
    {"__enter__", (PyCFunction)bpy_lib_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)bpy_lib_exit, METH_VARARGS},
    {"__dir__", (PyCFunction)bpy_lib_dir, METH_NOARGS},
    {nullptr} /* sentinel */
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static void bpy_lib_dealloc(BPy_Library *self)
{
  Py_XDECREF(self->dict);
  Py_TYPE(self)->tp_free(self);
}

static PyTypeObject bpy_lib_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "bpy_lib",
    /*tp_basicsize*/ sizeof(BPy_Library),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)bpy_lib_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ PyObject_GenericGetAttr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ bpy_lib_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ offsetof(BPy_Library, dict),
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

PyDoc_STRVAR(
    /* Wrap. */
    bpy_lib_load_doc,
    ".. method:: load("
    "filepath, "
    "*, "
    "link=False, "
    "relative=False, "
    "set_fake=False, "
    "recursive=False, "
    "reuse_local_id=False, "
    "assets_only=False, "
    "clear_asset_data=False, "
    "create_liboverrides=False, "
    "reuse_liboverrides=False, "
    "create_liboverrides_runtime=False)\n"
    "\n"
    "   Returns a context manager which exposes 2 library objects on entering.\n"
    "   Each object has attributes matching bpy.data which are lists of strings to be linked.\n"
    "\n"
    "   :arg filepath: The path to a blend file.\n"
    "   :type filepath: str | bytes\n"
    "   :arg link: When False reference to the original file is lost.\n"
    "   :type link: bool\n"
    "   :arg pack: If True, and ``link`` is also True, pack linked data-blocks into the current "
    "blend-file.\n"
    "   :type pack: bool\n"
    "   :arg relative: When True the path is stored relative to the open blend file.\n"
    "   :type relative: bool\n"
    "   :arg set_fake: If True, set fake user on appended IDs.\n"
    "   :type set_fake: bool\n"
    "   :arg recursive: If True, also make indirect dependencies of appended libraries local.\n"
    "   :type recursive: bool\n"
    "   :arg reuse_local_id: If True,"
    "try to re-use previously appended matching ID on new append.\n"
    "   :type reuse_local_id: bool\n"
    "   :arg assets_only: If True, only list data-blocks marked as assets.\n"
    "   :type assets_only: bool\n"
    "   :arg clear_asset_data: If True, "
    "clear the asset data on append (it is always kept for linked data).\n"
    "   :type clear_asset_data: bool\n"
    "   :arg create_liboverrides: If True and ``link`` is True, liboverrides will\n"
    "      be created for linked data.\n"
    "   :type create_liboverrides: bool\n"
    "   :arg reuse_liboverrides: If True and ``create_liboverride`` is True,\n"
    "      search for existing liboverride first.\n"
    "   :type reuse_liboverrides: bool\n"
    "   :arg create_liboverrides_runtime: If True and ``create_liboverride`` is True,\n"
    "      create (or search for existing) runtime liboverride.\n"
    "   :type create_liboverrides_runtime: bool\n");
static PyObject *bpy_lib_load(BPy_PropertyRNA *self, PyObject *args, PyObject *kw)
{
  Main *bmain_base = CTX_data_main(BPY_context_get());
  Main *bmain = static_cast<Main *>(self->ptr->data); /* Typically #G_MAIN */
  BPy_Library *ret;
  PyC_UnicodeAsBytesAndSize_Data filepath_data = {nullptr};

  /* #BPy_Library::flag
   *
   * - #BLO_LIBLINK_OBDATA_INSTANCE: The caller must manage instancing.
   * - #BLO_LIBLINK_COLLECTION_INSTANCE: The caller must manage instancing.
   */
  struct {
    BoolFlagPair is_link = {false, FILE_LINK};
    BoolFlagPair is_pack = {false, BLO_LIBLINK_PACK};
    BoolFlagPair is_relative = {false, FILE_RELPATH};
    BoolFlagPair set_fake = {false, BLO_LIBLINK_APPEND_SET_FAKEUSER};
    BoolFlagPair recursive = {false, BLO_LIBLINK_APPEND_RECURSIVE};
    BoolFlagPair reuse_local_id = {false, BLO_LIBLINK_APPEND_LOCAL_ID_REUSE};
    BoolFlagPair assets_only = {false, FILE_ASSETS_ONLY};
    BoolFlagPair clear_asset_data = {false, BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR};
  } flag_vars;

  bool create_liboverrides = false;

  /* #BPy_Library::liboverride_flags */
  struct {
    BoolFlagPair reuse_liboverrides = {false, BKE_LIBLINK_OVERRIDE_USE_EXISTING_LIBOVERRIDES};
    BoolFlagPair create_liboverrides_runtime = {false, BKE_LIBLINK_OVERRIDE_CREATE_RUNTIME};
  } liboverride_flag_vars;

  static const char *_keywords[] = {
      "filepath",
      "link",
      "pack",
      "relative",
      "set_fake",
      "recursive",
      "reuse_local_id",
      "assets_only",
      "clear_asset_data",
      "create_liboverrides",
      "reuse_liboverrides",
      "create_liboverrides_runtime",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O&" /* `filepath` */
      /* Optional keyword only arguments. */
      "|$"
      "O&" /* `link` */
      "O&" /* `pack` */
      "O&" /* `relative` */
      "O&" /* `recursive` */
      "O&" /* `set_fake` */
      "O&" /* `reuse_local_id` */
      "O&" /* `assets_only` */
      "O&" /* `clear_asset_data` */
      "O&" /* `create_liboverrides` */
      "O&" /* `reuse_liboverrides` */
      "O&" /* `create_liboverrides_runtime` */
      ":load",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        PyC_ParseUnicodeAsBytesAndSize,
                                        &filepath_data,
                                        PyC_ParseBool,
                                        &flag_vars.is_link,
                                        PyC_ParseBool,
                                        &flag_vars.is_pack,
                                        PyC_ParseBool,
                                        &flag_vars.is_relative,
                                        PyC_ParseBool,
                                        &flag_vars.recursive,
                                        PyC_ParseBool,
                                        &flag_vars.set_fake,
                                        PyC_ParseBool,
                                        &flag_vars.reuse_local_id,
                                        PyC_ParseBool,
                                        &flag_vars.assets_only,
                                        PyC_ParseBool,
                                        &flag_vars.clear_asset_data,
                                        PyC_ParseBool,
                                        &create_liboverrides,
                                        PyC_ParseBool,
                                        &liboverride_flag_vars.reuse_liboverrides,
                                        PyC_ParseBool,
                                        &liboverride_flag_vars.create_liboverrides_runtime))
  {
    return nullptr;
  }

  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  char filepath_rel[FILE_MAX];
  char filepath_abs[FILE_MAX];

  STRNCPY(filepath_rel, filepath_data.value);
  STRNCPY(filepath_abs, filepath_rel);
  BLI_path_abs(filepath_abs, blendfile_path);
  Py_XDECREF(filepath_data.value_coerce);

  if (blendfile_path[0]) {
    /* NOTE: intentionally leave `filepath_abs` and only use normalizing for comparison.
     * It's important that this comparison matches read-files logic for matching paths.
     * See the logic inside #BKE_blendfile_link.
     *
     * This means it's not necessary to check if the paths are *actually* the same.
     * It's possible to load from this file if a user makes a symbolic-link - for example.
     * See #140929. */
    char filepath_abs_normalized[FILE_MAX];
    STRNCPY(filepath_abs_normalized, filepath_abs);
    BLI_path_normalize(filepath_abs_normalized);
    if (BLI_path_cmp(filepath_abs_normalized, blendfile_path) == 0) {
      PyErr_SetString(PyExc_ValueError, "Cannot load from the current blend file.");
      return nullptr;
    }
  }

  if (flag_vars.is_link.value) {
    /* Link. */
    if (flag_vars.set_fake.value) {
      PyErr_SetString(PyExc_ValueError, "`link` must be False if `set_fake` is True");
      return nullptr;
    }
    if (flag_vars.recursive.value) {
      PyErr_SetString(PyExc_ValueError, "`link` must be False if `recursive` is True");
      return nullptr;
    }
    if (flag_vars.reuse_local_id.value) {
      PyErr_SetString(PyExc_ValueError, "`link` must be False if `reuse_local_id` is True");
      return nullptr;
    }
    if (flag_vars.clear_asset_data.value) {
      PyErr_SetString(PyExc_ValueError, "`link` must be False if `clear_asset_data` is True");
      return nullptr;
    }
  }
  else {
    /* Append. */
    if (create_liboverrides) {
      PyErr_SetString(PyExc_ValueError, "`link` is False but `create_liboverrides` is True");
      return nullptr;
    }
    if (flag_vars.is_pack.value) {
      PyErr_SetString(PyExc_ValueError, "`pack` must be False if `link` is False");
      return nullptr;
    }
  }

  if (create_liboverrides) {
    /* Library overrides. */
    if (flag_vars.is_pack.value) {
      PyErr_SetString(PyExc_ValueError, "`create_liboverrides` must be False if `pack` is True");
      return nullptr;
    }
  }
  else {
    /* Library overrides (disabled). */
    if (liboverride_flag_vars.reuse_liboverrides.value) {
      PyErr_SetString(PyExc_ValueError,
                      "`create_liboverrides` is False but `reuse_liboverrides` is True");
      return nullptr;
    }
    if (liboverride_flag_vars.create_liboverrides_runtime.value) {
      PyErr_SetString(PyExc_ValueError,
                      "`create_liboverrides` is False but `create_liboverrides_runtime` is True");
      return nullptr;
    }
  }

  ret = PyObject_New(BPy_Library, &bpy_lib_Type);

  STRNCPY(ret->relpath, filepath_rel);
  STRNCPY(ret->abspath, filepath_abs);

  ret->bmain = bmain;
  ret->bmain_is_temp = (bmain != bmain_base);

  ret->blo_handle = nullptr;

  ret->flag = bool_flag_pair_as_flag(reinterpret_cast<const BoolFlagPair *>(&flag_vars),
                                     sizeof(flag_vars) / sizeof(BoolFlagPair));

  ret->create_liboverrides = create_liboverrides;
  ret->liboverride_flags = create_liboverrides ?
                               eBKELibLinkOverride(bool_flag_pair_as_flag(
                                   reinterpret_cast<const BoolFlagPair *>(&liboverride_flag_vars),
                                   sizeof(liboverride_flag_vars) / sizeof(BoolFlagPair))) :
                               eBKELibLinkOverride(0);

  ret->dict = _PyDict_NewPresized(bpy_library_dict_num);

  return (PyObject *)ret;
}

static PyObject *_bpy_names(BPy_Library *self, int blocktype)
{
  int names_num;
  LinkNode *names = BLO_blendhandle_get_datablock_names(
      self->blo_handle, blocktype, (self->flag & FILE_ASSETS_ONLY) != 0, &names_num);
  PyObject *list = PyList_New(names_num);

  if (names) {
    int i = 0;
    for (LinkNode *l = names; l; l = l->next, i++) {
      PyList_SET_ITEM(list, i, PyUnicode_FromString((char *)l->link));
    }
    BLI_linklist_freeN(names); /* free linklist *and* each node's data */
  }

  return list;
}

static PyObject *bpy_lib_enter(BPy_Library *self)
{
  ReportList *reports = &self->reports;
  BlendFileReadReport *bf_reports = &self->bf_reports;

  BKE_reports_init(reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);
  memset(bf_reports, 0, sizeof(*bf_reports));
  bf_reports->reports = reports;

  self->blo_handle = BLO_blendhandle_from_file(self->abspath, bf_reports);

  if (self->blo_handle == nullptr) {
    if (BPy_reports_to_error(reports, PyExc_IOError, true) != -1) {
      PyErr_Format(PyExc_IOError, "load: %s failed to open blend file", self->abspath);
    }
    return nullptr;
  }

  PyObject *dict_src = _PyDict_NewPresized(bpy_library_dict_num);
  PyObject *dict_dst = self->dict; /* Only for convenience (always `self->dict`). */
  int dict_num_offset = 0;

  int i = 0, code;
  while ((code = BKE_idtype_idcode_iter_step(&i))) {
    if (!BKE_idtype_idcode_is_linkable(code)) {
      dict_num_offset += 1;
      continue;
    }
    const char *name_plural = BKE_idtype_idcode_to_name_plural(code);
    PyObject *str = PyUnicode_FromString(name_plural);
    PyObject *item;

    PyDict_SetItem(dict_dst, str, item = PyList_New(0));
    Py_DECREF(item);
    PyDict_SetItem(dict_src, str, item = _bpy_names(self, code));
    Py_DECREF(item);

    Py_DECREF(str);
  }

  /* Create a dummy. */
  BPy_Library *self_src = PyObject_New(BPy_Library, &bpy_lib_Type);
  STRNCPY(self_src->relpath, self->relpath);
  STRNCPY(self_src->abspath, self->abspath);

  /* Library blend-file version. */
  {
    PyObject *version;
    PyObject *identifier = PyUnicode_FromString("version");
    blender::int3 blendfile_version;

    /* Source. */
    blendfile_version = BLO_blendhandle_get_version(self->blo_handle);
    version = PyC_Tuple_PackArray_I32(&blendfile_version[0], 3);
    PyDict_SetItem(dict_src, identifier, version);
    Py_DECREF(version);

    /* Destination. */
    blendfile_version = blender::int3(
        BLENDER_FILE_VERSION / 100, BLENDER_FILE_VERSION % 100, BLENDER_FILE_SUBVERSION);
    version = PyC_Tuple_PackArray_I32(&blendfile_version[0], 3);
    PyDict_SetItem(dict_dst, identifier, version);
    Py_DECREF(version);

    Py_DECREF(identifier);
  }

  self_src->blo_handle = nullptr;
  self_src->flag = 0;
  self_src->create_liboverrides = false;
  self_src->liboverride_flags = BKE_LIBLINK_OVERRIDE_INIT;
  self_src->dict = dict_src; /* owns the dict */

  /* While it's not a bug if the sizes differ, the size is expected to match.
   * Ensure `bpy_library_dict_num` gets updated when members are added. */
  BLI_assert(PyDict_GET_SIZE(self_src->dict) + dict_num_offset == bpy_library_dict_num);
  BLI_assert(PyDict_GET_SIZE(self->dict) + dict_num_offset == bpy_library_dict_num);
  UNUSED_VARS_NDEBUG(dict_num_offset);

  BKE_reports_clear(reports);

  /* Return a pair. */
  PyObject *ret = PyTuple_New(2);
  PyTuple_SET_ITEMS(ret, (PyObject *)self_src, Py_NewRef((PyObject *)self));
  return ret;
}

static void bpy_lib_exit_warn_idname(BPy_Library *self,
                                     const char *name_plural,
                                     const char *idname)
{
  PyObject *exc, *val, *tb;
  PyErr_Fetch(&exc, &val, &tb);
  if (PyErr_WarnFormat(PyExc_UserWarning,
                       1,
                       "load: '%s' does not contain %s[\"%s\"]",
                       self->abspath,
                       name_plural,
                       idname))
  {
    /* Spurious errors can appear at shutdown */
    if (PyErr_ExceptionMatches(PyExc_Warning)) {
      PyErr_WriteUnraisable((PyObject *)self);
    }
  }
  PyErr_Restore(exc, val, tb);
}

static void bpy_lib_exit_warn_type(BPy_Library *self, PyObject *item)
{
  PyObject *exc, *val, *tb;
  PyErr_Fetch(&exc, &val, &tb);
  if (PyErr_WarnFormat(PyExc_UserWarning,
                       1,
                       "load: '%s' expected a string type, not a %.200s",
                       self->abspath,
                       Py_TYPE(item)->tp_name))
  {
    /* Spurious errors can appear at shutdown */
    if (PyErr_ExceptionMatches(PyExc_Warning)) {
      PyErr_WriteUnraisable((PyObject *)self);
    }
  }
  PyErr_Restore(exc, val, tb);
}

struct LibExitLappContextItemsIterData {
  short idcode;
  BPy_Library *py_library;
  PyObject *py_list;
  Py_ssize_t py_list_size;
};

static bool bpy_lib_exit_lapp_context_items_cb(BlendfileLinkAppendContext *lapp_context,
                                               BlendfileLinkAppendContextItem *item,
                                               LibExitLappContextItemsIterData &data)
{
  /* Since `bpy_lib_exit` loops over all ID types, all items in `lapp_context` end up being looped
   * over for each ID type, so when it does not match the item can simply be skipped: it either has
   * already been processed, or will be processed in a later loop. */
  if (BKE_blendfile_link_append_context_item_idcode_get(lapp_context, item) != data.idcode) {
    return true;
  }

  const int py_list_index = POINTER_AS_INT(
      BKE_blendfile_link_append_context_item_userdata_get(lapp_context, item));
  ID *new_id = BKE_blendfile_link_append_context_item_newid_get(lapp_context, item);
  ID *liboverride_id = data.py_library->create_liboverrides ?
                           BKE_blendfile_link_append_context_item_liboverrideid_get(lapp_context,
                                                                                    item) :
                           nullptr;

  BLI_assert(py_list_index < data.py_list_size);

  /* Fully invalid items (which got set to `Py_None` already in first loop of `bpy_lib_exit`)
   * should never be accessed here, since their index should never be set to any item in
   * `lapp_context`. */
  PyObject *item_src = PyList_GET_ITEM(data.py_list, py_list_index);
  BLI_assert(item_src != Py_None);

  PyObject *py_item;
  if (liboverride_id != nullptr) {
    PointerRNA newid_ptr = RNA_id_pointer_create(liboverride_id);
    py_item = pyrna_struct_CreatePyObject(&newid_ptr);
  }
  else if (new_id != nullptr) {
    PointerRNA newid_ptr = RNA_id_pointer_create(new_id);
    py_item = pyrna_struct_CreatePyObject(&newid_ptr);
  }
  else {
    const char *item_idname = PyUnicode_AsUTF8(item_src);
    const char *idcode_name_plural = BKE_idtype_idcode_to_name_plural(data.idcode);

    bpy_lib_exit_warn_idname(data.py_library, idcode_name_plural, item_idname);

    py_item = Py_NewRef(Py_None);
  }

  PyList_SET_ITEM(data.py_list, py_list_index, py_item);

  Py_DECREF(item_src);

  return true;
}

static PyObject *bpy_lib_exit(BPy_Library *self, PyObject * /*args*/)
{
  Main *bmain = self->bmain;
  const bool do_pack = ((self->flag & BLO_LIBLINK_PACK) != 0);
  const bool do_append = ((self->flag & FILE_LINK) == 0);
  const bool create_liboverrides = self->create_liboverrides;
  /* Code in #bpy_lib_load should have raised exception in case of incompatible parameter values.
   */
  BLI_assert(!do_append || !create_liboverrides);

  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, true);

  /* here appending/linking starts */
  const int id_tag_extra = self->bmain_is_temp ? int(ID_TAG_TEMP_MAIN) : 0;
  LibraryLink_Params liblink_params;
  BLO_library_link_params_init(&liblink_params, bmain, self->flag, id_tag_extra);

  BlendfileLinkAppendContext *lapp_context = BKE_blendfile_link_append_context_new(
      &liblink_params);
  /* NOTE: Transfers the ownership of the `blo_handle` to the `lapp_context`. */
  BKE_blendfile_link_append_context_library_add(lapp_context, self->abspath, self->blo_handle);
  self->blo_handle = nullptr;

  int idcode_step = 0;
  short idcode;
  while ((idcode = BKE_idtype_idcode_iter_step(&idcode_step))) {
    if (!BKE_idtype_idcode_is_linkable(idcode) || (idcode == ID_WS && !do_append)) {
      continue;
    }

    const char *name_plural = BKE_idtype_idcode_to_name_plural(idcode);
    PyObject *ls = PyDict_GetItemString(self->dict, name_plural);
    // printf("lib: %s\n", name_plural);
    if (ls == nullptr || !PyList_Check(ls)) {
      continue;
    }

    const Py_ssize_t size = PyList_GET_SIZE(ls);
    if (size == 0) {
      continue;
    }

    /* loop */
    for (Py_ssize_t i = 0; i < size; i++) {
      PyObject *item_src = PyList_GET_ITEM(ls, i);
      const char *item_idname = PyUnicode_AsUTF8(item_src);

      // printf("  %s\n", item_idname);

      /* NOTE: index of item in py list is stored in userdata pointer, so that it can be found
       * later on to replace the ID name by the actual ID pointer. */
      if (item_idname != nullptr) {
        BlendfileLinkAppendContextItem *item = BKE_blendfile_link_append_context_item_add(
            lapp_context, item_idname, idcode, POINTER_FROM_INT(i));
        BKE_blendfile_link_append_context_item_library_index_enable(lapp_context, item, 0);
      }
      else {
        /* XXX, could complain about this */
        bpy_lib_exit_warn_type(self, item_src);
        PyErr_Clear();

#ifdef USE_RNA_DATABLOCKS
        /* We can replace the item immediately with `None`. */
        PyObject *py_item = Py_NewRef(Py_None);
        PyList_SET_ITEM(ls, i, py_item);
        Py_DECREF(item_src);
#endif
      }
    }
  }

  BKE_blendfile_link_append_context_init_done(lapp_context);

  BKE_blendfile_link(lapp_context, nullptr);
  if (do_pack) {
    BKE_blendfile_link_pack(lapp_context, nullptr);
  }
  else if (do_append) {
    BKE_blendfile_append(lapp_context, nullptr);
  }
  else if (create_liboverrides) {
    BKE_blendfile_override(lapp_context, self->liboverride_flags, nullptr);
  }

  BKE_blendfile_link_append_context_finalize(lapp_context);

/* If enabled, replace named items in given lists by the final matching new ID pointer. */
#ifdef USE_RNA_DATABLOCKS
  idcode_step = 0;
  while ((idcode = BKE_idtype_idcode_iter_step(&idcode_step))) {
    if (!BKE_idtype_idcode_is_linkable(idcode) || (idcode == ID_WS && !do_append)) {
      continue;
    }
    const char *name_plural = BKE_idtype_idcode_to_name_plural(idcode);
    PyObject *ls = PyDict_GetItemString(self->dict, name_plural);
    // printf("lib: %s\n", name_plural);
    if (ls == nullptr || !PyList_Check(ls)) {
      continue;
    }

    const Py_ssize_t size = PyList_GET_SIZE(ls);
    if (size == 0) {
      continue;
    }

    /* Loop over linked items in `lapp_context` to find matching python one in the list, and
     * replace them with proper ID pointer. */
    LibExitLappContextItemsIterData iter_data{};
    iter_data.idcode = idcode;
    iter_data.py_library = self;
    iter_data.py_list = ls;
    iter_data.py_list_size = size;
    BKE_blendfile_link_append_context_item_foreach(
        lapp_context,
        [&iter_data](BlendfileLinkAppendContext *lapp_context,
                     BlendfileLinkAppendContextItem *item) -> bool {
          return bpy_lib_exit_lapp_context_items_cb(lapp_context, item, iter_data);
        },
        BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_DIRECT);
  }
#endif  // USE_RNA_DATABLOCKS

  BKE_blendfile_link_append_context_free(lapp_context);
  BKE_main_id_tag_all(bmain, ID_TAG_PRE_EXISTING, false);

  BKE_reports_free(&self->reports);

  Py_RETURN_NONE;
}

static PyObject *bpy_lib_dir(BPy_Library *self)
{
  return PyDict_Keys(self->dict);
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

PyMethodDef BPY_library_load_method_def = {
    "load",
    (PyCFunction)bpy_lib_load,
    METH_VARARGS | METH_KEYWORDS,
    bpy_lib_load_doc,
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

int BPY_library_load_type_ready()
{
  if (PyType_Ready(&bpy_lib_Type) < 0) {
    return -1;
  }

  return 0;
}
