/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_world.h"

#include "BLT_translation.h"

#include "DRW_engine.h"

#include "DEG_depsgraph.h"

#include "GPU_material.h"

#include "BLO_read_write.h"

/** Free (or release) any data used by this world (does not free the world itself). */
static void world_free_data(ID *id)
{
  World *wrld = (World *)id;

  DRW_drawdata_free(id);

  /* is no lib link block, but world extension */
  if (wrld->nodetree) {
    ntreeFreeEmbeddedTree(wrld->nodetree);
    MEM_freeN(wrld->nodetree);
    wrld->nodetree = nullptr;
  }

  GPU_material_free(&wrld->gpumaterial);

  BKE_icon_id_delete((ID *)wrld);
  BKE_previewimg_free(&wrld->preview);

  MEM_SAFE_FREE(wrld->lightgroup);
}

static void world_init_data(ID *id)
{
  World *wrld = (World *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(wrld, id));

  MEMCPY_STRUCT_AFTER(wrld, DNA_struct_default_get(World), id);
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

  const bool is_localized = (flag & LIB_ID_CREATE_LOCAL) != 0;
  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (wrld_src->nodetree) {
    if (is_localized) {
      wrld_dst->nodetree = ntreeLocalize(wrld_src->nodetree);
    }
    else {
      BKE_id_copy_ex(
          bmain, (ID *)wrld_src->nodetree, (ID **)&wrld_dst->nodetree, flag_private_id_data);
    }
    wrld_dst->nodetree->owner_id = &wrld_dst->id;
  }

  BLI_listbase_clear(&wrld_dst->gpumaterial);
  BLI_listbase_clear((ListBase *)&wrld_dst->drawdata);

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&wrld_dst->id, &wrld_src->id);
  }
  else {
    wrld_dst->preview = nullptr;
  }

  if (wrld_src->lightgroup) {
    wrld_dst->lightgroup = (LightgroupMembership *)MEM_dupallocN(wrld_src->lightgroup);
  }
}

static void world_foreach_id(ID *id, LibraryForeachIDData *data)
{
  World *world = reinterpret_cast<World *>(id);
  const int flag = BKE_lib_query_foreachid_process_flags_get(data);

  if (world->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&world->nodetree));
  }

  if (flag & IDWALK_DO_DEPRECATED_POINTERS) {
    BKE_LIB_FOREACHID_PROCESS_ID_NOCHECK(data, world->ipo, IDWALK_CB_USER);
  }
}

static void world_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  World *wrld = (World *)id;

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  BLI_listbase_clear(&wrld->gpumaterial);

  /* write LibData */
  BLO_write_id_struct(writer, World, id_address, &wrld->id);
  BKE_id_blend_write(writer, &wrld->id);

  /* nodetree is integral part of world, no libdata */
  if (wrld->nodetree) {
    BLO_Write_IDBuffer *temp_embedded_id_buffer = BLO_write_allocate_id_buffer();
    BLO_write_init_id_buffer_from_id(
        temp_embedded_id_buffer, &wrld->nodetree->id, BLO_write_is_undo(writer));
    BLO_write_struct_at_address(writer,
                                bNodeTree,
                                wrld->nodetree,
                                BLO_write_get_id_buffer_temp_id(temp_embedded_id_buffer));
    ntreeBlendWrite(writer, (bNodeTree *)BLO_write_get_id_buffer_temp_id(temp_embedded_id_buffer));
    BLO_write_destroy_id_buffer(&temp_embedded_id_buffer);
  }

  BKE_previewimg_blend_write(writer, wrld->preview);

  if (wrld->lightgroup) {
    BLO_write_struct(writer, LightgroupMembership, wrld->lightgroup);
  }
}

static void world_blend_read_data(BlendDataReader *reader, ID *id)
{
  World *wrld = (World *)id;

  BLO_read_data_address(reader, &wrld->preview);
  BKE_previewimg_blend_read(reader, wrld->preview);
  BLI_listbase_clear(&wrld->gpumaterial);

  BLO_read_data_address(reader, &wrld->lightgroup);
}

IDTypeInfo IDType_ID_WO = {
    /*id_code*/ ID_WO,
    /*id_filter*/ FILTER_ID_WO,
    /*main_listbase_index*/ INDEX_ID_WO,
    /*struct_size*/ sizeof(World),
    /*name*/ "World",
    /*name_plural*/ "worlds",
    /*translation_context*/ BLT_I18NCONTEXT_ID_WORLD,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ world_init_data,
    /*copy_data*/ world_copy_data,
    /*free_data*/ world_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ world_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ world_blend_write,
    /*blend_read_data*/ world_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

World *BKE_world_add(Main *bmain, const char *name)
{
  World *wrld;

  wrld = static_cast<World *>(BKE_id_new(bmain, ID_WO, name));

  return wrld;
}

void BKE_world_eval(Depsgraph *depsgraph, World *world)
{
  DEG_debug_print_eval(depsgraph, __func__, world->id.name, world);
  GPU_material_free(&world->gpumaterial);
}
