/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup wm
 *
 * Functions for dealing with wmOperator, adding, removing, calling
 * as well as some generic operators and shared operator properties.
 */

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#  include "GHOST_C-api.h"
#endif

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_ID.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_dynstr.h" /* For #WM_operator_pystring. */
#include "BLI_math.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h" /* BKE_ST_MAXNAME */
#include "BKE_unit.h"

#include "BKE_idtype.h"

#include "BLF_api.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "IMB_imbuf_types.h"

#include "ED_fileselect.h"
#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm.h"
#include "wm_draw.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_files.h"
#include "wm_window.h"
#ifdef WITH_XR_OPENXR
#  include "wm_xr.h"
#endif

#define UNDOCUMENTED_OPERATOR_TIP N_("(undocumented operator)")

/* -------------------------------------------------------------------- */
/** \name Operator API
 * \{ */

size_t WM_operator_py_idname(char *dst, const char *src)
{
  const char *sep = strstr(src, "_OT_");
  if (sep) {
    int ofs = (sep - src);

    /* NOTE: we use ascii `tolower` instead of system `tolower`, because the
     * latter depends on the locale, and can lead to `idname` mismatch. */
    memcpy(dst, src, sizeof(char) * ofs);
    BLI_str_tolower_ascii(dst, ofs);

    dst[ofs] = '.';
    return BLI_strncpy_rlen(dst + (ofs + 1), sep + 4, OP_MAX_TYPENAME - (ofs + 1)) + (ofs + 1);
  }
  /* Should not happen but support just in case. */
  return BLI_strncpy_rlen(dst, src, OP_MAX_TYPENAME);
}

size_t WM_operator_bl_idname(char *dst, const char *src)
{
  const char *sep = strchr(src, '.');
  int from_len;
  if (sep && (from_len = strlen(src)) < OP_MAX_TYPENAME - 3) {
    const int ofs = (sep - src);
    memcpy(dst, src, sizeof(char) * ofs);
    BLI_str_toupper_ascii(dst, ofs);
    memcpy(dst + ofs, "_OT_", 4);
    memcpy(dst + (ofs + 4), sep + 1, (from_len - ofs));
    return (from_len - ofs) - 1;
  }
  /* Should not happen but support just in case. */
  return BLI_strncpy_rlen(dst, src, OP_MAX_TYPENAME);
}

bool WM_operator_py_idname_ok_or_report(ReportList *reports,
                                        const char *classname,
                                        const char *idname)
{
  const char *ch = idname;
  int dot = 0;
  int i;
  for (i = 0; *ch; i++, ch++) {
    if ((*ch >= 'a' && *ch <= 'z') || (*ch >= '0' && *ch <= '9') || *ch == '_') {
      /* pass */
    }
    else if (*ch == '.') {
      dot++;
    }
    else {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Registering operator class: '%s', invalid bl_idname '%s', at position %d",
                  classname,
                  idname,
                  i);
      return false;
    }
  }

  if (i > (MAX_NAME - 3)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering operator class: '%s', invalid bl_idname '%s', "
                "is too long, maximum length is %d",
                classname,
                idname,
                MAX_NAME - 3);
    return false;
  }

  if (dot != 1) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Registering operator class: '%s', invalid bl_idname '%s', must contain 1 '.' character",
        classname,
        idname);
    return false;
  }
  return true;
}

char *WM_operator_pystring_ex(bContext *C,
                              wmOperator *op,
                              const bool all_args,
                              const bool macro_args,
                              wmOperatorType *ot,
                              PointerRNA *opptr)
{
  char idname_py[OP_MAX_TYPENAME];

  /* for building the string */
  DynStr *dynstr = BLI_dynstr_new();

  /* arbitrary, but can get huge string with stroke painting otherwise */
  int max_prop_length = 10;

  WM_operator_py_idname(idname_py, ot->idname);
  BLI_dynstr_appendf(dynstr, "bpy.ops.%s(", idname_py);

  if (op && op->macro.first) {
    /* Special handling for macros, else we only get default values in this case... */
    wmOperator *opm;
    bool first_op = true;

    opm = macro_args ? op->macro.first : NULL;

    for (; opm; opm = opm->next) {
      PointerRNA *opmptr = opm->ptr;
      PointerRNA opmptr_default;
      if (opmptr == NULL) {
        WM_operator_properties_create_ptr(&opmptr_default, opm->type);
        opmptr = &opmptr_default;
      }

      char *cstring_args = RNA_pointer_as_string_id(C, opmptr);
      if (first_op) {
        BLI_dynstr_appendf(dynstr, "%s=%s", opm->type->idname, cstring_args);
        first_op = false;
      }
      else {
        BLI_dynstr_appendf(dynstr, ", %s=%s", opm->type->idname, cstring_args);
      }
      MEM_freeN(cstring_args);

      if (opmptr == &opmptr_default) {
        WM_operator_properties_free(&opmptr_default);
      }
    }
  }
  else {
    /* only to get the original props for comparisons */
    PointerRNA opptr_default;
    const bool macro_args_test = ot->macro.first ? macro_args : true;

    if (opptr == NULL) {
      WM_operator_properties_create_ptr(&opptr_default, ot);
      opptr = &opptr_default;
    }

    char *cstring_args = RNA_pointer_as_string_keywords(
        C, opptr, false, all_args, macro_args_test, max_prop_length);
    BLI_dynstr_append(dynstr, cstring_args);
    MEM_freeN(cstring_args);

    if (opptr == &opptr_default) {
      WM_operator_properties_free(&opptr_default);
    }
  }

  BLI_dynstr_append(dynstr, ")");

  char *cstring = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);
  return cstring;
}

char *WM_operator_pystring(bContext *C, wmOperator *op, const bool all_args, const bool macro_args)
{
  return WM_operator_pystring_ex(C, op, all_args, macro_args, op->type, op->ptr);
}

bool WM_operator_pystring_abbreviate(char *str, int str_len_max)
{
  const int str_len = strlen(str);
  const char *parens_start = strchr(str, '(');

  if (parens_start) {
    const int parens_start_pos = parens_start - str;
    const char *parens_end = strrchr(parens_start + 1, ')');

    if (parens_end) {
      const int parens_len = parens_end - parens_start;

      if (parens_len > str_len_max) {
        const char *comma_first = strchr(parens_start, ',');

        /* Truncate after the first comma. */
        if (comma_first) {
          const char end_str[] = " ... )";
          const int end_str_len = sizeof(end_str) - 1;

          /* Leave a place for the first argument. */
          const int new_str_len = (comma_first - parens_start) + 1;

          if (str_len >= new_str_len + parens_start_pos + end_str_len + 1) {
            /* Append " ... )" to the string after the comma. */
            memcpy(str + new_str_len + parens_start_pos, end_str, end_str_len + 1);

            return true;
          }
        }
      }
    }
  }

  return false;
}

/* return NULL if no match is found */
#if 0
static const char *wm_context_member_from_ptr(bContext *C, const PointerRNA *ptr, bool *r_is_id)
{
  /* loop over all context items and do 2 checks
   *
   * - see if the pointer is in the context.
   * - see if the pointers ID is in the context.
   */

  /* Don't get from the context store since this is normally
   * set only for the UI and not usable elsewhere. */
  ListBase lb = CTX_data_dir_get_ex(C, false, true, true);
  LinkData *link;

  const char *member_found = NULL;
  const char *member_id = NULL;
  bool member_found_is_id = false;

  for (link = lb.first; link; link = link->next) {
    const char *identifier = link->data;
    PointerRNA ctx_item_ptr = {
        {0}};  /* CTX_data_pointer_get(C, identifier); */ /* XXX, this isn't working. */

    if (ctx_item_ptr.type == NULL) {
      continue;
    }

    if (ptr->owner_id == ctx_item_ptr.owner_id) {
      const bool is_id = RNA_struct_is_ID(ctx_item_ptr.type);
      if ((ptr->data == ctx_item_ptr.data) && (ptr->type == ctx_item_ptr.type)) {
        /* found! */
        member_found = identifier;
        member_found_is_id = is_id;
        break;
      }
      if (is_id) {
        /* Found a reference to this ID, so fallback to it if there is no direct reference. */
        member_id = identifier;
      }
    }
  }
  BLI_freelistN(&lb);

  if (member_found) {
    *r_is_id = member_found_is_id;
    return member_found;
  }
  else if (member_id) {
    *r_is_id = true;
    return member_id;
  }
  else {
    return NULL;
  }
}

#else

/* use hard coded checks for now */

/**
 * \param: r_is_id:
 * - When set to true, the returned member is an ID type.
 *   This is a signal that #RNA_path_from_ID_to_struct needs to be used to calculate
 *   the remainder of the RNA path.
 * - When set to false, the returned member is not an ID type.
 *   In this case the context path *must* resolve to `ptr`,
 *   since there is no convenient way to calculate partial RNA paths.
 *
 * \note While the path to the ID is typically sufficient to calculate the remainder of the path,
 * in practice this would cause #WM_context_path_resolve_property_full to create a path such as:
 * `object.data.bones["Bones"].use_deform` such paths are not useful for key-shortcuts,
 * so this function supports returning data-paths directly to context members that aren't ID types.
 */
static const char *wm_context_member_from_ptr(const bContext *C,
                                              const PointerRNA *ptr,
                                              bool *r_is_id)
{
  const char *member_id = NULL;
  bool is_id = false;

#  define CTX_TEST_PTR_ID(C, member, idptr) \
    { \
      const char *ctx_member = member; \
      PointerRNA ctx_item_ptr = CTX_data_pointer_get(C, ctx_member); \
      if (ctx_item_ptr.owner_id == idptr) { \
        member_id = ctx_member; \
        is_id = true; \
        break; \
      } \
    } \
    (void)0

#  define CTX_TEST_PTR_ID_CAST(C, member, member_full, cast, idptr) \
    { \
      const char *ctx_member = member; \
      const char *ctx_member_full = member_full; \
      PointerRNA ctx_item_ptr = CTX_data_pointer_get(C, ctx_member); \
      if (ctx_item_ptr.owner_id && (ID *)cast(ctx_item_ptr.owner_id) == idptr) { \
        member_id = ctx_member_full; \
        is_id = true; \
        break; \
      } \
    } \
    (void)0

#  define TEST_PTR_DATA_TYPE(member, rna_type, rna_ptr, dataptr_cmp) \
    { \
      const char *ctx_member = member; \
      if (RNA_struct_is_a((rna_ptr)->type, &(rna_type)) && (rna_ptr)->data == (dataptr_cmp)) { \
        member_id = ctx_member; \
        break; \
      } \
    } \
    (void)0

  /* A version of #TEST_PTR_DATA_TYPE that calls `CTX_data_pointer_get_type(C, member)`. */
#  define TEST_PTR_DATA_TYPE_FROM_CONTEXT(member, rna_type, rna_ptr) \
    { \
      const char *ctx_member = member; \
      if (RNA_struct_is_a((rna_ptr)->type, &(rna_type)) && \
          (rna_ptr)->data == (CTX_data_pointer_get_type(C, ctx_member, &(rna_type)).data)) { \
        member_id = ctx_member; \
        break; \
      } \
    } \
    (void)0

  /* General checks (multiple ID types). */
  if (ptr->owner_id) {
    const ID_Type ptr_id_type = GS(ptr->owner_id->name);

    /* Support break in the macros for an early exit. */
    do {
      /* Animation Data. */
      if (id_type_can_have_animdata(ptr_id_type)) {
        TEST_PTR_DATA_TYPE_FROM_CONTEXT("active_nla_track", RNA_NlaTrack, ptr);
        TEST_PTR_DATA_TYPE_FROM_CONTEXT("active_nla_strip", RNA_NlaStrip, ptr);
      }
    } while (0);
  }

  /* Specific ID type checks. */
  if (ptr->owner_id && (member_id == NULL)) {

    const ID_Type ptr_id_type = GS(ptr->owner_id->name);
    switch (ptr_id_type) {
      case ID_SCE: {
        TEST_PTR_DATA_TYPE_FROM_CONTEXT("active_sequence_strip", RNA_Sequence, ptr);

        CTX_TEST_PTR_ID(C, "scene", ptr->owner_id);
        break;
      }
      case ID_OB: {
        TEST_PTR_DATA_TYPE_FROM_CONTEXT("active_pose_bone", RNA_PoseBone, ptr);

        CTX_TEST_PTR_ID(C, "object", ptr->owner_id);
        break;
      }
      /* from rna_Main_objects_new */
      case OB_DATA_SUPPORT_ID_CASE: {

        if (ptr_id_type == ID_AR) {
          const bArmature *arm = (bArmature *)ptr->owner_id;
          if (arm->edbo != NULL) {
            TEST_PTR_DATA_TYPE("active_bone", RNA_EditBone, ptr, arm->act_edbone);
          }
          else {
            TEST_PTR_DATA_TYPE("active_bone", RNA_Bone, ptr, arm->act_bone);
          }
        }

#  define ID_CAST_OBDATA(id_pt) (((Object *)(id_pt))->data)
        CTX_TEST_PTR_ID_CAST(C, "object", "object.data", ID_CAST_OBDATA, ptr->owner_id);
        break;
#  undef ID_CAST_OBDATA
      }
      case ID_MA: {
#  define ID_CAST_OBMATACT(id_pt) \
    (BKE_object_material_get(((Object *)id_pt), ((Object *)id_pt)->actcol))
        CTX_TEST_PTR_ID_CAST(
            C, "object", "object.active_material", ID_CAST_OBMATACT, ptr->owner_id);
        break;
#  undef ID_CAST_OBMATACT
      }
      case ID_WO: {
#  define ID_CAST_SCENEWORLD(id_pt) (((Scene *)(id_pt))->world)
        CTX_TEST_PTR_ID_CAST(C, "scene", "scene.world", ID_CAST_SCENEWORLD, ptr->owner_id);
        break;
#  undef ID_CAST_SCENEWORLD
      }
      case ID_SCR: {
        CTX_TEST_PTR_ID(C, "screen", ptr->owner_id);

        TEST_PTR_DATA_TYPE("area", RNA_Area, ptr, CTX_wm_area(C));
        TEST_PTR_DATA_TYPE("region", RNA_Region, ptr, CTX_wm_region(C));

        SpaceLink *space_data = CTX_wm_space_data(C);
        if (space_data != NULL) {
          TEST_PTR_DATA_TYPE("space_data", RNA_Space, ptr, space_data);

          switch (space_data->spacetype) {
            case SPACE_VIEW3D: {
              const View3D *v3d = (View3D *)space_data;
              const View3DShading *shading = &v3d->shading;

              TEST_PTR_DATA_TYPE("space_data.overlay", RNA_View3DOverlay, ptr, v3d);
              TEST_PTR_DATA_TYPE("space_data.shading", RNA_View3DShading, ptr, shading);
              break;
            }
            case SPACE_GRAPH: {
              const SpaceGraph *sipo = (SpaceGraph *)space_data;
              const bDopeSheet *ads = sipo->ads;
              TEST_PTR_DATA_TYPE("space_data.dopesheet", RNA_DopeSheet, ptr, ads);
              break;
            }
            case SPACE_FILE: {
              const SpaceFile *sfile = (SpaceFile *)space_data;
              const FileSelectParams *params = ED_fileselect_get_active_params(sfile);
              TEST_PTR_DATA_TYPE("space_data.params", RNA_FileSelectParams, ptr, params);
              break;
            }
            case SPACE_IMAGE: {
              const SpaceImage *sima = (SpaceImage *)space_data;
              TEST_PTR_DATA_TYPE("space_data.overlay", RNA_SpaceImageOverlay, ptr, sima);
              TEST_PTR_DATA_TYPE("space_data.uv_editor", RNA_SpaceUVEditor, ptr, sima);
              break;
            }
            case SPACE_NLA: {
              const SpaceNla *snla = (SpaceNla *)space_data;
              const bDopeSheet *ads = snla->ads;
              TEST_PTR_DATA_TYPE("space_data.dopesheet", RNA_DopeSheet, ptr, ads);
              break;
            }
            case SPACE_ACTION: {
              const SpaceAction *sact = (SpaceAction *)space_data;
              const bDopeSheet *ads = &sact->ads;
              TEST_PTR_DATA_TYPE("space_data.dopesheet", RNA_DopeSheet, ptr, ads);
              break;
            }
          }
        }

        break;
      }
      default:
        break;
    }
#  undef CTX_TEST_PTR_ID
#  undef CTX_TEST_PTR_ID_CAST
#  undef TEST_PTR_DATA_TYPE
  }

  *r_is_id = is_id;

  return member_id;
}
#endif

