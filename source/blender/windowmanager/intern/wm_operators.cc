/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Functions for dealing with wmOperator, adding, removing, calling
 * as well as some generic operators and shared operator properties.
 */

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sstream>

#include <fmt/format.h>

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

#include "BLT_translation.hh"

#include "BLI_dial_2d.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.hh"
#include "BKE_brush.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_preview_image.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh" /* #BKE_ST_MAXNAME. */

#include "BKE_idtype.hh"

#include "BLF_api.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "IMB_imbuf_types.hh"

#include "ED_fileselect.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_grease_pencil.hh"
#include "ED_numinput.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"

#include "wm.hh"
#include "wm_draw.hh"
#include "wm_event_system.hh"
#include "wm_event_types.hh"
#include "wm_files.hh"
#include "wm_window.hh"
#ifdef WITH_XR_OPENXR
#  include "wm_xr.hh"
#endif

#define UNDOCUMENTED_OPERATOR_TIP N_("(undocumented operator)")

/* -------------------------------------------------------------------- */
/** \name Operator API
 * \{ */

#define OP_BL_SEP_STRING "_OT_"
#define OP_BL_SEP_LEN 4

#define OP_PY_SEP_CHAR '.'
#define OP_PY_SEP_LEN 1

/* Difference between python 'identifier' and BL/C code one ("." separator replaced by "_OT_"),
 * and final `\0` char. */
#define OP_MAX_PY_IDNAME (OP_MAX_TYPENAME - OP_BL_SEP_LEN + OP_PY_SEP_LEN - 1)

size_t WM_operator_py_idname(char *dst, const char *src)
{
  const char *sep = strstr(src, OP_BL_SEP_STRING);
  if (sep) {
    const size_t sep_offset = size_t(sep - src);

    /* NOTE: we use ASCII `tolower` instead of system `tolower`, because the
     * latter depends on the locale, and can lead to `idname` mismatch. */
    memcpy(dst, src, sep_offset);
    BLI_str_tolower_ascii(dst, sep_offset);

    dst[sep_offset] = OP_PY_SEP_CHAR;
    return BLI_strncpy_rlen(dst + (sep_offset + OP_PY_SEP_LEN),
                            sep + OP_BL_SEP_LEN,
                            OP_MAX_TYPENAME - sep_offset - OP_PY_SEP_LEN) +
           (sep_offset + OP_PY_SEP_LEN);
  }
  /* Should not happen but support just in case. */
  return BLI_strncpy_rlen(dst, src, OP_MAX_TYPENAME);
}

size_t WM_operator_bl_idname(char *dst, const char *src)
{
  const size_t from_len = strlen(src);

  const char *sep = strchr(src, OP_PY_SEP_CHAR);
  if (sep && (from_len <= OP_MAX_PY_IDNAME)) {
    const size_t sep_offset = size_t(sep - src);
    memcpy(dst, src, sep_offset);
    BLI_str_toupper_ascii(dst, sep_offset);

    memcpy(dst + sep_offset, OP_BL_SEP_STRING, OP_BL_SEP_LEN);
    BLI_strncpy(dst + sep_offset + OP_BL_SEP_LEN,
                sep + OP_PY_SEP_LEN,
                from_len - sep_offset - OP_PY_SEP_LEN + 1);
    return from_len + OP_BL_SEP_LEN - OP_PY_SEP_LEN;
  }
  /* Should not happen but support just in case. */
  return BLI_strncpy_rlen(dst, src, OP_MAX_TYPENAME);
}

bool WM_operator_bl_idname_is_valid(const char *idname)
{
  const char *sep = strstr(idname, OP_BL_SEP_STRING);
  /* Separator missing or at string beginning/end. */
  if ((sep == nullptr) || (sep == idname) || (sep[OP_BL_SEP_LEN] == '\0')) {
    return false;
  }

  for (const char *ch = idname; ch < sep; ch++) {
    if ((*ch >= 'A' && *ch <= 'Z') || (*ch >= '0' && *ch <= '9') || *ch == '_') {
      continue;
    }
    return false;
  }

  for (const char *ch = sep + OP_BL_SEP_LEN; *ch; ch++) {
    if ((*ch >= 'a' && *ch <= 'z') || (*ch >= '0' && *ch <= '9') || *ch == '_') {
      continue;
    }
    return false;
  }
  return true;
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
      /* Pass. */
    }
    else if (*ch == '.') {
      if (ch == idname || (*(ch + 1) == '\0')) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Registering operator class: '%s', invalid bl_idname '%s', at position %d",
                    classname,
                    idname,
                    i);
        return false;
      }
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

  if (i > OP_MAX_PY_IDNAME) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering operator class: '%s', invalid bl_idname '%s', "
                "is too long, maximum length is %d",
                classname,
                idname,
                OP_MAX_PY_IDNAME);
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

std::string WM_operator_pystring_ex(bContext *C,
                                    wmOperator *op,
                                    const bool all_args,
                                    const bool macro_args,
                                    wmOperatorType *ot,
                                    PointerRNA *opptr)
{
  char idname_py[OP_MAX_TYPENAME];

  /* For building the string. */
  std::stringstream ss;

  /* Arbitrary, but can get huge string with stroke painting otherwise. */
  int max_prop_length = 10;

  WM_operator_py_idname(idname_py, ot->idname);
  ss << "bpy.ops." << idname_py << "(";

  if (op && op->macro.first) {
    /* Special handling for macros, else we only get default values in this case... */
    wmOperator *opm;
    bool first_op = true;

    opm = static_cast<wmOperator *>(macro_args ? op->macro.first : nullptr);

    for (; opm; opm = opm->next) {
      PointerRNA *opmptr = opm->ptr;
      PointerRNA opmptr_default;
      if (opmptr == nullptr) {
        WM_operator_properties_create_ptr(&opmptr_default, opm->type);
        opmptr = &opmptr_default;
      }

      std::string string_args = RNA_pointer_as_string_id(C, opmptr);
      if (first_op) {
        ss << opm->type->idname << '=' << string_args;
        first_op = false;
      }
      else {
        ss << ", " << opm->type->idname << '=' << string_args;
      }

      if (opmptr == &opmptr_default) {
        WM_operator_properties_free(&opmptr_default);
      }
    }
  }
  else {
    /* Only to get the original props for comparisons. */
    PointerRNA opptr_default;
    const bool macro_args_test = ot->macro.first ? macro_args : true;

    if (opptr == nullptr) {
      WM_operator_properties_create_ptr(&opptr_default, ot);
      opptr = &opptr_default;
    }

    ss << RNA_pointer_as_string_keywords(
        C, opptr, false, all_args, macro_args_test, max_prop_length);

    if (opptr == &opptr_default) {
      WM_operator_properties_free(&opptr_default);
    }
  }

  ss << ')';

  return ss.str();
}

std::string WM_operator_pystring(bContext *C,
                                 wmOperator *op,
                                 const bool all_args,
                                 const bool macro_args)
{
  return WM_operator_pystring_ex(C, op, all_args, macro_args, op->type, op->ptr);
}

std::string WM_operator_pystring_abbreviate(std::string str, int str_len_max)
{
  const int str_len = str.size();
  const size_t parens_start = str.find('(');
  if (parens_start == std::string::npos) {
    return str;
  }

  const size_t parens_end = str.find(parens_start + 1, ')');
  if (parens_end == std::string::npos) {
    return str;
  }

  const int parens_len = parens_end - parens_start;
  if (parens_len <= str_len_max) {
    return str;
  }

  /* Truncate after the first comma. */
  const size_t comma_first = str.find(parens_start, ',');
  if (comma_first == std::string::npos) {
    return str;
  }
  const char end_str[] = " ... )";
  const int end_str_len = sizeof(end_str) - 1;

  /* Leave a place for the first argument. */
  const int new_str_len = (comma_first - parens_start) + 1;

  if (str_len < new_str_len + parens_start + end_str_len + 1) {
    return str;
  }

  return str.substr(0, comma_first) + end_str;
}

