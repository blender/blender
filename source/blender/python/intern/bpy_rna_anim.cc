/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines the animation related methods used in `bpy_rna.cc`.
 */

#include <Python.h>
#include <cfloat> /* FLT_MAX */

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_string_utils.hh"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "ED_keyframing.hh"

#include "ANIM_keyframing.hh"

#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "bpy_capi_utils.hh"
#include "bpy_rna.hh"
#include "bpy_rna_anim.hh"

#include "../generic/py_capi_rna.hh"
#include "../generic/python_utildefines.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "CLG_log.h"

/* for keyframes and drivers */
static int pyrna_struct_anim_args_parse_ex(PointerRNA *ptr,
                                           const char *error_prefix,
                                           const char *path,
                                           const char **r_path_full,
                                           int *r_index,
                                           bool *r_path_no_validate)
{
  const bool is_idbase = RNA_struct_is_ID(ptr->type);
  PropertyRNA *prop;
  PointerRNA r_ptr;

  if (ptr->data == nullptr) {
    PyErr_Format(
        PyExc_TypeError, "%.200s this struct has no data, cannot be animated", error_prefix);
    return -1;
  }

  /* full paths can only be given from ID base */
  if (is_idbase) {
    int path_index = -1;
    if (RNA_path_resolve_property_full(ptr, path, &r_ptr, &prop, &path_index) == false) {
      prop = nullptr;
    }
    else if (path_index != -1) {
      PyErr_Format(PyExc_ValueError,
                   "%.200s path includes index, must be a separate argument",
                   error_prefix,
                   path);
      return -1;
    }
    else if (ptr->owner_id != r_ptr.owner_id) {
      PyErr_Format(PyExc_ValueError, "%.200s path spans ID blocks", error_prefix, path);
      return -1;
    }
  }
  else {
    prop = RNA_struct_find_property(ptr, path);
    r_ptr = *ptr;
  }

  if (prop == nullptr) {
    if (r_path_no_validate) {
      *r_path_no_validate = true;
      return -1;
    }
    PyErr_Format(PyExc_TypeError, "%.200s property \"%s\" not found", error_prefix, path);
    return -1;
  }

  if (r_path_no_validate) {
    /* Don't touch the index. */
  }
  else {
    if (!RNA_property_animateable(&r_ptr, prop)) {
      PyErr_Format(PyExc_TypeError, "%.200s property \"%s\" not animatable", error_prefix, path);
      return -1;
    }

    if (RNA_property_array_check(prop) == 0) {
      if ((*r_index) == -1) {
        *r_index = 0;
      }
      else {
        PyErr_Format(PyExc_TypeError,
                     "%.200s index %d was given while property \"%s\" is not an array",
                     error_prefix,
                     *r_index,
                     path);
        return -1;
      }
    }
    else {
      const int array_len = RNA_property_array_length(&r_ptr, prop);
      if ((*r_index) < -1 || (*r_index) >= array_len) {
        PyErr_Format(PyExc_TypeError,
                     "%.200s index out of range \"%s\", given %d, array length is %d",
                     error_prefix,
                     path,
                     *r_index,
                     array_len);
        return -1;
      }
    }
  }

  if (is_idbase) {
    *r_path_full = BLI_strdup(path);
  }
  else {
    const std::optional<std::string> path_full = RNA_path_from_ID_to_property(&r_ptr, prop);
    *r_path_full = path_full ? BLI_strdup(path_full->c_str()) : nullptr;

    if (*r_path_full == nullptr) {
      PyErr_Format(PyExc_TypeError, "%.200s could not make path to \"%s\"", error_prefix, path);
      return -1;
    }
  }

  return 0;
}

static int pyrna_struct_anim_args_parse(PointerRNA *ptr,
                                        const char *error_prefix,
                                        const char *path,
                                        const char **r_path_full,
                                        int *r_index)
{
  return pyrna_struct_anim_args_parse_ex(ptr, error_prefix, path, r_path_full, r_index, nullptr);
}

/**
 * Unlike #pyrna_struct_anim_args_parse \a r_path_full may be copied from \a path.
 */