char *WM_context_path_resolve_property_full(const bContext *C,
                                            const PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            int index)
{
  bool is_id;
  const char *member_id = wm_context_member_from_ptr(C, ptr, &is_id);
  char *member_id_data_path = NULL;
  if (member_id != NULL) {
    if (is_id && !RNA_struct_is_ID(ptr->type)) {
      char *data_path = RNA_path_from_ID_to_struct(ptr);
      if (data_path != NULL) {
        if (prop != NULL) {
          char *prop_str = RNA_path_property_py(ptr, prop, index);
          if (prop_str[0] == '[') {
            member_id_data_path = BLI_string_joinN(member_id, ".", data_path, prop_str);
          }
          else {
            member_id_data_path = BLI_string_join_by_sep_charN(
                '.', member_id, data_path, prop_str);
          }
          MEM_freeN(prop_str);
        }
        else {
          member_id_data_path = BLI_string_join_by_sep_charN('.', member_id, data_path);
        }
        MEM_freeN(data_path);
      }
    }
    else {
      if (prop != NULL) {
        char *prop_str = RNA_path_property_py(ptr, prop, index);
        if (prop_str[0] == '[') {
          member_id_data_path = BLI_string_joinN(member_id, prop_str);
        }
        else {
          member_id_data_path = BLI_string_join_by_sep_charN('.', member_id, prop_str);
        }
        MEM_freeN(prop_str);
      }
      else {
        member_id_data_path = BLI_strdup(member_id);
      }
    }
  }
  return member_id_data_path;
}

char *WM_context_path_resolve_full(bContext *C, const PointerRNA *ptr)
{
  return WM_context_path_resolve_property_full(C, ptr, NULL, -1);
}

static char *wm_prop_pystring_from_context(bContext *C,
                                           PointerRNA *ptr,
                                           PropertyRNA *prop,
                                           int index)
{
  char *member_id_data_path = WM_context_path_resolve_property_full(C, ptr, prop, index);
  char *ret = NULL;
  if (member_id_data_path != NULL) {
    ret = BLI_sprintfN("bpy.context.%s", member_id_data_path);
    MEM_freeN(member_id_data_path);
  }
  return ret;
}

char *WM_prop_pystring_assign(bContext *C, PointerRNA *ptr, PropertyRNA *prop, int index)
{
  char *lhs = C ? wm_prop_pystring_from_context(C, ptr, prop, index) : NULL;

  if (lhs == NULL) {
    /* Fallback to `bpy.data.foo[id]` if we don't find in the context. */
    lhs = RNA_path_full_property_py(CTX_data_main(C), ptr, prop, index);
  }

  if (!lhs) {
    return NULL;
  }

  char *rhs = RNA_property_as_string(C, ptr, prop, index, INT_MAX);
  if (!rhs) {
    MEM_freeN(lhs);
    return NULL;
  }

  char *ret = BLI_sprintfN("%s = %s", lhs, rhs);
  MEM_freeN(lhs);
  MEM_freeN(rhs);
  return ret;
}

void WM_operator_properties_create_ptr(PointerRNA *ptr, wmOperatorType *ot)
{
  /* Set the ID so the context can be accessed: see #STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID. */
  RNA_pointer_create(G_MAIN->wm.first, ot->srna, NULL, ptr);
}

void WM_operator_properties_create(PointerRNA *ptr, const char *opstring)
{
  wmOperatorType *ot = WM_operatortype_find(opstring, false);

  if (ot) {
    WM_operator_properties_create_ptr(ptr, ot);
  }
  else {
    /* Set the ID so the context can be accessed: see #STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID. */
    RNA_pointer_create(G_MAIN->wm.first, &RNA_OperatorProperties, NULL, ptr);
  }
}

void WM_operator_properties_alloc(PointerRNA **ptr, IDProperty **properties, const char *opstring)
{
  IDProperty *tmp_properties = NULL;
  /* Allow passing NULL for properties, just create the properties here then. */
  if (properties == NULL) {
    properties = &tmp_properties;
  }

  if (*properties == NULL) {
    IDPropertyTemplate val = {0};
    *properties = IDP_New(IDP_GROUP, &val, "wmOpItemProp");
  }

  if (*ptr == NULL) {
    *ptr = MEM_callocN(sizeof(PointerRNA), "wmOpItemPtr");
    WM_operator_properties_create(*ptr, opstring);
  }

  (*ptr)->data = *properties;
}

void WM_operator_properties_sanitize(PointerRNA *ptr, const bool no_context)
{
  RNA_STRUCT_BEGIN (ptr, prop) {
    switch (RNA_property_type(prop)) {
      case PROP_ENUM:
        if (no_context) {
          RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
        }
        else {
          RNA_def_property_clear_flag(prop, PROP_ENUM_NO_CONTEXT);
        }
        break;
      case PROP_POINTER: {
        StructRNA *ptype = RNA_property_pointer_type(ptr, prop);

        /* recurse into operator properties */
        if (RNA_struct_is_a(ptype, &RNA_OperatorProperties)) {
          PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
          WM_operator_properties_sanitize(&opptr, no_context);
        }
        break;
      }
      default:
        break;
    }
  }
  RNA_STRUCT_END;
}

bool WM_operator_properties_default(PointerRNA *ptr, const bool do_update)
{
  bool changed = false;
  RNA_STRUCT_BEGIN (ptr, prop) {
    switch (RNA_property_type(prop)) {
      case PROP_POINTER: {
        StructRNA *ptype = RNA_property_pointer_type(ptr, prop);
        if (ptype != &RNA_Struct) {
          PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
          changed |= WM_operator_properties_default(&opptr, do_update);
        }
        break;
      }
      default:
        if ((do_update == false) || (RNA_property_is_set(ptr, prop) == false)) {
          if (RNA_property_reset(ptr, prop, -1)) {
            changed = true;
          }
        }
        break;
    }
  }
  RNA_STRUCT_END;

  return changed;
}

void WM_operator_properties_reset(wmOperator *op)
{
  if (op->ptr->data) {
    PropertyRNA *iterprop = RNA_struct_iterator_property(op->type->srna);

    RNA_PROP_BEGIN (op->ptr, itemptr, iterprop) {
      PropertyRNA *prop = itemptr.data;

      if ((RNA_property_flag(prop) & PROP_SKIP_SAVE) == 0) {
        const char *identifier = RNA_property_identifier(prop);
        RNA_struct_idprops_unset(op->ptr, identifier);
      }
    }
    RNA_PROP_END;
  }
}

void WM_operator_properties_clear(PointerRNA *ptr)
{
  IDProperty *properties = ptr->data;

  if (properties) {
    IDP_ClearProperty(properties);
  }
}

void WM_operator_properties_free(PointerRNA *ptr)
{
  IDProperty *properties = ptr->data;

  if (properties) {
    IDP_FreeProperty(properties);
    ptr->data = NULL; /* just in case */
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Last Properties API
 * \{ */

#if 1 /* may want to disable operator remembering previous state for testing */

static bool operator_last_properties_init_impl(wmOperator *op, IDProperty *last_properties)
{
  bool changed = false;
  IDPropertyTemplate val = {0};
  IDProperty *replaceprops = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");

  CLOG_INFO(WM_LOG_OPERATORS, 1, "loading previous properties for '%s'", op->type->idname);

  PropertyRNA *iterprop = RNA_struct_iterator_property(op->type->srna);

  RNA_PROP_BEGIN (op->ptr, itemptr, iterprop) {
    PropertyRNA *prop = itemptr.data;
    if ((RNA_property_flag(prop) & PROP_SKIP_SAVE) == 0) {
      if (!RNA_property_is_set(op->ptr, prop)) { /* don't override a setting already set */
        const char *identifier = RNA_property_identifier(prop);
        IDProperty *idp_src = IDP_GetPropertyFromGroup(last_properties, identifier);
        if (idp_src) {
          IDProperty *idp_dst = IDP_CopyProperty(idp_src);

          /* NOTE: in the future this may need to be done recursively,
           * but for now RNA doesn't access nested operators */
          idp_dst->flag |= IDP_FLAG_GHOST;

          /* add to temporary group instead of immediate replace,
           * because we are iterating over this group */
          IDP_AddToGroup(replaceprops, idp_dst);
          changed = true;
        }
      }
    }
  }
  RNA_PROP_END;

  IDP_MergeGroup(op->properties, replaceprops, true);
  IDP_FreeProperty(replaceprops);
  return changed;
}

bool WM_operator_last_properties_init(wmOperator *op)
{
  bool changed = false;
  if (op->type->last_properties) {
    changed |= operator_last_properties_init_impl(op, op->type->last_properties);
    LISTBASE_FOREACH (wmOperator *, opm, &op->macro) {
      IDProperty *idp_src = IDP_GetPropertyFromGroup(op->type->last_properties, opm->idname);
      if (idp_src) {
        changed |= operator_last_properties_init_impl(opm, idp_src);
      }
    }
  }
  return changed;
}

bool WM_operator_last_properties_store(wmOperator *op)
{
  if (op->type->last_properties) {
    IDP_FreeProperty(op->type->last_properties);
    op->type->last_properties = NULL;
  }

  if (op->properties) {
    CLOG_INFO(WM_LOG_OPERATORS, 1, "storing properties for '%s'", op->type->idname);
    op->type->last_properties = IDP_CopyProperty(op->properties);
  }

  if (op->macro.first != NULL) {
    LISTBASE_FOREACH (wmOperator *, opm, &op->macro) {
      if (opm->properties) {
        if (op->type->last_properties == NULL) {
          op->type->last_properties = IDP_New(
              IDP_GROUP, &(IDPropertyTemplate){0}, "wmOperatorProperties");
        }
        IDProperty *idp_macro = IDP_CopyProperty(opm->properties);
        STRNCPY(idp_macro->name, opm->type->idname);
        IDP_ReplaceInGroup(op->type->last_properties, idp_macro);
      }
    }
  }

  return (op->type->last_properties != NULL);
}

#else

bool WM_operator_last_properties_init(wmOperator *UNUSED(op))
{
  return false;
}

bool WM_operator_last_properties_store(wmOperator *UNUSED(op))
{
  return false;
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default Operator Callbacks
 * \{ */

int WM_generic_select_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  PropertyRNA *wait_to_deselect_prop = RNA_struct_find_property(op->ptr,
                                                                "wait_to_deselect_others");
  const short init_event_type = (short)POINTER_AS_INT(op->customdata);
  int ret_value = 0;

  /* get settings from RNA properties for operator */
  const int mval[2] = {RNA_int_get(op->ptr, "mouse_x"), RNA_int_get(op->ptr, "mouse_y")};

  if (init_event_type == 0) {
    if (event->val == KM_PRESS) {
      RNA_property_boolean_set(op->ptr, wait_to_deselect_prop, true);

      ret_value = op->type->exec(C, op);
      OPERATOR_RETVAL_CHECK(ret_value);
      op->customdata = POINTER_FROM_INT((int)event->type);
      if (ret_value & OPERATOR_RUNNING_MODAL) {
        WM_event_add_modal_handler(C, op);
      }
      return ret_value | OPERATOR_PASS_THROUGH;
    }
    /* If we are in init phase, and cannot validate init of modal operations,
     * just fall back to basic exec.
     */
    RNA_property_boolean_set(op->ptr, wait_to_deselect_prop, false);

    ret_value = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(ret_value);

    return ret_value | OPERATOR_PASS_THROUGH;
  }
  if (event->type == init_event_type && event->val == KM_RELEASE) {
    RNA_property_boolean_set(op->ptr, wait_to_deselect_prop, false);

    ret_value = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(ret_value);

    return ret_value | OPERATOR_PASS_THROUGH;
  }
  if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
    const int drag_delta[2] = {
        mval[0] - event->mval[0],
        mval[1] - event->mval[1],
    };
    /* If user moves mouse more than defined threshold, we consider select operator as
     * finished. Otherwise, it is still running until we get an 'release' event. In any
     * case, we pass through event, but select op is not finished yet. */
    if (WM_event_drag_test_with_delta(event, drag_delta)) {
      return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
    }
    /* Important not to return anything other than PASS_THROUGH here,
     * otherwise it prevents underlying drag detection code to work properly. */
    return OPERATOR_PASS_THROUGH;
  }

  return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
}

int WM_generic_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);

  int mval[2];
  WM_event_drag_start_mval(event, region, mval);

  RNA_int_set(op->ptr, "mouse_x", mval[0]);
  RNA_int_set(op->ptr, "mouse_y", mval[1]);

  op->customdata = POINTER_FROM_INT(0);

  return op->type->modal(C, op, event);
}

void WM_operator_view3d_unit_defaults(struct bContext *C, struct wmOperator *op)
{
  if (op->flag & OP_IS_INVOKE) {
    Scene *scene = CTX_data_scene(C);
    View3D *v3d = CTX_wm_view3d(C);

    const float dia = v3d ? ED_view3d_grid_scale(scene, v3d, NULL) :
                            ED_scene_grid_scale(scene, NULL);

    /* always run, so the values are initialized,
     * otherwise we may get differ behavior when (dia != 1.0) */
    RNA_STRUCT_BEGIN (op->ptr, prop) {
      if (RNA_property_type(prop) == PROP_FLOAT) {
        PropertySubType pstype = RNA_property_subtype(prop);
        if (pstype == PROP_DISTANCE) {
          /* we don't support arrays yet */
          BLI_assert(RNA_property_array_check(prop) == false);
          /* initialize */
          if (!RNA_property_is_set_ex(op->ptr, prop, false)) {
            const float value = RNA_property_float_get_default(op->ptr, prop) * dia;
            RNA_property_float_set(op->ptr, prop, value);
          }
        }
      }
    }
    RNA_STRUCT_END;
  }
}

int WM_operator_smooth_viewtx_get(const wmOperator *op)
{
  return (op->flag & OP_IS_INVOKE) ? U.smooth_viewtx : 0;
}

int WM_menu_invoke_ex(bContext *C, wmOperator *op, wmOperatorCallContext opcontext)
{
  PropertyRNA *prop = op->type->prop;

  if (prop == NULL) {
    CLOG_ERROR(WM_LOG_OPERATORS, "'%s' has no enum property set", op->type->idname);
  }
  else if (RNA_property_type(prop) != PROP_ENUM) {
    CLOG_ERROR(WM_LOG_OPERATORS,
               "'%s', '%s' is not an enum property",
               op->type->idname,
               RNA_property_identifier(prop));
  }
  else if (RNA_property_is_set(op->ptr, prop)) {
    const int retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);
    return retval;
  }
  else {
    uiPopupMenu *pup = UI_popup_menu_begin(C, WM_operatortype_name(op->type, op->ptr), ICON_NONE);
    uiLayout *layout = UI_popup_menu_layout(pup);
    /* set this so the default execution context is the same as submenus */
    uiLayoutSetOperatorContext(layout, opcontext);
    uiItemsFullEnumO(
        layout, op->type->idname, RNA_property_identifier(prop), op->ptr->data, opcontext, 0);
    UI_popup_menu_end(C, pup);
    return OPERATOR_INTERFACE;
  }

  return OPERATOR_CANCELLED;
}

