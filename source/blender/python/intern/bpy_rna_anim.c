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
 * This file defines the animation related methods used in bpy_rna.c
 */

#include <Python.h>
#include <float.h> /* FLT_MAX */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"

#include "DNA_scene_types.h"
#include "DNA_anim_types.h"

#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_idcode.h"
#include "BKE_library.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "bpy_rna.h"
#include "bpy_capi_utils.h"
#include "bpy_rna_anim.h"

#include "../generic/python_utildefines.h"

#include "DEG_depsgraph_build.h"

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

  if (ptr->data == NULL) {
    PyErr_Format(
        PyExc_TypeError, "%.200s this struct has no data, can't be animated", error_prefix);
    return -1;
  }

  /* full paths can only be given from ID base */
  if (is_idbase) {
    int path_index = -1;
    if (RNA_path_resolve_property_full(ptr, path, &r_ptr, &prop, &path_index) == false) {
      prop = NULL;
    }
    else if (path_index != -1) {
      PyErr_Format(PyExc_ValueError,
                   "%.200s path includes index, must be a separate argument",
                   error_prefix,
                   path);
      return -1;
    }
    else if (ptr->id.data != r_ptr.id.data) {
      PyErr_Format(PyExc_ValueError, "%.200s path spans ID blocks", error_prefix, path);
      return -1;
    }
  }
  else {
    prop = RNA_struct_find_property(ptr, path);
    r_ptr = *ptr;
  }

  if (prop == NULL) {
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
      int array_len = RNA_property_array_length(&r_ptr, prop);
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
    *r_path_full = RNA_path_from_ID_to_property(&r_ptr, prop);

    if (*r_path_full == NULL) {
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
  return pyrna_struct_anim_args_parse_ex(ptr, error_prefix, path, r_path_full, r_index, NULL);
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
  else {
    char *path_prefix = RNA_path_from_ID_to_struct(ptr);
    if (path_prefix == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%.200s could not make path for type %s",
                   error_prefix,
                   RNA_struct_identifier(ptr->type));
      return -1;
    }

    if (*path == '[') {
      *r_path_full = BLI_string_joinN(path_prefix, path);
    }
    else {
      *r_path_full = BLI_string_join_by_sep_charN('.', path_prefix, path);
    }
    MEM_freeN(path_prefix);
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
          ptr, error_prefix, path, r_path_full, r_index, &path_unresolved) == -1) {
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
                                       int *r_options)
{
  static const char *kwlist[] = {"data_path", "index", "frame", "group", "options", NULL};
  PyObject *pyoptions = NULL;
  const char *path;

  /* note, parse_str MUST start with 's|ifsO!' */
  if (!PyArg_ParseTupleAndKeywords(args,
                                   kw,
                                   parse_str,
                                   (char **)kwlist,
                                   &path,
                                   r_index,
                                   r_cfra,
                                   r_group_name,
                                   &PySet_Type,
                                   &pyoptions)) {
    return -1;
  }

  if (pyrna_struct_anim_args_parse(ptr, error_prefix, path, r_path_full, r_index) == -1) {
    return -1;
  }

  if (*r_cfra == FLT_MAX) {
    *r_cfra = CTX_data_scene(BPy_GetContext())->r.cfra;
  }

  /* flag may be null (no option currently for remove keyframes e.g.). */
  if (r_options) {
    if (pyoptions &&
        (pyrna_set_to_enum_bitfield(
             rna_enum_keying_flag_items_api, pyoptions, r_options, error_prefix) == -1)) {
      return -1;
    }

    *r_options |= INSERTKEY_NO_USERPREF;
  }

  return 0; /* success */
}