static int pyrna_struct_anim_args_parse_no_resolve(PointerRNA *ptr,
                                                   const char *error_prefix,
                                                   const char *path,
                                                   const char **r_path_full)
{
  const bool is_idbase = RNA_struct_is_ID(ptr->type);
  if (is_idbase) {
    *r_path_full = path;
    return 0;
  }

  const std::optional<std::string> path_prefix = RNA_path_from_ID_to_struct(ptr);
  if (!path_prefix) {
    PyErr_Format(PyExc_TypeError,
                 "%.200s could not make path for type %s",
                 error_prefix,
                 RNA_struct_identifier(ptr->type));
    return -1;
  }

  if (*path == '[') {
    *r_path_full = BLI_string_joinN(path_prefix->c_str(), path);
  }
  else {
    *r_path_full = BLI_string_join_by_sep_charN('.', path_prefix->c_str(), path);
  }

  return 0;
}

static int pyrna_struct_anim_args_parse_no_resolve_fallback(PointerRNA *ptr,
                                                            const char *error_prefix,
                                                            const char *path,
                                                            const char **r_path_full,
                                                            int *r_index)
{
  bool path_unresolved = false;
  if (pyrna_struct_anim_args_parse_ex(
          ptr, error_prefix, path, r_path_full, r_index, &path_unresolved) == -1)
  {
    if (path_unresolved == true) {
      if (pyrna_struct_anim_args_parse_no_resolve(ptr, error_prefix, path, r_path_full) == -1) {
        return -1;
      }
    }
    else {
      return -1;
    }
  }
  return 0;
}

/* internal use for insert and delete */
static int pyrna_struct_keyframe_parse(PointerRNA *ptr,
                                       PyObject *args,
                                       PyObject *kw,
                                       const char *parse_str,
                                       const char *error_prefix,
                                       /* return values */
                                       const char **r_path_full,
                                       int *r_index,
                                       float *r_cfra,
                                       const char **r_group_name,
                                       int *r_options,
                                       eBezTriple_KeyframeType *r_keytype)
{
  static const char *kwlist[] = {
      "data_path", "index", "frame", "group", "options", "keytype", nullptr};
  PyObject *pyoptions = nullptr;
  char *keytype_name = nullptr;
  const char *path;

  /* NOTE: `parse_str` MUST start with `s|ifsO!`. */
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   parse_str,
                                   (char **)kwlist,
                                   &path,
                                   r_index,
                                   r_cfra,
                                   r_group_name,
                                   &PySet_Type,
                                   &pyoptions,
                                   &keytype_name))
  {
    return -1;
  }

  /* flag may be null (no option currently for remove keyframes e.g.). */
  if (r_options) {
    if (pyoptions &&
        (pyrna_enum_bitfield_from_set(
             rna_enum_keying_flag_api_items, pyoptions, r_options, error_prefix) == -1))
    {
      return -1;
    }

    *r_options |= INSERTKEY_NO_USERPREF;
  }

  if (r_keytype) {
    int keytype_as_int = 0;
    if (keytype_name && pyrna_enum_value_from_id(rna_enum_beztriple_keyframe_type_items,
                                                 keytype_name,
                                                 &keytype_as_int,
                                                 error_prefix) == -1)
    {
      return -1;
    }
    *r_keytype = eBezTriple_KeyframeType(keytype_as_int);
  }

  if (pyrna_struct_anim_args_parse(ptr, error_prefix, path, r_path_full, r_index) == -1) {
    return -1;
  }

  if (*r_cfra == FLT_MAX) {
    *r_cfra = CTX_data_scene(BPY_context_get())->r.cfra;
  }

  return 0; /* success */
}

char pyrna_struct_keyframe_insert_doc[] =
    ".. method:: keyframe_insert(data_path, /, *, index=-1, "
    "frame=bpy.context.scene.frame_current, "
    "group=\"\", options=set(), keytype='KEYFRAME')\n"
    "\n"
    "   Insert a keyframe on the property given, adding fcurves and animation data when "
    "necessary.\n"
    "\n"
    "   :arg data_path: path to the property to key, analogous to the fcurve's data path.\n"
    "   :type data_path: str\n"
    "   :arg index: array index of the property to key.\n"
    "      Defaults to -1 which will key all indices or a single channel if the property is not "
    "an array.\n"
    "   :type index: int\n"
    "   :arg frame: The frame on which the keyframe is inserted, defaulting to the current "
    "frame.\n"
    "   :type frame: float\n"
    "   :arg group: The name of the group the F-Curve should be added to if it doesn't exist "
    "yet.\n"
    "   :type group: str\n"
    "   :arg options: Optional set of flags:\n"
    "\n"
    "      - ``INSERTKEY_NEEDED`` Only insert keyframes where they're needed in the relevant "
    "F-Curves.\n"
    "      - ``INSERTKEY_VISUAL`` Insert keyframes based on 'visual transforms'.\n"
    "      - ``INSERTKEY_REPLACE`` Only replace already existing keyframes.\n"
    "      - ``INSERTKEY_AVAILABLE`` Only insert into already existing F-Curves.\n"
    "      - ``INSERTKEY_CYCLE_AWARE`` Take cyclic extrapolation into account "
    "(Cycle-Aware Keying option).\n"
    "   :type options: set[str]\n"
    "   :arg keytype: Type of the key: 'KEYFRAME', 'BREAKDOWN', 'MOVING_HOLD', 'EXTREME', "
    "'JITTER', or 'GENERATED'\n"
    "   :type keytype: str\n"
    "   :return: Success of keyframe insertion.\n"
    "   :rtype: bool\n";