int WM_menu_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return WM_menu_invoke_ex(C, op, WM_OP_INVOKE_REGION_WIN);
}

struct EnumSearchMenu {
  wmOperator *op; /* the operator that will be executed when selecting an item */

  bool use_previews;
  short prv_cols, prv_rows;
};

/** Generic enum search invoke popup. */
static uiBlock *wm_enum_search_menu(bContext *C, ARegion *region, void *arg)
{
  struct EnumSearchMenu *search_menu = arg;
  wmWindow *win = CTX_wm_window(C);
  wmOperator *op = search_menu->op;
  /* template_ID uses 4 * widget_unit for width,
   * we use a bit more, some items may have a suffix to show. */
  const int width = search_menu->use_previews ? 5 * U.widget_unit * search_menu->prv_cols :
                                                UI_searchbox_size_x();
  const int height = search_menu->use_previews ? 5 * U.widget_unit * search_menu->prv_rows :
                                                 UI_searchbox_size_y();
  static char search[256] = "";

  uiBlock *block = UI_block_begin(C, region, "_popup", UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  search[0] = '\0';
  BLI_assert(search_menu->use_previews ||
             (search_menu->prv_cols == 0 && search_menu->prv_rows == 0));
#if 0 /* ok, this isn't so easy... */
  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           WM_operatortype_name(op->type, op->ptr),
           10,
           10,
           UI_searchbox_size_x(),
           UI_UNIT_Y,
           NULL,
           0.0,
           0.0,
           0,
           0,
           "");
#endif
  uiBut *but = uiDefSearchButO_ptr(block,
                                   op->type,
                                   op->ptr->data,
                                   search,
                                   0,
                                   ICON_VIEWZOOM,
                                   sizeof(search),
                                   10,
                                   10,
                                   width,
                                   UI_UNIT_Y,
                                   search_menu->prv_rows,
                                   search_menu->prv_cols,
                                   "");

  /* fake button, it holds space for search items */
  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           "",
           10,
           10 - UI_searchbox_size_y(),
           width,
           height,
           NULL,
           0,
           0,
           0,
           0,
           NULL);

  /* Move it downwards, mouse over button. */
  UI_block_bounds_set_popup(block, 0.3f * U.widget_unit, (const int[2]){0, -UI_UNIT_Y});

  UI_but_focus_on_enter_event(win, but);

  return block;
}

int WM_enum_search_invoke_previews(bContext *C, wmOperator *op, short prv_cols, short prv_rows)
{
  static struct EnumSearchMenu search_menu;

  search_menu.op = op;
  search_menu.use_previews = true;
  search_menu.prv_cols = prv_cols;
  search_menu.prv_rows = prv_rows;

  UI_popup_block_invoke(C, wm_enum_search_menu, &search_menu, NULL);

  return OPERATOR_INTERFACE;
}

int WM_enum_search_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  static struct EnumSearchMenu search_menu;
  search_menu.op = op;
  UI_popup_block_invoke(C, wm_enum_search_menu, &search_menu, NULL);
  return OPERATOR_INTERFACE;
}

int WM_operator_confirm_message_ex(bContext *C,
                                   wmOperator *op,
                                   const char *title,
                                   const int icon,
                                   const char *message,
                                   const wmOperatorCallContext opcontext)
{
  IDProperty *properties = op->ptr->data;

  if (properties && properties->len) {
    properties = IDP_CopyProperty(op->ptr->data);
  }
  else {
    properties = NULL;
  }

  uiPopupMenu *pup = UI_popup_menu_begin(C, title, icon);
  uiLayout *layout = UI_popup_menu_layout(pup);
  uiItemFullO_ptr(layout, op->type, message, ICON_NONE, properties, opcontext, 0, NULL);
  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

int WM_operator_confirm_message(bContext *C, wmOperator *op, const char *message)
{
  return WM_operator_confirm_message_ex(
      C, op, IFACE_("OK?"), ICON_QUESTION, message, WM_OP_EXEC_REGION_WIN);
}

int WM_operator_confirm(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return WM_operator_confirm_message(C, op, NULL);
}

int WM_operator_confirm_or_exec(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  const bool confirm = RNA_boolean_get(op->ptr, "confirm");
  if (confirm) {
    return WM_operator_confirm_message(C, op, NULL);
  }
  return op->type->exec(C, op);
}

int WM_operator_filesel(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return WM_operator_call_notest(C, op); /* call exec direct */
  }
  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

bool WM_operator_filesel_ensure_ext_imtype(wmOperator *op, const struct ImageFormatData *im_format)
{
  char filepath[FILE_MAX];
  /* Don't NULL check prop, this can only run on ops with a 'filepath'. */
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "filepath");
  RNA_property_string_get(op->ptr, prop, filepath);
  if (BKE_image_path_ensure_ext_from_imformat(filepath, im_format)) {
    RNA_property_string_set(op->ptr, prop, filepath);
    /* NOTE: we could check for and update 'filename' here,
     * but so far nothing needs this. */
    return true;
  }
  return false;
}

bool WM_operator_winactive(bContext *C)
{
  if (CTX_wm_window(C) == NULL) {
    return 0;
  }
  return 1;
}

bool WM_operator_check_ui_enabled(const bContext *C, const char *idname)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  Scene *scene = CTX_data_scene(C);

  return !((ED_undo_is_valid(C, idname) == false) || WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY));
}

wmOperator *WM_operator_last_redo(const bContext *C)
{
  wmWindowManager *wm = CTX_wm_manager(C);

  /* only for operators that are registered and did an undo push */
  LISTBASE_FOREACH_BACKWARD (wmOperator *, op, &wm->operators) {
    if ((op->type->flag & OPTYPE_REGISTER) && (op->type->flag & OPTYPE_UNDO)) {
      return op;
    }
  }

  return NULL;
}

IDProperty *WM_operator_last_properties_ensure_idprops(wmOperatorType *ot)
{
  if (ot->last_properties == NULL) {
    IDPropertyTemplate val = {0};
    ot->last_properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
  }
  return ot->last_properties;
}

void WM_operator_last_properties_ensure(wmOperatorType *ot, PointerRNA *ptr)
{
  IDProperty *props = WM_operator_last_properties_ensure_idprops(ot);
  RNA_pointer_create(G_MAIN->wm.first, ot->srna, props, ptr);
}

ID *WM_operator_drop_load_path(struct bContext *C, wmOperator *op, const short idcode)
{
  Main *bmain = CTX_data_main(C);
  ID *id = NULL;

  /* check input variables */
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
    char path[FILE_MAX];
    bool exists = false;

    RNA_string_get(op->ptr, "filepath", path);

    errno = 0;

    if (idcode == ID_IM) {
      id = (ID *)BKE_image_load_exists_ex(bmain, path, &exists);
    }
    else {
      BLI_assert_unreachable();
    }

    if (!id) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Cannot read %s '%s': %s",
                  BKE_idtype_idcode_to_name(idcode),
                  path,
                  errno ? strerror(errno) : TIP_("unsupported format"));
      return NULL;
    }

    if (is_relative_path) {
      if (exists == false) {
        if (idcode == ID_IM) {
          BLI_path_rel(((Image *)id)->filepath, BKE_main_blendfile_path(bmain));
        }
        else {
          BLI_assert_unreachable();
        }
      }
    }

    return id;
  }

  if (!WM_operator_properties_id_lookup_is_set(op->ptr)) {
    return NULL;
  }

  /* Lookup an already existing ID. */
  id = WM_operator_properties_id_lookup_from_name_or_session_uuid(bmain, op->ptr, idcode);

  if (!id) {
    /* Print error with the name if the name is available. */

    if (RNA_struct_property_is_set(op->ptr, "name")) {
      char name[MAX_ID_NAME - 2];
      RNA_string_get(op->ptr, "name", name);
      BKE_reportf(
          op->reports, RPT_ERROR, "%s '%s' not found", BKE_idtype_idcode_to_name(idcode), name);
      return NULL;
    }

    BKE_reportf(op->reports, RPT_ERROR, "%s not found", BKE_idtype_idcode_to_name(idcode));
    return NULL;
  }

  id_us_plus(id);
  return id;
}

static void wm_block_redo_cb(bContext *C, void *arg_op, int UNUSED(arg_event))
{
  wmOperator *op = arg_op;

  if (op == WM_operator_last_redo(C)) {
    /* operator was already executed once? undo & repeat */
    ED_undo_operator_repeat(C, op);
  }
  else {
    /* operator not executed yet, call it */
    ED_undo_push_op(C, op);
    wm_operator_register(C, op);

    WM_operator_repeat(C, op);
  }
}

static void wm_block_redo_cancel_cb(bContext *C, void *arg_op)
{
  wmOperator *op = arg_op;

  /* if operator never got executed, free it */
  if (op != WM_operator_last_redo(C)) {
    WM_operator_free(op);
  }
}

static uiBlock *wm_block_create_redo(bContext *C, ARegion *region, void *arg_op)
{
  wmOperator *op = arg_op;
  const uiStyle *style = UI_style_get_dpi();
  int width = 15 * UI_UNIT_X;

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_disable(block, UI_BLOCK_LOOP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_REGULAR);

  /* UI_BLOCK_NUMSELECT for layer buttons */
  UI_block_flag_enable(block, UI_BLOCK_NUMSELECT | UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT);

  /* if register is not enabled, the operator gets freed on OPERATOR_FINISHED
   * ui_apply_but_funcs_after calls ED_undo_operator_repeate_cb and crashes */
  BLI_assert(op->type->flag & OPTYPE_REGISTER);

  UI_block_func_handle_set(block, wm_block_redo_cb, arg_op);
  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, width, UI_UNIT_Y, 0, style);

  if (op == WM_operator_last_redo(C)) {
    if (!WM_operator_check_ui_enabled(C, op->type->name)) {
      uiLayoutSetEnabled(layout, false);
    }
  }

  uiLayout *col = uiLayoutColumn(layout, false);
  uiTemplateOperatorPropertyButs(
      C, col, op, UI_BUT_LABEL_ALIGN_NONE, UI_TEMPLATE_OP_PROPS_SHOW_TITLE);

  UI_block_bounds_set_popup(block, 6 * U.dpi_fac, NULL);

  return block;
}

typedef struct wmOpPopUp {
  wmOperator *op;
  int width;
  int height;
  int free_op;
} wmOpPopUp;

/* Only invoked by OK button in popups created with wm_block_dialog_create() */
static void dialog_exec_cb(bContext *C, void *arg1, void *arg2)
{
  wmOperator *op;
  {
    /* Execute will free the operator.
     * In this case, wm_operator_ui_popup_cancel won't run. */
    wmOpPopUp *data = arg1;
    op = data->op;
    MEM_freeN(data);
  }

  uiBlock *block = arg2;
  /* Explicitly set UI_RETURN_OK flag, otherwise the menu might be canceled
   * in case WM_operator_call_ex exits/reloads the current file (T49199). */

  UI_popup_menu_retval_set(block, UI_RETURN_OK, true);

  /* Get context data *after* WM_operator_call_ex
   * which might have closed the current file and changed context. */
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, block);

  WM_operator_call_ex(C, op, true);
}

/* Dialogs are popups that require user verification (click OK) before exec */
static uiBlock *wm_block_dialog_create(bContext *C, ARegion *region, void *userData)
{
  wmOpPopUp *data = userData;
  wmOperator *op = data->op;
  const uiStyle *style = UI_style_get_dpi();

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_disable(block, UI_BLOCK_LOOP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_REGULAR);

  /* intentionally don't use 'UI_BLOCK_MOVEMOUSE_QUIT', some dialogues have many items
   * where quitting by accident is very annoying */
  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_NUMSELECT);

  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, data->width, data->height, 0, style);

  uiTemplateOperatorPropertyButs(
      C, layout, op, UI_BUT_LABEL_ALIGN_SPLIT_COLUMN, UI_TEMPLATE_OP_PROPS_SHOW_TITLE);

  /* clear so the OK button is left alone */
  UI_block_func_set(block, NULL, NULL, NULL);

  /* new column so as not to interfere with custom layouts T26436. */
  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiBlock *col_block = uiLayoutGetBlock(col);
    /* Create OK button, the callback of which will execute op */
    uiBut *but = uiDefBut(
        col_block, UI_BTYPE_BUT, 0, IFACE_("OK"), 0, -30, 0, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
    UI_but_flag_enable(but, UI_BUT_ACTIVE_DEFAULT);
    UI_but_func_set(but, dialog_exec_cb, data, col_block);
  }

  /* center around the mouse */
  UI_block_bounds_set_popup(
      block, 6 * U.dpi_fac, (const int[2]){data->width / -2, data->height / 2});

  return block;
}

static uiBlock *wm_operator_ui_create(bContext *C, ARegion *region, void *userData)
{
  wmOpPopUp *data = userData;
  wmOperator *op = data->op;
  const uiStyle *style = UI_style_get_dpi();

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  UI_block_flag_disable(block, UI_BLOCK_LOOP);
  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_REGULAR);

  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, data->width, data->height, 0, style);

  /* since ui is defined the auto-layout args are not used */
  uiTemplateOperatorPropertyButs(C, layout, op, UI_BUT_LABEL_ALIGN_COLUMN, 0);

  UI_block_func_set(block, NULL, NULL, NULL);

  UI_block_bounds_set_popup(block, 6 * U.dpi_fac, NULL);

  return block;
}

static void wm_operator_ui_popup_cancel(struct bContext *C, void *userData)
{
  wmOpPopUp *data = userData;
  wmOperator *op = data->op;

  if (op) {
    if (op->type->cancel) {
      op->type->cancel(C, op);
    }

    if (data->free_op) {
      WM_operator_free(op);
    }
  }

  MEM_freeN(data);
}

static void wm_operator_ui_popup_ok(struct bContext *C, void *arg, int retval)
{
  wmOpPopUp *data = arg;
  wmOperator *op = data->op;

  if (op && retval > 0) {
    WM_operator_call_ex(C, op, true);
  }

  MEM_freeN(data);
}

int WM_operator_ui_popup(bContext *C, wmOperator *op, int width)
{
  wmOpPopUp *data = MEM_callocN(sizeof(wmOpPopUp), "WM_operator_ui_popup");
  data->op = op;
  data->width = width * U.dpi_fac;
  /* Actual used height depends on the content. */
  data->height = 0;
  data->free_op = true; /* if this runs and gets registered we may want not to free it */
  UI_popup_block_ex(C, wm_operator_ui_create, NULL, wm_operator_ui_popup_cancel, data, op);
  return OPERATOR_RUNNING_MODAL;
}

/**
 * For use by #WM_operator_props_popup_call, #WM_operator_props_popup only.
 *
 * \note operator menu needs undo flag enabled, for redo callback */
static int wm_operator_props_popup_ex(bContext *C,
                                      wmOperator *op,
                                      const bool do_call,
                                      const bool do_redo)
{
  if ((op->type->flag & OPTYPE_REGISTER) == 0) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Operator '%s' does not have register enabled, incorrect invoke function",
                op->type->idname);
    return OPERATOR_CANCELLED;
  }

  if (do_redo) {
    if ((op->type->flag & OPTYPE_UNDO) == 0) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Operator '%s' does not have undo enabled, incorrect invoke function",
                  op->type->idname);
      return OPERATOR_CANCELLED;
    }
  }

  /* if we don't have global undo, we can't do undo push for automatic redo,
   * so we require manual OK clicking in this popup */
  if (!do_redo || !(U.uiflag & USER_GLOBALUNDO)) {
    return WM_operator_props_dialog_popup(C, op, 300);
  }

  UI_popup_block_ex(C, wm_block_create_redo, NULL, wm_block_redo_cancel_cb, op, op);

  if (do_call) {
    wm_block_redo_cb(C, op, 0);
  }

  return OPERATOR_RUNNING_MODAL;
}

