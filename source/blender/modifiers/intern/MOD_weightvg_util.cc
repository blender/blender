/* SPDX-FileCopyrightText: 2011 by Bastien Montagne. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_math_color.h"
#include "BLI_rand.h"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_color_types.h" /* CurveMapping. */
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_colortools.hh" /* CurveMapping. */
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_modifier.hh"
#include "BKE_texture.h" /* Texture masking. */

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "DEG_depsgraph_query.hh"

#include "MEM_guardedalloc.h"
#include "MOD_ui_common.hh"
#include "MOD_util.hh"
#include "MOD_weightvg_util.hh"
#include "RE_texture.h" /* Texture masking. */

void weightvg_do_map(
    int num, float *new_w, short falloff_type, const bool do_invert, CurveMapping *cmap, RNG *rng)
{
  int i;

  /* Return immediately, if we have nothing to do! */
  /* Also security checks... */
  if (!do_invert && (((falloff_type == MOD_WVG_MAPPING_CURVE) && (cmap == nullptr)) ||
                     !ELEM(falloff_type,
                           MOD_WVG_MAPPING_CURVE,
                           MOD_WVG_MAPPING_SHARP,
                           MOD_WVG_MAPPING_SMOOTH,
                           MOD_WVG_MAPPING_ROOT,
                           MOD_WVG_MAPPING_SPHERE,
                           MOD_WVG_MAPPING_RANDOM,
                           MOD_WVG_MAPPING_STEP)))
  {
    return;
  }

  if (cmap && falloff_type == MOD_WVG_MAPPING_CURVE) {
    BKE_curvemapping_init(cmap);
  }

  /* Map each weight (vertex) to its new value, accordingly to the chosen mode. */
  for (i = 0; i < num; i++) {
    float fac = new_w[i];

    /* Code borrowed from the warp modifier. */
    /* Closely matches PROP_SMOOTH and similar. */
    switch (falloff_type) {
      case MOD_WVG_MAPPING_CURVE:
        fac = BKE_curvemapping_evaluateF(cmap, 0, fac);
        break;
      case MOD_WVG_MAPPING_SHARP:
        fac = fac * fac;
        break;
      case MOD_WVG_MAPPING_SMOOTH:
        fac = 3.0f * fac * fac - 2.0f * fac * fac * fac;
        break;
      case MOD_WVG_MAPPING_ROOT:
        fac = sqrtf(fac);
        break;
      case MOD_WVG_MAPPING_SPHERE:
        fac = sqrtf(2 * fac - fac * fac);
        break;
      case MOD_WVG_MAPPING_RANDOM:
        fac = BLI_rng_get_float(rng) * fac;
        break;
      case MOD_WVG_MAPPING_STEP:
        fac = (fac >= 0.5f) ? 1.0f : 0.0f;
        break;
      case MOD_WVG_MAPPING_NONE:
        BLI_assert(do_invert);
        break;
      default:
        BLI_assert_unreachable();
    }

    new_w[i] = do_invert ? 1.0f - fac : fac;
  }
}

