/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_defaults.h"
#include "DNA_object_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_speaker.h"

#include "BLO_read_write.h"

static void speaker_init_data(ID *id)
{
  Speaker *speaker = (Speaker *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(speaker, id));

  MEMCPY_STRUCT_AFTER(speaker, DNA_struct_default_get(Speaker), id);
}

static void speaker_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Speaker *speaker = (Speaker *)id;

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, speaker->sound, IDWALK_CB_USER);
}

static void speaker_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Speaker *spk = (Speaker *)id;

  /* write LibData */
  BLO_write_id_struct(writer, Speaker, id_address, &spk->id);
  BKE_id_blend_write(writer, &spk->id);

  if (spk->adt) {
    BKE_animdata_blend_write(writer, spk->adt);
  }
}

static void speaker_blend_read_data(BlendDataReader *reader, ID *id)
{
  Speaker *spk = (Speaker *)id;
  BLO_read_data_address(reader, &spk->adt);
  BKE_animdata_blend_read_data(reader, spk->adt);

#if 0
  spk->sound = newdataadr(fd, spk->sound);
  direct_link_sound(fd, spk->sound);
#endif
}

static void speaker_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Speaker *spk = (Speaker *)id;
  BLO_read_id_address(reader, id, &spk->sound);
}

static void speaker_blend_read_expand(BlendExpander *expander, ID *id)
{
  Speaker *spk = (Speaker *)id;
  BLO_expand(expander, spk->sound);
}

IDTypeInfo IDType_ID_SPK = {
    .id_code = ID_SPK,
    .id_filter = FILTER_ID_SPK,
    .main_listbase_index = INDEX_ID_SPK,
    .struct_size = sizeof(Speaker),
    .name = "Speaker",
    .name_plural = "speakers",
    .translation_context = BLT_I18NCONTEXT_ID_SPEAKER,
    .flags = IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    .asset_type_info = NULL,

    .init_data = speaker_init_data,
    .copy_data = NULL,
    .free_data = NULL,
    .make_local = NULL,
    .foreach_id = speaker_foreach_id,
    .foreach_cache = NULL,
    .foreach_path = NULL,
    .owner_pointer_get = NULL,

    .blend_write = speaker_blend_write,
    .blend_read_data = speaker_blend_read_data,
    .blend_read_lib = speaker_blend_read_lib,
    .blend_read_expand = speaker_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

void *BKE_speaker_add(Main *bmain, const char *name)
{
  Speaker *spk;

  spk = BKE_id_new(bmain, ID_SPK, name);

  return spk;
}