int WM_operator_props_popup_confirm(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return wm_operator_props_popup_ex(C, op, false, false);
}

int WM_operator_props_popup_call(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return wm_operator_props_popup_ex(C, op, true, true);
}

int WM_operator_props_popup(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  return wm_operator_props_popup_ex(C, op, false, true);
}

int WM_operator_props_dialog_popup(bContext *C, wmOperator *op, int width)
{
  wmOpPopUp *data = MEM_callocN(sizeof(wmOpPopUp), "WM_operator_props_dialog_popup");

  data->op = op;
  data->width = width * U.dpi_fac;
  /* Actual height depends on the content. */
  data->height = 0;
  data->free_op = true; /* if this runs and gets registered we may want not to free it */

  /* op is not executed until popup OK but is clicked */
  UI_popup_block_ex(
      C, wm_block_dialog_create, wm_operator_ui_popup_ok, wm_operator_ui_popup_cancel, data, op);

  return OPERATOR_RUNNING_MODAL;
}

int WM_operator_redo_popup(bContext *C, wmOperator *op)
{
  /* CTX_wm_reports(C) because operator is on stack, not active in event system */
  if ((op->type->flag & OPTYPE_REGISTER) == 0) {
    BKE_reportf(CTX_wm_reports(C),
                RPT_ERROR,
                "Operator redo '%s' does not have register enabled, incorrect invoke function",
                op->type->idname);
    return OPERATOR_CANCELLED;
  }
  if (op->type->poll && op->type->poll(C) == 0) {
    BKE_reportf(
        CTX_wm_reports(C), RPT_ERROR, "Operator redo '%s': wrong context", op->type->idname);
    return OPERATOR_CANCELLED;
  }

  UI_popup_block_invoke(C, wm_block_create_redo, op, NULL);

  return OPERATOR_CANCELLED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug Menu Operator
 *
 * Set internal debug value, mainly for developers.
 * \{ */

static int wm_debug_menu_exec(bContext *C, wmOperator *op)
{
  G.debug_value = RNA_int_get(op->ptr, "debug_value");
  ED_screen_refresh(CTX_wm_manager(C), CTX_wm_window(C));
  WM_event_add_notifier(C, NC_WINDOW, NULL);

  return OPERATOR_FINISHED;
}

static int wm_debug_menu_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  RNA_int_set(op->ptr, "debug_value", G.debug_value);
  return WM_operator_props_dialog_popup(C, op, 250);
}

static void WM_OT_debug_menu(wmOperatorType *ot)
{
  ot->name = "Debug Menu";
  ot->idname = "WM_OT_debug_menu";
  ot->description = "Open a popup to set the debug level";

  ot->invoke = wm_debug_menu_invoke;
  ot->exec = wm_debug_menu_exec;
  ot->poll = WM_operator_winactive;

  RNA_def_int(ot->srna, "debug_value", 0, SHRT_MIN, SHRT_MAX, "Debug Value", "", -10000, 10000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reset Defaults Operator
 * \{ */

static int wm_operator_defaults_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "active_operator", &RNA_Operator);

  if (!ptr.data) {
    BKE_report(op->reports, RPT_ERROR, "No operator in context");
    return OPERATOR_CANCELLED;
  }

  WM_operator_properties_reset((wmOperator *)ptr.data);
  return OPERATOR_FINISHED;
}

/* used by operator preset menu. pre-2.65 this was a 'Reset' button */
static void WM_OT_operator_defaults(wmOperatorType *ot)
{
  ot->name = "Restore Operator Defaults";
  ot->idname = "WM_OT_operator_defaults";
  ot->description = "Set the active operator to its default values";

  ot->exec = wm_operator_defaults_exec;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator/Menu Search Operator
 * \{ */

struct SearchPopupInit_Data {
  enum {
    SEARCH_TYPE_OPERATOR = 0,
    SEARCH_TYPE_MENU = 1,
  } search_type;

  int size[2];
};

static uiBlock *wm_block_search_menu(bContext *C, ARegion *region, void *userdata)
{
  const struct SearchPopupInit_Data *init_data = userdata;
  static char search[256] = "";

  uiBlock *block = UI_block_begin(C, region, "_popup", UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiBut *but = uiDefSearchBut(block,
                              search,
                              0,
                              ICON_VIEWZOOM,
                              sizeof(search),
                              10,
                              10,
                              init_data->size[0],
                              UI_UNIT_Y,
                              0,
                              0,
                              "");

  if (init_data->search_type == SEARCH_TYPE_OPERATOR) {
    UI_but_func_operator_search(but);
  }
  else if (init_data->search_type == SEARCH_TYPE_MENU) {
    UI_but_func_menu_search(but);
  }
  else {
    BLI_assert_unreachable();
  }

  UI_but_flag_enable(but, UI_BUT_ACTIVATE_ON_INIT);

  /* fake button, it holds space for search items */
  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           "",
           10,
           10 - init_data->size[1],
           init_data->size[0],
           init_data->size[1],
           NULL,
           0,
           0,
           0,
           0,
           NULL);

  /* Move it downwards, mouse over button. */
  UI_block_bounds_set_popup(block, 0.3f * U.widget_unit, (const int[2]){0, -UI_UNIT_Y});

  return block;
}

static int wm_search_menu_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
  return OPERATOR_FINISHED;
}

static int wm_search_menu_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Exception for launching via spacebar */
  if (event->type == EVT_SPACEKEY) {
    bool ok = true;
    ScrArea *area = CTX_wm_area(C);
    if (area) {
      if (area->spacetype == SPACE_CONSOLE) {
        /* So we can use the shortcut in the console. */
        ok = false;
      }
      else if (area->spacetype == SPACE_TEXT) {
        /* So we can use the spacebar in the text editor. */
        ok = false;
      }
    }
    else {
      Object *editob = CTX_data_edit_object(C);
      if (editob && editob->type == OB_FONT) {
        /* So we can use the spacebar for entering text. */
        ok = false;
      }
    }
    if (!ok) {
      return OPERATOR_PASS_THROUGH;
    }
  }

  int search_type;
  if (STREQ(op->type->idname, "WM_OT_search_menu")) {
    search_type = SEARCH_TYPE_MENU;
  }
  else {
    search_type = SEARCH_TYPE_OPERATOR;
  }

  static struct SearchPopupInit_Data data;
  data = (struct SearchPopupInit_Data){
      .search_type = search_type,
      .size = {UI_searchbox_size_x() * 2, UI_searchbox_size_y()},
  };

  UI_popup_block_invoke_ex(C, wm_block_search_menu, &data, NULL, false);

  return OPERATOR_INTERFACE;
}

static void WM_OT_search_menu(wmOperatorType *ot)
{
  ot->name = "Search Menu";
  ot->idname = "WM_OT_search_menu";
  ot->description = "Pop-up a search over all menus in the current context";

  ot->invoke = wm_search_menu_invoke;
  ot->exec = wm_search_menu_exec;
  ot->poll = WM_operator_winactive;
}

static void WM_OT_search_operator(wmOperatorType *ot)
{
  ot->name = "Search Operator";
  ot->idname = "WM_OT_search_operator";
  ot->description = "Pop-up a search over all available operators in current context";

  ot->invoke = wm_search_menu_invoke;
  ot->exec = wm_search_menu_exec;
  ot->poll = WM_operator_winactive;
}

static int wm_call_menu_exec(bContext *C, wmOperator *op)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(op->ptr, "name", idname);

  return UI_popup_menu_invoke(C, idname, op->reports);
}

static const char *wm_call_menu_get_name(wmOperatorType *ot, PointerRNA *ptr)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(ptr, "name", idname);
  MenuType *mt = WM_menutype_find(idname, true);
  return (mt) ? CTX_IFACE_(mt->translation_context, mt->label) :
                CTX_IFACE_(ot->translation_context, ot->name);
}

static void WM_OT_call_menu(wmOperatorType *ot)
{
  ot->name = "Call Menu";
  ot->idname = "WM_OT_call_menu";
  ot->description = "Open a predefined menu";

  ot->exec = wm_call_menu_exec;
  ot->poll = WM_operator_winactive;
  ot->get_name = wm_call_menu_get_name;

  ot->flag = OPTYPE_INTERNAL;

  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "name", NULL, BKE_ST_MAXNAME, "Name", "Name of the menu");
  RNA_def_property_string_search_func_runtime(
      prop,
      WM_menutype_idname_visit_for_search,
      /* Only a suggestion as menu items may be referenced from add-ons that have been disabled. */
      (PROP_STRING_SEARCH_SORT | PROP_STRING_SEARCH_SUGGESTION));
}

static int wm_call_pie_menu_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(op->ptr, "name", idname);

  return UI_pie_menu_invoke(C, idname, event);
}

static int wm_call_pie_menu_exec(bContext *C, wmOperator *op)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(op->ptr, "name", idname);

  return UI_pie_menu_invoke(C, idname, CTX_wm_window(C)->eventstate);
}

static void WM_OT_call_menu_pie(wmOperatorType *ot)
{
  ot->name = "Call Pie Menu";
  ot->idname = "WM_OT_call_menu_pie";
  ot->description = "Open a predefined pie menu";

  ot->invoke = wm_call_pie_menu_invoke;
  ot->exec = wm_call_pie_menu_exec;
  ot->poll = WM_operator_winactive;
  ot->get_name = wm_call_menu_get_name;

  ot->flag = OPTYPE_INTERNAL;

  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "name", NULL, BKE_ST_MAXNAME, "Name", "Name of the pie menu");
  RNA_def_property_string_search_func_runtime(
      prop,
      WM_menutype_idname_visit_for_search,
      /* Only a suggestion as menu items may be referenced from add-ons that have been disabled. */
      (PROP_STRING_SEARCH_SORT | PROP_STRING_SEARCH_SUGGESTION));
}

static int wm_call_panel_exec(bContext *C, wmOperator *op)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(op->ptr, "name", idname);
  const bool keep_open = RNA_boolean_get(op->ptr, "keep_open");

  return UI_popover_panel_invoke(C, idname, keep_open, op->reports);
}

static const char *wm_call_panel_get_name(wmOperatorType *ot, PointerRNA *ptr)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(ptr, "name", idname);
  PanelType *pt = WM_paneltype_find(idname, true);
  return (pt) ? CTX_IFACE_(pt->translation_context, pt->label) :
                CTX_IFACE_(ot->translation_context, ot->name);
}

