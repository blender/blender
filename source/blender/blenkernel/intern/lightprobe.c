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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_lightprobe_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lightprobe.h"
#include "BKE_main.h"

#include "BLT_translation.h"

#include "BLO_read_write.h"

static void lightprobe_init_data(ID *id)
{
  LightProbe *probe = (LightProbe *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(probe, id));

  MEMCPY_STRUCT_AFTER(probe, DNA_struct_default_get(LightProbe), id);
}

static void lightprobe_foreach_id(ID *id, LibraryForeachIDData *data)
{
  LightProbe *probe = (LightProbe *)id;

  BKE_LIB_FOREACHID_PROCESS(data, probe->image, IDWALK_CB_USER);
  BKE_LIB_FOREACHID_PROCESS(data, probe->visibility_grp, IDWALK_CB_NOP);
}

static void lightprobe_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  LightProbe *prb = (LightProbe *)id;
  if (prb->id.us > 0 || BLO_write_is_undo(writer)) {
    /* write LibData */
    BLO_write_id_struct(writer, LightProbe, id_address, &prb->id);
    BKE_id_blend_write(writer, &prb->id);

    if (prb->adt) {
      BKE_animdata_blend_write(writer, prb->adt);
    }
  }
}

static void lightprobe_blend_read_data(BlendDataReader *reader, ID *id)
{
  LightProbe *prb = (LightProbe *)id;
  BLO_read_data_address(reader, &prb->adt);
  BKE_animdata_blend_read_data(reader, prb->adt);
}

static void lightprobe_blend_read_lib(BlendLibReader *reader, ID *id)
{
  LightProbe *prb = (LightProbe *)id;
  BLO_read_id_address(reader, prb->id.lib, &prb->visibility_grp);
}

IDTypeInfo IDType_ID_LP = {
    .id_code = ID_LP,
    .id_filter = FILTER_ID_LP,
    .main_listbase_index = INDEX_ID_LP,
    .struct_size = sizeof(LightProbe),
    .name = "LightProbe",
    .name_plural = "lightprobes",
    .translation_context = BLT_I18NCONTEXT_ID_LIGHTPROBE,
    .flags = 0,

    .init_data = lightprobe_init_data,
    .copy_data = NULL,
    .free_data = NULL,
    .make_local = NULL,
    .foreach_id = lightprobe_foreach_id,
    .foreach_cache = NULL,

    .blend_write = lightprobe_blend_write,
    .blend_read_data = lightprobe_blend_read_data,
    .blend_read_lib = lightprobe_blend_read_lib,
    .blend_read_expand = NULL,

    .blend_read_undo_preserve = NULL,
};

void BKE_lightprobe_type_set(LightProbe *probe, const short lightprobe_type)
{
  probe->type = lightprobe_type;

  switch (probe->type) {
    case LIGHTPROBE_TYPE_GRID:
      probe->distinf = 0.3f;
      probe->falloff = 1.0f;
      probe->clipsta = 0.01f;
      break;
    case LIGHTPROBE_TYPE_PLANAR:
      probe->distinf = 0.1f;
      probe->falloff = 0.5f;
      probe->clipsta = 0.001f;
      break;
    case LIGHTPROBE_TYPE_CUBE:
      probe->attenuation_type = LIGHTPROBE_SHAPE_ELIPSOID;
      break;
    default:
      BLI_assert(!"LightProbe type not configured.");
      break;
  }
}

void *BKE_lightprobe_add(Main *bmain, const char *name)
{
  LightProbe *probe;

  probe = BKE_id_new(bmain, ID_LP, name);

  return probe;
}