/* Return nullptr if no match is found. */
#if 0
static const char *wm_context_member_from_ptr(bContext *C, const PointerRNA *ptr, bool *r_is_id)
{
  /* Loop over all context items and do 2 checks
   *
   * - See if the pointer is in the context.
   * - See if the pointers ID is in the context.
   */

  /* Don't get from the context store since this is normally
   * set only for the UI and not usable elsewhere. */
  ListBase lb = CTX_data_dir_get_ex(C, false, true, true);
  LinkData *link;

  const char *member_found = nullptr;
  const char *member_id = nullptr;
  bool member_found_is_id = false;

  for (link = lb.first; link; link = link->next) {
    const char *identifier = link->data;
    PointerRNA ctx_item_ptr = {};
    // CTX_data_pointer_get(C, identifier);  /* XXX, this isn't working. */

    if (ctx_item_ptr.type == nullptr) {
      continue;
    }

    if (ptr->owner_id == ctx_item_ptr.owner_id) {
      const bool is_id = RNA_struct_is_ID(ctx_item_ptr.type);
      if ((ptr->data == ctx_item_ptr.data) && (ptr->type == ctx_item_ptr.type)) {
        /* Found! */
        member_found = identifier;
        member_found_is_id = is_id;
        break;
      }
      if (is_id) {
        /* Found a reference to this ID, so fall back to it if there is no direct reference. */
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
    return nullptr;
  }
}

#else

/* Use hard coded checks for now. */

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
  const char *member_id = nullptr;
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
          (rna_ptr)->data == (CTX_data_pointer_get_type(C, ctx_member, &(rna_type)).data)) \
      { \
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
    } while (false);
  }

  /* Specific ID type checks. */
  if (ptr->owner_id && (member_id == nullptr)) {

    const ID_Type ptr_id_type = GS(ptr->owner_id->name);
    switch (ptr_id_type) {
      case ID_SCE: {
        TEST_PTR_DATA_TYPE_FROM_CONTEXT("active_strip", RNA_Strip, ptr);

        CTX_TEST_PTR_ID(C, "scene", ptr->owner_id);
        break;
      }
      case ID_OB: {
        TEST_PTR_DATA_TYPE_FROM_CONTEXT("active_pose_bone", RNA_PoseBone, ptr);

        CTX_TEST_PTR_ID(C, "object", ptr->owner_id);
        break;
      }
      /* From #rna_Main_objects_new. */
      case OB_DATA_SUPPORT_ID_CASE: {

        if (ptr_id_type == ID_AR) {
          const bArmature *arm = (bArmature *)ptr->owner_id;
          if (arm->edbo != nullptr) {
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
    BKE_object_material_get(((Object *)id_pt), ((Object *)id_pt)->actcol)
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
        if (space_data != nullptr) {
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
            case SPACE_NODE: {
              const SpaceNode *snode = (SpaceNode *)space_data;
              TEST_PTR_DATA_TYPE("space_data.overlay", RNA_SpaceNodeOverlay, ptr, snode);
              break;
            }
            case SPACE_CLIP: {
              const SpaceClip *sclip = (SpaceClip *)space_data;
              TEST_PTR_DATA_TYPE("space_data.overlay", RNA_SpaceClipOverlay, ptr, sclip);
              break;
            }
            case SPACE_SEQ: {
              const SpaceSeq *sseq = (SpaceSeq *)space_data;
              TEST_PTR_DATA_TYPE(
                  "space_data.preview_overlay", RNA_SequencerPreviewOverlay, ptr, sseq);
              TEST_PTR_DATA_TYPE(
                  "space_data.timeline_overlay", RNA_SequencerTimelineOverlay, ptr, sseq);
              TEST_PTR_DATA_TYPE("space_data.cache_overlay", RNA_SequencerCacheOverlay, ptr, sseq);
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

std::optional<std::string> WM_context_path_resolve_property_full(const bContext *C,
                                                                 const PointerRNA *ptr,
                                                                 PropertyRNA *prop,
                                                                 int index)
{
  bool is_id;
  const char *member_id = wm_context_member_from_ptr(C, ptr, &is_id);
  if (!member_id) {
    return std::nullopt;
  }
  std::string member_id_data_path;
  if (is_id && !RNA_struct_is_ID(ptr->type)) {
    std::optional<std::string> data_path = RNA_path_from_ID_to_struct(ptr);
    if (data_path) {
      if (prop != nullptr) {
        std::string prop_str = RNA_path_property_py(ptr, prop, index);
        if (prop_str[0] == '[') {
          member_id_data_path = fmt::format("{}.{}{}", member_id, *data_path, prop_str);
        }
        else {
          member_id_data_path = fmt::format("{}.{}.{}", member_id, *data_path, prop_str);
        }
      }
      else {
        member_id_data_path = fmt::format("{}.{}", member_id, *data_path);
      }
    }
  }
  else {
    if (prop != nullptr) {
      std::string prop_str = RNA_path_property_py(ptr, prop, index);
      if (prop_str[0] == '[') {
        member_id_data_path = fmt::format("{}{}", member_id, prop_str);
      }
      else {
        member_id_data_path = fmt::format("{}.{}", member_id, prop_str);
      }
    }
    else {
      member_id_data_path = member_id;
    }
  }

  return member_id_data_path;
}

std::optional<std::string> WM_context_path_resolve_full(bContext *C, const PointerRNA *ptr)
{
  return WM_context_path_resolve_property_full(C, ptr, nullptr, -1);
}

static std::optional<std::string> wm_prop_pystring_from_context(bContext *C,
                                                                PointerRNA *ptr,
                                                                PropertyRNA *prop,
                                                                int index)
{
  std::optional<std::string> member_id_data_path = WM_context_path_resolve_property_full(
      C, ptr, prop, index);
  if (!member_id_data_path.has_value()) {
    return std::nullopt;
  }
  return "bpy.context." + member_id_data_path.value();
}

std::optional<std::string> WM_prop_pystring_assign(bContext *C,
                                                   PointerRNA *ptr,
                                                   PropertyRNA *prop,
                                                   int index)
{
  std::optional<std::string> lhs = C ? wm_prop_pystring_from_context(C, ptr, prop, index) :
                                       std::nullopt;

  if (!lhs.has_value()) {
    /* Fall back to `bpy.data.foo[id]` if we don't find in the context. */
    if (std::optional<std::string> lhs_str = RNA_path_full_property_py(ptr, prop, index)) {
      lhs = lhs_str;
    }
    else {
      return std::nullopt;
    }
  }

  std::string rhs = RNA_property_as_string(C, ptr, prop, index, INT_MAX);

  std::string ret = fmt::format("{} = {}", lhs.value(), rhs);
  return ret;
}

void WM_operator_properties_create_ptr(PointerRNA *ptr, wmOperatorType *ot)
{
  /* Set the ID so the context can be accessed: see #STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID. */
  *ptr = RNA_pointer_create_discrete(static_cast<ID *>(G_MAIN->wm.first), ot->srna, nullptr);
}

void WM_operator_properties_create(PointerRNA *ptr, const char *opstring)
{
  wmOperatorType *ot = WM_operatortype_find(opstring, false);

  if (ot) {
    WM_operator_properties_create_ptr(ptr, ot);
  }
  else {
    /* Set the ID so the context can be accessed: see #STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID. */
    *ptr = RNA_pointer_create_discrete(
        static_cast<ID *>(G_MAIN->wm.first), &RNA_OperatorProperties, nullptr);
  }
}

void WM_operator_properties_alloc(PointerRNA **ptr, IDProperty **properties, const char *opstring)
{
  IDProperty *tmp_properties = nullptr;
  /* Allow passing nullptr for properties, just create the properties here then. */
  if (properties == nullptr) {
    properties = &tmp_properties;
  }

  if (*properties == nullptr) {
    *properties = blender::bke::idprop::create_group("wmOpItemProp").release();
  }

  if (*ptr == nullptr) {
    *ptr = MEM_new<PointerRNA>("wmOpItemPtr");
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

        /* Recurse into operator properties. */
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
      PropertyRNA *prop = static_cast<PropertyRNA *>(itemptr.data);

      if ((RNA_property_flag(prop) & (PROP_SKIP_SAVE | PROP_SKIP_PRESET)) == 0) {
        const char *identifier = RNA_property_identifier(prop);
        RNA_struct_system_idprops_unset(op->ptr, identifier);
      }
    }
    RNA_PROP_END;
  }
}

void WM_operator_properties_clear(PointerRNA *ptr)
{
  IDProperty *properties = static_cast<IDProperty *>(ptr->data);

  if (properties) {
    IDP_ClearProperty(properties);
  }
}

void WM_operator_properties_free(PointerRNA *ptr)
{
  IDProperty *properties = static_cast<IDProperty *>(ptr->data);

  if (properties) {
    IDP_FreeProperty(properties);
    ptr->data = nullptr; /* Just in case. */
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Last Properties API
 * \{ */

#if 1 /* May want to disable operator remembering previous state for testing. */

static bool operator_last_properties_init_impl(wmOperator *op, IDProperty *last_properties)
{
  bool changed = false;
  IDProperty *replaceprops = blender::bke::idprop::create_group("wmOperatorProperties").release();

  PropertyRNA *iterprop = RNA_struct_iterator_property(op->type->srna);

  RNA_PROP_BEGIN (op->ptr, itemptr, iterprop) {
    PropertyRNA *prop = static_cast<PropertyRNA *>(itemptr.data);
    if ((RNA_property_flag(prop) & PROP_SKIP_SAVE) == 0) {
      if (!RNA_property_is_set(op->ptr, prop)) { /* Don't override a setting already set. */
        const char *identifier = RNA_property_identifier(prop);
        IDProperty *idp_src = IDP_GetPropertyFromGroup(last_properties, identifier);
        if (idp_src) {
          IDProperty *idp_dst = IDP_CopyProperty(idp_src);

          /* NOTE: in the future this may need to be done recursively,
           * but for now RNA doesn't access nested operators. */
          idp_dst->flag |= IDP_FLAG_GHOST;

          /* Add to temporary group instead of immediate replace,
           * because we are iterating over this group. */
          IDP_AddToGroup(replaceprops, idp_dst);
          changed = true;
        }
      }
    }
  }
  RNA_PROP_END;

  if (changed) {
    CLOG_DEBUG(WM_LOG_OPERATORS, "Loading previous properties for '%s'", op->type->idname);
  }
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
    op->type->last_properties = nullptr;
  }

  if (op->properties) {
    if (!BLI_listbase_is_empty(&op->properties->data.group)) {
      CLOG_DEBUG(WM_LOG_OPERATORS, "Storing properties for '%s'", op->type->idname);
    }
    op->type->last_properties = IDP_CopyProperty(op->properties);
  }

  if (op->macro.first != nullptr) {
    LISTBASE_FOREACH (wmOperator *, opm, &op->macro) {
      if (opm->properties) {
        if (op->type->last_properties == nullptr) {
          op->type->last_properties =
              blender::bke::idprop::create_group("wmOperatorProperties").release();
        }
        IDProperty *idp_macro = IDP_CopyProperty(opm->properties);
        STRNCPY(idp_macro->name, opm->type->idname);
        IDP_ReplaceInGroup(op->type->last_properties, idp_macro);
      }
    }
  }

  return (op->type->last_properties != nullptr);
}

#else

bool WM_operator_last_properties_init(wmOperator * /*op*/)
{
  return false;
}

bool WM_operator_last_properties_store(wmOperator * /*op*/)
{
  return false;
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default Operator Callbacks
 * \{ */

wmOperatorStatus WM_generic_select_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  PropertyRNA *wait_to_deselect_prop = RNA_struct_find_property(op->ptr,
                                                                "wait_to_deselect_others");
  const bool use_select_on_click = RNA_struct_property_is_set(op->ptr, "use_select_on_click");
  const short init_event_type = short(POINTER_AS_INT(op->customdata));

  /* Get settings from RNA properties for operator. */
  const int mval[2] = {RNA_int_get(op->ptr, "mouse_x"), RNA_int_get(op->ptr, "mouse_y")};

  if (init_event_type == 0) {
    op->customdata = POINTER_FROM_INT(int(event->type));

    if (use_select_on_click) {
      /* Don't do any selection yet. Wait to see if there's a drag or click (release) event. */
      WM_event_add_modal_handler(C, op);
      return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
    }

    if (event->val == KM_PRESS) {
      RNA_property_boolean_set(op->ptr, wait_to_deselect_prop, true);

      wmOperatorStatus retval = op->type->exec(C, op);
      OPERATOR_RETVAL_CHECK(retval);

      if (retval & OPERATOR_RUNNING_MODAL) {
        WM_event_add_modal_handler(C, op);
      }
      return retval | OPERATOR_PASS_THROUGH;
    }
    /* If we are in init phase, and cannot validate init of modal operations,
     * just fall back to basic exec.
     */
    RNA_property_boolean_set(op->ptr, wait_to_deselect_prop, false);

    wmOperatorStatus retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);

    return retval | OPERATOR_PASS_THROUGH;
  }
  if (event->type == init_event_type && event->val == KM_RELEASE) {
    RNA_property_boolean_set(op->ptr, wait_to_deselect_prop, false);

    wmOperatorStatus retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);

    return retval | OPERATOR_PASS_THROUGH;
  }
  if (ISMOUSE_MOTION(event->type)) {
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

wmOperatorStatus WM_generic_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);

  int mval[2];
  WM_event_drag_start_mval(event, region, mval);

  RNA_int_set(op->ptr, "mouse_x", mval[0]);
  RNA_int_set(op->ptr, "mouse_y", mval[1]);

  op->customdata = POINTER_FROM_INT(0);

  wmOperatorStatus retval = op->type->modal(C, op, event);
  OPERATOR_RETVAL_CHECK(retval);
  return retval;
}

void WM_operator_view3d_unit_defaults(bContext *C, wmOperator *op)
{
  if (op->flag & OP_IS_INVOKE) {
    Scene *scene = CTX_data_scene(C);
    View3D *v3d = CTX_wm_view3d(C);

    const float dia = v3d ? ED_view3d_grid_scale(scene, v3d, nullptr) :
                            ED_scene_grid_scale(scene, nullptr);

    /* Always run, so the values are initialized,
     * otherwise we may get differ behavior when `dia != 1.0`. */
    RNA_STRUCT_BEGIN (op->ptr, prop) {
      if (RNA_property_type(prop) == PROP_FLOAT) {
        PropertySubType pstype = RNA_property_subtype(prop);
        if (pstype == PROP_DISTANCE) {
          /* We don't support arrays yet. */
          BLI_assert(RNA_property_array_check(prop) == false);
          /* Initialize. */
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
  return (op->flag & OP_IS_INVOKE && !(U.uiflag & USER_REDUCE_MOTION)) ? U.smooth_viewtx : 0;
}

wmOperatorStatus WM_menu_invoke_ex(bContext *C,
                                   wmOperator *op,
                                   blender::wm::OpCallContext opcontext)
{
  PropertyRNA *prop = op->type->prop;

  if (prop == nullptr) {
    CLOG_ERROR(WM_LOG_OPERATORS, "'%s' has no enum property set", op->type->idname);
  }
  else if (RNA_property_type(prop) != PROP_ENUM) {
    CLOG_ERROR(WM_LOG_OPERATORS,
               "'%s', '%s' is not an enum property",
               op->type->idname,
               RNA_property_identifier(prop));
  }
  else if (RNA_property_is_set(op->ptr, prop)) {
    const wmOperatorStatus retval = op->type->exec(C, op);
    OPERATOR_RETVAL_CHECK(retval);
    return retval;
  }
  else {
    uiPopupMenu *pup = UI_popup_menu_begin(
        C, WM_operatortype_name(op->type, op->ptr).c_str(), ICON_NONE);
    uiLayout *layout = UI_popup_menu_layout(pup);
    /* Set this so the default execution context is the same as submenus. */
    layout->operator_context_set(opcontext);
    layout->op_enum(op->type->idname,
                    RNA_property_identifier(prop),
                    static_cast<IDProperty *>(op->ptr->data),
                    opcontext,
                    UI_ITEM_NONE);
    UI_popup_menu_end(C, pup);
    return OPERATOR_INTERFACE;
  }

  return OPERATOR_CANCELLED;
}

wmOperatorStatus WM_menu_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_menu_invoke_ex(C, op, blender::wm::OpCallContext::InvokeRegionWin);
}

struct EnumSearchMenu {
  wmOperator *op; /* The operator that will be executed when selecting an item. */
};

/** Generic enum search invoke popup. */
static uiBlock *wm_enum_search_menu(bContext *C, ARegion *region, void *arg)
{
  EnumSearchMenu *search_menu = static_cast<EnumSearchMenu *>(arg);
  wmWindow *win = CTX_wm_window(C);
  wmOperator *op = search_menu->op;
  /* `template_ID` uses `4 * widget_unit` for width,
   * we use a bit more, some items may have a suffix to show. */
  const int width = UI_searchbox_size_x();
  const int height = UI_searchbox_size_y();
  static char search[256] = "";

  uiBlock *block = UI_block_begin(C, region, "_popup", blender::ui::EmbossType::Emboss);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  search[0] = '\0';
#if 0 /* Ok, this isn't so easy. */
  uiDefBut(block,
           ButType::Label,
           0,
           WM_operatortype_name(op->type, op->ptr),
           0,
           0,
           UI_searchbox_size_x(),
           UI_UNIT_Y,
           nullptr,
           0.0,
           0.0,
           "");
#endif
  uiBut *but = uiDefSearchButO_ptr(block,
                                   op->type,
                                   static_cast<IDProperty *>(op->ptr->data),
                                   search,
                                   0,
                                   ICON_VIEWZOOM,
                                   sizeof(search),
                                   0,
                                   0,
                                   width,
                                   UI_UNIT_Y,
                                   "");

  /* Fake button, it holds space for search items. */
  uiDefBut(block, ButType::Label, 0, "", 0, -height, width, height, nullptr, 0, 0, std::nullopt);

  /* Move it downwards, mouse over button. */
  UI_block_bounds_set_popup(block, UI_SEARCHBOX_BOUNDS, blender::int2{0, -UI_UNIT_Y});

  UI_but_focus_on_enter_event(win, but);

  return block;
}

wmOperatorStatus WM_enum_search_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  static EnumSearchMenu search_menu;
  search_menu.op = op;
  /* Refreshing not supported, because operator might get freed. */
  const bool can_refresh = false;
  UI_popup_block_invoke_ex(C, wm_enum_search_menu, &search_menu, nullptr, can_refresh);
  return OPERATOR_INTERFACE;
}

wmOperatorStatus WM_operator_confirm_message_ex(bContext *C,
                                                wmOperator *op,
                                                const char *title,
                                                const int icon,
                                                const char *message,
                                                const blender::wm::OpCallContext /*opcontext*/)
{
  int alert_icon = ALERT_ICON_QUESTION;
  switch (icon) {
    case ICON_NONE:
      alert_icon = ALERT_ICON_NONE;
      break;
    case ICON_ERROR:
      alert_icon = ALERT_ICON_WARNING;
      break;
    case ICON_QUESTION:
      alert_icon = ALERT_ICON_QUESTION;
      break;
    case ICON_CANCEL:
      alert_icon = ALERT_ICON_ERROR;
      break;
    case ICON_INFO:
      alert_icon = ALERT_ICON_INFO;
      break;
  }
  return WM_operator_confirm_ex(C, op, IFACE_(title), nullptr, IFACE_(message), alert_icon, false);
}

wmOperatorStatus WM_operator_confirm_message(bContext *C, wmOperator *op, const char *message)
{
  return WM_operator_confirm_ex(
      C, op, IFACE_(message), nullptr, IFACE_("OK"), ALERT_ICON_NONE, false);
}

wmOperatorStatus WM_operator_confirm(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_confirm_ex(
      C, op, IFACE_(op->type->name), nullptr, IFACE_("OK"), ALERT_ICON_NONE, false);
}

wmOperatorStatus WM_operator_confirm_or_exec(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  const bool confirm = RNA_boolean_get(op->ptr, "confirm");
  if (confirm) {
    return WM_operator_confirm_ex(
        C, op, IFACE_(op->type->name), nullptr, IFACE_("OK"), ALERT_ICON_NONE, false);
  }
  return op->type->exec(C, op);
}

wmOperatorStatus WM_operator_filesel(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    return WM_operator_call_notest(C, op); /* Call exec direct. */
  }
  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

bool WM_operator_filesel_ensure_ext_imtype(wmOperator *op, const ImageFormatData *im_format)
{
  char filepath[FILE_MAX];
  /* Don't nullptr check prop, this can only run on ops with a 'filepath'. */
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "filepath");
  RNA_property_string_get(op->ptr, prop, filepath);
  if (BKE_image_path_ext_from_imformat_ensure(filepath, sizeof(filepath), im_format)) {
    RNA_property_string_set(op->ptr, prop, filepath);
    /* NOTE: we could check for and update 'filename' here,
     * but so far nothing needs this. */
    return true;
  }
  return false;
}

bool WM_operator_winactive(bContext *C)
{
  if (CTX_wm_window(C) == nullptr) {
    return false;
  }
  return true;
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

  /* Only for operators that are registered and did an undo push. */
  LISTBASE_FOREACH_BACKWARD (wmOperator *, op, &wm->runtime->operators) {
    if ((op->type->flag & OPTYPE_REGISTER) && (op->type->flag & OPTYPE_UNDO)) {
      return op;
    }
  }

  return nullptr;
}

IDProperty *WM_operator_last_properties_ensure_idprops(wmOperatorType *ot)
{
  if (ot->last_properties == nullptr) {
    ot->last_properties = blender::bke::idprop::create_group("wmOperatorProperties").release();
  }
  return ot->last_properties;
}

void WM_operator_last_properties_ensure(wmOperatorType *ot, PointerRNA *ptr)
{
  IDProperty *props = WM_operator_last_properties_ensure_idprops(ot);
  *ptr = RNA_pointer_create_discrete(static_cast<ID *>(G_MAIN->wm.first), ot->srna, props);
}

ID *WM_operator_drop_load_path(bContext *C, wmOperator *op, const short idcode)
{
  Main *bmain = CTX_data_main(C);
  ID *id = nullptr;

  /* Check input variables. */
  if (RNA_struct_property_is_set(op->ptr, "filepath")) {
    const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
    char filepath[FILE_MAX];
    bool exists = false;

    RNA_string_get(op->ptr, "filepath", filepath);

    errno = 0;

    if (idcode == ID_IM) {
      id = reinterpret_cast<ID *>(BKE_image_load_exists(bmain, filepath, &exists));
    }
    else {
      BLI_assert_unreachable();
    }

    if (!id) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Cannot read %s '%s': %s",
                  BKE_idtype_idcode_to_name(idcode),
                  filepath,
                  errno ? strerror(errno) : RPT_("unsupported format"));
      return nullptr;
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
    return nullptr;
  }

  /* Lookup an already existing ID. */
  id = WM_operator_properties_id_lookup_from_name_or_session_uid(bmain, op->ptr, ID_Type(idcode));

  if (!id) {
    /* Print error with the name if the name is available. */

    if (RNA_struct_property_is_set(op->ptr, "name")) {
      char name[MAX_ID_NAME - 2];
      RNA_string_get(op->ptr, "name", name);
      BKE_reportf(
          op->reports, RPT_ERROR, "%s '%s' not found", BKE_idtype_idcode_to_name(idcode), name);
      return nullptr;
    }

    BKE_reportf(op->reports, RPT_ERROR, "%s not found", BKE_idtype_idcode_to_name(idcode));
    return nullptr;
  }

  id_us_plus(id);
  return id;
}

static void wm_block_redo_cb(bContext *C, void *arg_op, int /*arg_event*/)
{
  wmOperator *op = static_cast<wmOperator *>(arg_op);

  if (op == WM_operator_last_redo(C)) {
    /* Operator was already executed once? undo & repeat. */
    ED_undo_operator_repeat(C, op);
  }
  else {
    /* Operator not executed yet, call it. */
    ED_undo_push_op(C, op);
    wm_operator_register(C, op);

    WM_operator_repeat(C, op);
  }
}

static void wm_block_redo_cancel_cb(bContext *C, void *arg_op)
{
  wmOperator *op = static_cast<wmOperator *>(arg_op);

  /* If operator never got executed, free it. */
  if (op != WM_operator_last_redo(C)) {
    WM_operator_free(op);
  }
}

static uiBlock *wm_block_create_redo(bContext *C, ARegion *region, void *arg_op)
{
  wmOperator *op = static_cast<wmOperator *>(arg_op);
  const uiStyle *style = UI_style_get_dpi();
  int width = 15 * UI_UNIT_X;

  uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
  UI_block_flag_disable(block, UI_BLOCK_LOOP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_REGULAR);

  /* #UI_BLOCK_NUMSELECT for layer buttons. */
  UI_block_flag_enable(block, UI_BLOCK_NUMSELECT | UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT);

  /* If register is not enabled, the operator gets freed on #OPERATOR_FINISHED
   * ui_apply_but_funcs_after calls #ED_undo_operator_repeate_cb and crashes. */
  BLI_assert(op->type->flag & OPTYPE_REGISTER);

  UI_block_func_handle_set(block, wm_block_redo_cb, arg_op);
  UI_popup_dummy_panel_set(region, block);
  uiLayout &layout = blender::ui::block_layout(block,
                                               blender::ui::LayoutDirection::Vertical,
                                               blender::ui::LayoutType::Panel,
                                               0,
                                               0,
                                               width,
                                               UI_UNIT_Y,
                                               0,
                                               style);

  if (op == WM_operator_last_redo(C)) {
    if (!WM_operator_check_ui_enabled(C, op->type->name)) {
      layout.enabled_set(false);
    }
  }

  uiItemL_ex(&layout, WM_operatortype_name(op->type, op->ptr), ICON_NONE, true, false);
  layout.separator(0.2f, LayoutSeparatorType::Line);
  layout.separator(0.5f);

  uiLayout *col = &layout.column(false);
  uiTemplateOperatorPropertyButs(C, col, op, UI_BUT_LABEL_ALIGN_NONE, 0);

  UI_block_bounds_set_popup(block, 7 * UI_SCALE_FAC, nullptr);

  return block;
}

struct wmOpPopUp {
  wmOperator *op;
  int width;
  int free_op;
  std::string title;
  std::string message;
  std::string confirm_text;
  eAlertIcon icon;
  wmPopupSize size;
  wmPopupPosition position;
  bool cancel_default;
  bool mouse_move_quit;
  bool include_properties;
};

/* Only invoked by OK button in popups created with #wm_block_dialog_create(). */
static void dialog_exec_cb(bContext *C, void *arg1, void *arg2)
{
  wmOperator *op;
  {
    /* Execute will free the operator.
     * In this case, wm_operator_ui_popup_cancel won't run. */
    wmOpPopUp *data = static_cast<wmOpPopUp *>(arg1);
    op = data->op;
    MEM_delete(data);
  }

  uiBlock *block = static_cast<uiBlock *>(arg2);
  /* Explicitly set UI_RETURN_OK flag, otherwise the menu might be canceled
   * in case WM_operator_call_ex exits/reloads the current file (#49199). */

  UI_popup_menu_retval_set(block, UI_RETURN_OK, true);

  /* Get context data *after* WM_operator_call_ex
   * which might have closed the current file and changed context. */
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, block);

  WM_operator_call_ex(C, op, true);
}

static void wm_operator_ui_popup_cancel(bContext *C, void *user_data);

/* Only invoked by Cancel button in popups created with #wm_block_dialog_create(). */
static void dialog_cancel_cb(bContext *C, void *arg1, void *arg2)
{
  wm_operator_ui_popup_cancel(C, arg1);
  uiBlock *block = static_cast<uiBlock *>(arg2);
  UI_popup_menu_retval_set(block, UI_RETURN_CANCEL, true);
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, block);
}

/**
 * Dialogs are popups that require user verification (click OK) before exec.
 */
static uiBlock *wm_block_dialog_create(bContext *C, ARegion *region, void *user_data)
{
  wmOpPopUp *data = static_cast<wmOpPopUp *>(user_data);
  wmOperator *op = data->op;
  const uiStyle *style = UI_style_get_dpi();
  const bool small = data->size == WM_POPUP_SIZE_SMALL;
  const short icon_size = (small ? 32 : 40) * UI_SCALE_FAC;

  uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
  UI_block_flag_disable(block, UI_BLOCK_LOOP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  UI_popup_dummy_panel_set(region, block);

  if (data->mouse_move_quit) {
    UI_block_flag_enable(block, UI_BLOCK_MOVEMOUSE_QUIT);
  }
  if (data->icon < ALERT_ICON_NONE || data->icon >= ALERT_ICON_MAX) {
    data->icon = ALERT_ICON_QUESTION;
  }

  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_NUMSELECT);

  UI_fontstyle_set(&style->widget);
  /* Width based on the text lengths. */
  int text_width = std::max(
      120 * UI_SCALE_FAC,
      BLF_width(style->widget.uifont_id, data->title.c_str(), BLF_DRAW_STR_DUMMY_MAX));

  /* Break Message into multiple lines. */
  blender::Vector<std::string> message_lines;
  blender::StringRef messaged_trimmed = blender::StringRef(data->message).trim();
  std::istringstream message_stream(messaged_trimmed);
  std::string line;
  while (std::getline(message_stream, line)) {
    message_lines.append(line);
    text_width = std::max(
        text_width, int(BLF_width(style->widget.uifont_id, line.c_str(), BLF_DRAW_STR_DUMMY_MAX)));
  }

  int dialog_width = std::max(text_width + int(style->columnspace * 2.5), data->width);

  /* Adjust width if the button text is long. */
  const int longest_button_text = std::max(
      BLF_width(style->widget.uifont_id, data->confirm_text.c_str(), BLF_DRAW_STR_DUMMY_MAX),
      BLF_width(style->widget.uifont_id, IFACE_("Cancel"), BLF_DRAW_STR_DUMMY_MAX));
  dialog_width = std::max(dialog_width, 3 * longest_button_text);

  uiLayout *layout;
  if (data->icon != ALERT_ICON_NONE) {
    layout = uiItemsAlertBox(
        block, style, dialog_width + icon_size, eAlertIcon(data->icon), icon_size);
  }
  else {
    layout = &blender::ui::block_layout(block,
                                        blender::ui::LayoutDirection::Vertical,
                                        blender::ui::LayoutType::Panel,
                                        0,
                                        0,
                                        dialog_width,
                                        0,
                                        0,
                                        style);
  }

  /* Title. */
  if (!data->title.empty()) {
    uiItemL_ex(layout, data->title, ICON_NONE, true, false);

    /* Line under the title if there are properties but no message body. */
    if (data->include_properties && message_lines.size() == 0) {
      layout->separator(0.2f, LayoutSeparatorType::Line);
    };
  }

  /* Message lines. */
  if (message_lines.size() > 0) {
    uiLayout *lines = &layout->column(false);
    lines->scale_y_set(0.65f);
    lines->separator(0.1f);
    for (auto &st : message_lines) {
      lines->label(st, ICON_NONE);
    }
  }

  if (data->include_properties) {
    layout->separator(0.5f);
    uiTemplateOperatorPropertyButs(C, layout, op, UI_BUT_LABEL_ALIGN_SPLIT_COLUMN, 0);
  }

  layout->separator(small ? 0.1f : 1.8f);

  /* Clear so the OK button is left alone. */
  UI_block_func_set(block, nullptr, nullptr, nullptr);

#ifdef _WIN32
  const bool windows_layout = true;
#else
  const bool windows_layout = false;
#endif

  /* Check there are no active default buttons, allowing a dialog to define its own
   * confirmation buttons which are shown instead of these, see: #124098. */
  if (!UI_block_has_active_default_button(layout->block())) {
    /* New column so as not to interfere with custom layouts, see: #26436. */
    uiLayout *col = &layout->column(false);
    uiBlock *col_block = col->block();
    uiBut *confirm_but;
    uiBut *cancel_but;

    col = &col->split(0.0f, true);
    col->scale_y_set(small ? 1.0f : 1.2f);

    if (windows_layout) {
      confirm_but = uiDefBut(col_block,
                             ButType::But,
                             0,
                             data->confirm_text.c_str(),
                             0,
                             0,
                             0,
                             UI_UNIT_Y,
                             nullptr,
                             0,
                             0,
                             "");
      col->column(false);
    }

    cancel_but = uiDefBut(
        col_block, ButType::But, 0, IFACE_("Cancel"), 0, 0, 0, UI_UNIT_Y, nullptr, 0, 0, "");

    if (!windows_layout) {
      col->column(false);
      confirm_but = uiDefBut(col_block,
                             ButType::But,
                             0,
                             data->confirm_text.c_str(),
                             0,
                             0,
                             0,
                             UI_UNIT_Y,
                             nullptr,
                             0,
                             0,
                             "");
    }

    UI_but_func_set(confirm_but, dialog_exec_cb, data, col_block);
    UI_but_func_set(cancel_but, dialog_cancel_cb, data, col_block);
    UI_but_flag_enable((data->cancel_default) ? cancel_but : confirm_but, UI_BUT_ACTIVE_DEFAULT);
  }

  const int padding = (small ? 7 : 14) * UI_SCALE_FAC;

  if (data->position == WM_POPUP_POSITION_MOUSE) {
    const float button_center_x = windows_layout ? -0.4f : -0.90f;
    const float button_center_y = small ? 2.0f : 3.1f;
    const int bounds_offset[2] = {int(button_center_x * layout->width()),
                                  int(button_center_y * UI_UNIT_X)};
    UI_block_bounds_set_popup(block, padding, bounds_offset);
  }
  else if (data->position == WM_POPUP_POSITION_CENTER) {
    UI_block_bounds_set_centered(block, padding);
  }

  return block;
}

static uiBlock *wm_operator_ui_create(bContext *C, ARegion *region, void *user_data)
{
  wmOpPopUp *data = static_cast<wmOpPopUp *>(user_data);
  wmOperator *op = data->op;
  const uiStyle *style = UI_style_get_dpi();

  uiBlock *block = UI_block_begin(C, region, __func__, blender::ui::EmbossType::Emboss);
  UI_block_flag_disable(block, UI_BLOCK_LOOP);
  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_MOVEMOUSE_QUIT);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_REGULAR);

  UI_popup_dummy_panel_set(region, block);

  uiLayout &layout = blender::ui::block_layout(block,
                                               blender::ui::LayoutDirection::Vertical,
                                               blender::ui::LayoutType::Panel,
                                               0,
                                               0,
                                               data->width,
                                               0,
                                               0,
                                               style);

  /* Since UI is defined the auto-layout args are not used. */
  uiTemplateOperatorPropertyButs(C, &layout, op, UI_BUT_LABEL_ALIGN_COLUMN, 0);

  UI_block_func_set(block, nullptr, nullptr, nullptr);

  UI_block_bounds_set_popup(block, 6 * UI_SCALE_FAC, nullptr);

  return block;
}

static void wm_operator_ui_popup_cancel(bContext *C, void *user_data)
{
  wmOpPopUp *data = static_cast<wmOpPopUp *>(user_data);
  wmOperator *op = data->op;

  if (op) {
    if (op->type->cancel) {
      op->type->cancel(C, op);
    }

    if (data->free_op) {
      WM_operator_free(op);
    }
  }

  MEM_delete(data);
}

static void wm_operator_ui_popup_ok(bContext *C, void *arg, int retval)
{
  wmOpPopUp *data = static_cast<wmOpPopUp *>(arg);
  wmOperator *op = data->op;

  if (op && retval > 0) {
    WM_operator_call_ex(C, op, true);
  }

  MEM_delete(data);
}

wmOperatorStatus WM_operator_confirm_ex(bContext *C,
                                        wmOperator *op,
                                        const char *title,
                                        const char *message,
                                        const char *confirm_text,
                                        int icon,
                                        bool cancel_default)
{
  wmOpPopUp *data = MEM_new<wmOpPopUp>(__func__);
  data->op = op;

  /* Larger dialog needs a wider minimum width to balance with the big icon. */
  const float min_width = (message == nullptr) ? 180.0f : 230.0f;
  data->width = int(min_width * UI_SCALE_FAC * UI_style_get()->widget.points /
                    UI_DEFAULT_TEXT_POINTS);

  data->free_op = true;
  data->title = (title == nullptr) ? WM_operatortype_name(op->type, op->ptr) : title;
  data->message = (message == nullptr) ? std::string() : message;
  data->confirm_text = (confirm_text == nullptr) ? IFACE_("OK") : confirm_text;
  data->icon = eAlertIcon(icon);
  data->size = (message == nullptr) ? WM_POPUP_SIZE_SMALL : WM_POPUP_SIZE_LARGE;
  data->position = (message == nullptr) ? WM_POPUP_POSITION_MOUSE : WM_POPUP_POSITION_CENTER;
  data->cancel_default = cancel_default;
  data->mouse_move_quit = (message == nullptr) ? true : false;
  data->include_properties = false;

  UI_popup_block_ex(
      C, wm_block_dialog_create, wm_operator_ui_popup_ok, wm_operator_ui_popup_cancel, data, op);

  return OPERATOR_RUNNING_MODAL;
}

wmOperatorStatus WM_operator_ui_popup(bContext *C, wmOperator *op, int width)
{
  wmOpPopUp *data = MEM_new<wmOpPopUp>(__func__);
  data->op = op;
  data->width = width * UI_SCALE_FAC;
  data->free_op = true; /* If this runs and gets registered we may want not to free it. */
  UI_popup_block_ex(C, wm_operator_ui_create, nullptr, wm_operator_ui_popup_cancel, data, op);
  return OPERATOR_RUNNING_MODAL;
}

/**
 * For use by #WM_operator_props_popup_call, #WM_operator_props_popup only.
 *
 * \note operator menu needs undo flag enabled, for redo callback.
 */
static wmOperatorStatus wm_operator_props_popup_ex(
    bContext *C,
    wmOperator *op,
    const bool do_call,
    const bool do_redo,
    std::optional<std::string> title = std::nullopt,
    std::optional<std::string> confirm_text = std::nullopt,
    const bool cancel_default = false,
    std::optional<std::string> message = std::nullopt)
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

  /* If we don't have global undo, we can't do undo push for automatic redo,
   * so we require manual OK clicking in this popup. */
  if (!do_redo || !(U.uiflag & USER_GLOBALUNDO)) {
    return WM_operator_props_dialog_popup(
        C, op, 300, title, confirm_text, cancel_default, message);
  }

  UI_popup_block_ex(C, wm_block_create_redo, nullptr, wm_block_redo_cancel_cb, op, op);

  if (do_call) {
    wm_block_redo_cb(C, op, 0);
  }

  return OPERATOR_RUNNING_MODAL;
}

wmOperatorStatus WM_operator_props_popup_confirm_ex(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent * /*event*/,
                                                    std::optional<std::string> title,
                                                    std::optional<std::string> confirm_text,
                                                    const bool cancel_default,
                                                    std::optional<std::string> message)
{
  return wm_operator_props_popup_ex(
      C, op, false, false, title, confirm_text, cancel_default, message);
}

wmOperatorStatus WM_operator_props_popup_confirm(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent * /*event*/)
{
  return wm_operator_props_popup_ex(C, op, false, false, {}, {});
}

wmOperatorStatus WM_operator_props_popup_call(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  return wm_operator_props_popup_ex(C, op, true, true);
}

wmOperatorStatus WM_operator_props_popup(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return wm_operator_props_popup_ex(C, op, false, true);
}

wmOperatorStatus WM_operator_props_dialog_popup(bContext *C,
                                                wmOperator *op,
                                                int width,
                                                std::optional<std::string> title,
                                                std::optional<std::string> confirm_text,
                                                const bool cancel_default,
                                                std::optional<std::string> message)
{
  wmOpPopUp *data = MEM_new<wmOpPopUp>(__func__);
  data->op = op;
  data->width = int(float(width) * UI_SCALE_FAC * UI_style_get()->widget.points /
                    UI_DEFAULT_TEXT_POINTS);
  data->free_op = true; /* If this runs and gets registered we may want not to free it. */
  data->title = title ? std::move(*title) : WM_operatortype_name(op->type, op->ptr);
  data->confirm_text = confirm_text ? std::move(*confirm_text) : IFACE_("OK");
  data->message = message ? std::move(*message) : std::string();
  data->icon = ALERT_ICON_NONE;
  data->size = WM_POPUP_SIZE_SMALL;
  data->position = (message) ? WM_POPUP_POSITION_CENTER : WM_POPUP_POSITION_MOUSE;
  data->cancel_default = cancel_default;
  data->mouse_move_quit = false;
  data->include_properties = true;

  /* The operator is not executed until popup OK button is clicked. */
  UI_popup_block_ex(
      C, wm_block_dialog_create, wm_operator_ui_popup_ok, wm_operator_ui_popup_cancel, data, op);

  return OPERATOR_RUNNING_MODAL;
}

wmOperatorStatus WM_operator_redo_popup(bContext *C, wmOperator *op)
{
  /* `CTX_wm_reports(C)` because operator is on stack, not active in event system. */
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

  /* Operator is stored and kept alive in the window manager. So passing a pointer to the UI is
   * fine, it will remain valid. */
  UI_popup_block_invoke(C, wm_block_create_redo, op, nullptr);

  return OPERATOR_CANCELLED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug Menu Operator
 *
 * Set internal debug value, mainly for developers.
 * \{ */

static wmOperatorStatus wm_debug_menu_exec(bContext *C, wmOperator *op)
{
  G.debug_value = RNA_int_get(op->ptr, "debug_value");
  ED_screen_refresh(C, CTX_wm_manager(C), CTX_wm_window(C));
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus wm_debug_menu_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  RNA_int_set(op->ptr, "debug_value", G.debug_value);
  return WM_operator_props_dialog_popup(C, op, 250, IFACE_("Set Debug Value"), IFACE_("Set"));
}

static void WM_OT_debug_menu(wmOperatorType *ot)
{
  ot->name = "Debug Menu";
  ot->idname = "WM_OT_debug_menu";
  ot->description = "Open a popup to set the debug level";

  ot->invoke = wm_debug_menu_invoke;
  ot->exec = wm_debug_menu_exec;
  ot->poll = WM_operator_winactive;

  ot->prop = RNA_def_int(
      ot->srna, "debug_value", 0, SHRT_MIN, SHRT_MAX, "Debug Value", "", -10000, 10000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reset Defaults Operator
 * \{ */

static wmOperatorStatus wm_operator_defaults_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "active_operator", &RNA_Operator);

  if (!ptr.data) {
    BKE_report(op->reports, RPT_ERROR, "No operator in context");
    return OPERATOR_CANCELLED;
  }

  WM_operator_properties_reset((wmOperator *)ptr.data);
  return OPERATOR_FINISHED;
}

/* Used by operator preset menu. pre-2.65 this was a 'Reset' button. */
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

enum SearchType {
  SEARCH_TYPE_OPERATOR = 0,
  SEARCH_TYPE_MENU = 1,
  SEARCH_TYPE_SINGLE_MENU = 2,
};

struct SearchPopupInit_Data {
  SearchType search_type;
  int size[2];
  std::string single_menu_idname;
};

static char g_search_text[256] = "";

static uiBlock *wm_block_search_menu(bContext *C, ARegion *region, void *userdata)
{
  const SearchPopupInit_Data *init_data = static_cast<const SearchPopupInit_Data *>(userdata);

  uiBlock *block = UI_block_begin(C, region, "_popup", blender::ui::EmbossType::Emboss);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiBut *but = uiDefSearchBut(block,
                              g_search_text,
                              0,
                              ICON_VIEWZOOM,
                              sizeof(g_search_text),
                              0,
                              0,
                              init_data->size[0],
                              UI_UNIT_Y,
                              "");

  if (init_data->search_type == SEARCH_TYPE_OPERATOR) {
    UI_but_func_operator_search(but);
  }
  else if (init_data->search_type == SEARCH_TYPE_MENU) {
    UI_but_func_menu_search(but);
  }
  else if (init_data->search_type == SEARCH_TYPE_SINGLE_MENU) {
    UI_but_func_menu_search(but, init_data->single_menu_idname.c_str());
    UI_but_flag2_enable(but, UI_BUT2_ACTIVATE_ON_INIT_NO_SELECT);
  }
  else {
    BLI_assert_unreachable();
  }

  UI_but_flag_enable(but, UI_BUT_ACTIVATE_ON_INIT);

  /* Fake button, it holds space for search items. */
  const int height = init_data->size[1] - UI_SEARCHBOX_BOUNDS;
  uiDefBut(block,
           ButType::Label,
           0,
           "",
           0,
           -height,
           init_data->size[0],
           height,
           nullptr,
           0,
           0,
           std::nullopt);

  /* Move it downwards, mouse over button. */
  UI_block_bounds_set_popup(block, UI_SEARCHBOX_BOUNDS, blender::int2{0, -UI_UNIT_Y});

  return block;
}

static wmOperatorStatus wm_search_menu_exec(bContext * /*C*/, wmOperator * /*op*/)
{
  return OPERATOR_FINISHED;
}

static wmOperatorStatus wm_search_menu_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Exception for launching via space-bar. */
  if (event->type == EVT_SPACEKEY) {
    bool ok = true;
    ScrArea *area = CTX_wm_area(C);
    if (area) {
      if (area->spacetype == SPACE_CONSOLE) {
        /* So we can use the shortcut in the console. */
        ok = false;
      }
      else if (area->spacetype == SPACE_TEXT) {
        /* So we can use the space-bar in the text editor. */
        ok = false;
      }
    }
    else {
      Object *editob = CTX_data_edit_object(C);
      if (editob && editob->type == OB_FONT) {
        /* So we can use the space-bar for entering text. */
        ok = false;
      }
    }
    if (!ok) {
      return OPERATOR_PASS_THROUGH;
    }
  }

  SearchType search_type;
  if (STREQ(op->type->idname, "WM_OT_search_menu")) {
    search_type = SEARCH_TYPE_MENU;
  }
  else if (STREQ(op->type->idname, "WM_OT_search_single_menu")) {
    search_type = SEARCH_TYPE_SINGLE_MENU;
  }
  else {
    search_type = SEARCH_TYPE_OPERATOR;
  }

  static SearchPopupInit_Data data{};

  if (search_type == SEARCH_TYPE_SINGLE_MENU) {
    data.single_menu_idname = RNA_string_get(op->ptr, "menu_idname");

    std::string buffer = RNA_string_get(op->ptr, "initial_query");
    STRNCPY(g_search_text, buffer.c_str());
  }
  else {
    g_search_text[0] = '\0';
  }

  data.search_type = search_type;
  data.size[0] = UI_searchbox_size_x() * 2;
  data.size[1] = UI_searchbox_size_y();

  UI_popup_block_invoke_ex(C, wm_block_search_menu, &data, nullptr, false);

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

static void WM_OT_search_single_menu(wmOperatorType *ot)
{
  ot->name = "Search Single Menu";
  ot->idname = "WM_OT_search_single_menu";
  ot->description = "Pop-up a search for a menu in current context";

  ot->invoke = wm_search_menu_invoke;
  ot->exec = wm_search_menu_exec;
  ot->poll = WM_operator_winactive;

  RNA_def_string(ot->srna, "menu_idname", nullptr, 0, "Menu Name", "Menu to search in");
  RNA_def_string(ot->srna,
                 "initial_query",
                 nullptr,
                 0,
                 "Initial Query",
                 "Query to insert into the search box");
}

static wmOperatorStatus wm_call_menu_exec(bContext *C, wmOperator *op)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(op->ptr, "name", idname);

  return UI_popup_menu_invoke(C, idname, op->reports);
}

static std::string wm_call_menu_get_name(wmOperatorType *ot, PointerRNA *ptr)
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

  prop = RNA_def_string(ot->srna, "name", nullptr, BKE_ST_MAXNAME, "Name", "Name of the menu");
  RNA_def_property_string_search_func_runtime(
      prop,
      WM_menutype_idname_visit_for_search,
      /* Only a suggestion as menu items may be referenced from add-ons that have been disabled. */
      (PROP_STRING_SEARCH_SORT | PROP_STRING_SEARCH_SUGGESTION));
}

static wmOperatorStatus wm_call_pie_menu_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(op->ptr, "name", idname);

  return UI_pie_menu_invoke(C, idname, event);
}

static wmOperatorStatus wm_call_pie_menu_exec(bContext *C, wmOperator *op)
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

  prop = RNA_def_string(ot->srna, "name", nullptr, BKE_ST_MAXNAME, "Name", "Name of the pie menu");
  RNA_def_property_string_search_func_runtime(
      prop,
      WM_menutype_idname_visit_for_search,
      /* Only a suggestion as menu items may be referenced from add-ons that have been disabled. */
      (PROP_STRING_SEARCH_SORT | PROP_STRING_SEARCH_SUGGESTION));
}

static wmOperatorStatus wm_call_panel_exec(bContext *C, wmOperator *op)
{
  char idname[BKE_ST_MAXNAME];
  RNA_string_get(op->ptr, "name", idname);
  const bool keep_open = RNA_boolean_get(op->ptr, "keep_open");

  return UI_popover_panel_invoke(C, idname, keep_open, op->reports);
}

static std::string wm_call_panel_get_name(wmOperatorType *ot, PointerRNA *ptr)
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

  prop = RNA_def_string(ot->srna, "name", nullptr, BKE_ST_MAXNAME, "Name", "Name of the menu");
  RNA_def_property_string_search_func_runtime(
      prop,
      WM_paneltype_idname_visit_for_search,
      /* Only a suggestion as menu items may be referenced from add-ons that have been disabled. */
      (PROP_STRING_SEARCH_SORT | PROP_STRING_SEARCH_SUGGESTION));
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna, "keep_open", true, "Keep Open", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static wmOperatorStatus asset_shelf_popover_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{
  std::string asset_shelf_id = RNA_string_get(op->ptr, "name");

  if (!blender::ui::asset_shelf_popover_invoke(*C, asset_shelf_id, *op->reports)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return OPERATOR_INTERFACE;
}

/* Needs to be defined at WM level to be globally accessible. */
static void WM_OT_call_asset_shelf_popover(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Call Asset Shelf Popover";
  ot->idname = "WM_OT_call_asset_shelf_popover";
  ot->description = "Open a predefined asset shelf in a popup";

  /* API callbacks. */
  ot->invoke = asset_shelf_popover_invoke;

  ot->flag = OPTYPE_INTERNAL;

  RNA_def_string(ot->srna,
                 "name",
                 nullptr,
                 0,
                 "Asset Shelf Name",
                 "Identifier of the asset shelf to display");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window/Screen Operators
 * \{ */

/**
 * This poll functions is needed in place of #WM_operator_winactive
 * while it crashes on full screen.
 */
static bool wm_operator_winactive_normal(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen;

  if (win == nullptr) {
    return false;
  }
  if (!((screen = WM_window_get_active_screen(win)) && (screen->state == SCREENNORMAL))) {
    return false;
  }
  if (G.background) {
    return false;
  }

  return true;
}

static bool wm_operator_winactive_not_full(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen;

  if (win == nullptr) {
    return false;
  }
  if (!((screen = WM_window_get_active_screen(win)) && (screen->state != SCREENFULL))) {
    return false;
  }
  if (G.background) {
    return false;
  }

  return true;
}

/* Included for script-access. */
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
  ot->poll = wm_operator_winactive_not_full;
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

static wmOperatorStatus wm_exit_blender_exec(bContext *C, wmOperator * /*op*/)
{
  wm_exit_schedule_delayed(C);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus wm_exit_blender_invoke(bContext *C,
                                               wmOperator * /*op*/,
                                               const wmEvent * /*event*/)
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

static wmOperatorStatus wm_console_toggle_exec(bContext * /*C*/, wmOperator * /*op*/)
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
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);

  wmPaintCursor *pc = MEM_callocN<wmPaintCursor>("paint cursor");

  BLI_addtail(&wm->runtime->paintcursors, pc);

  pc->customdata = customdata;
  pc->poll = poll;
  pc->draw = draw;

  pc->space_type = space_type;
  pc->region_type = region_type;

  return pc;
}

bool WM_paint_cursor_end(wmPaintCursor *handle)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  LISTBASE_FOREACH (wmPaintCursor *, pc, &wm->runtime->paintcursors) {
    if (pc == handle) {
      BLI_remlink(&wm->runtime->paintcursors, pc);
      MEM_freeN(pc);
      return true;
    }
  }
  return false;
}

void WM_paint_cursor_remove_by_type(wmWindowManager *wm, void *draw_fn, void (*free)(void *))
{
  LISTBASE_FOREACH_MUTABLE (wmPaintCursor *, pc, &wm->runtime->paintcursors) {
    if (pc->draw == draw_fn) {
      if (free) {
        free(pc->customdata);
      }
      BLI_remlink(&wm->runtime->paintcursors, pc);
      MEM_freeN(pc);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Radial Control Operator
 * \{ */

#define WM_RADIAL_CONTROL_DISPLAY_SIZE (200 * UI_SCALE_FAC)
#define WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE (35 * UI_SCALE_FAC)
#define WM_RADIAL_CONTROL_DISPLAY_WIDTH \
  (WM_RADIAL_CONTROL_DISPLAY_SIZE - WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE)
#define WM_RADIAL_MAX_STR 10

struct RadialControl {
  PropertyType type;
  PropertySubType subtype;
  PointerRNA ptr, col_ptr, fill_col_ptr, rot_ptr, zoom_ptr, image_id_ptr;
  PointerRNA fill_col_override_ptr, fill_col_override_test_ptr;
  PropertyRNA *prop = nullptr;
  PropertyRNA *col_prop = nullptr;
  PropertyRNA *fill_col_prop = nullptr;
  PropertyRNA *rot_prop = nullptr;
  PropertyRNA *zoom_prop = nullptr;
  PropertyRNA *fill_col_override_prop = nullptr;
  PropertyRNA *fill_col_override_test_prop = nullptr;
  StructRNA *image_id_srna = nullptr;
  float initial_value = 0.0f;
  float current_value = 0.0f;
  float min_value = 0.0f;
  float max_value = 0.0f;
  /* Original screen space coordinates that the operator started on. */
  int initial_co[2] = {};
  /* Modified value of #initial_co to simplify calculating new values. */
  int initial_radial_center[2] = {};
  int slow_mouse[2] = {};
  bool slow_mode = false;
  Dial *dial = nullptr;
  blender::gpu::Texture *texture = nullptr;
  ListBase orig_paintcursors = {};
  bool use_secondary_tex = false;
  void *cursor = nullptr;
  NumInput num_input = {};
  int init_event = 0;
};

static void radial_control_update_header(wmOperator *op, bContext *C)
{
  RadialControl *rc = static_cast<RadialControl *>(op->customdata);
  char msg[UI_MAX_DRAW_STR];
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);

  if (hasNumInput(&rc->num_input)) {
    char num_str[NUM_STR_REP_LEN];
    outputNumInput(&rc->num_input, num_str, scene->unit);
    SNPRINTF(msg, "%s: %s", RNA_property_ui_name(rc->prop), num_str);
  }
  else {
    const char *ui_name = RNA_property_ui_name(rc->prop);
    switch (rc->subtype) {
      case PROP_NONE:
      case PROP_DISTANCE:
      case PROP_DISTANCE_DIAMETER:
        SNPRINTF(msg, "%s: %0.4f", ui_name, rc->current_value);
        break;
      case PROP_PIXEL:
      case PROP_PIXEL_DIAMETER:
        SNPRINTF(msg, "%s: %d", ui_name, int(rc->current_value)); /* XXX: round to nearest? */
        break;
      case PROP_PERCENTAGE:
        SNPRINTF(msg, "%s: %3.1f%%", ui_name, rc->current_value);
        break;
      case PROP_FACTOR:
        SNPRINTF(msg, "%s: %1.3f", ui_name, rc->current_value);
        break;
      case PROP_ANGLE:
        SNPRINTF(msg, "%s: %3.2f", ui_name, RAD2DEGF(rc->current_value));
        break;
      default:
        STRNCPY(msg, ui_name); /* XXX: No value? */
        break;
    }
  }

  ED_area_status_text(area, msg);
}

static void radial_control_set_initial_mouse(RadialControl *rc, const wmEvent *event)
{
  float d[2] = {0, 0};
  float zoom[2] = {1, 1};

  copy_v2_v2_int(rc->initial_radial_center, event->xy);
  copy_v2_v2_int(rc->initial_co, event->xy);

  switch (rc->subtype) {
    case PROP_NONE:
    case PROP_DISTANCE:
    case PROP_DISTANCE_DIAMETER:
    case PROP_PIXEL:
    case PROP_PIXEL_DIAMETER:
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

  rc->initial_radial_center[0] -= d[0];
  rc->initial_radial_center[1] -= d[1];
}

static void radial_control_set_tex(RadialControl *rc)
{
  ImBuf *ibuf;

  switch (RNA_type_to_ID_code(rc->image_id_ptr.type)) {
    case ID_BR:
      if ((ibuf = BKE_brush_gen_radial_control_imbuf(static_cast<Brush *>(rc->image_id_ptr.data),
                                                     rc->use_secondary_tex,
                                                     !ELEM(rc->subtype,
                                                           PROP_NONE,
                                                           PROP_PIXEL,
                                                           PROP_PIXEL_DIAMETER,
                                                           PROP_DISTANCE,
                                                           PROP_DISTANCE_DIAMETER))))
      {

        rc->texture = GPU_texture_create_2d("radial_control",
                                            ibuf->x,
                                            ibuf->y,
                                            1,
                                            blender::gpu::TextureFormat::UNORM_8,
                                            GPU_TEXTURE_USAGE_SHADER_READ,
                                            ibuf->float_buffer.data);

        GPU_texture_filter_mode(rc->texture, true);
        GPU_texture_swizzle_set(rc->texture, "111r");

        MEM_freeN(ibuf->float_buffer.data);
        MEM_freeN(ibuf);
      }
      break;
    default:
      break;
  }
}

static void radial_control_paint_tex(RadialControl *rc, float radius, float alpha)
{

  /* Set fill color. */
  float col[3] = {0, 0, 0};
  if (rc->fill_col_prop) {
    PointerRNA *fill_ptr;
    PropertyRNA *fill_prop;

    if (rc->fill_col_override_prop &&
        RNA_property_boolean_get(&rc->fill_col_override_test_ptr, rc->fill_col_override_test_prop))
    {
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
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  if (rc->texture) {
    uint texCoord = GPU_vertformat_attr_add(
        format, "texCoord", blender::gpu::VertAttrType::SFLOAT_32_32);

    /* Set up rotation if available. */
    if (rc->rot_prop) {
      float rot = RNA_property_float_get(&rc->rot_ptr, rc->rot_prop);
      GPU_matrix_push();
      GPU_matrix_rotate_2d(RAD2DEGF(rot));
    }

    immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);

    immUniformColor3fvAlpha(col, alpha);
    immBindTexture("image", rc->texture);

    /* Draw textured quad. */
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

    /* Undo rotation. */
    if (rc->rot_prop) {
      GPU_matrix_pop();
    }
  }
  else {
    /* Flat color if no texture available. */
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor3fvAlpha(col, alpha);
    imm_draw_circle_fill_2d(pos, 0.0f, 0.0f, radius, 40);
  }

  immUnbindProgram();
}

static void radial_control_paint_curve(uint pos, Brush *br, float radius, int line_segments)
{
  GPU_line_width(2.0f);
  immUniformColor4f(0.8f, 0.8f, 0.8f, 0.85f);
  float step = (radius * 2.0f) / float(line_segments);
  BKE_curvemapping_init(br->curve_distance_falloff);
  immBegin(GPU_PRIM_LINES, line_segments * 2);
  for (int i = 0; i < line_segments; i++) {
    float h1 = BKE_brush_curve_strength_clamped(br, fabsf((i * step) - radius), radius);
    immVertex2f(pos, -radius + (i * step), h1 * radius);
    float h2 = BKE_brush_curve_strength_clamped(br, fabsf(((i + 1) * step) - radius), radius);
    immVertex2f(pos, -radius + ((i + 1) * step), h2 * radius);
  }
  immEnd();
}

static void radial_control_paint_cursor(bContext * /*C*/,
                                        const blender::int2 & /*xy*/,
                                        const blender::float2 & /*tilt*/,
                                        void *customdata)
{
  RadialControl *rc = static_cast<RadialControl *>(customdata);
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
    case PROP_DISTANCE_DIAMETER:
    case PROP_PIXEL_DIAMETER:
      r1 = rc->current_value / 2.0f;
      r2 = rc->initial_value / 2.0f;
      tex_radius = r1;
      alpha = 0.75;
      break;
    case PROP_PERCENTAGE:
      r1 = rc->current_value / 100.0f * WM_RADIAL_CONTROL_DISPLAY_WIDTH +
           WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
      rmin = WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      SNPRINTF(str, "%3.1f%%", rc->current_value);
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
      SNPRINTF(str, "%1.3f", rc->current_value);
      strdrawlen = BLI_strlen_utf8(str);
      break;
    case PROP_ANGLE:
      r1 = r2 = tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE;
      alpha = 0.75;
      rmin = WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE;
      SNPRINTF(str, "%3.2f", RAD2DEGF(rc->current_value));
      strdrawlen = BLI_strlen_utf8(str);
      break;
    default:
      tex_radius = WM_RADIAL_CONTROL_DISPLAY_SIZE; /* NOTE: this is a dummy value. */
      alpha = 0.75;
      break;
  }

  int x, y;
  if (rc->subtype == PROP_ANGLE) {
    /* Use the initial mouse position to draw the rotation preview. This avoids starting the
     * rotation in a random direction. */
    x = rc->initial_radial_center[0];
    y = rc->initial_radial_center[1];
  }
  else {
    /* Keep cursor in the original place. */
    x = rc->initial_co[0];
    y = rc->initial_co[1];
  }
  GPU_matrix_translate_2f(float(x), float(y));

  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);

  /* Apply zoom if available. */
  if (rc->zoom_prop) {
    RNA_property_float_get_array(&rc->zoom_ptr, rc->zoom_prop, zoom);
    GPU_matrix_scale_2fv(zoom);
  }

  /* Draw rotated texture. */
  radial_control_paint_tex(rc, tex_radius, alpha);

  /* Set line color. */
  if (rc->col_prop) {
    RNA_property_float_get_array(&rc->col_ptr, rc->col_prop, col);
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  if (rc->subtype == PROP_ANGLE) {
    GPU_matrix_push();

    /* Draw original angle line. */
    GPU_matrix_rotate_3f(RAD2DEGF(rc->initial_value), 0.0f, 0.0f, 1.0f);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, float(WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE), 0.0f);
    immVertex2f(pos, float(WM_RADIAL_CONTROL_DISPLAY_SIZE), 0.0f);
    immEnd();

    /* Draw new angle line. */
    GPU_matrix_rotate_3f(RAD2DEGF(rc->current_value - rc->initial_value), 0.0f, 0.0f, 1.0f);
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, float(WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE), 0.0f);
    immVertex2f(pos, float(WM_RADIAL_CONTROL_DISPLAY_SIZE), 0.0f);
    immEnd();

    GPU_matrix_pop();
  }

  /* Draw circles on top. */
  GPU_line_width(2.0f);
  immUniformColor3fvAlpha(col, 0.8f);
  imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, r1, 80);

  GPU_line_width(1.0f);
  immUniformColor3fvAlpha(col, 0.5f);
  imm_draw_circle_wire_2d(pos, 0.0f, 0.0f, r2, 80);
  if (rmin > 0.0f) {
    /* Inner fill circle to increase the contrast of the value. */
    const float black[3] = {0.0f};
    immUniformColor3fvAlpha(black, 0.2f);
    imm_draw_circle_fill_2d(pos, 0.0, 0.0f, rmin, 80);

    immUniformColor3fvAlpha(col, 0.5f);
    imm_draw_circle_wire_2d(pos, 0.0, 0.0f, rmin, 80);
  }

  /* Draw curve falloff preview. */
  if (RNA_type_to_ID_code(rc->image_id_ptr.type) == ID_BR && rc->subtype == PROP_FACTOR) {
    Brush *br = static_cast<Brush *>(rc->image_id_ptr.data);
    if (br) {
      radial_control_paint_curve(pos, br, r2, 120);
    }
  }

  immUnbindProgram();

  BLF_size(fontid, 1.75f * fstyle_points * UI_SCALE_FAC);
  UI_GetThemeColor4fv(TH_TEXT_HI, text_color);
  BLF_color4fv(fontid, text_color);

  /* Draw value. */
  BLF_width_and_height(fontid, str, strdrawlen, &strwidth, &strheight);
  BLF_position(fontid, -0.5f * strwidth, -0.5f * strheight, 0.0f);
  BLF_draw(fontid, str, strdrawlen);

  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

enum RCPropFlags {
  RC_PROP_ALLOW_MISSING = 1,
  RC_PROP_REQUIRE_FLOAT = 2,
  RC_PROP_REQUIRE_BOOL = 4,
};

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

  /* Check flags. */
  if ((flags & RC_PROP_REQUIRE_BOOL) && (flags & RC_PROP_REQUIRE_FLOAT)) {
    BKE_report(op->reports, RPT_ERROR, "Property cannot be both boolean and float");
    return 0;
  }

  /* Get an rna string path from the operator's properties. */
  std::string str = RNA_string_get(op->ptr, name);
  if (str.empty()) {
    if (r_prop) {
      *r_prop = nullptr;
    }
    return 1;
  }

  if (!r_prop) {
    r_prop = &unused_prop;
  }

  /* Get rna from path. */
  if (!RNA_path_resolve(ctx_ptr, str.c_str(), r_ptr, r_prop)) {
    if (flags & RC_PROP_ALLOW_MISSING) {
      return 1;
    }
    BKE_reportf(op->reports, RPT_ERROR, "Could not resolve path '%s'", name);
    return 0;
  }

  /* Check property type. */
  if (flags & (RC_PROP_REQUIRE_BOOL | RC_PROP_REQUIRE_FLOAT)) {
    PropertyType prop_type = RNA_property_type(*r_prop);

    if (((flags & RC_PROP_REQUIRE_BOOL) && (prop_type != PROP_BOOLEAN)) ||
        ((flags & RC_PROP_REQUIRE_FLOAT) && (prop_type != PROP_FLOAT)))
    {
      BKE_reportf(op->reports, RPT_ERROR, "Property from path '%s' is not a float", name);
      return 0;
    }
  }

  /* Check property's array length. */
  int len;
  if (*r_prop && (len = RNA_property_array_length(r_ptr, *r_prop)) != req_length) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Property from path '%s' has length %d instead of %d",
                name,
                len,
                req_length);
    return 0;
  }

  /* Success. */
  return 1;
}

/* Initialize the rna pointers and properties using rna paths. */
static int radial_control_get_properties(bContext *C, wmOperator *op)
{
  RadialControl *rc = static_cast<RadialControl *>(op->customdata);

  PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Context, C);

  /* Check if we use primary or secondary path. */
  PointerRNA use_secondary_ptr;
  PropertyRNA *use_secondary_prop = nullptr;
  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "use_secondary",
                               &use_secondary_ptr,
                               &use_secondary_prop,
                               0,
                               RCPropFlags(RC_PROP_ALLOW_MISSING | RC_PROP_REQUIRE_BOOL)))
  {
    return 0;
  }

  const char *data_path;
  if (use_secondary_prop && RNA_property_boolean_get(&use_secondary_ptr, use_secondary_prop)) {
    data_path = "data_path_secondary";
  }
  else {
    data_path = "data_path_primary";
  }

  if (!radial_control_get_path(&ctx_ptr, op, data_path, &rc->ptr, &rc->prop, 0, RCPropFlags(0))) {
    return 0;
  }

  /* Data path is required. */
  if (!rc->prop) {
    return 0;
  }

  if (!radial_control_get_path(
          &ctx_ptr, op, "rotation_path", &rc->rot_ptr, &rc->rot_prop, 0, RC_PROP_REQUIRE_FLOAT))
  {
    return 0;
  }

  if (!radial_control_get_path(
          &ctx_ptr, op, "color_path", &rc->col_ptr, &rc->col_prop, 4, RC_PROP_REQUIRE_FLOAT))
  {
    return 0;
  }

  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "fill_color_path",
                               &rc->fill_col_ptr,
                               &rc->fill_col_prop,
                               3,
                               RC_PROP_REQUIRE_FLOAT))
  {
    return 0;
  }

  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "fill_color_override_path",
                               &rc->fill_col_override_ptr,
                               &rc->fill_col_override_prop,
                               3,
                               RC_PROP_REQUIRE_FLOAT))
  {
    return 0;
  }
  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "fill_color_override_test_path",
                               &rc->fill_col_override_test_ptr,
                               &rc->fill_col_override_test_prop,
                               0,
                               RC_PROP_REQUIRE_BOOL))
  {
    return 0;
  }

  /* Slightly ugly; allow this property to not resolve correctly.
   * Needed because 3d texture paint shares the same key-map as 2d image paint. */
  if (!radial_control_get_path(&ctx_ptr,
                               op,
                               "zoom_path",
                               &rc->zoom_ptr,
                               &rc->zoom_prop,
                               2,
                               RCPropFlags(RC_PROP_REQUIRE_FLOAT | RC_PROP_ALLOW_MISSING)))
  {
    return 0;
  }

  if (!radial_control_get_path(
          &ctx_ptr, op, "image_id", &rc->image_id_ptr, nullptr, 0, RCPropFlags(0)))
  {
    return 0;
  }
  if (rc->image_id_ptr.data) {
    /* Extra check, pointer must be to an ID. */
    if (!RNA_struct_is_ID(rc->image_id_ptr.type)) {
      BKE_report(op->reports, RPT_ERROR, "Pointer from path image_id is not an ID");
      return 0;
    }
  }

  rc->use_secondary_tex = RNA_boolean_get(op->ptr, "secondary_tex");

  return 1;
}