static void WM_OT_call_panel(wmOperatorType *ot)
{
  ot->name = "Call Panel";
  ot->idname = "WM_OT_call_panel";
  ot->description = "Open a predefined panel";

  ot->exec = wm_call_panel_exec;
  ot->poll = WM_operator_winactive;
  ot->get_name = wm_call_panel_get_name;

  ot->flag = OPTYPE_INTERNAL;

  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna, "name", NULL, BKE_ST_MAXNAME, "Name", "Name of the menu");
  RNA_def_property_string_search_func_runtime(
      prop,
      WM_paneltype_idname_visit_for_search,
      /* Only a suggestion as menu items may be referenced from add-ons that have been disabled. */
      (PROP_STRING_SEARCH_SORT | PROP_STRING_SEARCH_SUGGESTION));
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "keep_open", true, "Keep Open", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window/Screen Operators
 * \{ */

/* this poll functions is needed in place of WM_operator_winactive
 * while it crashes on full screen */
static bool wm_operator_winactive_normal(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen;

  if (win == NULL) {
    return 0;
  }
  if (!((screen = WM_window_get_active_screen(win)) && (screen->state == SCREENNORMAL))) {
    return 0;
  }
  if (G.background) {
    return 0;
  }

  return 1;
}

/* included for script-access */
static void WM_OT_window_close(wmOperatorType *ot)
{
  ot->name = "Close Window";
  ot->idname = "WM_OT_window_close";
  ot->description = "Close the current window";

  ot->exec = wm_window_close_exec;
  ot->poll = WM_operator_winactive;
}

static void WM_OT_window_new(wmOperatorType *ot)
{
  ot->name = "New Window";
  ot->idname = "WM_OT_window_new";
  ot->description = "Create a new window";

  ot->exec = wm_window_new_exec;
  ot->poll = wm_operator_winactive_normal;
}

static void WM_OT_window_new_main(wmOperatorType *ot)
{
  ot->name = "New Main Window";
  ot->idname = "WM_OT_window_new_main";
  ot->description = "Create a new main window with its own workspace and scene selection";

  ot->exec = wm_window_new_main_exec;
  ot->poll = wm_operator_winactive_normal;
}

static void WM_OT_window_fullscreen_toggle(wmOperatorType *ot)
{
  ot->name = "Toggle Window Fullscreen";
  ot->idname = "WM_OT_window_fullscreen_toggle";
  ot->description = "Toggle the current window full-screen";

  ot->exec = wm_window_fullscreen_toggle_exec;
  ot->poll = WM_operator_winactive;
}

static int wm_exit_blender_exec(bContext *C, wmOperator *UNUSED(op))
{
  wm_exit_schedule_delayed(C);
  return OPERATOR_FINISHED;
}

static int wm_exit_blender_invoke(bContext *C,
                                  wmOperator *UNUSED(op),
                                  const wmEvent *UNUSED(event))
{
  if (U.uiflag & USER_SAVE_PROMPT) {
    wm_quit_with_optional_confirmation_prompt(C, CTX_wm_window(C));
  }
  else {
    wm_exit_schedule_delayed(C);
  }
  return OPERATOR_FINISHED;
}

static void WM_OT_quit_blender(wmOperatorType *ot)
{
  ot->name = "Quit Blender";
  ot->idname = "WM_OT_quit_blender";
  ot->description = "Quit Blender";

  ot->invoke = wm_exit_blender_invoke;
  ot->exec = wm_exit_blender_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Console Toggle Operator (WIN32 only)
 * \{ */

#if defined(WIN32)

static int wm_console_toggle_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
  GHOST_setConsoleWindowState(GHOST_kConsoleWindowStateToggle);
  return OPERATOR_FINISHED;
}

static void WM_OT_console_toggle(wmOperatorType *ot)
{
  /* XXX Have to mark these for xgettext, as under linux they do not exists... */
  ot->name = CTX_N_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Toggle System Console");
  ot->idname = "WM_OT_console_toggle";
  ot->description = N_("Toggle System Console");

  ot->exec = wm_console_toggle_exec;
  ot->poll = WM_operator_winactive;
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name default paint cursors, draw always around cursor
 *
 * - Returns handler to free.
 * - `poll(bContext)`: returns 1 if draw should happen.
 * - `draw(bContext)`: drawing callback for paint cursor.
 *
 * \{ */

wmPaintCursor *WM_paint_cursor_activate(short space_type,
                                        short region_type,
                                        bool (*poll)(bContext *C),
                                        wmPaintCursorDraw draw,
                                        void *customdata)
{
  wmWindowManager *wm = G_MAIN->wm.first;

  wmPaintCursor *pc = MEM_callocN(sizeof(wmPaintCursor), "paint cursor");

  BLI_addtail(&wm->paintcursors, pc);

  pc->customdata = customdata;
  pc->poll = poll;
  pc->draw = draw;

  pc->space_type = space_type;
  pc->region_type = region_type;

  return pc;
}

bool WM_paint_cursor_end(wmPaintCursor *handle)
{
  wmWindowManager *wm = G_MAIN->wm.first;
  LISTBASE_FOREACH (wmPaintCursor *, pc, &wm->paintcursors) {
    if (pc == (wmPaintCursor *)handle) {
      BLI_remlink(&wm->paintcursors, pc);
      MEM_freeN(pc);
      return true;
    }
  }
  return false;
}

void WM_paint_cursor_remove_by_type(wmWindowManager *wm, void *draw_fn, void (*free)(void *))
{
  LISTBASE_FOREACH_MUTABLE (wmPaintCursor *, pc, &wm->paintcursors) {
    if (pc->draw == draw_fn) {
      if (free) {
        free(pc->customdata);
      }
      BLI_remlink(&wm->paintcursors, pc);
      MEM_freeN(pc);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Radial Control Operator
 * \{ */

#define WM_RADIAL_CONTROL_DISPLAY_SIZE (200 * UI_DPI_FAC)
#define WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE (35 * UI_DPI_FAC)
#define WM_RADIAL_CONTROL_DISPLAY_WIDTH \
  (WM_RADIAL_CONTROL_DISPLAY_SIZE - WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE)
#define WM_RADIAL_MAX_STR 10

typedef struct {
  PropertyType type;
  PropertySubType subtype;
  PointerRNA ptr, col_ptr, fill_col_ptr, rot_ptr, zoom_ptr, image_id_ptr;
  PointerRNA fill_col_override_ptr, fill_col_override_test_ptr;
  PropertyRNA *prop, *col_prop, *fill_col_prop, *rot_prop, *zoom_prop;
  PropertyRNA *fill_col_override_prop, *fill_col_override_test_prop;
  StructRNA *image_id_srna;
  float initial_value, current_value, min_value, max_value;
  int initial_mouse[2];
  int initial_co[2];
  int slow_mouse[2];
  bool slow_mode;
  Dial *dial;
  GPUTexture *texture;
  ListBase orig_paintcursors;
  bool use_secondary_tex;
  void *cursor;
  NumInput num_input;
  int init_event;
} RadialControl;

static void radial_control_update_header(wmOperator *op, bContext *C)
{
  RadialControl *rc = op->customdata;
  char msg[UI_MAX_DRAW_STR];
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);

  if (hasNumInput(&rc->num_input)) {
    char num_str[NUM_STR_REP_LEN];
    outputNumInput(&rc->num_input, num_str, &scene->unit);
    BLI_snprintf(msg, sizeof(msg), "%s: %s", RNA_property_ui_name(rc->prop), num_str);
  }
  else {
    const char *ui_name = RNA_property_ui_name(rc->prop);
    switch (rc->subtype) {
      case PROP_NONE:
      case PROP_DISTANCE:
        BLI_snprintf(msg, sizeof(msg), "%s: %0.4f", ui_name, rc->current_value);
        break;
      case PROP_PIXEL:
        BLI_snprintf(msg,
                     sizeof(msg),
                     "%s: %d",
                     ui_name,
                     (int)rc->current_value); /* XXX: round to nearest? */
        break;
      case PROP_PERCENTAGE:
        BLI_snprintf(msg, sizeof(msg), "%s: %3.1f%%", ui_name, rc->current_value);
        break;
      case PROP_FACTOR:
        BLI_snprintf(msg, sizeof(msg), "%s: %1.3f", ui_name, rc->current_value);
        break;
      case PROP_ANGLE:
        BLI_snprintf(msg, sizeof(msg), "%s: %3.2f", ui_name, RAD2DEGF(rc->current_value));
        break;
      default:
        BLI_snprintf(msg, sizeof(msg), "%s", ui_name); /* XXX: No value? */
        break;
    }
  }

  ED_area_status_text(area, msg);
}

static void radial_control_set_initial_mouse(RadialControl *rc, const wmEvent *event)
{
  float d[2] = {0, 0};
  float zoom[2] = {1, 1};

  copy_v2_v2_int(rc->initial_mouse, event->xy);
  copy_v2_v2_int(rc->initial_co, event->xy);

  switch (rc->subtype) {
    case PROP_NONE:
    case PROP_DISTANCE:
    case PROP_PIXEL:
      d[0] = rc->initial_value;
      break;
    case PROP_PERCENTAGE:
      d[0] = (rc->initial_value) / 100.0f * WM_RADIAL_CONTROL_DISPLAY_WIDTH +
             WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      break;
    case PROP_FACTOR:
      d[0] = rc->initial_value * WM_RADIAL_CONTROL_DISPLAY_WIDTH +
             WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      break;
    case PROP_ANGLE:
      d[0] = WM_RADIAL_CONTROL_DISPLAY_SIZE * cosf(rc->initial_value);
      d[1] = WM_RADIAL_CONTROL_DISPLAY_SIZE * sinf(rc->initial_value);
      break;
    default:
      return;
  }

  if (rc->zoom_prop) {
    RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
    d[0] *= zoom[0];
    d[1] *= zoom[1];
  }

  rc->initial_mouse[0] -= d[0];
  rc->initial_mouse[1] -= d[1];
}

static void radial_control_set_tex(RadialControl *rc)
{
  ImBuf *ibuf;

  switch (RNA_type_to_ID_code(rc->image_id_ptr.type)) {
    case ID_BR:
      if ((ibuf = BKE_brush_gen_radial_control_imbuf(
               rc->image_id_ptr.data,
               rc->use_secondary_tex,
               !ELEM(rc->subtype, PROP_NONE, PROP_PIXEL, PROP_DISTANCE)))) {

        rc->texture = GPU_texture_create_2d(
            "radial_control", ibuf->x, ibuf->y, 1, GPU_R8, ibuf->rect_float);

        GPU_texture_filter_mode(rc->texture, true);
        GPU_texture_swizzle_set(rc->texture, "111r");

        MEM_freeN(ibuf->rect_float);
        MEM_freeN(ibuf);
      }
      break;
    default:
      break;
  }
}

static void radial_control_paint_tex(RadialControl *rc, float radius, float alpha)
{

  /* set fill color */
  float col[3] = {0, 0, 0};
  if (rc->fill_col_prop) {
    PointerRNA *fill_ptr;
    PropertyRNA *fill_prop;

    if (rc->fill_col_override_prop && RNA_property_boolean_get(&rc->fill_col_override_test_ptr,
                                                               rc->fill_col_override_test_prop)) {
      fill_ptr = &rc->fill_col_override_ptr;
      fill_prop = rc->fill_col_override_prop;
    }
    else {
      fill_ptr = &rc->fill_col_ptr;
      fill_prop = rc->fill_col_prop;
    }

    RNA_property_float_get_array(fill_ptr, fill_prop, col);
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  if (rc->texture) {
    uint texCoord = GPU_vertformat_attr_add(format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    /* set up rotation if available */
    if (rc->rot_prop) {
      float rot = RNA_property_float_get(&rc->rot_ptr, rc->rot_prop);
      GPU_matrix_push();
      GPU_matrix_rotate_2d(RAD2DEGF(rot));
    }

    immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_COLOR);

    immUniformColor3fvAlpha(col, alpha);
    immBindTexture("image", rc->texture);

    /* draw textured quad */
    immBegin(GPU_PRIM_TRI_FAN, 4);

    immAttr2f(texCoord, 0, 0);
    immVertex2f(pos, -radius, -radius);

    immAttr2f(texCoord, 1, 0);
    immVertex2f(pos, radius, -radius);

    immAttr2f(texCoord, 1, 1);
    immVertex2f(pos, radius, radius);

    immAttr2f(texCoord, 0, 1);
    immVertex2f(pos, -radius, radius);

    immEnd();

    GPU_texture_unbind(rc->texture);

    /* undo rotation */
    if (rc->rot_prop) {
      GPU_matrix_pop();
    }
  }
  else {
    /* flat color if no texture available */
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformColor3fvAlpha(col, alpha);
    imm_draw_circle_fill_2d(pos, 0.0f, 0.0f, radius, 40);
  }

  immUnbindProgram();
}

static void radial_control_paint_curve(uint pos, Brush *br, float radius, int line_segments)
{
  GPU_line_width(2.0f);
  immUniformColor4f(0.8f, 0.8f, 0.8f, 0.85f);
  float step = (radius * 2.0f) / (float)line_segments;
  BKE_curvemapping_init(br->curve);
  immBegin(GPU_PRIM_LINES, line_segments * 2);
  for (int i = 0; i < line_segments; i++) {
    float h1 = BKE_brush_curve_strength_clamped(br, fabsf((i * step) - radius), radius);
    immVertex2f(pos, -radius + (i * step), h1 * radius);
    float h2 = BKE_brush_curve_strength_clamped(br, fabsf(((i + 1) * step) - radius), radius);
    immVertex2f(pos, -radius + ((i + 1) * step), h2 * radius);
  }
  immEnd();
}

static void radial_control_paint_cursor(bContext *UNUSED(C), int x, int y, void *customdata)
{
  RadialControl *rc = customdata;
  const uiStyle *style = UI_style_get();
  const uiFontStyle *fstyle = &style->widget;
  const int fontid = fstyle->uifont_id;
  short fstyle_points = fstyle->points;
  char str[WM_RADIAL_MAX_STR];
  short strdrawlen = 0;
  float strwidth, strheight;
  float r1 = 0.0f, r2 = 0.0f, rmin = 0.0, tex_radius, alpha;
  float zoom[2], col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float text_color[4];

  switch (rc->subtype) {
    case PROP_NONE:
    case PROP_DISTANCE:
    case PROP_PIXEL:
      r1 = rc->current_value;
      r2 = rc->initial_value;
      tex_radius = r1;
      alpha = 0.75;
      break;
    case PROP_PERCENTAGE:
      r1 = rc->current_value / 100.0f * WM_RADIAL_CONTROL_DISPLAY_WIDTH +
           WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
      rmin = WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      BLI_snprintf(str, WM_RADIAL_MAX_STR, "%3.1f%%", rc->current_value);
      strdrawlen = BLI_strlen_utf8(str);
      tex_radius = r1;
      alpha = 0.75;
      break;
    case PROP_FACTOR:
      r1 = rc->current_value * WM_RADIAL_CONTROL_DISPLAY_WIDTH +
           WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
      rmin = WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      alpha = rc->current_value / 2.0f + 0.5f;
      BLI_snprintf(str, WM_RADIAL_MAX_STR, "%1.3f", rc->current_value);
      strdrawlen = BLI_strlen_utf8(str);
      break;
    case PROP_ANGLE:
      r1 = r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
      alpha = 0.75;
      rmin = WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      BLI_snprintf(str, WM_RADIAL_MAX_STR, "%3.2f", RAD2DEGF(rc->current_value));
      strdrawlen = BLI_strlen_utf8(str);
      break;
    default:
      tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE; /* NOTE: this is a dummy value. */
      alpha = 0.75;
      break;
  }

  if (rc->subtype == PROP_ANGLE) {
    /* Use the initial mouse position to draw the rotation preview. This avoids starting the
     * rotation in a random direction */
    x = rc->initial_mouse[0];
    y = rc->initial_mouse[1];
  }
  else {
    /* Keep cursor in the original place */
    x = rc->initial_co[0];
    y = rc->initial_co[1];
  }
  GPU_matrix_translate_2f((float)x, (float)y);

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);

  /* apply zoom if available */
  if (rc->zoom_prop) {
    RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
    GPU_matrix_scale_2fv(zoom);
  }

  /* draw rotated texture */
  radial_control_paint_tex(rc, tex_radius, alpha);

  /* set line color */
  if (rc->col_prop) {
    RNA_property_float_get_array(&rc->col_ptr, rc->col_prop, col);
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  if (rc->subtype == PROP_ANGLE) {
    GPU_matrix_push();

    /* draw original angle line */
    GPU_matrix_rotate_3f(RAD2DEGF(rc->initial_value), 0.0f, 0.0f, 1.0f);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, (float)WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE, 0.0f);
    immVertex2f(pos, (float)WM_RADIAL_CONTROL_DISPLAY_SIZE, 0.0f);
    immEnd();

    /* draw new angle line */
    GPU_matrix_rotate_3f(RAD2DEGF(rc->current_value - rc->initial_value), 0.0f, 0.0f, 1.0f);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, (float)WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE, 0.0f);
    immVertex2f(pos, (float)WM_RADIAL_CONTROL_DISPLAY_SIZE, 0.0f);
    immEnd();

    GPU_matrix_pop();
  }

  /* draw circles on top */
  GPU_line_width(2.0f);
  immUniformColor3fvAlpha(col, 0.8f);
  imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, r1, 80);

  GPU_line_width(1.0f);
  immUniformColor3fvAlpha(col, 0.5f);
  imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, r2, 80);
  if (rmin > 0.0f) {
    /* Inner fill circle to increase the contrast of the value */
    const float black[3] = {0.0f};
    immUniformColor3fvAlpha(black, 0.2f);
    imm_draw_circle_fill_2d(pos, 0.0, 0.0f, rmin, 80);

    immUniformColor3fvAlpha(col, 0.5f);
    imm_draw_circle_wire_2d(pos, 0.0, 0.0f, rmin, 80);
  }

  /* draw curve falloff preview */
  if (RNA_type_to_ID_code(rc->image_id_ptr.type) == ID_BR && rc->subtype == PROP_FACTOR) {
    Brush *br = rc->image_id_ptr.data;
    if (br) {
      radial_control_paint_curve(pos, br, r2, 120);
    }
  }

  immUnbindProgram();

  BLF_size(fontid, 1.75f * fstyle_points * U.pixelsize, U.dpi);
  UI_GetThemeColor4fv(TH_TEXT_HI, text_color);
  BLF_color4fv(fontid, text_color);

  /* draw value */
  BLF_width_and_height(fontid, str, strdrawlen, &strwidth, &strheight);
  BLF_position(fontid, -0.5f * strwidth, -0.5f * strheight, 0.0f);
  BLF_draw(fontid, str, strdrawlen);

  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

typedef enum {
  RC_PROP_ALLOW_MISSING = 1,
  RC_PROP_REQUIRE_FLOAT = 2,
  RC_PROP_REQUIRE_BOOL = 4,
} RCPropFlags;

/**
 * Attempt to retrieve the rna pointer/property from an rna path.
 *
 * \return 0 for failure, 1 for success, and also 1 if property is not set.
 */
static int radial_control_get_path(PointerRNA *ctx_ptr,
                                   wmOperator *op,
                                   const char *name,
                                   PointerRNA *r_ptr,
                                   PropertyRNA **r_prop,
                                   int req_length,
                                   RCPropFlags flags)
{
  PropertyRNA *unused_prop;

  /* check flags */
  if ((flags & RC_PROP_REQUIRE_BOOL) && (flags & RC_PROP_REQUIRE_FLOAT)) {
    BKE_report(op->reports, RPT_ERROR, "Property cannot be both boolean and float");
    return 0;
  }

  /* get an rna string path from the operator's properties */
  char *str;
  if (!(str = RNA_string_get_alloc(op->ptr, name, NULL, 0, NULL))) {
    return 1;
  }

  if (str[0] == '\0') {
    if (r_prop) {
      *r_prop = NULL;
    }
    MEM_freeN(str);
    return 1;
  }

  if (!r_prop) {
    r_prop = &unused_prop;
  }

  /* get rna from path */
  if (!RNA_path_resolve(ctx_ptr, str, r_ptr, r_prop)) {
    MEM_freeN(str);
    if (flags & RC_PROP_ALLOW_MISSING) {
      return 1;
    }
    BKE_reportf(op->reports, RPT_ERROR, "Could not resolve path '%s'", name);
    return 0;
  }

  /* check property type */
  if (flags & (RC_PROP_REQUIRE_BOOL | RC_PROP_REQUIRE_FLOAT)) {
    PropertyType prop_type = RNA_property_type(*r_prop);

    if (((flags & RC_PROP_REQUIRE_BOOL) && (prop_type != PROP_BOOLEAN)) ||
        ((flags & RC_PROP_REQUIRE_FLOAT) && (prop_type != PROP_FLOAT))) {
      MEM_freeN(str);
      BKE_reportf(op->reports, RPT_ERROR, "Property from path '%s' is not a float", name);
      return 0;
    }
  }

  /* check property's array length */
  int len;
  if (*r_prop && (len = RNA_property_array_length(r_ptr, *r_prop)) != req_length) {
    MEM_freeN(str);
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Property from path '%s' has length %d instead of %d",
                name,
                len,
                req_length);
    return 0;
  }

  /* success */
  MEM_freeN(str);
  return 1;
}

/* initialize the rna pointers and properties using rna paths */
static int radial_control_get_properties(bContext *C, wmOperator *op)
{
  RadialControl *rc = op->customdata;

  PointerRNA ctx_ptr;
  RNA_pointer_create(NULL, &RNA_Context, C, &ctx_ptr);

  /* check if we use primary or secondary path */
  PointerRNA use_secondary_ptr;
  PropertyRNA *use_secondary_prop = NULL;
  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "use_secondary",
                               &use_secondary_ptr,
                               &use_secondary_prop,
                               0,
                               (RC_PROP_ALLOW_MISSING | RC_PROP_REQUIRE_BOOL))) {
    return 0;
  }

  const char *data_path;
  if (use_secondary_prop && RNA_property_boolean_get(&use_secondary_ptr, use_secondary_prop)) {
    data_path = "data_path_secondary";
  }
  else {
    data_path = "data_path_primary";
  }

  if (!radial_control_get_path(&ctx_ptr, op, data_path, &rc->ptr, &rc->prop, 0, 0)) {
    return 0;
  }

  /* data path is required */
  if (!rc->prop) {
    return 0;
  }

  if (!radial_control_get_path(
          &ctx_ptr, op, "rotation_path", &rc->rot_ptr, &rc->rot_prop, 0, RC_PROP_REQUIRE_FLOAT)) {
    return 0;
  }

  if (!radial_control_get_path(
          &ctx_ptr, op, "color_path", &rc->col_ptr, &rc->col_prop, 4, RC_PROP_REQUIRE_FLOAT)) {
    return 0;
  }

  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "fill_color_path",
                               &rc->fill_col_ptr,
                               &rc->fill_col_prop,
                               3,
                               RC_PROP_REQUIRE_FLOAT)) {
    return 0;
  }

  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "fill_color_override_path",
                               &rc->fill_col_override_ptr,
                               &rc->fill_col_override_prop,
                               3,
                               RC_PROP_REQUIRE_FLOAT)) {
    return 0;
  }
  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "fill_color_override_test_path",
                               &rc->fill_col_override_test_ptr,
                               &rc->fill_col_override_test_prop,
                               0,
                               RC_PROP_REQUIRE_BOOL)) {
    return 0;
  }

  /* slightly ugly; allow this property to not resolve
   * correctly. needed because 3d texture paint shares the same
   * keymap as 2d image paint */
  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "zoom_path",
                               &rc->zoom_ptr,
                               &rc->zoom_prop,
                               2,
                               RC_PROP_REQUIRE_FLOAT | RC_PROP_ALLOW_MISSING)) {
    return 0;
  }

  if (!radial_control_get_path(&ctx_ptr, op, "image_id", &rc->image_id_ptr, NULL, 0, 0)) {
    return 0;
  }
  if (rc->image_id_ptr.data) {
    /* extra check, pointer must be to an ID */
    if (!RNA_struct_is_ID(rc->image_id_ptr.type)) {
      BKE_report(op->reports, RPT_ERROR, "Pointer from path image_id is not an ID");
      return 0;
    }
  }

  rc->use_secondary_tex = RNA_boolean_get(op->ptr, "secondary_tex");

  return 1;
}

