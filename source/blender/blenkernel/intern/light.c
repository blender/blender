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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_colortools.h"
#include "BKE_icons.h"
#include "BKE_light.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"

void BKE_light_init(Light *la)
{
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(la, id));

  la->r = la->g = la->b = la->k = 1.0f;
  la->energy = 10.0f;
  la->dist = 25.0f;
  la->spotsize = DEG2RADF(45.0f);
  la->spotblend = 0.15f;
  la->att2 = 1.0f;
  la->mode = LA_SHADOW;
  la->bufsize = 512;
  la->clipsta = 0.05f;
  la->clipend = 40.0f;
  la->bleedexp = 2.5f;
  la->samp = 3;
  la->bias = 1.0f;
  la->soft = 3.0f;
  la->area_size = la->area_sizey = la->area_sizez = 0.25f;
  la->buffers = 1;
  la->preview = NULL;
  la->falloff_type = LA_FALLOFF_INVSQUARE;
  la->coeff_const = 1.0f;
  la->coeff_lin = 0.0f;
  la->coeff_quad = 0.0f;
  la->curfalloff = curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
  la->cascade_max_dist = 200.0f;
  la->cascade_count = 4;
  la->cascade_exponent = 0.8f;
  la->cascade_fade = 0.1f;
  la->contact_dist = 0.2f;
  la->contact_bias = 0.03f;
  la->contact_spread = 0.2f;
  la->contact_thickness = 0.2f;
  la->spec_fac = 1.0f;
  la->att_dist = 40.0f;
  la->sun_angle = DEG2RADF(0.526f);

  curvemapping_initialize(la->curfalloff);
}

Light *BKE_light_add(Main *bmain, const char *name)
{
  Light *la;

  la = BKE_libblock_alloc(bmain, ID_LA, name, 0);

  BKE_light_init(la);

  return la;
}

/**
 * Only copy internal data of Light ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_light_copy_data(Main *bmain, Light *la_dst, const Light *la_src, const int flag)
{
  la_dst->curfalloff = curvemapping_copy(la_src->curfalloff);

  if (la_src->nodetree) {
    /* Note: nodetree is *not* in bmain, however this specific case is handled at lower level
     *       (see BKE_libblock_copy_ex()). */
    BKE_id_copy_ex(bmain, (ID *)la_src->nodetree, (ID **)&la_dst->nodetree, flag);
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&la_dst->id, &la_src->id);
  }
  else {
    la_dst->preview = NULL;
  }
}

Light *BKE_light_copy(Main *bmain, const Light *la)
{
  Light *la_copy;
  BKE_id_copy(bmain, &la->id, (ID **)&la_copy);
  return la_copy;
}

Light *BKE_light_localize(Light *la)
{
  /* TODO(bastien): Replace with something like:
   *
   *   Light *la_copy;
   *   BKE_id_copy_ex(bmain, &la->id, (ID **)&la_copy,
   *                  LIB_ID_COPY_NO_MAIN | LIB_ID_COPY_NO_PREVIEW | LIB_ID_COPY_NO_USER_REFCOUNT,
   *                  false);
   *   return la_copy;
   *
   * NOTE: Only possible once nested node trees are fully converted to that too. */

  Light *lan = BKE_libblock_copy_for_localize(&la->id);

  lan->curfalloff = curvemapping_copy(la->curfalloff);

  if (la->nodetree) {
    lan->nodetree = ntreeLocalize(la->nodetree);
  }

  lan->preview = NULL;

  lan->id.tag |= LIB_TAG_LOCALIZED;

  return lan;
}

void BKE_light_make_local(Main *bmain, Light *la, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &la->id, true, lib_local);
}

void BKE_light_free(Light *la)
{
  BKE_animdata_free((ID *)la, false);

  curvemapping_free(la->curfalloff);

  /* is no lib link block, but light extension */
  if (la->nodetree) {
    ntreeFreeNestedTree(la->nodetree);
    MEM_freeN(la->nodetree);
    la->nodetree = NULL;
  }

  BKE_previewimg_free(&la->preview);
  BKE_icon_id_delete(&la->id);
  la->id.icon_id = 0;
}
