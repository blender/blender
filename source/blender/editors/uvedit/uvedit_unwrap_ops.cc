/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.hh"

#include "BLI_array.hh"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_string_utf8.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_object_types.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_mesh.hh"
#include "BKE_subdiv_modifier.hh"
#include "BKE_uvproject.h"

#include "DEG_depsgraph.hh"

#include "GEO_uv_pack.hh"
#include "GEO_uv_parametrizer.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "ED_image.hh"
#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"
#include "ED_uvedit.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "uvedit_intern.hh"

using blender::Span;
using blender::Vector;
using blender::geometry::ParamHandle;
using blender::geometry::ParamKey;
using blender::geometry::ParamSlimOptions;

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static bool uvedit_ensure_uvs(Object *obedit)
{
  if (ED_uvedit_test(obedit)) {
    return true;
  }

  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  BMFace *efa;
  BMIter iter;

  if (em && em->bm->totface && !CustomData_has_layer(&em->bm->ldata, CD_PROP_FLOAT2)) {
    ED_mesh_uv_add(static_cast<Mesh *>(obedit->data), nullptr, true, true, nullptr);
  }

  /* Happens when there are no faces. */
  if (!ED_uvedit_test(obedit)) {
    return false;
  }

  /* select new UVs (ignore UV_FLAG_SELECT_SYNC in this case) */
  em->bm->uv_select_sync_valid = false;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    BMIter liter;
    BMLoop *l;

    BM_elem_flag_enable(efa, BM_ELEM_SELECT_UV);
    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      BM_elem_flag_enable(l, BM_ELEM_SELECT_UV);
      BM_elem_flag_enable(l, BM_ELEM_SELECT_UV_EDGE);
    }
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Properties
 * \{ */

static void uv_map_operator_property_correct_aspect(wmOperatorType *ot)
{
  RNA_def_boolean(
      ot->srna,
      "correct_aspect",
      true,
      "Correct Aspect",
      "Map UVs taking aspect ratio of the image associated with the material into account");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UDIM Access
 * \{ */

void blender::geometry::UVPackIsland_Params::setUDIMOffsetFromSpaceImage(const SpaceImage *sima)
{
  if (!sima) {
    return; /* Nothing to do. */
  }

  /* NOTE: Presently, when UDIM grid and tiled image are present together, only active tile for
   * the tiled image is considered. */
  const Image *image = sima->image;
  if (image && image->source == IMA_SRC_TILED) {
    ImageTile *active_tile = static_cast<ImageTile *>(
        BLI_findlink(&image->tiles, image->active_tile_index));
    if (active_tile) {
      udim_base_offset[0] = (active_tile->tile_number - 1001) % 10;
      udim_base_offset[1] = (active_tile->tile_number - 1001) / 10;
    }
    return;
  }

  /* TODO: Support storing an active UDIM when there are no tiles present.
   * Until then, use 2D cursor to find the active tile index for the UDIM grid. */
  if (uv_coords_isect_udim(sima->image, sima->tile_grid_shape, sima->cursor)) {
    udim_base_offset[0] = floorf(sima->cursor[0]);
    udim_base_offset[1] = floorf(sima->cursor[1]);
  }
}
/** \} */

bool blender::geometry::UVPackIsland_Params::isCancelled() const
{
  if (stop) {
    return *stop;
  }
  return false;
}

/* -------------------------------------------------------------------- */
/** \name Parametrizer Conversion
 * \{ */

struct UnwrapOptions {
  /** Connectivity based on UV coordinates instead of seams. */
  bool topology_from_uvs;
  /** Also use seams as well as UV coordinates (only valid when `topology_from_uvs` is enabled). */
  bool topology_from_uvs_use_seams;
  /** Only affect selected faces. */
  bool only_selected_faces;
  /**
   * Only affect selected UVs.
   * \note Disable this for operations that don't run in the image-window.
   * Unwrapping from the 3D view for example, where only 'only_selected_faces' should be used.
   */
  bool only_selected_uvs;
  /** Fill holes to better preserve shape. */
  bool fill_holes;
  /** Correct for mapped image texture aspect ratio. */
  bool correct_aspect;
  /** Treat unselected uvs as if they were pinned. */
  bool pin_unselected;

  int method;
  bool use_slim;
  bool use_abf;
  bool use_subsurf;
  bool use_weights;

  ParamSlimOptions slim;
  char weight_group[MAX_VGROUP_NAME];
};

void blender::geometry::UVPackIsland_Params::setFromUnwrapOptions(const UnwrapOptions &options)
{
  only_selected_uvs = options.only_selected_uvs;
  only_selected_faces = options.only_selected_faces;
  use_seams = !options.topology_from_uvs || options.topology_from_uvs_use_seams;
  correct_aspect = options.correct_aspect;
  pin_unselected = options.pin_unselected;
}

static void modifier_unwrap_state(Object *obedit,
                                  const UnwrapOptions *options,
                                  bool *r_use_subsurf)
{
  ModifierData *md;
  bool subsurf = options->use_subsurf;

  md = static_cast<ModifierData *>(obedit->modifiers.first);

  /* Subdivision-surface will take the modifier settings
   * only if modifier is first or right after mirror. */
  if (subsurf) {
    if (md && md->type == eModifierType_Subsurf) {
      const SubsurfModifierData &smd = *reinterpret_cast<const SubsurfModifierData *>(md);
      if (smd.levels > 0) {
        /* Skip all calculation for zero subdivision levels, similar to the way the modifier is
         * disabled in that case. */
        subsurf = true;
      }
      else {
        subsurf = false;
      }
    }
    else {
      subsurf = false;
    }
  }

  *r_use_subsurf = subsurf;
}

static UnwrapOptions unwrap_options_get(wmOperator *op, Object *ob, const ToolSettings *ts)
{
  UnwrapOptions options{};

  /* To be set by the upper layer */
  options.topology_from_uvs = false;
  options.topology_from_uvs_use_seams = false;
  options.only_selected_faces = false;
  options.only_selected_uvs = false;
  options.pin_unselected = false;

  options.slim.skip_init = false;

  if (ts) {
    options.method = ts->unwrapper;
    options.correct_aspect = (ts->uvcalc_flag & UVCALC_NO_ASPECT_CORRECT) == 0;
    options.fill_holes = (ts->uvcalc_flag & UVCALC_FILLHOLES) != 0;
    options.use_subsurf = (ts->uvcalc_flag & UVCALC_USESUBSURF) != 0;

    options.use_weights = ts->uvcalc_flag & UVCALC_UNWRAP_USE_WEIGHTS;
    STRNCPY_UTF8(options.weight_group, ts->uvcalc_weight_group);
    options.slim.weight_influence = ts->uvcalc_weight_factor;

    options.slim.iterations = ts->uvcalc_iterations;
    options.slim.no_flip = ts->uvcalc_flag & UVCALC_UNWRAP_NO_FLIP;
  }
  else {
    options.method = RNA_enum_get(op->ptr, "method");
    options.correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect");
    options.fill_holes = RNA_boolean_get(op->ptr, "fill_holes");
    options.use_subsurf = RNA_boolean_get(op->ptr, "use_subsurf_data");

    options.use_weights = RNA_boolean_get(op->ptr, "use_weights");
    RNA_string_get(op->ptr, "weight_group", options.weight_group);
    options.slim.weight_influence = RNA_float_get(op->ptr, "weight_factor");

    options.slim.iterations = RNA_int_get(op->ptr, "iterations");
    options.slim.no_flip = RNA_boolean_get(op->ptr, "no_flip");
  }

#ifndef WITH_UV_SLIM
  if (options.method == UVCALC_UNWRAP_METHOD_MINIMUM_STRETCH) {
    options.method = UVCALC_UNWRAP_METHOD_CONFORMAL;
    if (op) {
      BKE_report(op->reports, RPT_WARNING, "Built without SLIM, falling back to conformal method");
    }
  }
#endif /* !WITH_UV_SLIM */

  if (options.weight_group[0] == '\0' || options.use_weights == false) {
    options.slim.weight_influence = 0.0f;
  }

  options.use_abf = options.method == UVCALC_UNWRAP_METHOD_ANGLE;
  options.use_slim = options.method == UVCALC_UNWRAP_METHOD_MINIMUM_STRETCH;

  /* SLIM requires hole filling */
  if (options.use_slim) {
    options.fill_holes = true;
  }

  if (ob) {
    bool use_subsurf_final;
    modifier_unwrap_state(ob, &options, &use_subsurf_final);
    options.use_subsurf = use_subsurf_final;
  }

  return options;
}

/* Generic sync functions
 *
 * NOTE: these could be moved to a generic API.
 */

static bool rna_property_sync_flag(
    PointerRNA *ptr, const char *prop_name, char flag, bool flipped, char *value_p)
{
  if (PropertyRNA *prop = RNA_struct_find_property(ptr, prop_name)) {
    if (RNA_property_is_set(ptr, prop)) {
      if (RNA_property_boolean_get(ptr, prop) ^ flipped) {
        *value_p |= flag;
      }
      else {
        *value_p &= ~flag;
      }
      return true;
    }
    RNA_property_boolean_set(ptr, prop, ((*value_p & flag) > 0) ^ flipped);
    return false;
  }
  BLI_assert_unreachable();
  return false;
}

static bool rna_property_sync_enum(PointerRNA *ptr, const char *prop_name, int *value_p)
{
  if (PropertyRNA *prop = RNA_struct_find_property(ptr, prop_name)) {
    if (RNA_property_is_set(ptr, prop)) {
      *value_p = RNA_property_enum_get(ptr, prop);
      return true;
    }
    RNA_property_enum_set(ptr, prop, *value_p);
    return false;
  }
  BLI_assert_unreachable();
  return false;
}

static bool rna_property_sync_enum_char(PointerRNA *ptr, const char *prop_name, char *value_p)
{
  int value_i = *value_p;
  if (rna_property_sync_enum(ptr, prop_name, &value_i)) {
    *value_p = value_i;
    return true;
  }
  return false;
}

static bool rna_property_sync_int(PointerRNA *ptr, const char *prop_name, int *value_p)
{
  if (PropertyRNA *prop = RNA_struct_find_property(ptr, prop_name)) {
    if (RNA_property_is_set(ptr, prop)) {
      *value_p = RNA_property_int_get(ptr, prop);
      return true;
    }
    RNA_property_int_set(ptr, prop, *value_p);
    return false;
  }
  BLI_assert_unreachable();
  return false;
}

static bool rna_property_sync_float(PointerRNA *ptr, const char *prop_name, float *value_p)
{
  if (PropertyRNA *prop = RNA_struct_find_property(ptr, prop_name)) {
    if (RNA_property_is_set(ptr, prop)) {
      *value_p = RNA_property_float_get(ptr, prop);
      return true;
    }
    RNA_property_float_set(ptr, prop, *value_p);
    return false;
  }
  BLI_assert_unreachable();
  return false;
}

static bool rna_property_sync_string(PointerRNA *ptr, const char *prop_name, char value_p[])
{
  if (PropertyRNA *prop = RNA_struct_find_property(ptr, prop_name)) {
    if (RNA_property_is_set(ptr, prop)) {
      RNA_property_string_get(ptr, prop, value_p);
      return true;
    }
    RNA_property_string_set(ptr, prop, value_p);
    return false;
  }
  BLI_assert_unreachable();
  return false;
}

static void unwrap_options_sync_toolsettings(wmOperator *op, ToolSettings *ts)
{
  /* Remember last method for live unwrap. */
  rna_property_sync_enum_char(op->ptr, "method", &ts->unwrapper);

  /* Remember packing margin. */
  rna_property_sync_float(op->ptr, "margin", &ts->uvcalc_margin);

  rna_property_sync_int(op->ptr, "iterations", &ts->uvcalc_iterations);

  rna_property_sync_float(op->ptr, "weight_factor", &ts->uvcalc_weight_factor);

  rna_property_sync_string(op->ptr, "weight_group", ts->uvcalc_weight_group);

  rna_property_sync_flag(op->ptr, "fill_holes", UVCALC_FILLHOLES, false, &ts->uvcalc_flag);
  rna_property_sync_flag(
      op->ptr, "correct_aspect", UVCALC_NO_ASPECT_CORRECT, true, &ts->uvcalc_flag);
  rna_property_sync_flag(op->ptr, "use_subsurf_data", UVCALC_USESUBSURF, false, &ts->uvcalc_flag);
  rna_property_sync_flag(op->ptr, "no_flip", UVCALC_UNWRAP_NO_FLIP, false, &ts->uvcalc_flag);

  rna_property_sync_flag(
      op->ptr, "use_weights", UVCALC_UNWRAP_USE_WEIGHTS, false, &ts->uvcalc_flag);
}

static bool uvedit_have_selection(const Scene *scene, BMEditMesh *em, const UnwrapOptions *options)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);

  if (offsets.uv == -1) {
    return (em->bm->totfacesel != 0);
  }

  /* verify if we have any selected uv's before unwrapping,
   * so we can cancel the operator early */
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (scene->toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) {
      if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
        continue;
      }
    }
    else if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, em->bm, l, offsets)) {
        break;
      }
    }

    if (options->only_selected_uvs && !l) {
      continue;
    }

    return true;
  }

  return false;
}

static bool uvedit_have_selection_multi(const Scene *scene,
                                        const Span<Object *> objects,
                                        const UnwrapOptions *options)
{
  bool have_select = false;
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    if (uvedit_have_selection(scene, em, options)) {
      have_select = true;
      break;
    }
  }
  return have_select;
}

void ED_uvedit_get_aspect_from_material(Object *ob,
                                        const int material_index,
                                        float *r_aspx,
                                        float *r_aspy)
{
  if (UNLIKELY(material_index < 0 || material_index >= ob->totcol)) {
    *r_aspx = 1.0f;
    *r_aspy = 1.0f;
    return;
  }
  Image *ima;
  ED_object_get_active_image(ob, material_index + 1, &ima, nullptr, nullptr, nullptr);
  ED_image_get_uv_aspect(ima, nullptr, r_aspx, r_aspy);
}

void ED_uvedit_get_aspect(Object *ob, float *r_aspx, float *r_aspy)
{
  BMEditMesh *em = BKE_editmesh_from_object(ob);
  BLI_assert(em != nullptr);
  bool sloppy = true;
  bool selected = false;
  BMFace *efa = BM_mesh_active_face_get(em->bm, sloppy, selected);
  if (!efa) {
    *r_aspx = 1.0f;
    *r_aspy = 1.0f;
    return;
  }

  ED_uvedit_get_aspect_from_material(ob, efa->mat_nr, r_aspx, r_aspy);
}

float ED_uvedit_get_aspect_y(Object *ob)
{
  float aspect[2];
  ED_uvedit_get_aspect(ob, &aspect[0], &aspect[1]);
  return aspect[0] / aspect[1];
}

static bool uvedit_is_face_affected(const Scene *scene,
                                    const BMesh *bm,
                                    BMFace *efa,
                                    const UnwrapOptions *options,
                                    const BMUVOffsets &offsets)
{
  if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
    return false;
  }

  if (options->only_selected_faces && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
    return false;
  }

  if (options->only_selected_uvs) {
    BMLoop *l;
    BMIter iter;
    BM_ITER_ELEM (l, &iter, efa, BM_LOOPS_OF_FACE) {
      if (uvedit_uv_select_test(scene, bm, l, offsets)) {
        return true;
      }
    }
    return false;
  }

  return true;
}

/* Prepare unique indices for each unique pinned UV, even if it shares a BMVert.
 */
static void uvedit_prepare_pinned_indices(ParamHandle *handle,
                                          const Scene *scene,
                                          const BMesh *bm,
                                          BMFace *efa,
                                          const UnwrapOptions *options,
                                          const BMUVOffsets &offsets)
{
  BMIter liter;
  BMLoop *l;
  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    bool pin = BM_ELEM_CD_GET_BOOL(l, offsets.pin);
    if (options->pin_unselected && !pin) {
      pin = !uvedit_uv_select_test(scene, bm, l, offsets);
    }
    if (pin) {
      int bmvertindex = BM_elem_index_get(l->v);
      const float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      blender::geometry::uv_prepare_pin_index(handle, bmvertindex, luv);
    }
  }
}