PyObject *pyrna_struct_keyframe_insert(BPy_StructRNA *self, PyObject *args, PyObject *kw)
{
  using namespace blender::animrig;
  /* args, pyrna_struct_keyframe_parse handles these */
  const char *path_full = nullptr;
  int index = -1;
  float cfra = FLT_MAX;
  const char *group_name = nullptr;
  eBezTriple_KeyframeType keytype = BEZT_KEYTYPE_KEYFRAME;
  int options = 0;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (pyrna_struct_keyframe_parse(&self->ptr.value(),
                                  args,
                                  kw,
                                  "s|$ifsO!s:bpy_struct.keyframe_insert()",
                                  "bpy_struct.keyframe_insert()",
                                  &path_full,
                                  &index,
                                  &cfra,
                                  &group_name,
                                  &options,
                                  &keytype) == -1)
  {
    return nullptr;
  }

  ReportList reports;
  bool result = false;

  BKE_reports_init(&reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);

  /* This assumes that keyframes are only added on original data & using the active depsgraph. If
   * it turns out to be necessary for some reason to insert keyframes on evaluated objects, we can
   * revisit this and add an explicit `depsgraph` keyword argument to the function call.
   *
   * The depsgraph is only used for evaluating the NLA so this might not be needed in the future.
   */
  bContext *C = BPY_context_get();
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    cfra);

  if (self->ptr->type == &RNA_NlaStrip) {
    /* Handle special properties for NLA Strips, whose F-Curves are stored on the
     * strips themselves. These are stored separately or else the properties will
     * not have any effect.
     */

    PointerRNA &ptr = *self->ptr;
    PropertyRNA *prop = nullptr;
    const char *prop_name;

    /* Retrieve the property identifier from the full path, since we can't get it any other way */
    prop_name = strrchr(path_full, '.');
    if ((prop_name >= path_full) && (prop_name + 1 < path_full + strlen(path_full))) {
      prop = RNA_struct_find_property(&ptr, prop_name + 1);
    }

    if (prop) {
      NlaStrip *strip = static_cast<NlaStrip *>(ptr.data);
      FCurve *fcu = BKE_fcurve_find(&strip->fcurves, RNA_property_identifier(prop), index);
      result = insert_keyframe_direct(&reports,
                                      ptr,
                                      prop,
                                      fcu,
                                      &anim_eval_context,
                                      eBezTriple_KeyframeType(keytype),
                                      nullptr,
                                      eInsertKeyFlags(options));
    }
    else {
      BKE_reportf(&reports, RPT_ERROR, "Could not resolve path (%s)", path_full);
    }
  }
  else {
    BLI_assert(BKE_id_is_in_global_main(self->ptr->owner_id));

    const std::optional<blender::StringRefNull> channel_group = group_name ?
                                                                    std::optional(group_name) :
                                                                    std::nullopt;
    PointerRNA id_pointer = RNA_id_pointer_create(self->ptr->owner_id);
    CombinedKeyingResult combined_result = insert_keyframes(G_MAIN,
                                                            &id_pointer,
                                                            channel_group,
                                                            {{path_full, {}, index}},
                                                            std::nullopt,
                                                            anim_eval_context,
                                                            eBezTriple_KeyframeType(keytype),
                                                            eInsertKeyFlags(options));
    const int success_count = combined_result.get_count(SingleKeyingResult::SUCCESS);
    if (success_count == 0) {
      /* Ideally this would use the GUI presentation of RPT_ERROR, as the resulting pop-up has more
       * vertical space than the single-line warning in the status bar. However, semantically these
       * may not be errors at all, as skipping the keying of certain properties due to the 'only
       * insert available' flag is not an error.
       *
       * Furthermore, using RPT_ERROR here would cause this function to raise a Python exception,
       * rather than returning a boolean. */
      combined_result.generate_reports(&reports, RPT_WARNING);
    }
    result = success_count != 0;
  }

  MEM_freeN(path_full);

  if (BPy_reports_to_error(&reports, PyExc_RuntimeError, false) == -1) {
    BKE_reports_free(&reports);
    return nullptr;
  }
  BKE_report_print_level_set(&reports, CLG_quiet_get() ? RPT_WARNING : RPT_DEBUG);
  BPy_reports_write_stdout(&reports, nullptr);
  BKE_reports_free(&reports);

  if (result) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);
  }

  return PyBool_FromLong(result);
}

