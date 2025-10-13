/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "BKE_global.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_view3d.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

/* Own includes. */
#include "wm_gizmo_intern.hh"
#include "wm_gizmo_wmapi.hh"

using blender::StringRef;

static void wm_gizmo_register(wmGizmoGroup *gzgroup, wmGizmo *gz);

/**
 * \note Follow #wm_operator_create convention.
 */
static wmGizmo *wm_gizmo_create(const wmGizmoType *gzt, PointerRNA *properties)
{
  BLI_assert(gzt != nullptr);
  BLI_assert(gzt->struct_size >= sizeof(wmGizmo));

  /* FIXME: Old C-style allocation is not trivial to port to C++ here, because actual allocation
   * depends on the 'subtype' of gizmo. The whole gizmo type hierarchy should probably be moved to
   * proper C++ virtual inheritance at some point. */
  wmGizmo *gz = static_cast<wmGizmo *>(MEM_callocN(gzt->struct_size, __func__));
  new (gz) wmGizmo();
  gz->type = gzt;

  /* Initialize properties, either copy or create. */
  gz->ptr = MEM_new<PointerRNA>("wmGizmoPtrRNA");
  if (properties && properties->data) {
    gz->properties = IDP_CopyProperty(static_cast<const IDProperty *>(properties->data));
  }
  else {
    gz->properties = blender::bke::idprop::create_group("wmGizmoProperties").release();
  }
  *gz->ptr = RNA_pointer_create_discrete(
      static_cast<ID *>(G_MAIN->wm.first), gzt->srna, gz->properties);

  WM_gizmo_properties_sanitize(gz->ptr, false);

  unit_m4(gz->matrix_space);
  unit_m4(gz->matrix_basis);
  unit_m4(gz->matrix_offset);

  gz->drag_part = -1;

  /* Only ensure expected size for the target properties array. Actual initialization of these
   * happen separately (see e.g. #WM_gizmo_target_property_def_rna and related). */
  gz->target_properties.resize(gzt->target_property_defs_len);

  return gz;
}

wmGizmo *WM_gizmo_new_ptr(const wmGizmoType *gzt, wmGizmoGroup *gzgroup, PointerRNA *properties)
{
  wmGizmo *gz = wm_gizmo_create(gzt, properties);

  wm_gizmo_register(gzgroup, gz);

  if (gz->type->setup != nullptr) {
    gz->type->setup(gz);
  }

  return gz;
}

wmGizmo *WM_gizmo_new(const StringRef idname, wmGizmoGroup *gzgroup, PointerRNA *properties)
{
  const wmGizmoType *gzt = WM_gizmotype_find(idname, false);
  return WM_gizmo_new_ptr(gzt, gzgroup, properties);
}

/**
 * Initialize default values and allocate needed memory for members.
 */