char pyrna_struct_keyframe_insert_doc[] =
    ".. method:: keyframe_insert(data_path, index=-1, frame=bpy.context.scene.frame_current, "
    "group=\"\")\n"
    "\n"
    "   Insert a keyframe on the property given, adding fcurves and animation data when "
    "necessary.\n"
    "\n"
    "   :arg data_path: path to the property to key, analogous to the fcurve's data path.\n"
    "   :type data_path: string\n"
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
    "   :arg options: Optional flags:\n"
    "\n"
    "      - ``INSERTKEY_NEEDED`` Only insert keyframes where they're needed in the relevant "
    "F-Curves.\n"
    "      - ``INSERTKEY_VISUAL`` Insert keyframes based on 'visual transforms'.\n"
    "      - ``INSERTKEY_XYZ_TO_RGB`` Color for newly added transformation F-Curves (Location, "
    "Rotation, Scale) is based on the transform axis.\n"
    "      - ``INSERTKEY_REPLACE`` Only replace already exising keyframes.\n"
    "      - ``INSERTKEY_AVAILABLE`` Only insert into already existing F-Curves.\n"
    "      - ``INSERTKEY_CYCLE_AWARE`` Take cyclic extrapolation into account "
    "(Cycle-Aware Keying option).\n"
    "   :type flag: set\n"
    "   :return: Success of keyframe insertion.\n"
    "   :rtype: boolean\n";
PyObject *pyrna_struct_keyframe_insert(BPy_StructRNA *self, PyObject *args, PyObject *kw)
{
  /* args, pyrna_struct_keyframe_parse handles these */
  const char *path_full = NULL;
  int index = -1;
  float cfra = FLT_MAX;
  const char *group_name = NULL;
  char keytype = BEZT_KEYTYPE_KEYFRAME; /* XXX: Expose this as a one-off option... */
  int options = 0;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (pyrna_struct_keyframe_parse(&self->ptr,
                                  args,
                                  kw,
                                  "s|ifsO!:bpy_struct.keyframe_insert()",
                                  "bpy_struct.keyframe_insert()",
                                  &path_full,
                                  &index,
                                  &cfra,
                                  &group_name,
                                  &options) == -1) {
    return NULL;
  }
  else if (self->ptr.type == &RNA_NlaStrip) {
    /* Handle special properties for NLA Strips, whose F-Curves are stored on the
     * strips themselves. These are stored separately or else the properties will
     * not have any effect.
     */
    ReportList reports;
    short result = 0;

    PointerRNA ptr = self->ptr;
    PropertyRNA *prop = NULL;
    const char *prop_name;

    BKE_reports_init(&reports, RPT_STORE);

    /* Retrieve the property identifier from the full path, since we can't get it any other way */
    prop_name = strrchr(path_full, '.');
    if ((prop_name >= path_full) && (prop_name + 1 < path_full + strlen(path_full))) {
      prop = RNA_struct_find_property(&ptr, prop_name + 1);
    }

    if (prop) {
      NlaStrip *strip = (NlaStrip *)ptr.data;
      FCurve *fcu = list_find_fcurve(&strip->fcurves, RNA_property_identifier(prop), index);

      result = insert_keyframe_direct(&reports, ptr, prop, fcu, cfra, keytype, NULL, options);
    }
    else {
      BKE_reportf(&reports, RPT_ERROR, "Could not resolve path (%s)", path_full);
    }
    MEM_freeN((void *)path_full);

    if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
      return NULL;
    }

    return PyBool_FromLong(result);
  }
  else {
    ID *id = self->ptr.id.data;
    ReportList reports;
    short result;

    BKE_reports_init(&reports, RPT_STORE);

    BLI_assert(BKE_id_is_in_global_main(id));
    result = insert_keyframe(
        G_MAIN, &reports, id, NULL, group_name, path_full, index, cfra, keytype, NULL, options);
    MEM_freeN((void *)path_full);

    if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
      return NULL;
    }

    return PyBool_FromLong(result);
  }
}