static int radial_control_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindowManager *wm;
  RadialControl *rc;

  if (!(op->customdata = rc = MEM_callocN(sizeof(RadialControl), "RadialControl"))) {
    return OPERATOR_CANCELLED;
  }

  if (!radial_control_get_properties(C, op)) {
    MEM_freeN(rc);
    return OPERATOR_CANCELLED;
  }

  /* get type, initial, min, and max values of the property */
  switch ((rc->type = RNA_property_type(rc->prop))) {
    case PROP_INT: {
      int value, min, max, step;

      value = RNA_property_int_get(&rc->ptr, rc->prop);
      RNA_property_int_ui_range(&rc->ptr, rc->prop, &min, &max, &step);

      rc->initial_value = value;
      rc->min_value = min_ii(value, min);
      rc->max_value = max_ii(value, max);
      break;
    }
    case PROP_FLOAT: {
      float value, min, max, step, precision;

      value = RNA_property_float_get(&rc->ptr, rc->prop);
      RNA_property_float_ui_range(&rc->ptr, rc->prop, &min, &max, &step, &precision);

      rc->initial_value = value;
      rc->min_value = min_ff(value, min);
      rc->max_value = max_ff(value, max);
      break;
    }
    default:
      BKE_report(op->reports, RPT_ERROR, "Property must be an integer or a float");
      MEM_freeN(rc);
      return OPERATOR_CANCELLED;
  }

  /* initialize numerical input */
  initNumInput(&rc->num_input);
  rc->num_input.idx_max = 0;
  rc->num_input.val_flag[0] |= NUM_NO_NEGATIVE;
  rc->num_input.unit_sys = USER_UNIT_NONE;
  rc->num_input.unit_type[0] = RNA_SUBTYPE_UNIT_VALUE(RNA_property_unit(rc->prop));

  /* get subtype of property */
  rc->subtype = RNA_property_subtype(rc->prop);
  if (!ELEM(rc->subtype,
            PROP_NONE,
            PROP_DISTANCE,
            PROP_FACTOR,
            PROP_PERCENTAGE,
            PROP_ANGLE,
            PROP_PIXEL)) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Property must be a none, distance, factor, percentage, angle, or pixel");
    MEM_freeN(rc);
    return OPERATOR_CANCELLED;
  }

  rc->current_value = rc->initial_value;
  radial_control_set_initial_mouse(rc, event);
  radial_control_set_tex(rc);

  rc->init_event = WM_userdef_event_type_from_keymap_type(event->type);

  /* temporarily disable other paint cursors */
  wm = CTX_wm_manager(C);
  rc->orig_paintcursors = wm->paintcursors;
  BLI_listbase_clear(&wm->paintcursors);

  /* add radial control paint cursor */
  rc->cursor = WM_paint_cursor_activate(
      SPACE_TYPE_ANY, RGN_TYPE_ANY, op->type->poll, radial_control_paint_cursor, rc);

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void radial_control_set_value(RadialControl *rc, float val)
{
  switch (rc->type) {
    case PROP_INT:
      RNA_property_int_set(&rc->ptr, rc->prop, val);
      break;
    case PROP_FLOAT:
      RNA_property_float_set(&rc->ptr, rc->prop, val);
      break;
    default:
      break;
  }
}

static void radial_control_cancel(bContext *C, wmOperator *op)
{
  RadialControl *rc = op->customdata;
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area = CTX_wm_area(C);

  MEM_SAFE_FREE(rc->dial);

  ED_area_status_text(area, NULL);

  WM_paint_cursor_end(rc->cursor);

  /* restore original paint cursors */
  wm->paintcursors = rc->orig_paintcursors;

  /* not sure if this is a good notifier to use;
   * intended purpose is to update the UI so that the
   * new value is displayed in sliders/numfields */
  WM_event_add_notifier(C, NC_WINDOW, NULL);

  if (rc->texture != NULL) {
    GPU_texture_free(rc->texture);
  }

  MEM_freeN(rc);
}

static int radial_control_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  RadialControl *rc = op->customdata;
  float new_value, dist = 0.0f, zoom[2];
  float delta[2];
  int ret = OPERATOR_RUNNING_MODAL;
  float angle_precision = 0.0f;
  const bool has_numInput = hasNumInput(&rc->num_input);
  bool handled = false;
  float numValue;
  /* TODO: fix hardcoded events */

  bool snap = (event->modifier & KM_CTRL) != 0;

  /* Modal numinput active, try to handle numeric inputs first... */
  if (event->val == KM_PRESS && has_numInput && handleNumInput(C, &rc->num_input, event)) {
    handled = true;
    applyNumInput(&rc->num_input, &numValue);

    if (rc->subtype == PROP_ANGLE) {
      numValue = fmod(numValue, 2.0f * (float)M_PI);
      if (numValue < 0.0f) {
        numValue += 2.0f * (float)M_PI;
      }
    }

    CLAMP(numValue, rc->min_value, rc->max_value);
    new_value = numValue;

    radial_control_set_value(rc, new_value);
    rc->current_value = new_value;
    radial_control_update_header(op, C);
    return OPERATOR_RUNNING_MODAL;
  }

  handled = false;
  switch (event->type) {
    case EVT_ESCKEY:
    case RIGHTMOUSE:
      /* canceled; restore original value */
      radial_control_set_value(rc, rc->initial_value);
      ret = OPERATOR_CANCELLED;
      break;

    case LEFTMOUSE:
    case EVT_PADENTER:
    case EVT_RETKEY:
      /* done; value already set */
      RNA_property_update(C, &rc->ptr, rc->prop);
      ret = OPERATOR_FINISHED;
      break;

    case MOUSEMOVE:
      if (!has_numInput) {
        if (rc->slow_mode) {
          if (rc->subtype == PROP_ANGLE) {
            /* calculate the initial angle here first */
            delta[0] = rc->initial_mouse[0] - rc->slow_mouse[0];
            delta[1] = rc->initial_mouse[1] - rc->slow_mouse[1];

            /* precision angle gets calculated from dial and gets added later */
            angle_precision = -0.1f * BLI_dial_angle(rc->dial, (float[2]){UNPACK2(event->xy)});
          }
          else {
            delta[0] = rc->initial_mouse[0] - rc->slow_mouse[0];
            delta[1] = 0.0f;

            if (rc->zoom_prop) {
              RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
              delta[0] /= zoom[0];
            }

            dist = len_v2(delta);

            delta[0] = event->xy[0] - rc->slow_mouse[0];

            if (rc->zoom_prop) {
              delta[0] /= zoom[0];
            }

            dist = dist + 0.1f * (delta[0]);
          }
        }
        else {
          delta[0] = (float)(rc->initial_mouse[0] - event->xy[0]);
          delta[1] = (float)(rc->initial_mouse[1] - event->xy[1]);
          if (rc->zoom_prop) {
            RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
            delta[0] /= zoom[0];
            delta[1] /= zoom[1];
          }
          if (rc->subtype == PROP_ANGLE) {
            dist = len_v2(delta);
          }
          else {
            dist = clamp_f(-delta[0], 0.0f, FLT_MAX);
          }
        }

        /* Calculate new value and apply snapping. */
        switch (rc->subtype) {
          case PROP_NONE:
          case PROP_DISTANCE:
          case PROP_PIXEL:
            new_value = dist;
            if (snap) {
              new_value = ((int)new_value + 5) / 10 * 10;
            }
            break;
          case PROP_PERCENTAGE:
            new_value = ((dist - WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE) /
                         WM_RADIAL_CONTROL_DISPLAY_WIDTH) *
                        100.0f;
            if (snap) {
              new_value = ((int)(new_value + 2.5f)) / 5 * 5;
            }
            break;
          case PROP_FACTOR:
            new_value = (WM_RADIAL_CONTROL_DISPLAY_SIZE - dist) / WM_RADIAL_CONTROL_DISPLAY_WIDTH;
            if (snap) {
              new_value = ((int)ceil(new_value * 10.0f) * 10.0f) / 100.0f;
            }
            /* Invert new value to increase the factor moving the mouse to the right */
            new_value = 1 - new_value;
            break;
          case PROP_ANGLE:
            new_value = atan2f(delta[1], delta[0]) + (float)M_PI + angle_precision;
            new_value = fmod(new_value, 2.0f * (float)M_PI);
            if (new_value < 0.0f) {
              new_value += 2.0f * (float)M_PI;
            }
            if (snap) {
              new_value = DEG2RADF(((int)RAD2DEGF(new_value) + 5) / 10 * 10);
            }
            break;
          default:
            new_value = dist; /* dummy value, should this ever happen? - campbell */
            break;
        }

        /* clamp and update */
        CLAMP(new_value, rc->min_value, rc->max_value);
        radial_control_set_value(rc, new_value);
        rc->current_value = new_value;
        handled = true;
        break;
      }
      break;

    case EVT_LEFTSHIFTKEY:
    case EVT_RIGHTSHIFTKEY: {
      if (event->val == KM_PRESS) {
        rc->slow_mouse[0] = event->xy[0];
        rc->slow_mouse[1] = event->xy[1];
        rc->slow_mode = true;
        if (rc->subtype == PROP_ANGLE) {
          const float initial_position[2] = {UNPACK2(rc->initial_mouse)};
          const float current_position[2] = {UNPACK2(rc->slow_mouse)};
          rc->dial = BLI_dial_init(initial_position, 0.0f);
          /* immediately set the position to get a an initial direction */
          BLI_dial_angle(rc->dial, current_position);
        }
        handled = true;
      }
      if (event->val == KM_RELEASE) {
        rc->slow_mode = false;
        handled = true;
        MEM_SAFE_FREE(rc->dial);
      }
      break;
    }
  }

  /* Modal numinput inactive, try to handle numeric inputs last... */
  if (!handled && event->val == KM_PRESS && handleNumInput(C, &rc->num_input, event)) {
    applyNumInput(&rc->num_input, &numValue);

    if (rc->subtype == PROP_ANGLE) {
      numValue = fmod(numValue, 2.0f * (float)M_PI);
      if (numValue < 0.0f) {
        numValue += 2.0f * (float)M_PI;
      }
    }

    CLAMP(numValue, rc->min_value, rc->max_value);
    new_value = numValue;

    radial_control_set_value(rc, new_value);

    rc->current_value = new_value;
    radial_control_update_header(op, C);
    return OPERATOR_RUNNING_MODAL;
  }

  if (!handled && (event->val == KM_RELEASE) && (rc->init_event == event->type) &&
      RNA_boolean_get(op->ptr, "release_confirm")) {
    ret = OPERATOR_FINISHED;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
  radial_control_update_header(op, C);

  if (ret & OPERATOR_FINISHED) {
    wmWindowManager *wm = CTX_wm_manager(C);
    if (wm->op_undo_depth == 0) {
      ID *id = rc->ptr.owner_id;
      if (ED_undo_is_legacy_compatible_for_property(C, id)) {
        ED_undo_push(C, op->type->name);
      }
    }
  }

  if (ret != OPERATOR_RUNNING_MODAL) {
    radial_control_cancel(C, op);
  }

  return ret;
}

static void WM_OT_radial_control(wmOperatorType *ot)
{
  ot->name = "Radial Control";
  ot->idname = "WM_OT_radial_control";
  ot->description = "Set some size property (e.g. brush size) with mouse wheel";

  ot->invoke = radial_control_invoke;
  ot->modal = radial_control_modal;
  ot->cancel = radial_control_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_BLOCKING;

  /* all paths relative to the context */
  PropertyRNA *prop;
  prop = RNA_def_string(ot->srna,
                        "data_path_primary",
                        NULL,
                        0,
                        "Primary Data Path",
                        "Primary path of property to be set by the radial control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "data_path_secondary",
                        NULL,
                        0,
                        "Secondary Data Path",
                        "Secondary path of property to be set by the radial control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "use_secondary",
                        NULL,
                        0,
                        "Use Secondary",
                        "Path of property to select between the primary and secondary data paths");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "rotation_path",
                        NULL,
                        0,
                        "Rotation Path",
                        "Path of property used to rotate the texture display");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "color_path",
                        NULL,
                        0,
                        "Color Path",
                        "Path of property used to set the color of the control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "fill_color_path",
                        NULL,
                        0,
                        "Fill Color Path",
                        "Path of property used to set the fill color of the control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(
      ot->srna, "fill_color_override_path", NULL, 0, "Fill Color Override Path", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_string(
      ot->srna, "fill_color_override_test_path", NULL, 0, "Fill Color Override Test", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "zoom_path",
                        NULL,
                        0,
                        "Zoom Path",
                        "Path of property used to set the zoom level for the control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "image_id",
                        NULL,
                        0,
                        "Image ID",
                        "Path of ID that is used to generate an image for the control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "secondary_tex", false, "Secondary Texture", "Tweak brush secondary/mask texture");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "release_confirm", false, "Confirm On Release", "Finish operation on key release");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Redraw Timer Operator
 *
 * Use for simple benchmarks.
 * \{ */

/* uses no type defines, fully local testing function anyway... ;) */

static void redraw_timer_window_swap(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);

  CTX_wm_menu_set(C, NULL);

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    ED_area_tag_redraw(area);
  }
  wm_draw_update(C);

  CTX_wm_window_set(C, win); /* XXX context manipulation warning! */
}

