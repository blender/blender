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
#include "DNA_defaults.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_colortools.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_light.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BLT_translation.h"

static void light_init_data(ID *id)
{
  Light *la = (Light *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(la, id));

  MEMCPY_STRUCT_AFTER(la, DNA_struct_default_get(Light), id);

  la->curfalloff = BKE_curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
  BKE_curvemapping_initialize(la->curfalloff);
}

/**
 * Only copy internal data of Light ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
 */
static void light_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Light *la_dst = (Light *)id_dst;
  const Light *la_src = (const Light *)id_src;
  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  la_dst->curfalloff = BKE_curvemapping_copy(la_src->curfalloff);

  if (la_src->nodetree) {
    BKE_id_copy_ex(bmain, (ID *)la_src->nodetree, (ID **)&la_dst->nodetree, flag_private_id_data);
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&la_dst->id, &la_src->id);
  }
  else {
    la_dst->preview = NULL;
  }
}

static void light_free_data(ID *id)
{
  Light *la = (Light *)id;

  BKE_curvemapping_free(la->curfalloff);

  /* is no lib link block, but light extension */
  if (la->nodetree) {
    ntreeFreeEmbeddedTree(la->nodetree);
    MEM_freeN(la->nodetree);
    la->nodetree = NULL;
  }

  BKE_previewimg_free(&la->preview);
  BKE_icon_id_delete(&la->id);
  la->id.icon_id = 0;
}

static void light_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Light *lamp = (Light *)id;
  if (lamp->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_library_foreach_ID_embedded(data, (ID **)&lamp->nodetree);
  }
}

IDTypeInfo IDType_ID_LA = {
    .id_code = ID_LA,
    .id_filter = FILTER_ID_LA,
    .main_listbase_index = INDEX_ID_LA,
    .struct_size = sizeof(Light),
    .name = "Light",
    .name_plural = "lights",
    .translation_context = BLT_I18NCONTEXT_ID_LIGHT,
    .flags = 0,

    .init_data = light_init_data,
    .copy_data = light_copy_data,
    .free_data = light_free_data,
    .make_local = NULL,
    .foreach_id = light_foreach_id,
};

Light *BKE_light_add(Main *bmain, const char *name)
{
  Light *la;

  la = BKE_libblock_alloc(bmain, ID_LA, name, 0);

  light_init_data(&la->id);

  return la;
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

  lan->curfalloff = BKE_curvemapping_copy(la->curfalloff);

  if (la->nodetree) {
    lan->nodetree = ntreeLocalize(la->nodetree);
  }

  lan->preview = NULL;

  lan->id.tag |= LIB_TAG_LOCALIZED;

  return lan;
}