static wmOperatorStatus radial_control_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  op->customdata = MEM_new<RadialControl>(__func__);
  if (!op->customdata) {
    return OPERATOR_CANCELLED;
  }
  RadialControl *rc = static_cast<RadialControl *>(op->customdata);

  if (!radial_control_get_properties(C, op)) {
    MEM_delete(rc);
    return OPERATOR_CANCELLED;
  }

  /* Get type, initial, min, and max values of the property. */
  switch (rc->type = RNA_property_type(rc->prop)) {
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
      MEM_delete(rc);
      return OPERATOR_CANCELLED;
  }

  /* Initialize numerical input. */
  initNumInput(&rc->num_input);
  rc->num_input.idx_max = 0;
  rc->num_input.val_flag[0] |= NUM_NO_NEGATIVE;
  rc->num_input.unit_sys = USER_UNIT_NONE;
  rc->num_input.unit_type[0] = RNA_SUBTYPE_UNIT_VALUE(RNA_property_unit(rc->prop));

  /* Get subtype of property. */
  rc->subtype = RNA_property_subtype(rc->prop);
  if (!ELEM(rc->subtype,
            PROP_NONE,
            PROP_DISTANCE,
            PROP_DISTANCE_DIAMETER,
            PROP_FACTOR,
            PROP_PERCENTAGE,
            PROP_ANGLE,
            PROP_PIXEL,
            PROP_PIXEL_DIAMETER))
  {
    BKE_report(op->reports,
               RPT_ERROR,
               "Property must be a none, distance, factor, percentage, angle, or pixel");
    MEM_delete(rc);
    return OPERATOR_CANCELLED;
  }

  rc->current_value = rc->initial_value;
  radial_control_set_initial_mouse(rc, event);
  radial_control_set_tex(rc);

  rc->init_event = WM_userdef_event_type_from_keymap_type(event->type);

  /* Temporarily disable other paint cursors. */
  wmWindowManager *wm = CTX_wm_manager(C);
  rc->orig_paintcursors = wm->runtime->paintcursors;
  BLI_listbase_clear(&wm->runtime->paintcursors);

  /* Add radial control paint cursor. */
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
  RadialControl *rc = static_cast<RadialControl *>(op->customdata);
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area = CTX_wm_area(C);

  if (rc->dial) {
    BLI_dial_free(rc->dial);
    rc->dial = nullptr;
  }

  ED_area_status_text(area, nullptr);

  WM_paint_cursor_end(static_cast<wmPaintCursor *>(rc->cursor));

  /* Restore original paint cursors. */
  wm->runtime->paintcursors = rc->orig_paintcursors;

  /* Not sure if this is a good notifier to use;
   * intended purpose is to update the UI so that the
   * new value is displayed in sliders/number-fields. */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  if (rc->texture != nullptr) {
    GPU_texture_free(rc->texture);
  }

  MEM_delete(rc);
}