char pyrna_struct_keyframe_delete_doc[] =
    ".. method:: keyframe_delete(data_path, /, *, index=-1, "
    "frame=bpy.context.scene.frame_current, "
    "group=\"\")\n"
    "\n"
    "   Remove a keyframe from this properties fcurve.\n"
    "\n"
    "   :arg data_path: path to the property to remove a key, analogous to the fcurve's data "
    "path.\n"
    "   :type data_path: str\n"
    "   :arg index: array index of the property to remove a key. Defaults to -1 removing all "
    "indices or a single channel if the property is not an array.\n"
    "   :type index: int\n"
    "   :arg frame: The frame on which the keyframe is deleted, defaulting to the current frame.\n"
    "   :type frame: float\n"
    "   :arg group: The name of the group the F-Curve should be added to if it doesn't exist "
    "yet.\n"
    "   :type group: str\n"
    "   :return: Success of keyframe deletion.\n"
    "   :rtype: bool\n";
PyObject *pyrna_struct_keyframe_delete(BPy_StructRNA *self, PyObject *args, PyObject *kw)
{
  /* args, pyrna_struct_keyframe_parse handles these */
  const char *path_full = nullptr;
  int index = -1;
  float cfra = FLT_MAX;
  const char *group_name = nullptr;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (pyrna_struct_keyframe_parse(&self->ptr.value(),
                                  args,
                                  kw,
                                  "s|$ifsOs!:bpy_struct.keyframe_delete()",
                                  "bpy_struct.keyframe_insert()",
                                  &path_full,
                                  &index,
                                  &cfra,
                                  &group_name,
                                  nullptr,
                                  nullptr) == -1)
  {
    return nullptr;
  }

  ReportList reports;
  bool result = false;

  BKE_reports_init(&reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);

  if (self->ptr->type == &RNA_NlaStrip) {
    /* Handle special properties for NLA Strips, whose F-Curves are stored on the
     * strips themselves. These are stored separately or else the properties will
     * not have any effect.
     */

    PointerRNA ptr = *self->ptr;
    PropertyRNA *prop = nullptr;
    const char *prop_name;

    /* Retrieve the property identifier from the full path, since we can't get it any other way */
    prop_name = strrchr(path_full, '.');
    if ((prop_name >= path_full) && (prop_name + 1 < path_full + strlen(path_full))) {
      prop = RNA_struct_find_property(&ptr, prop_name + 1);
    }

    if (prop) {
      ID *id = ptr.owner_id;
      NlaStrip *strip = static_cast<NlaStrip *>(ptr.data);
      FCurve *fcu = BKE_fcurve_find(&strip->fcurves, RNA_property_identifier(prop), index);

      /* NOTE: This should be true, or else we wouldn't be able to get here. */
      BLI_assert(fcu != nullptr);

      if (BKE_fcurve_is_protected(fcu)) {
        BKE_reportf(
            &reports,
            RPT_WARNING,
            "Not deleting keyframe for locked F-Curve for NLA Strip influence on %s - %s '%s'",
            strip->name,
            BKE_idtype_idcode_to_name(GS(id->name)),
            id->name + 2);
      }
      else {
        /* remove the keyframe directly
         * NOTE: cannot use delete_keyframe_fcurve(), as that will free the curve,
         *       and delete_keyframe() expects the FCurve to be part of an action
         */
        bool found = false;
        int i;

        /* try to find index of beztriple to get rid of */
        i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, cfra, fcu->totvert, &found);
        if (found) {
          /* delete the key at the index (will sanity check + do recalc afterwards) */
          BKE_fcurve_delete_key(fcu, i);
          BKE_fcurve_handles_recalc(fcu);
          result = true;
        }
      }
    }
    else {
      BKE_reportf(&reports, RPT_ERROR, "Could not resolve path (%s)", path_full);
    }
  }
  else {
    RNAPath rna_path = {path_full, std::nullopt, index};
    if (index < 0) {
      rna_path.index = std::nullopt;
    }
    result = (blender::animrig::delete_keyframe(
                  G.main, &reports, self->ptr->owner_id, rna_path, cfra) != 0);
  }

  MEM_freeN(path_full);

  if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
    return nullptr;
  }

  return PyBool_FromLong(result);
}