char pyrna_struct_keyframe_delete_doc[] =
    ".. method:: keyframe_delete(data_path, index=-1, frame=bpy.context.scene.frame_current, "
    "group=\"\")\n"
    "\n"
    "   Remove a keyframe from this properties fcurve.\n"
    "\n"
    "   :arg data_path: path to the property to remove a key, analogous to the fcurve's data "
    "path.\n"
    "   :type data_path: string\n"
    "   :arg index: array index of the property to remove a key. Defaults to -1 removing all "
    "indices or a single channel if the property is not an array.\n"
    "   :type index: int\n"
    "   :arg frame: The frame on which the keyframe is deleted, defaulting to the current frame.\n"
    "   :type frame: float\n"
    "   :arg group: The name of the group the F-Curve should be added to if it doesn't exist "
    "yet.\n"
    "   :type group: str\n"
    "   :return: Success of keyframe deleation.\n"
    "   :rtype: boolean\n";
PyObject *pyrna_struct_keyframe_delete(BPy_StructRNA *self, PyObject *args, PyObject *kw)
{
  /* args, pyrna_struct_keyframe_parse handles these */
  const char *path_full = NULL;
  int index = -1;
  float cfra = FLT_MAX;
  const char *group_name = NULL;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (pyrna_struct_keyframe_parse(&self->ptr,
                                  args,
                                  kw,
                                  "s|ifsO!:bpy_struct.keyframe_delete()",
                                  "bpy_struct.keyframe_insert()",
                                  &path_full,
                                  &index,
                                  &cfra,
                                  &group_name,
                                  NULL) == -1) {
    return NULL;
  }
  else if (self->ptr.type == &RNA_NlaStrip) {
    /* Handle special properties for NLA Strips, whose F-Curves are stored on the
     * strips themselves. These are stored separately or else the properties will
     * not have any effect.
     */
    ReportList reports;
    short result = 0;

    PointerRNA ptr = self->ptr;
    PropertyRNA *prop = NULL;
    const char *prop_name;

    BKE_reports_init(&reports, RPT_STORE);

    /* Retrieve the property identifier from the full path, since we can't get it any other way */
    prop_name = strrchr(path_full, '.');
    if ((prop_name >= path_full) && (prop_name + 1 < path_full + strlen(path_full))) {
      prop = RNA_struct_find_property(&ptr, prop_name + 1);
    }

    if (prop) {
      ID *id = ptr.id.data;
      NlaStrip *strip = (NlaStrip *)ptr.data;
      FCurve *fcu = list_find_fcurve(&strip->fcurves, RNA_property_identifier(prop), index);

      BLI_assert(fcu !=
                 NULL); /* NOTE: This should be true, or else we wouldn't be able to get here */

      if (BKE_fcurve_is_protected(fcu)) {
        BKE_reportf(
            &reports,
            RPT_WARNING,
            "Not deleting keyframe for locked F-Curve for NLA Strip influence on %s - %s '%s'",
            strip->name,
            BKE_idcode_to_name(GS(id->name)),
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
        i = binarysearch_bezt_index(fcu->bezt, cfra, fcu->totvert, &found);
        if (found) {
          /* delete the key at the index (will sanity check + do recalc afterwards) */
          delete_fcurve_key(fcu, i, 1);
          result = true;
        }
      }
    }
    else {
      BKE_reportf(&reports, RPT_ERROR, "Could not resolve path (%s)", path_full);
    }
    MEM_freeN((void *)path_full);

    if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
      return NULL;
    }

    return PyBool_FromLong(result);
  }
  else {
    short result;
    ReportList reports;

    BKE_reports_init(&reports, RPT_STORE);

    result = delete_keyframe(
        G.main, &reports, (ID *)self->ptr.id.data, NULL, group_name, path_full, index, cfra, 0);
    MEM_freeN((void *)path_full);

    if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
      return NULL;
    }

    return PyBool_FromLong(result);
  }
}

char pyrna_struct_driver_add_doc[] =
    ".. method:: driver_add(path, index=-1)\n"
    "\n"
    "   Adds driver(s) to the given property\n"
    "\n"
    "   :arg path: path to the property to drive, analogous to the fcurve's data path.\n"
    "   :type path: string\n"
    "   :arg index: array index of the property drive. Defaults to -1 for all indices or a single "
    "channel if the property is not an array.\n"
    "   :type index: int\n"
    "   :return: The driver(s) added.\n"
    "   :rtype: :class:`bpy.types.FCurve` or list if index is -1 with an array property.\n";