static wmOperatorStatus radial_control_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  RadialControl *rc = static_cast<RadialControl *>(op->customdata);
  float new_value, dist = 0.0f, zoom[2];
  float delta[2];
  wmOperatorStatus ret = OPERATOR_RUNNING_MODAL;
  float angle_precision = 0.0f;
  const bool has_numInput = hasNumInput(&rc->num_input);
  bool handled = false;
  float numValue;
  /* TODO: fix hard-coded events. */

  bool snap = (event->modifier & KM_CTRL) != 0;

  /* Modal numinput active, try to handle numeric inputs first... */
  if (event->val == KM_PRESS && has_numInput && handleNumInput(C, &rc->num_input, event)) {
    handled = true;
    applyNumInput(&rc->num_input, &numValue);

    if (rc->subtype == PROP_ANGLE) {
      numValue = fmod(numValue, 2.0f * float(M_PI));
      if (numValue < 0.0f) {
        numValue += 2.0f * float(M_PI);
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
      /* Canceled; restore original value. */
      if (rc->init_event != RIGHTMOUSE) {
        radial_control_set_value(rc, rc->initial_value);
        ret = OPERATOR_CANCELLED;
      }
      break;

    case LEFTMOUSE:
    case EVT_PADENTER:
    case EVT_RETKEY:
      /* Done; value already set. */
      /* Keep the RNA update separate from setting the value, for some properties this could lead
       * to a continues flickering due to invalidating the overlay texture. */
      RNA_property_update(C, &rc->ptr, rc->prop);
      ret = OPERATOR_FINISHED;
      break;

    case MOUSEMOVE:
      if (!has_numInput) {
        if (rc->slow_mode) {
          if (rc->subtype == PROP_ANGLE) {
            /* Calculate the initial angle here first. */
            delta[0] = rc->initial_radial_center[0] - rc->slow_mouse[0];
            delta[1] = rc->initial_radial_center[1] - rc->slow_mouse[1];

            /* Precision angle gets calculated from dial and gets added later. */
            angle_precision = -0.1f * BLI_dial_angle(rc->dial,
                                                     blender::float2{float(event->xy[0]),
                                                                     float(event->xy[1])});
          }
          else {
            delta[0] = rc->initial_radial_center[0] - rc->slow_mouse[0];
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
          delta[0] = float(rc->initial_radial_center[0] - event->xy[0]);
          delta[1] = float(rc->initial_radial_center[1] - event->xy[1]);
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
          case PROP_DISTANCE_DIAMETER:
          case PROP_PIXEL:
          case PROP_PIXEL_DIAMETER:
            new_value = dist;
            if (snap) {
              new_value = (int(new_value) + 5) / 10 * 10;
            }
            break;
          case PROP_PERCENTAGE:
            new_value = ((dist - WM_RADIAL_CONTROL_DISPLAY_MIN_SIZE) /
                         WM_RADIAL_CONTROL_DISPLAY_WIDTH) *
                        100.0f;
            if (snap) {
              new_value = int(new_value + 2.5f) / 5 * 5;
            }
            break;
          case PROP_FACTOR:
            new_value = (WM_RADIAL_CONTROL_DISPLAY_SIZE - dist) / WM_RADIAL_CONTROL_DISPLAY_WIDTH;
            if (snap) {
              new_value = (int(ceil(new_value * 10.0f)) * 10.0f) / 100.0f;
            }
            /* Invert new value to increase the factor moving the mouse to the right. */
            new_value = 1 - new_value;
            break;
          case PROP_ANGLE:
            new_value = atan2f(delta[1], delta[0]) + float(M_PI) + angle_precision;
            new_value = fmod(new_value, 2.0f * float(M_PI));
            if (new_value < 0.0f) {
              new_value += 2.0f * float(M_PI);
            }
            if (snap) {
              new_value = DEG2RADF((int(RAD2DEGF(new_value)) + 5) / 10 * 10);
            }
            break;
          default:
            new_value = dist; /* NOTE(@ideasman42): Dummy value, should this ever happen? */
            break;
        }

        /* Clamp and update. */
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
          const float initial_position[2] = {float(rc->initial_radial_center[0]),
                                             float(rc->initial_radial_center[1])};
          const float current_position[2] = {float(rc->slow_mouse[0]), float(rc->slow_mouse[1])};
          rc->dial = BLI_dial_init(initial_position, 0.0f);
          /* Immediately set the position to get a an initial direction. */
          BLI_dial_angle(rc->dial, current_position);
        }
        handled = true;
      }
      if (event->val == KM_RELEASE) {
        rc->slow_mode = false;
        handled = true;
        if (rc->dial) {
          BLI_dial_free(rc->dial);
          rc->dial = nullptr;
        }
      }
      break;
    }
    default: {
      break;
    }
  }

  /* Modal numinput inactive, try to handle numeric inputs last... */
  if (!handled && event->val == KM_PRESS && handleNumInput(C, &rc->num_input, event)) {
    applyNumInput(&rc->num_input, &numValue);

    if (rc->subtype == PROP_ANGLE) {
      numValue = fmod(numValue, 2.0f * float(M_PI));
      if (numValue < 0.0f) {
        numValue += 2.0f * float(M_PI);
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
      RNA_boolean_get(op->ptr, "release_confirm"))
  {
    /* Keep the RNA update separate from setting the value, for some properties this could lead to
     * a continues flickering due to invalidating the overlay texture. */
    RNA_property_update(C, &rc->ptr, rc->prop);
    ret = OPERATOR_FINISHED;
  }

  ED_region_tag_redraw(CTX_wm_region(C));
  radial_control_update_header(op, C);

  if (ret & OPERATOR_FINISHED) {
    wmWindowManager *wm = CTX_wm_manager(C);
    if (wm->op_undo_depth == 0) {
      ID *id = rc->ptr.owner_id;
      if (ED_undo_is_legacy_compatible_for_property(C, id, rc->ptr)) {
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

  /* All paths relative to the context. */
  PropertyRNA *prop;
  prop = RNA_def_string(ot->srna,
                        "data_path_primary",
                        nullptr,
                        0,
                        "Primary Data Path",
                        "Primary path of property to be set by the radial control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "data_path_secondary",
                        nullptr,
                        0,
                        "Secondary Data Path",
                        "Secondary path of property to be set by the radial control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "use_secondary",
                        nullptr,
                        0,
                        "Use Secondary",
                        "Path of property to select between the primary and secondary data paths");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "rotation_path",
                        nullptr,
                        0,
                        "Rotation Path",
                        "Path of property used to rotate the texture display");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "color_path",
                        nullptr,
                        0,
                        "Color Path",
                        "Path of property used to set the color of the control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "fill_color_path",
                        nullptr,
                        0,
                        "Fill Color Path",
                        "Path of property used to set the fill color of the control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(
      ot->srna, "fill_color_override_path", nullptr, 0, "Fill Color Override Path", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_string(
      ot->srna, "fill_color_override_test_path", nullptr, 0, "Fill Color Override Test", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "zoom_path",
                        nullptr,
                        0,
                        "Zoom Path",
                        "Path of property used to set the zoom level for the control");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_string(ot->srna,
                        "image_id",
                        nullptr,
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

/* Uses no type defines, fully local testing function anyway. */

static void redraw_timer_window_swap(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);

  CTX_wm_region_popup_set(C, nullptr);

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
    {0, nullptr, 0, nullptr, nullptr},
};

static void redraw_timer_step(bContext *C,
                              Scene *scene,
                              Depsgraph *depsgraph,
                              wmWindow *win,
                              ScrArea *area,
                              ARegion *region,
                              const int type,
                              const int cfra,
                              const int steps_done,
                              const int steps_total)
{
  if (type == eRTDrawRegion) {
    if (region) {
      wm_draw_region_test(C, area, region);
    }
  }
  else if (type == eRTDrawRegionSwap) {
    CTX_wm_region_popup_set(C, nullptr);

    ED_region_tag_redraw(region);
    wm_draw_update(C);

    CTX_wm_window_set(C, win); /* XXX context manipulation warning! */
  }
  else if (type == eRTDrawWindow) {
    bScreen *screen = WM_window_get_active_screen(win);

    CTX_wm_region_popup_set(C, nullptr);

    LISTBASE_FOREACH (ScrArea *, area_iter, &screen->areabase) {
      CTX_wm_area_set(C, area_iter);
      LISTBASE_FOREACH (ARegion *, region_iter, &area_iter->regionbase) {
        if (!region_iter->runtime->visible) {
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
    /* Play anim, return on same frame as started with. */
    int tot = (scene->r.efra - scene->r.sfra) + 1;
    const int frames_total = tot * steps_total;
    int frames_done = tot * steps_done;

    while (tot--) {
      WM_progress_set(win, float(frames_done) / float(frames_total));
      frames_done++;

      /* TODO: ability to escape! */
      scene->r.cfra++;
      if (scene->r.cfra > scene->r.efra) {
        scene->r.cfra = scene->r.sfra;
      }

      BKE_scene_graph_update_for_newframe(depsgraph);
      redraw_timer_window_swap(C);
    }
  }
  else { /* #eRTUndo. */
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
   * NOTE(@ideasman42): if it's useful to support undo or animation step this could
   * be allowed at the moment this seems like a corner case that isn't needed. */
  return !G.background && WM_operator_winactive(C);
}

static wmOperatorStatus redraw_timer_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  const int type = RNA_enum_get(op->ptr, "type");
  const int iter = RNA_int_get(op->ptr, "iterations");
  const double time_limit = double(RNA_float_get(op->ptr, "time_limit"));
  const int cfra = scene->r.cfra;
  const char *infostr = "";

  /* NOTE: Depsgraph is used to update scene for a new state, so no need to ensure evaluation
   * here.
   */
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);

  RNA_enum_description(redraw_timer_type_items, type, &infostr);

  WM_cursor_wait(true);

  double time_start = BLI_time_now_seconds();

  wm_window_make_drawable(wm, win);

  int iter_steps = 0;
  for (int a = 0; a < iter; a++) {

    if (type == eRTAnimationPlay) {
      WorkspaceStatus status(C);
      status.item(fmt::format("{} / {} {}", a + 1, iter, infostr), ICON_INFO);
    }

    redraw_timer_step(C, scene, depsgraph, win, area, region, type, cfra, a, iter);
    iter_steps += 1;

    if (time_limit != 0.0) {
      if ((BLI_time_now_seconds() - time_start) > time_limit) {
        break;
      }
      a = 0;
    }
  }

  double time_delta = (BLI_time_now_seconds() - time_start) * 1000;

  if (type == eRTAnimationPlay) {
    ED_workspace_status_text(C, nullptr);
    WM_progress_clear(win);
  }

  WM_cursor_wait(false);

  BKE_reportf(op->reports,
              RPT_WARNING,
              "%d \u00D7 %s: %.4f ms, average: %.8f ms",
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

static wmOperatorStatus memory_statistics_exec(bContext * /*C*/, wmOperator * /*op*/)
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

struct PreviewsIDEnsureData {
  bContext *C;
  Scene *scene;
};

static void previews_id_ensure(bContext *C, Scene *scene, ID *id)
{
  BLI_assert(ELEM(GS(id->name), ID_MA, ID_TE, ID_IM, ID_WO, ID_LA));

  /* Only preview non-library datablocks, lib ones do not pertain to this .blend file!
   * Same goes for ID with no user. */
  if (ID_IS_EDITABLE(id) && (id->us != 0)) {
    UI_icon_render_id(C, scene, id, ICON_SIZE_ICON, false);
    UI_icon_render_id(C, scene, id, ICON_SIZE_PREVIEW, false);
  }
}

static int previews_id_ensure_callback(LibraryIDLinkCallbackData *cb_data)
{
  const LibraryForeachIDCallbackFlag cb_flag = cb_data->cb_flag;

  if (cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
    return IDWALK_RET_NOP;
  }

  PreviewsIDEnsureData *data = static_cast<PreviewsIDEnsureData *>(cb_data->user_data);
  ID *id = *cb_data->id_pointer;

  if (id && (id->tag & ID_TAG_DOIT)) {
    BLI_assert(ELEM(GS(id->name), ID_MA, ID_TE, ID_IM, ID_WO, ID_LA));
    previews_id_ensure(data->C, data->scene, id);
    id->tag &= ~ID_TAG_DOIT;
  }

  return IDWALK_RET_NOP;
}

static wmOperatorStatus previews_ensure_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  ListBase *lb[] = {&bmain->materials,
                    &bmain->textures,
                    &bmain->images,
                    &bmain->worlds,
                    &bmain->lights,
                    nullptr};
  PreviewsIDEnsureData preview_id_data;

  /* We use ID_TAG_DOIT to check whether we have already handled a given ID or not. */
  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);
  for (int i = 0; lb[i]; i++) {
    BKE_main_id_tag_listbase(lb[i], ID_TAG_DOIT, true);
  }

  preview_id_data.C = C;
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    preview_id_data.scene = scene;
    ID *id = (ID *)scene;

    BKE_library_foreach_ID_link(
        nullptr, id, previews_id_ensure_callback, &preview_id_data, IDWALK_RECURSE);
  }

  /* Check a last time for ID not used (fake users only, in theory), and
   * do our best for those, using current scene... */
  for (int i = 0; lb[i]; i++) {
    LISTBASE_FOREACH (ID *, id, lb[i]) {
      if (id->tag & ID_TAG_DOIT) {
        previews_id_ensure(C, nullptr, id);
        id->tag &= ~ID_TAG_DOIT;
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

enum PreviewFilterID {
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
};

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
#if 0 /* XXX: TODO. */
    {PREVIEW_FILTER_BRUSH, "BRUSH", 0, "Brushes", ""},
#endif
    {0, nullptr, 0, nullptr, nullptr},
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

static wmOperatorStatus previews_clear_exec(bContext *C, wmOperator *op)
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
      nullptr,
  };

  const int id_filters = preview_filter_to_idfilter(
      PreviewFilterID(RNA_enum_get(op->ptr, "id_type")));

  for (int i = 0; lb[i]; i++) {
    ID *id = static_cast<ID *>(lb[i]->first);
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

    for (; id; id = static_cast<ID *>(id->next)) {
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

static wmOperatorStatus doc_view_manual_ui_context_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr_props;
  wmOperatorStatus retval = OPERATOR_CANCELLED;

  if (std::optional<std::string> manual_id = UI_but_online_manual_id_from_active(C)) {
    WM_operator_properties_create(&ptr_props, "WM_OT_doc_view_manual");
    RNA_string_set(&ptr_props, "doc_id", manual_id.value().c_str());

    retval = WM_operator_name_call_ptr(C,
                                       WM_operatortype_find("WM_OT_doc_view_manual", false),
                                       blender::wm::OpCallContext::ExecDefault,
                                       &ptr_props,
                                       nullptr);

    WM_operator_properties_free(&ptr_props);
  }

  return retval;
}

static void WM_OT_doc_view_manual_ui_context(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "View Online Manual";
  ot->idname = "WM_OT_doc_view_manual_ui_context";
  ot->description = "View a context based online manual in a web browser";

  /* Callbacks. */
  ot->poll = ED_operator_regionactive;
  ot->exec = doc_view_manual_ui_context_exec;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Stereo 3D Operator
 *
 * Turning it full-screen if needed.
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

void wm_operatortypes_register()
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
  WM_operatortype_append(WM_OT_id_linked_relocate);
  WM_operatortype_append(WM_OT_lib_relocate);
  WM_operatortype_append(WM_OT_lib_reload);
  WM_operatortype_append(WM_OT_recover_last_session);
  WM_operatortype_append(WM_OT_recover_auto_save);
  WM_operatortype_append(WM_OT_save_as_mainfile);
  WM_operatortype_append(WM_OT_save_mainfile);
  WM_operatortype_append(WM_OT_clear_recent_files);
  WM_operatortype_append(WM_OT_redraw_timer);
  WM_operatortype_append(WM_OT_memory_statistics);
  WM_operatortype_append(WM_OT_debug_menu);
  WM_operatortype_append(WM_OT_operator_defaults);
  WM_operatortype_append(WM_OT_splash);
  WM_operatortype_append(WM_OT_splash_about);
  WM_operatortype_append(WM_OT_search_menu);
  WM_operatortype_append(WM_OT_search_operator);
  WM_operatortype_append(WM_OT_search_single_menu);
  WM_operatortype_append(WM_OT_call_menu);
  WM_operatortype_append(WM_OT_call_menu_pie);
  WM_operatortype_append(WM_OT_call_panel);
  WM_operatortype_append(WM_OT_call_asset_shelf_popover);
  WM_operatortype_append(WM_OT_radial_control);
  WM_operatortype_append(WM_OT_stereo3d_set);
#if defined(WIN32)
  WM_operatortype_append(WM_OT_console_toggle);
#endif
  WM_operatortype_append(WM_OT_previews_ensure);
  WM_operatortype_append(WM_OT_previews_clear);
  WM_operatortype_append(WM_OT_doc_view_manual_ui_context);
  WM_operatortype_append(WM_OT_set_working_color_space);

#ifdef WITH_XR_OPENXR
  wm_xr_operatortypes_register();
#endif

  /* Gizmos. */
  WM_operatortype_append(GIZMOGROUP_OT_gizmo_select);
  WM_operatortype_append(GIZMOGROUP_OT_gizmo_tweak);
}

/* Circle-select-like modal operators. */
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

      {0, nullptr, 0, nullptr, nullptr},
  };

  /* WARNING: Name is incorrect, use for non-3d views. */
  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "View3D Gesture Circle");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "View3D Gesture Circle", modal_items);

  /* Assign map to operators. */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_circle");
  WM_modalkeymap_assign(keymap, "UV_OT_select_circle");
  WM_modalkeymap_assign(keymap, "SEQUENCER_OT_select_circle");
  WM_modalkeymap_assign(keymap, "CLIP_OT_select_circle");
  WM_modalkeymap_assign(keymap, "MASK_OT_select_circle");
  WM_modalkeymap_assign(keymap, "NODE_OT_select_circle");
  WM_modalkeymap_assign(keymap, "GRAPH_OT_select_circle");
  WM_modalkeymap_assign(keymap, "ACTION_OT_select_circle");
}

/* Straight line modal operators. */
static void gesture_straightline_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {GESTURE_MODAL_SELECT, "SELECT", 0, "Select", ""},
      {GESTURE_MODAL_BEGIN, "BEGIN", 0, "Begin", ""},
      {GESTURE_MODAL_MOVE, "MOVE", 0, "Move", ""},
      {GESTURE_MODAL_SNAP, "SNAP", 0, "Snap", ""},
      {GESTURE_MODAL_FLIP, "FLIP", 0, "Flip", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Straight Line");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Straight Line", modal_items);

  /* Assign map to operators. */
  WM_modalkeymap_assign(keymap, "IMAGE_OT_sample_line");
  WM_modalkeymap_assign(keymap, "PAINT_OT_weight_gradient");
  WM_modalkeymap_assign(keymap, "MESH_OT_bisect");
  WM_modalkeymap_assign(keymap, "PAINT_OT_mask_line_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_face_set_line_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_trim_line_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_project_line_gesture");
  WM_modalkeymap_assign(keymap, "PAINT_OT_hide_show_line_gesture");
}

/* Box_select-like modal operators. */
static void gesture_box_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {GESTURE_MODAL_SELECT, "SELECT", 0, "Select", ""},
      {GESTURE_MODAL_DESELECT, "DESELECT", 0, "Deselect", ""},
      {GESTURE_MODAL_BEGIN, "BEGIN", 0, "Begin", ""},
      {GESTURE_MODAL_MOVE, "MOVE", 0, "Move", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Box");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Box", modal_items);

  /* Assign map to operators. */
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
  WM_modalkeymap_assign(keymap, "UV_OT_custom_region_set");
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
  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_erase_box");
}

/* Lasso modal operators. */
static void gesture_lasso_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_MOVE, "MOVE", 0, "Move", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Lasso");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Lasso", modal_items);

  /* Assign map to operators. */
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "MASK_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "PAINT_OT_mask_lasso_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_face_set_lasso_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_trim_lasso_gesture");
  WM_modalkeymap_assign(keymap, "ACTION_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "CLIP_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "GRAPH_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "NODE_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "UV_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "SEQUENCER_OT_select_lasso");
  WM_modalkeymap_assign(keymap, "PAINT_OT_hide_show_lasso_gesture");
  WM_modalkeymap_assign(keymap, "GREASE_PENCIL_OT_erase_lasso");
}

/* Polyline modal operators */
static void gesture_polyline_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {GESTURE_MODAL_SELECT, "SELECT", 0, "Select", ""},
      {GESTURE_MODAL_MOVE, "MOVE", 0, "Move", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Polyline");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Polyline", modal_items);

  /* assign map to operators */
  WM_modalkeymap_assign(keymap, "PAINT_OT_hide_show_polyline_gesture");
  WM_modalkeymap_assign(keymap, "PAINT_OT_mask_polyline_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_face_set_polyline_gesture");
  WM_modalkeymap_assign(keymap, "SCULPT_OT_trim_polyline_gesture");
}

/* Zoom to border modal operators. */
static void gesture_zoom_border_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {GESTURE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {GESTURE_MODAL_IN, "IN", 0, "In", ""},
      {GESTURE_MODAL_OUT, "OUT", 0, "Out", ""},
      {GESTURE_MODAL_BEGIN, "BEGIN", 0, "Begin", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Gesture Zoom Border");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Gesture Zoom Border", modal_items);

  /* Assign map to operators. */
  WM_modalkeymap_assign(keymap, "VIEW2D_OT_zoom_border");
  WM_modalkeymap_assign(keymap, "VIEW3D_OT_zoom_border");
  WM_modalkeymap_assign(keymap, "IMAGE_OT_view_zoom_border");
}

void wm_window_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Window", SPACE_EMPTY, RGN_TYPE_WINDOW);

  wm_gizmos_keymap(keyconf);
  gesture_circle_modal_keymap(keyconf);
  gesture_box_modal_keymap(keyconf);
  gesture_zoom_border_modal_keymap(keyconf);
  gesture_straightline_modal_keymap(keyconf);
  gesture_lasso_modal_keymap(keyconf);
  gesture_polyline_modal_keymap(keyconf);

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

static bool rna_id_enum_filter_single_and_assets(const ID *id, void *user_data)
{
  if (!rna_id_enum_filter_single(id, user_data)) {
    return false;
  }
  if (id->asset_data != nullptr) {
    return false;
  }
  return true;
}

/* Generic itemf's for operators that take library args. */
static const EnumPropertyItem *rna_id_itemf(bool *r_free,
                                            ID *id,
                                            bool local,
                                            bool (*filter_ids)(const ID *id, void *user_data),
                                            void *user_data)
{
  EnumPropertyItem item_tmp = {0}, *item = nullptr;
  int totitem = 0;
  int i = 0;

  if (id != nullptr) {
    const short id_type = GS(id->name);
    for (; id; id = static_cast<ID *>(id->next)) {
      if ((filter_ids != nullptr) && filter_ids(id, user_data) == false) {
        i++;
        continue;
      }
      if (local == false || !ID_IS_LINKED(id)) {
        item_tmp.identifier = item_tmp.name = id->name + 2;
        item_tmp.value = i++;

        /* Show collection color tag icons in menus. */
        if (id_type == ID_GR) {
          item_tmp.icon = UI_icon_color_from_collection((Collection *)id);
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
                                         PointerRNA * /*ptr*/,
                                         PropertyRNA * /*prop*/,
                                         bool *r_free)
{

  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->actions.first : nullptr, false, nullptr, nullptr);
}
#if 0 /* UNUSED. */
const EnumPropertyItem *RNA_action_local_itemf(bContext *C,
                                               PointerRNA * /*ptr*/,
                                               PropertyRNA * /*prop*/,
                                               bool *r_free)
{
  return rna_id_itemf(r_free, C ? (ID *)CTX_data_main(C)->action.first : nullptr, true);
}
#endif

const EnumPropertyItem *RNA_collection_itemf(bContext *C,
                                             PointerRNA * /*ptr*/,
                                             PropertyRNA * /*prop*/,
                                             bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->collections.first : nullptr, false, nullptr, nullptr);
}
const EnumPropertyItem *RNA_collection_local_itemf(bContext *C,
                                                   PointerRNA * /*ptr*/,
                                                   PropertyRNA * /*prop*/,
                                                   bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->collections.first : nullptr, true, nullptr, nullptr);
}

const EnumPropertyItem *RNA_image_itemf(bContext *C,
                                        PointerRNA * /*ptr*/,
                                        PropertyRNA * /*prop*/,
                                        bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->images.first : nullptr, false, nullptr, nullptr);
}
const EnumPropertyItem *RNA_image_local_itemf(bContext *C,
                                              PointerRNA * /*ptr*/,
                                              PropertyRNA * /*prop*/,
                                              bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->images.first : nullptr, true, nullptr, nullptr);
}

const EnumPropertyItem *RNA_scene_itemf(bContext *C,
                                        PointerRNA * /*ptr*/,
                                        PropertyRNA * /*prop*/,
                                        bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->scenes.first : nullptr, false, nullptr, nullptr);
}
const EnumPropertyItem *RNA_scene_local_itemf(bContext *C,
                                              PointerRNA * /*ptr*/,
                                              PropertyRNA * /*prop*/,
                                              bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->scenes.first : nullptr, true, nullptr, nullptr);
}
const EnumPropertyItem *RNA_scene_without_sequencer_scene_itemf(bContext *C,
                                                                PointerRNA * /*ptr*/,
                                                                PropertyRNA * /*prop*/,
                                                                bool *r_free)
{
  Scene *sequencer_scene = C ? CTX_data_sequencer_scene(C) : nullptr;
  return rna_id_itemf(r_free,
                      C ? (ID *)CTX_data_main(C)->scenes.first : nullptr,
                      false,
                      rna_id_enum_filter_single_and_assets,
                      sequencer_scene);
}
const EnumPropertyItem *RNA_movieclip_itemf(bContext *C,
                                            PointerRNA * /*ptr*/,
                                            PropertyRNA * /*prop*/,
                                            bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->movieclips.first : nullptr, false, nullptr, nullptr);
}
const EnumPropertyItem *RNA_movieclip_local_itemf(bContext *C,
                                                  PointerRNA * /*ptr*/,
                                                  PropertyRNA * /*prop*/,
                                                  bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->movieclips.first : nullptr, true, nullptr, nullptr);
}

const EnumPropertyItem *RNA_mask_itemf(bContext *C,
                                       PointerRNA * /*ptr*/,
                                       PropertyRNA * /*prop*/,
                                       bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->masks.first : nullptr, false, nullptr, nullptr);
}
const EnumPropertyItem *RNA_mask_local_itemf(bContext *C,
                                             PointerRNA * /*ptr*/,
                                             PropertyRNA * /*prop*/,
                                             bool *r_free)
{
  return rna_id_itemf(
      r_free, C ? (ID *)CTX_data_main(C)->masks.first : nullptr, true, nullptr, nullptr);
}

/** \} */
