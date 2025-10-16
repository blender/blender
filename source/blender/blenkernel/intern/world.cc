/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>
#include <optional>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_icons.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_node.hh"
#include "BKE_preview_image.hh"
#include "BKE_world.h"

#include "BLT_translation.hh"

#include "DRW_engine.hh"

#include "DEG_depsgraph.hh"

#include "GPU_material.hh"

#include "BLO_read_write.hh"

/** Free (or release) any data used by this world (does not free the world itself). */
static void world_free_data(ID *id)
{
  World *wrld = (World *)id;

  /* is no lib link block, but world extension */
  if (wrld->nodetree) {
    blender::bke::node_tree_free_embedded_tree(wrld->nodetree);
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
 * \param flag: Copying options (see BKE_lib_id.hh's LIB_ID_COPY_... flags for more).
 */
static void world_copy_data(Main *bmain,
                            std::optional<Library *> owner_library,
                            ID *id_dst,
                            const ID *id_src,
                            const int flag)
{
  World *wrld_dst = (World *)id_dst;
  const World *wrld_src = (const World *)id_src;

  const bool is_localized = (flag & LIB_ID_CREATE_LOCAL) != 0;
  /* Never handle user-count here for own sub-data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;
  /* Always need allocation of the embedded ID data. */
  const int flag_embedded_id_data = flag_subdata & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (wrld_src->nodetree) {
    if (is_localized) {
      wrld_dst->nodetree = blender::bke::node_tree_localize(wrld_src->nodetree, &wrld_dst->id);
    }
    else {
      BKE_id_copy_in_lib(bmain,
                         owner_library,
                         &wrld_src->nodetree->id,
                         &wrld_dst->id,
                         reinterpret_cast<ID **>(&wrld_dst->nodetree),
                         flag_embedded_id_data);
    }
  }

  BLI_listbase_clear(&wrld_dst->gpumaterial);

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

  if (world->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&world->nodetree));
  }
}

static void world_foreach_working_space_color(ID *id, const IDTypeForeachColorFunctionCallback &fn)
{
  World *world = reinterpret_cast<World *>(id);

  fn.single(&world->horr);
}

static void world_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  World *wrld = (World *)id;

  /* Clean up runtime data, important in undo case to reduce false detection of changed
   * datablocks. */
  BLI_listbase_clear(&wrld->gpumaterial);
  wrld->last_update = 0;

  /* Set deprecated #use_nodes for forward compatibility. */
  wrld->use_nodes = true;

  /* write LibData */
  BLO_write_id_struct(writer, World, id_address, &wrld->id);
  BKE_id_blend_write(writer, &wrld->id);

  /* nodetree is integral part of world, no libdata */
  if (wrld->nodetree) {
    BLO_Write_IDBuffer temp_embedded_id_buffer{wrld->nodetree->id, writer};
    BLO_write_struct_at_address(writer, bNodeTree, wrld->nodetree, temp_embedded_id_buffer.get());
    blender::bke::node_tree_blend_write(
        writer, reinterpret_cast<bNodeTree *>(temp_embedded_id_buffer.get()));
  }

  BKE_previewimg_blend_write(writer, wrld->preview);

  if (wrld->lightgroup) {
    BLO_write_struct(writer, LightgroupMembership, wrld->lightgroup);
  }
}

static void world_blend_read_data(BlendDataReader *reader, ID *id)
{
  World *wrld = (World *)id;

  BLO_read_struct(reader, PreviewImage, &wrld->preview);
  BKE_previewimg_blend_read(reader, wrld->preview);
  BLI_listbase_clear(&wrld->gpumaterial);

  BLO_read_struct(reader, LightgroupMembership, &wrld->lightgroup);
}

IDTypeInfo IDType_ID_WO = {
    /*id_code*/ World::id_type,
    /*id_filter*/ FILTER_ID_WO,
    /*dependencies_id_types*/ FILTER_ID_TE,
    /*main_listbase_index*/ INDEX_ID_WO,
    /*struct_size*/ sizeof(World),
    /*name*/ "World",
    /*name_plural*/ N_("worlds"),
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
    /*foreach_working_space_color*/ world_foreach_working_space_color,
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

  wrld = BKE_id_new<World>(bmain, name);

  return wrld;
}

void BKE_world_eval(Depsgraph *depsgraph, World *world)
{
  DEG_debug_print_eval(depsgraph, __func__, world->id.name, world);
  GPU_material_free(&world->gpumaterial);
  world->last_update = DEG_get_update_count(depsgraph);
}
