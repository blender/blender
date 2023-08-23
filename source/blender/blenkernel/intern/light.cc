/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_defaults.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_colortools.h"
#include "BKE_icons.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_light.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BLT_translation.h"

#include "DEG_depsgraph.h"

#include "BLO_read_write.h"

static void light_init_data(ID *id)
{
  Light *la = (Light *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(la, id));

  MEMCPY_STRUCT_AFTER(la, DNA_struct_default_get(Light), id);
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

  const bool is_localized = (flag & LIB_ID_CREATE_LOCAL) != 0;
  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (la_src->nodetree) {
    if (is_localized) {
      la_dst->nodetree = ntreeLocalize(la_src->nodetree);
    }
    else {
      BKE_id_copy_ex(
          bmain, (ID *)la_src->nodetree, (ID **)&la_dst->nodetree, flag_private_id_data);
    }
    la_dst->nodetree->owner_id = &la_dst->id;
  }

  if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
    BKE_previewimg_id_copy(&la_dst->id, &la_src->id);
  }
  else {
    la_dst->preview = nullptr;
  }
}

static void light_free_data(ID *id)
{
  Light *la = (Light *)id;

  /* is no lib link block, but light extension */
  if (la->nodetree) {
    ntreeFreeEmbeddedTree(la->nodetree);
    MEM_freeN(la->nodetree);
    la->nodetree = nullptr;
  }

  BKE_previewimg_free(&la->preview);
  BKE_icon_id_delete(&la->id);
  la->id.icon_id = 0;
}

static void light_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Light *lamp = reinterpret_cast<Light *>(id);
  const int flag = BKE_lib_query_foreachid_process_flags_get(data);

  if (lamp->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&lamp->nodetree));
  }

  if (flag & IDWALK_DO_DEPRECATED_POINTERS) {
    BKE_LIB_FOREACHID_PROCESS_ID_NOCHECK(data, lamp->ipo, IDWALK_CB_USER);
  }
}

static void light_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Light *la = (Light *)id;

  /* Forward compatibility for energy. */
  la->energy_deprecated = la->energy;
  if (la->type == LA_AREA) {
    la->energy_deprecated /= M_PI_4;
  }

  /* write LibData */
  BLO_write_id_struct(writer, Light, id_address, &la->id);
  BKE_id_blend_write(writer, &la->id);

  /* Node-tree is integral part of lights, no libdata. */
  if (la->nodetree) {
    BLO_Write_IDBuffer *temp_embedded_id_buffer = BLO_write_allocate_id_buffer();
    BLO_write_init_id_buffer_from_id(
        temp_embedded_id_buffer, &la->nodetree->id, BLO_write_is_undo(writer));
    BLO_write_struct_at_address(
        writer, bNodeTree, la->nodetree, BLO_write_get_id_buffer_temp_id(temp_embedded_id_buffer));
    ntreeBlendWrite(writer, (bNodeTree *)BLO_write_get_id_buffer_temp_id(temp_embedded_id_buffer));
    BLO_write_destroy_id_buffer(&temp_embedded_id_buffer);
  }

  BKE_previewimg_blend_write(writer, la->preview);
}

static void light_blend_read_data(BlendDataReader *reader, ID *id)
{
  Light *la = (Light *)id;

  BLO_read_data_address(reader, &la->preview);
  BKE_previewimg_blend_read(reader, la->preview);
}

static void light_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Light *la = (Light *)id;
  BLO_read_id_address(reader, id, &la->ipo);  // XXX deprecated - old animation system
}

IDTypeInfo IDType_ID_LA = {
    /*id_code*/ ID_LA,
    /*id_filter*/ FILTER_ID_LA,
    /*main_listbase_index*/ INDEX_ID_LA,
    /*struct_size*/ sizeof(Light),
    /*name*/ "Light",
    /*name_plural*/ "lights",
    /*translation_context*/ BLT_I18NCONTEXT_ID_LIGHT,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ light_init_data,
    /*copy_data*/ light_copy_data,
    /*free_data*/ light_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ light_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ light_blend_write,
    /*blend_read_data*/ light_blend_read_data,
    /*blend_read_lib*/ light_blend_read_lib,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

Light *BKE_light_add(Main *bmain, const char *name)
{
  Light *la;

  la = static_cast<Light *>(BKE_id_new(bmain, ID_LA, name));

  return la;
}

void BKE_light_eval(Depsgraph *depsgraph, Light *la)
{
  DEG_debug_print_eval(depsgraph, __func__, la->id.name, la);
}