PyObject *pyrna_struct_driver_add(BPy_StructRNA *self, PyObject *args)
{
  const char *path, *path_full;
  int index = -1;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|i:driver_add", &path, &index)) {
    return NULL;
  }

  if (pyrna_struct_anim_args_parse(
          &self->ptr, "bpy_struct.driver_add():", path, &path_full, &index) == -1) {
    return NULL;
  }
  else {
    PyObject *ret = NULL;
    ReportList reports;
    int result;

    BKE_reports_init(&reports, RPT_STORE);

    result = ANIM_add_driver(&reports,
                             (ID *)self->ptr.id.data,
                             path_full,
                             index,
                             CREATEDRIVER_WITH_FMODIFIER,
                             DRIVER_TYPE_PYTHON);

    if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
      return NULL;
    }

    if (result) {
      ID *id = self->ptr.id.data;
      AnimData *adt = BKE_animdata_from_id(id);
      FCurve *fcu;

      PointerRNA tptr;

      if (index == -1) { /* all, use a list */
        int i = 0;
        ret = PyList_New(0);
        while ((fcu = list_find_fcurve(&adt->drivers, path_full, i++))) {
          RNA_pointer_create(id, &RNA_FCurve, fcu, &tptr);
          PyList_APPEND(ret, pyrna_struct_CreatePyObject(&tptr));
        }
      }
      else {
        fcu = list_find_fcurve(&adt->drivers, path_full, index);
        RNA_pointer_create(id, &RNA_FCurve, fcu, &tptr);
        ret = pyrna_struct_CreatePyObject(&tptr);
      }

      bContext *context = BPy_GetContext();
      WM_event_add_notifier(BPy_GetContext(), NC_ANIMATION | ND_FCURVES_ORDER, NULL);
      DEG_relations_tag_update(CTX_data_main(context));
    }
    else {
      /* XXX, should be handled by reports, */
      PyErr_SetString(PyExc_TypeError,
                      "bpy_struct.driver_add(): failed because of an internal error");
      return NULL;
    }

    MEM_freeN((void *)path_full);

    return ret;
  }
}

char pyrna_struct_driver_remove_doc[] =
    ".. method:: driver_remove(path, index=-1)\n"
    "\n"
    "   Remove driver(s) from the given property\n"
    "\n"
    "   :arg path: path to the property to drive, analogous to the fcurve's data path.\n"
    "   :type path: string\n"
    "   :arg index: array index of the property drive. Defaults to -1 for all indices or a single "
    "channel if the property is not an array.\n"
    "   :type index: int\n"
    "   :return: Success of driver removal.\n"
    "   :rtype: boolean\n";
PyObject *pyrna_struct_driver_remove(BPy_StructRNA *self, PyObject *args)
{
  const char *path, *path_full;
  int index = -1;

  PYRNA_STRUCT_CHECK_OBJ(self);

  if (!PyArg_ParseTuple(args, "s|i:driver_remove", &path, &index)) {
    return NULL;
  }

  if (pyrna_struct_anim_args_parse_no_resolve_fallback(
          &self->ptr, "bpy_struct.driver_remove():", path, &path_full, &index) == -1) {
    return NULL;
  }
  else {
    short result;
    ReportList reports;

    BKE_reports_init(&reports, RPT_STORE);

    result = ANIM_remove_driver(&reports, (ID *)self->ptr.id.data, path_full, index, 0);

    if (path != path_full) {
      MEM_freeN((void *)path_full);
    }

    if (BPy_reports_to_error(&reports, PyExc_RuntimeError, true) == -1) {
      return NULL;
    }

    bContext *context = BPy_GetContext();
    WM_event_add_notifier(context, NC_ANIMATION | ND_FCURVES_ORDER, NULL);
    DEG_relations_tag_update(CTX_data_main(context));

    return PyBool_FromLong(result);
  }
}