static void construct_param_handle_face_add(ParamHandle *handle,
                                            const Scene *scene,
                                            const BMesh *bm,
                                            BMFace *efa,
                                            blender::geometry::ParamKey face_index,
                                            const UnwrapOptions *options,
                                            const BMUVOffsets &offsets,
                                            const int cd_weight_offset,
                                            const int cd_weight_index)
{
  blender::Array<ParamKey, BM_DEFAULT_NGON_STACK_SIZE> vkeys(efa->len);
  blender::Array<bool, BM_DEFAULT_NGON_STACK_SIZE> pin(efa->len);
  blender::Array<bool, BM_DEFAULT_NGON_STACK_SIZE> select(efa->len);
  blender::Array<const float *, BM_DEFAULT_NGON_STACK_SIZE> co(efa->len);
  blender::Array<float *, BM_DEFAULT_NGON_STACK_SIZE> uv(efa->len);
  blender::Array<float, BM_DEFAULT_NGON_STACK_SIZE> weight(efa->len);

  int i;

  BMIter liter;
  BMLoop *l;

  /* let parametrizer split the ngon, it can make better decisions
   * about which split is best for unwrapping than poly-fill. */
  BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
    float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);

    vkeys[i] = blender::geometry::uv_find_pin_index(handle, BM_elem_index_get(l->v), luv);
    co[i] = l->v->co;
    uv[i] = luv;
    pin[i] = BM_ELEM_CD_GET_BOOL(l, offsets.pin);
    select[i] = uvedit_uv_select_test(scene, bm, l, offsets);
    if (options->pin_unselected && !select[i]) {
      pin[i] = true;
    }

    /* Optional vertex group weighting. */
    if (cd_weight_offset >= 0 && cd_weight_index >= 0) {
      MDeformVert *dv = (MDeformVert *)BM_ELEM_CD_GET_VOID_P(l->v, cd_weight_offset);
      weight[i] = BKE_defvert_find_weight(dv, cd_weight_index);
    }
    else {
      weight[i] = 1.0f;
    }
  }

  blender::geometry::uv_parametrizer_face_add(handle,
                                              face_index,
                                              i,
                                              vkeys.data(),
                                              co.data(),
                                              uv.data(),
                                              weight.data(),
                                              pin.data(),
                                              select.data());
}

/* Set seams on UV Parametrizer based on options. */
static void construct_param_edge_set_seams(ParamHandle *handle,
                                           BMesh *bm,
                                           const UnwrapOptions *options)
{
  if (options->topology_from_uvs && !options->topology_from_uvs_use_seams) {
    return; /* Seams are not required with these options. */
  }

  const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);
  if (offsets.uv == -1) {
    return; /* UVs aren't present on BMesh. Nothing to do. */
  }

  BMEdge *edge;
  BMIter iter;
  BM_ITER_MESH (edge, &iter, bm, BM_EDGES_OF_MESH) {
    if (!BM_elem_flag_test(edge, BM_ELEM_SEAM)) {
      continue; /* No seam on this edge, nothing to do. */
    }

    /* Pinned vertices might have more than one ParamKey per BMVert.
     * Check all the BM_LOOPS_OF_EDGE to find all the ParamKeys.
     */
    BMLoop *l;
    BMIter liter;
    BM_ITER_ELEM (l, &liter, edge, BM_LOOPS_OF_EDGE) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      float *luv_next = BM_ELEM_CD_GET_FLOAT_P(l->next, offsets.uv);
      ParamKey vkeys[2];
      vkeys[0] = blender::geometry::uv_find_pin_index(handle, BM_elem_index_get(l->v), luv);
      vkeys[1] = blender::geometry::uv_find_pin_index(
          handle, BM_elem_index_get(l->next->v), luv_next);

      /* Set the seam. */
      blender::geometry::uv_parametrizer_edge_set_seam(handle, vkeys);
    }
  }
}

/*
 * Version of #construct_param_handle_multi with a separate BMesh parameter.
 */
static ParamHandle *construct_param_handle(const Scene *scene,
                                           Object *ob,
                                           BMesh *bm,
                                           const UnwrapOptions *options,
                                           int *r_count_failed = nullptr)
{
  BMFace *efa;
  BMIter iter;
  int i;

  ParamHandle *handle = new blender::geometry::ParamHandle();

  if (options->correct_aspect) {
    blender::geometry::uv_parametrizer_aspect_ratio(handle, ED_uvedit_get_aspect_y(ob));
  }

  /* we need the vert indices */
  BM_mesh_elem_index_ensure(bm, BM_VERT);

  const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);
  const int cd_weight_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
  const int cd_weight_index = BKE_object_defgroup_name_index(ob, options->weight_group);

  BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
    if (uvedit_is_face_affected(scene, bm, efa, options, offsets)) {
      uvedit_prepare_pinned_indices(handle, scene, bm, efa, options, offsets);
    }
  }

  BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
    if (uvedit_is_face_affected(scene, bm, efa, options, offsets)) {
      construct_param_handle_face_add(
          handle, scene, bm, efa, i, options, offsets, cd_weight_offset, cd_weight_index);
    }
  }

  construct_param_edge_set_seams(handle, bm, options);

  blender::geometry::uv_parametrizer_construct_end(
      handle, options->fill_holes, options->topology_from_uvs, r_count_failed);

  return handle;
}

/**
 * Version of #construct_param_handle that handles multiple objects.
 */
static ParamHandle *construct_param_handle_multi(const Scene *scene,
                                                 const Span<Object *> objects,
                                                 const UnwrapOptions *options)
{
  BMFace *efa;
  BMIter iter;
  int i;

  ParamHandle *handle = new blender::geometry::ParamHandle();

  if (options->correct_aspect) {
    Object *ob = objects[0];
    blender::geometry::uv_parametrizer_aspect_ratio(handle, ED_uvedit_get_aspect_y(ob));
  }

  /* we need the vert indices */
  EDBM_mesh_elem_index_ensure_multi(objects, BM_VERT);

  int offset = 0;

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);

    if (offsets.uv == -1) {
      continue;
    }

    const int cd_weight_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
    const int cd_weight_index = BKE_object_defgroup_name_index(obedit, options->weight_group);

    BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
      if (uvedit_is_face_affected(scene, bm, efa, options, offsets)) {
        uvedit_prepare_pinned_indices(handle, scene, bm, efa, options, offsets);
      }
    }

    BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, i) {
      if (uvedit_is_face_affected(scene, bm, efa, options, offsets)) {
        construct_param_handle_face_add(handle,
                                        scene,
                                        bm,
                                        efa,
                                        i + offset,
                                        options,
                                        offsets,
                                        cd_weight_offset,
                                        cd_weight_index);
      }
    }

    construct_param_edge_set_seams(handle, bm, options);

    offset += bm->totface;
  }

  blender::geometry::uv_parametrizer_construct_end(
      handle, options->fill_holes, options->topology_from_uvs, nullptr);

  return handle;
}

static void texface_from_original_index(const Scene *scene,
                                        const BMesh *bm,
                                        const BMUVOffsets &offsets,
                                        BMFace *efa,
                                        int index,
                                        float **r_uv,
                                        bool *r_pin,
                                        bool *r_select)
{
  BMLoop *l;
  BMIter liter;

  *r_uv = nullptr;
  *r_pin = false;
  *r_select = true;

  if (index == ORIGINDEX_NONE) {
    return;
  }

  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    if (BM_elem_index_get(l->v) == index) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      *r_uv = luv;
      *r_pin = BM_ELEM_CD_GET_BOOL(l, offsets.pin);
      *r_select = uvedit_uv_select_test(scene, bm, l, offsets);
      break;
    }
  }
}

static Mesh *subdivide_edit_mesh(const Object *object,
                                 const BMEditMesh *em,
                                 const SubsurfModifierData *smd)
{
  using namespace blender;
  Mesh *me_from_em = BKE_mesh_from_bmesh_for_eval_nomain(
      em->bm, nullptr, static_cast<const Mesh *>(object->data));
  BKE_mesh_ensure_default_orig_index_customdata(me_from_em);

  bke::subdiv::Settings settings = BKE_subsurf_modifier_settings_init(smd, false);
  /* A zero level must be prevented by #modifier_unwrap_state
   * since necessary data won't be available, see: #128958. */
  BLI_assert(settings.level > 0);

  /* Level 1 causes disconnected triangles, force level 2 to prevent this, see: #129503. */
  if (settings.level == 1) {
    settings.level = 2;
  }

  bke::subdiv::ToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << smd->levels) + 1;
  mesh_settings.use_optimal_display = (smd->flags & eSubsurfModifierFlag_ControlEdges);

  bke::subdiv::Subdiv *subdiv = bke::subdiv::new_from_mesh(&settings, me_from_em);
  if (!subdiv) {
    return nullptr;
  }
  Mesh *result = bke::subdiv::subdiv_to_mesh(subdiv, &mesh_settings, me_from_em);
  BKE_id_free(nullptr, me_from_em);
  bke::subdiv::free(subdiv);
  return result;
}

/**
 * Unwrap handle initialization for subsurf aware-unwrapper.
 * The many modifications required to make the original function(see above)
 * work justified the existence of a new function.
 */