static void gizmo_init(wmGizmo *gz)
{
  const float color_default[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  gz->scale_basis = 1.0f;
  gz->line_width = 1.0f;

  /* Defaults. */
  copy_v4_v4(gz->color, color_default);
  copy_v4_v4(gz->color_hi, color_default);
}

/**
 * Register \a gizmo.
 *
 * \note Not to be confused with type registration from RNA.
 */
static void wm_gizmo_register(wmGizmoGroup *gzgroup, wmGizmo *gz)
{
  gizmo_init(gz);
  wm_gizmogroup_gizmo_register(gzgroup, gz);
}

void WM_gizmo_free(wmGizmo *gz)
{
  if (gz->type->free != nullptr) {
    gz->type->free(gz);
  }

#ifdef WITH_PYTHON
  if (gz->py_instance) {
    /* Do this first in case there are any `__del__` functions or
     * similar that use properties. */
    BPY_DECREF_RNA_INVALIDATE(gz->py_instance);
  }
#endif

  for (wmGizmoOpElem &gzop : gz->op_data) {
    WM_operator_properties_free(&gzop.ptr);
  }

  if (gz->ptr != nullptr) {
    WM_gizmo_properties_free(gz->ptr);
    MEM_delete(gz->ptr);
  }

  for (wmGizmoProperty &gz_prop : gz->target_properties) {
    if (gz_prop.custom_func.free_fn) {
      gz_prop.custom_func.free_fn(gz, &gz_prop);
    }
  }

  /* Explicit calling of the destructor is needed here because allocation still happens 'the C
   * way', see FIXME note in #wm_gizmo_create. */
  gz->~wmGizmo();
  MEM_freeN(static_cast<void *>(gz));
}

void WM_gizmo_unlink(ListBase *gizmolist, wmGizmoMap *gzmap, wmGizmo *gz, bContext *C)
{
  if (gz->state & WM_GIZMO_STATE_HIGHLIGHT) {
    wm_gizmomap_highlight_set(gzmap, C, nullptr, 0);
  }
  if (gz->state & WM_GIZMO_STATE_MODAL) {
    wm_gizmomap_modal_set(gzmap, C, gz, nullptr, false);
  }
  /* Unlink instead of setting so we don't run callbacks. */
  if (gz->state & WM_GIZMO_STATE_SELECT) {
    WM_gizmo_select_unlink(gzmap, gz);
  }

  if (gizmolist) {
    BLI_remlink(gizmolist, gz);
  }

  BLI_assert(gzmap->gzmap_context.highlight != gz);
  BLI_assert(gzmap->gzmap_context.modal != gz);

  WM_gizmo_free(gz);
}

/* -------------------------------------------------------------------- */
/** \name Gizmo Creation API
 *
 * API for defining data on gizmo creation.
 *
 * \{ */

wmGizmoOpElem *WM_gizmo_operator_get(wmGizmo *gz, int part_index)
{
  if ((part_index >= 0) && (part_index < gz->op_data.size())) {
    return &gz->op_data[part_index];
  }
  return nullptr;
}

PointerRNA *WM_gizmo_operator_set(wmGizmo *gz,
                                  int part_index,
                                  wmOperatorType *ot,
                                  IDProperty *properties)
{
  BLI_assert(part_index < 255);
  if (part_index >= gz->op_data.size()) {
    gz->op_data.resize(part_index + 1);
  }
  wmGizmoOpElem &gzop = gz->op_data[part_index];
  gzop.type = ot;

  if (gzop.ptr.data) {
    WM_operator_properties_free(&gzop.ptr);
  }
  WM_operator_properties_create_ptr(&gzop.ptr, ot);

  if (properties) {
    gzop.ptr.data = properties;
  }

  return &gzop.ptr;
}

wmOperatorStatus WM_gizmo_operator_invoke(bContext *C,
                                          wmGizmo *gz,
                                          wmGizmoOpElem *gzop,
                                          const wmEvent *event)
{
  if (gz->flag & WM_GIZMO_OPERATOR_TOOL_INIT) {
    /* Merge tool-settings into the gizmo properties. */
    PointerRNA tref_ptr;
    bToolRef *tref = WM_toolsystem_ref_from_context(C);
    if (tref && WM_toolsystem_ref_properties_get_from_operator(tref, gzop->type, &tref_ptr)) {
      if (gzop->ptr.data == nullptr) {
        gzop->ptr.data = blender::bke::idprop::create_group("wmOperatorProperties").release();
      }
      IDP_MergeGroup(static_cast<IDProperty *>(gzop->ptr.data),
                     static_cast<const IDProperty *>(tref_ptr.data),
                     false);
    }
  }
  return WM_operator_name_call_ptr(
      C, gzop->type, blender::wm::OpCallContext::InvokeDefault, &gzop->ptr, event);
}

static void wm_gizmo_set_matrix_rotation_from_z_axis__internal(float matrix[4][4],
                                                               const float z_axis[3])
{
/* Old code, seems we can use simpler method. */
#if 0
  const float z_global[3] = {0.0f, 0.0f, 1.0f};
  float rot[3][3];

  rotation_between_vecs_to_mat3(rot, z_global, z_axis);
  copy_v3_v3(matrix[0], rot[0]);
  copy_v3_v3(matrix[1], rot[1]);
  copy_v3_v3(matrix[2], rot[2]);
#else
  normalize_v3_v3(matrix[2], z_axis);
  ortho_basis_v3v3_v3(matrix[0], matrix[1], matrix[2]);
#endif
}

static void wm_gizmo_set_matrix_rotation_from_yz_axis__internal(float matrix[4][4],
                                                                const float y_axis[3],
                                                                const float z_axis[3])
{
  normalize_v3_v3(matrix[1], y_axis);
  normalize_v3_v3(matrix[2], z_axis);
  cross_v3_v3v3(matrix[0], matrix[1], matrix[2]);
  normalize_v3(matrix[0]);
}

void WM_gizmo_set_matrix_rotation_from_z_axis(wmGizmo *gz, const float z_axis[3])
{
  wm_gizmo_set_matrix_rotation_from_z_axis__internal(gz->matrix_basis, z_axis);
}
void WM_gizmo_set_matrix_rotation_from_yz_axis(wmGizmo *gz,
                                               const float y_axis[3],
                                               const float z_axis[3])
{
  wm_gizmo_set_matrix_rotation_from_yz_axis__internal(gz->matrix_basis, y_axis, z_axis);
}
void WM_gizmo_set_matrix_location(wmGizmo *gz, const float origin[3])
{
  copy_v3_v3(gz->matrix_basis[3], origin);
}

void WM_gizmo_set_matrix_offset_rotation_from_z_axis(wmGizmo *gz, const float z_axis[3])
{
  wm_gizmo_set_matrix_rotation_from_z_axis__internal(gz->matrix_offset, z_axis);
}
void WM_gizmo_set_matrix_offset_rotation_from_yz_axis(wmGizmo *gz,
                                                      const float y_axis[3],
                                                      const float z_axis[3])
{
  wm_gizmo_set_matrix_rotation_from_yz_axis__internal(gz->matrix_offset, y_axis, z_axis);
}
void WM_gizmo_set_matrix_offset_location(wmGizmo *gz, const float offset[3])
{
  copy_v3_v3(gz->matrix_offset[3], offset);
}

void WM_gizmo_set_flag(wmGizmo *gz, const int flag, const bool enable)
{
  if (enable) {
    gz->flag |= eWM_GizmoFlag(flag);
  }
  else {
    gz->flag &= ~eWM_GizmoFlag(flag);
  }
}

void WM_gizmo_set_scale(wmGizmo *gz, const float scale)
{
  gz->scale_basis = scale;
}

void WM_gizmo_set_line_width(wmGizmo *gz, const float line_width)
{
  gz->line_width = line_width;
}

void WM_gizmo_get_color(const wmGizmo *gz, float color[4])
{
  copy_v4_v4(color, gz->color);
}
void WM_gizmo_set_color(wmGizmo *gz, const float color[4])
{
  copy_v4_v4(gz->color, color);
}

void WM_gizmo_get_color_highlight(const wmGizmo *gz, float color_hi[4])
{
  copy_v4_v4(color_hi, gz->color_hi);
}
void WM_gizmo_set_color_highlight(wmGizmo *gz, const float color_hi[4])
{
  copy_v4_v4(gz->color_hi, color_hi);
}

/** \} */ /* Gizmo Creation API. */

/* -------------------------------------------------------------------- */
/** \name Gizmo Callback Assignment
 * \{ */

void WM_gizmo_set_fn_custom_modal(wmGizmo *gz, wmGizmoFnModal fn)
{
  gz->custom_modal = fn;
}

/** \} */

/* -------------------------------------------------------------------- */

bool wm_gizmo_select_set_ex(
    wmGizmoMap *gzmap, wmGizmo *gz, bool select, bool use_array, bool use_callback)
{
  bool changed = false;

  if (select) {
    if ((gz->state & WM_GIZMO_STATE_SELECT) == 0) {
      if (use_array) {
        wm_gizmomap_select_array_push_back(gzmap, gz);
      }
      gz->state |= WM_GIZMO_STATE_SELECT;
      changed = true;
    }
  }
  else {
    if (gz->state & WM_GIZMO_STATE_SELECT) {
      if (use_array) {
        wm_gizmomap_select_array_remove(gzmap, gz);
      }
      gz->state &= ~WM_GIZMO_STATE_SELECT;
      changed = true;
    }
  }

  /* In the case of unlinking we only want to remove from the array
   * and not write to the external state. */
  if (use_callback && changed) {
    if (gz->type->select_refresh) {
      gz->type->select_refresh(gz);
    }
  }

  return changed;
}

bool WM_gizmo_select_unlink(wmGizmoMap *gzmap, wmGizmo *gz)
{
  return wm_gizmo_select_set_ex(gzmap, gz, false, true, false);
}

bool WM_gizmo_select_set(wmGizmoMap *gzmap, wmGizmo *gz, bool select)
{
  return wm_gizmo_select_set_ex(gzmap, gz, select, true, true);
}

bool WM_gizmo_highlight_set(wmGizmoMap *gzmap, wmGizmo *gz)
{
  return wm_gizmomap_highlight_set(gzmap, nullptr, gz, gz ? gz->highlight_part : 0);
}

bool wm_gizmo_select_and_highlight(bContext *C, wmGizmoMap *gzmap, wmGizmo *gz)
{
  if (WM_gizmo_select_set(gzmap, gz, true)) {
    wm_gizmomap_highlight_set(gzmap, C, gz, gz->highlight_part);
    return true;
  }
  return false;
}

void WM_gizmo_modal_set_from_setup(
    wmGizmoMap *gzmap, bContext *C, wmGizmo *gz, int part_index, const wmEvent *event)
{
  gz->highlight_part = part_index;
  WM_gizmo_highlight_set(gzmap, gz);
  if (false) {
    wm_gizmomap_modal_set(gzmap, C, gz, event, true);
  }
  else {
    /* WEAK: but it works. */
    WM_operator_name_call(
        C, "GIZMOGROUP_OT_gizmo_tweak", blender::wm::OpCallContext::InvokeDefault, nullptr, event);
  }
}

void WM_gizmo_modal_set_while_modal(wmGizmoMap *gzmap,
                                    bContext *C,
                                    wmGizmo *gz,
                                    const wmEvent *event)
{
  if (gzmap->gzmap_context.modal) {
    wm_gizmomap_modal_set(gzmap, C, gzmap->gzmap_context.modal, event, false);
  }

  if (gz) {
    wm_gizmo_calculate_scale(gz, C);

    /* Set `highlight_part` to -1 to skip operator invocation. */
    const int highlight_part = gz->highlight_part;
    gz->highlight_part = -1;
    wm_gizmomap_modal_set(gzmap, C, gz, event, true);
    gz->highlight_part = highlight_part;
  }
}

void wm_gizmo_calculate_scale(wmGizmo *gz, const bContext *C)
{
  const RegionView3D *rv3d = CTX_wm_region_view3d(C);
  float scale = UI_SCALE_FAC;

  if ((gz->parent_gzgroup->type->flag & WM_GIZMOGROUPTYPE_SCALE) == 0) {
    scale *= U.gizmo_size;
    if (rv3d) {
      /* #ED_view3d_pixel_size includes #U.pixelsize, remove it. */
      float matrix_world[4][4];
      if (gz->type->matrix_basis_get) {
        float matrix_basis[4][4];
        gz->type->matrix_basis_get(gz, matrix_basis);
        mul_m4_m4m4(matrix_world, gz->matrix_space, matrix_basis);
      }
      else {
        mul_m4_m4m4(matrix_world, gz->matrix_space, gz->matrix_basis);
      }

      /* Exclude matrix_offset from scale. */
      scale *= ED_view3d_pixel_size_no_ui_scale(rv3d, matrix_world[3]);
    }
  }

  gz->scale_final = gz->scale_basis * scale;
}

static void gizmo_update_prop_data(wmGizmo *gz)
{
  /* Gizmo property might have been changed, so update gizmo. */
  if (gz->type->property_update) {
    for (wmGizmoProperty &gz_prop : gz->target_properties) {
      if (WM_gizmo_target_property_is_valid(&gz_prop)) {
        gz->type->property_update(gz, &gz_prop);
      }
    }
  }
}

void wm_gizmo_update(wmGizmo *gz, const bContext *C, const bool refresh_map)
{
  if (refresh_map) {
    gizmo_update_prop_data(gz);
  }
  wm_gizmo_calculate_scale(gz, C);
}

int wm_gizmo_is_visible(wmGizmo *gz)
{
  if (gz->flag & WM_GIZMO_HIDDEN) {
    return 0;
  }
  if ((gz->state & WM_GIZMO_STATE_MODAL) &&
      !(gz->flag & (WM_GIZMO_DRAW_MODAL | WM_GIZMO_DRAW_VALUE)))
  {
    /* Don't draw while modal (dragging). */
    return 0;
  }
  if ((gz->flag & WM_GIZMO_DRAW_HOVER) && !(gz->state & WM_GIZMO_STATE_HIGHLIGHT) &&
      !(gz->state & WM_GIZMO_STATE_SELECT)) /* Still draw selected gizmos. */
  {
    /* Update but don't draw. */
    return WM_GIZMO_IS_VISIBLE_UPDATE;
  }

  return WM_GIZMO_IS_VISIBLE_UPDATE | WM_GIZMO_IS_VISIBLE_DRAW;
}

void WM_gizmo_calc_matrix_final_params(const wmGizmo *gz,
                                       const WM_GizmoMatrixParams *params,
                                       float r_mat[4][4])
{
  const float (*const matrix_space)[4] = params->matrix_space ? params->matrix_space :
                                                                gz->matrix_space;
  const float (*const matrix_basis)[4] = params->matrix_basis ? params->matrix_basis :
                                                                gz->matrix_basis;
  const float (*const matrix_offset)[4] = params->matrix_offset ? params->matrix_offset :
                                                                  gz->matrix_offset;
  const float *scale_final = params->scale_final ? params->scale_final : &gz->scale_final;

  float final_matrix[4][4];
  if (params->matrix_basis == nullptr && gz->type->matrix_basis_get) {
    gz->type->matrix_basis_get(gz, final_matrix);
  }
  else {
    copy_m4_m4(final_matrix, matrix_basis);
  }

  if (gz->flag & WM_GIZMO_DRAW_NO_SCALE) {
    mul_m4_m4m4(final_matrix, final_matrix, matrix_offset);
  }
  else {
    if (gz->flag & WM_GIZMO_DRAW_OFFSET_SCALE) {
      mul_mat3_m4_fl(final_matrix, *scale_final);
      mul_m4_m4m4(final_matrix, final_matrix, matrix_offset);
    }
    else {
      mul_m4_m4m4(final_matrix, final_matrix, matrix_offset);
      mul_mat3_m4_fl(final_matrix, *scale_final);
    }
  }

  mul_m4_m4m4(r_mat, matrix_space, final_matrix);
}

void WM_gizmo_calc_matrix_final_no_offset(const wmGizmo *gz, float r_mat[4][4])
{
  float mat_identity[4][4];
  unit_m4(mat_identity);

  WM_GizmoMatrixParams params{};
  params.matrix_space = nullptr;
  params.matrix_basis = nullptr;
  params.matrix_offset = mat_identity;
  params.scale_final = nullptr;
  WM_gizmo_calc_matrix_final_params(gz, &params, r_mat);
}

void WM_gizmo_calc_matrix_final(const wmGizmo *gz, float r_mat[4][4])
{
  WM_GizmoMatrixParams params{};
  params.matrix_space = nullptr;
  params.matrix_basis = nullptr;
  params.matrix_offset = nullptr;
  params.scale_final = nullptr;
  WM_gizmo_calc_matrix_final_params(gz, &params, r_mat);
}

/* -------------------------------------------------------------------- */
/** \name Gizmo Property Access
 *
 * Matches `WM_operator_properties` conventions.
 *
 * \{ */

void WM_gizmo_properties_create_ptr(PointerRNA *ptr, wmGizmoType *gzt)
{
  *ptr = RNA_pointer_create_discrete(nullptr, gzt->srna, nullptr);
}

void WM_gizmo_properties_create(PointerRNA *ptr, const StringRef gtstring)
{
  const wmGizmoType *gzt = WM_gizmotype_find(gtstring, false);

  if (gzt) {
    WM_gizmo_properties_create_ptr(ptr, (wmGizmoType *)gzt);
  }
  else {
    *ptr = RNA_pointer_create_discrete(nullptr, &RNA_GizmoProperties, nullptr);
  }
}

void WM_gizmo_properties_alloc(PointerRNA **ptr, IDProperty **properties, const StringRef gtstring)
{
  if (*properties == nullptr) {
    *properties = blender::bke::idprop::create_group("wmOpItemProp").release();
  }

  if (*ptr == nullptr) {
    *ptr = MEM_new<PointerRNA>("wmOpItemPtr");
    WM_gizmo_properties_create(*ptr, gtstring);
  }

  (*ptr)->data = *properties;
}

void WM_gizmo_properties_sanitize(PointerRNA *ptr, const bool no_context)
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

        /* Recurse into gizmo properties. */
        if (RNA_struct_is_a(ptype, &RNA_GizmoProperties)) {
          PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
          WM_gizmo_properties_sanitize(&opptr, no_context);
        }
        break;
      }
      default:
        break;
    }
  }
  RNA_STRUCT_END;
}

