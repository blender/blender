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

#include "BLI_math.h"
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

  la->curfalloff = BKE_curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
  BKE_curvemapping_init(la->curfalloff);
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

  la_dst->curfalloff = BKE_curvemapping_copy(la_src->curfalloff);

  if (la_src->nodetree) {
    if (is_localized) {
      la_dst->nodetree = ntreeLocalize(la_src->nodetree);
    }
    else {
      BKE_id_copy_ex(
          bmain, (ID *)la_src->nodetree, (ID **)&la_dst->nodetree, flag_private_id_data);
    }
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
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&lamp->nodetree));
  }
}

static void light_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Light *la = (Light *)id;

  /* write LibData */
  BLO_write_id_struct(writer, Light, id_address, &la->id);
  BKE_id_blend_write(writer, &la->id);

  if (la->adt) {
    BKE_animdata_blend_write(writer, la->adt);
  }

  if (la->curfalloff) {
    BKE_curvemapping_blend_write(writer, la->curfalloff);
  }

  /* Node-tree is integral part of lights, no libdata. */
  if (la->nodetree) {
    BLO_write_struct(writer, bNodeTree, la->nodetree);
    ntreeBlendWrite(writer, la->nodetree);
  }

  BKE_previewimg_blend_write(writer, la->preview);
}

static void light_blend_read_data(BlendDataReader *reader, ID *id)
{
  Light *la = (Light *)id;
  BLO_read_data_address(reader, &la->adt);
  BKE_animdata_blend_read_data(reader, la->adt);

  BLO_read_data_address(reader, &la->curfalloff);
  if (la->curfalloff) {
    BKE_curvemapping_blend_read(reader, la->curfalloff);
  }

  BLO_read_data_address(reader, &la->preview);
  BKE_previewimg_blend_read(reader, la->preview);
}

static void light_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Light *la = (Light *)id;
  BLO_read_id_address(reader, la->id.lib, &la->ipo);  // XXX deprecated - old animation system
}

static void light_blend_read_expand(BlendExpander *expander, ID *id)
{
  Light *la = (Light *)id;
  BLO_expand(expander, la->ipo);  // XXX deprecated - old animation system
}

IDTypeInfo IDType_ID_LA = {
    .id_code = ID_LA,
    .id_filter = FILTER_ID_LA,
    .main_listbase_index = INDEX_ID_LA,
    .struct_size = sizeof(Light),
    .name = "Light",
    .name_plural = "lights",
    .translation_context = BLT_I18NCONTEXT_ID_LIGHT,
    .flags = IDTYPE_FLAGS_APPEND_IS_REUSABLE,

    .init_data = light_init_data,
    .copy_data = light_copy_data,
    .free_data = light_free_data,
    .make_local = NULL,
    .foreach_id = light_foreach_id,
    .foreach_cache = NULL,
    .owner_get = NULL,

    .blend_write = light_blend_write,
    .blend_read_data = light_blend_read_data,
    .blend_read_lib = light_blend_read_lib,
    .blend_read_expand = light_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

Light *BKE_light_add(Main *bmain, const char *name)
{
  Light *la;

  la = BKE_id_new(bmain, ID_LA, name);

  return la;
}

void BKE_light_eval(struct Depsgraph *depsgraph, Light *la)
{
  DEG_debug_print_eval(depsgraph, __func__, la->id.name, la);
}