static ParamHandle *construct_param_handle_subsurfed(const Scene *scene,
                                                     Object *ob,
                                                     BMEditMesh *em,
                                                     const UnwrapOptions *options,
                                                     int *r_count_failed = nullptr)
{
  /* pointers to modifier data for unwrap control */
  SubsurfModifierData *smd_real;
  /* Modifier initialization data, will control what type of subdivision will happen. */
  SubsurfModifierData smd = {{nullptr}};

  /* Holds a map to edit-faces for every subdivision-surface face.
   * These will be used to get hidden/ selected flags etc. */
  BMFace **faceMap;
  /* Similar to the above, we need a way to map edges to their original ones. */
  BMEdge **edgeMap;

  const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
  const int cd_weight_index = BKE_object_defgroup_name_index(ob, options->weight_group);

  ParamHandle *handle = new blender::geometry::ParamHandle();

  if (options->correct_aspect) {
    blender::geometry::uv_parametrizer_aspect_ratio(handle, ED_uvedit_get_aspect_y(ob));
  }

  /* number of subdivisions to perform */
  ModifierData *md = static_cast<ModifierData *>(ob->modifiers.first);
  smd_real = (SubsurfModifierData *)md;

  smd.levels = smd_real->levels;
  smd.subdivType = smd_real->subdivType;
  smd.flags = smd_real->flags;
  smd.quality = smd_real->quality;

  Mesh *subdiv_mesh = subdivide_edit_mesh(ob, em, &smd);

  const blender::Span<blender::float3> subsurf_positions = subdiv_mesh->vert_positions();
  const blender::Span<blender::int2> subsurf_edges = subdiv_mesh->edges();
  const blender::OffsetIndices subsurf_facess = subdiv_mesh->faces();
  const blender::Span<int> subsurf_corner_verts = subdiv_mesh->corner_verts();
  const blender::Span<MDeformVert> subsurf_deform_verts = subdiv_mesh->deform_verts();

  const int *origVertIndices = static_cast<const int *>(
      CustomData_get_layer(&subdiv_mesh->vert_data, CD_ORIGINDEX));
  const int *origEdgeIndices = static_cast<const int *>(
      CustomData_get_layer(&subdiv_mesh->edge_data, CD_ORIGINDEX));
  const int *origPolyIndices = static_cast<const int *>(
      CustomData_get_layer(&subdiv_mesh->face_data, CD_ORIGINDEX));

  faceMap = MEM_malloc_arrayN<BMFace *>(subdiv_mesh->faces_num, "unwrap_edit_face_map");

  BM_mesh_elem_index_ensure(em->bm, BM_VERT);
  BM_mesh_elem_table_ensure(em->bm, BM_EDGE | BM_FACE);

  /* map subsurfed faces to original editFaces */
  for (int i = 0; i < subdiv_mesh->faces_num; i++) {
    faceMap[i] = BM_face_at_index(em->bm, origPolyIndices[i]);
  }

  edgeMap = MEM_malloc_arrayN<BMEdge *>(subdiv_mesh->edges_num, "unwrap_edit_edge_map");

  /* map subsurfed edges to original editEdges */
  for (int i = 0; i < subdiv_mesh->edges_num; i++) {
    /* not all edges correspond to an old edge */
    edgeMap[i] = (origEdgeIndices[i] != ORIGINDEX_NONE) ?
                     BM_edge_at_index(em->bm, origEdgeIndices[i]) :
                     nullptr;
  }

  /* Prepare and feed faces to the solver. */
  for (const int i : subsurf_facess.index_range()) {
    ParamKey key, vkeys[4];
    bool pin[4], select[4];
    const float *co[4];
    float *uv[4];
    float weight[4];
    BMFace *origFace = faceMap[i];

    if (scene->toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) {
      if (BM_elem_flag_test(origFace, BM_ELEM_HIDDEN)) {
        continue;
      }
    }
    else {
      if (BM_elem_flag_test(origFace, BM_ELEM_HIDDEN) ||
          (options->only_selected_faces && !BM_elem_flag_test(origFace, BM_ELEM_SELECT)))
      {
        continue;
      }
    }

    const blender::Span<int> poly_corner_verts = subsurf_corner_verts.slice(subsurf_facess[i]);

    /* We will not check for v4 here. Sub-surface faces always have 4 vertices. */
    BLI_assert(poly_corner_verts.size() == 4);
    key = (ParamKey)i;
    vkeys[0] = (ParamKey)poly_corner_verts[0];
    vkeys[1] = (ParamKey)poly_corner_verts[1];
    vkeys[2] = (ParamKey)poly_corner_verts[2];
    vkeys[3] = (ParamKey)poly_corner_verts[3];

    co[0] = subsurf_positions[poly_corner_verts[0]];
    co[1] = subsurf_positions[poly_corner_verts[1]];
    co[2] = subsurf_positions[poly_corner_verts[2]];
    co[3] = subsurf_positions[poly_corner_verts[3]];

    /* Optional vertex group weights. */
    if (cd_weight_index >= 0) {
      weight[0] = BKE_defvert_find_weight(&subsurf_deform_verts[poly_corner_verts[0]],
                                          cd_weight_index);
      weight[1] = BKE_defvert_find_weight(&subsurf_deform_verts[poly_corner_verts[1]],
                                          cd_weight_index);
      weight[2] = BKE_defvert_find_weight(&subsurf_deform_verts[poly_corner_verts[2]],
                                          cd_weight_index);
      weight[3] = BKE_defvert_find_weight(&subsurf_deform_verts[poly_corner_verts[3]],
                                          cd_weight_index);
    }
    else {
      weight[0] = 1.0f;
      weight[1] = 1.0f;
      weight[2] = 1.0f;
      weight[3] = 1.0f;
    }

    /* This is where all the magic is done.
     * If the vertex exists in the, we pass the original uv pointer to the solver, thus
     * flushing the solution to the edit mesh. */
    texface_from_original_index(scene,
                                em->bm,
                                offsets,
                                origFace,
                                origVertIndices[poly_corner_verts[0]],
                                &uv[0],
                                &pin[0],
                                &select[0]);
    texface_from_original_index(scene,
                                em->bm,
                                offsets,
                                origFace,
                                origVertIndices[poly_corner_verts[1]],
                                &uv[1],
                                &pin[1],
                                &select[1]);
    texface_from_original_index(scene,
                                em->bm,
                                offsets,
                                origFace,
                                origVertIndices[poly_corner_verts[2]],
                                &uv[2],
                                &pin[2],
                                &select[2]);
    texface_from_original_index(scene,
                                em->bm,
                                offsets,
                                origFace,
                                origVertIndices[poly_corner_verts[3]],
                                &uv[3],
                                &pin[3],
                                &select[3]);

    blender::geometry::uv_parametrizer_face_add(
        handle, key, 4, vkeys, co, uv, weight, pin, select);
  }

  /* These are calculated from original mesh too. */
  for (const int64_t i : subsurf_edges.index_range()) {
    if ((edgeMap[i] != nullptr) && BM_elem_flag_test(edgeMap[i], BM_ELEM_SEAM)) {
      const blender::int2 &edge = subsurf_edges[i];
      ParamKey vkeys[2];
      vkeys[0] = (ParamKey)edge[0];
      vkeys[1] = (ParamKey)edge[1];
      blender::geometry::uv_parametrizer_edge_set_seam(handle, vkeys);
    }
  }

  blender::geometry::uv_parametrizer_construct_end(
      handle, options->fill_holes, options->topology_from_uvs, r_count_failed);

  /* cleanup */
  MEM_freeN(faceMap);
  MEM_freeN(edgeMap);
  BKE_id_free(nullptr, subdiv_mesh);

  return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Minimize Stretch Operator
 * \{ */

struct MinStretch {
  const Scene *scene;
  Vector<Object *> objects_edit;
  ParamHandle *handle;
  float blend;
  double lasttime;
  int i, iterations;
  wmTimer *timer;
};

static bool minimize_stretch_init(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  UnwrapOptions options{};
  options.topology_from_uvs = true;
  options.fill_holes = RNA_boolean_get(op->ptr, "fill_holes");
  options.only_selected_faces = true;
  options.only_selected_uvs = true;
  options.correct_aspect = true;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, CTX_wm_view3d(C));

  if (!uvedit_have_selection_multi(scene, objects, &options)) {
    return false;
  }

  MinStretch *ms = MEM_new<MinStretch>(__func__);
  ms->scene = scene;
  ms->objects_edit = objects;
  ms->blend = RNA_float_get(op->ptr, "blend");
  ms->iterations = RNA_int_get(op->ptr, "iterations");
  ms->i = 0;
  ms->handle = construct_param_handle_multi(scene, objects, &options);
  ms->lasttime = BLI_time_now_seconds();

  blender::geometry::uv_parametrizer_stretch_begin(ms->handle);
  if (ms->blend != 0.0f) {
    blender::geometry::uv_parametrizer_stretch_blend(ms->handle, ms->blend);
  }

  op->customdata = ms;

  return true;
}

static void minimize_stretch_iteration(bContext *C, wmOperator *op, bool interactive)
{
  MinStretch *ms = static_cast<MinStretch *>(op->customdata);
  ScrArea *area = CTX_wm_area(C);
  const Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  blender::geometry::uv_parametrizer_stretch_blend(ms->handle, ms->blend);
  blender::geometry::uv_parametrizer_stretch_iter(ms->handle);

  ms->i++;
  RNA_int_set(op->ptr, "iterations", ms->i);

  if (interactive && (BLI_time_now_seconds() - ms->lasttime > 0.5)) {
    char str[UI_MAX_DRAW_STR];

    blender::geometry::uv_parametrizer_flush(ms->handle);

    if (area) {
      SNPRINTF_UTF8(str, IFACE_("Minimize Stretch. Blend %.2f"), ms->blend);
      ED_area_status_text(area, str);
      ED_workspace_status_text(C, IFACE_("Press + and -, or scroll wheel to set blending"));
    }

    ms->lasttime = BLI_time_now_seconds();

    for (Object *obedit : ms->objects_edit) {
      BMEditMesh *em = BKE_editmesh_from_object(obedit);

      if (synced_selection && (em->bm->totfacesel == 0)) {
        continue;
      }

      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }
}

static void minimize_stretch_exit(bContext *C, wmOperator *op, bool cancel)
{
  MinStretch *ms = static_cast<MinStretch *>(op->customdata);
  ScrArea *area = CTX_wm_area(C);
  const Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  ED_area_status_text(area, nullptr);
  ED_workspace_status_text(C, nullptr);

  if (ms->timer) {
    WM_event_timer_remove(CTX_wm_manager(C), CTX_wm_window(C), ms->timer);
  }

  if (cancel) {
    blender::geometry::uv_parametrizer_flush_restore(ms->handle);
  }
  else {
    blender::geometry::uv_parametrizer_flush(ms->handle);
  }

  blender::geometry::uv_parametrizer_stretch_end(ms->handle);
  delete (ms->handle);

  for (Object *obedit : ms->objects_edit) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totfacesel == 0)) {
      continue;
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  MEM_delete(ms);
  op->customdata = nullptr;
}

static wmOperatorStatus minimize_stretch_exec(bContext *C, wmOperator *op)
{
  int i, iterations;

  if (!minimize_stretch_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  iterations = RNA_int_get(op->ptr, "iterations");
  for (i = 0; i < iterations; i++) {
    minimize_stretch_iteration(C, op, false);
  }
  minimize_stretch_exit(C, op, false);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus minimize_stretch_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  if (!minimize_stretch_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  minimize_stretch_iteration(C, op, true);

  MinStretch *ms = static_cast<MinStretch *>(op->customdata);
  WM_event_add_modal_handler(C, op);
  ms->timer = WM_event_timer_add(CTX_wm_manager(C), CTX_wm_window(C), TIMER, 0.01f);

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus minimize_stretch_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  MinStretch *ms = static_cast<MinStretch *>(op->customdata);

  switch (event->type) {
    case EVT_ESCKEY:
    case RIGHTMOUSE:
      minimize_stretch_exit(C, op, true);
      return OPERATOR_CANCELLED;
    case EVT_RETKEY:
    case EVT_PADENTER:
    case LEFTMOUSE:
      minimize_stretch_exit(C, op, false);
      return OPERATOR_FINISHED;
    case EVT_PADPLUSKEY:
    case WHEELUPMOUSE:
      if (event->val == KM_PRESS) {
        if (ms->blend < 0.95f) {
          ms->blend += 0.1f;
          ms->lasttime = 0.0f;
          RNA_float_set(op->ptr, "blend", ms->blend);
          minimize_stretch_iteration(C, op, true);
        }
      }
      break;
    case EVT_PADMINUS:
    case WHEELDOWNMOUSE:
      if (event->val == KM_PRESS) {
        if (ms->blend > 0.05f) {
          ms->blend -= 0.1f;
          ms->lasttime = 0.0f;
          RNA_float_set(op->ptr, "blend", ms->blend);
          minimize_stretch_iteration(C, op, true);
        }
      }
      break;
    case TIMER:
      if (ms->timer == event->customdata) {
        double start = BLI_time_now_seconds();

        do {
          minimize_stretch_iteration(C, op, true);
        } while (BLI_time_now_seconds() - start < 0.01);
      }
      break;
    default: {
      break;
    }
  }

  if (ms->iterations && ms->i >= ms->iterations) {
    minimize_stretch_exit(C, op, false);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void minimize_stretch_cancel(bContext *C, wmOperator *op)
{
  minimize_stretch_exit(C, op, true);
}

void UV_OT_minimize_stretch(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Minimize Stretch";
  ot->idname = "UV_OT_minimize_stretch";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR_XY | OPTYPE_BLOCKING;
  ot->description = "Reduce UV stretching by relaxing angles";

  /* API callbacks. */
  ot->exec = minimize_stretch_exec;
  ot->invoke = minimize_stretch_invoke;
  ot->modal = minimize_stretch_modal;
  ot->cancel = minimize_stretch_cancel;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "fill_holes",
                  true,
                  "Fill Holes",
                  "Virtually fill holes in mesh before unwrapping, to better avoid overlaps and "
                  "preserve symmetry");
  RNA_def_float_factor(ot->srna,
                       "blend",
                       0.0f,
                       0.0f,
                       1.0f,
                       "Blend",
                       "Blend factor between stretch minimized and original",
                       0.0f,
                       1.0f);
  RNA_def_int(ot->srna,
              "iterations",
              0,
              0,
              INT_MAX,
              "Iterations",
              "Number of iterations to run, 0 is unlimited when run interactively",
              0,
              100);
}

/** \} */

static void island_uv_transform(FaceIsland *island,
                                const float matrix[2][2],    /* Scale and rotation. */
                                const float pre_translate[2] /* (pre) Translation. */
)
{
  /* Use a pre-transform to compute `A * (x+b)`
   *
   * \note Ordinarily, we'd use a post_transform like `A * x + b`
   * In general, post-transforms are easier to work with when using homogeneous coordinates.
   *
   * When UV mapping into the unit square, post-transforms can lose precision on small islands.
   * Instead we're using a pre-transform to maintain precision.
   *
   * To convert post-transform to pre-transform, use `A * x + b == A * (x + c), c = A^-1 * b`
   */

  const int cd_loop_uv_offset = island->offsets.uv;
  const int faces_len = island->faces_len;
  for (int i = 0; i < faces_len; i++) {
    BMFace *f = island->faces[i];
    BMLoop *l;
    BMIter iter;
    BM_ITER_ELEM (l, &iter, f, BM_LOOPS_OF_FACE) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
      blender::geometry::mul_v2_m2_add_v2v2(luv, matrix, luv, pre_translate);
    }
  }
}

/**
 * Calculates distance to nearest UDIM image tile in UV space and its UDIM tile number.
 */
static float uv_nearest_image_tile_distance(const Image *image,
                                            const float coords[2],
                                            float nearest_tile_co[2])
{
  BKE_image_find_nearest_tile_with_offset(image, coords, nearest_tile_co);

  /* Add 0.5 to get tile center coordinates. */
  float nearest_tile_center_co[2] = {nearest_tile_co[0], nearest_tile_co[1]};
  add_v2_fl(nearest_tile_center_co, 0.5f);

  return len_squared_v2v2(coords, nearest_tile_center_co);
}

/**
 * Calculates distance to nearest UDIM grid tile in UV space and its UDIM tile number.
 */
static float uv_nearest_grid_tile_distance(const int udim_grid[2],
                                           const float coords[2],
                                           float nearest_tile_co[2])
{
  const float coords_floor[2] = {floorf(coords[0]), floorf(coords[1])};

  if (coords[0] > udim_grid[0]) {
    nearest_tile_co[0] = udim_grid[0] - 1;
  }
  else if (coords[0] < 0) {
    nearest_tile_co[0] = 0;
  }
  else {
    nearest_tile_co[0] = coords_floor[0];
  }

  if (coords[1] > udim_grid[1]) {
    nearest_tile_co[1] = udim_grid[1] - 1;
  }
  else if (coords[1] < 0) {
    nearest_tile_co[1] = 0;
  }
  else {
    nearest_tile_co[1] = coords_floor[1];
  }

  /* Add 0.5 to get tile center coordinates. */
  float nearest_tile_center_co[2] = {nearest_tile_co[0], nearest_tile_co[1]};
  add_v2_fl(nearest_tile_center_co, 0.5f);

  return len_squared_v2v2(coords, nearest_tile_center_co);
}

static bool island_has_pins(const Scene *scene,
                            const BMesh *bm,
                            FaceIsland *island,
                            const blender::geometry::UVPackIsland_Params *params)
{
  const bool pin_unselected = params->pin_unselected;
  const bool only_selected_faces = params->only_selected_faces;
  BMLoop *l;
  BMIter iter;
  const int pin_offset = island->offsets.pin;
  for (int i = 0; i < island->faces_len; i++) {
    BMFace *efa = island->faces[i];
    if (pin_unselected && only_selected_faces && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      return true;
    }
    BM_ITER_ELEM (l, &iter, island->faces[i], BM_LOOPS_OF_FACE) {
      if (BM_ELEM_CD_GET_BOOL(l, pin_offset)) {
        return true;
      }
      if (pin_unselected && !uvedit_uv_select_test(scene, bm, l, island->offsets)) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Pack UV islands from multiple objects.
 *
 * \param scene: Scene containing the objects to be packed.
 * \param objects: Array of Objects to pack.
 * \param objects_len: Length of `objects` array.
 * \param bmesh_override: BMesh array aligned with `objects`.
 * Optional, when non-null this overrides object's BMesh.
 * This is needed to perform UV packing on objects that aren't in edit-mode.
 * \param udim_source_closest: UDIM source SpaceImage.
 * \param original_selection: Pack to original selection.
 * \param notify_wm: Notify the WM of any changes. (UI thread only.)
 * \param params: Parameters and options to pass to the packing engine.
 */
static void uvedit_pack_islands_multi(const Scene *scene,
                                      const Span<Object *> objects,
                                      BMesh **bmesh_override,
                                      const SpaceImage *udim_source_closest,
                                      const bool original_selection,
                                      const bool notify_wm,
                                      const rctf *custom_region,
                                      blender::geometry::UVPackIsland_Params *params)
{
  blender::Vector<FaceIsland *> island_vector;
  blender::Vector<bool> pinned_vector;

  for (const int ob_index : objects.index_range()) {
    Object *obedit = objects[ob_index];
    BMesh *bm = nullptr;
    if (bmesh_override) {
      /* NOTE: obedit is still required for aspect ratio and ID_RECALC_GEOMETRY. */
      bm = bmesh_override[ob_index];
    }
    else {
      BMEditMesh *em = BKE_editmesh_from_object(obedit);
      bm = em->bm;
    }
    BLI_assert(bm);

    const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);
    if (offsets.uv == -1) {
      continue;
    }

    const float aspect_y = params->correct_aspect ? ED_uvedit_get_aspect_y(obedit) : 1.0f;

    bool only_selected_faces = params->only_selected_faces;
    bool only_selected_uvs = params->only_selected_uvs;
    const bool ignore_pinned = params->pin_method == ED_UVPACK_PIN_IGNORE;
    if (ignore_pinned && params->pin_unselected) {
      only_selected_faces = false;
      only_selected_uvs = false;
    }
    ListBase island_list = {nullptr};
    bm_mesh_calc_uv_islands(scene,
                            bm,
                            &island_list,
                            only_selected_faces,
                            only_selected_uvs,
                            params->use_seams,
                            aspect_y,
                            offsets);

    /* Remove from linked list and append to blender::Vector. */
    LISTBASE_FOREACH_MUTABLE (FaceIsland *, island, &island_list) {
      BLI_remlink(&island_list, island);
      const bool pinned = island_has_pins(scene, bm, island, params);
      if (ignore_pinned && pinned) {
        MEM_freeN(island->faces);
        MEM_freeN(island);
        continue;
      }
      island_vector.append(island);
      pinned_vector.append(pinned);
    }
  }

  if (island_vector.is_empty()) {
    return;
  }

  /* Coordinates of bounding box containing all selected UVs. */
  float selection_min_co[2], selection_max_co[2];
  INIT_MINMAX2(selection_min_co, selection_max_co);

  for (int index = 0; index < island_vector.size(); index++) {
    FaceIsland *island = island_vector[index];

    for (int i = 0; i < island->faces_len; i++) {
      BMFace *f = island->faces[i];
      BM_face_uv_minmax(f, selection_min_co, selection_max_co, island->offsets.uv);
    }
  }

  /* Center of bounding box containing all selected UVs. */
  float selection_center[2];
  mid_v2_v2v2(selection_center, selection_min_co, selection_max_co);

  if (original_selection) {
    /* Protect against degenerate source AABB. */
    if ((selection_max_co[0] - selection_min_co[0]) * (selection_max_co[1] - selection_min_co[1]) >
        1e-40f)
    {
      copy_v2_v2(params->udim_base_offset, selection_min_co);
      params->target_extent = selection_max_co[1] - selection_min_co[1];
      params->target_aspect_y = (selection_max_co[0] - selection_min_co[0]) /
                                (selection_max_co[1] - selection_min_co[1]);
    }
  }
  else if (custom_region) {
    if (!BLI_rctf_is_empty(custom_region)) {
      const blender::float2 custom_region_size = {
          BLI_rctf_size_x(custom_region),
          BLI_rctf_size_y(custom_region),
      };
      ARRAY_SET_ITEMS(params->udim_base_offset, custom_region->xmin, custom_region->ymin);
      params->target_extent = custom_region_size.y;
      params->target_aspect_y = custom_region_size.x / custom_region_size.y;
    }
  }

  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  Heap *heap = BLI_heap_new();

  blender::Vector<blender::geometry::PackIsland *> pack_island_vector;
  for (int i = 0; i < island_vector.size(); i++) {
    FaceIsland *face_island = island_vector[i];
    blender::geometry::PackIsland *pack_island = new blender::geometry::PackIsland();
    pack_island->caller_index = i;
    pack_island->aspect_y = face_island->aspect_y;
    pack_island->pinned = pinned_vector[i];
    pack_island_vector.append(pack_island);

    for (int i = 0; i < face_island->faces_len; i++) {
      BMFace *f = face_island->faces[i];

      /* Storage. */
      blender::Array<blender::float2> uvs(f->len);

      /* Obtain UVs of face. */
      BMLoop *l;
      BMIter iter;
      int j;
      BM_ITER_ELEM_INDEX (l, &iter, f, BM_LOOPS_OF_FACE, j) {
        copy_v2_v2(uvs[j], BM_ELEM_CD_GET_FLOAT_P(l, face_island->offsets.uv));
      }

      pack_island->add_polygon(uvs, arena, heap);

      BLI_memarena_clear(arena);
    }
  }
  BLI_heap_free(heap, nullptr);
  BLI_memarena_free(arena);

  const float scale = pack_islands(pack_island_vector, *params);
  const bool is_cancelled = params->isCancelled();

  float base_offset[2] = {0.0f, 0.0f};
  copy_v2_v2(base_offset, params->udim_base_offset);

  if (udim_source_closest) {
    const Image *image = udim_source_closest->image;
    const int *udim_grid = udim_source_closest->tile_grid_shape;
    /* Check if selection lies on a valid UDIM grid tile. */
    bool is_valid_udim = uv_coords_isect_udim(image, udim_grid, selection_center);
    if (is_valid_udim) {
      base_offset[0] = floorf(selection_center[0]);
      base_offset[1] = floorf(selection_center[1]);
    }
    /* If selection doesn't lie on any UDIM then find the closest UDIM grid or image tile. */
    else {
      float nearest_image_tile_co[2] = {FLT_MAX, FLT_MAX};
      float nearest_image_tile_dist = FLT_MAX, nearest_grid_tile_dist = FLT_MAX;
      if (image) {
        nearest_image_tile_dist = uv_nearest_image_tile_distance(
            image, selection_center, nearest_image_tile_co);
      }

      float nearest_grid_tile_co[2] = {0.0f, 0.0f};
      nearest_grid_tile_dist = uv_nearest_grid_tile_distance(
          udim_grid, selection_center, nearest_grid_tile_co);

      base_offset[0] = (nearest_image_tile_dist < nearest_grid_tile_dist) ?
                           nearest_image_tile_co[0] :
                           nearest_grid_tile_co[0];
      base_offset[1] = (nearest_image_tile_dist < nearest_grid_tile_dist) ?
                           nearest_image_tile_co[1] :
                           nearest_grid_tile_co[1];
    }
  }

  float matrix[2][2];
  float matrix_inverse[2][2];
  float pre_translate[2];
  for (const int64_t i : pack_island_vector.index_range()) {
    if (is_cancelled) {
      continue;
    }
    blender::geometry::PackIsland *pack_island = pack_island_vector[i];
    FaceIsland *island = island_vector[pack_island->caller_index];
    const float island_scale = pack_island->can_scale_(*params) ? scale : 1.0f;
    pack_island->build_transformation(island_scale, pack_island->angle, matrix);
    invert_m2_m2(matrix_inverse, matrix);

    /* Add base_offset, post transform. */
    if (!pinned_vector[i] || params->pin_method != ED_UVPACK_PIN_LOCK_ALL) {
      mul_v2_m2v2(pre_translate, matrix_inverse, base_offset);

      /* Add pre-translation from #pack_islands. */
      pre_translate[0] += pack_island->pre_translate.x;
      pre_translate[1] += pack_island->pre_translate.y;

      /* Perform the transformation. */
      island_uv_transform(island, matrix, pre_translate);
    }
  }

  for (const int64_t i : pack_island_vector.index_range()) {
    blender::geometry::PackIsland *pack_island = pack_island_vector[i];
    /* Cleanup memory. */
    pack_island_vector[i] = nullptr;
    delete pack_island;
  }

  if (notify_wm && !is_cancelled) {
    for (Object *obedit : objects) {
      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
      WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
    }
  }

  for (FaceIsland *island : island_vector) {
    MEM_freeN(island->faces);
    MEM_freeN(island);
  }
}

/* -------------------------------------------------------------------- */
/** \name Pack UV Islands Operator
 * \{ */

/* TODO: support this, interaction with the job-system needs to be handled carefully. */
// #define USE_INTERACTIVE_PACK

/* Packing targets. */
enum {
  PACK_UDIM_SRC_CLOSEST = 0,
  PACK_UDIM_SRC_ACTIVE,
  PACK_ORIGINAL_AABB,
  PACK_CUSTOM_REGION,
};

struct UVPackIslandsData {
  wmWindowManager *wm;

  const Scene *scene;

  Vector<Object *> objects;
  const SpaceImage *sima;
  int udim_source;

  bContext *undo_context;
  const char *undo_str;
  bool use_job;

  blender::geometry::UVPackIsland_Params pack_island_params;
  rctf custom_region;
};

static void pack_islands_startjob(void *pidv, wmJobWorkerStatus *worker_status)
{
  worker_status->progress = 0.02f;

  UVPackIslandsData *pid = static_cast<UVPackIslandsData *>(pidv);

  pid->pack_island_params.stop = &worker_status->stop;
  pid->pack_island_params.do_update = &worker_status->do_update;
  pid->pack_island_params.progress = &worker_status->progress;

  uvedit_pack_islands_multi(pid->scene,
                            pid->objects,
                            nullptr,
                            (pid->udim_source == PACK_UDIM_SRC_CLOSEST) ? pid->sima : nullptr,
                            (pid->udim_source == PACK_ORIGINAL_AABB),
                            !pid->use_job,
                            (pid->udim_source == PACK_CUSTOM_REGION) ? &pid->custom_region :
                                                                       nullptr,
                            &pid->pack_island_params);

  worker_status->progress = 0.99f;
  worker_status->do_update = true;
}

static void pack_islands_endjob(void *pidv)
{
  UVPackIslandsData *pid = static_cast<UVPackIslandsData *>(pidv);
  for (Object *obedit : pid->objects) {
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
  }
  WM_main_add_notifier(NC_SPACE | ND_SPACE_IMAGE, nullptr);

  if (pid->undo_str) {
    ED_undo_push(pid->undo_context, pid->undo_str);
  }
}

static void pack_islands_freejob(void *pidv)
{
  WM_cursor_wait(false);
  UVPackIslandsData *pid = static_cast<UVPackIslandsData *>(pidv);
  WM_locked_interface_set(pid->wm, false);
  MEM_delete(pid);
}

static wmOperatorStatus pack_islands_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Scene *scene = CTX_data_scene(C);
  const SpaceImage *sima = CTX_wm_space_image(C);
  const ToolSettings *ts = scene->toolsettings;

  UnwrapOptions options = unwrap_options_get(op, nullptr, scene->toolsettings);
  options.topology_from_uvs = true;
  options.only_selected_faces = true;
  options.only_selected_uvs = true;
  options.fill_holes = false;
  options.correct_aspect = true;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, CTX_wm_view3d(C));

  /* Early exit in case no UVs are selected. */
  if (!uvedit_have_selection_multi(scene, objects, &options)) {
    return OPERATOR_CANCELLED;
  }

  /* RNA props */
  const int udim_source = RNA_enum_get(op->ptr, "udim_source");
  if (RNA_struct_property_is_set(op->ptr, "margin")) {
    scene->toolsettings->uvcalc_margin = RNA_float_get(op->ptr, "margin");
  }
  else {
    RNA_float_set(op->ptr, "margin", scene->toolsettings->uvcalc_margin);
  }

  UVPackIslandsData *pid = MEM_new<UVPackIslandsData>(__func__);
  pid->use_job = op->flag & OP_IS_INVOKE;
  pid->scene = scene;
  pid->objects = std::move(objects);
  pid->sima = sima;
  pid->udim_source = udim_source;
  pid->wm = CTX_wm_manager(C);

  if (udim_source == PACK_CUSTOM_REGION) {
    if (ts->uv_flag & UV_FLAG_CUSTOM_REGION) {
      pid->custom_region = ts->uv_custom_region;
    }
    else {
      pid->custom_region.xmin = pid->custom_region.ymin = 0.0f;
      pid->custom_region.xmax = pid->custom_region.ymax = 1.0f;
    }
  }
  else {
    pid->custom_region = {0.0f};
  }

  blender::geometry::UVPackIsland_Params &pack_island_params = pid->pack_island_params;
  {
    /* Call default constructor and copy the defaults. */
    blender::geometry::UVPackIsland_Params default_params;
    pack_island_params = default_params;
  }

  pack_island_params.setFromUnwrapOptions(options);
  if (RNA_boolean_get(op->ptr, "rotate")) {
    pack_island_params.rotate_method = eUVPackIsland_RotationMethod(
        RNA_enum_get(op->ptr, "rotate_method"));
  }
  else {
    pack_island_params.rotate_method = ED_UVPACK_ROTATION_NONE;
  }
  pack_island_params.scale_to_fit = RNA_boolean_get(op->ptr, "scale");
  pack_island_params.merge_overlap = RNA_boolean_get(op->ptr, "merge_overlap");

  if (RNA_boolean_get(op->ptr, "pin")) {
    pack_island_params.pin_method = eUVPackIsland_PinMethod(RNA_enum_get(op->ptr, "pin_method"));
  }
  else {
    pack_island_params.pin_method = ED_UVPACK_PIN_NONE;
  }

  pack_island_params.margin_method = eUVPackIsland_MarginMethod(
      RNA_enum_get(op->ptr, "margin_method"));
  pack_island_params.margin = RNA_float_get(op->ptr, "margin");
  pack_island_params.shape_method = eUVPackIsland_ShapeMethod(
      RNA_enum_get(op->ptr, "shape_method"));

  if (udim_source == PACK_UDIM_SRC_ACTIVE) {
    pack_island_params.setUDIMOffsetFromSpaceImage(sima);
  }

  /* Needed even when jobs aren't used, see: #146195. */
  G.is_break = false;

  if (pid->use_job) {
    /* Setup job. */
    if (pid->wm->op_undo_depth == 0) {
      /* The job must do its own undo push. */
      pid->undo_context = C;
      pid->undo_str = op->type->name;
    }

    wmJob *wm_job = WM_jobs_get(
        pid->wm, CTX_wm_window(C), scene, "Packing UVs...", WM_JOB_PROGRESS, WM_JOB_TYPE_UV_PACK);
    WM_jobs_customdata_set(wm_job, pid, pack_islands_freejob);
    WM_jobs_timer(wm_job, 0.1, 0, 0);
    WM_locked_interface_set_with_flags(pid->wm, REGION_DRAW_LOCK_RENDER);
    WM_jobs_callbacks(wm_job, pack_islands_startjob, nullptr, nullptr, pack_islands_endjob);

    WM_cursor_wait(true);
    WM_jobs_start(CTX_wm_manager(C), wm_job);
    return OPERATOR_FINISHED;
  }

  wmJobWorkerStatus worker_status = {};
  pack_islands_startjob(pid, &worker_status);
  pack_islands_endjob(pid);
  pack_islands_freejob(pid);

  return OPERATOR_FINISHED;
}

static const EnumPropertyItem pack_margin_method_items[] = {
    {ED_UVPACK_MARGIN_SCALED,
     "SCALED",
     0,
     "Scaled",
     "Use scale of existing UVs to multiply margin"},
    {ED_UVPACK_MARGIN_ADD, "ADD", 0, "Add", "Just add the margin, ignoring any UV scale"},
    {ED_UVPACK_MARGIN_FRACTION,
     "FRACTION",
     0,
     "Fraction",
     "Specify a precise fraction of final UV output"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem pack_rotate_method_items[] = {
    {ED_UVPACK_ROTATION_ANY, "ANY", 0, "Any", "Any angle is allowed for rotation"},
    {ED_UVPACK_ROTATION_CARDINAL,
     "CARDINAL",
     0,
     "Cardinal",
     "Only 90 degree rotations are allowed"},
    RNA_ENUM_ITEM_SEPR,

#define PACK_ROTATE_METHOD_AXIS_ALIGNED_OFFSET 3
    {ED_UVPACK_ROTATION_AXIS_ALIGNED,
     "AXIS_ALIGNED",
     0,
     "Axis-aligned",
     "Rotated to a minimal rectangle, either vertical or horizontal"},
    {ED_UVPACK_ROTATION_AXIS_ALIGNED_X,
     "AXIS_ALIGNED_X",
     0,
     "Axis-aligned (Horizontal)",
     "Rotate islands to be aligned horizontally"},
    {ED_UVPACK_ROTATION_AXIS_ALIGNED_Y,
     "AXIS_ALIGNED_Y",
     0,
     "Axis-aligned (Vertical)",
     "Rotate islands to be aligned vertically"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem pack_shape_method_items[] = {
    {ED_UVPACK_SHAPE_CONCAVE, "CONCAVE", 0, "Exact Shape (Concave)", "Uses exact geometry"},
    {ED_UVPACK_SHAPE_CONVEX, "CONVEX", 0, "Boundary Shape (Convex)", "Uses convex hull"},
    RNA_ENUM_ITEM_SEPR,
    {ED_UVPACK_SHAPE_AABB, "AABB", 0, "Bounding Box", "Uses bounding boxes"},
    {0, nullptr, 0, nullptr, nullptr},
};

/**
 * \note #ED_UVPACK_PIN_NONE is exposed as a boolean "pin".
 * \note #ED_UVPACK_PIN_IGNORE is intentionally not exposed as it is confusing from the UI level
 * (users can simply not select these islands).
 * The option is kept internally because it's used for live unwrap.
 */
static const EnumPropertyItem pinned_islands_method_items[] = {
    {ED_UVPACK_PIN_LOCK_SCALE, "SCALE", 0, "Scale", "Pinned islands won't rescale"},
    {ED_UVPACK_PIN_LOCK_ROTATION, "ROTATION", 0, "Rotation", "Pinned islands won't rotate"},
    {ED_UVPACK_PIN_LOCK_ROTATION_SCALE,
     "ROTATION_SCALE",
     0,
     "Rotation and Scale",
     "Pinned islands will translate only"},
    {ED_UVPACK_PIN_LOCK_ALL, "LOCKED", 0, "All", "Pinned islands are locked in place"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void uv_pack_islands_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  layout->prop(op->ptr, "shape_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  {
    layout->prop(op->ptr, "rotate", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    uiLayout *sub = &layout->row(true);
    sub->active_set(RNA_boolean_get(op->ptr, "rotate"));
    sub->prop(op->ptr, "rotate_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->separator();
  }
  layout->prop(op->ptr, "margin_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "margin", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->separator();
  {
    layout->prop(op->ptr, "pin", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    uiLayout *sub = &layout->row(true);
    sub->active_set(RNA_boolean_get(op->ptr, "pin"));
    sub->prop(op->ptr, "pin_method", UI_ITEM_NONE, IFACE_("Lock Method"), ICON_NONE);
    layout->separator();
  }
  layout->prop(op->ptr, "merge_overlap", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "udim_source", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->separator();
}

static wmOperatorStatus uv_pack_islands_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return WM_operator_props_popup_confirm_ex(C, op, event, IFACE_("Pack Islands"), IFACE_("Pack"));
}

void UV_OT_pack_islands(wmOperatorType *ot)
{
  static const EnumPropertyItem pack_target[] = {
      {PACK_UDIM_SRC_CLOSEST, "CLOSEST_UDIM", 0, "Closest UDIM", "Pack islands to closest UDIM"},
      {PACK_UDIM_SRC_ACTIVE,
       "ACTIVE_UDIM",
       0,
       "Active UDIM",
       "Pack islands to active UDIM image tile or UDIM grid tile where 2D cursor is located"},
      {PACK_ORIGINAL_AABB,
       "ORIGINAL_AABB",
       0,
       "Original bounding box",
       "Pack to starting bounding box of islands"},
      {PACK_CUSTOM_REGION, "CUSTOM_REGION", 0, "Custom Region", "Pack islands to custom region"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  /* identifiers */
  ot->name = "Pack Islands";
  ot->idname = "UV_OT_pack_islands";
  ot->description =
      "Transform all islands so that they fill up the UV/UDIM space as much as possible";

#ifdef USE_INTERACTIVE_PACK
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
#else
  /* The operator will handle undo, so the job system can push() it after the job completes. */
  ot->flag = OPTYPE_REGISTER;
#endif

  /* API callbacks. */
  ot->exec = pack_islands_exec;

#ifdef USE_INTERACTIVE_PACK
  ot->invoke = WM_operator_props_popup_call;
#else
  ot->invoke = uv_pack_islands_invoke;
#endif
  ot->ui = uv_pack_islands_ui;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_enum(ot->srna, "udim_source", pack_target, PACK_UDIM_SRC_CLOSEST, "Pack to", "");
  RNA_def_boolean(ot->srna, "rotate", true, "Rotate", "Rotate islands to improve layout");
  RNA_def_enum(ot->srna,
               "rotate_method",
               pack_rotate_method_items,
               ED_UVPACK_ROTATION_ANY,
               "Rotation Method",
               "");
  RNA_def_boolean(ot->srna, "scale", true, "Scale", "Scale islands to fill unit square");
  RNA_def_boolean(
      ot->srna, "merge_overlap", false, "Merge Overlapping", "Overlapping islands stick together");
  RNA_def_enum(ot->srna,
               "margin_method",
               pack_margin_method_items,
               ED_UVPACK_MARGIN_SCALED,
               "Margin Method",
               "");
  RNA_def_float_factor(
      ot->srna, "margin", 0.001f, 0.0f, 1.0f, "Margin", "Space between islands", 0.0f, 1.0f);
  RNA_def_boolean(ot->srna,
                  "pin",
                  false,
                  "Lock Pinned Islands",
                  "Constrain islands containing any pinned UV's");
  RNA_def_enum(ot->srna,
               "pin_method",
               pinned_islands_method_items,
               ED_UVPACK_PIN_LOCK_ALL,
               "Pin Method",
               "");
  RNA_def_enum(ot->srna,
               "shape_method",
               pack_shape_method_items,
               ED_UVPACK_SHAPE_CONCAVE,
               "Shape Method",
               "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Average UV Islands Scale Operator
 * \{ */

static wmOperatorStatus average_islands_scale_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ToolSettings *ts = scene->toolsettings;
  const bool synced_selection = (ts->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  UnwrapOptions options = unwrap_options_get(nullptr, nullptr, ts);
  options.topology_from_uvs = true;
  options.only_selected_faces = true;
  options.only_selected_uvs = true;
  options.fill_holes = false;
  options.correct_aspect = true;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, CTX_wm_view3d(C));

  if (!uvedit_have_selection_multi(scene, objects, &options)) {
    return OPERATOR_CANCELLED;
  }

  /* RNA props */
  const bool scale_uv = RNA_boolean_get(op->ptr, "scale_uv");
  const bool shear = RNA_boolean_get(op->ptr, "shear");

  ParamHandle *handle = construct_param_handle_multi(scene, objects, &options);
  blender::geometry::uv_parametrizer_average(handle, false, scale_uv, shear);
  blender::geometry::uv_parametrizer_flush(handle);
  delete (handle);

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (synced_selection && (em->bm->totvertsel == 0)) {
      continue;
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }
  return OPERATOR_FINISHED;
}

void UV_OT_average_islands_scale(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Average Islands Scale";
  ot->idname = "UV_OT_average_islands_scale";
  ot->description = "Average the size of separate UV islands, based on their area in 3D space";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = average_islands_scale_exec;
  ot->poll = ED_operator_uvedit;

  /* properties */
  RNA_def_boolean(ot->srna, "scale_uv", false, "Non-Uniform", "Scale U and V independently");
  RNA_def_boolean(ot->srna, "shear", false, "Shear", "Reduce shear within islands");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Live UV Unwrap
 * \{ */

static struct {
  ParamHandle **handles;
  uint len, len_alloc;
  wmTimer *timer;
} g_live_unwrap = {nullptr};

bool ED_uvedit_live_unwrap_timer_check(const wmTimer *timer)
{
  /* NOTE: don't validate the timer, assume the timer passed in is valid. */
  return g_live_unwrap.timer == timer;
}

/**
 * In practice the timer should practically always be valid.
 * Use this to prevent the unlikely case of a stale timer being set.
 *
 * Loading a new file while unwrapping is running could cause this for example.
 */
static bool uvedit_live_unwrap_timer_validate(const wmWindowManager *wm)
{
  if (g_live_unwrap.timer == nullptr) {
    return false;
  }
  if (BLI_findindex(&wm->runtime->timers, g_live_unwrap.timer) != -1) {
    return false;
  }
  g_live_unwrap.timer = nullptr;
  return true;
}

void ED_uvedit_live_unwrap_begin(Scene *scene, Object *obedit, wmWindow *win_modal)
{
  ParamHandle *handle = nullptr;
  BMEditMesh *em = BKE_editmesh_from_object(obedit);

  if (!ED_uvedit_test(obedit)) {
    return;
  }

  UnwrapOptions options = unwrap_options_get(nullptr, obedit, scene->toolsettings);
  options.topology_from_uvs = false;
  options.only_selected_faces = false;
  options.only_selected_uvs = false;

  if (options.use_subsurf) {
    handle = construct_param_handle_subsurfed(scene, obedit, em, &options, nullptr);
  }
  else {
    handle = construct_param_handle(scene, obedit, em->bm, &options, nullptr);
  }

  if (options.use_slim) {
    options.slim.no_flip = false;
    options.slim.skip_init = true;
    uv_parametrizer_slim_live_begin(handle, &options.slim);

    if (win_modal) {
      wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
      /* Clear in the unlikely event this is still set. */
      uvedit_live_unwrap_timer_validate(wm);
      BLI_assert(!g_live_unwrap.timer);
      g_live_unwrap.timer = WM_event_timer_add(wm, win_modal, TIMER, 0.01f);
    }
  }
  else {
    blender::geometry::uv_parametrizer_lscm_begin(handle, true, options.use_abf);
  }

  /* Create or increase size of g_live_unwrap.handles array */
  if (g_live_unwrap.handles == nullptr) {
    g_live_unwrap.len_alloc = 32;
    g_live_unwrap.handles = MEM_malloc_arrayN<ParamHandle *>(g_live_unwrap.len_alloc,
                                                             "uvedit_live_unwrap_liveHandles");
    g_live_unwrap.len = 0;
  }
  if (g_live_unwrap.len >= g_live_unwrap.len_alloc) {
    g_live_unwrap.len_alloc *= 2;
    g_live_unwrap.handles = static_cast<ParamHandle **>(
        MEM_reallocN(g_live_unwrap.handles, sizeof(ParamHandle *) * g_live_unwrap.len_alloc));
  }
  g_live_unwrap.handles[g_live_unwrap.len] = handle;
  g_live_unwrap.len++;
}

void ED_uvedit_live_unwrap_re_solve()
{
  if (g_live_unwrap.handles) {
    for (int i = 0; i < g_live_unwrap.len; i++) {
      if (uv_parametrizer_is_slim(g_live_unwrap.handles[i])) {
        uv_parametrizer_slim_live_solve_iteration(g_live_unwrap.handles[i]);
      }
      else {
        blender::geometry::uv_parametrizer_lscm_solve(g_live_unwrap.handles[i], nullptr, nullptr);
      }

      blender::geometry::uv_parametrizer_flush(g_live_unwrap.handles[i]);
    }
  }
}

void ED_uvedit_live_unwrap_end(const bool cancel)
{
  if (g_live_unwrap.timer) {
    wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
    uvedit_live_unwrap_timer_validate(wm);
    if (g_live_unwrap.timer) {
      wmWindow *win = g_live_unwrap.timer->win;
      WM_event_timer_remove(wm, win, g_live_unwrap.timer);
      g_live_unwrap.timer = nullptr;
    }
  }

  if (g_live_unwrap.handles) {
    for (int i = 0; i < g_live_unwrap.len; i++) {
      if (uv_parametrizer_is_slim(g_live_unwrap.handles[i])) {
        uv_parametrizer_slim_live_end(g_live_unwrap.handles[i]);
      }
      else {
        blender::geometry::uv_parametrizer_lscm_end(g_live_unwrap.handles[i]);
      }

      if (cancel) {
        blender::geometry::uv_parametrizer_flush_restore(g_live_unwrap.handles[i]);
      }
      delete (g_live_unwrap.handles[i]);
    }
    MEM_freeN(g_live_unwrap.handles);
    g_live_unwrap.handles = nullptr;
    g_live_unwrap.len = 0;
    g_live_unwrap.len_alloc = 0;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Map Common Transforms
 * \{ */

#define VIEW_ON_EQUATOR 0
#define VIEW_ON_POLES 1
#define ALIGN_TO_OBJECT 2

#define POLAR_ZX 0
#define POLAR_ZY 1

enum {
  PINCH = 0,
  FAN = 1,
};

static void uv_map_transform_calc_bounds(BMEditMesh *em, float r_min[3], float r_max[3])
{
  BMFace *efa;
  BMIter iter;
  INIT_MINMAX(r_min, r_max);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      BM_face_calc_bounds_expand(efa, r_min, r_max);
    }
  }
}

static void uv_map_transform_calc_center_median(BMEditMesh *em, float r_center[3])
{
  BMFace *efa;
  BMIter iter;
  uint center_accum_num = 0;
  zero_v3(r_center);
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      float center[3];
      BM_face_calc_center_median(efa, center);
      add_v3_v3(r_center, center);
      center_accum_num += 1;
    }
  }
  mul_v3_fl(r_center, 1.0f / float(center_accum_num));
}

static void uv_map_transform_center(const Scene *scene,
                                    View3D *v3d,
                                    Object *ob,
                                    BMEditMesh *em,
                                    float r_center[3],
                                    float r_bounds[2][3])
{
  /* only operates on the edit object - this is all that's needed now */
  const int around = (v3d) ? scene->toolsettings->transform_pivot_point :
                             int(V3D_AROUND_CENTER_BOUNDS);

  float bounds[2][3];
  INIT_MINMAX(bounds[0], bounds[1]);
  bool is_minmax_set = false;

  switch (around) {
    case V3D_AROUND_CENTER_BOUNDS: /* bounding box center */
    {
      uv_map_transform_calc_bounds(em, bounds[0], bounds[1]);
      is_minmax_set = true;
      mid_v3_v3v3(r_center, bounds[0], bounds[1]);
      break;
    }
    case V3D_AROUND_CENTER_MEDIAN: {
      uv_map_transform_calc_center_median(em, r_center);
      break;
    }
    case V3D_AROUND_CURSOR: /* cursor center */
    {
      invert_m4_m4(ob->runtime->world_to_object.ptr(), ob->object_to_world().ptr());
      mul_v3_m4v3(r_center, ob->world_to_object().ptr(), scene->cursor.location);
      break;
    }
    case V3D_AROUND_ACTIVE: {
      BMEditSelection ese;
      if (BM_select_history_active_get(em->bm, &ese)) {
        BM_editselection_center(&ese, r_center);
        break;
      }
      ATTR_FALLTHROUGH;
    }
    case V3D_AROUND_LOCAL_ORIGINS: /* object center */
    default:
      zero_v3(r_center);
      break;
  }

  /* if this is passed, always set! */
  if (r_bounds) {
    if (!is_minmax_set) {
      uv_map_transform_calc_bounds(em, bounds[0], bounds[1]);
    }
    copy_v3_v3(r_bounds[0], bounds[0]);
    copy_v3_v3(r_bounds[1], bounds[1]);
  }
}

static void uv_map_rotation_matrix_ex(float result[4][4],
                                      RegionView3D *rv3d,
                                      Object *ob,
                                      float upangledeg,
                                      float sideangledeg,
                                      float radius,
                                      const float offset[4])
{
  float rotup[4][4], rotside[4][4], viewmatrix[4][4], rotobj[4][4];
  float sideangle = 0.0f, upangle = 0.0f;

  /* get rotation of the current view matrix */
  if (rv3d) {
    copy_m4_m4(viewmatrix, rv3d->viewmat);
  }
  else {
    unit_m4(viewmatrix);
  }

  /* but shifting */
  zero_v3(viewmatrix[3]);

  /* get rotation of the current object matrix */
  copy_m4_m4(rotobj, ob->object_to_world().ptr());
  zero_v3(rotobj[3]);

  /* but shifting */
  add_v4_v4(rotobj[3], offset);
  rotobj[3][3] = 0.0f;

  zero_m4(rotup);
  zero_m4(rotside);

  /* Compensate front/side.. against opengl x,y,z world definition.
   * This is "a sledgehammer to crack a nut" (overkill), a few plus minus 1 will do here.
   * I wanted to keep the reason here, so we're rotating. */
  sideangle = float(M_PI) * (sideangledeg + 180.0f) / 180.0f;
  rotside[0][0] = cosf(sideangle);
  rotside[0][1] = -sinf(sideangle);
  rotside[1][0] = sinf(sideangle);
  rotside[1][1] = cosf(sideangle);
  rotside[2][2] = 1.0f;

  upangle = float(M_PI) * upangledeg / 180.0f;
  rotup[1][1] = cosf(upangle) / radius;
  rotup[1][2] = -sinf(upangle) / radius;
  rotup[2][1] = sinf(upangle) / radius;
  rotup[2][2] = cosf(upangle) / radius;
  rotup[0][0] = 1.0f / radius;

  /* Calculate transforms. */
  mul_m4_series(result, rotup, rotside, viewmatrix, rotobj);
}

static void uv_map_transform(bContext *C, wmOperator *op, float rotmat[3][3])
{
  Object *obedit = CTX_data_edit_object(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  const int align = RNA_enum_get(op->ptr, "align");
  const int direction = RNA_enum_get(op->ptr, "direction");
  const float radius = RNA_struct_find_property(op->ptr, "radius") ?
                           RNA_float_get(op->ptr, "radius") :
                           1.0f;

  /* Be compatible to the "old" sphere/cylinder mode. */
  if (direction == ALIGN_TO_OBJECT) {
    unit_m3(rotmat);

    if (align == POLAR_ZY) {
      rotmat[0][0] = 0.0f;
      rotmat[0][1] = 1.0f;
      rotmat[1][0] = -1.0f;
      rotmat[1][1] = 0.0f;
    }
    return;
  }

  const float up_angle_deg = (direction == VIEW_ON_EQUATOR) ? 90.0f : 0.0f;
  const float side_angle_deg = (align == POLAR_ZY) == (direction == VIEW_ON_EQUATOR) ? 90.0f :
                                                                                       0.0f;
  const float offset[4] = {0};
  float rotmat4[4][4];
  uv_map_rotation_matrix_ex(rotmat4, rv3d, obedit, up_angle_deg, side_angle_deg, radius, offset);
  copy_m3_m4(rotmat, rotmat4);
}

static void uv_transform_properties(wmOperatorType *ot, int radius)
{
  static const EnumPropertyItem direction_items[] = {
      {VIEW_ON_EQUATOR, "VIEW_ON_EQUATOR", 0, "View on Equator", "3D view is on the equator"},
      {VIEW_ON_POLES, "VIEW_ON_POLES", 0, "View on Poles", "3D view is on the poles"},
      {ALIGN_TO_OBJECT,
       "ALIGN_TO_OBJECT",
       0,
       "Align to Object",
       "Align according to object transform"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem align_items[] = {
      {POLAR_ZX, "POLAR_ZX", 0, "Polar ZX", "Polar 0 is X"},
      {POLAR_ZY, "POLAR_ZY", 0, "Polar ZY", "Polar 0 is Y"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem pole_items[] = {
      {PINCH, "PINCH", 0, "Pinch", "UVs are pinched at the poles"},
      {FAN, "FAN", 0, "Fan", "UVs are fanned at the poles"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna,
               "direction",
               direction_items,
               VIEW_ON_EQUATOR,
               "Direction",
               "Direction of the sphere or cylinder");
  RNA_def_enum(ot->srna,
               "align",
               align_items,
               POLAR_ZX,
               "Align",
               "How to determine rotation around the pole");
  RNA_def_enum(ot->srna, "pole", pole_items, PINCH, "Pole", "How to handle faces at the poles");
  RNA_def_boolean(ot->srna,
                  "seam",
                  false,
                  "Preserve Seams",
                  "Separate projections by islands isolated by seams");

  if (radius) {
    RNA_def_float(ot->srna,
                  "radius",
                  1.0f,
                  0.0f,
                  FLT_MAX,
                  "Radius",
                  "Radius of the sphere or cylinder",
                  0.0001f,
                  100.0f);
  }
}

static void shrink_loop_uv_by_aspect_ratio(BMFace *efa,
                                           const int cd_loop_uv_offset,
                                           const float aspect_y)
{
  BLI_assert(aspect_y != 1.0f); /* Nothing to do, should be handled by caller. */
  BLI_assert(aspect_y > 0.0f);  /* Negative aspect ratios are not supported. */

  BMLoop *l;
  BMIter iter;
  BM_ITER_ELEM (l, &iter, efa, BM_LOOPS_OF_FACE) {
    float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    if (aspect_y > 1.0f) {
      /* Reduce round-off error, i.e. `u = (u - 0.5) / aspect_y + 0.5`. */
      luv[0] = luv[0] / aspect_y + (0.5f - 0.5f / aspect_y);
    }
    else {
      /* Reduce round-off error, i.e. `v = (v - 0.5) * aspect_y + 0.5`. */
      luv[1] = luv[1] * aspect_y + (0.5f - 0.5f * aspect_y);
    }
  }
}

static void correct_uv_aspect(Object *ob, BMEditMesh *em)
{
  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);
  const float aspect_y = ED_uvedit_get_aspect_y(ob);
  if (aspect_y == 1.0f) {
    /* Scaling by 1.0 has no effect. */
    return;
  }
  BMFace *efa;
  BMIter iter;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      shrink_loop_uv_by_aspect_ratio(efa, cd_loop_uv_offset, aspect_y);
    }
  }
}

static void correct_uv_aspect_per_face(Object *ob, BMEditMesh *em)
{
  const int materials_num = ob->totcol;
  if (materials_num == 0) {
    /* Without any materials, there is no aspect_y information and nothing to do. */
    return;
  }

  blender::Array<float, 16> material_aspect_y(materials_num, -1);
  /* Lazily initialize aspect ratio for materials. */

  const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);

  BMFace *efa;
  BMIter iter;
  BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
    if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }

    const int material_index = efa->mat_nr;
    if (UNLIKELY(material_index < 0 || material_index >= materials_num)) {
      /* The index might be for a material slot which is not currently setup. */
      continue;
    }

    float aspect_y = material_aspect_y[material_index];
    if (aspect_y == -1.0f) {
      /* Lazily initialize aspect ratio for materials. */
      float aspx, aspy;
      ED_uvedit_get_aspect_from_material(ob, material_index, &aspx, &aspy);
      aspect_y = aspx / aspy;
      material_aspect_y[material_index] = aspect_y;
    }

    if (aspect_y == 1.0f) {
      /* Scaling by 1.0 has no effect. */
      continue;
    }
    shrink_loop_uv_by_aspect_ratio(efa, cd_loop_uv_offset, aspect_y);
  }
}

#undef VIEW_ON_EQUATOR
#undef VIEW_ON_POLES
#undef ALIGN_TO_OBJECT

#undef POLAR_ZX
#undef POLAR_ZY

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Map Clip & Correct
 * \{ */

static void uv_map_clip_correct_properties_ex(wmOperatorType *ot, bool clip_to_bounds)
{
  uv_map_operator_property_correct_aspect(ot);
  /* Optional, since not all unwrapping types need to be clipped. */
  if (clip_to_bounds) {
    RNA_def_boolean(ot->srna,
                    "clip_to_bounds",
                    false,
                    "Clip to Bounds",
                    "Clip UV coordinates to bounds after unwrapping");
  }
  RNA_def_boolean(ot->srna,
                  "scale_to_bounds",
                  false,
                  "Scale to Bounds",
                  "Scale UV coordinates to bounds after unwrapping");
}

static void uv_map_clip_correct_properties(wmOperatorType *ot)
{
  uv_map_clip_correct_properties_ex(ot, true);
}

/**
 * \param per_face_aspect: Calculate the aspect ratio per-face,
 * otherwise use a single aspect for all UVs based on the material of the active face.
 * TODO: using per-face aspect may split UV islands so more advanced UV projection methods
 * such as "Unwrap" & "Smart UV Projections" will need to handle aspect correction themselves.
 * For now keep using a single aspect for all faces in this case.
 */
static void uv_map_clip_correct(const Scene *scene,
                                const Span<Object *> objects,
                                wmOperator *op,
                                bool per_face_aspect,
                                bool only_selected_uvs)
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float dx, dy, min[2], max[2];
  const bool correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect");
  const bool clip_to_bounds = (RNA_struct_find_property(op->ptr, "clip_to_bounds") &&
                               RNA_boolean_get(op->ptr, "clip_to_bounds"));
  const bool scale_to_bounds = RNA_boolean_get(op->ptr, "scale_to_bounds");

  INIT_MINMAX2(min, max);

  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);
    const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);

    /* Correct for image aspect ratio. */
    if (correct_aspect) {
      if (per_face_aspect) {
        correct_uv_aspect_per_face(ob, em);
      }
      else {
        correct_uv_aspect(ob, em);
      }
    }

    if (scale_to_bounds) {
      /* find uv limits */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        if (only_selected_uvs && !uvedit_face_select_test(scene, em->bm, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          minmax_v2v2_v2(min, max, luv);
        }
      }
    }
    else if (clip_to_bounds) {
      /* clipping and wrapping */
      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        if (only_selected_uvs && !uvedit_face_select_test(scene, em->bm, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          clamp_v2(luv, 0.0f, 1.0f);
        }
      }
    }
  }

  if (scale_to_bounds) {
    /* rescale UV to be in 1/1 */
    dx = (max[0] - min[0]);
    dy = (max[1] - min[1]);

    if (dx > 0.0f) {
      dx = 1.0f / dx;
    }
    if (dy > 0.0f) {
      dy = 1.0f / dy;
    }

    if (dx == 1.0f && dy == 1.0f && min[0] == 0.0f && min[1] == 0.0f) {
      /* Scaling by 1.0, without translating, has no effect. */
      return;
    }

    for (Object *ob : objects) {
      BMEditMesh *em = BKE_editmesh_from_object(ob);
      const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        if (only_selected_uvs && !uvedit_face_select_test(scene, em->bm, efa)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);

          luv[0] = (luv[0] - min[0]) * dx;
          luv[1] = (luv[1] - min[1]) * dy;
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Unwrap Operator
 * \{ */

/* Assumes UV Map exists, doesn't run update functions. */
static void uvedit_unwrap(const Scene *scene,
                          Object *obedit,
                          const UnwrapOptions *options,
                          int *r_count_changed,
                          int *r_count_failed)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  if (!CustomData_has_layer(&em->bm->ldata, CD_PROP_FLOAT2)) {
    return;
  }

  bool use_subsurf;
  modifier_unwrap_state(obedit, options, &use_subsurf);

  ParamHandle *handle;
  if (use_subsurf) {
    handle = construct_param_handle_subsurfed(scene, obedit, em, options, r_count_failed);
  }
  else {
    handle = construct_param_handle(scene, obedit, em->bm, options, r_count_failed);
  }

  if (options->use_slim) {
    uv_parametrizer_slim_solve(handle, &options->slim, r_count_changed, r_count_failed);
  }
  else {
    blender::geometry::uv_parametrizer_lscm_begin(handle, false, options->use_abf);
    blender::geometry::uv_parametrizer_lscm_solve(handle, r_count_changed, r_count_failed);
    blender::geometry::uv_parametrizer_lscm_end(handle);
  }

  blender::geometry::uv_parametrizer_average(handle, true, false, false);

  blender::geometry::uv_parametrizer_flush(handle);

  delete (handle);
}

static void uvedit_unwrap_multi(const Scene *scene,
                                const Span<Object *> objects,
                                const UnwrapOptions *options,
                                int *r_count_changed = nullptr,
                                int *r_count_failed = nullptr)
{
  for (Object *obedit : objects) {
    uvedit_unwrap(scene, obedit, options, r_count_changed, r_count_failed);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
  }
}

void ED_uvedit_live_unwrap(const Scene *scene, const Span<Object *> objects)
{
  if (scene->toolsettings->edge_mode_live_unwrap) {
    UnwrapOptions options = unwrap_options_get(nullptr, nullptr, scene->toolsettings);
    options.topology_from_uvs = false;
    options.only_selected_faces = false;
    options.only_selected_uvs = false;

    uvedit_unwrap_multi(scene, objects, &options, nullptr);

    blender::geometry::UVPackIsland_Params pack_island_params;
    pack_island_params.setFromUnwrapOptions(options);
    pack_island_params.rotate_method = ED_UVPACK_ROTATION_ANY;
    pack_island_params.pin_method = ED_UVPACK_PIN_IGNORE;
    pack_island_params.margin_method = ED_UVPACK_MARGIN_SCALED;
    pack_island_params.margin = scene->toolsettings->uvcalc_margin;

    uvedit_pack_islands_multi(
        scene, objects, nullptr, nullptr, false, true, nullptr, &pack_island_params);
  }
}

enum {
  UNWRAP_ERROR_NONUNIFORM = (1 << 0),
  UNWRAP_ERROR_NEGATIVE = (1 << 1),
};

static wmOperatorStatus unwrap_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Scene *scene = CTX_data_scene(C);
  const bool sync_selection = (scene->toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  int reported_errors = 0;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  unwrap_options_sync_toolsettings(op, scene->toolsettings);

  UnwrapOptions options = unwrap_options_get(op, nullptr, nullptr);
  options.topology_from_uvs = false;
  options.only_selected_faces = true;
  options.only_selected_uvs = false;

  /* We will report an error unless at least one object
   * has the subsurf modifier in the right place. */
  bool subsurf_error = options.use_subsurf;

  if (CTX_wm_space_image(C)) {
    /* Inside the UV Editor, only unwrap selected UVs. */
    if (sync_selection) {
      /* It's important to include unselected faces so they are taken into account
       * when unwrapping adjacent UV's, so it's possible to select part of an island
       * without it becoming disconnected from the unselected region, see: #148536. */
      options.only_selected_faces = false;
    }
    options.only_selected_uvs = true;
    options.pin_unselected = true;
  }

  if (!uvedit_have_selection_multi(scene, objects, &options)) {
    return OPERATOR_CANCELLED;
  }

  /* add uvs if they don't exist yet */
  for (Object *obedit : objects) {
    float obsize[3];
    bool use_subsurf_final;

    if (!uvedit_ensure_uvs(obedit)) {
      continue;
    }

    if (subsurf_error) {
      /* Double up the check here but better keep uvedit_unwrap interface simple and not
       * pass operator for warning append. */
      modifier_unwrap_state(obedit, &options, &use_subsurf_final);
      if (use_subsurf_final) {
        subsurf_error = false;
      }
    }

    if (reported_errors & (UNWRAP_ERROR_NONUNIFORM | UNWRAP_ERROR_NEGATIVE)) {
      continue;
    }

    mat4_to_size(obsize, obedit->object_to_world().ptr());
    if (!(fabsf(obsize[0] - obsize[1]) < 1e-4f && fabsf(obsize[1] - obsize[2]) < 1e-4f)) {
      if ((reported_errors & UNWRAP_ERROR_NONUNIFORM) == 0) {
        BKE_report(op->reports,
                   RPT_INFO,
                   "Object has non-uniform scale, unwrap will operate on a non-scaled version of "
                   "the mesh");
        reported_errors |= UNWRAP_ERROR_NONUNIFORM;
      }
    }
    else if (is_negative_m4(obedit->object_to_world().ptr())) {
      if ((reported_errors & UNWRAP_ERROR_NEGATIVE) == 0) {
        BKE_report(
            op->reports,
            RPT_INFO,
            "Object has negative scale, unwrap will operate on a non-flipped version of the mesh");
        reported_errors |= UNWRAP_ERROR_NEGATIVE;
      }
    }
  }

  if (subsurf_error) {
    BKE_report(op->reports,
               RPT_INFO,
               "Subdivision Surface modifier needs to be first to work with unwrap");
  }

  /* execute unwrap */
  int count_changed = 0;
  int count_failed = 0;
  uvedit_unwrap_multi(scene, objects, &options, &count_changed, &count_failed);

  blender::geometry::UVPackIsland_Params pack_island_params;
  pack_island_params.setFromUnwrapOptions(options);
  pack_island_params.rotate_method = ED_UVPACK_ROTATION_ANY;
  pack_island_params.pin_method = ED_UVPACK_PIN_IGNORE;
  pack_island_params.margin_method = eUVPackIsland_MarginMethod(
      RNA_enum_get(op->ptr, "margin_method"));
  pack_island_params.margin = RNA_float_get(op->ptr, "margin");

  uvedit_pack_islands_multi(
      scene, objects, nullptr, nullptr, false, true, nullptr, &pack_island_params);

  if (count_failed == 0 && count_changed == 0) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Unwrap could not solve any island(s), edge seams may need to be added");
  }
  else if (count_failed) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Unwrap failed to solve %d of %d island(s), edge seams may need to be added",
                count_failed,
                count_changed + count_failed);
  }

  return OPERATOR_FINISHED;
}

static void unwrap_draw(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  /* Main draw call */
  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, op->type->srna, op->properties);

  uiLayout *col;

  col = &layout->column(true);
  col->prop(&ptr, "method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  bool is_slim = RNA_enum_get(op->ptr, "method") == UVCALC_UNWRAP_METHOD_MINIMUM_STRETCH;

  if (is_slim) {
    col->prop(&ptr, "iterations", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&ptr, "no_flip", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col->separator();
    col->prop(&ptr, "use_weights", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    if (RNA_boolean_get(op->ptr, "use_weights")) {
      col = &layout->column(true);
      col->prop(&ptr, "weight_group", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col->prop(&ptr, "weight_factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
  else {
    col->prop(&ptr, "fill_holes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  col->separator();
  col->prop(&ptr, "use_subsurf_data", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col->separator();
  col->prop(&ptr, "correct_aspect", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&ptr, "margin_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&ptr, "margin", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

void UV_OT_unwrap(wmOperatorType *ot)
{
  const ToolSettings *tool_settings_default = DNA_struct_default_get(ToolSettings);

  static const EnumPropertyItem method_items[] = {
      {UVCALC_UNWRAP_METHOD_ANGLE, "ANGLE_BASED", 0, "Angle Based", ""},
      {UVCALC_UNWRAP_METHOD_CONFORMAL, "CONFORMAL", 0, "Conformal", ""},
      {UVCALC_UNWRAP_METHOD_MINIMUM_STRETCH, "MINIMUM_STRETCH", 0, "Minimum Stretch", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Unwrap";
  ot->description = "Unwrap the mesh of the object being edited";
  ot->idname = "UV_OT_unwrap";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = unwrap_exec;
  ot->poll = ED_operator_uvmap;

  /* Only draw relevant ui elements */
  ot->ui = unwrap_draw;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna,
      "method",
      method_items,
      tool_settings_default->unwrapper,
      "Method",
      "Unwrapping method (Angle Based usually gives better results than Conformal, while "
      "being somewhat slower)");
  RNA_def_boolean(ot->srna,
                  "fill_holes",
                  tool_settings_default->uvcalc_flag & UVCALC_FILLHOLES,
                  "Fill Holes",
                  "Virtually fill holes in mesh before unwrapping, to better avoid overlaps and "
                  "preserve symmetry");

  uv_map_operator_property_correct_aspect(ot);

  RNA_def_boolean(
      ot->srna,
      "use_subsurf_data",
      false,
      "Use Subdivision Surface",
      "Map UVs taking vertex position after Subdivision Surface modifier has been applied");
  RNA_def_enum(ot->srna,
               "margin_method",
               pack_margin_method_items,
               ED_UVPACK_MARGIN_SCALED,
               "Margin Method",
               "");
  RNA_def_float_factor(
      ot->srna, "margin", 0.001f, 0.0f, 1.0f, "Margin", "Space between islands", 0.0f, 1.0f);

  /* SLIM only */
  RNA_def_boolean(ot->srna,
                  "no_flip",
                  tool_settings_default->uvcalc_flag & UVCALC_UNWRAP_NO_FLIP,
                  "No Flip",
                  "Prevent flipping UV's, "
                  "flipping may lower distortion depending on the position of pins");

  RNA_def_int(ot->srna,
              "iterations",
              tool_settings_default->uvcalc_iterations,
              0,
              10000,
              "Iterations",
              "Number of iterations when \"Minimum Stretch\" method is used",
              1,
              30);

  RNA_def_boolean(ot->srna,
                  "use_weights",
                  tool_settings_default->uvcalc_flag & UVCALC_UNWRAP_USE_WEIGHTS,
                  "Importance Weights",
                  "Whether to take into account per-vertex importance weights");
  RNA_def_string(ot->srna,
                 "weight_group",
                 tool_settings_default->uvcalc_weight_group,
                 MAX_ID_NAME,
                 "Weight Group",
                 "Vertex group name for importance weights (modulating the deform)");
  RNA_def_float(
      ot->srna,
      "weight_factor",
      tool_settings_default->uvcalc_weight_factor,
      -10000.0,
      10000.0,
      "Weight Factor",
      "How much influence the weightmap has for weighted parameterization, 0 being no influence",
      -10.0,
      10.0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smart UV Project Operator
 * \{ */

/* Ignore all areas below this, as the UVs get zeroed. */
static const float smart_uv_project_area_ignore = 1e-12f;

struct ThickFace {
  float area;
  BMFace *efa;
};

static int smart_uv_project_thickface_area_cmp_fn(const void *tf_a_p, const void *tf_b_p)
{
  const ThickFace *tf_a = (ThickFace *)tf_a_p;
  const ThickFace *tf_b = (ThickFace *)tf_b_p;

  /* Ignore the area of small faces.
   * Also, order checks so `!isfinite(...)` values are counted as zero area. */
  if (!((tf_a->area > smart_uv_project_area_ignore) ||
        (tf_b->area > smart_uv_project_area_ignore)))
  {
    return 0;
  }

  if (tf_a->area < tf_b->area) {
    return 1;
  }
  if (tf_a->area > tf_b->area) {
    return -1;
  }
  return 0;
}

static blender::Vector<blender::float3> smart_uv_project_calculate_project_normals(
    const ThickFace *thick_faces,
    const uint thick_faces_len,
    BMesh *bm,
    const float project_angle_limit_half_cos,
    const float project_angle_limit_cos,
    const float area_weight)
{
  if (UNLIKELY(thick_faces_len == 0)) {
    return {};
  }

  const float *project_normal = thick_faces[0].efa->no;

  blender::Vector<const ThickFace *> project_thick_faces;
  blender::Vector<blender::float3> project_normal_array;

  BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

  while (true) {
    for (int f_index = thick_faces_len - 1; f_index >= 0; f_index--) {
      if (BM_elem_flag_test(thick_faces[f_index].efa, BM_ELEM_TAG)) {
        continue;
      }

      if (dot_v3v3(thick_faces[f_index].efa->no, project_normal) > project_angle_limit_half_cos) {
        project_thick_faces.append(&thick_faces[f_index]);
        BM_elem_flag_set(thick_faces[f_index].efa, BM_ELEM_TAG, true);
      }
    }

    float average_normal[3] = {0.0f, 0.0f, 0.0f};

    if (area_weight <= 0.0f) {
      for (int f_proj_index = 0; f_proj_index < project_thick_faces.size(); f_proj_index++) {
        const ThickFace *tf = project_thick_faces[f_proj_index];
        add_v3_v3(average_normal, tf->efa->no);
      }
    }
    else if (area_weight >= 1.0f) {
      for (int f_proj_index = 0; f_proj_index < project_thick_faces.size(); f_proj_index++) {
        const ThickFace *tf = project_thick_faces[f_proj_index];
        madd_v3_v3fl(average_normal, tf->efa->no, tf->area);
      }
    }
    else {
      for (int f_proj_index = 0; f_proj_index < project_thick_faces.size(); f_proj_index++) {
        const ThickFace *tf = project_thick_faces[f_proj_index];
        const float area_blend = (tf->area * area_weight) + (1.0f - area_weight);
        madd_v3_v3fl(average_normal, tf->efa->no, area_blend);
      }
    }

    /* Avoid NAN. */
    if (normalize_v3(average_normal) != 0.0f) {
      project_normal_array.append(average_normal);
    }

    /* Find the most unique angle that points away from other normals. */
    float anble_best = 1.0f;
    uint angle_best_index = 0;

    for (int f_index = thick_faces_len - 1; f_index >= 0; f_index--) {
      if (BM_elem_flag_test(thick_faces[f_index].efa, BM_ELEM_TAG)) {
        continue;
      }

      float angle_test = -1.0f;
      for (int p_index = 0; p_index < project_normal_array.size(); p_index++) {
        angle_test = max_ff(angle_test,
                            dot_v3v3(project_normal_array[p_index], thick_faces[f_index].efa->no));
      }

      if (angle_test < anble_best) {
        anble_best = angle_test;
        angle_best_index = f_index;
      }
    }

    if (anble_best < project_angle_limit_cos) {
      project_normal = thick_faces[angle_best_index].efa->no;
      project_thick_faces.clear();
      project_thick_faces.append(&thick_faces[angle_best_index]);
      BM_elem_flag_enable(thick_faces[angle_best_index].efa, BM_ELEM_TAG);
    }
    else {
      if (project_normal_array.size() >= 1) {
        break;
      }
    }
  }

  BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

  return project_normal_array;
}

static wmOperatorStatus smart_project_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* May be nullptr. */
  View3D *v3d = CTX_wm_view3d(C);

  bool only_selected_uvs = false;
  if (CTX_wm_space_image(C)) {
    /* Inside the UV Editor, only project selected UVs. */
    only_selected_uvs = true;
  }

  const float project_angle_limit = RNA_float_get(op->ptr, "angle_limit");
  const float island_margin = RNA_float_get(op->ptr, "island_margin");
  const float area_weight = RNA_float_get(op->ptr, "area_weight");

  const float project_angle_limit_cos = cosf(project_angle_limit);
  const float project_angle_limit_half_cos = cosf(project_angle_limit / 2);

  /* Memory arena for list links (cleared for each object). */
  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, v3d);

  Vector<Object *> objects_changed;

  BMFace *efa;
  BMIter iter;

  for (const int ob_index : objects.index_range()) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool changed = false;

    if (!uvedit_ensure_uvs(obedit)) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
    BLI_assert(offsets.uv >= 0);
    ThickFace *thick_faces = MEM_malloc_arrayN<ThickFace>(em->bm->totface, __func__);

    uint thick_faces_len = 0;
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        continue;
      }

      if (only_selected_uvs) {
        if (!uvedit_face_select_test(scene, em->bm, efa)) {
          uvedit_face_select_disable(scene, em->bm, efa);
          continue;
        }
      }

      thick_faces[thick_faces_len].area = BM_face_calc_area(efa);
      thick_faces[thick_faces_len].efa = efa;
      thick_faces_len++;
    }

    qsort(thick_faces, thick_faces_len, sizeof(ThickFace), smart_uv_project_thickface_area_cmp_fn);

    /* Remove all zero area faces. */
    while ((thick_faces_len > 0) &&
           !(thick_faces[thick_faces_len - 1].area > smart_uv_project_area_ignore))
    {

      /* Zero UVs so they don't overlap with other faces being unwrapped. */
      BMIter liter;
      BMLoop *l;
      BM_ITER_ELEM (l, &liter, thick_faces[thick_faces_len - 1].efa, BM_LOOPS_OF_FACE) {
        float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
        zero_v2(luv);
        changed = true;
      }

      thick_faces_len -= 1;
    }

    blender::Vector<blender::float3> project_normal_array =
        smart_uv_project_calculate_project_normals(thick_faces,
                                                   thick_faces_len,
                                                   em->bm,
                                                   project_angle_limit_half_cos,
                                                   project_angle_limit_cos,
                                                   area_weight);

    if (project_normal_array.is_empty()) {
      MEM_freeN(thick_faces);
      continue;
    }

    /* After finding projection vectors, we find the uv positions. */
    LinkNode **thickface_project_groups = static_cast<LinkNode **>(
        MEM_callocN(sizeof(*thickface_project_groups) * project_normal_array.size(), __func__));

    BLI_memarena_clear(arena);

    for (int f_index = thick_faces_len - 1; f_index >= 0; f_index--) {
      const float *f_normal = thick_faces[f_index].efa->no;

      float angle_best = dot_v3v3(f_normal, project_normal_array[0]);
      uint angle_best_index = 0;

      for (int p_index = 1; p_index < project_normal_array.size(); p_index++) {
        const float angle_test = dot_v3v3(f_normal, project_normal_array[p_index]);
        if (angle_test > angle_best) {
          angle_best = angle_test;
          angle_best_index = p_index;
        }
      }

      BLI_linklist_prepend_arena(
          &thickface_project_groups[angle_best_index], &thick_faces[f_index], arena);
    }

    for (int p_index = 0; p_index < project_normal_array.size(); p_index++) {
      if (thickface_project_groups[p_index] == nullptr) {
        continue;
      }

      float axis_mat[3][3];
      axis_dominant_v3_to_m3(axis_mat, project_normal_array[p_index]);

      for (LinkNode *list = thickface_project_groups[p_index]; list; list = list->next) {
        ThickFace *tf = static_cast<ThickFace *>(list->link);
        BMIter liter;
        BMLoop *l;
        BM_ITER_ELEM (l, &liter, tf->efa, BM_LOOPS_OF_FACE) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          mul_v2_m3v3(luv, axis_mat, l->v->co);
        }
        changed = true;
      }
    }

    MEM_freeN(thick_faces);

    /* No need to free the lists in 'thickface_project_groups' values as the 'arena' is used. */
    MEM_freeN(thickface_project_groups);

    if (changed) {
      objects_changed.append(obedit);
    }
  }

  BLI_memarena_free(arena);

  /* Pack islands & Stretch to UV bounds */
  if (!objects_changed.is_empty()) {

    scene->toolsettings->uvcalc_margin = island_margin;

    /* Depsgraph refresh functions are called here. */
    const bool correct_aspect = RNA_boolean_get(op->ptr, "correct_aspect");

    blender::geometry::UVPackIsland_Params params;
    params.rotate_method = eUVPackIsland_RotationMethod(RNA_enum_get(op->ptr, "rotate_method"));
    params.only_selected_uvs = only_selected_uvs;
    params.only_selected_faces = true;
    params.correct_aspect = correct_aspect;
    params.use_seams = true;
    params.margin_method = eUVPackIsland_MarginMethod(RNA_enum_get(op->ptr, "margin_method"));
    params.margin = RNA_float_get(op->ptr, "island_margin");

    uvedit_pack_islands_multi(
        scene, objects_changed, nullptr, nullptr, false, true, nullptr, &params);

    /* #uvedit_pack_islands_multi only supports `per_face_aspect = false`. */
    const bool per_face_aspect = false;
    uv_map_clip_correct(scene, objects_changed, op, per_face_aspect, only_selected_uvs);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus smart_project_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return WM_operator_props_popup_confirm_ex(
      C, op, event, IFACE_("Smart UV Project"), IFACE_("Unwrap"));
}

void UV_OT_smart_project(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Smart UV Project";
  ot->idname = "UV_OT_smart_project";
  ot->description = "Projection unwraps the selected faces of mesh objects";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = smart_project_exec;
  ot->poll = ED_operator_uvmap;
  ot->invoke = smart_project_invoke;

  /* properties */
  prop = RNA_def_float_rotation(ot->srna,
                                "angle_limit",
                                0,
                                nullptr,
                                DEG2RADF(0.0f),
                                DEG2RADF(90.0f),
                                "Angle Limit",
                                "Lower for more projection groups, higher for less distortion",
                                DEG2RADF(0.0f),
                                DEG2RADF(89.0f));
  RNA_def_property_float_default(prop, DEG2RADF(66.0f));

  RNA_def_enum(ot->srna,
               "margin_method",
               pack_margin_method_items,
               ED_UVPACK_MARGIN_SCALED,
               "Margin Method",
               "");
  RNA_def_enum(ot->srna,
               "rotate_method",
               /* Only show aligned options as the rotation from a projection
                * generated from a direction vector isn't meaningful. */
               pack_rotate_method_items + PACK_ROTATE_METHOD_AXIS_ALIGNED_OFFSET,
               ED_UVPACK_ROTATION_AXIS_ALIGNED_Y,
               "Rotation Method",
               "");
  RNA_def_float(ot->srna,
                "island_margin",
                0.0f,
                0.0f,
                1.0f,
                "Island Margin",
                "Margin to reduce bleed from adjacent islands",
                0.0f,
                1.0f);
  RNA_def_float(ot->srna,
                "area_weight",
                0.0f,
                0.0f,
                1.0f,
                "Area Weight",
                "Weight projection's vector by faces with larger areas",
                0.0f,
                1.0f);

  uv_map_clip_correct_properties_ex(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Project UV From View Operator
 * \{ */

static wmOperatorStatus uv_from_view_exec(bContext *C, wmOperator *op);

static wmOperatorStatus uv_from_view_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const Camera *camera = ED_view3d_camera_data_get(v3d, rv3d);
  PropertyRNA *prop;

  prop = RNA_struct_find_property(op->ptr, "camera_bounds");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_boolean_set(op->ptr, prop, (camera != nullptr));
  }
  prop = RNA_struct_find_property(op->ptr, "correct_aspect");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_boolean_set(op->ptr, prop, (camera == nullptr));
  }

  return uv_from_view_exec(C, op);
}

static wmOperatorStatus uv_from_view_exec(bContext *C, wmOperator *op)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  const Camera *camera = ED_view3d_camera_data_get(v3d, rv3d);
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float rotmat[4][4];
  float objects_pos_offset[4];

  const bool use_orthographic = RNA_boolean_get(op->ptr, "orthographic");

  /* NOTE: objects that aren't touched are set to nullptr (to skip clipping). */
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, v3d);

  if (use_orthographic) {
    /* Calculate average object position. */
    float objects_pos_avg[4] = {0};

    for (Object *object : objects) {
      add_v4_v4(objects_pos_avg, object->object_to_world().location());
    }

    mul_v4_fl(objects_pos_avg, 1.0f / objects.size());
    negate_v4_v4(objects_pos_offset, objects_pos_avg);
  }

  Vector<Object *> changed_objects;

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool changed = false;

    /* add uvs if they don't exist yet */
    if (!uvedit_ensure_uvs(obedit)) {
      continue;
    }

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);

    if (use_orthographic) {
      uv_map_rotation_matrix_ex(rotmat, rv3d, obedit, 90.0f, 0.0f, 1.0f, objects_pos_offset);

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
          BKE_uvproject_from_view_ortho(luv, l->v->co, rotmat);
        }
        changed = true;
      }
    }
    else if (camera) {
      const bool camera_bounds = RNA_boolean_get(op->ptr, "camera_bounds");
      ProjCameraInfo *uci = BKE_uvproject_camera_info(
          v3d->camera,
          obedit->object_to_world().ptr(),
          camera_bounds ? (scene->r.xsch * scene->r.xasp) : 1.0f,
          camera_bounds ? (scene->r.ysch * scene->r.yasp) : 1.0f);

      if (uci) {
        BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
          if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
            continue;
          }

          BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
            float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
            BKE_uvproject_from_camera(luv, l->v->co, uci);
          }
          changed = true;
        }

        BKE_uvproject_camera_info_free(uci);
      }
    }
    else {
      copy_m4_m4(rotmat, obedit->object_to_world().ptr());

      BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
        if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
          continue;
        }

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
          BKE_uvproject_from_view(
              luv, l->v->co, rv3d->persmat, rotmat, region->winx, region->winy);
        }
        changed = true;
      }
    }

    if (changed) {
      changed_objects.append(obedit);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
    }
  }

  if (changed_objects.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  const bool per_face_aspect = true;
  const bool only_selected_uvs = false;
  uv_map_clip_correct(scene, objects, op, per_face_aspect, only_selected_uvs);
  return OPERATOR_FINISHED;
}

static bool uv_from_view_poll(bContext *C)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  if (!ED_operator_uvmap(C)) {
    return false;
  }

  return (rv3d != nullptr);
}

void UV_OT_project_from_view(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Project from View";
  ot->idname = "UV_OT_project_from_view";
  ot->description = "Project the UV vertices of the mesh as seen in current 3D view";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->invoke = uv_from_view_invoke;
  ot->exec = uv_from_view_exec;
  ot->poll = uv_from_view_poll;

  /* properties */
  RNA_def_boolean(ot->srna, "orthographic", false, "Orthographic", "Use orthographic projection");
  RNA_def_boolean(ot->srna,
                  "camera_bounds",
                  true,
                  "Camera Bounds",
                  "Map UVs to the camera region taking resolution and aspect into account");
  uv_map_clip_correct_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reset UV Operator
 * \{ */

static wmOperatorStatus reset_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, v3d);
  for (Object *obedit : objects) {
    Mesh *mesh = (Mesh *)obedit->data;
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totfacesel == 0) {
      continue;
    }

    /* add uvs if they don't exist yet */
    if (!uvedit_ensure_uvs(obedit)) {
      continue;
    }

    ED_mesh_uv_loop_reset(C, mesh);

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void UV_OT_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset";
  ot->idname = "UV_OT_reset";
  ot->description = "Reset UV projection";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = reset_exec;
  ot->poll = ED_operator_uvmap;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sphere UV Project Operator
 * \{ */

static void uv_map_mirror(BMFace *efa,
                          const bool *regular,
                          const bool fan,
                          const int cd_loop_uv_offset)
{
  /* A heuristic to improve alignment of faces near the seam.
   * In simple terms, we're looking for faces which span more
   * than 0.5 units in the *u* coordinate.
   * If we find such a face, we try and improve the unwrapping
   * by adding (1.0, 0.0) onto some of the face's UVs.
   *
   * Note that this is only a heuristic. The property we're
   * attempting to maintain is that the winding of the face
   * in UV space corresponds with the handedness of the face
   * in 3D space w.r.t to the unwrapping. Even for triangles,
   * that property is somewhat complicated to evaluate. */

  float right_u = -1.0e30f;
  BMLoop *l;
  BMIter liter;
  blender::Array<float *, BM_DEFAULT_NGON_STACK_SIZE> uvs(efa->len);
  int j;
  BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, j) {
    float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    uvs[j] = luv;
    if (luv[0] >= 1.0f) {
      luv[0] -= 1.0f;
    }
    right_u = max_ff(right_u, luv[0]);
  }

  float left_u = 1.0e30f;
  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    if (right_u <= luv[0] + 0.5f) {
      left_u = min_ff(left_u, luv[0]);
    }
  }

  BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
    float *luv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);
    if (luv[0] + 0.5f < right_u) {
      if (2 * luv[0] + 1.0f < left_u + right_u) {
        luv[0] += 1.0f;
      }
    }
  }
  if (!fan) {
    return;
  }

  /* Another heuristic, this time, we attempt to "fan"
   * the UVs of faces which pass through one of the poles
   * of the unwrapping. */

  /* Need to recompute min and max. */
  float minmax_u[2] = {1.0e30f, -1.0e30f};
  int pole_count = 0;
  for (int i = 0; i < efa->len; i++) {
    if (regular[i]) {
      minmax_u[0] = min_ff(minmax_u[0], uvs[i][0]);
      minmax_u[1] = max_ff(minmax_u[1], uvs[i][0]);
    }
    else {
      pole_count++;
    }
  }
  if (ELEM(pole_count, 0, efa->len)) {
    return;
  }
  for (int i = 0; i < efa->len; i++) {
    if (regular[i]) {
      continue;
    }
    float u = 0.0f;
    float sum = 0.0f;
    const int i_plus = (i + 1) % efa->len;
    const int i_minus = (i + efa->len - 1) % efa->len;
    if (regular[i_plus]) {
      u += uvs[i_plus][0];
      sum += 1.0f;
    }
    if (regular[i_minus]) {
      u += uvs[i_minus][0];
      sum += 1.0f;
    }
    if (sum == 0) {
      u += minmax_u[0] + minmax_u[1];
      sum += 2.0f;
    }
    uvs[i][0] = u / sum;
  }
}

/**
 * Store a face and it's current branch on the generalized atan2 function.
 *
 * In complex analysis, we can generalize the `arctangent` function
 * into a multi-valued function that is "almost everywhere continuous"
 * in the complex plane.
 *
 * The downside is that we need to keep track of which "branch" of the
 * multi-valued function we are currently on.
 *
 * \note Even though `atan2(a+bi, c+di)` is now (multiply) defined for all
 * complex inputs, we will only evaluate it with `b==0` and `d==0`.
 */
struct UV_FaceBranch {
  BMFace *efa;
  float branch;
};

/**
 * Compute the sphere projection for a BMFace using #map_to_sphere and store on BMLoops.
 *
 * Heuristics are used in #uv_map_mirror to improve winding.
 *
 * if `fan` is true, faces with UVs at the pole have corrections applied to fan the UVs.
 *
 * if `use_seams` is true, the unwrapping will flood fill across the mesh, using
 * seams to mark boundaries, and #BM_ELEM_TAG to prevent revisiting faces.
 */
static float uv_sphere_project(const Scene *scene,
                               BMesh *bm,
                               BMFace *efa_init,
                               const float center[3],
                               const float rotmat[3][3],
                               const bool fan,
                               const BMUVOffsets &offsets,
                               const bool only_selected_uvs,
                               const bool use_seams,
                               const float branch_init)
{
  float max_u = 0.0f;
  if (use_seams && BM_elem_flag_test(efa_init, BM_ELEM_TAG)) {
    return max_u;
  }

  /* Similar to #BM_mesh_calc_face_groups with added connectivity information. */
  blender::Vector<UV_FaceBranch> stack;
  stack.append({efa_init, branch_init});

  while (stack.size()) {
    UV_FaceBranch face_branch = stack.pop_last();
    BMFace *efa = face_branch.efa;

    if (use_seams) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        continue; /* Faces might be in the stack more than once. */
      }

      BM_elem_flag_set(efa, BM_ELEM_TAG, true); /* Visited, don't consider again. */
    }

    if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue; /* Unselected face, ignore. */
    }

    if (only_selected_uvs) {
      if (!uvedit_face_select_test(scene, bm, efa)) {
        uvedit_face_select_disable(scene, bm, efa);
        continue; /* Unselected UV, ignore. */
      }
    }

    /* Remember which UVs are at the pole. */
    blender::Array<bool, BM_DEFAULT_NGON_STACK_SIZE> regular(efa->len);

    int i;
    BMLoop *l;
    BMIter iter;
    BM_ITER_ELEM_INDEX (l, &iter, efa, BM_LOOPS_OF_FACE, i) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      float pv[3];
      sub_v3_v3v3(pv, l->v->co, center);
      mul_m3_v3(rotmat, pv);
      regular[i] = map_to_sphere(&luv[0], &luv[1], pv[0], pv[1], pv[2]);
      if (!use_seams) {
        continue; /* Nothing more to do. */
      }

      /* Move UV to correct branch. */
      luv[0] = luv[0] + ceilf(face_branch.branch - 0.5f - luv[0]);
      max_u = max_ff(max_u, luv[0]);

      BMEdge *edge = l->e;
      if (BM_elem_flag_test(edge, BM_ELEM_SEAM)) {
        continue; /* Stop flood fill at seams. */
      }

      /* Extend flood fill by pushing to stack. */
      BMFace *efa2;
      BMIter iter2;
      BM_ITER_ELEM (efa2, &iter2, edge, BM_FACES_OF_EDGE) {
        if (!BM_elem_flag_test(efa2, BM_ELEM_TAG)) {
          stack.append({efa2, luv[0]});
        }
      }
    }
    uv_map_mirror(efa, regular.data(), fan, offsets.uv);
  }

  return max_u;
}

static wmOperatorStatus sphere_project_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  bool only_selected_uvs = false;
  if (CTX_wm_space_image(C)) {
    /* Inside the UV Editor, only project selected UVs. */
    only_selected_uvs = true;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, v3d);
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMIter iter;

    if (em->bm->totfacesel == 0) {
      continue;
    }

    /* add uvs if they don't exist yet */
    if (!uvedit_ensure_uvs(obedit)) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
    float center[3], rotmat[3][3];

    uv_map_transform(C, op, rotmat);
    uv_map_transform_center(scene, v3d, obedit, em, center, nullptr);

    const bool fan = RNA_enum_get(op->ptr, "pole");
    const bool use_seams = RNA_boolean_get(op->ptr, "seam");

    if (use_seams) {
      BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);
    }

    float island_offset = 0.0f;
    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      const float max_u = uv_sphere_project(scene,
                                            em->bm,
                                            efa,
                                            center,
                                            rotmat,
                                            fan,
                                            offsets,
                                            only_selected_uvs,
                                            use_seams,
                                            island_offset + 0.5f);
      island_offset = ceilf(max_ff(max_u, island_offset));
    }

    const bool per_face_aspect = true;
    uv_map_clip_correct(scene, {obedit}, op, per_face_aspect, only_selected_uvs);

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void UV_OT_sphere_project(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sphere Projection";
  ot->idname = "UV_OT_sphere_project";
  ot->description = "Project the UV vertices of the mesh over the curved surface of a sphere";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = sphere_project_exec;
  ot->poll = ED_operator_uvmap;

  /* properties */
  uv_transform_properties(ot, 0);
  uv_map_clip_correct_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cylinder UV Project Operator
 * \{ */

/* See #uv_sphere_project for description of parameters. */
static float uv_cylinder_project(const Scene *scene,
                                 BMesh *bm,
                                 BMFace *efa_init,
                                 const float center[3],
                                 const float rotmat[3][3],
                                 const bool fan,
                                 const BMUVOffsets &offsets,
                                 const bool only_selected_uvs,
                                 const bool use_seams,
                                 const float branch_init)
{
  float max_u = 0.0f;
  if (use_seams && BM_elem_flag_test(efa_init, BM_ELEM_TAG)) {
    return max_u;
  }

  /* Similar to BM_mesh_calc_face_groups with added connectivity information. */

  blender::Vector<UV_FaceBranch> stack;

  stack.append({efa_init, branch_init});

  while (stack.size()) {
    UV_FaceBranch face_branch = stack.pop_last();
    BMFace *efa = face_branch.efa;

    if (use_seams) {
      if (BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        continue; /* Faces might be in the stack more than once. */
      }

      BM_elem_flag_set(efa, BM_ELEM_TAG, true); /* Visited, don't consider again. */
    }

    if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue; /* Unselected face, ignore. */
    }

    if (only_selected_uvs) {
      if (!uvedit_face_select_test(scene, bm, efa)) {
        uvedit_face_select_disable(scene, bm, efa);
        continue; /* Unselected UV, ignore. */
      }
    }

    /* Remember which UVs are at the pole. */
    blender::Array<bool, BM_DEFAULT_NGON_STACK_SIZE> regular(efa->len);

    int i;
    BMLoop *l;
    BMIter iter;
    BM_ITER_ELEM_INDEX (l, &iter, efa, BM_LOOPS_OF_FACE, i) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      float pv[3];
      sub_v3_v3v3(pv, l->v->co, center);
      mul_m3_v3(rotmat, pv);
      regular[i] = map_to_tube(&luv[0], &luv[1], pv[0], pv[1], pv[2]);

      if (!use_seams) {
        continue; /* Nothing more to do. */
      }

      /* Move UV to correct branch. */
      luv[0] = luv[0] + ceilf(face_branch.branch - 0.5f - luv[0]);
      max_u = max_ff(max_u, luv[0]);

      BMEdge *edge = l->e;
      if (BM_elem_flag_test(edge, BM_ELEM_SEAM)) {
        continue; /* Stop flood fill at seams. */
      }

      /* Extend flood fill by pushing to stack. */
      BMFace *efa2;
      BMIter iter2;
      BM_ITER_ELEM (efa2, &iter2, edge, BM_FACES_OF_EDGE) {
        if (!BM_elem_flag_test(efa2, BM_ELEM_TAG)) {
          stack.append({efa2, luv[0]});
        }
      }
    }

    uv_map_mirror(efa, regular.data(), fan, offsets.uv);
  }

  return max_u;
}

static wmOperatorStatus cylinder_project_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  bool only_selected_uvs = false;
  if (CTX_wm_space_image(C)) {
    /* Inside the UV Editor, only project selected UVs. */
    only_selected_uvs = true;
  }

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, v3d);
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMFace *efa;
    BMIter iter;

    if (em->bm->totfacesel == 0) {
      continue;
    }

    /* add uvs if they don't exist yet */
    if (!uvedit_ensure_uvs(obedit)) {
      continue;
    }

    const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);
    float center[3], rotmat[3][3];

    uv_map_transform(C, op, rotmat);
    uv_map_transform_center(scene, v3d, obedit, em, center, nullptr);

    const bool fan = RNA_enum_get(op->ptr, "pole");
    const bool use_seams = RNA_boolean_get(op->ptr, "seam");

    if (use_seams) {
      BM_mesh_elem_hflag_disable_all(em->bm, BM_FACE, BM_ELEM_TAG, false);
    }

    float island_offset = 0.0f;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
        continue;
      }

      if (only_selected_uvs && !uvedit_face_select_test(scene, em->bm, efa)) {
        uvedit_face_select_disable(scene, em->bm, efa);
        continue;
      }

      const float max_u = uv_cylinder_project(scene,
                                              em->bm,
                                              efa,
                                              center,
                                              rotmat,
                                              fan,
                                              offsets,
                                              only_selected_uvs,
                                              use_seams,
                                              island_offset + 0.5f);
      island_offset = ceilf(max_ff(max_u, island_offset));
    }

    const bool per_face_aspect = true;
    uv_map_clip_correct(scene, {obedit}, op, per_face_aspect, only_selected_uvs);

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void UV_OT_cylinder_project(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cylinder Projection";
  ot->idname = "UV_OT_cylinder_project";
  ot->description = "Project the UV vertices of the mesh over the curved wall of a cylinder";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = cylinder_project_exec;
  ot->poll = ED_operator_uvmap;

  /* properties */
  uv_transform_properties(ot, 1);
  uv_map_clip_correct_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cube UV Project Operator
 * \{ */

static void uvedit_unwrap_cube_project(const Scene *scene,
                                       BMesh *bm,
                                       float cube_size,
                                       const bool use_select,
                                       const bool only_selected_uvs,
                                       const float center[3])
{
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  float loc[3];
  int cox, coy;

  const BMUVOffsets offsets = BM_uv_map_offsets_get(bm);

  if (center) {
    copy_v3_v3(loc, center);
  }
  else {
    zero_v3(loc);
  }

  if (UNLIKELY(cube_size == 0.0f)) {
    cube_size = 1.0f;
  }

  /* choose x,y,z axis for projection depending on the largest normal
   * component, but clusters all together around the center of map. */

  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    if (use_select && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }
    if (only_selected_uvs && !uvedit_face_select_test(scene, bm, efa)) {
      uvedit_face_select_disable(scene, bm, efa);
      continue;
    }

    axis_dominant_v3(&cox, &coy, efa->no);

    BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      luv[0] = 0.5f + ((l->v->co[cox] - loc[cox]) / cube_size);
      luv[1] = 0.5f + ((l->v->co[coy] - loc[coy]) / cube_size);
    }
  }
}