void weightvg_do_mask(const ModifierEvalContext *ctx,
                      const int num,
                      const int *indices,
                      float *org_w,
                      const float *new_w,
                      Object *ob,
                      Mesh *mesh,
                      const float fact,
                      const char defgrp_name[MAX_VGROUP_NAME],
                      Scene * /*scene*/,
                      Tex *texture,
                      const int tex_use_channel,
                      const int tex_mapping,
                      Object *tex_map_object,
                      const char *text_map_bone,
                      const char *tex_uvlayer_name,
                      const bool invert_vgroup_mask)
{
  int ref_didx;
  int i;

  /* If influence factor is null, nothing to do! */
  if (fact == 0.0f) {
    return;
  }

  /* If we want to mask vgroup weights from a texture. */
  if (texture != nullptr) {
    /* The texture coordinates. */
    float (*tex_co)[3];
    /* See mapping note below... */
    MappingInfoModifierData t_map;
    const int verts_num = mesh->verts_num;

    /* Use new generic get_texture_coords, but do not modify our DNA struct for it...
     * XXX Why use a ModifierData stuff here ? Why not a simple, generic struct for parameters?
     *     What e.g. if a modifier wants to use several textures?
     *     Why use only v_co, and not mesh positions (or both)?
     */
    t_map.texture = texture;
    t_map.map_object = tex_map_object;
    STRNCPY(t_map.map_bone, text_map_bone);
    STRNCPY(t_map.uvlayer_name, tex_uvlayer_name);
    t_map.texmapping = tex_mapping;

    tex_co = MEM_calloc_arrayN<float[3]>(verts_num, __func__);
    MOD_get_texture_coords(&t_map, ctx, ob, mesh, nullptr, tex_co);

    MOD_init_texture(&t_map, ctx);

    /* For each weight (vertex), make the mix between org and new weights. */
    for (i = 0; i < num; i++) {
      int idx = indices ? indices[i] : i;
      TexResult texres;
      float hsv[3]; /* For HSV color space. */
      bool do_color_manage = tex_use_channel != MOD_WVG_MASK_TEX_USE_INT;

      BKE_texture_get_value(texture, tex_co[idx], &texres, do_color_manage);
      /* Get the good channel value... */
      switch (tex_use_channel) {
        case MOD_WVG_MASK_TEX_USE_INT:
          org_w[i] = (new_w[i] * texres.tin * fact) + (org_w[i] * (1.0f - (texres.tin * fact)));
          break;
        case MOD_WVG_MASK_TEX_USE_RED:
          org_w[i] = (new_w[i] * texres.trgba[0] * fact) +
                     (org_w[i] * (1.0f - (texres.trgba[0] * fact)));
          break;
        case MOD_WVG_MASK_TEX_USE_GREEN:
          org_w[i] = (new_w[i] * texres.trgba[1] * fact) +
                     (org_w[i] * (1.0f - (texres.trgba[1] * fact)));
          break;
        case MOD_WVG_MASK_TEX_USE_BLUE:
          org_w[i] = (new_w[i] * texres.trgba[2] * fact) +
                     (org_w[i] * (1.0f - (texres.trgba[2] * fact)));
          break;
        case MOD_WVG_MASK_TEX_USE_HUE:
          rgb_to_hsv_v(texres.trgba, hsv);
          org_w[i] = (new_w[i] * hsv[0] * fact) + (org_w[i] * (1.0f - (hsv[0] * fact)));
          break;
        case MOD_WVG_MASK_TEX_USE_SAT:
          rgb_to_hsv_v(texres.trgba, hsv);
          org_w[i] = (new_w[i] * hsv[1] * fact) + (org_w[i] * (1.0f - (hsv[1] * fact)));
          break;
        case MOD_WVG_MASK_TEX_USE_VAL:
          rgb_to_hsv_v(texres.trgba, hsv);
          org_w[i] = (new_w[i] * hsv[2] * fact) + (org_w[i] * (1.0f - (hsv[2] * fact)));
          break;
        case MOD_WVG_MASK_TEX_USE_ALPHA:
          org_w[i] = (new_w[i] * texres.trgba[3] * fact) +
                     (org_w[i] * (1.0f - (texres.trgba[3] * fact)));
          break;
        default:
          org_w[i] = (new_w[i] * texres.tin * fact) + (org_w[i] * (1.0f - (texres.tin * fact)));
          break;
      }
    }

    MEM_freeN(tex_co);
  }
  else if ((ref_didx = BKE_id_defgroup_name_index(&mesh->id, defgrp_name)) != -1) {
    /* Check whether we want to set vgroup weights from a constant weight factor or a vertex
     * group.
     */
    /* Get vgroup idx from its name. */

    /* Proceed only if vgroup is valid, else use constant factor. */
    /* Get actual deform-verts (ie vertex group data). */
    const MDeformVert *dvert = mesh->deform_verts().data();
    /* Proceed only if vgroup is valid, else assume factor = O. */
    if (dvert == nullptr) {
      return;
    }

    /* For each weight (vertex), make the mix between org and new weights. */
    for (i = 0; i < num; i++) {
      int idx = indices ? indices[i] : i;
      const float f = (invert_vgroup_mask ?
                           (1.0f - BKE_defvert_find_weight(&dvert[idx], ref_didx)) :
                           BKE_defvert_find_weight(&dvert[idx], ref_didx)) *
                      fact;
      org_w[i] = (new_w[i] * f) + (org_w[i] * (1.0f - f));
      /* If that vertex is not in ref vgroup, assume null factor, and hence do nothing! */
    }
  }
  else {
    /* Default "influence" behavior. */
    /* For each weight (vertex), make the mix between org and new weights. */
    const float ifact = 1.0f - fact;
    for (i = 0; i < num; i++) {
      org_w[i] = (new_w[i] * fact) + (org_w[i] * ifact);
    }
  }
}

