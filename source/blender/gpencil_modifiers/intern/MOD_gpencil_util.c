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

#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_colortools.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_object.h"

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
  INIT_GP_TYPE(Multiply);
  INIT_GP_TYPE(Texture);
#undef INIT_GP_TYPE
}

/* verify if valid layer, material and pass index */
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
  Material *ma = BKE_gpencil_material(ob, gps->mat_nr + 1);
  MaterialGPencilStyle *gp_style = ma->gp_style;

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
  if (material != NULL) {
    if (inv4 == false) {
      if (material != ma) {
        return false;
      }
    }
    else {
      if (material == ma) {
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
    MDeformWeight *dw = BKE_defvert_find_index(dvert, def_nr);
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

    return -1.0f;
  }

  return weight;
}