static wmOperatorStatus cube_project_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);

  bool only_selected_uvs = false;
  if (CTX_wm_space_image(C)) {
    /* Inside the UV Editor, only cube project selected UVs. */
    only_selected_uvs = true;
  }

  PropertyRNA *prop_cube_size = RNA_struct_find_property(op->ptr, "cube_size");
  const float cube_size_init = RNA_property_float_get(op->ptr, prop_cube_size);

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, v3d);
  for (const int ob_index : objects.index_range()) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totfacesel == 0) {
      continue;
    }

    /* add uvs if they don't exist yet */
    if (!uvedit_ensure_uvs(obedit)) {
      continue;
    }

    float bounds[2][3];
    float (*bounds_buf)[3] = nullptr;

    if (!RNA_property_is_set(op->ptr, prop_cube_size)) {
      bounds_buf = bounds;
    }

    float center[3];
    uv_map_transform_center(scene, v3d, obedit, em, center, bounds_buf);

    /* calculate based on bounds */
    float cube_size = cube_size_init;
    if (bounds_buf) {
      float dims[3];
      sub_v3_v3v3(dims, bounds[1], bounds[0]);
      cube_size = max_fff(UNPACK3(dims));
      if (ob_index == 0) {
        /* This doesn't fit well with, multiple objects. */
        RNA_property_float_set(op->ptr, prop_cube_size, cube_size);
      }
    }

    uvedit_unwrap_cube_project(scene, em->bm, cube_size, true, only_selected_uvs, center);

    const bool per_face_aspect = true;
    uv_map_clip_correct(scene, {obedit}, op, per_face_aspect, only_selected_uvs);

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void UV_OT_cube_project(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cube Projection";
  ot->idname = "UV_OT_cube_project";
  ot->description = "Project the UV vertices of the mesh over the six faces of a cube";

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = cube_project_exec;
  ot->poll = ED_operator_uvmap;

  /* properties */
  RNA_def_float(ot->srna,
                "cube_size",
                1.0f,
                0.0f,
                FLT_MAX,
                "Cube Size",
                "Size of the cube to project on",
                0.001f,
                100.0f);
  uv_map_clip_correct_properties(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simple UVs for Texture Painting
 * \{ */

void ED_uvedit_add_simple_uvs(Main *bmain, const Scene *scene, Object *ob)
{
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  bool sync_selection = (scene->toolsettings->uv_flag & UV_FLAG_SELECT_SYNC) != 0;

  BMeshCreateParams create_params{};
  create_params.use_toolflags = false;
  BMesh *bm = BM_mesh_create(&bm_mesh_allocsize_default, &create_params);

  /* turn sync selection off,
   * since we are not in edit mode we need to ensure only the uv flags are tested */
  scene->toolsettings->uv_flag &= ~UV_FLAG_SELECT_SYNC;

  ED_mesh_uv_ensure(mesh, nullptr);

  BMeshFromMeshParams bm_from_me_params{};
  bm_from_me_params.calc_face_normal = true;
  bm_from_me_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, mesh, &bm_from_me_params);

  /* Select all UVs for cube_project. */
  ED_uvedit_select_all(scene->toolsettings, bm);
  /* A cube size of 2.0 maps [-1..1] vertex coords to [0.0..1.0] in UV coords. */
  uvedit_unwrap_cube_project(scene, bm, 2.0, false, false, nullptr);

  /* Pack UVs. */
  blender::geometry::UVPackIsland_Params params;
  params.rotate_method = ED_UVPACK_ROTATION_ANY;
  params.only_selected_uvs = false;
  params.only_selected_faces = false;
  params.correct_aspect = false;
  params.use_seams = true;
  params.margin_method = ED_UVPACK_MARGIN_SCALED;
  params.margin = 0.001f;

  uvedit_pack_islands_multi(scene, {ob}, &bm, nullptr, false, true, nullptr, &params);

  /* Write back from BMesh to Mesh. */
  BMeshToMeshParams bm_to_me_params{};
  BM_mesh_bm_to_me(bmain, bm, mesh, &bm_to_me_params);
  BM_mesh_free(bm);

  if (sync_selection) {
    scene->toolsettings->uv_flag |= UV_FLAG_SELECT_SYNC;
  }
}

/** \} */