enum {
  eRTDrawRegion = 0,
  eRTDrawRegionSwap = 1,
  eRTDrawWindow = 2,
  eRTDrawWindowSwap = 3,
  eRTAnimationStep = 4,
  eRTAnimationPlay = 5,
  eRTUndo = 6,
};

static const EnumPropertyItem redraw_timer_type_items[] = {
    {eRTDrawRegion, "DRAW", 0, "Draw Region", "Draw region"},
    {eRTDrawRegionSwap, "DRAW_SWAP", 0, "Draw Region & Swap", "Draw region and swap"},
    {eRTDrawWindow, "DRAW_WIN", 0, "Draw Window", "Draw window"},
    {eRTDrawWindowSwap, "DRAW_WIN_SWAP", 0, "Draw Window & Swap", "Draw window and swap"},
    {eRTAnimationStep, "ANIM_STEP", 0, "Animation Step", "Animation steps"},
    {eRTAnimationPlay, "ANIM_PLAY", 0, "Animation Play", "Animation playback"},
    {eRTUndo, "UNDO", 0, "Undo/Redo", "Undo and redo"},
    {0, NULL, 0, NULL, NULL},
};

static void redraw_timer_step(bContext *C,
                              Scene *scene,
                              struct Depsgraph *depsgraph,
                              wmWindow *win,
                              ScrArea *area,
                              ARegion *region,
                              const int type,
                              const int cfra)
{
  if (type == eRTDrawRegion) {
    if (region) {
      wm_draw_region_test(C, area, region);
    }
  }
  else if (type == eRTDrawRegionSwap) {
    CTX_wm_menu_set(C, NULL);

    ED_region_tag_redraw(region);
    wm_draw_update(C);

    CTX_wm_window_set(C, win); /* XXX context manipulation warning! */
  }
  else if (type == eRTDrawWindow) {
    bScreen *screen = WM_window_get_active_screen(win);

    CTX_wm_menu_set(C, NULL);

    LISTBASE_FOREACH (ScrArea *, area_iter, &screen->areabase) {
      CTX_wm_area_set(C, area_iter);
      LISTBASE_FOREACH (ARegion *, region_iter, &area_iter->regionbase) {
        if (!region_iter->visible) {
          continue;
        }
        CTX_wm_region_set(C, region_iter);
        wm_draw_region_test(C, area_iter, region_iter);
      }
    }

    CTX_wm_window_set(C, win); /* XXX context manipulation warning! */

    CTX_wm_area_set(C, area);
    CTX_wm_region_set(C, region);
  }
  else if (type == eRTDrawWindowSwap) {
    redraw_timer_window_swap(C);
  }
  else if (type == eRTAnimationStep) {
    scene->r.cfra += (cfra == scene->r.cfra) ? 1 : -1;
    BKE_scene_graph_update_for_newframe(depsgraph);
  }
  else if (type == eRTAnimationPlay) {
    /* play anim, return on same frame as started with */
    int tot = (scene->r.efra - scene->r.sfra) + 1;

    while (tot--) {
      /* TODO: ability to escape! */
      scene->r.cfra++;
      if (scene->r.cfra > scene->r.efra) {
        scene->r.cfra = scene->r.sfra;
      }

      BKE_scene_graph_update_for_newframe(depsgraph);
      redraw_timer_window_swap(C);
    }
  }
  else { /* eRTUndo */
    /* Undo and redo, including depsgraph update since that can be a
     * significant part of the cost. */
    ED_undo_pop(C);
    wm_event_do_refresh_wm_and_depsgraph(C);
    ED_undo_redo(C);
    wm_event_do_refresh_wm_and_depsgraph(C);
  }
}

static bool redraw_timer_poll(bContext *C)
{
  /* Check background mode as many of these actions use redrawing.
   * NOTE(@campbellbarton): if it's useful to support undo or animation step this could
   * be allowed at the moment this seems like a corner case that isn't needed. */
  return !G.background && WM_operator_winactive(C);
}

static int redraw_timer_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  const int type = RNA_enum_get(op->ptr, "type");
  const int iter = RNA_int_get(op->ptr, "iterations");
  const double time_limit = (double)RNA_float_get(op->ptr, "time_limit");
  const int cfra = scene->r.cfra;
  const char *infostr = "";

  /* NOTE: Depsgraph is used to update scene for a new state, so no need to ensure evaluation here.
   */
  struct Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  WM_cursor_wait(true);

  double time_start = PIL_check_seconds_timer();

  wm_window_make_drawable(wm, win);

  int iter_steps = 0;
  for (int a = 0; a < iter; a++) {
    redraw_timer_step(C, scene, depsgraph, win, area, region, type, cfra);
    iter_steps += 1;

    if (time_limit != 0.0) {
      if ((PIL_check_seconds_timer() - time_start) > time_limit) {
        break;
      }
      a = 0;
    }
  }

  double time_delta = (PIL_check_seconds_timer() - time_start) * 1000;

  RNA_enum_description(redraw_timer_type_items, type, &infostr);

  WM_cursor_wait(false);

  BKE_reportf(op->reports,
              RPT_WARNING,
              "%d x %s: %.4f ms, average: %.8f ms",
              iter_steps,
              infostr,
              time_delta,
              time_delta / iter_steps);

  return OPERATOR_FINISHED;
}