bool WM_gizmo_properties_default(PointerRNA *ptr, const bool do_update)
{
  bool changed = false;
  RNA_STRUCT_BEGIN (ptr, prop) {
    switch (RNA_property_type(prop)) {
      case PROP_POINTER: {
        StructRNA *ptype = RNA_property_pointer_type(ptr, prop);
        if (ptype != &RNA_Struct) {
          PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
          changed |= WM_gizmo_properties_default(&opptr, do_update);
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

void WM_gizmo_properties_reset(wmGizmo *gz)
{
  if (gz->ptr->data) {
    PropertyRNA *iterprop;
    iterprop = RNA_struct_iterator_property(gz->type->srna);

    RNA_PROP_BEGIN (gz->ptr, itemptr, iterprop) {
      PropertyRNA *prop = static_cast<PropertyRNA *>(itemptr.data);

      if ((RNA_property_flag(prop) & PROP_SKIP_SAVE) == 0) {
        const char *identifier = RNA_property_identifier(prop);
        RNA_struct_system_idprops_unset(gz->ptr, identifier);
      }
    }
    RNA_PROP_END;
  }
}

void WM_gizmo_properties_clear(PointerRNA *ptr)
{
  IDProperty *properties = static_cast<IDProperty *>(ptr->data);

  if (properties) {
    IDP_ClearProperty(properties);
  }
}

void WM_gizmo_properties_free(PointerRNA *ptr)
{
  IDProperty *properties = static_cast<IDProperty *>(ptr->data);

  if (properties) {
    IDP_FreeProperty(properties);
    ptr->data = nullptr; /* Just in case. */
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General Utilities
 * \{ */

bool WM_gizmo_group_is_modal(const wmGizmoGroup *gzgroup)
{
  wmGizmo *gz = WM_gizmomap_get_modal(gzgroup->parent_gzmap);
  if (gz && gz->parent_gzgroup == gzgroup) {
    return true;
  }
  return false;
}

bool WM_gizmo_context_check_drawstep(const bContext *C, eWM_GizmoFlagMapDrawStep step)
{
  switch (step) {
    case WM_GIZMOMAP_DRAWSTEP_2D: {
      break;
    }
    case WM_GIZMOMAP_DRAWSTEP_3D: {
      wmWindowManager *wm = CTX_wm_manager(C);
      if (ED_screen_animation_playing(wm)) {
        return false;
      }
      break;
    }
  }
  return true;
}

/** \} */