char pyrna_struct_driver_add_doc[] =
    ".. method:: driver_add(path, index=-1, /)\n"
    "\n"
    "   Adds driver(s) to the given property\n"
    "\n"
    "   :arg path: path to the property to drive, analogous to the fcurve's data path.\n"
    "   :type path: str\n"
    "   :arg index: array index of the property drive. Defaults to -1 for all indices or a single "
    "channel if the property is not an array.\n"
    "   :type index: int\n"
    "   :return: The driver added or a list of drivers when index is -1.\n"
    "   :rtype: :class:`bpy.types.FCurve` | list[:class:`bpy.types.FCurve`]\n";
PyObject *pyrna_struct_driver_add(BPy_StructRNA *self, PyObject *args)
{
  const char *path, *path_full;
  int index = -1;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|i:driver_add", &path, &index)) {
    return nullptr;
  }

  if (pyrna_struct_anim_args_parse(
          &self->ptr.value(), "bpy_struct.driver_add():", path, &path_full, &index) == -1)
  {
    return nullptr;
  }

  PyObject *ret = nullptr;
  ReportList reports;
  int result;

  BKE_reports_init(&reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);

  result = ANIM_add_driver(&reports, self->ptr->owner_id, path_full, index, 0, DRIVER_TYPE_PYTHON);

  if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
    /* Pass. */
  }
  else if (result == 0) {
    /* XXX: should be handled by reports. */
    PyErr_SetString(PyExc_TypeError,
                    "bpy_struct.driver_add(): failed because of an internal error");
  }
  else {
    ID *id = self->ptr->owner_id;
    AnimData *adt = BKE_animdata_from_id(id);
    FCurve *fcu;

    PointerRNA tptr;

    if (index == -1) { /* all, use a list */
      int i = 0;
      ret = PyList_New(0);
      while ((fcu = BKE_fcurve_find(&adt->drivers, path_full, i++))) {
        tptr = RNA_pointer_create_discrete(id, &RNA_FCurve, fcu);
        PyList_APPEND(ret, pyrna_struct_CreatePyObject(&tptr));
      }
    }
    else {
      fcu = BKE_fcurve_find(&adt->drivers, path_full, index);
      tptr = RNA_pointer_create_discrete(id, &RNA_FCurve, fcu);
      ret = pyrna_struct_CreatePyObject(&tptr);
    }

    bContext *context = BPY_context_get();
    WM_event_add_notifier(BPY_context_get(), NC_ANIMATION | ND_FCURVES_ORDER, nullptr);
    DEG_id_tag_update(id, ID_RECALC_SYNC_TO_EVAL);
    DEG_relations_tag_update(CTX_data_main(context));
  }

  MEM_freeN(path_full);

  return ret;
}

char pyrna_struct_driver_remove_doc[] =
    ".. method:: driver_remove(path, index=-1, /)\n"
    "\n"
    "   Remove driver(s) from the given property\n"
    "\n"
    "   :arg path: path to the property to drive, analogous to the fcurve's data path.\n"
    "   :type path: str\n"
    "   :arg index: array index of the property drive. Defaults to -1 for all indices or a single "
    "channel if the property is not an array.\n"
    "   :type index: int\n"
    "   :return: Success of driver removal.\n"
    "   :rtype: bool\n";
PyObject *pyrna_struct_driver_remove(BPy_StructRNA *self, PyObject *args)
{
  const char *path, *path_full;
  int index = -1;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|i:driver_remove", &path, &index)) {
    return nullptr;
  }

  if (pyrna_struct_anim_args_parse_no_resolve_fallback(
          &self->ptr.value(), "bpy_struct.driver_remove():", path, &path_full, &index) == -1)
  {
    return nullptr;
  }

  short result;
  ReportList reports;

  BKE_reports_init(&reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);

  result = ANIM_remove_driver(self->ptr->owner_id, path_full, index);

  if (path != path_full) {
    MEM_freeN(path_full);
  }

  if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
    return nullptr;
  }

  bContext *context = BPY_context_get();
  WM_event_add_notifier(context, NC_ANIMATION | ND_FCURVES_ORDER, nullptr);
  DEG_id_tag_update(self->ptr->owner_id, ID_RECALC_ANIMATION);
  DEG_relations_tag_update(CTX_data_main(context));

  return PyBool_FromLong(result);
}
