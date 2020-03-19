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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_world.h"

#include "BLT_translation.h"

#include "DRW_engine.h"

#include "DEG_depsgraph.h"

#include "GPU_material.h"

/** Free (or release) any data used by this world (does not free the world itself). */
static void world_free_data(ID *id)
{
  World *wrld = (World *)id;

  DRW_drawdata_free(id);

  /* is no lib link block, but world extension */
  if (wrld->nodetree) {
    ntreeFreeNestedTree(wrld->nodetree);
    MEM_freeN(wrld->nodetree);
    wrld->nodetree = NULL;
  }

  GPU_material_free(&wrld->gpumaterial);

  BKE_icon_id_delete((struct ID *)wrld);
  BKE_previewimg_free(&wrld->preview);
}

static void world_init_data(ID *id)
{
  World *wrld = (World *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wrld, id));

  MEMCPY_STRUCT_AFTER(wrld, DNA_struct_default_get(World), id);
}

World *BKE_world_add(Main *bmain, const char *name)
{
  World *wrld;

  wrld = BKE_libblock_alloc(bmain, ID_WO, name, 0);

  world_init_data(&wrld->id);

  return wrld;
}

/**
 * Only copy internal data of World ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more).
 */
static void world_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  World *wrld_dst = (World *)id_dst;
  const World *wrld_src = (const World *)id_src;
  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (wrld_src->nodetree) {
    BKE_id_copy_ex(
        bmain, (ID *)wrld_src->nodetree, (ID **)&wrld_dst->nodetree, flag_private_id_data);
  }

  BLI_listbase_clear(&wrld_dst->gpumaterial);
  BLI_listbase_clear((ListBase *)&wrld_dst->drawdata);

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&wrld_dst->id, &wrld_src->id);
  }
  else {
    wrld_dst->preview = NULL;
  }
}

World *BKE_world_copy(Main *bmain, const World *wrld)
{
  World *wrld_copy;
  BKE_id_copy(bmain, &wrld->id, (ID **)&wrld_copy);
  return wrld_copy;
}

World *BKE_world_localize(World *wrld)
{
  /* TODO(bastien): Replace with something like:
   *
   *   World *wrld_copy;
   *   BKE_id_copy_ex(bmain, &wrld->id, (ID **)&wrld_copy,
   *                  LIB_ID_COPY_NO_MAIN | LIB_ID_COPY_NO_PREVIEW | LIB_ID_COPY_NO_USER_REFCOUNT,
   *                  false);
   *   return wrld_copy;
   *
   * NOTE: Only possible once nested node trees are fully converted to that too. */

  World *wrldn;

  wrldn = BKE_libblock_copy_for_localize(&wrld->id);

  if (wrld->nodetree) {
    wrldn->nodetree = ntreeLocalize(wrld->nodetree);
  }

  wrldn->preview = NULL;

  BLI_listbase_clear(&wrldn->gpumaterial);
  BLI_listbase_clear((ListBase *)&wrldn->drawdata);

  wrldn->id.tag |= LIB_TAG_LOCALIZED;

  return wrldn;
}

static void world_make_local(Main *bmain, ID *id, const int flags)
{
  BKE_lib_id_make_local_generic(bmain, id, flags);
}

IDTypeInfo IDType_ID_WO = {
    .id_code = ID_WO,
    .id_filter = FILTER_ID_WO,
    .main_listbase_index = INDEX_ID_WO,
    .struct_size = sizeof(World),
    .name = "World",
    .name_plural = "worlds",
    .translation_context = BLT_I18NCONTEXT_ID_WORLD,
    .flags = 0,

    .init_data = world_init_data,
    .copy_data = world_copy_data,
    .free_data = world_free_data,
    .make_local = world_make_local,
};

void BKE_world_eval(struct Depsgraph *depsgraph, World *world)
{
  DEG_debug_print_eval(depsgraph, __func__, world->id.name, world);
  GPU_material_free(&world->gpumaterial);
}
