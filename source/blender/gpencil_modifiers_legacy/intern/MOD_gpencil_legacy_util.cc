/* SPDX-FileCopyrightText: 2017 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_deform.h"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_material.h"
#include "BKE_scene.h"

#include "MOD_gpencil_legacy_modifiertypes.h"
#include "MOD_gpencil_legacy_util.h"

#include "DEG_depsgraph_query.h"

void gpencil_modifier_type_init(GpencilModifierTypeInfo *types[])
{
#define INIT_GP_TYPE(typeName) \
  (types[eGpencilModifierType_##typeName] = &modifierType_Gpencil_##typeName)
  INIT_GP_TYPE(Noise);
  INIT_GP_TYPE(Subdiv);
  INIT_GP_TYPE(Simplify);
  INIT_GP_TYPE(Thick);
  INIT_GP_TYPE(Tint);
  INIT_GP_TYPE(Color);
  INIT_GP_TYPE(Array);
  INIT_GP_TYPE(Build);
  INIT_GP_TYPE(Opacity);
  INIT_GP_TYPE(Outline);
  INIT_GP_TYPE(Lattice);
  INIT_GP_TYPE(Length);
  INIT_GP_TYPE(Mirror);
  INIT_GP_TYPE(Smooth);
  INIT_GP_TYPE(Hook);
  INIT_GP_TYPE(Offset);
  INIT_GP_TYPE(Armature);
  INIT_GP_TYPE(Time);
  INIT_GP_TYPE(Multiply);
  INIT_GP_TYPE(Texture);
  INIT_GP_TYPE(WeightAngle);
  INIT_GP_TYPE(WeightProximity);
  INIT_GP_TYPE(Lineart);
  INIT_GP_TYPE(Dash);
  INIT_GP_TYPE(Shrinkwrap);
  INIT_GP_TYPE(Envelope);
#undef INIT_GP_TYPE
}

bool is_stroke_affected_by_modifier(Object *ob,
                                    char *mlayername,
                                    Material *material,
                                    const int mpassindex,
                                    const int gpl_passindex,
                                    const int minpoints,
                                    bGPDlayer *gpl,
                                    bGPDstroke *gps,
                                    const bool inv1,
                                    const bool inv2,
                                    const bool inv3,
                                    const bool inv4)
{
  Material *ma_gps = BKE_gpencil_material(ob, gps->mat_nr + 1);
  MaterialGPencilStyle *gp_style = ma_gps->gp_style;

  /* omit if filter by layer */
  if (mlayername[0] != '\0') {
    if (inv1 == false) {
      if (!STREQ(mlayername, gpl->info)) {
        return false;
      }
    }
    else {
      if (STREQ(mlayername, gpl->info)) {
        return false;
      }
    }
  }
  /* Omit if filter by material. */
  if (material != nullptr) {
    /* Requires to use the original material to compare the same pointer address. */
    Material *ma_md_orig = (Material *)DEG_get_original_id(&material->id);
    Material *ma_gps_orig = (Material *)DEG_get_original_id(&ma_gps->id);
    if (inv4 == false) {
      if (ma_md_orig != ma_gps_orig) {
        return false;
      }
    }
    else {
      if (ma_md_orig == ma_gps_orig) {
        return false;
      }
    }
  }
  /* verify layer pass */
  if (gpl_passindex > 0) {
    if (inv3 == false) {
      if (gpl->pass_index != gpl_passindex) {
        return false;
      }
    }
    else {
      if (gpl->pass_index == gpl_passindex) {
        return false;
      }
    }
  }
  /* verify material pass */
  if (mpassindex > 0) {
    if (inv2 == false) {
      if (gp_style->index != mpassindex) {
        return false;
      }
    }
    else {
      if (gp_style->index == mpassindex) {
        return false;
      }
    }
  }
  /* need to have a minimum number of points */
  if ((minpoints > 0) && (gps->totpoints < minpoints)) {
    return false;
  }

  return true;
}

float get_modifier_point_weight(const MDeformVert *dvert, bool inverse, int def_nr)
{
  float weight = 1.0f;

  if ((dvert != nullptr) && (def_nr != -1)) {
    MDeformWeight *dw = BKE_defvert_find_index(dvert, def_nr);
    weight = dw ? dw->weight : -1.0f;
    if ((weight >= 0.0f) && (inverse)) {
      return 1.0f - weight;
    }

    if ((weight < 0.0f) && (!inverse)) {
      return -1.0f;
    }

    /* if inverse, weight is always 1 */
    if ((weight < 0.0f) && (inverse)) {
      return 1.0f;
    }
  }

  /* handle special empty groups */
  if ((dvert == nullptr) && (def_nr != -1)) {
    if (inverse == 1) {
      return 1.0f;
    }

    return -1.0f;
  }

  return weight;
}

void generic_bake_deform_stroke(
    Depsgraph *depsgraph, GpencilModifierData *md, Object *ob, const bool retime, gpBakeCb bake_cb)
{
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  bGPdata *gpd = static_cast<bGPdata *>(ob->data);
  int oldframe = int(DEG_get_ctime(depsgraph));

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      if (retime) {
        scene->r.cfra = gpf->framenum;
        BKE_scene_graph_update_for_newframe(depsgraph);
      }
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        bake_cb(md, depsgraph, ob, gpl, gpf, gps);
      }
    }
  }

  /* Return frame state and DB to original state. */
  if (retime) {
    scene->r.cfra = oldframe;
    BKE_scene_graph_update_for_newframe(depsgraph);
  }
}
