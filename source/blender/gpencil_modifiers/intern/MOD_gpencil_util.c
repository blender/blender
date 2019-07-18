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
 *
 * The Original Code is Copyright (C) 2017, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup bke
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math_vector.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_gpencil_modifier_types.h"

#include "BKE_deform.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_colortools.h"

#include "DEG_depsgraph.h"

#include "MOD_gpencil_modifiertypes.h"
#include "MOD_gpencil_util.h"

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
  INIT_GP_TYPE(Lattice);
  INIT_GP_TYPE(Mirror);
  INIT_GP_TYPE(Smooth);
  INIT_GP_TYPE(Hook);
  INIT_GP_TYPE(Offset);
  INIT_GP_TYPE(Armature);
  INIT_GP_TYPE(Time);
#undef INIT_GP_TYPE
}

/* verify if valid layer and pass index */
bool is_stroke_affected_by_modifier(Object *ob,
                                    char *mlayername,
                                    int mpassindex,
                                    int gpl_passindex,
                                    int minpoints,
                                    bGPDlayer *gpl,
                                    bGPDstroke *gps,
                                    bool inv1,
                                    bool inv2,
                                    bool inv3)
{
  MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);

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

/* verify if valid vertex group *and return weight */
float get_modifier_point_weight(MDeformVert *dvert, bool inverse, int def_nr)
{
  float weight = 1.0f;

  if ((dvert != NULL) && (def_nr != -1)) {
    MDeformWeight *dw = defvert_find_index(dvert, def_nr);
    weight = dw ? dw->weight : -1.0f;
    if ((weight >= 0.0f) && (inverse == 1)) {
      return -1.0f;
    }

    if ((weight < 0.0f) && (inverse == 0)) {
      return -1.0f;
    }

    /* if inverse, weight is always 1 */
    if ((weight < 0.0f) && (inverse == 1)) {
      return 1.0f;
    }
  }

  /* handle special empty groups */
  if ((dvert == NULL) && (def_nr != -1)) {
    if (inverse == 1) {
      return 1.0f;
    }
    else {
      return -1.0f;
    }
  }

  return weight;
}

/* set material when apply modifiers (used in tint and color modifier) */
void gpencil_apply_modifier_material(
    Main *bmain, Object *ob, Material *mat, GHash *gh_color, bGPDstroke *gps, bool crt_material)
{
  MaterialGPencilStyle *gp_style = mat->gp_style;

  /* look for color */
  if (crt_material) {
    Material *newmat = BLI_ghash_lookup(gh_color, mat->id.name);
    if (newmat == NULL) {
      BKE_object_material_slot_add(bmain, ob);
      newmat = BKE_material_copy(bmain, mat);
      newmat->preview = NULL;

      assign_material(bmain, ob, newmat, ob->totcol, BKE_MAT_ASSIGN_USERPREF);

      copy_v4_v4(newmat->gp_style->stroke_rgba, gps->runtime.tmp_stroke_rgba);
      copy_v4_v4(newmat->gp_style->fill_rgba, gps->runtime.tmp_fill_rgba);

      BLI_ghash_insert(gh_color, mat->id.name, newmat);
      DEG_id_tag_update(&newmat->id, ID_RECALC_COPY_ON_WRITE);
    }
    /* Reassign color index. */
    gps->mat_nr = BKE_gpencil_object_material_get_index(ob, newmat);
  }
  else {
    /* reuse existing color (but update only first time) */
    if (BLI_ghash_lookup(gh_color, mat->id.name) == NULL) {
      copy_v4_v4(gp_style->stroke_rgba, gps->runtime.tmp_stroke_rgba);
      copy_v4_v4(gp_style->fill_rgba, gps->runtime.tmp_fill_rgba);
      BLI_ghash_insert(gh_color, mat->id.name, mat);
    }
    /* update previews (icon and thumbnail) */
    if (mat->preview != NULL) {
      mat->preview->flag[ICON_SIZE_ICON] |= PRV_CHANGED;
      mat->preview->flag[ICON_SIZE_PREVIEW] |= PRV_CHANGED;
    }
    DEG_id_tag_update(&mat->id, ID_RECALC_COPY_ON_WRITE);
  }
}