void weightvg_update_vg(MDeformVert *dvert,
                        int defgrp_idx,
                        MDeformWeight **dws,
                        int num,
                        const int *indices,
                        const float *weights,
                        const bool do_add,
                        const float add_thresh,
                        const bool do_rem,
                        const float rem_thresh,
                        const bool do_normalize)
{
  int i;

  float min_w = weights[0];
  float norm_fac = 1.0f;
  if (do_normalize) {
    float max_w = weights[0];
    for (i = 1; i < num; i++) {
      const float w = weights[i];

      /* No need to clamp here, normalization will ensure we stay within [0.0, 1.0] range. */
      if (w < min_w) {
        min_w = w;
      }
      else if (w > max_w) {
        max_w = w;
      }
    }

    const float range = max_w - min_w;
    if (fabsf(range) > FLT_EPSILON) {
      norm_fac = 1.0f / range;
    }
    else {
      min_w = 0.0f;
    }
  }

  for (i = 0; i < num; i++) {
    float w = weights[i];
    MDeformVert *dv = &dvert[indices ? indices[i] : i];
    MDeformWeight *dw = dws ?
                            dws[i] :
                            ((defgrp_idx >= 0) ? BKE_defvert_find_index(dv, defgrp_idx) : nullptr);

    if (do_normalize) {
      w = (w - min_w) * norm_fac;
    }
    /* Never allow weights out of [0.0, 1.0] range. */
    CLAMP(w, 0.0f, 1.0f);

    /* If the vertex is in this vgroup, remove it if needed, or just update it. */
    if (dw != nullptr) {
      if (do_rem && w <= rem_thresh) {
        BKE_defvert_remove_group(dv, dw);
      }
      else {
        dw->weight = w;
      }
    }
    /* Else, add it if needed! */
    else if (do_add && w >= add_thresh) {
      BKE_defvert_add_index_notest(dv, defgrp_idx, w);
    }
  }
}

void weightvg_ui_common(const bContext *C, PointerRNA *ob_ptr, PointerRNA *ptr, uiLayout *layout)
{
  PointerRNA mask_texture_ptr = RNA_pointer_get(ptr, "mask_texture");
  bool has_mask_texture = !RNA_pointer_is_null(&mask_texture_ptr);
  bool has_mask_vertex_group = RNA_string_length(ptr, "mask_vertex_group") != 0;
  int mask_tex_mapping = RNA_enum_get(ptr, "mask_tex_mapping");

  layout->use_property_split_set(true);

  layout->prop(ptr, "mask_constant", UI_ITEM_R_SLIDER, IFACE_("Global Influence:"), ICON_NONE);

  if (!has_mask_texture) {
    modifier_vgroup_ui(
        layout, ptr, ob_ptr, "mask_vertex_group", "invert_mask_vertex_group", std::nullopt);
  }

  if (!has_mask_vertex_group) {
    uiTemplateID(layout,
                 C,
                 ptr,
                 "mask_texture",
                 "texture.new",
                 nullptr,
                 nullptr,
                 UI_TEMPLATE_ID_FILTER_ALL,
                 false,
                 IFACE_("Mask Texture"));

    if (has_mask_texture) {
      layout->prop(ptr, "mask_tex_use_channel", UI_ITEM_NONE, IFACE_("Channel"), ICON_NONE);
      layout->prop(ptr, "mask_tex_mapping", UI_ITEM_NONE, std::nullopt, ICON_NONE);

      if (mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
        layout->prop(ptr, "mask_tex_map_object", UI_ITEM_NONE, IFACE_("Object"), ICON_NONE);
      }
      else if (mask_tex_mapping == MOD_DISP_MAP_UV && RNA_enum_get(ob_ptr, "type") == OB_MESH) {
        PointerRNA obj_data_ptr = RNA_pointer_get(ob_ptr, "data");
        layout->prop_search(
            ptr, "mask_tex_uv_layer", &obj_data_ptr, "uv_layers", std::nullopt, ICON_NONE);
      }
    }
  }
}