static void WM_OT_redraw_timer(wmOperatorType *ot)
{
  ot->name = "Redraw Timer";
  ot->idname = "WM_OT_redraw_timer";
  ot->description = "Simple redraw timer to test the speed of updating the interface";

  ot->invoke = WM_menu_invoke;
  ot->exec = redraw_timer_exec;
  ot->poll = redraw_timer_poll;

  ot->prop = RNA_def_enum(ot->srna, "type", redraw_timer_type_items, eRTDrawRegion, "Type", "");
  RNA_def_int(
      ot->srna, "iterations", 10, 1, INT_MAX, "Iterations", "Number of times to redraw", 1, 1000);
  RNA_def_float(ot->srna,
                "time_limit",
                0.0,
                0.0,
                FLT_MAX,
                "Time Limit",
                "Seconds to run the test for (override iterations)",
                0.0,
                60.0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Report Memory Statistics
 *
 * Use for testing/debugging.
 * \{ */

static int memory_statistics_exec(bContext *UNUSED(C), wmOperator *UNUSED(op))
{
  MEM_printmemlist_stats();
  return OPERATOR_FINISHED;
}

static void WM_OT_memory_statistics(wmOperatorType *ot)
{
  ot->name = "Memory Statistics";
  ot->idname = "WM_OT_memory_statistics";
  ot->description = "Print memory statistics to the console";

  ot->exec = memory_statistics_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data-Block Preview Generation Operator
 *
 * Use for material/texture/light ... etc.
 * \{ */

typedef struct PreviewsIDEnsureData {
  bContext *C;
  Scene *scene;
} PreviewsIDEnsureData;

static void previews_id_ensure(bContext *C, Scene *scene, ID *id)
{
  BLI_assert(ELEM(GS(id->name), ID_MA, ID_TE, ID_IM, ID_WO, ID_LA));

  /* Only preview non-library datablocks, lib ones do not pertain to this .blend file!
   * Same goes for ID with no user. */
  if (!ID_IS_LINKED(id) && (id->us != 0)) {
    UI_icon_render_id(C, scene, id, ICON_SIZE_ICON, false);
    UI_icon_render_id(C, scene, id, ICON_SIZE_PREVIEW, false);
  }
}

static int previews_id_ensure_callback(LibraryIDLinkCallbackData *cb_data)
{
  const int cb_flag = cb_data->cb_flag;

  if (cb_flag & IDWALK_CB_EMBEDDED) {
    return IDWALK_RET_NOP;
  }

  PreviewsIDEnsureData *data = cb_data->user_data;
  ID *id = *cb_data->id_pointer;

  if (id && (id->tag & LIB_TAG_DOIT)) {
    BLI_assert(ELEM(GS(id->name), ID_MA, ID_TE, ID_IM, ID_WO, ID_LA));
    previews_id_ensure(data->C, data->scene, id);
    id->tag &= ~LIB_TAG_DOIT;
  }

  return IDWALK_RET_NOP;
}

static int previews_ensure_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  ListBase *lb[] = {
      &bmain->materials, &bmain->textures, &bmain->images, &bmain->worlds, &bmain->lights, NULL};
  PreviewsIDEnsureData preview_id_data;

  /* We use LIB_TAG_DOIT to check whether we have already handled a given ID or not. */
  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  for (int i = 0; lb[i]; i++) {
    BKE_main_id_tag_listbase(lb[i], LIB_TAG_DOIT, true);
  }

  preview_id_data.C = C;
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    preview_id_data.scene = scene;
    ID *id = (ID *)scene;

    BKE_library_foreach_ID_link(
        NULL, id, previews_id_ensure_callback, &preview_id_data, IDWALK_RECURSE);
  }

  /* Check a last time for ID not used (fake users only, in theory), and
   * do our best for those, using current scene... */
  for (int i = 0; lb[i]; i++) {
    LISTBASE_FOREACH (ID *, id, lb[i]) {
      if (id->tag & LIB_TAG_DOIT) {
        previews_id_ensure(C, NULL, id);
        id->tag &= ~LIB_TAG_DOIT;
      }
    }
  }

  return OPERATOR_FINISHED;
}

static void WM_OT_previews_ensure(wmOperatorType *ot)
{
  ot->name = "Refresh Data-Block Previews";
  ot->idname = "WM_OT_previews_ensure";
  ot->description =
      "Ensure data-block previews are available and up-to-date "
      "(to be saved in .blend file, only for some types like materials, textures, etc.)";

  ot->exec = previews_ensure_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data-Block Preview Clear Operator
 * \{ */

typedef enum PreviewFilterID {
  PREVIEW_FILTER_ALL,
  PREVIEW_FILTER_GEOMETRY,
  PREVIEW_FILTER_SHADING,
  PREVIEW_FILTER_SCENE,
  PREVIEW_FILTER_COLLECTION,
  PREVIEW_FILTER_OBJECT,
  PREVIEW_FILTER_MATERIAL,
  PREVIEW_FILTER_LIGHT,
  PREVIEW_FILTER_WORLD,
  PREVIEW_FILTER_TEXTURE,
  PREVIEW_FILTER_IMAGE,
} PreviewFilterID;

/* Only types supporting previews currently. */
static const EnumPropertyItem preview_id_type_items[] = {
    {PREVIEW_FILTER_ALL, "ALL", 0, "All Types", ""},
    {PREVIEW_FILTER_GEOMETRY,
     "GEOMETRY",
     0,
     "All Geometry Types",
     "Clear previews for scenes, collections and objects"},
    {PREVIEW_FILTER_SHADING,
     "SHADING",
     0,
     "All Shading Types",
     "Clear previews for materials, lights, worlds, textures and images"},
    {PREVIEW_FILTER_SCENE, "SCENE", 0, "Scenes", ""},
    {PREVIEW_FILTER_COLLECTION, "COLLECTION", 0, "Collections", ""},
    {PREVIEW_FILTER_OBJECT, "OBJECT", 0, "Objects", ""},
    {PREVIEW_FILTER_MATERIAL, "MATERIAL", 0, "Materials", ""},
    {PREVIEW_FILTER_LIGHT, "LIGHT", 0, "Lights", ""},
    {PREVIEW_FILTER_WORLD, "WORLD", 0, "Worlds", ""},
    {PREVIEW_FILTER_TEXTURE, "TEXTURE", 0, "Textures", ""},
    {PREVIEW_FILTER_IMAGE, "IMAGE", 0, "Images", ""},
#if 0 /* XXX TODO */
    {PREVIEW_FILTER_BRUSH, "BRUSH", 0, "Brushes", ""},
#endif
    {0, NULL, 0, NULL, NULL},
};

static uint preview_filter_to_idfilter(enum PreviewFilterID filter)
{
  switch (filter) {
    case PREVIEW_FILTER_ALL:
      return FILTER_ID_SCE | FILTER_ID_GR | FILTER_ID_OB | FILTER_ID_MA | FILTER_ID_LA |
             FILTER_ID_WO | FILTER_ID_TE | FILTER_ID_IM;
    case PREVIEW_FILTER_GEOMETRY:
      return FILTER_ID_SCE | FILTER_ID_GR | FILTER_ID_OB;
    case PREVIEW_FILTER_SHADING:
      return FILTER_ID_MA | FILTER_ID_LA | FILTER_ID_WO | FILTER_ID_TE | FILTER_ID_IM;
    case PREVIEW_FILTER_SCENE:
      return FILTER_ID_SCE;
    case PREVIEW_FILTER_COLLECTION:
      return FILTER_ID_GR;
    case PREVIEW_FILTER_OBJECT:
      return FILTER_ID_OB;
    case PREVIEW_FILTER_MATERIAL:
      return FILTER_ID_MA;
    case PREVIEW_FILTER_LIGHT:
      return FILTER_ID_LA;
    case PREVIEW_FILTER_WORLD:
      return FILTER_ID_WO;
    case PREVIEW_FILTER_TEXTURE:
      return FILTER_ID_TE;
    case PREVIEW_FILTER_IMAGE:
      return FILTER_ID_IM;
  }

  return 0;
}

static int previews_clear_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ListBase *lb[] = {
      &bmain->objects,
      &bmain->collections,
      &bmain->materials,
      &bmain->worlds,
      &bmain->lights,
      &bmain->textures,
      &bmain->images,
      NULL,
  };

  const int id_filters = preview_filter_to_idfilter(RNA_enum_get(op->ptr, "id_type"));

  for (int i = 0; lb[i]; i++) {
    ID *id = lb[i]->first;
    if (!id) {
      continue;
    }

#if 0
    printf("%s: %d, %d, %d -> %d\n",
           id->name,
           GS(id->name),
           BKE_idtype_idcode_to_idfilter(GS(id->name)),
           id_filters,
           BKE_idtype_idcode_to_idfilter(GS(id->name)) & id_filters);
#endif

    if (!(BKE_idtype_idcode_to_idfilter(GS(id->name)) & id_filters)) {
      continue;
    }

    for (; id; id = id->next) {
      PreviewImage *prv_img = BKE_previewimg_id_ensure(id);

      BKE_previewimg_clear(prv_img);
    }
  }

  return OPERATOR_FINISHED;
}

static void WM_OT_previews_clear(wmOperatorType *ot)
{
  ot->name = "Clear Data-Block Previews";
  ot->idname = "WM_OT_previews_clear";
  ot->description =
      "Clear data-block previews (only for some types like objects, materials, textures, etc.)";

  ot->exec = previews_clear_exec;
  ot->invoke = WM_menu_invoke;

  ot->prop = RNA_def_enum_flag(ot->srna,
                               "id_type",
                               preview_id_type_items,
                               PREVIEW_FILTER_ALL,
                               "Data-Block Type",
                               "Which data-block previews to clear");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Doc from UI Operator
 * \{ */

static int doc_view_manual_ui_context_exec(bContext *C, wmOperator *UNUSED(op))
{
  PointerRNA ptr_props;
  char buf[512];
  short retval = OPERATOR_CANCELLED;

  if (UI_but_online_manual_id_from_active(C, buf, sizeof(buf))) {
    WM_operator_properties_create(&ptr_props, "WM_OT_doc_view_manual");
    RNA_string_set(&ptr_props, "doc_id", buf);

    retval = WM_operator_name_call_ptr(C,
                                       WM_operatortype_find("WM_OT_doc_view_manual", false),
                                       WM_OP_EXEC_DEFAULT,
                                       &ptr_props,
                                       NULL);

    WM_operator_properties_free(&ptr_props);
  }

  return retval;
}

static void WM_OT_doc_view_manual_ui_context(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Online Manual";
  ot->idname = "WM_OT_doc_view_manual_ui_context";
  ot->description = "View a context based online manual in a web browser";

  /* callbacks */
  ot->poll = ED_operator_regionactive;
  ot->exec = doc_view_manual_ui_context_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Stereo 3D Operator
 *
 * Turning it fullscreen if needed.
 * \{ */

static void WM_OT_stereo3d_set(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Set Stereo 3D";
  ot->idname = "WM_OT_set_stereo_3d";
  ot->description = "Toggle 3D stereo support for current window (or change the display mode)";

  ot->exec = wm_stereo3d_set_exec;
  ot->invoke = wm_stereo3d_set_invoke;
  ot->poll = WM_operator_winactive;
  ot->ui = wm_stereo3d_set_draw;
  ot->check = wm_stereo3d_set_check;
  ot->cancel = wm_stereo3d_set_cancel;

  prop = RNA_def_enum(ot->srna,
                      "display_mode",
                      rna_enum_stereo3d_display_items,
                      S3D_DISPLAY_ANAGLYPH,
                      "Display Mode",
                      "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna,
                      "anaglyph_type",
                      rna_enum_stereo3d_anaglyph_type_items,
                      S3D_ANAGLYPH_REDCYAN,
                      "Anaglyph Type",
                      "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna,
                      "interlace_type",
                      rna_enum_stereo3d_interlace_type_items,
                      S3D_INTERLACE_ROW,
                      "Interlace Type",
                      "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "use_interlace_swap",
                         false,
                         "Swap Left/Right",
                         "Swap left and right stereo channels");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "use_sidebyside_crosseyed",
                         false,
                         "Cross-Eyed",
                         "Right eye should see left image and vice versa");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Registration & Keymaps
 * \{ */

void wm_operatortypes_register(void)
{
  WM_operatortype_append(WM_OT_window_close);
  WM_operatortype_append(WM_OT_window_new);
  WM_operatortype_append(WM_OT_window_new_main);
  WM_operatortype_append(WM_OT_read_history);
  WM_operatortype_append(WM_OT_read_homefile);
  WM_operatortype_append(WM_OT_read_factory_settings);
  WM_operatortype_append(WM_OT_save_homefile);
  WM_operatortype_append(WM_OT_save_userpref);
  WM_operatortype_append(WM_OT_read_userpref);
  WM_operatortype_append(WM_OT_read_factory_userpref);
  WM_operatortype_append(WM_OT_window_fullscreen_toggle);
  WM_operatortype_append(WM_OT_quit_blender);
  WM_operatortype_append(WM_OT_open_mainfile);
  WM_operatortype_append(WM_OT_revert_mainfile);
  WM_operatortype_append(WM_OT_link);
  WM_operatortype_append(WM_OT_append);
  WM_operatortype_append(WM_OT_lib_relocate);
  WM_operatortype_append(WM_OT_lib_reload);
  WM_operatortype_append(WM_OT_recover_last_session);
  WM_operatortype_append(WM_OT_recover_auto_save);
  WM_operatortype_append(WM_OT_save_as_mainfile);
  WM_operatortype_append(WM_OT_save_mainfile);
  WM_operatortype_append(WM_OT_redraw_timer);
  WM_operatortype_append(WM_OT_memory_statistics);
  WM_operatortype_append(WM_OT_debug_menu);
  WM_operatortype_append(WM_OT_operator_defaults);
  WM_operatortype_append(WM_OT_splash);
  WM_operatortype_append(WM_OT_splash_about);
  WM_operatortype_append(WM_OT_search_menu);
  WM_operatortype_append(WM_OT_search_operator);
  WM_operatortype_append(WM_OT_call_menu);
  WM_operatortype_append(WM_OT_call_menu_pie);
  WM_operatortype_append(WM_OT_call_panel);
  WM_operatortype_append(WM_OT_radial_control);
  WM_operatortype_append(WM_OT_stereo3d_set);
#if defined(WIN32)
  WM_operatortype_append(WM_OT_console_toggle);
#endif
  WM_operatortype_append(WM_OT_previews_ensure);
  WM_operatortype_append(WM_OT_previews_clear);
  WM_operatortype_append(WM_OT_doc_view_manual_ui_context);

#ifdef WITH_XR_OPENXR
  wm_xr_operatortypes_register();
#endif

  /* gizmos */
  WM_operatortype_append(GIZMOGROUP_OT_gizmo_select);
  WM_operatortype_append(GIZMOGROUP_OT_gizmo_tweak);
}

/* circleselect-like modal operators */
static void gesture_circle_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {GESTURE_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {GESTURE_MODAL_CIRCLE_ADD, "ADD", 0, "Add", ""},
      {GESTURE_MODAL_CIRCLE_SUB, "SUBTRACT", 0, "Subtract", ""},
      {GESTURE_MODAL_CIRCLE_SIZE, "SIZE", 0, "Size", ""},

      {GESTURE_MODAL_SELECT, "SELECT", 0, "Select", ""},
      {GESTURE_MODAL_DESELECT, "DESELECT", 0, "Deselect", ""},
      {GESTURE_MODAL_NOP, "NOP", 0, "No Operation", ""},

      {0, NULL, 0, NULL, NULL},
  };

  /* WARNING: Name is incorrect, use for non-3d views. */
  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "View3D Gesture Circle");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "View3D Gesture Circle", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_circle");
  WM_modalkeymap_assign(keymap, "UV_OT_select_circle");
  WM_modalkeymap_assign(keymap, "CLIP_OT_select_circle");
  WM_modalkeymap_assign(keymap, "MASK_OT_select_circle");
  WM_modalkeymap_assign(keymap, "NODE_OT_select_circle");
  WM_modalkeymap_assign(keymap, "GPENCIL_OT_select_circle");
  WM_modalkeymap_assign(keymap, "GRAPH_OT_select_circle");
  WM_modalkeymap_assign(keymap, "ACTION_OT_select_circle");
}

/* straight line modal operators */
static void gesture_straightline_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {GESTURE_MODAL_SELECT, "SELECT", 0, "Select", ""},
      {GESTURE_MODAL_BEGIN, "BEGIN", 0, "Begin", ""},
      {GESTURE_MODAL_MOVE, "MOVE", 0, "Move", ""},
      {GESTURE_MODAL_SNAP, "SNAP", 0, "Snap", ""},
      {GESTURE_MODAL_FLIP, "FLIP", 0, "Flip", ""},
      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Straight Line");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Straight Line", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "IMAGE_OT_sample_line");
  WM_modalkeymap_assign(keymap, "PAINT_OT_weight_gradient");
  WM_modalkeymap_assign(keymap, "MESH_OT_bisect");
  WM_modalkeymap_assign(keymap, "PAINT_OT_mask_line_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_project_line_gesture");
}

/* box_select-like modal operators */
static void gesture_box_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {GESTURE_MODAL_SELECT, "SELECT", 0, "Select", ""},
      {GESTURE_MODAL_DESELECT, "DESELECT", 0, "Deselect", ""},
      {GESTURE_MODAL_BEGIN, "BEGIN", 0, "Begin", ""},
      {GESTURE_MODAL_MOVE, "MOVE", 0, "Move", ""},
      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Box");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Box", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "ACTION_OT_select_box");
  WM_modalkeymap_assign(keymap, "ANIM_OT_channels_select_box");
  WM_modalkeymap_assign(keymap, "ANIM_OT_previewrange_set");
  WM_modalkeymap_assign(keymap, "INFO_OT_select_box");
  WM_modalkeymap_assign(keymap, "FILE_OT_select_box");
  WM_modalkeymap_assign(keymap, "GRAPH_OT_select_box");
  WM_modalkeymap_assign(keymap, "MARKER_OT_select_box");
  WM_modalkeymap_assign(keymap, "NLA_OT_select_box");
  WM_modalkeymap_assign(keymap, "NODE_OT_select_box");
  WM_modalkeymap_assign(keymap, "NODE_OT_viewer_border");
  WM_modalkeymap_assign(keymap, "PAINT_OT_hide_show");
  WM_modalkeymap_assign(keymap, "OUTLINER_OT_select_box");
#if 0 /* Template. */
  WM_modalkeymap_assign(keymap, "SCREEN_OT_box_select");
#endif
  WM_modalkeymap_assign(keymap, "SEQUENCER_OT_select_box");
  WM_modalkeymap_assign(keymap, "SEQUENCER_OT_view_ghost_border");
  WM_modalkeymap_assign(keymap, "UV_OT_select_box");
  WM_modalkeymap_assign(keymap, "CLIP_OT_select_box");
  WM_modalkeymap_assign(keymap, "CLIP_OT_graph_select_box");
  WM_modalkeymap_assign(keymap, "MASK_OT_select_box");
  WM_modalkeymap_assign(keymap, "PAINT_OT_mask_box_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_face_set_box_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_trim_box_gesture");
  WM_modalkeymap_assign(keymap, "VIEW2D_OT_zoom_border");
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_clip_border");
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_render_border");
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_box");
  /* XXX TODO: zoom border should perhaps map right-mouse to zoom out instead of in+cancel. */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom_border");
  WM_modalkeymap_assign(keymap, "IMAGE_OT_render_border");
  WM_modalkeymap_assign(keymap, "IMAGE_OT_view_zoom_border");
  WM_modalkeymap_assign(keymap, "GPENCIL_OT_select_box");
}

/* lasso modal operators */
static void gesture_lasso_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_MOVE, "MOVE", 0, "Move", ""},
      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Lasso");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Lasso", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "GPENCIL_OT_stroke_cutter");
  WM_modalkeymap_assign(keymap, "GPENCIL_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "MASK_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "PAINT_OT_mask_lasso_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_face_set_lasso_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_trim_lasso_gesture");
  WM_modalkeymap_assign(keymap, "ACTION_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "CLIP_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "GRAPH_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "NODE_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "UV_OT_select_lasso");
}

/* zoom to border modal operators */
static void gesture_zoom_border_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {GESTURE_MODAL_IN, "IN", 0, "In", ""},
      {GESTURE_MODAL_OUT, "OUT", 0, "Out", ""},
      {GESTURE_MODAL_BEGIN, "BEGIN", 0, "Begin", ""},
      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Zoom Border");

  /* this function is called for each spacetype, only needs to add map once */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Zoom Border", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "VIEW2D_OT_zoom_border");
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom_border");
  WM_modalkeymap_assign(keymap, "IMAGE_OT_view_zoom_border");
}

void wm_window_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Window", 0, 0);

  wm_gizmos_keymap(keyconf);
  gesture_circle_modal_keymap(keyconf);
  gesture_box_modal_keymap(keyconf);
  gesture_zoom_border_modal_keymap(keyconf);
  gesture_straightline_modal_keymap(keyconf);
  gesture_lasso_modal_keymap(keyconf);

  WM_keymap_fix_linking();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Enum Filter Functions
 *
 * Filter functions that can be used with rna_id_itemf() below.
 * Should return false if 'id' should be excluded.
 *
 * \{ */

static bool rna_id_enum_filter_single(const ID *id, void *user_data)
{
  return (id != user_data);
}

/* Generic itemf's for operators that take library args */
static const EnumPropertyItem *rna_id_itemf(bool *r_free,
                                            ID *id,
                                            bool local,
                                            bool (*filter_ids)(const ID *id, void *user_data),
                                            void *user_data)
{
  EnumPropertyItem item_tmp = {0}, *item = NULL;
  int totitem = 0;
  int i = 0;

  if (id != NULL) {
    const short id_type = GS(id->name);
    for (; id; id = id->next) {
      if ((filter_ids != NULL) && filter_ids(id, user_data) == false) {
        i++;
        continue;
      }
      if (local == false || !ID_IS_LINKED(id)) {
        item_tmp.identifier = item_tmp.name = id->name + 2;
        item_tmp.value = i++;

        /* Show collection color tag icons in menus. */
        if (id_type == ID_GR) {
          item_tmp.icon = UI_icon_color_from_collection((struct Collection *)id);
        }

        RNA_enum_item_add(&item, &totitem, &item_tmp);
      }
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

/* Can add more ID types as needed. */

const EnumPropertyItem *RNA_action_itemf(bContext *C,
                                         PointerRNA *UNUSED(ptr),
                                         PropertyRNA *UNUSED(prop),
                                         bool *r_free)
{

  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->actions.first : NULL, false, NULL, NULL);
}
#if 0 /* UNUSED */
const EnumPropertyItem *RNA_action_local_itemf(bContext *C,
                                               PointerRNA *UNUSED(ptr),
                                               PropertyRNA *UNUSED(prop),
                                               bool *r_free)
{
  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->action.first : NULL, true);
}
#endif

const EnumPropertyItem *RNA_collection_itemf(bContext *C,
                                             PointerRNA *UNUSED(ptr),
                                             PropertyRNA *UNUSED(prop),
                                             bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->collections.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_collection_local_itemf(bContext *C,
                                                   PointerRNA *UNUSED(ptr),
                                                   PropertyRNA *UNUSED(prop),
                                                   bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->collections.first : NULL, true, NULL, NULL);
}

const EnumPropertyItem *RNA_image_itemf(bContext *C,
                                        PointerRNA *UNUSED(ptr),
                                        PropertyRNA *UNUSED(prop),
                                        bool *r_free)
{
  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->images.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_image_local_itemf(bContext *C,
                                              PointerRNA *UNUSED(ptr),
                                              PropertyRNA *UNUSED(prop),
                                              bool *r_free)
{
  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->images.first : NULL, true, NULL, NULL);
}

const EnumPropertyItem *RNA_scene_itemf(bContext *C,
                                        PointerRNA *UNUSED(ptr),
                                        PropertyRNA *UNUSED(prop),
                                        bool *r_free)
{
  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->scenes.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_scene_local_itemf(bContext *C,
                                              PointerRNA *UNUSED(ptr),
                                              PropertyRNA *UNUSED(prop),
                                              bool *r_free)
{
  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->scenes.first : NULL, true, NULL, NULL);
}
const EnumPropertyItem *RNA_scene_without_active_itemf(bContext *C,
                                                       PointerRNA *UNUSED(ptr),
                                                       PropertyRNA *UNUSED(prop),
                                                       bool *r_free)
{
  Scene *scene_active = C ? CTX_data_scene(C) : NULL;
  return rna_id_itemf(r_free,
                      C ? (ID *)CTX_data_main(C)->scenes.first : NULL,
                      false,
                      rna_id_enum_filter_single,
                      scene_active);
}
const EnumPropertyItem *RNA_movieclip_itemf(bContext *C,
                                            PointerRNA *UNUSED(ptr),
                                            PropertyRNA *UNUSED(prop),
                                            bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->movieclips.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_movieclip_local_itemf(bContext *C,
                                                  PointerRNA *UNUSED(ptr),
                                                  PropertyRNA *UNUSED(prop),
                                                  bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->movieclips.first : NULL, true, NULL, NULL);
}

const EnumPropertyItem *RNA_mask_itemf(bContext *C,
                                       PointerRNA *UNUSED(ptr),
                                       PropertyRNA *UNUSED(prop),
                                       bool *r_free)
{
  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->masks.first : NULL, false, NULL, NULL);
}
const EnumPropertyItem *RNA_mask_local_itemf(bContext *C,
                                             PointerRNA *UNUSED(ptr),
                                             PropertyRNA *UNUSED(prop),
                                             bool *r_free)
{
  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->masks.first : NULL, true, NULL, NULL);
}

/** \} */
